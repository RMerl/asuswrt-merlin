/** @file rx_sm.c
  OpenLLDP RX Statemachine

  See LICENSE file for more info.

Authors: Terry Simons (terry.simons@gmail.com)
*/

#ifndef WIN32
#include <arpa/inet.h>
#include <strings.h>
#else // WIN32
#include <Winsock2.h>
//#define strncpy _strncpy
#endif // WIN32

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "rx_sm.h"
#include "lldp_port.h"
#include "tlv/tlv.h"
#include "tlv/tlv_common.h"
#include "msap.h"
#include "lldp_debug.h"

#define FALSE 0
#define TRUE 1

/* This is an 802.1AB per-port variable, 
   so it should go in the port structure */
uint8_t badFrame;

/* Defined by the IEEE 802.1AB standard */
uint8_t rxInitializeLLDP(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB section 10.5.5.3 */
    lldp_port->rx.rcvFrame        = 0;

    /* As per IEEE 802.1AB section 10.1.2 */
    lldp_port->rx.tooManyNeighbors = 0;

    lldp_port->rx.rxInfoAge = 0;

    mibDeleteObjects(lldp_port);

    return 0;
}

/* Defined by the IEEE 802.1AB standard */
int rxProcessFrame(struct lldp_port *lldp_port) {
    /* 802.1AB Variables */
    uint8_t badFrame = 0;
    /* End 802.1AB Variables */

    /* Keep track of the last TLV so we can adhere to the specification */
    /* Which requires the first 3 TLVs to be in the correct order       */
    uint16_t last_tlv_type = 0;
    uint16_t num_tlvs      = 0;
    uint8_t tlv_type       = 0;
    uint16_t tlv_length    = 0;
    uint16_t tlv_offset    = 0;

    struct eth_hdr *ether_hdr;
    struct eth_hdr expect_hdr;

	// The current TLV and respective helper variables
	struct lldp_tlv *tlv     = NULL;
	uint16_t *tlv_hdr        = NULL;
	uint16_t debug_offset    = 0;
	uint8_t *tlv_info_string = NULL;

	// The TLV to be cached for this frame's MSAP
	struct lldp_tlv *cached_tlv = NULL;

    /* The IEEE 802.1AB MSAP is made up of the TLV information string from */
    /* TLV 1 and TLV 2. */
    uint8_t *msap_id           = NULL;
    uint32_t msap_length       = 0;
    uint8_t have_msap          = 0;
    struct lldp_tlv *msap_tlv1 = NULL;
    struct lldp_tlv *msap_tlv2 = NULL;
    struct lldp_tlv *msap_ttl_tlv = NULL;

    /* The TLV list for this frame */
    /* This list will be added to the MSAP cache */
    struct lldp_tlv_list *tlv_list = NULL;

    /* The MSAP cache for this frame */
    struct lldp_msap *msap_cache = NULL;

	// Variables for location based LLDP-MED TLV
	char *elin   = NULL;
	int calength = 0;
	int catype   = 0;

    debug_printf(DEBUG_INT, "(%s) Processing Frame: \n", lldp_port->if_name);

    debug_hex_dump(DEBUG_INT, lldp_port->rx.frame, lldp_port->rx.recvsize);

    /* As per section 10.3.1, verify the destination and ethertype */
    expect_hdr.dst[0] = 0x01;
    expect_hdr.dst[1] = 0x80;
    expect_hdr.dst[2] = 0xc2;
    expect_hdr.dst[3] = 0x00;
    expect_hdr.dst[4] = 0x00;
    expect_hdr.dst[5] = 0x0e;
    expect_hdr.ethertype = htons(0x88cc);

    ether_hdr = (struct eth_hdr *)&lldp_port->rx.frame[0];

    debug_printf(DEBUG_INT, "LLPDU Dst: ");
    debug_hex_printf(DEBUG_INT, (uint8_t *)ether_hdr->dst, 6);

    debug_printf(DEBUG_EXCESSIVE, "Expect Dst: ");
    debug_hex_printf(DEBUG_EXCESSIVE, (uint8_t *)expect_hdr.dst, 6);

    /* Validate the frame's destination */
    if(
            ether_hdr->dst[0] != expect_hdr.dst[0] ||
            ether_hdr->dst[1] != expect_hdr.dst[1] ||
            ether_hdr->dst[2] != expect_hdr.dst[2] ||
            ether_hdr->dst[3] != expect_hdr.dst[3] ||
            ether_hdr->dst[4] != expect_hdr.dst[4] ||
            ether_hdr->dst[5] != expect_hdr.dst[5] ) {

        debug_printf(DEBUG_NORMAL, "[ERROR] This frame is incorrectly addressed to: ");
        debug_hex_printf(DEBUG_NORMAL, (uint8_t *)ether_hdr->dst, 6);
        debug_printf(DEBUG_NORMAL, "[ERROR] This frame should be addressed to: ");
        debug_hex_printf(DEBUG_NORMAL, (uint8_t *)expect_hdr.dst, 6);
        debug_printf(DEBUG_NORMAL, "[ERROR] statsFramesInTotal will *NOT* be incremented\n");

        badFrame++;
    }

    debug_printf(DEBUG_INT, "LLPDU Src: ");
    debug_hex_printf(DEBUG_INT, (uint8_t *)ether_hdr->src, 6);

    debug_printf(DEBUG_INT, "LLPDU Ethertype: %x\n", htons(ether_hdr->ethertype));

    debug_printf(DEBUG_EXCESSIVE, "Expect Ethertype: %x\n", htons(expect_hdr.ethertype));

    /* Validate the frame's ethertype */
    if(ether_hdr->ethertype != expect_hdr.ethertype) {
        debug_printf(DEBUG_NORMAL, "[ERROR] This frame has an incorrect ethertype of: '%x'.\n", htons(ether_hdr->ethertype));

        badFrame++;
    }

    if(!badFrame) {
        lldp_port->rx.statistics.statsFramesInTotal ++;
    }

    do {
        num_tlvs++;

        debug_printf(DEBUG_TLV, "Processing TLV #: %d\n", num_tlvs);

        if(tlv_offset > lldp_port->rx.recvsize) {
            debug_printf(DEBUG_NORMAL, "[ERROR] Offset is larger than received frame!");

            badFrame++;

            break;
        }

        tlv_hdr = (uint16_t *)&lldp_port->rx.frame[sizeof(struct eth_hdr) + tlv_offset];

        /* Grab the first 9 bits */
        tlv_length = htons(*tlv_hdr) & 0x01FF;

        /* Then shift to get the last 7 bits */
        tlv_type = htons(*tlv_hdr) >> 9;

        /* Validate as per 802.1AB section 10.3.2*/
        if(num_tlvs <= 3) {
            if(num_tlvs != tlv_type) {
                debug_printf(DEBUG_NORMAL, "[ERROR] TLV number %d should have tlv_type %d, but is actually %d\n", num_tlvs, num_tlvs, tlv_type);
                debug_printf(DEBUG_NORMAL, "[ERROR] statsFramesDiscardedTotal and statsFramesInErrorsTotal will be incremented as per 802.1AB 10.3.2\n");
                lldp_port->rx.statistics.statsFramesDiscardedTotal++;
                lldp_port->rx.statistics.statsFramesInErrorsTotal++;
                badFrame++;
            }
        }

        debug_printf(DEBUG_EXCESSIVE, "TLV type: %d (%s)  Length: %d\n", tlv_type, tlv_typetoname(tlv_type), tlv_length);

        /* Create a compound offset */
        debug_offset = tlv_length + sizeof(struct eth_hdr) + tlv_offset + sizeof(*tlv_hdr);

        /* The TLV is telling us to index off the end of the frame... tisk tisk */
        if(debug_offset > lldp_port->rx.recvsize) {
            debug_printf(DEBUG_NORMAL, "[ERROR] Received a bad TLV:  %d bytes too long!  Frame will be skipped.\n", debug_offset - lldp_port->rx.recvsize);

            badFrame++;

            break;
        } else {
            /* Temporary Debug to validate above... */
            debug_printf(DEBUG_EXCESSIVE, "TLV would read to: %d, Frame ends at: %d\n", debug_offset, lldp_port->rx.recvsize);
        }

        tlv_info_string = (uint8_t *)&lldp_port->rx.frame[sizeof(struct eth_hdr) + sizeof(*tlv_hdr) + tlv_offset];

	
        tlv = initialize_tlv();

        if(!tlv) {
            debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
            break;
        }

        tlv->type        = tlv_type;
        tlv->length      = tlv_length;
	if(tlv->length > 0)	  
	  tlv->info_string = calloc(1, tlv_length);

	// XXX Hmmm is this right?
        if(tlv_type == TIME_TO_LIVE_TLV) {
	  if(tlv_length != 2) {
                debug_printf(DEBUG_NORMAL, "[ERROR] TTL TLV has an invalid length!  Should be '2', but is '%d'\n", tlv_length);
#ifndef WIN32
#warning We should actually discard this frame and print out an error...
#warning Write a unit test to stress this
#endif // WIN32
            } else {
                lldp_port->rx.timers.rxTTL = htons(*(uint16_t *)&tlv_info_string[0]);
		msap_ttl_tlv = tlv;
                debug_printf(DEBUG_EXCESSIVE, "rxTTL is: %d\n", lldp_port->rx.timers.rxTTL);
            }
        }

        if(tlv->info_string) {
            memset(tlv->info_string, 0x0, tlv_length);
            memcpy(tlv->info_string, tlv_info_string, tlv_length);
        } 

        /* Validate the TLV */
        if(validate_tlv[tlv_type] != NULL) {
            debug_printf(DEBUG_EXCESSIVE, "Found a validator for TLV type %d.\n", tlv_type);

            debug_hex_dump(DEBUG_EXCESSIVE, tlv->info_string, tlv->length);

            if(validate_tlv[tlv_type](tlv) != XVALIDTLV) {
                badFrame++;
            }
        } else {
	  // NOTE: Any organizationally specific TLVs should get processed through validate_generic_tlv
            debug_printf(DEBUG_EXCESSIVE, "Didn't find specific validator for TLV type %d.  Using validate_generic_tlv.\n", tlv_type);
            if(validate_generic_tlv(tlv) != XVALIDTLV) {
                badFrame++;
            }
        }

	cached_tlv = initialize_tlv();

	if(tlvcpy(cached_tlv, tlv) != 0) {
	  debug_printf(DEBUG_TLV, "Error copying TLV for MSAP cache!\n");
	  } 

	debug_printf(DEBUG_EXCESSIVE, "Adding exploded TLV to MSAP TLV list.\n");
	// Now we can start stuffing the msap data... ;)
	add_tlv(cached_tlv, &tlv_list);

	debug_printf(DEBUG_EXCESSIVE, "Done\n");

	/* Store the MSAP elements */
	if(tlv_type == CHASSIS_ID_TLV) {
	  debug_printf(DEBUG_NORMAL, "Copying TLV1 for MSAP Processing...\n");
	  msap_tlv1 = initialize_tlv();
	  tlvcpy(msap_tlv1, tlv);
	} else if(tlv_type == PORT_ID_TLV) {
	  debug_printf(DEBUG_NORMAL, "Copying TLV2 for MSAP Processing...\n");
	  msap_tlv2 = initialize_tlv();
	  tlvcpy(msap_tlv2, tlv);

	  
	  //Minus 2, for the chassis id subtype and port id subtype... 
	  // IEEE 802.1AB specifies that the MSAP shall be composed of 
	  // The value of the subtypes. 
	  msap_id = calloc(1, msap_tlv1->length - 1  + msap_tlv2->length - 1);

	  if(msap_id == NULL)
	    {
	      debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
	    }
	  
	  // Copy the first part of the MSAP 
	  memcpy(msap_id, &msap_tlv1->info_string[1], msap_tlv1->length - 1);
	  
	  // Copy the second part of the MSAP 
	  memcpy(&msap_id[msap_tlv1->length - 1], &msap_tlv2->info_string[1], msap_tlv2->length - 1);
	  
	  msap_length = (msap_tlv1->length - 1) + (msap_tlv2->length - 1);
	  
	  debug_printf(DEBUG_MSAP, "MSAP TLV1 Length: %d\n", msap_tlv1->length);
	  debug_printf(DEBUG_MSAP, "MSAP TLV2 Length: %d\n", msap_tlv2->length);
	  
	  debug_printf(DEBUG_MSAP, "MSAP is %d bytes: ", msap_length);
	  debug_hex_printf(DEBUG_MSAP, msap_id, msap_length);
	  debug_hex_dump(DEBUG_MSAP, msap_id, msap_length);

	  // Free the MSAP pieces
	  destroy_tlv(&msap_tlv1);
	  destroy_tlv(&msap_tlv2);
	  
	  msap_tlv1 = NULL;
	  msap_tlv2 = NULL;
	  	  
	  have_msap = 1;
	  }
	


//************************************
//LLDP-MED location identification TLV
//************************************
	if (tlv_type == ORG_SPECIFIC_TLV)
	  {
	    int i, j, lcilength;
	    //check TIA OUI
	    if ((tlv->info_string[0] == 0x00) && (tlv->info_string[1] == 0x12)
		&& (tlv->info_string[2] == 0xBB))
	      {
		//debug_printf (DEBUG_NORMAL, "TIA found\n");
		if (tlv->info_string[3] == 3)
		  {
		    
		    debug_printf (DEBUG_NORMAL,
				  "TIA Location Identification found\n");

		    switch (tlv->info_string[4])
		      {
			
		      case 0:
			debug_printf (DEBUG_NORMAL,
				      "Invalid Location data format type! \n");
			break;
		      case 1:		     
			debug_printf (DEBUG_NORMAL, "Coordinate-based LCI\n",
				      tlv->length);
			//encoded location starts at 5
			for (i = 5; i < 16 + 5; i++)
			  {
			    debug_printf (DEBUG_NORMAL, "%02x",
					  tlv->info_string[i]);
			  }
			debug_printf (DEBUG_NORMAL, "\n");
			break;		      
			  case 2:
			    
			    i = 9;//start of first CA
			    j = 0;
			    lcilength = tlv->info_string[5];
			    debug_printf (DEBUG_NORMAL, "Civic Address LCI\n");
			    debug_printf (DEBUG_NORMAL, "LCI Length = %i  \n",
					  lcilength);
			    debug_printf (DEBUG_NORMAL, "What = %i  \n",
					  tlv->info_string[6]);
			    debug_printf (DEBUG_NORMAL, "Countrycode %c%c \n",
					  tlv->info_string[7], tlv->info_string[8]);
			    
			    //lcilength counts from 'what' element, which is on position 6
			    while (i < 6 + lcilength)
			      {				
				catype = tlv->info_string[i];
				i++;
				calength = tlv->info_string[i];
				i++;
				debug_printf (DEBUG_NORMAL,
					      "CA-Type %i, CALength %i = ", catype,
					      calength);
				for (j = i; j < i + calength; j++)
				  debug_printf (DEBUG_NORMAL, "%c",
						tlv->info_string[j]);
				//i++;
				debug_printf (DEBUG_NORMAL, "\n");
				i += calength;
			      }
			    break;
		      case 3:
			debug_printf (DEBUG_NORMAL, "ECS ELIN\n");
			
			//check if ELIN length is ok
			if ((tlv->length < 15) || (tlv->length > 30))
			  {
			    debug_printf (DEBUG_NORMAL,
					  "[ERROR] ELIN length is wrong!\n");
			  }
			
			elin = calloc (1, sizeof (char) * (tlv->length - 5 + 1));
			
			strncpy(elin, (char *)&tlv->info_string[5], tlv->length - 5);
			elin[tlv->length - 5] = '\0';
			debug_printf (DEBUG_NORMAL, "%s \n", elin);
			free (elin);
			
			break;
			
		      default:
			debug_printf (DEBUG_NORMAL,
				      "Reserved location data format for future use detected!\n");
			
		      }
		  }
	      }  
	  }

        tlv_offset += sizeof(*tlv_hdr) + tlv_length;

        last_tlv_type = tlv_type;

        //decode_tlv_subtype(tlv);

        destroy_tlv(&tlv);

    }while(tlv_type != 0);

    //lldp_port->rxChanges = TRUE;

    /* We're done processing the frame and all TLVs... now we can cache it */
    /* Only cache this TLV list if we have an MSAP for it */
    if(have_msap)
      {
#ifndef WIN32
        #warning We need to verify whether this is actually the case.
#endif // WIN32
	lldp_port->rxChanges = TRUE;
	
	debug_printf(DEBUG_TLV, "We have a(n) %d byte MSAP!\n", msap_length);

	msap_cache = calloc(1, sizeof(struct lldp_msap));

	msap_cache->id = msap_id;
	msap_cache->length = msap_length;
	msap_cache->tlv_list = tlv_list;
	msap_cache->next = NULL;

	msap_cache->ttl_tlv = msap_ttl_tlv;
	msap_ttl_tlv = NULL;

	//debug_printf(DEBUG_MSAP, "Iterating MSAP Cache...\n");

	//iterate_msap_cache(msap_cache);

	//debug_printf(DEBUG_MSAP, "Updating MSAP Cache...\n");

	debug_printf(DEBUG_MSAP, "Setting rxInfoTTL to: %d\n", lldp_port->rx.timers.rxTTL);

	msap_cache->rxInfoTTL = lldp_port->rx.timers.rxTTL;

	update_msap_cache(lldp_port, msap_cache);

	if(msap_tlv1 != NULL) {
	  debug_printf(DEBUG_NORMAL, "Error: msap_tlv1 is still allocated!\n");
	  free(msap_tlv1);
	  msap_tlv1 = NULL;
	}

	if(msap_tlv2 != NULL) {
	  debug_printf(DEBUG_NORMAL, "Error: msap_tlv2 is still allocated!\n");
	  free(msap_tlv2);
	  msap_tlv2 = NULL;
	}

      }
    else
      {
	debug_printf(DEBUG_NORMAL, "[ERROR] No MSAP for TLVs in Frame!\n");
      }

    /* Report frame errors */
    if(badFrame) {
        rxBadFrameInfo(badFrame);
    }

    return badFrame;
}
  
