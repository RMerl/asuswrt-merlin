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

#ifndef PIM_MACRO_H
#define PIM_MACRO_H

#include <zebra.h>

#include "if.h"

#include "pim_upstream.h"
#include "pim_ifchannel.h"

int pim_macro_ch_lost_assert(const struct pim_ifchannel *ch);
int pim_macro_chisin_joins(const struct pim_ifchannel *ch);
int pim_macro_chisin_pim_include(const struct pim_ifchannel *ch);
int pim_macro_chisin_joins_or_include(const struct pim_ifchannel *ch);
int pim_macro_ch_could_assert_eval(const struct pim_ifchannel *ch);
struct pim_assert_metric pim_macro_spt_assert_metric(const struct pim_rpf *rpf,
						     struct in_addr ifaddr);
struct pim_assert_metric pim_macro_ch_my_assert_metric_eval(const struct pim_ifchannel *ch);
int pim_macro_chisin_oiflist(const struct pim_ifchannel *ch);
int pim_macro_assert_tracking_desired_eval(const struct pim_ifchannel *ch);

#endif /* PIM_MACRO_H */
