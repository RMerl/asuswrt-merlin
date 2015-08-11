/*
 * $Id: rend.h 801 2006-03-05 08:09:27Z rpedde $
 * Rendezvous stuff
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _REND_H_
#define _REND_H_

extern int rend_init(char *user);
extern int rend_running(void);
extern int rend_stop(void);
extern int rend_register(char *name, char *type, int port, char *iface, char *txt);
extern int rend_unregister(char *name, char *type, int port);

#endif /* _REND_H_ */
