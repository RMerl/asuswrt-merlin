/*******************************************************************
 *
 * OpenLLDP TLV
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_tlv.c
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef WIN32
#include <arpa/inet.h>
#include <stdint.h>
#else
#define strdup _strdup
#include "stdintwin.h"
#include "Winsock2.h"
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "lldp_port.h"
#include "lldp_debug.h"
#include "tlv.h"
#include "tlv_common.h"
#include "lldp_neighbor.h"

// The LLDP-MED location configuration data.
extern struct lci_s lci;

/* There are a max of 128 TLV validators (types 0 through 127), so we'll stick them in a static array indexed by their tlv type */
uint8_t (*validate_tlv[128])(struct lldp_tlv *tlv) = {
    validate_end_of_lldpdu_tlv,        /* 0 End of LLDPU TLV        */
    validate_chassis_id_tlv,          /* 1 Chassis ID TLV          */
    validate_port_id_tlv,             /* 2 Port ID TLV             */
    validate_ttl_tlv,                 /* 3 Time To Live TLV        */
    validate_port_description_tlv,    /* 4 Port Description TLV    */
    validate_system_name_tlv,         /* 5 System Name TLV         */
    validate_system_description_tlv,  /* 6 System Description TLV  */
    validate_system_capabilities_tlv, /* 7 System Capabilities TLV */
    validate_management_address_tlv,  /* 8 Management Address TLV  */
    /* 9 - 126 are reserved and set to NULL in lldp_tlv_validator_init()                        */
    /* 127 is populated for validate_organizationally_specific_tlv in lldp_tlv_validator_init() */
};

#ifndef WIN32
#warning Write a test suite that will run all the possible combinations of decode
#endif // WIN32

