/*
 * $Id: db-sql.c 1695 2007-10-24 05:44:35Z rpedde $
 * sql-specific db implementation
 *
 * Copyright (C) 2005 Ron Pedde (ron@pedde.com)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 500

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "daapd.h"
#include "err.h"
#include "mp3-scanner.h"
#include "db-generic.h"
#include "db-sql.h"
#include "restart.h"
#include "smart-parser.h"
#include "plugin.h"
#include "conf.h"  /* FIXME */

#ifdef HAVE_LIBSQLITE
#include "db-sql-sqlite2.h"
#endif
#ifdef HAVE_LIBSQLITE3
#include "db-sql-sqlite3.h"
#endif

/* Globals */
static int db_sql_reload=0;
static int db_sql_in_playlist_scan=0;
static int db_sql_in_scan=0;
static int db_sql_need_dispose=0;

extern char *db_sqlite_updates[];

/* Forwards */


void db_sql_build_mp3file(char **valarray, MP3FILE *pmp3);
int db_sql_update(char **pe, MP3FILE *pmp3, int *id);
int db_sql_update_playlists(char **pe);
int db_sql_parse_smart(char **pe, char **clause, char *phrase);
char *_db_proper_path(char *path);

#define STR(a) (a) ? (a) : ""
#define ISSTR(a) ((a) && strlen((a)))
#define MAYBEFREE(a) { if((a)) free((a)); };
/*
#define DMAPLEN(a) (((a) && strlen(a)) ? (8+(int)strlen((a))) : \
                                         ((pinfo->zero_length) ? 8 : 0))
#define EMIT(a) (pinfo->zero_length ? 1 : ((a) && strlen((a))) ? 1 : 0)
*/
/**
 * functions for the specific db backend
 */
int (*db_sql_open_fn)(char **pe, char *dsn) = NULL;
int (*db_sql_close_fn)(void) = NULL;
int (*db_sql_exec_fn)(char **pe, int loglevel, char *fmt, ...) = NULL;
char* (*db_sql_vmquery_fn)(char *fmt,va_list ap) = NULL;
void (*db_sql_vmfree_fn)(char *query) = NULL;
int (*db_sql_enum_begin_fn)(char **pe, char *fmt, ...) = NULL;
int (*db_sql_enum_fetch_fn)(char **pe, SQL_ROW *pr) = NULL;
int (*db_sql_enum_end_fn)(char **pe) = NULL;
int (*db_sql_enum_restart_fn)(char **pe);
int (*db_sql_event_fn)(int event_type);
int (*db_sql_insert_id_fn)(void);

#ifdef HAVE_LIBSQLITE
int db_sql_open_sqlite2(char **pe, char *parameters) {
    /* first, set our external links to sqlite2 */
    db_sql_open_fn = db_sqlite2_open;
    db_sql_close_fn = db_sqlite2_close;
    db_sql_exec_fn = db_sqlite2_exec;
    db_sql_vmquery_fn = db_sqlite2_vmquery;
    db_sql_vmfree_fn = db_sqlite2_vmfree;
    db_sql_enum_begin_fn = db_sqlite2_enum_begin;
    db_sql_enum_fetch_fn = db_sqlite2_enum_fetch;
    db_sql_enum_end_fn = db_sqlite2_enum_end;
    db_sql_enum_restart_fn = db_sqlite2_enum_restart;
    db_sql_event_fn = db_sqlite2_event;
    db_sql_insert_id_fn = db_sqlite2_insert_id;

    return db_sql_open(pe,parameters);
}
#endif

#ifdef HAVE_LIBSQLITE3
int db_sql_open_sqlite3(char **pe, char *parameters) {
    /* first, set our external links to sqlite2 */
    db_sql_open_fn = db_sqlite3_open;
    db_sql_close_fn = db_sqlite3_close;
    db_sql_exec_fn = db_sqlite3_exec;
    db_sql_vmquery_fn = db_sqlite3_vmquery;
    db_sql_vmfree_fn = db_sqlite3_vmfree;
    db_sql_enum_begin_fn = db_sqlite3_enum_begin;
    db_sql_enum_fetch_fn = db_sqlite3_enum_fetch;
    db_sql_enum_end_fn = db_sqlite3_enum_end;
    db_sql_enum_restart_fn = db_sqlite3_enum_restart;
    db_sql_event_fn = db_sqlite3_event;
    db_sql_insert_id_fn = db_sqlite3_insert_id;

    return db_sql_open(pe,parameters);
}
#endif

char *_db_proper_path(char *path) {
    char *new_path;
    char *path_ptr;

    new_path = strdup(path);
    if(!new_path) {
        DPRINTF(E_FATAL,L_DB,"malloc: _db_proper_path\n");
    }

    if(conf_get_int("scanning","case_sensitive",1) == 0) {
        path_ptr = new_path;
        while(*path_ptr) {
            *path_ptr = toupper(*path_ptr);
            path_ptr++;
        }
    }

    return new_path;
}


int db_sql_atoi(const char *what) {
    return what ? atoi(what) : 0;
}
char *db_sql_strdup(const char *what) {
    return what ? (strlen(what) ? strdup(what) : NULL) : NULL;
}

uint64_t db_sql_atol(const char *what) {
    return what ? atoll(what) : 0;
}
/**
 * escape a sql string, returning it the supplied buffer.
 * note that this uses the sqlite escape format -- use %q for quoted
 * sql.
 *
 * @param buffer buffer to throw the escaped sql into
 * @param size size of buffer (or size required, if DB_E_SIZE)
 * @param fmt printf style format
 * @returns DB_E_SUCCESS with buffer filled, or DB_E_SIZE, with size
 * set to the required size
 */
int db_sql_escape(char *buffer, int *size, char *fmt, ...) {
    va_list ap;
    char *escaped;

    va_start(ap,fmt);
    escaped = db_sql_vmquery_fn(fmt,ap);
    va_end(ap);

    if(*size < (int)strlen(escaped)) {
        *size = (int)strlen(escaped) + 1;
        db_sql_vmfree_fn(escaped);
        return DB_E_SIZE;
    }

    strcpy(buffer,escaped);
    *size = (int)strlen(escaped);
    db_sql_vmfree_fn(escaped);

    return DB_E_SUCCESS;
}


/**
 * fetch a single row, using the underlying database enum
 * functions
 */
