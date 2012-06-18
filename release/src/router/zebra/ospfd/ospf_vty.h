/* OSPF VTY interface.
 * Copyright (C) 2000 Toshiaki Takada
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* Macros. */
#define VTY_GET_UINT32(NAME,V,STR)                                            \
{                                                                             \
  char *endptr = NULL;                                                        \
  (V) = strtoul ((STR), &endptr, 10);                                         \
  if (*endptr != '\0' || ((V) == ULONG_MAX && errno == ERANGE))               \
    {                                                                         \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);              \
      return CMD_WARNING;                                                     \
    }                                                                         \
}

#define VTY_GET_IPV4_ADDRESS(NAME,V,STR)                                      \
{                                                                             \
  int retv;                                                                   \
  retv = inet_aton ((STR), &(V));                                             \
  if (!retv)                                                                  \
    {                                                                         \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);              \
      return CMD_WARNING;                                                     \
    }                                                                         \
}

#define VTY_GET_IPV4_PREFIX(NAME,V,STR)                                       \
{                                                                             \
  int retv;                                                                   \
  retv = str2prefix_ipv4 ((STR), &(V));                                       \
  if (retv <= 0)                                                              \
    {                                                                         \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);              \
      return CMD_WARNING;                                                     \
    }                                                                         \
}

#define VTY_GET_OSPF_AREA_ID(V,F,STR)                                         \
{                                                                             \
  int retv;                                                                   \
  retv = ospf_str2area_id ((STR), &(V), &(F));                                \
  if (retv < 0)                                                               \
    {                                                                         \
      vty_out (vty, "%% Invalid OSPF area ID%s", VTY_NEWLINE);                \
      return CMD_WARNING;                                                     \
    }                                                                         \
}

#define VTY_GET_OSPF_AREA_ID_NO_BB(NAME,V,F,STR)                              \
{                                                                             \
  int retv;                                                                   \
  retv = ospf_str2area_id ((STR), &(V), &(F));                                \
  if (retv < 0)                                                               \
    {                                                                         \
      vty_out (vty, "%% Invalid OSPF area ID%s", VTY_NEWLINE);                \
      return CMD_WARNING;                                                     \
    }                                                                         \
  if (OSPF_IS_AREA_ID_BACKBONE ((V)))                                         \
    {                                                                         \
      vty_out (vty, "%% You can't configure %s to backbone%s",                \
               NAME, VTY_NEWLINE);                                            \
    }                                                                         \
}

/* Prototypes. */
void ospf_vty_init ();
void ospf_vty_show_init ();
