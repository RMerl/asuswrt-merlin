#ifndef LLDP_MSAP_H
#define LLDP_MSAP_H
/** @file msap.h
    OpenLLDP MSAP Cache Header

    See LICENSE file for more info.
 
    Authors: Terry Simons (terry.simons@gmail.com)
*/

#include "lldp_port.h"
#include "tlv/tlv_common.h"

struct lldp_msap *create_msap(struct lldp_tlv *tlv1, struct lldp_tlv *tlv2);
void update_msap(struct lldp_port *lldp_port, struct lldp_msap *msap_cache);
void update_msap_cache(struct lldp_port *lldp_port, struct lldp_msap* msap_cache);
void iterate_msap_cache(struct lldp_msap *msap_cache);
#endif