int db_sql_fetch_row(char **pe, SQL_ROW *row, char *fmt, ...) {
    int err;
    char *query;
    va_list ap;


    *row=NULL;

    va_start(ap,fmt);
    query=db_sql_vmquery_fn(fmt,ap);
    va_end(ap);

    err=db_sql_enum_begin_fn(pe,"%s",query);
    db_sql_vmfree_fn(query);

    if(err != DB_E_SUCCESS) {
        DPRINTF(E_LOG,L_DB,"Error: enum_begin failed (error %d): %s\n",
                err,(pe) ? *pe : "?");
        return err;
    }

    err=db_sql_enum_fetch_fn(pe, row);
    if(err != DB_E_SUCCESS) {
        DPRINTF(E_LOG,L_DB,"Error: enum_fetch failed (error %d): %s\n",
                err,(pe) ? *pe : "?");
        db_sql_enum_end_fn(NULL);
        return err;
    }

    if(!(*row)) {
        db_sql_need_dispose=0;
        db_sql_enum_end_fn(NULL);
        db_get_error(pe,DB_E_NOROWS);
        return DB_E_NOROWS;
    }

    db_sql_need_dispose = 1;
    return DB_E_SUCCESS;
}

int db_sql_fetch_int(char **pe, int *result, char *fmt, ...) {
    int err;
    char *query;
    va_list ap;
    SQL_ROW row;

    va_start(ap,fmt);
    query=db_sql_vmquery_fn(fmt,ap);
    va_end(ap);

    err = db_sql_fetch_row(pe, &row, "%s", query);
    db_sql_vmfree_fn(query);

    if(err != DB_E_SUCCESS) {
        DPRINTF(E_SPAM,L_DB,"fetch_row failed in fetch_int: %s\n",pe ? *pe : NULL);
        return err;
    }

    *result = atoi(row[0]);
    db_sql_dispose_row();
    return DB_E_SUCCESS;
}

int db_sql_fetch_char(char **pe, char **result, char *fmt, ...) {
    int err;
    char *query;
    va_list ap;
    SQL_ROW row;

    va_start(ap,fmt);
    query=db_sql_vmquery_fn(fmt,ap);
    va_end(ap);

    err = db_sql_fetch_row(pe, &row, "%s", query);
    if(err != DB_E_SUCCESS)
        return err;

    *result = strdup(row[0]);
    db_sql_dispose_row();
    return DB_E_SUCCESS;
}

int db_sql_dispose_row(void) {
    int err = DB_E_SUCCESS;

    /* don't really need the row */
    if(db_sql_need_dispose) {
        db_sql_need_dispose=0;
        err=db_sql_enum_end_fn(NULL);
    }

    return err;
}

/**
 * get the sql where clause for a smart playlist spec.  This
 * where clause must be freed by the caller
 *
 * @param phrase playlist spec to be converted
 * @returns sql where clause if successful, NULL otherwise
 */
int db_sql_parse_smart(char **pe, char **clause, char *phrase) {
    PARSETREE pt;

    if(strcmp(phrase,"1") == 0) {
        *clause = strdup("1");
        return TRUE;
    }

    pt=sp_init();
    if(!pt) {
        if(pe) *pe = strdup("Could not initialize parse tree");
        return FALSE;
    }

    if(!sp_parse(pt,phrase,SP_TYPE_PLAYLIST)) {
        if(pe) *pe = strdup(sp_get_error(pt));

        DPRINTF(E_LOG,L_DB,"Error parsing playlist: %s\n",sp_get_error(pt));

        sp_dispose(pt);
        return FALSE;
    } else {
        *clause = sp_sql_clause(pt);
    }

    sp_dispose(pt);
    return TRUE;
}

/**
 * open sqlite database
 *
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_open(char **pe, char *parameters) {
    int result;
    int current_version;
    int max_version;
    char **db_updates;

    result = db_sql_open_fn(pe,parameters);

    if(result == DB_E_WRONGVERSION) {
        /* need to update the version */
        if(pe) free(*pe);

        db_updates = db_sqlite_updates;

        result = db_sql_fetch_int(pe,&current_version,"select value from "
                                  "config where term='version'");

        if(result != DB_E_SUCCESS)
            return result;

        max_version = 0;
        while(db_updates[max_version])
            max_version++;

        DPRINTF(E_DBG,L_DB,"Current db version: %d\n",current_version);
        DPRINTF(E_DBG,L_DB,"Target db version:  %d\n",max_version);

        while(current_version < max_version) {
            DPRINTF(E_LOG,L_DB,"Upgrading db: %d --> %d\n",current_version,
                    current_version + 1);
            result = db_sql_exec_fn(pe,E_LOG,"%s",db_updates[current_version]);
            if(result != DB_E_SUCCESS) {
                DPRINTF(E_LOG,L_DB,"Error upgrading db: %s\n", pe ? *pe :
                        "Unknown");
                return result;
            }
            current_version++;
        }
    }

    return result;
}

/**
 * initialize the sqlite database, reloading if requested
 *
 * @param reload whether or not to do a full reload on the db
 *        on return, reload is set to 1 if the db MUST be rescanned
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_init(int reload) {
    int items;
    int rescan = 0;
    int err;
    int do_reload = reload;

    err=db_sql_get_count(NULL,&items, countSongs);
    if(err != DB_E_SUCCESS)
        items = 0;

    /* check if a request has been written into the db (by a db upgrade?) */
    if(db_sql_fetch_int(NULL,&rescan,"select value from config where "
                        "term='rescan'") == DB_E_SUCCESS) {
        if(rescan)
            do_reload=1;
    }


    /* we could pass back a status to describe whether a reaload was
     * required (for reasons other than expicit request)
     */
    if(do_reload || (!items)) {
        DPRINTF(E_LOG,L_DB,"Full reload...\n");
        db_sql_event_fn(DB_SQL_EVENT_FULLRELOAD);
        db_sql_reload=1;
    } else {
        db_sql_event_fn(DB_SQL_EVENT_STARTUP);
        db_sql_reload=0;
    }

    return DB_E_SUCCESS;
}

/**
 * close the database
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_deinit(void) {
    return db_sql_close_fn();
}


/**
 * force a rescan
 */
int db_sql_force_rescan(char **pe) {
    int result;
    result = db_sql_exec_fn(pe,E_LOG,"update playlists set db_timestamp=0");
    if(result != DB_E_SUCCESS)
        return result;

    return db_sql_exec_fn(pe,E_LOG,"update songs set force_update=1");

}