void rxBadFrameInfo(uint8_t frameErrors) {
    debug_printf(DEBUG_NORMAL, "[WARNING] This frame had %d errors!\n", frameErrors);
}

/* Just a stub */
uint8_t mibUpdateObjects(struct lldp_port *lldp_port) {
    return 0;
}

uint8_t mibDeleteObjects(struct lldp_port *lldp_port) {
  struct lldp_msap *current = lldp_port->msap_cache;
  struct lldp_msap *tail    = NULL;
  struct lldp_msap *tmp     = NULL;

  while(current != NULL) {
    if(current->rxInfoTTL <= 0) {
      
      // If the top list is expired, then adjust the list
      // before we delete the node.
      if(current == lldp_port->msap_cache) {
	lldp_port->msap_cache = current->next;
      } else {
	tail->next = current->next;
      }

      tmp = current;
      current = current->next;

      if(tmp->id != NULL) {
	free(tmp->id);
      }
	
      destroy_tlv_list(&tmp->tlv_list);
      free(tmp);
    } else {
      tail = current;
      current = current->next;
    } 
  }

  return 0;
}

void rxChangeToState(struct lldp_port *lldp_port, uint8_t state) {
    debug_printf(DEBUG_STATE, "[%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));

    switch(state) {
        case LLDP_WAIT_PORT_OPERATIONAL: {
                                             // Do nothing
                                         }break;    
        case RX_LLDP_INITIALIZE: {
                                     if(lldp_port->rx.state != LLDP_WAIT_PORT_OPERATIONAL) {
                                         debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                                     }

                                     /*
                                     // From the 10.5.5.3 rx state machine diagram
                                     rxInitializeLLDP(lldp_port);
                                     lldp_port->rx.rcvFrame = 0;
                                     */
                                 }break;
        case DELETE_AGED_INFO: {
                                   if(lldp_port->rx.state != LLDP_WAIT_PORT_OPERATIONAL) {
                                       debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                                   }

                                   /*
                                   // From the 10.5.5.3 rx state machine diagram
                                   lldp_port->rx.somethingChangedRemote = 0;
                                   mibDeleteObjects(lldp_port);
                                   lldp_port->rx.rxInfoAge = 0;
                                   lldp_port->rx.somethingChangedRemote = 1;
                                   */
                               }break;
        case RX_WAIT_FOR_FRAME: {    
                                    if(!(lldp_port->rx.state == RX_LLDP_INITIALIZE ||
                                                lldp_port->rx.state == DELETE_INFO ||
                                                lldp_port->rx.state == UPDATE_INFO ||
                                                lldp_port->rx.state == RX_FRAME)) {
                                        debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                                    }

                                    /*
                                    // From the 10.5.5.3 rx state machine diagram
                                    lldp_port->rx.badFrame               = 0;
                                    lldp_port->rx.rxInfoAge              = 0;
                                    lldp_port->rx.somethingChangedRemote = 0;
                                    */
                                }break;
        case RX_FRAME: {
                           if(lldp_port->rx.state != RX_WAIT_FOR_FRAME) {
                               debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                           }

                           /*
                           // From the 10.5.5.3 rx state machine diagram
                           lldp_port->rx.rxChanges = 0;
                           lldp_port->rcvFrame     = 0;
                           rxProcessFrame(lldp_port);    
                           */
                       }break;
        case DELETE_INFO: {
                              if(!(lldp_port->rx.state == RX_WAIT_FOR_FRAME ||
                                          lldp_port->rx.state == RX_FRAME)) {
                                  debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                              }
                          }break;
        case UPDATE_INFO: {
                              if(lldp_port->rx.state != RX_FRAME) {
                                  debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                              }
                          }break;
        default: {
                     debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, rxStateFromID(lldp_port->rx.state), rxStateFromID(state));      
                     // Do nothing
                 };
    };

    // Now update the interface state
    lldp_port->rx.state = state;
}

