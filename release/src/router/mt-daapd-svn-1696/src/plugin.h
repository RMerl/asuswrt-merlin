/*
 * $Id: $
 * Simple plug-in api for output, transcode, and scanning plug-ins
 *
 * Copyright (C) 2006 Ron Pedde (ron@pedde.com)
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

#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include "webserver.h"
#include "xml-rpc.h"
#include "db-generic.h"
#include "ff-plugin-events.h"

extern int plugin_init(void);
extern int plugin_load(char **pe, char *path);
extern int plugin_deinit(void);

/* Interfaces for web */
extern int plugin_url_candispatch(WS_CONNINFO *pwsc);
extern void plugin_url_handle(WS_CONNINFO *pwsc);
extern int plugin_auth_handle(WS_CONNINFO *pwsc, char *username, char *pw);
extern int plugin_rend_register(char *name, int port, char *iface, char *txt);
extern void plugin_event_dispatch(int event_id, int intval, void *vp, int len);

extern void *plugin_enum(void *);
extern char *plugin_get_description(void *);

extern int plugin_ssc_should_transcode(WS_CONNINFO *pwsc, char *codec);
extern int plugin_ssc_transcode(WS_CONNINFO *pwsc, MP3FILE *pmp3, int offset, int headers);

/* these should really get rows */

#define PLUGIN_E_SUCCESS     0
#define PLUGIN_E_NOLOAD      1
#define PLUGIN_E_BADFUNCS    2

#endif /* _PLUGIN_H_ */