/**
 * start a background scan
 *
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_start_scan() {
    DPRINTF(E_DBG,L_DB,"Starting db scan\n");
    db_sql_event_fn(DB_SQL_EVENT_SONGSCANSTART);

    db_sql_in_scan=1;
    db_sql_in_playlist_scan=0;
    return DB_E_SUCCESS;
}

/**
 * end song scan -- start playlist scan
 *
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_end_song_scan(void) {
    DPRINTF(E_DBG,L_DB,"Ending song scan\n");

    db_sql_event_fn(DB_SQL_EVENT_SONGSCANEND);
    db_sql_event_fn(DB_SQL_EVENT_PLSCANSTART);

    db_sql_in_scan=0;
    db_sql_in_playlist_scan=1;

    return DB_E_SUCCESS;
}

/**
 * stop a db scan
 *
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_end_scan(void) {
    db_sql_event_fn(DB_SQL_EVENT_PLSCANEND);

    db_sql_update_playlists(NULL);
    db_sql_reload=0;
    db_sql_in_playlist_scan=0;

    return DB_E_SUCCESS;
}

/**
 * delete a playlist
 *
 * @param playlistid playlist to delete
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_delete_playlist(char **pe, int playlistid) {
    int type;
    int result;

    result=db_sql_fetch_int(pe,&type,"select type from playlists where id=%d",
                            playlistid);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override the generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            return DB_E_INVALID_PLAYLIST;
        }

        return result;
    }

    /* got a good playlist, now do what we need to do */
    db_sql_exec_fn(pe,E_FATAL,"delete from playlists where id=%d",playlistid);
    db_sql_exec_fn(pe,E_FATAL,"delete from playlistitems where playlistid=%d",playlistid);

    return DB_E_SUCCESS;
}

/**
 * delete a song from a playlist
 *
 * @param playlistid playlist to delete item from
 * @param songid song to delete from playlist
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_delete_playlist_item(char **pe, int playlistid, int songid) {
    int result;
    int playlist_type;
    int count;

    /* first, check the playlist */
    result=db_sql_fetch_int(pe,&playlist_type,
                            "select type from playlists where id=%d",
                            playlistid);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            return DB_E_INVALID_PLAYLIST;
        }
        return result;
    }

    if(playlist_type == PL_SMART) { /* can't delete from a smart playlist */
        db_get_error(pe,DB_E_INVALIDTYPE);
        return DB_E_INVALIDTYPE;
    }

    /* make sure the songid is valid */
    result=db_sql_fetch_int(pe,&count,"select count(*) from playlistitems "
                            "where playlistid=%d and songid=%d",
                            playlistid,songid);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_SONGID);
            return DB_E_INVALID_SONGID;
        }
        return result;
    }

    /* looks valid, so lets add the item */
    result=db_sql_exec_fn(pe,E_DBG,"delete from playlistitems where "
                           "playlistid=%d and songid=%d",playlistid,songid);

    return result;
}

/**
 * edit a playlist.  The only things worth changing are the name
 * and the "where" clause.
 *
 * @param id id of the playlist to alter
 * @param name new name of the playlist (or NULL)
 * @param where new where clause (or NULL)
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_edit_playlist(char **pe, int id, char *name, char *clause) {
    int result;
    int playlist_type;
    int dup_id=id;
    char *criteria;
    char *estring;

    if((name == NULL) && (clause == NULL))
        return DB_E_SUCCESS;  /* I guess?? */

    if(id == 1) { /* can't edit the library query */
        db_get_error(pe,DB_E_INVALID_PLAYLIST);
        return DB_E_INVALID_PLAYLIST;
    }

    /* first, check the playlist */
    result=db_sql_fetch_int(pe,&playlist_type,
                            "select type from playlists where id=%d",id);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            return DB_E_INVALID_PLAYLIST;
        }
        return result;
    }


    if((playlist_type == PL_SMART) && (clause)) {
        if(!db_sql_parse_smart(&estring,&criteria,clause)) {
            db_get_error(pe,DB_E_PARSE,estring);
            free(estring);
            return DB_E_PARSE;
        }
        free(criteria);
    }

    /* TODO: check for duplicate names here */
    if(name) {
        result = db_sql_fetch_int(pe,&dup_id,"select id from playlists "
                                  "where upper(title)=upper('%q')",name);

        if((result != DB_E_SUCCESS) && (result != DB_E_NOROWS))
            return result;

        if(result == DB_E_NOROWS)
            if(pe) free(*pe);

        if(dup_id != id) {
            db_get_error(pe,DB_E_DUPLICATE_PLAYLIST,name);
            return DB_E_DUPLICATE_PLAYLIST;
        }

        if((playlist_type == PL_SMART)&&(clause)) {
            result=db_sql_exec_fn(pe,E_LOG,"update playlists set title='%q', "
                                  "query='%q' where id=%d",name,clause,id);

        } else {
            result=db_sql_exec_fn(pe,E_LOG,"update playlists set title='%q' "
                                  "where id=%d",name,id);
        }
        db_sql_update_playlists(NULL);
        return result;
    }

    if((playlist_type == PL_SMART) && (clause)) {
        result= db_sql_exec_fn(pe,E_LOG,"update playlists set query='%q' "
                               "where id=%d",clause,id);
        db_sql_update_playlists(NULL);
        return result;
    }

    return DB_E_SUCCESS;  /* ?? */
}

