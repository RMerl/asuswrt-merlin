/*
 * SEGV / backtrace handling test.
 *
 * copied from test-sig.c
 *
 * Copyright (C) 2013 by David Lamparter, Open Source Routing.
 * Copyright (C) 2013 by Internet Systems Consortium, Inc. ("ISC")
 *
 * This file is part of Quagga
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>
#include <sigevent.h>
#include "lib/log.h"
#include "lib/memory.h"

struct quagga_signal_t sigs[] = 
{
};

struct thread_master *master;

int
threadfunc (struct thread *thread)
{
  int *null = NULL;
  *null += 1;
  return 0;
}

int
main (void)
{
  master = thread_master_create ();
  signal_init (master, array_size(sigs), sigs);

  zlog_default = openzlog("testsegv", ZLOG_NONE,
                          LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  zlog_set_level (NULL, ZLOG_DEST_SYSLOG, ZLOG_DISABLED);
  zlog_set_level (NULL, ZLOG_DEST_STDOUT, LOG_DEBUG);
  zlog_set_level (NULL, ZLOG_DEST_MONITOR, ZLOG_DISABLED);

  thread_execute (master, threadfunc, 0, 0);

  exit (0);
}
