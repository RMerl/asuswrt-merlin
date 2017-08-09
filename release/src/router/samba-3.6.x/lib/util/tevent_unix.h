/*
   Unix SMB/CIFS implementation.
   Wrap unix errno around tevent_req
   Copyright (C) Volker Lendecke 2009

     ** NOTE! The following LGPL license applies to the tevent_unix
     ** helper library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TEVENT_UNIX_H
#define _TEVENT_UNIX_H

#include <tevent.h>

bool tevent_req_is_unix_error(struct tevent_req *req, int *perrno);

#endif
