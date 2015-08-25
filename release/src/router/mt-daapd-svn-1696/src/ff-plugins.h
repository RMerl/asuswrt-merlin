/*
 * $Id: $
 * Public plug-in interface
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

#ifndef _FF_PLUGINS_H_
#define _FF_PLUGINS_H_

#include "ff-dbstruct.h"
#include "ff-plugin-events.h"

#ifdef WIN32
# ifdef _WINDLL
#  define EXPORT __declspec(dllimport)
# else
#  define EXPORT __declspec(dllexport)
# endif
#else
# define EXPORT
#endif


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef E_FATAL
# define E_FATAL 0
# define E_LOG   1
# define E_WARN  2
# define E_INF   5
# define E_DBG   9
# define E_SPAM  10
#endif

#define COUNT_SONGS 0
#define COUNT_PLAYLISTS 1

struct tag_ws_conninfo;
typedef void* HANDLE;

/* Functions that must be exported by different plugin types */
typedef struct tag_plugin_output_fn {
    int(*can_handle)(struct tag_ws_conninfo *pwsc);
    void(*handler)(struct tag_ws_conninfo *pwsc);
    int(*auth)(struct tag_ws_conninfo *pwsc, char *username, char *pw);
} PLUGIN_OUTPUT_FN;

typedef struct tag_plugin_event_fn {
    void(*handler)(int event_id, int intval, void *vp, int len);
} PLUGIN_EVENT_FN;

typedef struct tag_plugin_transcode_fn {
    void *(*ssc_init)(void);
    void (*ssc_deinit)(void*);
    int (*ssc_open)(void*, MP3FILE *);
    int (*ssc_close)(void*);
    int (*ssc_read)(void*, char*, int);
    char *(*ssc_error)(void*);
} PLUGIN_TRANSCODE_FN;

/* info for rendezvous advertising */
typedef struct tag_plugin_rend_info {
    char *type;
    char *txt;
} PLUGIN_REND_INFO;

/* main info struct that plugins must provide */
typedef struct tag_plugin_info {
    int version;                  /* PLUGIN_VERSION */
    int type;                     /* PLUGIN_OUTPUT, etc */
    char *server;                 /* Server/version format */
    PLUGIN_OUTPUT_FN *output_fns; /* functions for different plugin types */
    PLUGIN_EVENT_FN *event_fns;
    PLUGIN_TRANSCODE_FN *transcode_fns;
    PLUGIN_REND_INFO *rend_info;  /* array of rend announcements */
    char *codeclist;              /* comma separated list of codecs */
} PLUGIN_INFO;


#define QUERY_TYPE_ITEMS     0
#define QUERY_TYPE_PLAYLISTS 1
#define QUERY_TYPE_DISTINCT  2

#define FILTER_TYPE_FIREFLY  0
#define FILTER_TYPE_APPLE    1

typedef struct tag_db_query {
    int query_type;
    char *distinct_field;
    int filter_type;
    char *filter;

    int offset;
    int limit;

    int playlist_id;            /* for items query */
    int totalcount;             /* returned total count */
    void *priv;
} DB_QUERY;

#ifdef __cplusplus
extern "C" {
#endif

/* webserver functions */
extern EXPORT char *pi_ws_uri(struct tag_ws_conninfo *);
extern EXPORT void pi_ws_will_close(struct tag_ws_conninfo *);
extern EXPORT int pi_ws_returnerror(struct tag_ws_conninfo *, int, char *);
extern EXPORT char *pi_ws_getvar(struct tag_ws_conninfo *, char *);
extern EXPORT int pi_ws_writefd(struct tag_ws_conninfo *, char *, ...);
extern EXPORT int pi_ws_addresponseheader(struct tag_ws_conninfo *, char *, char *, ...);
extern EXPORT void pi_ws_emitheaders(struct tag_ws_conninfo *);
extern EXPORT char *pi_ws_getrequestheader(struct tag_ws_conninfo *, char *);
extern EXPORT int pi_ws_writebinary(struct tag_ws_conninfo *, char *, int);
extern EXPORT char *pi_ws_gethostname(struct tag_ws_conninfo *);
extern EXPORT int pi_ws_matchesrole(struct tag_ws_conninfo *, char *, char *, char *);

/* misc helpers */
extern EXPORT char *pi_server_ver(void);
extern EXPORT int pi_server_name(char *, int *);
extern EXPORT void pi_log(int, char *, ...);
extern EXPORT int pi_should_transcode(struct tag_ws_conninfo *, char *);

/* db functions */
extern EXPORT int pi_db_enum_start(char **, DB_QUERY *);
extern EXPORT int pi_db_enum_fetch_row(char **, char ***, DB_QUERY *);
extern EXPORT int pi_db_enum_end(char **);
extern EXPORT int pi_db_enum_restart(char **, DB_QUERY *);
extern EXPORT void pi_db_enum_dispose(char **, DB_QUERY*);
extern EXPORT void pi_stream(struct tag_ws_conninfo *, char *);

extern EXPORT int pi_db_add_playlist(char **pe, char *name, int type, char *clause, char *path, int index, int *playlistid);
extern EXPORT int pi_db_add_playlist_item(char **pe, int playlistid, int songid);
extern EXPORT int pi_db_edit_playlist(char **pe, int id, char *name, char *clause);
extern EXPORT int pi_db_delete_playlist(char **pe, int playlistid);
extern EXPORT int pi_db_delete_playlist_item(char **pe, int playlistid, int songid);
extern EXPORT int pi_db_revision(void);
extern EXPORT int pi_db_count_items(int what);
extern EXPORT int pi_db_wait_update(struct tag_ws_conninfo *);

/* config/misc functions */
extern EXPORT char *pi_conf_alloc_string(char *section, char *key, char *dflt);
extern EXPORT void pi_conf_dispose_string(char *str);
extern EXPORT int pi_conf_get_int(char *section, char *key, int dflt);
extern EXPORT void pi_config_set_status(struct tag_ws_conninfo *pwsc, int session, char *fmt, ...);

/* io functions */
extern EXPORT HANDLE pi_io_new(void);
extern EXPORT int pi_io_open(HANDLE io, char *fmt, ...);
extern EXPORT int pi_io_close(HANDLE io);
extern EXPORT int pi_io_read(HANDLE io, unsigned char *buf, uint32_t *len);
extern EXPORT int pi_io_read_timeout(HANDLE io, unsigned char *buf, uint32_t *len, uint32_t *ms);
extern EXPORT int pi_io_write(HANDLE io, unsigned char *buf, uint32_t *len);
extern EXPORT int pi_io_printf(HANDLE io, char *fmt, ...);
extern EXPORT int pi_io_size(HANDLE io, uint64_t *size);
extern EXPORT int pi_io_setpos(HANDLE io, uint64_t offset, int whence);
extern EXPORT int pi_io_getpos(HANDLE io, uint64_t *pos);
extern EXPORT int pi_io_buffer(HANDLE io); /* unimplemented */
extern EXPORT int pi_io_readline(HANDLE io, unsigned char *buf, uint32_t *len);
extern EXPORT int pi_io_readline_timeout(HANDLE io, unsigned char *buf, uint32_t *len, uint32_t *ms);
extern EXPORT char* pi_io_errstr(HANDLE io);
extern EXPORT int pi_io_errcode(HANDLE io);
extern EXPORT void pi_io_dispose(HANDLE io);

#ifdef __cplusplus
}
#endif


#endif /* _FF_PLUGINS_ */