/**
 * add a playlist
 *
 * @param name name of the playlist
 * @param type playlist type: 0 - static, 1 - smart, 2 - m3u
 * @param clause: "where" clause for smart playlist
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_add_playlist(char **pe, char *name, int type, char *clause, char *path, int index, int *playlistid) {
    int cnt=0;
    int result=DB_E_SUCCESS;
    char *criteria;
    char *estring;
    char *correct_path;
    SQL_ROW row;

    result=db_sql_fetch_int(pe,&cnt,"select count(*) from playlists where "
                            "upper(title)=upper('%q')",name);

    if(result != DB_E_SUCCESS) {
        return result;
    }

    if(cnt != 0) { /* duplicate */
        db_sql_fetch_row(NULL,&row, "select * from playlists where "
                         "upper(title)=upper('%q')",name);
        DPRINTF(E_LOG,L_MISC,"Attempt to add duplicate playlist: '%s' "
                "type: %d, path: %s, idx: %d\n",name,atoi(row[PL_TYPE]),
                row[PL_PATH],atoi(row[PL_IDX]));
        db_sql_dispose_row();
        db_get_error(pe,DB_E_DUPLICATE_PLAYLIST,name);
        return DB_E_DUPLICATE_PLAYLIST;
    }

    if((type == PL_SMART) && (!clause)) {
        db_get_error(pe,DB_E_NOCLAUSE);
        return DB_E_NOCLAUSE;
    }

    /* Let's throw it in  */
    switch(type) {
    case PL_STATICFILE: /* static, from file */
    case PL_STATICXML: /* from iTunes XML file */
        correct_path = _db_proper_path(path);
        result = db_sql_exec_fn(pe,E_LOG,"insert into playlists "
                                "(title,type,items,query,db_timestamp,path,idx) "
                                 "values ('%q',%d,0,NULL,%d,'%q',%d)",
                                 name,type,(int)time(NULL),correct_path,index);
        free(correct_path);
        break;
    case PL_STATICWEB: /* static, maintained in web interface */
        result = db_sql_exec_fn(pe,E_LOG,"insert into playlists "
                                "(title,type,items,query,db_timestamp,path,idx) "
                                 "values ('%q',%d,0,NULL,%d,NULL,%d)",
                                 name,type,time(NULL),index);
        break;
    case PL_SMART: /* smart */
        if(!db_sql_parse_smart(&estring,&criteria,clause)) {
            db_get_error(pe,DB_E_PARSE,estring);
            free(estring);
            return DB_E_PARSE;
        }
        free(criteria);

        result = db_sql_exec_fn(pe,E_LOG,"insert into playlists "
                                 "(title,type,items,query,db_timestamp,idx) "
                                 "values ('%q',%d,%d,'%q',%d,0)",
                                 name,PL_SMART,cnt,clause,time(NULL));
        break;
    }

    if(result)
        return result;

    result = db_sql_fetch_int(pe,playlistid,
                              "select id from playlists where title='%q'",
                              name);

    if(((type==PL_STATICFILE)||(type==PL_STATICXML))
        && (db_sql_in_playlist_scan) && (!db_sql_reload)) {
        db_sql_exec_fn(NULL,E_FATAL,"insert into plupdated values (%d)",*playlistid);
    }

    db_sql_update_playlists(NULL);
    return result;
}

/**
 * add a song to a static playlist
 *
 * @param playlistid playlist to add song to
 * @param songid song to add
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_add_playlist_item(char **pe, int playlistid, int songid) {
    int result;
    int playlist_type;
    int count;

    /* first, check the playlist */
    result=db_sql_fetch_int(pe,&playlist_type,
                            "select type from playlists where id=%d",
                            playlistid);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            return DB_E_INVALID_PLAYLIST;
        }
        return result;
    }

    if(playlist_type == PL_SMART) { /* can't add to smart playlists */
        db_get_error(pe,DB_E_INVALIDTYPE);
        return DB_E_INVALIDTYPE;
    }

    /* make sure the songid is valid */
    result=db_sql_fetch_int(pe,&count,"select count(*) from songs where "
                            "id=%d",songid);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_SONGID);
            return DB_E_INVALID_SONGID;
        }
        return result;
    }

    /* looks valid, so lets add the item */
    result=db_sql_exec_fn(pe,E_DBG,"insert into playlistitems "
                           "(playlistid, songid) values (%d,%d)",
                           playlistid,songid);
    return result;
}


/**
 * add a database item
 *
 * @param pmp3 mp3 file to add
 */
int db_sql_add(char **pe, MP3FILE *pmp3, int *id) {
    int err;
    int count;
    int insertid;
    char *query;
    char *path;
    char sample_count[40];
    char file_size[40];

    DPRINTF(E_SPAM,L_DB,"Entering db_sql_add\n");

    if(!pmp3->time_added)
        pmp3->time_added = (int)time(NULL);

    if(!pmp3->time_modified)
        pmp3->time_modified = (int)time(NULL);

    pmp3->db_timestamp = (int)time(NULL);

    path = _db_proper_path(pmp3->path);

    /* Always an add if in song scan on full reload */
    if((!db_sql_reload)||(!db_sql_in_scan)) {
        query = "select count(*) from songs where path='%q' and idx=%d";
        err=db_sql_fetch_int(NULL,&count,query,path,pmp3->index);

        if((err == DB_E_SUCCESS) && (count == 1)) { /* we should update */
            free(path);
            return db_sql_update(pe,pmp3,id);
        } else if((err != DB_E_SUCCESS) && (err != DB_E_NOROWS)) {
            free(path);
            DPRINTF(E_LOG,L_DB,"Error: %s\n",pe);
            return err;
        }

    }

    pmp3->play_count=0;
    pmp3->time_played=0;

    /* sqlite2 doesn't support 64 bit ints */
    sprintf(sample_count,"%lld",pmp3->sample_count);
    sprintf(file_size,"%lld",pmp3->file_size);

    err=db_sql_exec_fn(pe,E_DBG,"INSERT INTO songs VALUES "
                       "(NULL," // id
                       "'%q',"  // path
                       "'%q',"  // fname
                       "'%q',"  // title
                       "'%q',"  // artist
                       "'%q',"  // album
                       "'%q',"  // genre
                       "'%q',"  // comment
                       "'%q',"  // type
                       "'%q',"  // composer
                       "'%q',"  // orchestra
                       "'%q',"  // conductor
                       "'%q',"  // grouping
                       "'%q',"  // url
                       "%d,"    // bitrate
                       "%d,"    // samplerate
                       "%d,"    // song_length
                       "%s,"    // file_size
                       "%d,"    // year
                       "%d,"    // track
                       "%d,"    // total_tracks
                       "%d,"    // disc
                       "%d,"    // total_discs
                       "%d,"    // bpm
                       "%d,"    // compilation
                       "%d,"    // rating
                       "0,"     // play_count
                       "%d,"    // data_kind
                       "%d,"    // item_kind
                       "'%q',"  // description
                       "%d,"    // time_added
                       "%d,"    // time_modified
                       "%d,"    // time_played
                       "%d,"    // db_timestamp
                       "%d,"    // disabled
                       "%s,"    // sample_count
                       "0,"     // force_update
                       "'%q',"  // codectype
                       "%d,"    // index
                       "%d,"    // has_video
                       "%d,"    // contentrating
                       "%d,"    // bits_per_sample
                       "'%q')",   // albumartist
                       path,
                       STR(pmp3->fname),
                       STR(pmp3->title),
                       STR(pmp3->artist),
                       STR(pmp3->album),
                       STR(pmp3->genre),
                       STR(pmp3->comment),
                       STR(pmp3->type),
                       STR(pmp3->composer),
                       STR(pmp3->orchestra),
                       STR(pmp3->conductor),
                       STR(pmp3->grouping),
                       STR(pmp3->url),
                       pmp3->bitrate,
                       pmp3->samplerate,
                       pmp3->song_length,
                       file_size,
                       pmp3->year,
                       pmp3->track,
                       pmp3->total_tracks,
                       pmp3->disc,
                       pmp3->total_discs,
                       pmp3->bpm,
                       pmp3->compilation,
                       pmp3->rating,
                       pmp3->data_kind,
                       pmp3->item_kind,
                       STR(pmp3->description),
                       pmp3->time_added,
                       pmp3->time_modified,
                       pmp3->time_played,
                       pmp3->db_timestamp,
                       pmp3->disabled,
                       sample_count,
                       STR(pmp3->codectype),
                       pmp3->index,
                       pmp3->has_video,
                       pmp3->contentrating,
                       pmp3->bits_per_sample,
                       STR(pmp3->album_artist));

    free(path);
    if(err != DB_E_SUCCESS)
        DPRINTF(E_FATAL,L_DB,"Error inserting file %s in database\n",pmp3->fname);

    insertid = db_sql_insert_id_fn();
    if((db_sql_in_scan || db_sql_in_playlist_scan)&&(!db_sql_reload)) {
        /* fetch threads have different sqlite handle -- won't see the update table */
        db_sql_exec_fn(NULL,E_DBG,"insert into updated values (%d)",
                       insertid);
    }

    if((!db_sql_in_scan) && (!db_sql_in_playlist_scan))
        db_sql_update_playlists(NULL);

    if(id)
        *id = insertid;

    DPRINTF(E_SPAM,L_DB,"Exiting db_sql_add\n");
    return DB_E_SUCCESS;
}

