/** @file tlv_common.h
 * 
 * OpenLLDP TLV Common Routines Header
 * 
 * See LICENSE file for more info.
 * 
 * File: tlv_common.h
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
*/

#ifndef LLDP_TLV_COMMON_H
#define LLDP_TLV_COMMON_H

#include <sys/types.h>

#ifdef WIN32
#include "stdintwin.h"
#else
#include <stdint.h>
#endif

struct lldp_flat_tlv {
    uint16_t size;
    uint8_t *tlv;
};

struct lldp_tlv {
    uint8_t  type;
    uint16_t length;
    uint8_t  *info_string;
};

struct lldp_organizational_tlv {
    uint8_t oui[3];
    uint8_t oui_subtype;
};

struct lldp_tlv_list {
    struct lldp_tlv_list *next;
    struct lldp_tlv *tlv;
};

void add_tlv(struct lldp_tlv *tlv, struct lldp_tlv_list **tlv_list);

struct lldp_flat_tlv *flatten_tlv(struct lldp_tlv *tlv);
struct lldp_tlv *explode_tlv(struct lldp_flat_tlv *flat_tlv);

uint8_t validate_generic_tlv(struct lldp_tlv *tlv);

void destroy_tlv(struct lldp_tlv **tlv);
void destroy_flattened_tlv(struct lldp_flat_tlv **tlv);

void destroy_tlv_list(struct lldp_tlv_list **tlv_list);

#endif /* LLDP_TLV_COMMON_H */
