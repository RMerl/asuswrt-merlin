/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _MON_H_
#define _MON_H_

#include "basics.h"
#include "itable.h"

/* monitor/logger: counts what the simulation is up to */

typedef unsigned long count_type;

/* Interfact to model to return model specific information */
typedef struct _model_print model_print;
struct _model_print {
  model_print *next;
  const char *name;
  const char *suffix_singular;
  const char *suffix_plural;
  count_type count;
};

/* Additional events to monitor */
typedef enum _mon_events {
  mon_event_icache_miss,
  nr_mon_events
} mon_events;

typedef struct _mon mon;
typedef struct _cpu_mon cpu_mon;

INLINE_MON\
(mon *) mon_create
(void);

INLINE_MON\
(cpu_mon *) mon_cpu
(mon *monitor,
 int cpu_nr);

INLINE_MON\
(void) mon_init
(mon *monitor,
 int nr_cpus);

INLINE_MON\
(void) mon_issue
(itable_index index,
 cpu *processor, 
 unsigned_word cia);

/* NOTE - there is no mon_iload - it is made reduntant by mon_issue()
   and besides when the cpu's have their own cache, the information is
   wrong */

INLINE_MON\
(void) mon_read
(unsigned_word ea,
 unsigned_word ra,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_MON\
(void) mon_write
(unsigned_word ea,
 unsigned_word ra,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_MON\
(void) mon_event
(mon_events event,
 cpu *processor,
 unsigned_word cia);

INLINE_MON\
(unsigned) mon_get_number_of_insns
(mon *monitor,
 int cpu_nr);

INLINE_MON\
(void) mon_print_info
(psim *system,
 mon *monitor,
 int verbose);

#endif
