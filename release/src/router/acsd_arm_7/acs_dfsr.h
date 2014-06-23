/*
 *      acs_dfsr.h
 *
 *	Header file for the ACSD DFS Re-entry module.
 *
 *	Copyright (C) 2014, Broadcom Corporation
 *	All Rights Reserved.
 *	
 *	This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *	the contents of this file may not be disclosed to third parties, copied
 *	or duplicated in any form, in whole or in part, without the prior
 *	written permission of Broadcom Corporation.
 *
 *	$Id$
 */
#ifndef __acs_dfsr_h__
#define __acs_dfsr_h__

typedef struct dfsr_context dfsr_context_t;

typedef enum {
	DFS_REENTRY_NONE = 0,
	DFS_REENTRY_DEFERRED,
	DFS_REENTRY_IMMEDIATE
} dfsr_reentry_type_t;

extern dfsr_context_t *acs_dfsr_init(char *prefix, bool enable);
extern void acs_dfsr_exit(dfsr_context_t *);
extern dfsr_reentry_type_t acs_dfsr_chanspec_update(dfsr_context_t *, chanspec_t,
	const char *caller);
extern dfsr_reentry_type_t acs_dfsr_activity_update(dfsr_context_t *, char *if_name);
extern dfsr_reentry_type_t acs_dfsr_reentry_type(dfsr_context_t *);
extern void acs_dfsr_reentry_done(dfsr_context_t *);
extern bool acs_dfsr_enabled(dfsr_context_t *ctx);
extern bool acs_dfsr_enable(dfsr_context_t *ctx, bool enable);
extern int acs_dfsr_dump(dfsr_context_t *ctx, char *buf, unsigned buflen);

#endif /* __acs_dfsr_h__ */
