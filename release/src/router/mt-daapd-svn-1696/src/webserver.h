/*
 * $Id: webserver.h 1622 2007-07-31 04:34:33Z rpedde $
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

#include "io.h"

/*
 * Defines
 */

#define RT_GET       0
#define RT_POST      1

#define E_WS_SUCCESS       0 
#define E_WS_NATIVE        1   /**< A native platform error */
#define E_WS_MEMORY        2   /**< Malloc error */
#define E_WS_PTHREADS      3   /**< Pthreads error */
#define E_WS_EXHAUSTED     4   /**< ports exhausted!? */
#define E_WS_LISTEN        5   /**< can't listen */
#define E_WS_CONTENTLEN    6   /**< Invalid content length in POST */
#define E_WS_READ          7   /**< read error on socket */
#define E_WS_GETVARS       8   /**< could not parse get vars */
#define E_WS_TIMEOUT       9   /**< timeout listening on socket */
#define E_WS_REQTYPE      10   /**< invalid request type (POST, GET, etc) */
#define E_WS_BADPATH      11   /**< requested path out of root */

#define L_WS_SPAM         10   /**< Logorrhea! */
#define L_WS_DBG           9   /**< Way too verbose */
#define L_WS_INF           5   /**< Good info, not too much spam */
#define L_WS_WARN          2   /**< Goes in text log, but not syslog */
#define L_WS_LOG           1   /**< Something that should go in syslog */
#define L_WS_FATAL         0   /**< Log and force an exit */

/*
 * Typedefs
 */
typedef void* WSHANDLE;
typedef void* WSTHREADENUM;

typedef struct tag_wsconfig {
    char *web_root;
    char *id;
    char *ssl_cert;
    char *ssl_key;
    char *ssl_pw;
    unsigned short port;
    unsigned short ssl_port;
} WSCONFIG;

typedef struct tag_arglist {
    char *key;
    char *value;
    struct tag_arglist *next;
} ARGLIST;

typedef struct tag_ws_conninfo {
    int err_code;
    int err_native;
    char *err_msg;
    WSHANDLE pwsp;
    int threadno;
    int error;
    IOHANDLE hclient;
    int request_type;
    char *uri;
    char *hostname;
    int close;
    int secure;
    void *secure_storage;
    void *local_storage;
    void (*storage_callback)(void*);
    ARGLIST request_headers;
    ARGLIST response_headers;
    ARGLIST request_vars;
} WS_CONNINFO;

/*
 * Externs
 */

#define WS_REQ_HANDLER void (*)(WS_CONNINFO *)
#define WS_AUTH_HANDLER int (*)(WS_CONNINFO*, char *, char *)

/** Server functions */
extern WSHANDLE ws_init(WSCONFIG *config);
extern int ws_start(WSHANDLE ws);
extern int ws_stop(WSHANDLE ws);
extern int ws_registerhandler(WSHANDLE ws, char *stem,
    void(*handler)(WS_CONNINFO*),
    int(*auth)(WS_CONNINFO*, char *, char *),
    int flags,
    int addheaders);
extern int ws_server_errcode(WSHANDLE ws);

                          

/** Local storage functions */
extern void ws_lock_local_storage(WS_CONNINFO *pwsc);
extern void ws_unlock_local_storage(WS_CONNINFO *pwsc);
extern void *ws_get_local_storage(WS_CONNINFO *pwsc);
extern void ws_set_local_storage(WS_CONNINFO *pwsc, void *ptr, void (*callback)(void *));

extern WS_CONNINFO *ws_thread_enum_first(WSHANDLE, WSTHREADENUM *);
extern WS_CONNINFO *ws_thread_enum_next(WSHANDLE, WSTHREADENUM *);

/* for handlers */
extern void ws_close(WS_CONNINFO *pwsc);
extern int ws_returnerror(WS_CONNINFO *pwsc, int error, char *description);
extern int ws_addresponseheader(WS_CONNINFO *pwsc, char *header, char *fmt, ...);
extern int ws_writefd(WS_CONNINFO *pwsc, char *fmt, ...);
extern int ws_writebinary(WS_CONNINFO *pwsc, char *data, int len);
extern char *ws_getvar(WS_CONNINFO *pwsc, char *var);
extern char *ws_getrequestheader(WS_CONNINFO *pwsc, char *header);
extern int ws_testrequestheader(WS_CONNINFO *pwsc, char *header, char *value);
extern void ws_emitheaders(WS_CONNINFO *pwsc);
extern char *ws_uri(WS_CONNINFO *pwsc);
extern void *ws_enum_var(WS_CONNINFO *pwsc, char **key, char **value, void *last);
extern int ws_copyfile(WS_CONNINFO *pwsc, IOHANDLE hfile, uint64_t *bytes_copied);
extern void ws_should_close(WS_CONNINFO *pwsc, int should_close);
extern int ws_threadno(WS_CONNINFO *pwsc);
extern char *ws_hostname(WS_CONNINFO *pwsc);

extern void ws_set_errhandler(void(*err_handler)(int, char*));
extern int ws_set_err(WS_CONNINFO *pwsc, int ws_error);
extern int ws_get_err(WS_CONNINFO *pwsc, int *errcode, char **err_msg);

#endif /* _WEBSERVER_H_ */
