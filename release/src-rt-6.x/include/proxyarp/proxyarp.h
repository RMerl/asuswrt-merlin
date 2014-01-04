/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: proxyarp.h 246051 2011-03-12 03:30:44Z $
 */
#ifndef _PROXYARP_H_
#define _PROXYARP_H_

#include <osl.h>

struct proxyarp_info {
	osl_t	*osh;		/* OS layer handle */
	void	*lock;
	void	*timer;
	int	enabled;	/* proxyarp service enable or not */
	uint16	count;		/* proxyarp entry number */
};
typedef struct proxyarp_info proxyarp_info_t;

extern uint8 proxyarp_packets_handle(osl_t *osh, void *sdu, void *fproto, bool send, void **reply);
extern void _proxyarp_watchdog(bool all, uint8 *ea);
extern bool proxyarp_get(void);
extern void proxyarp_set(bool enabled);
extern void proxyarp_init(proxyarp_info_t *spa_info);
extern void proxyarp_deinit(void);
extern void proxyarp_lock(proxyarp_info_t *pah);
extern void proxyarp_unlock(proxyarp_info_t *pah);
#define PA_LOCK(pah)	proxyarp_lock(pah)
#define PA_UNLOCK(pah)	proxyarp_unlock(pah)

#ifdef BCMDBG
#define PROXYARP_DEBUG
#endif

#ifdef PROXYARP_DEBUG

#define PROXYARP_ERR_LEVEL		0x1
#define PROXYARP_DBG_LEVEL		0x2
#define PROXYARP_WARN_LEVEL		0x4
#define PROXYARP_INFO_LEVEL		0x8
#define PROXYARP_MFDB_LEVEL		0x10

#define PROXYARP_ERR(fmt, args...)	printf("PARP-ERR:: " fmt, ##args)

#define PROXYARP_DBG(fmt, args...)	if ((proxyarp_msglevel & PROXYARP_DBG_LEVEL)) \
						printf("PARP-DBG:: " fmt, ##args)
#define PROXYARP_WARN(fmt, args...)	if ((proxyarp_msglevel & PROXYARP_WARN_LEVEL)) \
						printf("PARP-WARN:: " fmt, ##args)
#define PROXYARP_INFO(fmt, args...)	if ((proxyarp_msglevel & PROXYARP_INFO_LEVEL)) \
						printf("PARP-INFO:: " fmt, ##args)
#define PROXYARP_MFDB(fmt, args...)	if ((proxyarp_msglevel & PROXYARP_MFDB_LEVEL)) \
						printf("PARP-MFDB:: " fmt, ##args)
#else
#define PROXYARP_ERR(fmt, args...)
#define PROXYARP_DBG(fmt, args...)
#define PROXYARP_WARN(fmt, args...)
#define PROXYARP_INFO(fmt, args...)
#define PROXYARP_MFDB(fmt, args...)

#endif /* PROXYARP_DEBUG */

#endif /* _PROXYARP_H_ */