char *decode_tlv_subtype(struct lldp_tlv *tlv)
{
  char *result = calloc(1, 2048);
  char *title  = NULL;
  
    switch(tlv->type)
    {
        case CHASSIS_ID_TLV:
            {
                uint8_t *chassis_id = &tlv->info_string[1];

                switch(tlv->info_string[0])
                {
                    case CHASSIS_ID_CHASSIS_COMPONENT:
                        {
			  title = "\tChassis Component - ";
			  strncat(result, title, strlen(title));
			  debug_hex_strcat((uint8_t *)result, chassis_id, (tlv->length - 1));
                        }break;
                    case CHASSIS_ID_INTERFACE_ALIAS:
                        {
			  strncat(result, "\tInterface Alias - ", 19);
			  debug_hex_strcat((uint8_t *)result, chassis_id, (tlv->length - 1));
                        }break;
                    case CHASSIS_ID_PORT_COMPONENT:
                        {
			  strncat(result, "\tPort Component - ", 19);
			  debug_hex_strcat((uint8_t *)result, chassis_id, (tlv->length - 1));
                        }break;
                    case CHASSIS_ID_MAC_ADDRESS:
                        {
			  title = "\tMAC Address - ";
			  strncat(result, title, strlen(title));
			  debug_hex_strcat((uint8_t *)result, chassis_id, (tlv->length - 1));
                        }break;
                    case CHASSIS_ID_NETWORK_ADDRESS:
                        {
			  char *network_address = decode_network_address(chassis_id);

			  strncat(result, "\tNetwork Address - ", 2048);

			  strncat(result, network_address, 2048);

			  free(network_address);
                        }break;
                    case CHASSIS_ID_INTERFACE_NAME:
                        {
			  strncat(result, "\tInterface Name - ", 2048);
			  debug_hex_strcat((uint8_t *)result, chassis_id, (tlv->length - 1));
                        }break;
                    case CHASSIS_ID_LOCALLY_ASSIGNED:
                        {
			  // According to 802.1AB 9.5.3.2, this is an alphanumeric
			  char *tmp = calloc(1, tlv->length + 1);

			  memcpy(tmp, tlv->info_string, tlv->length);

			  strncat(result, "\tLocally Assigned - ", 2048);

			  strncat(result, tmp, 255);

			  free(tmp);
                        }break;
                    default:
                        {
			  char *tmp = calloc(1, 2048);

			  sprintf(tmp, "\tReserved (%c)\n", tlv->info_string[0]);

			  strncat(result, tmp, 2048);

			  free(tmp);
                        }
                };
            }break;
        case PORT_ID_TLV:
            {
                uint8_t *port_id = &tlv->info_string[1];

		//	strncat(result, "Port ID: ", 2048);

                switch(tlv->info_string[0])
                {
                    case PORT_ID_INTERFACE_ALIAS:
                        {
			  strncat(result, "\tInterface Alias - ", 2048);
			  debug_hex_strcat((uint8_t *)result, port_id, tlv->length -1);
                        }break;
                    case PORT_ID_PORT_COMPONENT:
                        {
			  strncat(result, "\tPort Component - ", 2048);
			  debug_hex_strcat((uint8_t *)result, port_id, tlv->length -1);
                        }break;
                    case PORT_ID_MAC_ADDRESS:
                        {
			  strncat(result, "\tMAC Address - ", 2048);
			  debug_hex_strcat((uint8_t *)result, port_id, tlv->length -1);
                        }break;
                    case PORT_ID_NETWORK_ADDRESS:
                        {
			  char *network_address = decode_network_address(port_id);
			  strncat(result, "\tNetwork Address - ", 2048);

			  strncat(result, network_address, 2048);

			  free(network_address);
                        }break;
                    case PORT_ID_INTERFACE_NAME:
                        {
			  char *tmp = calloc(1, 18 + tlv->length + 1);

			  //char *cstr = tlv_info_string_to_cstr(tlv);
			  // The port interface name is 1 less than the lenght.
			  char *cstr = calloc(1, tlv->length);
			  memcpy(cstr, &tlv->info_string[1], tlv->length - 1);

			  if(cstr != NULL)
			    {
			      sprintf(tmp, "\tInterface Name - %s", cstr);
			      
			      free(cstr);

			      strncat(result, tmp, 2048);
			    }
			  
			  free(tmp);

                        }break;
                    case PORT_ID_LOCALLY_ASSIGNED:
		      {
			// According to 802.1AB 9.5.3.2, this is an alphanumeric
			char *tmp = calloc(1, tlv->length);
			
			memcpy(tmp, tlv->info_string, tlv->length);
			strncat(result, "\tLocally Assigned - ", 2048);

			//debug_hex_strcat(result, tlv->info_string, tlv->length - 1);

			strncat(result, tmp, 2048);

			free(tmp);;
		      }break;
                    default:
                        {
                          char *tmp = calloc(1, 2048);

                          sprintf(tmp, "\tReserved (%c)", tlv->info_string[0]);

                          strncat(result, tmp, 2048);

			  free(tmp);
                        }
                };
            }break;
        case TIME_TO_LIVE_TLV:
            {
                uint16_t *ttl = (uint16_t *)&tlv->info_string[0];
		
		char *tmp = calloc(1, 255);

		sprintf(tmp, "\t%d seconds", htons(*ttl));

		strncat(result, tmp, 2048);

		free(tmp);
	       
                //debug_printf(DEBUG_TLV, "Time To Live: %d seconds", htons(*ttl));
            }break;
        case PORT_DESCRIPTION_TLV:
            {
                char *port_description = calloc(1, tlv->length + 1);

                memcpy(port_description, tlv->info_string, tlv->length);
		
		//sprintf(tmp, "Port Description:\t%s\n", port_description);

		strncat(result, port_description, 2048);

		//strncat(result, tmp, 2048);

		//free(tmp);	      
		
            //debug_printf(DEBUG_TLV, "Port Description: %s\n", port_description);

                free(port_description);
            }break;
        case SYSTEM_NAME_TLV:
            {
	      char *tmp  = calloc(1, tlv->length + 2);
	      char *cstr = tlv_info_string_to_cstr(tlv);

	      if(cstr != NULL)
		{
		  snprintf(tmp, tlv->length + 2, "\t%s", cstr);	      
		  
		  free(cstr);
		  
		  strncat(result, tmp, strlen(tmp));
		}
	      

	      
	      free(tmp);
            }break;
        case SYSTEM_DESCRIPTION_TLV:
            {
	      char *cstr = tlv_info_string_to_cstr(tlv);

	      if(cstr != NULL)
		{
		  strncat(result, cstr, strlen(cstr));
		  free(cstr);
		}
            }break;
        case SYSTEM_CAPABILITIES_TLV:
            {
	      char *capabilities = NULL;
	      
	      uint16_t *system_capabilities = (uint16_t *)&tlv->info_string[0];
	      uint16_t *enabled_capabilities = (uint16_t *)&tlv->info_string[2];	    
	      
	      capabilities = decode_tlv_system_capabilities(htons(*system_capabilities), htons(*enabled_capabilities));

	      strncat(result, capabilities, 2048);

	      free(capabilities);

            }break;
        case MANAGEMENT_ADDRESS_TLV:
            {
	      //char *management_address = NULL;
	      char *management_address = decode_management_address(tlv);

	      if(management_address != NULL) {

		strncat(result, management_address, 2048);
		
		free(management_address);
	      } else {
		debug_hex_strcat((uint8_t *)result, tlv->info_string, tlv->length - 1);
	      }

            }break;
        case ORG_SPECIFIC_TLV:
            {
	      char *org_specific = decode_organizationally_specific_tlv(tlv);
	      
	      debug_hex_strcat((uint8_t *)result, tlv->info_string, tlv->length - 1);

	      free(org_specific);
            }break;
        case END_OF_LLDPDU_TLV:
            {
	      ; // Don't do anything here
            }break;
        default:
	  debug_printf(DEBUG_NORMAL, "Hit default case\n");

            debug_printf(DEBUG_NORMAL, "Got unrecognized type '%d'\n", tlv->type);
	    debug_hex_strcat((uint8_t *)result, tlv->info_string, tlv->length - 1);
    };

    //    strncat(result, "\n", 2048);
	    
	    return result;
}

