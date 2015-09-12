/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#ifndef PIM_TIME_H
#define PIM_TIME_H

#include <stdint.h>

#include <zebra.h>
#include "thread.h"

int64_t pim_time_monotonic_sec(void);
int64_t pim_time_monotonic_dsec(void);
int pim_time_mmss(char *buf, int buf_size, long sec);
void pim_time_timer_to_mmss(char *buf, int buf_size, struct thread *t);
void pim_time_timer_to_hhmmss(char *buf, int buf_size, struct thread *t);
void pim_time_uptime(char *buf, int buf_size, int64_t uptime_sec);
void pim_time_uptime_begin(char *buf, int buf_size, int64_t now, int64_t begin);
long pim_time_timer_remain_msec(struct thread *t_timer);

#endif /* PIM_TIME_H */
