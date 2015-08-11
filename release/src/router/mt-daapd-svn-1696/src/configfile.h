/*
 * $Id: configfile.h 1538 2007-04-15 23:57:00Z rpedde $
 * Functions for reading and writing the config file
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

#ifndef _CONFIGFILE_H_
#define _CONFIGFILE_H_

#include "daapd.h"
#include "webserver.h"

extern int config_auth(WS_CONNINFO *pwsc, char *user, char *password);
extern void config_handler(WS_CONNINFO *pwsc);
extern void config_set_status(WS_CONNINFO *pwsc, int session, char *fmt, ...);
extern int config_get_session_count(void);
extern int config_matches_role(WS_CONNINFO *pwsc, char *username, char *password, char *role);

/** thread local storage */
typedef struct tag_scan_status {
    int session;
    int thread;
    char *what;
    char *host;
} SCAN_STATUS;

#endif /* _CONFIGFILE_H_ */