/**
 * update a database item
 *
 * @param pmp3 mp3 file to update
 */
int db_sql_update(char **pe, MP3FILE *pmp3, int *id) {
    int err;
    char query[1024];
    char *path;
    char sample_count[40];
    char file_size[40];

    if(!pmp3->time_modified)
        pmp3->time_modified = (int)time(NULL);

    pmp3->db_timestamp = (int)time(NULL);

    sprintf(sample_count,"%lld",pmp3->sample_count);
    sprintf(file_size,"%lld",pmp3->file_size);

    strcpy(query,"UPDATE songs SET "
           "title='%q',"  // title
           "artist='%q',"  // artist
           "album='%q',"  // album
           "genre='%q',"  // genre
           "comment='%q',"  // comment
           "type='%q',"  // type
           "composer='%q',"  // composer
           "orchestra='%q',"  // orchestra
           "conductor='%q',"  // conductor
           "grouping='%q',"  // grouping
           "url='%q',"  // url
           "bitrate=%d,"    // bitrate
           "samplerate=%d,"    // samplerate
           "song_length=%d,"    // song_length
           "file_size=%s,"    // file_size
           "year=%d,"    // year
           "track=%d,"    // track
           "total_tracks=%d,"    // total_tracks
           "disc=%d,"    // disc
           "total_discs=%d,"    // total_discs
           "time_modified=%d,"    // time_modified
           "db_timestamp=%d,"    // db_timestamp
           "bpm=%d,"    // bpm
           "disabled=%d," // disabled
           "compilation=%d,"    // compilation
           "rating=%d,"    // rating
           "sample_count=%s," // sample_count
           "codectype='%q',"   // codec
           "album_artist='%q'"
           " WHERE path='%q' and idx=%d");

    path = _db_proper_path(pmp3->path);

    err = db_sql_exec_fn(pe,E_LOG,query,
                         STR(pmp3->title),
                         STR(pmp3->artist),
                         STR(pmp3->album),
                         STR(pmp3->genre),
                         STR(pmp3->comment),
                         STR(pmp3->type),
                         STR(pmp3->composer),
                         STR(pmp3->orchestra),
                         STR(pmp3->conductor),
                         STR(pmp3->grouping),
                         STR(pmp3->url),
                         pmp3->bitrate,
                         pmp3->samplerate,
                         pmp3->song_length,
                         file_size,
                         pmp3->year,
                         pmp3->track,
                         pmp3->total_tracks,
                         pmp3->disc,
                         pmp3->total_discs,
                         pmp3->time_modified,
                         pmp3->db_timestamp,
                         pmp3->bpm,
                         pmp3->disabled,
                         pmp3->compilation,
                         pmp3->rating,
                         sample_count,
                         STR(pmp3->codectype),
                         STR(pmp3->album_artist),
                         path,
                         pmp3->index);

    if(err != DB_E_SUCCESS)
        DPRINTF(E_FATAL,L_DB,"Error updating file: %s\n",pmp3->fname);

    if(id) { /* we need the insert/update id */
        strcpy(query,"select id from songs where path='%q' and idx=%d");

        err=db_sql_fetch_int(pe,id,query,path,pmp3->index);
        if(err != DB_E_SUCCESS) {
            free(path);
            return err;
        }
    }

    if((db_sql_in_scan || db_sql_in_playlist_scan) && (!db_sql_reload)) {
        if(id) {
            db_sql_exec_fn(NULL,E_FATAL,"insert into updated (id) values (%d)",*id);
        } else {
            strcpy(query,"insert into updated (id) select id from "
                   "songs where path='%q' and idx=%d");
            db_sql_exec_fn(NULL,E_FATAL,query,path,pmp3->index);
        }
    }

    if((!db_sql_in_scan) && (!db_sql_in_playlist_scan))
        db_sql_update_playlists(NULL);

    free(path);
    return 0;
}


/**
 * Update the playlist item counts
 */