char *rxStateFromID(uint8_t state)
{
    switch(state)
    {
        case LLDP_WAIT_PORT_OPERATIONAL:
            return "LLDP_WAIT_PORT_OPERATIONAL";
        case DELETE_AGED_INFO:
            return "DELETE_AGED_INFO";
        case RX_LLDP_INITIALIZE:
            return "RX_LLDP_INITIALIZE";
        case RX_WAIT_FOR_FRAME:
            return "RX_WAIT_FOR_FRAME";
        case RX_FRAME:
            return "RX_FRAME";
        case DELETE_INFO:
            return "DELETE_INFO";
        case UPDATE_INFO:
            return "UPDATE_INFO";
    };

    debug_printf(DEBUG_NORMAL, "[ERROR] Unknown RX State: '%d'\n", state);
    return "Unknown";
}

uint8_t rxGlobalStatemachineRun(struct lldp_port *lldp_port)
{
  /* NB: IEEE 802.1AB Section 10.5.5.3 claims that */
  /* An unconditional transfer should occur when */
  /* "(rxInfoAge = FALSE) && (portEnabled == FALSE)" */
  /* I believe that "(rxInfoAge = FALSE)" is a typo and should be: */
  /* "(rxInfoAge == FALSE)" */
  if((lldp_port->rx.rxInfoAge == FALSE) && (lldp_port->portEnabled == FALSE))
    {
      rxChangeToState(lldp_port, LLDP_WAIT_PORT_OPERATIONAL);
    }
  
  switch(lldp_port->rx.state)
    {
    case LLDP_WAIT_PORT_OPERATIONAL:
      {
	if(lldp_port->rx.rxInfoAge == TRUE)
	  rxChangeToState(lldp_port, DELETE_AGED_INFO);
	if(lldp_port->portEnabled == TRUE) 
	  rxChangeToState(lldp_port, RX_LLDP_INITIALIZE);
      }break;
    case DELETE_AGED_INFO:
      {
	rxChangeToState(lldp_port, LLDP_WAIT_PORT_OPERATIONAL);
      }break;
    case RX_LLDP_INITIALIZE:
      {
	if((lldp_port->adminStatus == enabledRxTx) || (lldp_port->adminStatus == enabledRxOnly))
	  rxChangeToState(lldp_port, RX_WAIT_FOR_FRAME);
      }break;
    case RX_WAIT_FOR_FRAME:
      {
	if(lldp_port->rx.rxInfoAge == TRUE)
	  rxChangeToState(lldp_port, DELETE_INFO);
	if(lldp_port->rx.rcvFrame == TRUE)
	  rxChangeToState(lldp_port, RX_FRAME);
      }break;
    case DELETE_INFO:
      {
	rxChangeToState(lldp_port, RX_WAIT_FOR_FRAME);
      }break;
    case RX_FRAME:
      {
	if(lldp_port->rx.timers.rxTTL == 0)
	  rxChangeToState(lldp_port, DELETE_INFO);
	if((lldp_port->rx.timers.rxTTL != 0) && (lldp_port->rxChanges == TRUE))
	  {
	    rxChangeToState(lldp_port, UPDATE_INFO);
	  }
      }break;
    case UPDATE_INFO:
      {
	rxChangeToState(lldp_port, RX_WAIT_FOR_FRAME);
      }break;
    default:
            debug_printf(DEBUG_NORMAL, "[ERROR] The RX Global State Machine is broken!\n");
    };
  
  return 0;
}

