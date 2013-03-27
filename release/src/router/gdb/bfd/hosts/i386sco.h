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

/* Core file stuff.  At least some, perhaps all, of the following
   defines work on many more systems than just SCO.  */

#define NBPG NBPC
#define UPAGES USIZE
#define HOST_DATA_START_ADDR u.u_exdata.ux_datorg
#define HOST_STACK_START_ADDR u.u_sub
#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(abfd) \
  ((core_upage(abfd)->u_sysabort != 0) \
   ? core_upage(abfd)->u_sysabort \
   : -1)

/* According to the manpage, a version 2 SCO corefile can contain
   various additional sections (it is cleverly arranged so the u area,
   data, and stack are first where we can find them).  So without
   writing lots of code to parse all their headers and stuff, we can't
   know whether a corefile is bigger than it should be.  */

#define TRAD_CORE_ALLOW_ANY_EXTRA_SIZE 1
