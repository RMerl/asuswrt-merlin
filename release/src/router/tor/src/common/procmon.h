/* Copyright (c) 2011-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file procmon.h
 * \brief Headers for procmon.c
 **/

#ifndef TOR_PROCMON_H
#define TOR_PROCMON_H

#include "compat.h"
#include "compat_libevent.h"

#include "torlog.h"

typedef struct tor_process_monitor_t tor_process_monitor_t;

/* DOCDOC tor_procmon_callback_t */
typedef void (*tor_procmon_callback_t)(void *);

int tor_validate_process_specifier(const char *process_spec,
                                   const char **msg);
tor_process_monitor_t *tor_process_monitor_new(struct event_base *base,
                                               const char *process_spec,
                                               log_domain_mask_t log_domain,
                                               tor_procmon_callback_t cb,
                                               void *cb_arg,
                                               const char **msg);
void tor_process_monitor_free(tor_process_monitor_t *procmon);

#endif

