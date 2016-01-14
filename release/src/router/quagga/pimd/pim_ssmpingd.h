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

#ifndef PIM_SSMPINGD_H
#define PIM_SSMPINGD_H

#include <zebra.h>

#include "if.h"

#include "pim_iface.h"

struct ssmpingd_sock {
  int            sock_fd;     /* socket */
  struct thread *t_sock_read; /* thread for reading socket */
  struct in_addr source_addr; /* source address */
  int64_t        creation;    /* timestamp of socket creation */
  int64_t        requests;    /* counter */
};

void pim_ssmpingd_init(void);
void pim_ssmpingd_destroy(void);
int pim_ssmpingd_start(struct in_addr source_addr);
int pim_ssmpingd_stop(struct in_addr source_addr);

#endif /* PIM_SSMPINGD_H */
