/*
   Unix SMB/CIFS implementation.

   Echo structures, server service example

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

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

#ifndef __ECHO_SERVER_H__
#define __ECHO_SERVER_H__

struct task_server;

struct echo_server {
        struct task_server *task;
};

#define ECHO_SERVICE_PORT 7

#endif /*__ECHO_SERVER_H__*/
