/** @file tlv_00120f.c

License: See LICENSE file for more info.

Authors: Terry Simons (terry.simons@gmail.com)
 */

#include "tlv_00120f.h"
#include "tlv_common.h"
#include "tlv.h"
#include "lldp_port.h"
#include <stdlib.h>
#include <string.h>

// MAC/PHY Configuration/Status TLV
struct lldp_tlv *create_mac_phy_configuration_status_tlv(struct lldp_port *lldp_port) {
  struct lldp_tlv *tlv = initialize_tlv();
  char OUI[3]  = {0x00, 0x01, 0x0F};
  char subtype = 1;

  if(tlv == NULL)
    return NULL;

  tlv->type = MAC_PHY_CONFIGURATION_STATUS_TLV; // Constant defined in tlv_00120f.h
  tlv->length = 9;                       // The MAC/PHY Configuration/Status TLV length is always 9.

  tlv->info_string = malloc(9);

  if(tlv->info_string == NULL) {
    destroy_tlv(&tlv);
    return NULL;
  }

  memcpy(&tlv->info_string[0], OUI, 3);

  memcpy(&tlv->info_string[3], &subtype, 1);
  /*
  memcpy(&tlv->info_string[4], &lldp_port->autoNegCapabilitiesAndSupport, 1);

  memcpy(&tlv->info_string[5], &lldp_port->ifMauAutoNegCapAdvertisedBits, 2);
  
  memcpy(&tlv->info_string[7], &lldp_port->ifMauType, 2);
  */ 

  return tlv;  
}

// Power Via MDI TLV

struct lldp_tlv *create_power_via_mdi_tlv(struct lldp_port *lldp_port) {
  struct lldp_tlv* tlv = initialize_tlv();
  
  char OUI[3]  = {0x00, 0x01, 0x0F};
  char subtype = 2;

  if(tlv == NULL)
    return NULL;

  tlv->type = POWER_VIA_MDI_TLV;           // Constant defined in tlv_00120f.h
  tlv->length = 7; // The Power Via MDI TLV length is always 7.

  tlv->info_string = malloc(7);

  if(tlv->info_string == NULL) {
    destroy_tlv(&tlv);
    return NULL;
  }

  memcpy(tlv->info_string, OUI, 3);

  memcpy(tlv->info_string, &subtype, 1);

  // Are there any devices in *NIX that support PSE or PD as per RFC 3621?
 
  return tlv;  
}

uint8_t validate_power_via_mdi_tlv(struct lldp_tlv *tlv) {
  return 0;
}

char *decode_power_via_mdi_tlv(struct lldp_tlv *tlv) {
  return NULL;
}

// Register verification callback based on OID?
