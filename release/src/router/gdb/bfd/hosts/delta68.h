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

/* Definitions for a Motorola Delta 3300 box running System V R3.0.
   Contributed by manfred@lts.sel.alcatel.de.  */

#include <sys/param.h>

/* Definitions used by trad-core.c.  */
#define	NBPG			NBPC
#define	HOST_DATA_START_ADDR	u.u_exdata.ux_datorg
#define	HOST_TEXT_START_ADDR	u.u_exdata.ux_txtorg
/* User's stack, copied from sys/param.h  */
#define HOST_STACK_END_ADDR	USRSTACK
#define	UPAGES			USIZE
#define	TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(abfd) \
  abfd->tdata.trad_core_data->u.u_abort
