/*
 * $Id: w32-eventlog.h 779 2006-02-26 08:46:24Z rpedde $
 *
 * eventlog messages utility functions
 *
 * Copyright (C) 2005 Ron Pedde (ron.pedde@firstmarkcu.org)
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

#ifndef _EVENTLOG_H_
#define _EVENTLOG_H_

extern int elog_register(void);
extern int elog_unregister(void);
extern int elog_init(void);
extern int elog_deinit(void);
extern int elog_message(int level, char *msg);

#endif /* _EVENTLOG_H_ */