void rxStatemachineRun(struct lldp_port *lldp_port)
{
  debug_printf(DEBUG_NORMAL, "Running RX state machine for %s\n", lldp_port->if_name);

    rxGlobalStatemachineRun(lldp_port);

    switch(lldp_port->rx.state)
      {
      case LLDP_WAIT_PORT_OPERATIONAL:
	{
	  // Do nothing here... we'll transition in the global state machine check
	  rx_do_lldp_wait_port_operational(lldp_port);
	}break;
      case DELETE_AGED_INFO:
	{
	  rx_do_delete_aged_info(lldp_port);
	}break;
      case RX_LLDP_INITIALIZE:
	{
	  rx_do_rx_lldp_initialize(lldp_port);
	}break;
      case RX_WAIT_FOR_FRAME:
	{
	  rx_do_rx_wait_for_frame(lldp_port);
	}break;
      case RX_FRAME:
	{
	  rx_do_rx_frame(lldp_port);
	}break;
      case DELETE_INFO: {
	rx_do_rx_delete_info(lldp_port);
      }break;
      case UPDATE_INFO: {
	rx_do_rx_update_info(lldp_port);
      }break;
      default:
	debug_printf(DEBUG_NORMAL, "[ERROR] The RX State Machine is broken!\n");      
    };

    rx_do_update_timers(lldp_port);
}