int db_sql_update_playlists(char **pe) {
    typedef struct tag_plinfo {
        char *plid;
        char *type;
        char *clause;
    } PLINFO;

    PLINFO *pinfo;
    int playlists;
    int err;
    int index;
    SQL_ROW row;
    char *where_clause;

    DPRINTF(E_LOG,L_DB,"Updating playlists\n");

    /* FIXME: There is a race here for externally added playlists */

    err = db_sql_fetch_int(pe,&playlists,"select count(*) from playlists");

    if(err != DB_E_SUCCESS) {
        return err;
    }

    pinfo = (PLINFO*)malloc(sizeof(PLINFO) * playlists);
    if(!pinfo) {
        DPRINTF(E_FATAL,L_DB,"Malloc error\n");
    }

    /* now, let's walk through the table */
    err = db_sql_enum_begin_fn(pe,"select * from playlists");
    if(err != DB_E_SUCCESS)
        return err;

    /* otherwise, walk the table */
    index=0;
    while((db_sql_enum_fetch_fn(pe, &row) == DB_E_SUCCESS) && (row) &&
          (index < playlists))
    {
        /* process row */
        pinfo[index].plid=strdup(STR(row[PL_ID]));
        pinfo[index].type=strdup(STR(row[PL_TYPE]));
        pinfo[index].clause=strdup(STR(row[PL_QUERY]));
        DPRINTF(E_SPAM,L_DB,"Found playlist %s: type %s, clause %s\n",pinfo[index].plid,
            pinfo[index].type,pinfo[index].clause);
        index++;
    }
    db_sql_enum_end_fn(pe);
    if(index != playlists) {
        DPRINTF(E_FATAL,L_DB,"Playlist count mismatch -- transaction problem?\n");
    }

    /* Now, update the playlists */
    for(index=0;index < playlists; index++) {
        if(atoi(pinfo[index].type) == PL_SMART) {
            /* smart */
            if(!db_sql_parse_smart(NULL,&where_clause,pinfo[index].clause)) {
                DPRINTF(E_LOG,L_DB,"Playlist %d bad syntax",pinfo[index].plid);
                where_clause = strdup("0");
            }
            db_sql_exec_fn(NULL,E_FATAL,"update playlists set items=("
                            "select count(*) from songs where %s) "
                            "where id=%s",where_clause,pinfo[index].plid);
            free(where_clause);
        } else {
            db_sql_exec_fn(NULL,E_FATAL,"update playlists set items=("
                            "select count(*) from playlistitems where "
                            "playlistid=%s) where id=%s",
                            pinfo[index].plid, pinfo[index].plid);
        }

        if(pinfo[index].plid) free(pinfo[index].plid);
        if(pinfo[index].type) free(pinfo[index].type);
        if(pinfo[index].clause) free(pinfo[index].clause);
    }

    free(pinfo);
    return DB_E_SUCCESS;
}


/**
 * start enum based on the DBQUERYINFO struct passed
 *
 * @param pinfo DBQUERYINFO struct detailing what to enum
 */
int db_sql_enum_start(char **pe, DBQUERYINFO *pinfo) {
    char scratch[4096];
    char query[4096];
    char query_select[255];
    char query_count[255];
    char query_rest[4096];
    char *where_clause;
    char *filter;

    int is_smart;
    int have_clause=0;
    int err;
    int browse=0;
    int results=0;
    SQL_ROW temprow;

    query[0] = '\0';
    query_select[0] = '\0';
    query_count[0] = '\0';
    query_rest[0] = '\0';


    /* get the where qualifier to filter based on playlist, if there */
    if((pinfo->playlist_id) && (pinfo->playlist_id != 1)) {
        err = db_sql_enum_begin_fn(pe, "select type,query from playlists "
                                   "where id=%d",pinfo->playlist_id);

        if(err != DB_E_SUCCESS)
            return err;

        err = db_sql_enum_fetch_fn(pe,&temprow);

        if(err != DB_E_SUCCESS) {
            db_sql_enum_end_fn(NULL);
            return err;
        }

        if(!temprow) { /* bad playlist */
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            db_sql_enum_end_fn(NULL);
            return DB_E_INVALID_PLAYLIST;
        }

        is_smart=(atoi(temprow[0]) == 1);
        if(is_smart) {
            if(!db_sql_parse_smart(NULL,&where_clause,temprow[1]))
                where_clause = strdup("0");

            if(!where_clause) {
                db_sql_enum_end_fn(NULL);
                db_get_error(pe,DB_E_PARSE);
                return DB_E_PARSE;
            }

            snprintf(query_rest,sizeof(query_rest)," where (%s)",where_clause);
            free(where_clause);
        } else {

            /* We need to fix playlist queries to stop
             * pulling the whole song db... the performance
             * of these playlist queries sucks.
             */

            if(pinfo->correct_order) {
                sprintf(query_rest,",playlistitems where "
                        "(songs.id=playlistitems.songid and "
                        "playlistitems.playlistid=%d) order by "
                        "playlistitems.id",pinfo->playlist_id);
            } else {
                sprintf(query_rest," where (songs.id in (select songid from "
                        "playlistitems where playlistid=%d))",
                        pinfo->playlist_id);
            }
        }
        have_clause=1;
        db_sql_enum_end_fn(NULL);
    }

    switch(pinfo->query_type) {
    case queryTypeItems:
        strcpy(query_select,"select * from songs ");
        strcpy(query_count,"select count (*) from songs ");
        break;

    case queryTypePlaylists:
        strcpy(query_select,"select * from playlists ");
        strcpy(query_count,"select count (*) from playlists ");
        //        sprintf(query_rest,"where (%s)",query_playlist);
        break;

    case queryTypePlaylistItems:  /* Figure out if it's smart or dull */
        sprintf(query_select,"select * from songs ");
        sprintf(query_count,"select count(songs.id) from songs ");
        break;

        /* Note that sqlite doesn't support COUNT(DISTINCT x) */
    case queryTypeBrowseAlbums:
        strcpy(query_select,"select distinct album from songs ");
        strcpy(query_count,"select count(album) from (select "
                           "distinct album from songs ");
        browse=1;
        break;

    case queryTypeBrowseArtists:
        strcpy(query_select,"select distinct artist from songs ");
        strcpy(query_count,"select count(artist) from (select "
                           "distinct artist from songs ");
        browse=1;
        break;

    case queryTypeBrowseGenres:
        strcpy(query_select,"select distinct genre from songs ");
        strcpy(query_count,"select count(genre) from (select "
                           "distinct genre from songs ");
        browse=1;
        break;

    case queryTypeBrowseComposers:
        strcpy(query_select,"select distinct composer from songs ");
        strcpy(query_count,"select count(composer) from (select "
                           "distinct composer from songs ");
        browse=1;
        break;
    default:
        DPRINTF(E_LOG,L_DB|L_DAAP,"Unknown query type\n");
        return -1;
    }

    /* don't browse radio station metadata */
    if(browse) {
        if(have_clause) {
            strcat(query_rest," and ");
        } else {
            strcpy(query_rest," where ");
            have_clause = 1;
        }

        strcat(query_rest,"(data_kind = 0) ");
    }

    /* Apply the query/filter */
    if(pinfo->pt) {
        DPRINTF(E_DBG,L_DB,"Got query/filter\n");
        filter = sp_sql_clause(pinfo->pt);
        if(filter) {
            if(have_clause) {
                strcat(query_rest," and ");
            } else {
                strcpy(query_rest," where ");
                have_clause=1;
            }
            strcat(query_rest,"(");
            strcat(query_rest,filter);
            strcat(query_rest,")");
            free(filter);
        } else {
            DPRINTF(E_LOG,L_DB,"Error getting sql for parse tree\n");
        }
    } else {
        DPRINTF(E_DBG,L_DB,"No query/filter\n");
    }

    /* disable empty */
    if(browse) {
        if((have_clause) || (pinfo->pt)) {
            strcat(query_rest," and (");
        } else {
            strcpy(query_rest," where (");
            have_clause = 1;
        }

        switch(pinfo->query_type) {
        case queryTypeBrowseAlbums:
            strcat(query_rest,"album");
            break;
        case queryTypeBrowseArtists:
            strcat(query_rest,"artist");
            break;
        case queryTypeBrowseGenres:
            strcat(query_rest,"genre");
            break;
        case queryTypeBrowseComposers:
            strcat(query_rest,"composer");
            break;
        default: /* ?? */
            strcat(query_rest,"album");
            break;
        }

        strcat(query_rest, " !='')");
    }

    if((pinfo->index_type != indexTypeNone) || (pinfo->want_count)) {
        /* the only time returned count is not specifiedtotalcount
         * is if we have an index. */
        strcpy(scratch,query_count);
        strcat(scratch,query_rest);
        if(browse)
            strcat(scratch,")");

        err = db_sql_fetch_int(pe,&results,"%s",scratch);
        if(err != DB_E_SUCCESS)
            return err;
        DPRINTF(E_DBG,L_DB,"Number of results: %d\n",results);
        pinfo->specifiedtotalcount = results;
    }

    strcpy(query,query_select);
    strcat(query,query_rest);

    /* FIXME: sqlite specific */
    /* Apply any index */
    switch(pinfo->index_type) {
    case indexTypeFirst:
        sprintf(scratch," limit %d",pinfo->index_low);
        break;
    case indexTypeLast:
        if(pinfo->index_low >= results) {
            sprintf(scratch," limit %d",pinfo->index_low); /* unnecessary */
        } else {
            sprintf(scratch," limit %d offset %d",pinfo->index_low, results-pinfo->index_low);
        }
        break;
    case indexTypeSub:
        sprintf(scratch," limit %d offset %d",
                pinfo->index_high - pinfo->index_low + 1,
                pinfo->index_low);
        break;
    case indexTypeNone:
        break;
    default:
        DPRINTF(E_LOG,L_DB,"Bad indexType: %d\n",(int)pinfo->index_type);
        scratch[0]='\0';
        break;
    }

    if(pinfo->index_type != indexTypeNone)
        strcat(query,scratch);

    /* start fetching... */
    err=db_sql_enum_begin_fn(pe,"%s",query);
    return err;
}


