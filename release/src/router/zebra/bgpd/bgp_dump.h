/* BGP dump routine.
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* MRT compatible packet dump values.  */
/* type value */
#define MSG_PROTOCOL_BGP4MP  16
/* subtype value */
#define BGP4MP_STATE_CHANGE   0
#define BGP4MP_MESSAGE        1
#define BGP4MP_ENTRY          2
#define BGP4MP_SNAPSHOT       3

#define BGP_DUMP_HEADER_SIZE 12

void bgp_dump_init ();
void bgp_dump_state (struct peer *, int, int);
void bgp_dump_packet (struct peer *, int, struct stream *);
