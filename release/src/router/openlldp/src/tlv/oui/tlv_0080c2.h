/** @file tlv_0080c2.h

License: See LICENSE file for more info.

Author: Terry Simons (terry.simons@gamail.com)
 */

#ifndef LLDP_TLV_0080C2_H
#define LLDP_TLV_0080C2_H

/* 802.1AB Annex F IEEE 802.1 Organizationally Specific TLVs */
/* 0 reserved */
#define PORT_VLAN_ID_TLV              1  /* Subclause Reference: F.2 */
#define PORT_AND_PROTOCOL_VLAN_ID_TLV 2  /* Subclause Reference: F.3 */
#define VLAN_NAME_TLV                 3  /* Subclause Reference: F.4 */
#define PROTOCOL_IDENTITY_TLV         4  /* Subclause Reference: F.5 */
/* 5 - 255 reserved */
/* End 802.1AB Annex F IEEE 802.1 Organizationally Specific TLVs */

/* IEEE 802.1 Port and Protocol VLAN ID Capability/Status Masks */
/* bit 0 reserved */
#define PORT_AND_PROTOCOL_VLAN_SUPPORTED 2 /* bit 1 */
#define PORT_AND_PROTOCOL_VLAN_ENABLED   4 /* bit 2 */
/* bits 3 - 7 reserved (set to zero) */
/* End IEE 802.1 Port and Protocol VLAN ID Capability/Status Masks */

#endif /* LLDP_TLV_0080C2_H */
