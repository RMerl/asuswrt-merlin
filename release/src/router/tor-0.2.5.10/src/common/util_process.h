/* Copyright (c) 2011-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file util_process.h
 * \brief Headers for util_process.c
 **/

#ifndef TOR_UTIL_PROCESS_H
#define TOR_UTIL_PROCESS_H

#ifndef _WIN32
/** A callback structure waiting for us to get a SIGCHLD informing us that a
 * PID has been closed. Created by set_waitpid_callback. Cancelled or cleaned-
 * up from clear_waitpid_callback().  Do not access outside of the main thread;
 * do not access from inside a signal handler. */
typedef struct waitpid_callback_t waitpid_callback_t;

waitpid_callback_t *set_waitpid_callback(pid_t pid,
                                         void (*fn)(int, void *), void *arg);
void clear_waitpid_callback(waitpid_callback_t *ent);
void notify_pending_waitpid_callbacks(void);
#endif

#endif

