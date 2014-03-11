/*******************************************************************
 *
 * OpenLLDP TX Statemachine
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_sm_tx.c
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef WIN32
#include <stdint.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else  // WIN32
#include <Winsock2.h>
#include "stdintwin.h"
#endif // WIN32
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef __LINUX__
#include <malloc.h>
#endif /* __LINUX__ */
#include <stdlib.h>
#include "tx_sm.h"
#include "lldp_port.h"
#include "lldp_debug.h"
#include "tlv/tlv.h"
#include "tlv/tlv_common.h"
#include "platform/framehandler.h"

#define TRUE 1
#define FALSE 0

extern struct lci_s lci;

/* This function tests out of order TLVs to the upstream device */
void mibConstrInfoLLDPDU_test_out_of_order() {
}

/* This function tests invalid TLV creation and validation */
void mibConstrInfoLLDPDU_test_invalid_tlv() {
}

void mibConstrInfoLLDPDU(struct lldp_port *lldp_port)
{
    // This code will only work for Linux!
    struct eth_hdr tx_hdr;
    struct lldp_tlv_list *tlv_list = NULL;
	struct lldp_flat_tlv *tlv      = NULL;
	struct lldp_tlv_list *tmp      = 0;
	uint32_t frame_offset          = 0;

    // Update our port information so it's current. ;)
    // This should be replaced with OS-specific events if possible...
    refreshInterfaceData(lldp_port);

    /* As per section 10.3.1, verify the destination and ethertype */
    tx_hdr.dst[0] = 0x01;
    tx_hdr.dst[1] = 0x80;
    tx_hdr.dst[2] = 0xc2;
    tx_hdr.dst[3] = 0x00;
    tx_hdr.dst[4] = 0x00;
    tx_hdr.dst[5] = 0x0e;

#ifndef WIN32
#warning Write a test that sends a frame with a source & dest of 0180c2000e... 
#endif // WIN32
    tx_hdr.src[0] = lldp_port->source_mac[0];
    tx_hdr.src[1] = lldp_port->source_mac[1];
    tx_hdr.src[2] = lldp_port->source_mac[2];
    tx_hdr.src[3] = lldp_port->source_mac[3];
    tx_hdr.src[4] = lldp_port->source_mac[4];
    tx_hdr.src[5] = lldp_port->source_mac[5];

    tx_hdr.ethertype = htons(0x88cc);

    // How far into our frame should we offset our copies?
    frame_offset = 0;

	if(lldp_port->tx.frame != NULL)
	{
		memcpy(&lldp_port->tx.frame[frame_offset], &tx_hdr, sizeof(tx_hdr));
	}

    frame_offset += sizeof(struct eth_hdr);

    // This TLV *MUST* be first.
    add_tlv(create_chassis_id_tlv(lldp_port), &tlv_list);

    // This TLV *MUST* be second.
    add_tlv(create_port_id_tlv(lldp_port), &tlv_list);

    // This TLV *MUST* be third.
    add_tlv(create_ttl_tlv(lldp_port), &tlv_list);

    add_tlv(create_port_description_tlv(lldp_port), &tlv_list);

    add_tlv(create_system_name_tlv(lldp_port), &tlv_list);

    add_tlv(create_system_description_tlv(lldp_port), &tlv_list);

    add_tlv(create_system_capabilities_tlv(lldp_port), &tlv_list);

    add_tlv(create_management_address_tlv(lldp_port), &tlv_list);

    //add LLDP-MED location identification
    if (lci.location_data_format != -1)
      add_tlv (create_lldpmed_location_identification_tlv (lldp_port),
	       &tlv_list);

    // This TLV *MUST* be last.
    add_tlv(create_end_of_lldpdu_tlv(lldp_port), &tlv_list);

    tmp = tlv_list;

    while(tmp != NULL) {
        tlv = flatten_tlv(tmp->tlv);

		if(lldp_port->tx.frame != NULL)
		{
			memcpy(&lldp_port->tx.frame[frame_offset], tlv->tlv, tlv->size);
		}

        frame_offset += tlv->size;

        destroy_flattened_tlv(&tlv);

        tmp = tmp->next;
    }

    destroy_tlv_list(&tlv_list);

    // Pad to 64 bytes
    if(frame_offset < 64) {
        lldp_port->tx.sendsize = 64;
    } else {
        lldp_port->tx.sendsize = frame_offset;
    }

}

void mibConstrShutdownLLDPDU(struct lldp_port *lldp_port)
{
    struct lldp_tlv *end_of_lldpdu_tlv = create_end_of_lldpdu_tlv(lldp_port);

	debug_printf(DEBUG_NORMAL, "Would send shutdown!");

    if(validate_end_of_lldpdu_tlv(end_of_lldpdu_tlv))
    {
        // We only want the first 3 bytes... 
        // The type/length pair are bytes 1 & 2 (7 and 9 bits respectively)
        // The 3rd octect should contain a 0.
        memcpy(&lldp_port->tx.frame[0], end_of_lldpdu_tlv, 3);
    }
    else
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV End of LLDPDU validation faliure in %s() at line: %d!\n", __FUNCTION__, __LINE__);
    }

    free(end_of_lldpdu_tlv);
}

