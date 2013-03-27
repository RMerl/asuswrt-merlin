/* Copyright 2007 Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* Symmetry running either dynix 3.1 (bsd) or ptx (sysv).  */

#define NBPG 4096
#define UPAGES 1

#ifdef _SEQUENT_
/* ptx */
#define HOST_TEXT_START_ADDR 0
#define HOST_STACK_END_ADDR 0x3fffe000
#define TRAD_CORE_USER_OFFSET ((UPAGES * NBPG) - sizeof (struct user))
#else
/* dynix */
#define HOST_TEXT_START_ADDR 0x1000
#define HOST_DATA_START_ADDR (NBPG * u.u_tsize)
#define HOST_STACK_END_ADDR 0x3ffff000
#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_arg[0])
#endif

#define TRAD_CORE_DSIZE_INCLUDES_TSIZE
