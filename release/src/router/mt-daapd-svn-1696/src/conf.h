/*
 * $Id: conf.h 1077 2006-05-24 04:19:44Z rpedde $
 * Functions for reading and writing the config file
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

#ifndef _CONF_H_
#define _CONF_H_

#define CONF_E_SUCCESS      0
#define CONF_E_FOPEN        1
#define CONF_E_UNKNOWN      2
#define CONF_E_BADHEADER    3
#define CONF_E_PARSE        4
#define CONF_E_OVERFLOW     5  /** <Buffer passed too small */
#define CONF_E_NOCONF       6  /** <No open config file */
#define CONF_E_NOTFOUND     7
#define CONF_E_NOTWRITABLE  8
#define CONF_E_BADELEMENT   9
#define CONF_E_PATHEXPECTED 10
#define CONF_E_INTEXPECTED  11
#define CONF_E_BADCONFIG    12

extern int conf_read(char *file);
extern int conf_reload(void);
extern int conf_close(void);
extern int conf_get_int(char *section, char *key, int dflt);
extern int conf_get_string(char *section, char *key, char *dflt,
                             char *out, int *size);
extern char *conf_alloc_string(char *section, char *key, char *dflt);
extern int conf_set_int(char *section, char *key, int value, int verify);
extern int conf_set_string(char *section, char *key, char *value, int verify);

extern int conf_isset(char *section, char *key);
extern int conf_iswritable(void);
extern int conf_write(void);

extern char *conf_implode(char *section, char *key, char *delimiter);
extern int conf_get_array(char *section, char *key, char ***argvp);
extern void conf_dispose_array(char **argv);

extern char *conf_get_filename(void);

/* FIXME: get enum functions and move to xml-rpc */
#include "webserver.h"
extern int conf_xml_dump(WS_CONNINFO *pwsc);

/* FIXME: well used config reading stuff... */
extern char *conf_get_servername(void);


#endif /* _CONF_H_ */