uint8_t txFrame(struct lldp_port *lldp_port)
{
    debug_printf(DEBUG_INT, "Sending LLDPDU on [%s]!\n", lldp_port->if_name);

    // Call the platform specific frame transmission code.
    // lldp_port contains all of the necessary information to do platform-specific
    // transmits.
    lldp_write(lldp_port);

    if(lldp_port->tx.frame != NULL)
      {
	// Should only need to memset 0x0 to sendsize, because we shouldn't have filled it more than that. ;)
	memset(&lldp_port->tx.frame[0], 0x0, lldp_port->tx.sendsize);
      }
    // Update the statsFramesOutTotal counter...
    lldp_port->tx.statistics.statsFramesOutTotal++;
    
    return 0;
}

uint8_t txInitializeLLDP(struct lldp_port *lldp_port)
{
    /* As per IEEE 802.1AB section 10.1.1 */
    lldp_port->tx.somethingChangedLocal = 0;

    /* Defined in 10.5.2.1 */
    lldp_port->tx.statistics.statsFramesOutTotal = 0;

    lldp_port->tx.timers.reinitDelay   = 2;  // Recommended minimum by 802.1AB 10.5.3.3
    lldp_port->tx.timers.msgTxHold     = 4;  // Recommended minimum by 802.1AB 10.5.3.3
    lldp_port->tx.timers.msgTxInterval = 30; // Recommended minimum by 802.1AB 10.5.3.3
    lldp_port->tx.timers.txDelay       = 2;  // Recommended minimum by 802.1AB 10.5.3.3

    // Unsure what to set these to...
    lldp_port->tx.timers.txShutdownWhile = 0;

    /* Collect all of the system specific information here */
    return 0;
}

void txChangeToState(struct lldp_port *lldp_port, uint8_t state) {
    debug_printf(DEBUG_STATE, "%s -> %s\n", txStateFromID(lldp_port->tx.state), txStateFromID(state));

    switch(state) {
        case TX_LLDP_INITIALIZE: {
                                     if((lldp_port->tx.state != TX_SHUTDOWN_FRAME) && lldp_port->portEnabled) {
                                         debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, txStateFromID(lldp_port->tx.state), txStateFromID(state));  
                                     } 
                                 }break;
        case TX_IDLE: {
                          if(!(lldp_port->tx.state == TX_LLDP_INITIALIZE ||
                                      lldp_port->tx.state == TX_INFO_FRAME)) {
                              debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, txStateFromID(lldp_port->tx.state), txStateFromID(state));
                          }

                          lldp_port->tx.txTTL = min(65535, (lldp_port->tx.timers.msgTxInterval * lldp_port->tx.timers.msgTxHold));
                          lldp_port->tx.timers.txTTR = lldp_port->tx.timers.msgTxInterval;
                          lldp_port->tx.somethingChangedLocal = FALSE;
                          lldp_port->tx.timers.txDelayWhile = lldp_port->tx.timers.txDelay;
                      }break;
        case TX_SHUTDOWN_FRAME:
        case TX_INFO_FRAME: {
                                if(lldp_port->tx.state != TX_IDLE) {
                                    debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, txStateFromID(lldp_port->tx.state), txStateFromID(state));
                                }
                            }break;
        default:
                            debug_printf(DEBUG_STATE, "[ERROR] Illegal Transition: [%s] %s -> %s\n", lldp_port->if_name, txStateFromID(lldp_port->tx.state), txStateFromID(state));
    };

    lldp_port->tx.state = state;
}

char *txStateFromID(uint8_t state) {
    switch(state) {
        case TX_LLDP_INITIALIZE:
            return "TX_LLDP_INITIALIZE";
        case TX_IDLE:
            return "TX_IDLE";
        case TX_SHUTDOWN_FRAME:
            return "TX_SHUTDOWN_FRAME";
        case TX_INFO_FRAME:
            return "TX_INFO_FRAME";
    };

    debug_printf(DEBUG_NORMAL, "[ERROR] Unknown TX State: '%d'\n", state);
    return "Unknown";
}