/**
 * fetch the next row in raw row format
 */
int db_sql_enum_fetch_row(char **pe, PACKED_MP3FILE *row, DBQUERYINFO *pinfo) {
    int err;

    err=db_sql_enum_fetch_fn(pe, (char***)row);
    if(err != DB_E_SUCCESS) {
        db_sql_enum_end_fn(NULL);
        return err;
    }

    return DB_E_SUCCESS;
}

/**
 * start the enum again
 */
int db_sql_enum_reset(char **pe, DBQUERYINFO *pinfo) {
    return db_sql_enum_restart_fn(pe);
}


/**
 * stop the enum
 */
int db_sql_enum_end(char **pe) {
    return db_sql_enum_end_fn(pe);
}

void db_sql_build_m3ufile(SQL_ROW valarray, M3UFILE *pm3u) {
    memset(pm3u,0x00,sizeof(M3UFILE));

    pm3u->id=db_sql_atoi(valarray[0]);
    pm3u->title=db_sql_strdup(valarray[1]);
    pm3u->type=db_sql_atoi(valarray[2]);
    pm3u->items=db_sql_atoi(valarray[3]);
    pm3u->query=db_sql_strdup(valarray[4]);
    pm3u->db_timestamp=db_sql_atoi(valarray[5]);
    pm3u->path=db_sql_strdup(valarray[6]);
    pm3u->index=db_sql_atoi(valarray[7]);
    return;
}

void db_sql_build_mp3file(SQL_ROW valarray, MP3FILE *pmp3) {
    memset(pmp3,0x00,sizeof(MP3FILE));
    pmp3->id=db_sql_atoi(valarray[0]);
    pmp3->path=db_sql_strdup(valarray[1]);
    pmp3->fname=db_sql_strdup(valarray[2]);
    pmp3->title=db_sql_strdup(valarray[3]);
    pmp3->artist=db_sql_strdup(valarray[4]);
    pmp3->album=db_sql_strdup(valarray[5]);
    pmp3->genre=db_sql_strdup(valarray[6]);
    pmp3->comment=db_sql_strdup(valarray[7]);
    pmp3->type=db_sql_strdup(valarray[8]);
    pmp3->composer=db_sql_strdup(valarray[9]);
    pmp3->orchestra=db_sql_strdup(valarray[10]);
    pmp3->conductor=db_sql_strdup(valarray[11]);
    pmp3->grouping=db_sql_strdup(valarray[12]);
    pmp3->url=db_sql_strdup(valarray[13]);
    pmp3->bitrate=db_sql_atoi(valarray[14]);
    pmp3->samplerate=db_sql_atoi(valarray[15]);
    pmp3->song_length=db_sql_atoi(valarray[16]);
    pmp3->file_size=db_sql_atol(valarray[17]);
    pmp3->year=db_sql_atoi(valarray[18]);
    pmp3->track=db_sql_atoi(valarray[19]);
    pmp3->total_tracks=db_sql_atoi(valarray[20]);
    pmp3->disc=db_sql_atoi(valarray[21]);
    pmp3->total_discs=db_sql_atoi(valarray[22]);
    pmp3->bpm=db_sql_atoi(valarray[23]);
    pmp3->compilation=db_sql_atoi(valarray[24]);
    pmp3->rating=db_sql_atoi(valarray[25]);
    pmp3->play_count=db_sql_atoi(valarray[26]);
    pmp3->data_kind=db_sql_atoi(valarray[27]);
    pmp3->item_kind=db_sql_atoi(valarray[28]);
    pmp3->description=db_sql_strdup(valarray[29]);
    pmp3->time_added=db_sql_atoi(valarray[30]);
    pmp3->time_modified=db_sql_atoi(valarray[31]);
    pmp3->time_played=db_sql_atoi(valarray[32]);
    pmp3->db_timestamp=db_sql_atoi(valarray[33]);
    pmp3->disabled=db_sql_atoi(valarray[34]);
    pmp3->sample_count=db_sql_atol(valarray[35]);
    pmp3->force_update=db_sql_atoi(valarray[36]);
    pmp3->codectype=db_sql_strdup(valarray[37]);
    pmp3->index=db_sql_atoi(valarray[38]);
    pmp3->has_video=db_sql_atoi(valarray[39]);
    pmp3->contentrating=db_sql_atoi(valarray[40]);
    pmp3->bits_per_sample=db_sql_atoi(valarray[41]);
    pmp3->album_artist=db_sql_strdup(valarray[42]);
}

