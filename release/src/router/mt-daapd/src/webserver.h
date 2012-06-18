/*
 * $Id: webserver.h,v 1.1 2009-06-30 02:31:09 steven Exp $
 * Webserver library
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

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

/*
 * Defines
 */

#define RT_GET       0
#define RT_POST      1

/* 
 * Typedefs
 */


typedef void* WSHANDLE;

typedef struct tag_wsconfig {
    char *web_root;
    char *id;
    unsigned short port;
} WSCONFIG;

typedef struct tag_arglist {
    char *key;
    char *value;
    struct tag_arglist *next;
} ARGLIST;

typedef struct tag_ws_conninfo {
    WSHANDLE pwsp;
    int threadno;
    int error;
    int fd;
    int request_type;
    char *uri;
    char *hostname;
    int close;
    ARGLIST request_headers;
    ARGLIST response_headers;
    ARGLIST request_vars;
} WS_CONNINFO;

/*
 * Externs
 */

#define WS_REQ_HANDLER void (*)(WS_CONNINFO *)
#define WS_AUTH_HANDLER int (*)(char *, char *)

extern WSHANDLE ws_start(WSCONFIG *config);
extern int ws_stop(WSHANDLE ws);
extern int ws_registerhandler(WSHANDLE ws, char *regex, 
			      void(*handler)(WS_CONNINFO*),
			      int(*auth)(char *, char *),
			      int addheaders);

/* for handlers */
extern void ws_close(WS_CONNINFO *pwsc);
extern int ws_returnerror(WS_CONNINFO *pwsc, int error, char *description);
extern int ws_addresponseheader(WS_CONNINFO *pwsc, char *header, char *fmt, ...);
extern int ws_writefd(WS_CONNINFO *pwsc, char *fmt, ...);
extern char *ws_getvar(WS_CONNINFO *pwsc, char *var);
extern char *ws_getrequestheader(WS_CONNINFO *pwsc, char *header);
extern int ws_testrequestheader(WS_CONNINFO *pwsc, char *header, char *value);
extern void ws_emitheaders(WS_CONNINFO *pwsc);

#endif /* _WEBSERVER_H_ */