void rx_decrement_timer(uint16_t *timer) {
    if((*timer) > 0) {
        (*timer)--;
    }
}

void rx_do_update_timers(struct lldp_port *lldp_port) {
  struct lldp_msap *msap_cache = lldp_port->msap_cache;

  debug_printf(DEBUG_NORMAL, "Decrementing RX Timers\n");

  // Here's where we update the IEEE 802.1AB RX timers:
  while(msap_cache != NULL) {

    rx_decrement_timer(&msap_cache->rxInfoTTL);

    /*if(msap_cache->ttl_tlv != NULL) {
      rx_decrement_timer((uint16_t *)&msap_cache->ttl_tlv->info_string);
      }*/

    // We're going to potenetially break the state machine here for a performance bump.
    // The state machine isn't clear (to me) how rxInfoAge is supposed to be set (per MSAP or per port)
    // and it seems to me that having a single tag that gets set if at least 1 MSAP is out of date 
    // is much more efficient than traversing the entire cache every state machine loop looking for an 
    // expired MSAP... 
    if(msap_cache->rxInfoTTL <= 0)
      lldp_port->rx.rxInfoAge = TRUE;

    msap_cache = msap_cache->next;
  }

  rx_decrement_timer(&lldp_port->rx.timers.tooManyNeighborsTimer);

  rx_display_timers(lldp_port);
}

