/*
 * OID encapsulation defines for user-mode to driver interface.
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: oidencap.h 342966 2012-07-05 02:40:08Z $
 */

#ifndef _oidencap_h_
#define	_oidencap_h_

/*
 * NOTE: same as OID_EPI_BASE defined in epiioctl.h
 */
#define OID_BCM_BASE					0xFFFEDA00

/*
 * These values are now set in stone to preserve forward
 * binary compatibility.
 */
#define	OID_BCM_SETINFORMATION 			(OID_BCM_BASE + 0x3e)
#define	OID_BCM_GETINFORMATION 			(OID_BCM_BASE + 0x3f)
#define OID_DHD_IOCTLS					(OID_BCM_BASE + 0x41)


#define	OIDENCAP_COOKIE	0xABADCEDE

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

/*
 * In the following two structs keep cookie as last element
 * before data. This allows struct validation when fields
 * are added or deleted.  The data immediately follows the
 * structure and is required to be 4-byte aligned.
 *
 * OID_BCM_SETINFORMATION uses setinformation_t
 * OID_BCM_GETINFORMATION uses getinformation_t
*/
typedef struct _setinformation {
    ULONG cookie;	/* OIDENCAP_COOKIE */
    ULONG oid;	/* actual OID value for set */
} setinformation_t;

#define SETINFORMATION_SIZE			(sizeof(setinformation_t))
#define SETINFORMATION_DATA(shdr)		((UCHAR *)&(shdr)[1])

typedef struct _getinformation {
    ULONG oid;	/* actual OID value for query */
    ULONG len;	/* length of response buffer, including this header */
    ULONG cookie;	/* OIDENCAP_COOKIE; altered by driver if more data available */
} getinformation_t;

#define GETINFORMATION_SIZE			(sizeof(getinformation_t))
#define GETINFORMATION_DATA(ghdr)		((UCHAR *)&(ghdr)[1])

typedef struct _reqinformation_hdr {
	ULONG version; /* REQINFORMATION_XXX_VERSION */
	ULONG cookie;  /* OIDENCAP_COOKIE; altered by driver if more data available */
	ULONG len;     /* REQINFORMATION_XXX_SIZE */
} reqinformation_hdr_t;

#define REQINFORMATION_HDR_SIZE			(sizeof(reqinformation_hdr_t))

/* This structure should be used as a replacement to
 * getinfomation_t and setinformation_t.
 * When new fields are added to this structure, add them to the end
 * and increment the version field.
*/
typedef struct _reqinformation_0 {
	reqinformation_hdr_t hdr;
	ULONG oid;     /* actual OID value for the request */
	ULONG idx;     /* bsscfg index */
	ULONG status;  /* NDIS_STATUS for actual OID */
	/* Add new fields here... */
	/* 4-byte aligned data follows */
} reqinformation_0_t;

#define REQINFORMATION_0_VERSION		0
#define REQINFORMATION_0_SIZE			(sizeof(reqinformation_0_t))
#define REQINFORMATION_0_DATA(ghdr)		((UCHAR *)(ghdr) + REQINFORMATION_0_SIZE)

typedef reqinformation_0_t reqinformation_t;

#define REQINFORMATION_VERSION			REQINFORMATION_0_VERSION
#define REQINFORMATION_SIZE			REQINFORMATION_0_SIZE
#define REQINFORMATION_DATA(ghdr)		REQINFORMATION_0_DATA(ghdr)

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif /* _oidencap_h_ */
