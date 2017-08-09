/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  this is the change notify database. It implements mechanisms for
  storing current change notify waiters in a tdb, and checking if a
  given event matches any of the stored notify waiiters.
*/

#include "includes.h"
#include "ntvfs/sysdep/sys_notify.h"

NTSTATUS ntvfs_common_init(void)
{
	return sys_notify_init();
}
