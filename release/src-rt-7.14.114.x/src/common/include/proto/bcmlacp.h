/*
 * LACP protocol definitions
 *
 * $Copyright Open Broadcom Corporation$
 *
 * $Id:
 */

#ifndef _bcmlacp_h_
#define _bcmlacp_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* subtype of slow protocol */
#define SLOW_SUBTYPE_LACP	1

/* Link Aggregation Control Protocol(LACP) data unit structure
 * (43.4.2.2 in the 802.3ad standard)
 */
typedef BWL_PRE_PACKED_STRUCT struct lacpdu {
	int8 subtype;			/* LACP(= 0x01) */
	int8 version_number;
	int8 tlv_type_actor_info;	/* actor information(type/length/value) */
	int8 actor_information_length; 	/* 20 */
	int16 actor_system_priority;
	int8 actor_system[ETH_ALEN];
	int16 actor_key;
	int16 actor_port_priority;
	int16 actor_port;
	int8 actor_state;
	int8 reserved_3_1[3];		/* 0 */
	int8 tlv_type_partner_info;     /* partner information */
	int8 partner_information_length; /* 20 */
	int16 partner_system_priority;
	int8 partner_system[ETH_ALEN];
	int16 partner_key;
	int16 partner_port_priority;
	int16 partner_port;
	int8 partner_state;
	int8 reserved_3_2[3];		/* 0 */
	int8 tlv_type_collector_info;	/* collector information */
	int8 collector_information_length; /* 16 */
	int16 collector_max_delay;
	int8 reserved_12[12];
	int8 tlv_type_terminator;	/* terminator */
	int8 terminator_length;		/* 0 */
	int8 reserved_50[50];		/* 0 */
} BWL_POST_PACKED_STRUCT lacpdu_t;

typedef BWL_PRE_PACKED_STRUCT struct lacpdu_header {
	struct ethhdr hdr;
	struct lacpdu lacpdu;
} BWL_POST_PACKED_STRUCT lacpdu_header_t;

#define LACP_SET_SRC_PID(lacph, src_pid)	(((lacpdu_t *)(lacph))->reserved_3_2[0] = src_pid)
#define LACP_GET_SRC_PID(lacph)			(((lacpdu_t *)(lacph))->reserved_3_2[0])

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _bcmlacp_h_ */
