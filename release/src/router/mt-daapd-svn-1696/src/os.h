/*
 * $Id: os.h 1479 2007-01-09 18:12:39Z rpedde $
 * Abstract os interface for non-unix platforms
 *
 * Copyright (C) 2006 Ron Pedde (rpedde@users.sourceforge.net)
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

#ifndef _OS_H_
#define _OS_H_

#define S_STOP 0
#define S_SCAN 1
#define S_FULL 2

/* backgrounding, signal handling, etc */
extern int os_init(int foreground, char *runas);
extern void os_deinit(void);
extern void os_wait(int seconds);

/* system native logging functions */
extern int os_opensyslog(void);
extern int os_closesyslog(void);
extern int os_syslog(int level, char *msg);
extern int os_chown(char *path, char *user);

/* library loading functions */
extern void *os_loadlib(char **pe, char *path);
extern void *os_libfunc(char **pe, void *handle, char *function);
extern int os_unload(void *handle);

/* misc */
extern int os_stat(const char *path, struct stat *sb);
extern int os_lstat(const char *path, struct stat *sb);
extern int os_islocaladdr(char *hostaddr);
extern char *os_apppath(char *parm);
extern int os_signal_server(int what); /* signal a running server */

#ifdef WIN32
# include "os-win32.h"
#else
# include "os-unix.h"
#endif

#endif
