/*
 * OID encapsulation defines for user-mode to driver interface.
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: oidencap.h,v 13.8.586.1 2010-06-09 18:11:29 Exp $
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

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif /* _oidencap_h_ */