char *decode_organizationally_specific_tlv(struct lldp_tlv *tlv) {
  #ifndef WIN32
  #warning Not implemented
  #endif // WIN32

  return NULL;
}

char *interface_subtype_name(uint8_t subtype) {

  switch(subtype) {
  case 1: 
    return strdup("Unknown");
    break;
  case 2:
    return strdup("ifIndex");
    break;
  case 3:
    return strdup("System Port Number");
    break;
  default:
    return strdup("Invalid Subtype");
  }
}

// http://www.iana.org/assignments/address-family-numbers
char *decode_iana_address_family(uint8_t family) {
  switch(family)
    {
    case IANA_RESERVED_LOW:
#warning - IANA_RESERVED_HIGH is defined as 65536, which is too big for uint8_t, so am I using the wrong type, or should it be 255?
    case IANA_RESERVED_HIGH:
      return strdup("Reserved");
    case IANA_IP:
      return strdup("IPv4");
    case IANA_IP6:
      return strdup("IPv6");
    case IANA_NSAP:
      return strdup("NSAP");
    case IANA_HDLC:
      return strdup("HDLC (8-bit multidrop)");
    case IANA_BBN_1822:
      return strdup("BBN 1822");
    case IANA_E_163:
      return strdup("E.163");
    case IANA_E_164_ATM:
      return strdup("E.164 (SMDS, Frame Relay, ATM)");
    case IANA_F_69:
      return strdup("F.69 (Telex)");
    case IANA_X_121:
      return strdup("X.121 (X.25, Frame Relay)");
    case IANA_IPX:
      return strdup("IPX");
    case IANA_APPLETALK:
      return strdup("Appletalk");
    case IANA_DECNET_IV:
      return strdup("Decnet IV");
    case IANA_BANYAN_VINES:
      return strdup("Banyan Vines");
    case IANA_E_164_NSAP:
      return strdup("E.164 with NSAP format subaddress");
    case IANA_DNS:
      return strdup("DNS (Domain Name System)");
    case IANA_DISTINGUISHED:
      return strdup("Distinguished Name");
    case IANA_AS_NUMBER:
      return strdup("AS Number");
    case IANA_XTP_IPV4:
      return strdup("XTP over IP version 4");
    case IANA_XTP_IPV6:
      return strdup("XTP over IP version 6");
    case IANA_FIBRE_PORT_NAME:
      return strdup("Fibre Channel World-Wide Port Name");
    case IANA_FIBRE_NODE_NAME:
      return strdup("Fibre Channel World-Wide Node name");
    case IANA_GWID:
      return strdup("GWID");
    case IANA_AFI_L2VPN:
      return strdup("AFI for L2VPN information");
    default:
      return strdup("Unassigned");
    }
}

