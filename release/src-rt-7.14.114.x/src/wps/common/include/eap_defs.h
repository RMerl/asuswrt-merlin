/*
 * EAP defines
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: eap_defs.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef EAP_DEFS_H
#define EAP_DEFS_H

/* enable structure packing */
#include <packed_section_start.h>

/* RFC 3748 - Extensible Authentication Protocol (EAP) */

BWL_PRE_PACKED_STRUCT struct eap_hdr {
	uint8 code;
	uint8 identifier;
	/*
	 * including code and identifier; network byte order
	 * followed by length-4 octets of data
	 */
	uint16 length;
} BWL_POST_PACKED_STRUCT;

enum {
	EAP_CODE_REQUEST = 1,
	EAP_CODE_RESPONSE = 2,
	EAP_CODE_SUCCESS = 3,
	EAP_CODE_FAILURE = 4
};

/*
 * EAP Request and Response data begins with one octet Type. Success and
 * Failure do not have additional data.
 */
typedef enum {
	EAP_TYPE_NONE = 0,
	EAP_TYPE_IDENTITY = 1 /* RFC 3748 */,
	EAP_TYPE_NOTIFICATION = 2 /* RFC 3748 */,
	EAP_TYPE_NAK = 3 /* Response only, RFC 3748 */,
	EAP_TYPE_MD5 = 4, /* RFC 3748 */
	EAP_TYPE_OTP = 5 /* RFC 3748 */,
	EAP_TYPE_GTC = 6, /* RFC 3748 */
	EAP_TYPE_TLS = 13 /* RFC 2716 */,
	EAP_TYPE_LEAP = 17 /* Cisco proprietary */,
	EAP_TYPE_SIM = 18 /* draft-haverinen-pppext-eap-sim-12.txt */,
	EAP_TYPE_TTLS = 21 /* draft-ietf-pppext-eap-ttls-02.txt */,
	EAP_TYPE_AKA = 23 /* draft-arkko-pppext-eap-aka-12.txt */,
	EAP_TYPE_PEAP = 25 /* draft-josefsson-pppext-eap-tls-eap-06.txt */,
	EAP_TYPE_MSCHAPV2 = 26 /* draft-kamath-pppext-eap-mschapv2-00.txt */,
	EAP_TYPE_TLV = 33 /* draft-josefsson-pppext-eap-tls-eap-07.txt */,
	EAP_TYPE_FAST = 43 /* draft-cam-winget-eap-fast-00.txt */,
	EAP_TYPE_PAX = 46, /* draft-clancy-eap-pax-04.txt */
	EAP_TYPE_EXPANDED_NAK = 253 /* RFC 3748 */,
	EAP_TYPE_WPS = 254 /* Wireless Simple Config */,
	EAP_TYPE_PSK = 255 /* EXPERIMENTAL - type not yet allocated draft-bersani-eap-psk-09 */
} EapType;

/* WPS relative define */
#define WPS_EAP_DATA_MAX_LENGTH	2048

/* WPS Message types */
#define WPS_Start			0x01
#define WPS_ACK				0x02
#define WPS_NACK			0x03
#define WPS_MSG				0x04
#define WPS_Done			0x05
#define WPS_FRAG_ACK			0x06

#define WPS_VENDORID1		0x00
#define WPS_VENDORID2		0x37
#define WPS_VENDORID3		0x2A
#define WPS_VENDORTYPE		0x00000001

#define EAP_WPS_FRAG_MAX	1398
#define EAP_WPS_FLAGS_MF	0x01	/* More fragmentation */
#define EAP_WPS_FLAGS_LF	0x02	/* Length information, this is WPS data total length */

#define EAP_WPS_LF_OFFSET	2	/* 2 bytes of EAP_WPS_FLAGS_LF */


typedef BWL_PRE_PACKED_STRUCT struct wps_eap_header_tag {
	uint8 code;
	uint8 id;
	uint16 length;
	uint8 type;
	uint8 vendorId[3];
	uint32 vendorType;
	uint8 opcode;
	uint8 flags;
} BWL_POST_PACKED_STRUCT WpsEapHdr;

#include <packed_section_end.h>
#endif /* EAP_DEFS_H */
