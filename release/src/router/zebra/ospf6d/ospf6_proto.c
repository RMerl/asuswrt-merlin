/*
 * Copyright (C) 2003 Yasuhiro Ohara
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "log.h"

#include "ospf6_proto.h"

void
ospf6_prefix_apply_mask (struct ospf6_prefix *op)
{
  u_char *pnt, mask;
  int index, offset;

  pnt = (u_char *)((caddr_t) op + sizeof (struct ospf6_prefix));
  index = op->prefix_length / 8;
  offset = op->prefix_length % 8;
  mask = 0xff << (8 - offset);

  if (index > 16)
    {
      zlog_warn ("Prefix length too long: %d", op->prefix_length);
      return;
    }

  if (index == 16)
    return;

  pnt[index] &= mask;
  index ++;

  while (index < OSPF6_PREFIX_SPACE (op->prefix_length))
    pnt[index++] = 0;
}

void
ospf6_prefix_options_printbuf (u_int8_t prefix_options, char *buf, int size)
{
  snprintf (buf, size, "xxx");
}

void
ospf6_capability_printbuf (char capability, char *buf, int size)
{
  char w, v, e, b;
  w = (capability & OSPF6_ROUTER_BIT_W ? 'W' : '-');
  v = (capability & OSPF6_ROUTER_BIT_V ? 'V' : '-');
  e = (capability & OSPF6_ROUTER_BIT_E ? 'E' : '-');
  b = (capability & OSPF6_ROUTER_BIT_B ? 'B' : '-');
  snprintf (buf, size, "----%c%c%c%c", w, v, e, b);
}

void
ospf6_options_printbuf (u_char *options, char *buf, int size)
{
  char *dc, *r, *n, *mc, *e, *v6;
  dc = (OSPF6_OPT_ISSET (options, OSPF6_OPT_DC) ? "DC" : "--");
  r  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_R)  ? "R"  : "-" );
  n  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_N)  ? "N"  : "-" );
  mc = (OSPF6_OPT_ISSET (options, OSPF6_OPT_MC) ? "MC" : "--");
  e  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_E)  ? "E"  : "-" );
  v6 = (OSPF6_OPT_ISSET (options, OSPF6_OPT_V6) ? "V6" : "--");
  snprintf (buf, size, "%s|%s|%s|%s|%s|%s", dc, r, n, mc, e, v6);
}