void txGlobalStatemachineRun(struct lldp_port *lldp_port) {
    /* Sit in TX_LLDP_INITIALIZE until the next initialization */
    if(lldp_port->portEnabled == FALSE) {
        lldp_port->portEnabled = TRUE;

#ifndef WIN32
#warning ChEAT
#endif // WIN32
        txChangeToState(lldp_port, TX_LLDP_INITIALIZE);           
    }

    switch(lldp_port->tx.state) {
        case TX_LLDP_INITIALIZE: {
                                     if((lldp_port->adminStatus == enabledRxTx) || (lldp_port->adminStatus == enabledTxOnly)) {    
                                         txChangeToState(lldp_port, TX_IDLE);
                                     }
                                 }break;
        case TX_IDLE: {
                          // It's time to send a shutdown frame...
                          if((lldp_port->adminStatus == disabled) || (lldp_port->adminStatus == enabledRxOnly)) {
                              txChangeToState(lldp_port, TX_SHUTDOWN_FRAME);
                              break;
                          }

                          // It's time to send a frame...
                          if((lldp_port->tx.timers.txDelayWhile == 0) && ((lldp_port->tx.timers.txTTR == 0) || (lldp_port->tx.somethingChangedLocal))) {
                              txChangeToState(lldp_port, TX_INFO_FRAME);
                          }
                      }break;
        case TX_SHUTDOWN_FRAME: {
                                    if(lldp_port->tx.timers.txShutdownWhile == 0)
                                        txChangeToState(lldp_port, TX_LLDP_INITIALIZE);
                                }break;
        case TX_INFO_FRAME: {   
                                txChangeToState(lldp_port, TX_IDLE);
                            }break;
        default:
                            debug_printf(DEBUG_NORMAL, "[ERROR] The TX State Machine is broken!\n");
    };
}

#ifndef WIN32
uint16_t min(uint16_t value1, uint16_t value2)
{
    if(value1 < value2)
    {
        return value1;
    }

    return value2;
}
#endif // WIN32

void txStatemachineRun(struct lldp_port *lldp_port)
{
    debug_printf(DEBUG_STATE, "%s -> %s\n", lldp_port->if_name, txStateFromID(lldp_port->tx.state));

    txGlobalStatemachineRun(lldp_port);

    switch(lldp_port->tx.state)
    {
        case TX_LLDP_INITIALIZE:
            {
                tx_do_tx_lldp_initialize(lldp_port);
            }break;
        case TX_IDLE:      
            {
                tx_do_tx_idle(lldp_port);
            }break;
        case TX_SHUTDOWN_FRAME:
            {
                tx_do_tx_shutdown_frame(lldp_port);
            }break;
        case TX_INFO_FRAME:
            {
                tx_do_tx_info_frame(lldp_port);
            }break;
        default:
            debug_printf(DEBUG_NORMAL, "[ERROR] The TX State Machine is broken!\n");
    };

    tx_do_update_timers(lldp_port);
}

void tx_decrement_timer(uint16_t *timer) {
  if((*timer) > 0)
    (*timer)--;
}

void tx_do_update_timers(struct lldp_port *lldp_port) {

    tx_decrement_timer(&lldp_port->tx.timers.txShutdownWhile);
    tx_decrement_timer(&lldp_port->tx.timers.txDelayWhile);
    tx_decrement_timer(&lldp_port->tx.timers.txTTR);

    tx_display_timers(lldp_port);
}

void tx_display_timers(struct lldp_port *lldp_port) {

  debug_printf(DEBUG_STATE, "[IP] (%s) IP: %d.%d.%d.%d\n", lldp_port->if_name, lldp_port->source_ipaddr[0], lldp_port->source_ipaddr[1], lldp_port->source_ipaddr[2], lldp_port->source_ipaddr[3]);

    debug_printf(DEBUG_STATE, "[TIMER] (%s) txTTL: %d\n", lldp_port->if_name, lldp_port->tx.txTTL);
    debug_printf(DEBUG_STATE, "[TIMER] (%s) txTTR: %d\n", lldp_port->if_name, lldp_port->tx.timers.txTTR);
    debug_printf(DEBUG_STATE, "[TIMER] (%s) txDelayWhile: %d\n", lldp_port->if_name, lldp_port->tx.timers.txDelayWhile);
    debug_printf(DEBUG_STATE, "[TIMER] (%s) txShutdownWhile: %d\n", lldp_port->if_name, lldp_port->tx.timers.txShutdownWhile);
}

void tx_do_tx_lldp_initialize(struct lldp_port *lldp_port) {
    /* As per 802.1AB 10.5.4.3 */
    txInitializeLLDP(lldp_port);
}

void tx_do_tx_idle(struct lldp_port *lldp_port) {
    /* As per 802.1AB 10.5.4.3 */
    /* I think these belong in the change to state block...
       lldp_port->tx.txTTL = min(65535, (lldp_port->tx.timers.msgTxInterval * lldp_port->tx.timers.msgTxHold));
       lldp_port->tx.timers.txTTR = lldp_port->tx.timers.msgTxInterval;
       lldp_port->tx.somethingChangedLocal = FALSE;
       lldp_port->tx.timers.txDelayWhile = lldp_port->tx.timers.txDelay;
       */
}

void tx_do_tx_shutdown_frame(struct lldp_port *lldp_port) {
    /* As per 802.1AB 10.5.4.3 */
    mibConstrShutdownLLDPDU(lldp_port);
    txFrame(lldp_port);
    lldp_port->tx.timers.txShutdownWhile = lldp_port->tx.timers.reinitDelay;
}

void tx_do_tx_info_frame(struct lldp_port *lldp_port) {
    /* As per 802.1AB 10.5.4.3 */
    mibConstrInfoLLDPDU(lldp_port);
    txFrame(lldp_port);
}
