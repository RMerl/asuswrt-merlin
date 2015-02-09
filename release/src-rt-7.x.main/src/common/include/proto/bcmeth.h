/*
 * Broadcom Ethernettype  protocol definitions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmeth.h 445748 2013-12-30 12:57:34Z $
 */

/*
 * Broadcom Ethernet protocol defines
 */

#ifndef _BCMETH_H_
#define _BCMETH_H_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* ETHER_TYPE_BRCM is defined in ethernet.h */

/*
 * Following the 2byte BRCM ether_type is a 16bit BRCM subtype field
 * in one of two formats: (only subtypes 32768-65535 are in use now)
 *
 * subtypes 0-32767:
 *     8 bit subtype (0-127)
 *     8 bit length in bytes (0-255)
 *
 * subtypes 32768-65535:
 *     16 bit big-endian subtype
 *     16 bit big-endian length in bytes (0-65535)
 *
 * length is the number of additional bytes beyond the 4 or 6 byte header
 *
 * Reserved values:
 * 0 reserved
 * 5-15 reserved for iLine protocol assignments
 * 17-126 reserved, assignable
 * 127 reserved
 * 32768 reserved
 * 32769-65534 reserved, assignable
 * 65535 reserved
 */

/*
 * While adding the subtypes and their specific processing code make sure
 * bcmeth_bcm_hdr_t is the first data structure in the user specific data structure definition
 */

#define	BCMILCP_SUBTYPE_RATE		1
#define	BCMILCP_SUBTYPE_LINK		2
#define	BCMILCP_SUBTYPE_CSA		3
#define	BCMILCP_SUBTYPE_LARQ		4
#define BCMILCP_SUBTYPE_VENDOR		5
#define	BCMILCP_SUBTYPE_FLH		17

#define BCMILCP_SUBTYPE_VENDOR_LONG	32769
#define BCMILCP_SUBTYPE_CERT		32770
#define BCMILCP_SUBTYPE_SES		32771


#define BCMILCP_BCM_SUBTYPE_RESERVED		0
#define BCMILCP_BCM_SUBTYPE_EVENT		1
#define BCMILCP_BCM_SUBTYPE_SES			2
/*
 * The EAPOL type is not used anymore. Instead EAPOL messages are now embedded
 * within BCMILCP_BCM_SUBTYPE_EVENT type messages
 */
/* #define BCMILCP_BCM_SUBTYPE_EAPOL		3 */
#define BCMILCP_BCM_SUBTYPE_DPT                 4

#define BCMILCP_BCM_SUBTYPEHDR_MINLENGTH	8
#define BCMILCP_BCM_SUBTYPEHDR_VERSION		0

/* These fields are stored in network order */
typedef BWL_PRE_PACKED_STRUCT struct bcmeth_hdr
{
	uint16	subtype;	/* Vendor specific..32769 */
	uint16	length;
	uint8	version;	/* Version is 0 */
	uint8	oui[3];		/* Broadcom OUI */
	/* user specific Data */
	uint16	usr_subtype;
} BWL_POST_PACKED_STRUCT bcmeth_hdr_t;


/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/*  _BCMETH_H_ */