char *decode_network_address(uint8_t *network_address) {
  char *result         = NULL;
  uint8_t addr_len     = 0;
  uint8_t subtype      = 0;
  char *addr_family    = NULL;
  uint8_t *addr        = NULL;
  uint8_t *tmp         = NULL;

  if(network_address == NULL)
    return NULL;

  addr_len    = network_address[0];
  subtype     = network_address[1];
  addr        = &network_address[2];
  addr_family = decode_iana_address_family(subtype);

  if(addr_family == NULL) {
    debug_printf(DEBUG_NORMAL, "NULL Address Family in %s\n", __FUNCTION__);
    return NULL;
  }

  result = calloc(1, 2048);

  sprintf(result, "%s - ", addr_family);

  free(addr_family);

  switch(subtype)
    {
    case IANA_IP:
      {
	if(addr_len == 5) {
	  tmp = calloc(1, 16);

	  sprintf((char *)tmp, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	  
	  strncat(result, (char *)tmp, 2048);
	  
	  free(tmp);
	} else {
	  debug_printf(DEBUG_NORMAL, "Invalid IPv4 address length: %d\n", addr_len);
	  debug_hex_strcat((uint8_t *)result, addr, addr_len - 1);
	}
      }break;
    default:
      debug_hex_strcat((uint8_t *)result, addr, addr_len - 1);
    }

  return result;
}

char *decode_management_address(struct lldp_tlv *tlv){
  uint8_t addr_len            = tlv->info_string[0];
  //uint8_t subtype             = tlv->info_string[1];
  //uint8_t *addr               = &tlv->info_string[2];
  uint8_t interface_subtype   = tlv->info_string[addr_len + 1];
  char *if_subtype_name       = NULL;
  uint32_t *interface_number  = (uint32_t *)&tlv->info_string[addr_len + 2];
  uint8_t oid_length          = tlv->info_string[addr_len + 6];
  char *oid_string            = NULL;
  char *addr_buf              = NULL;
  char *result = calloc(1, 2048);

  if(oid_length == 0) {
    oid_string = strdup("Standard LLDP MIB");
  } else {
    oid_string = strdup("Proprietary MIB");
  }

  if_subtype_name = interface_subtype_name(interface_subtype);

  addr_buf = decode_network_address(&tlv->info_string[0]);

  sprintf(result, "%s (%s - %d) (OID: %s)", 
	  addr_buf,
	  if_subtype_name,
	  (*interface_number),
	  oid_string
	  );

  free(addr_buf);
  free(oid_string);
  free(if_subtype_name);

  return result;
}


char *decode_tlv_system_capabilities( uint16_t system_capabilities, uint16_t enabled_capabilities)
{
    int capability = 0;
    char *result = calloc(1, 2048);
    char *capability_string = calloc(1, 2048);

    //    sprintf(result, "System Capabilities: %02X, Enabled Capabilities: %02X\n", system_capabilities, enabled_capabilities);
    //sprintf(result, "System Capabilities: \n");

    //debug_printf(DEBUG_TLV, "System Capabilities: %02X, Enabled Capabilities: %02X\n", system_capabilities, enabled_capabilities);

    //strncat(result, "\t\tCapabilities:", 2048);

    //debug_printf(DEBUG_TLV, "Capabilities:\n");

    for(capability = 1; capability <= 65535; capability *= 2)
    {
      memset(capability_string, 0x0, 2048);
	  
        if((system_capabilities & capability))
        {
            if(enabled_capabilities & capability)
            {
	      //debug_printf(DEBUG_TLV, "%s (enabled)\n", capability_name(capability));
	      sprintf(capability_string, "\n\t\t\t\t%s (enabled)", capability_name(capability));
	      strncat(result, capability_string, 2048);
            }
            else
            {
	      //debug_printf(DEBUG_TLV, "%s (disabled)\n", capability_name(capability));
	      sprintf(capability_string, "\n\t\t\t\t%s (disabled)", capability_name(capability));
	      strncat(result, capability_string, 2048);
            }
        } 
    }

    free(capability_string);

    return result;
}

char *capability_name(uint16_t capability)
{
    switch(capability)
    {
        case SYSTEM_CAPABILITY_OTHER:
            return "Other";
        case SYSTEM_CAPABILITY_REPEATER:
            return "Repeater/Hub";
        case SYSTEM_CAPABILITY_BRIDGE:
            return "Bridge/Switch";
        case SYSTEM_CAPABILITY_WLAN:
            return "Wireless LAN";
        case SYSTEM_CAPABILITY_ROUTER:
            return "Router";
        case SYSTEM_CAPABILITY_TELEPHONE:
            return "Telephone";
        case SYSTEM_CAPABILITY_DOCSIS:
            return "DOCSIS/Cable Modem";
        case SYSTEM_CAPABILITY_STATION:
            return "Station";
        default:
            return "Unknown";
    };
}


char *tlv_typetoname(uint8_t tlv_type)
{
    switch(tlv_type)
    {
        case CHASSIS_ID_TLV:
            return "Chassis ID";
            break;
        case PORT_ID_TLV:
            return "Port ID";
            break;
        case TIME_TO_LIVE_TLV:
            return "Time To Live";
            break;
        case PORT_DESCRIPTION_TLV:
            return "Port Description";
            break;
        case SYSTEM_NAME_TLV:
            return "System Name";
            break;
        case SYSTEM_DESCRIPTION_TLV:
            return "System Description";
            break;
        case SYSTEM_CAPABILITIES_TLV:
            return "System Capabiltiies";
            break;
        case MANAGEMENT_ADDRESS_TLV:
            return "Management Address";     
            break;
        case ORG_SPECIFIC_TLV:
            return "Organizationally Specific";
            break;
        case END_OF_LLDPDU_TLV:
            return "End Of LLDPDU";
            break;
        default:
            return "Unknown"; 
    };
}

uint8_t tlvInitializeLLDP(struct lldp_port *lldp_port)
{
    return 0;
}

void tlvCleanupLLDP()
{

}


uint8_t initializeTLVFunctionValidators()
{
    int index = 0;

    /* Set all of the reserved TLVs to NULL validator functions */
    /* so they're forced to go through the generic validator    */ 
    for(index = LLDP_BEGIN_RESERVED_TLV; index < LLDP_END_RESERVED_TLV; index++)
    {
        //debug_printf(DEBUG_EXCESSIVE, "Setting TLV Validator '%d' to NULL - it's reserved\n", index);

        validate_tlv[index] = NULL;
    }

    debug_printf(DEBUG_EXCESSIVE, "Setting TLV Validator '%d' - it's the organizational specific TLV validator...\n", ORG_SPECIFIC_TLV);

    validate_tlv[ORG_SPECIFIC_TLV] = validate_organizationally_specific_tlv;

    return 0;
}

struct lldp_tlv *initialize_tlv() {
  struct lldp_tlv *tlv = (struct lldp_tlv *)calloc(1, sizeof(struct lldp_tlv));

  return tlv;
}



struct lldp_tlv *create_end_of_lldpdu_tlv(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = END_OF_LLDPDU_TLV; // Constant defined in lldp_tlv.h
    tlv->length = 0;     // The End of LLDPDU TLV is length 0.

    tlv->info_string = NULL;

    return tlv;
}

uint8_t validate_end_of_lldpdu_tlv(struct lldp_tlv *tlv)
{
    if(tlv->length != 0)
    { 
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV type is 'End of LLDPDU' (0), but TLV length is %d when it should be 0!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

uint8_t validate_length_max_255(struct lldp_tlv *tlv)
{
    //Length will never be below 0 because the variable used is unsigned... 
    if(tlv->length > 255)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tIt should be between 0 and 255 inclusive!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

uint8_t validate_length_max_256(struct lldp_tlv *tlv)
{
    if(tlv->length < 2 || tlv->length > 256)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tIt should be between 2 and 256 inclusive!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

struct lldp_tlv *create_chassis_id_tlv_invalid_length(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = CHASSIS_ID_TLV; // Constant defined in lldp_tlv.h
    tlv->length = 7; //The size of a MAC + the size of the subtype (1 byte)

    tlv->info_string = calloc(1, tlv->length);  

    // CHASSIS_ID_MAC_ADDRESS is a 1-byte value - 4 in this case. Defined in lldp_tlv.h
    tlv->info_string[0] = CHASSIS_ID_MAC_ADDRESS;

    // We need to start copying at the 2nd byte, so we use [1] here...
    // This reads "memory copy to the destination at the address of tlv->info_string[1] with the source my_mac for 6 bytes" (the size of a MAC address)
    memcpy(&tlv->info_string[1], &lldp_port->source_mac[0], 6);

    return tlv;
}

struct lldp_tlv *create_chassis_id_tlv(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = CHASSIS_ID_TLV; // Constant defined in lldp_tlv.h
    tlv->length = 7; //The size of a MAC + the size of the subtype (1 byte)

    tlv->info_string = calloc(1, tlv->length);  

    // CHASSIS_ID_MAC_ADDRESS is a 1-byte value - 4 in this case. Defined in lldp_tlv.h
    tlv->info_string[0] = CHASSIS_ID_MAC_ADDRESS;

    // We need to start copying at the 2nd byte, so we use [1] here...
    // This reads "memory copy to the destination at the address of tlv->info_string[1] with the source my_mac for 6 bytes" (the size of a MAC address)
    memcpy(&tlv->info_string[1], &lldp_port->source_mac[0], 6);

    return tlv;
}

uint8_t validate_chassis_id_tlv(struct lldp_tlv *tlv)
{
    // Several TLVs have this requirement.
    return validate_length_max_256(tlv);
}

struct lldp_tlv *create_port_id_tlv(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = PORT_ID_TLV; // Constant defined in lldp_tlv.h
    tlv->length = 1 + strlen(lldp_port->if_name); //The length of the interface name + the size of the subtype (1 byte)

    tlv->info_string = calloc(1, tlv->length);

    // PORT_ID_INTERFACE_NAME is a 1-byte value - 5 in this case. Defined in lldp_tlv.h
    tlv->info_string[0] = PORT_ID_INTERFACE_NAME;


    // We need to start copying at the 2nd byte, so we use [1] here...
    // This reads "memory copy to the destination at the address of tlv->info_string[1] with the source lldp_port->if_name for strlen(lldp_port->if_name) bytes"
    memcpy(&tlv->info_string[1], lldp_port->if_name, strlen(lldp_port->if_name));

    return tlv;
}

uint8_t validate_port_id_tlv(struct lldp_tlv *tlv)
{
    // Several TLVs have this requirement.
    return validate_length_max_256(tlv);
}

struct lldp_tlv *create_ttl_tlv(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();
    uint16_t ttl = htons(lldp_port->tx.txTTL);

    tlv->type = TIME_TO_LIVE_TLV; // Constant defined in lldp_tlv.h
    tlv->length = 2; // Static length defined by IEEE 802.1AB section 9.5.4

    tlv->info_string = calloc(1, tlv->length);

    memcpy(tlv->info_string, &ttl, tlv->length);

    return tlv;
}

uint8_t validate_ttl_tlv(struct lldp_tlv *tlv)
{
    if(tlv->length != 2)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tLength should be '2'.\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

struct lldp_tlv *create_port_description_tlv(struct lldp_port *lldp_port) {

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = PORT_DESCRIPTION_TLV; // onstant defined in lldp_tlv.h
    tlv->length = strlen(lldp_port->if_name);

    tlv->info_string = calloc(1, tlv->length);

    memcpy(&tlv->info_string[0], lldp_port->if_name, strlen(lldp_port->if_name));

    return tlv;

}


uint8_t validate_port_description_tlv(struct lldp_tlv *tlv)
{
    // Several TLVs have this requirement.
    return validate_length_max_255(tlv);
}

struct lldp_tlv *create_system_name_tlv(struct lldp_port *lldp_port) 
{

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = SYSTEM_NAME_TLV; // Constant defined in lldp_tlv.h
    tlv->length = strlen(lldp_systemname);

    tlv->info_string = calloc(1, tlv->length);

    memcpy(tlv->info_string, lldp_systemname, tlv->length); 

    return tlv;
}

uint8_t validate_system_name_tlv(struct lldp_tlv *tlv)
{
    // Several TLVs have this requirement.
    return validate_length_max_255(tlv);
}

struct lldp_tlv *create_system_description_tlv(struct lldp_port *lldp_port)
{

    struct lldp_tlv* tlv = initialize_tlv();

    tlv->type = SYSTEM_DESCRIPTION_TLV; // Constant defined in lldp_tlv.h

    tlv->length = strlen(lldp_systemdesc);

    tlv->info_string = calloc(1, tlv->length);

    memcpy(tlv->info_string, lldp_systemdesc, tlv->length);

    return tlv;

}

uint8_t validate_system_description_tlv(struct lldp_tlv *tlv)
{
    // Several TLVs have this requirement.
    return validate_length_max_255(tlv);
}

struct lldp_tlv *create_system_capabilities_tlv(struct lldp_port *lldp_port) {
    struct lldp_tlv* tlv = initialize_tlv();
    // Tell it we're a station for now... bit 7
    uint16_t capabilities = htons(128);

    tlv->type = SYSTEM_CAPABILITIES_TLV; // Constant defined in lldp_tlv.h

    tlv->length = 4;

    tlv->info_string = calloc(1, tlv->length);

    memcpy(&tlv->info_string[0], &capabilities, sizeof(uint16_t));
    memcpy(&tlv->info_string[2], &capabilities, sizeof(uint16_t));

    return tlv;
}

uint8_t validate_system_capabilities_tlv(struct lldp_tlv *tlv)
{
    if(tlv->length != 4)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tLength should be '4'.\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

// NB: Initial deployment will do IPv4 only... 
// 
struct lldp_tlv *create_management_address_tlv(struct lldp_port *lldp_port) {
   struct lldp_tlv *tlv = initialize_tlv();
   uint32_t if_index = lldp_port->if_index;

    tlv->type = MANAGEMENT_ADDRESS_TLV; // Constant defined in lldp_tlv.h

#define MGMT_ADDR_STR_LEN 1
#define MGMT_ADDR_SUBTYPE 1
#define IPV4_LEN 4
#define IF_NUM_SUBTYPE 1
#define IF_NUM 4
#define OID 1
#define OBJ_IDENTIFIER 0

    // management address string length (1 octet)
    // management address subtype (1 octet)
    // management address (4 bytes for IPv4)
    // interface numbering subtype (1 octet)
    // interface number (4 bytes)
    // OID string length (1 byte)
    // object identifier (0 to 128 octets)
    tlv->length = MGMT_ADDR_STR_LEN + MGMT_ADDR_SUBTYPE + IPV4_LEN + IF_NUM_SUBTYPE + IF_NUM + OID + OBJ_IDENTIFIER ;

    //uint64_t tlv_offset = 0;

    tlv->info_string = calloc(1, tlv->length);

    // Management address string length
    // subtype of 1 byte + management address length, so 5 for IPv4
    tlv->info_string[0] = 5;

    // 1 for IPv4 as per http://www.iana.org/assignments/address-family-numbers
    tlv->info_string[1] = 1;

    // Copy in our IP
    memcpy(&tlv->info_string[2], lldp_port->source_ipaddr, 4);

    // Interface numbering subtype... system port number in our case.
    tlv->info_string[6] = 3;

    // Interface number... 4 bytes long, or uint32_t
    memcpy(&tlv->info_string[7], &lldp_port->if_index, sizeof(uint32_t));

    debug_printf(DEBUG_NORMAL, "Would stuff interface #: %d\n", if_index);

    // OID - 0 for us
    tlv->info_string[11] = 0;

    // object identifier... doesn't exist for us because it's not required, and we don't have an OID.

    return tlv;
}

uint8_t validate_management_address_tlv(struct lldp_tlv *tlv)
{
    if(tlv->length < 9 || tlv->length > 167)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tIt should be between 9 and 167 inclusive!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}




//***********************************
//*LLDP-MED Location Indentification*
//***********************************

struct lldp_tlv *create_lldpmed_location_identification_tlv (struct lldp_port *lldp_port)
{
  int j;  
  int len = 0;
  int pos = 0;
  struct lldp_tlv *tlv = initialize_tlv();


  //just 1, 2 or 3 is allowed for location data format, 0 is invalid, 4-255 is reserved for future use
  if ((lci.location_data_format < 1) || (lci.location_data_format > 3))
    {
      debug_printf (DEBUG_NORMAL, "[ERROR] in config file: invalid location_data_format '%d' \n", lci.location_data_format);

      free(tlv);

      return NULL;
    }

  //set CATypes with no content "" to NULL, otherwise CAType with length 0 is created
  for (j = 0; j < 33; j++)
    {
      if ((lci.civic_ca[j] != NULL) && (strcmp (lci.civic_ca[j], "") == 0))
	lci.civic_ca[j] = NULL;
    }
  
  //calculate LCI Length for Civic Location
  if (lci.location_data_format == LCI_CIVIC)
    {
      for (j = 0; j < 33; j++)
	{
	  //printf ("%i\n", j);
	  //(strlen(civic_ca[j]) > 0) &&
	  if ((lci.civic_ca[j] != NULL))
	    len += strlen (lci.civic_ca[j]) + 1 + 1;//length of CAvalue + CAtype + CAlength
	}
      len += 1 + 2;//add len for What and Countrycode
      tlv->length = 1 + len + 4 + 1;//1 for the LCI length field, LCI length plus 4 for the MED Header and 1 for Location Data Format
    }
  
  //length for coordinate based
  if (lci.location_data_format == LCI_COORDINATE)
    {
      int tmp = strlen (lci.coordinate_based_lci) / 2;
      if (tmp == 23)
	debug_printf (DEBUG_NORMAL, "Coordinate based LCI contains colons\n");
      
      if ((tmp != 16) && (tmp != 23))
	debug_printf (DEBUG_NORMAL, "coordinate_based_lci has wrong length '%d' \n", tmp);
      
      tlv->length = 16 + 5;//MED Header (5) + geo location is always 16
      //printf ("geo loc data length calculated = %i \n", tlv->length);
    }
  
  //length for ELIN
  if (lci.location_data_format == LCI_ELIN)
    tlv->length = strlen (lci.elin) + 5;
  
  tlv->type = ORG_SPECIFIC_TLV;
  
  tlv->info_string = calloc(1, tlv->length);
  
  //MED-Header
  tlv->info_string[0] = 0x00;
  tlv->info_string[1] = 0x12;
  tlv->info_string[2] = 0xBB;
  tlv->info_string[3] = 3;
  
  //set Location Data Format
  tlv->info_string[4] = lci.location_data_format;
  
  
  //--------------------
  // create Location ID
  //--------------------

  //handle civic location LCI
  if (lci.location_data_format == 2)
    {
      tlv->info_string[5] = len;//LCI Length
      tlv->info_string[6] = lci.civic_what;
      tlv->info_string[7] = lci.civic_countrycode[0];
      tlv->info_string[8] = lci.civic_countrycode[1];
      pos = 9;
      
      debug_printf (DEBUG_NORMAL, "create civic location identification TLV\n");
      //for(j = 0; j < 33; j++)
      //printf("CA typ %i len(%i) = %s \n", j, strlen(civic_ca[j]), civic_ca[j]);
      //printf("nr 12 %s",civic_ca[12]);
      
      for (j = 0; j < 33; j++)
	{
	  //(strlen(civic_ca[j]) > 1) &&
	  if ((lci.civic_ca[j] != NULL))
	    {
	      int calength = strlen (lci.civic_ca[j]);
	      debug_printf (DEBUG_NORMAL, "CA Type %i with len %i contains %s \n", j,
			    calength, lci.civic_ca[j]);
	      tlv->info_string[pos] = j;
	      pos++;
	      tlv->info_string[pos] = calength;
	      pos++;
	      memcpy(&tlv->info_string[pos], lci.civic_ca[j], calength);
	      pos += calength;
	    }
	}
       }
  
  //handle ECS ELIN data format
  if (lci.location_data_format == 3)
    memcpy(&tlv->info_string[5], lci.elin, strlen(lci.elin));
  
  //handling coordinate-based LCI
  if (lci.location_data_format == 1)
    {
      char temp[3];
      char out[17]; // binary location + string terminator
      char *in = calloc (1, strlen(lci.coordinate_based_lci) + 1);
      int i, u;
      int counter = 0;
      //remove colons from string
      for (u = 0; u < strlen (lci.coordinate_based_lci); u++)
	{
	  if (lci.coordinate_based_lci[u] != ':')
	    {
	      in[counter] = lci.coordinate_based_lci[u];
	      counter++;
	    }
	}
      
      in[32] = '\0'; // initialize string terminators
      out[16] = '\0';
      temp[2] = '\0'; 
      
            debug_printf (DEBUG_NORMAL, "coordinate_based_lci: %s\n", lci.coordinate_based_lci);
	    debug_printf (DEBUG_NORMAL, "coordinate_based without colons: %s \n", in);
	    
	    //convert string to hex
	    for (i = 0; i < 16; i++)
	      {
		temp[0] = in[i * 2];
		temp[1] = in[i * 2 + 1];
		out[i] = (char) strtol (temp, NULL, 16);
	      }
	    
	    free (in);
	    
	    debug_printf (DEBUG_NORMAL, "out: ");
	    for (i = 0; i < 16; i++)
	      {
		//proper output requires unsigned char cast
	        debug_printf (DEBUG_NORMAL, "%02x", (unsigned char)out[i]);
	      }
	    debug_printf (DEBUG_NORMAL, "\n");
	    
	    memcpy(&tlv->info_string[5], out, 16);
    }
  
  return tlv;
}
//****************************LLDP-MED location identification end **************************


uint8_t validate_organizationally_specific_tlv(struct lldp_tlv *tlv)
{
    if(tlv->length < 4 || tlv->length > 511)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tIt should be between 4 and 511 inclusive!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}

uint8_t validate_generic_tlv(struct lldp_tlv *tlv)
{
    debug_printf(DEBUG_TLV, "Generic TLV Validation for TLV type: %d.\n", tlv->type);
    debug_printf(DEBUG_TLV, "TLV Info String Length: %d\n", tlv->length);
    debug_printf(DEBUG_TLV, "TLV Info String: ");
    debug_hex_dump(DEBUG_TLV, tlv->info_string, tlv->length);

    // Length will never fall below 0 because it's an unsigned variable
    if(tlv->length > 511)
    {
        debug_printf(DEBUG_NORMAL, "[ERROR] TLV has invalid length '%d'.\n\tIt should be between 0 and 511 inclusive!\n", tlv->length);

        return XEINVALIDTLV;
    }

    return XVALIDTLV;
}



// A helper function to explode a flattened TLV.
struct lldp_tlv *explode_tlv(struct lldp_flat_tlv *flat_tlv) {

    uint16_t type_and_length = 0;
    struct lldp_tlv *tlv   = NULL;

    tlv = calloc(1, sizeof(struct lldp_tlv));

    if(tlv) {

        // Suck the type and length out...
        type_and_length = *(uint16_t *)&tlv[0];

        tlv->length     = type_and_length & 511;
        type_and_length = type_and_length >> 9;
        tlv->type       = (uint8_t)type_and_length;

        tlv->info_string = calloc(1, tlv->length);

        if(tlv->info_string) {
            // Copy the info string into our TLV...
            memcpy(&tlv->info_string[0], &flat_tlv->tlv[sizeof(type_and_length)], tlv->length);
        } else { // tlv->info_string == NULL
            debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
        }
    } else { // tlv == NULL
        debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);    
    }

    return tlv;
}

uint8_t tlvcpy(struct lldp_tlv *dst, struct lldp_tlv *src)
{
  if(src == NULL)
    return -1;

  if(dst == NULL)
    return -1;
  
  dst->type = src->type;
  dst->length = src->length;
  dst->info_string = calloc(1, dst->length);
  
  if(((dst->info_string != NULL) && (src->info_string != NULL)))
    {
      memcpy(dst->info_string, src->info_string, dst->length);
    }
  else
    {
      debug_printf(DEBUG_NORMAL, "[ERROR] Couldn't allocate memory!!\n");
      
      return -1;
    }
  
  return 0;
}

struct lldp_tlv *tlvpop(uint8_t *buffer)
{
    return NULL;
}

uint8_t tlvpush(uint8_t *buffer, struct lldp_tlv *tlv)
{
    return -1;
}

uint8_t lldp_cache_tlv(struct lldp_tlv *tlv)
{
    return -1;
}

char *tlv_info_string_to_cstr(struct lldp_tlv *tlv)
{
  char *cstr = NULL;

  if(tlv == NULL)
    return NULL;

  if(tlv->length <= 0)
    return NULL;

  if(tlv->info_string == NULL)
    return NULL;

  cstr = calloc(1, tlv->length + 1);
  
  memcpy(cstr, tlv->info_string, tlv->length);

  return cstr;
}
