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

#ifndef PIM_ZEBRA_H
#define PIM_ZEBRA_H

#include "pim_igmp.h"
#include "pim_ifchannel.h"

void pim_zebra_init(char *zebra_sock_path);

void pim_scan_oil(void);

void igmp_anysource_forward_start(struct igmp_group *group);
void igmp_anysource_forward_stop(struct igmp_group *group);

void igmp_source_forward_start(struct igmp_source *source);
void igmp_source_forward_stop(struct igmp_source *source);

void pim_forward_start(struct pim_ifchannel *ch);
void pim_forward_stop(struct pim_ifchannel *ch);

#endif /* PIM_ZEBRA_H */