void rx_display_timers(struct lldp_port *lldp_port) {
  struct lldp_msap *msap_cache = lldp_port->msap_cache;

  debug_printf(DEBUG_NORMAL, "Displaying RX Timers\n");

  while(msap_cache != NULL) {
        debug_printf(DEBUG_STATE, "[TIMER] (%s with MSAP: ", lldp_port->if_name);
	debug_hex_printf(DEBUG_STATE, msap_cache->id, msap_cache->length);
	debug_printf(DEBUG_NORMAL, ") rxInfoTTL: %d\n", msap_cache->rxInfoTTL);
	
	msap_cache = msap_cache->next;
  }

  debug_printf(DEBUG_STATE, "[TIMER] (%s) tooManyNeighborsTimer: %d\n", lldp_port->if_name, lldp_port->rx.timers.tooManyNeighborsTimer);
}

void rx_do_lldp_wait_port_operational(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
}


void rx_do_delete_aged_info(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    lldp_port->rx.somethingChangedRemote = FALSE;
    mibDeleteObjects(lldp_port);
    lldp_port->rx.rxInfoAge = FALSE;
    lldp_port->rx.somethingChangedRemote = TRUE;
}

void rx_do_rx_lldp_initialize(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    rxInitializeLLDP(lldp_port);
    lldp_port->rx.rcvFrame = FALSE;
}

void rx_do_rx_wait_for_frame(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    lldp_port->rx.badFrame = FALSE;
    lldp_port->rx.rxInfoAge = FALSE;
    lldp_port->rx.somethingChangedRemote = FALSE;
}

void rx_do_rx_frame(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    lldp_port->rxChanges = FALSE;
    lldp_port->rx.rcvFrame = FALSE;
    rxProcessFrame(lldp_port);

    // Clear the frame buffer out to avoid weird problems. ;)
    memset(&lldp_port->rx.frame[0], 0x0, lldp_port->rx.recvsize);
}

void rx_do_rx_delete_info(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    mibDeleteObjects(lldp_port);
    lldp_port->rx.somethingChangedRemote = TRUE;
}

void rx_do_rx_update_info(struct lldp_port *lldp_port) {
    /* As per IEEE 802.1AB 10.5.5.3 state diagram */
    mibUpdateObjects(lldp_port);
    lldp_port->rx.somethingChangedRemote = TRUE;
}
