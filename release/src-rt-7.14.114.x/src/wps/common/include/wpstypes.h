/*
 * WPS TYPE
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpstypes.h 291174 2011-10-21 07:57:07Z $
 */

#ifndef _WPS_TYPE_
#define _WPS_TYPE_

#if !defined(WIN32)
#ifdef __ECOS
typedef void *uintptr_t;
#else /* !__ECOS */
#include <stdint.h> /* for uintptr_t */
#endif /* __ECOS */
#endif /* !WIN32 */

#define UINT2PTR(val)   ((void *)(uintptr_t)(uint32)(val))
#define PTR2UINT(val)   ((uint32)(uintptr_t)(val))

#ifndef	BOOL
#define	BOOL int
#endif

#ifdef __cplusplus
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
#endif

#ifdef _MSC_VER
#pragma warning(disable:4786)   /* identifier truncated to 255 characters in debug info */
typedef unsigned __int64 uint64;
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#define SIZE_1_BYTE         1
#define SIZE_2_BYTES        2
#define SIZE_3_BYTES        3
#define SIZE_4_BYTES        4
#define SIZE_6_BYTES        6
#define SIZE_8_BYTES        8
#define SIZE_16_BYTES       16
#define SIZE_20_BYTES       20
#define SIZE_32_BYTES       32
#define SIZE_64_BYTES       64
#define SIZE_80_BYTES       80
#define SIZE_128_BYTES      128
#define SIZE_192_BYTES      192

#define SIZE_SSID_LENGTH       33

#define SIZE_64_BITS        8
#define SIZE_128_BITS       16
#define SIZE_160_BITS       20
#define SIZE_256_BITS       32

#define SIZE_ENCR_IV            SIZE_128_BITS
#define ENCR_DATA_BLOCK_SIZE    SIZE_128_BITS
#define SIZE_DATA_HASH          SIZE_160_BITS
#define SIZE_PUB_KEY_HASH       SIZE_160_BITS
#define SIZE_UUID               SIZE_16_BYTES
#define SIZE_MAC_ADDR           SIZE_6_BYTES
#define SIZE_PUB_KEY            SIZE_192_BYTES /* 1536 BITS */
#define SIZE_ENROLLEE_NONCE     SIZE_128_BITS

#define SIZE_AUTHORIZEDMACS_NUM	5

#endif /* _WPS_TYPE_ */
