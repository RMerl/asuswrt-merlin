/* Serial interface for raw TCP connections on Un*x like systems.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef SER_TCP_H
#define SER_TCP_H

struct serial;

extern int net_open (struct serial *scb, const char *name);
extern void net_close (struct serial *scb);
extern int net_read_prim (struct serial *scb, size_t count);
extern int net_write_prim (struct serial *scb, const void *buf, size_t count);

#endif
