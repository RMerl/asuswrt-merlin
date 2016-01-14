/*
 * Zebra debug related function
 * Copyright (C) 1999 Kunihiro Ishiguro
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

#ifndef _ZEBRA_DEBUG_H
#define _ZEBRA_DEBUG_H

/* Debug flags. */
#define ZEBRA_DEBUG_EVENT   0x01

#define ZEBRA_DEBUG_PACKET  0x01
#define ZEBRA_DEBUG_SEND    0x20
#define ZEBRA_DEBUG_RECV    0x40
#define ZEBRA_DEBUG_DETAIL  0x80

#define ZEBRA_DEBUG_KERNEL  0x01

#define ZEBRA_DEBUG_RIB     0x01
#define ZEBRA_DEBUG_RIB_Q   0x02

#define ZEBRA_DEBUG_FPM     0x01

/* Debug related macro. */
#define IS_ZEBRA_DEBUG_EVENT  (zebra_debug_event & ZEBRA_DEBUG_EVENT)

#define IS_ZEBRA_DEBUG_PACKET (zebra_debug_packet & ZEBRA_DEBUG_PACKET)
#define IS_ZEBRA_DEBUG_SEND   (zebra_debug_packet & ZEBRA_DEBUG_SEND)
#define IS_ZEBRA_DEBUG_RECV   (zebra_debug_packet & ZEBRA_DEBUG_RECV)
#define IS_ZEBRA_DEBUG_DETAIL (zebra_debug_packet & ZEBRA_DEBUG_DETAIL)

#define IS_ZEBRA_DEBUG_KERNEL (zebra_debug_kernel & ZEBRA_DEBUG_KERNEL)

#define IS_ZEBRA_DEBUG_RIB  (zebra_debug_rib & ZEBRA_DEBUG_RIB)
#define IS_ZEBRA_DEBUG_RIB_Q  (zebra_debug_rib & ZEBRA_DEBUG_RIB_Q)

#define IS_ZEBRA_DEBUG_FPM (zebra_debug_fpm & ZEBRA_DEBUG_FPM)

extern unsigned long zebra_debug_event;
extern unsigned long zebra_debug_packet;
extern unsigned long zebra_debug_kernel;
extern unsigned long zebra_debug_rib;
extern unsigned long zebra_debug_fpm;

extern void zebra_debug_init (void);

#endif /* _ZEBRA_DEBUG_H */