/**
 * fetch a playlist by path and index
 *
 * @param path path to fetch
 */
M3UFILE *db_sql_fetch_playlist(char **pe, char *path, int index) {
    int result;
    M3UFILE *pm3u=NULL;
    SQL_ROW row;
    char *proper_path;

    proper_path = _db_proper_path(path);
    result = db_sql_fetch_row(pe, &row, "select * from playlists where "
                              "path='%q' and idx=%d",proper_path,index);

    free(proper_path);

    if(result != DB_E_SUCCESS) {
        if(result == DB_E_NOROWS) {
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_PLAYLIST);
            return NULL;
        }

        return NULL; /* sql error or something */
    }

    pm3u=(M3UFILE*)malloc(sizeof(M3UFILE));
    if(!pm3u)
        DPRINTF(E_FATAL,L_MISC,"malloc error: db_sql_fetch_playlist\n");

    db_sql_build_m3ufile(row,pm3u);
    db_sql_dispose_row();

    if((db_sql_in_playlist_scan) && (!db_sql_reload)) {
        db_sql_exec_fn(NULL,E_FATAL,"insert into plupdated values (%d)",
                        pm3u->id);
    }

    return pm3u;
}


/* FIXME: bad error handling -- not like the rest */

/**
 * fetch a MP3FILE for a specific id
 *
 * @param id id to fetch
 */
MP3FILE *db_sql_fetch_item(char **pe, int id) {
    SQL_ROW row;
    MP3FILE *pmp3=NULL;
    int err;

    err=db_sql_fetch_row(pe,&row,"select * from songs where id=%d",id);
    if(err != DB_E_SUCCESS) {
        if(err == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_INVALID_SONGID);
            return NULL;
        }
        return NULL;
    }

    pmp3=(MP3FILE*)malloc(sizeof(MP3FILE));
    if(!pmp3)
        DPRINTF(E_FATAL,L_MISC,"Malloc error in db_sql_fetch_item\n");

    db_sql_build_mp3file(row,pmp3);

    db_sql_dispose_row();

    if ((db_sql_in_scan || db_sql_in_playlist_scan) && (!db_sql_reload)) {
        db_sql_exec_fn(pe,E_DBG,"INSERT INTO updated VALUES (%d)",id);
    }

    return pmp3;
}

/**
 * retrieve a MP3FILE struct for the song with a given path
 *
 * @param path path of the file to retreive
 */
MP3FILE *db_sql_fetch_path(char **pe, char *path, int index) {
    SQL_ROW row;
    MP3FILE *pmp3=NULL;
    int err;
    char *query;
    char *proper_path;

    /* if we are doing a full reload, then it can't be in here.
     * besides, we don't have an index anyway, so we don't want to
     * do this fetch right now
     */
    if((db_sql_in_scan) && (db_sql_reload))
        return NULL;

    /* not very portable, but works for sqlite */
    proper_path = _db_proper_path(path);
    query="select * from songs where path='%q' and idx=%d";

    err=db_sql_fetch_row(pe,&row,query,proper_path,index);
    free(proper_path);

    if(err != DB_E_SUCCESS) {
        if(err == DB_E_NOROWS) { /* Override generic error */
            if(pe) { free(*pe); };
            db_get_error(pe,DB_E_NOTFOUND);
            return NULL;
        }
        return NULL;
    }

    pmp3=(MP3FILE*)malloc(sizeof(MP3FILE));
    if(!pmp3)
        DPRINTF(E_FATAL,L_MISC,"Malloc error in db_sql_fetch_path\n");

    db_sql_build_mp3file(row,pmp3);

    db_sql_dispose_row();

    if ((db_sql_in_scan || db_sql_in_playlist_scan) && (!db_sql_reload)) {
        db_sql_exec_fn(pe,E_DBG,"INSERT INTO updated VALUES (%d)",pmp3->id);
    }

    return pmp3;
}

/**
 * dispose of a MP3FILE struct that was obtained
 * from db_sql_fetch_item
 *
 * @param pmp3 item obtained from db_sql_fetch_item
 */
void db_sql_dispose_item(MP3FILE *pmp3) {
    if(!pmp3)
        return;

    MAYBEFREE(pmp3->path);
    MAYBEFREE(pmp3->fname);
    MAYBEFREE(pmp3->title);
    MAYBEFREE(pmp3->artist);
    MAYBEFREE(pmp3->album);
    MAYBEFREE(pmp3->genre);
    MAYBEFREE(pmp3->comment);
    MAYBEFREE(pmp3->type);
    MAYBEFREE(pmp3->composer);
    MAYBEFREE(pmp3->orchestra);
    MAYBEFREE(pmp3->conductor);
    MAYBEFREE(pmp3->grouping);
    MAYBEFREE(pmp3->description);
    MAYBEFREE(pmp3->url);
    MAYBEFREE(pmp3->codectype);
    MAYBEFREE(pmp3->album_artist);
    free(pmp3);
}

void db_sql_dispose_playlist(M3UFILE *pm3u) {
    if(!pm3u)
        return;

    MAYBEFREE(pm3u->title);
    MAYBEFREE(pm3u->query);
    MAYBEFREE(pm3u->path);
    free(pm3u);
}

/**
 * count either the number of playlists, or the number of
 * songs
 *
 * @param type either countPlaylists or countSongs (type to count)
 */
int db_sql_get_count(char **pe, int *count, CountType_t type) {
    char *table;
    int err;

    switch(type) {
    case countPlaylists:
        table="playlists";
        break;

    case countSongs:
    default:
        table="songs";
        break;
    }

    err=db_sql_fetch_int(pe,count,"select count(*) FROM %q", table);
    return err;
}

/**
 * increment the play_count and time_played
 *
 * @param pe error string
 * @param id id of the song to increment
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_sql_playcount_increment(char **pe, int id) {
    time_t now = time(NULL);

    return db_sql_exec_fn(pe,E_INF,"update songs set play_count=play_count + 1"
        ", time_played=%d where id=%d",now,id);
}


