/* Copyright (c) 2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_BACKTRACE_H
#define TOR_BACKTRACE_H

#include "orconfig.h"

void log_backtrace(int severity, int domain, const char *msg);
int configure_backtrace_handler(const char *tor_version);
void clean_up_backtrace_handler(void);

#ifdef EXPOSE_CLEAN_BACKTRACE
#if defined(HAVE_EXECINFO_H) && defined(HAVE_BACKTRACE) && \
  defined(HAVE_BACKTRACE_SYMBOLS_FD) && defined(HAVE_SIGACTION)
void clean_backtrace(void **stack, int depth, const ucontext_t *ctx);
#endif
#endif

#endif

