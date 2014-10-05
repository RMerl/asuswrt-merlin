/* Copyright (C) 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _NETINET_IF_TR_H
#define	_NETINET_IF_TR_H 1

#include <sys/cdefs.h>
#include <sys/types.h>
#include <asm/types.h>

#include <linux/if_tr.h>

#ifdef __USE_BSD

struct trn_hdr {
  u_int8_t trn_ac;                /* access control field */
  u_int8_t trn_fc;                /* field control field */
  u_int8_t trn_dhost[TR_ALEN];    /* destination host */
  u_int8_t trn_shost[TR_ALEN];    /* source host */
  u_int16_t trn_rcf;              /* route control field */
  u_int16_t trn_rseg[8];          /* routing registers */
};

#endif

#endif	/* netinet/if_tr.h */
