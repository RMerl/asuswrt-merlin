/*
 * $Id: db-sql-sqlite2.c 1676 2007-10-13 22:03:52Z rpedde $
 * sqlite2-specific db implementation
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

/*
 * This file handles sqlite2 databases.  SQLite2 databases
 * should have a dsn of:
 *
 * sqlite2:/path/to/folder
 *
 * The actual db will be appended to the passed path.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 500

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "daapd.h"
#include "conf.h"
#include "err.h"
#include "db-generic.h"
#include "db-sql.h"
#include "db-sql-sqlite2.h"

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif


/* Globals */
static pthread_mutex_t db_sqlite2_mutex = PTHREAD_MUTEX_INITIALIZER; /**< sqlite not reentrant */
static sqlite_vm *db_sqlite2_pvm;
static int db_sqlite2_reload=0;
static char *db_sqlite2_enum_query;
static pthread_key_t db_sqlite2_key;

static char db_sqlite2_path[PATH_MAX + 1];

#define DB_SQLITE2_VERSION 13


/* Forwards */
void db_sqlite2_lock(void);
void db_sqlite2_unlock(void);
extern char *db_sqlite2_initial1;
extern char *db_sqlite2_initial2;
int db_sqlite2_enum_begin_helper(char **pe);

/**
 * get (or create) the db handle
 */
sqlite *db_sqlite2_handle(void) {
    sqlite *pdb = NULL;
    char *perr;
    char *pe = NULL;

    pdb = (sqlite *)pthread_getspecific(db_sqlite2_key);
    if(pdb == NULL) { /* don't have a handle yet */
        if((pdb = sqlite_open(db_sqlite2_path,0666,&perr)) == NULL) {
            db_get_error(&pe,DB_E_SQL_ERROR,perr);
            DPRINTF(E_FATAL,L_DB,"db_sqlite2_open: %s (%s)\n",perr,
                    db_sqlite2_path);
            sqlite_freemem(perr);
            db_sqlite2_unlock();
            return NULL;
        }
        sqlite_busy_timeout(pdb,30000);  /* 30 seconds */
        pthread_setspecific(db_sqlite2_key,(void*)pdb);
    }

    return pdb;
}

/**
 * free a thread-specific db handle
 */
void db_sqlite2_freedb(sqlite *pdb) {
    sqlite_close(pdb);
}

/**
 * lock the db_mutex
 */
void db_sqlite2_lock(void) {
    int err;

    //    DPRINTF(E_SPAM,L_LOCK,"entering db_sqlite2_lock\n");
    if((err=pthread_mutex_lock(&db_sqlite2_mutex))) {
        DPRINTF(E_FATAL,L_DB,"cannot lock sqlite lock: %s\n",strerror(err));
    }
    //    DPRINTF(E_SPAM,L_LOCK,"acquired db_sqlite2_lock\n");
}

/**
 * unlock the db_mutex
 */
void db_sqlite2_unlock(void) {
    int err;

    //    DPRINTF(E_SPAM,L_LOCK,"releasing db_sqlite2_lock\n");
    if((err=pthread_mutex_unlock(&db_sqlite2_mutex))) {
        DPRINTF(E_FATAL,L_DB,"cannot unlock sqlite2 lock: %s\n",strerror(err));
    }
    //    DPRINTF(E_SPAM,L_LOCK,"released db_sqlite2_lock\n");
}

/**
 *
 */
char *db_sqlite2_vmquery(char *fmt,va_list ap) {
    return sqlite_vmprintf(fmt,ap);
}

/**
 *
 */
void db_sqlite2_vmfree(char *query) {
    sqlite_freemem(query);
}


/**
 * open a sqlite2 database
 *
 * @param dsn the full dns to the database
 *        (sqlite2:/path/to/database)
 *
 * @returns DB_E_SUCCESS on success
 */
int db_sqlite2_open(char **pe, char *dsn) {
    sqlite *pdb;
    char *perr;
    int ver;
    int err;

    pthread_key_create(&db_sqlite2_key, (void*)db_sqlite2_freedb);
    snprintf(db_sqlite2_path,sizeof(db_sqlite2_path),"%s/songs.db",dsn);

    db_sqlite2_lock();
    pdb=sqlite_open(db_sqlite2_path,0666,&perr);
    if(!pdb) {
        db_get_error(pe,DB_E_SQL_ERROR,perr);
        DPRINTF(E_LOG,L_DB,"db_sqlite2_open: %s (%s)\n",perr,
            db_sqlite2_path);
        sqlite_freemem(perr);
        db_sqlite2_unlock();
        return DB_E_SQL_ERROR;
    }
    sqlite_close(pdb);
    db_sqlite2_unlock();

    err = db_sql_fetch_int(pe,&ver,"select value from config where "
                           "term='version'");
    if(err != DB_E_SUCCESS) {
        if(pe) { free(*pe); }
        /* we'll catch this on the init */
        DPRINTF(E_LOG,L_DB,"Can't get db version. New database?\n");
    } else if(ver < DB_SQLITE2_VERSION) {
        /* we'll deal with this in the db handler */
        DPRINTF(E_LOG,L_DB,"Old database version: %d, expecting %d\n",
                ver, DB_SQLITE2_VERSION);
        db_get_error(pe,DB_E_WRONGVERSION);
        return DB_E_WRONGVERSION;
    } else if(ver > DB_SQLITE2_VERSION) { /* Back-grade from nightlies ? */
        DPRINTF(E_LOG,L_DB,"Bad db version: %d, expecting %d\n",
                ver, DB_SQLITE2_VERSION);
        db_sqlite2_exec(pe,E_FATAL,"insert into config (term, value) "
                        "values ('rescan',1)");
    }

    return DB_E_SUCCESS;
}

/**
 * close the database
 */
int db_sqlite2_close(void) {
    return DB_E_SUCCESS;
}

/**
 * execute a throwaway query against the database, disregarding
 * the outcome
 *
 * @param pe db error structure
 * @param loglevel error level to return if the query fails
 * @param fmt sprintf-style arguements
 *
 * @returns DB_E_SUCCESS on success
 */
int db_sqlite2_exec(char **pe, int loglevel, char *fmt, ...) {
    va_list ap;
    char *query;
    int err;
    char *perr;

    va_start(ap,fmt);
    query=sqlite_vmprintf(fmt,ap);
    va_end(ap);

    DPRINTF(E_DBG,L_DB,"Executing: %s\n",query);

    db_sqlite2_lock();
    err=sqlite_exec(db_sqlite2_handle(),query,NULL,NULL,&perr);
    if(err != SQLITE_OK) {
        db_get_error(pe,DB_E_SQL_ERROR,perr);

        DPRINTF(loglevel == E_FATAL ? E_LOG : loglevel,L_DB,"Query: %s\n",
                query);
        DPRINTF(loglevel,L_DB,"Error: %s\n",perr);
        sqlite_freemem(perr);
    } else {
        DPRINTF(E_DBG,L_DB,"Rows: %d\n",sqlite_changes(db_sqlite2_handle()));
    }
    sqlite_freemem(query);

    db_sqlite2_unlock();

    if(err != SQLITE_OK)
        return DB_E_SQL_ERROR;
    return DB_E_SUCCESS;
}

/**
 * start enumerating rows in a select
 */
int db_sqlite2_enum_begin(char **pe, char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    db_sqlite2_lock();
    db_sqlite2_enum_query = sqlite_vmprintf(fmt,ap);
    va_end(ap);

    return db_sqlite2_enum_begin_helper(pe);
}

int db_sqlite2_enum_begin_helper(char **pe) {
    int err;
    char *perr;
    const char *ptail;

    DPRINTF(E_DBG,L_DB,"Executing: %s\n",db_sqlite2_enum_query);

    err=sqlite_compile(db_sqlite2_handle(),db_sqlite2_enum_query,
                       &ptail,&db_sqlite2_pvm,&perr);

    if(err != SQLITE_OK) {
        db_get_error(pe,DB_E_SQL_ERROR,perr);
        sqlite_freemem(perr);
        db_sqlite2_unlock();
        sqlite_freemem(db_sqlite2_enum_query);
        return DB_E_SQL_ERROR;
    }

    /* otherwise, we leave the db locked while we walk through the enums */
    return DB_E_SUCCESS;
}


/**
 * fetch the next row
 *
 * @param pe error string, if result isn't DB_E_SUCCESS
 * @param pr pointer to a row struct
 *
 * @returns DB_E_SUCCESS with *pr=NULL when end of table,
 *          DB_E_SUCCESS with a valid row when more data,
 *          DB_E_* on error
 */
int db_sqlite2_enum_fetch(char **pe, SQL_ROW *pr) {
    int err;
    char *perr=NULL;
    const char **colarray;
    int cols;
    int counter=10;

    while(counter--) {
        err=sqlite_step(db_sqlite2_pvm,&cols,(const char ***)pr,&colarray);
        if(err != SQLITE_BUSY)
            break;
        usleep(100);
    }

    if(err == SQLITE_DONE) {
        *pr = NULL;
        return DB_E_SUCCESS;
    }

    if(err == SQLITE_ROW) {
        return DB_E_SUCCESS;
    }

    db_get_error(pe,DB_E_SQL_ERROR,perr);
    return DB_E_SQL_ERROR;
}

/**
 * end the db enumeration
 */
int db_sqlite2_enum_end(char **pe) {
    int err;
    char *perr;

    sqlite_freemem(db_sqlite2_enum_query);

    err = sqlite_finalize(db_sqlite2_pvm,&perr);
    if(err != SQLITE_OK) {
        db_get_error(pe,DB_E_SQL_ERROR,perr);
        sqlite_freemem(perr);
        db_sqlite2_unlock();
        return DB_E_SQL_ERROR;
    }

    db_sqlite2_unlock();
    return DB_E_SUCCESS;
}

/**
 * restart the enumeration
 */
int db_sqlite2_enum_restart(char **pe) {
    return db_sqlite2_enum_begin_helper(pe);
}


int db_sqlite2_event(int event_type) {
    switch(event_type) {

    case DB_SQL_EVENT_STARTUP: /* this is a startup with existing songs */
        if(!conf_get_int("database","quick_startup",0))
            db_sqlite2_exec(NULL,E_FATAL,"vacuum");

        /* make sure our indexes exist */
        db_sqlite2_exec(NULL,E_DBG,"create index idx_path on "
                        "songs(path,idx)");
        db_sqlite2_exec(NULL,E_DBG,"create index idx_songid on "
                        "playlistitems(songid)");
        db_sqlite2_exec(NULL,E_DBG,"create index idx_playlistid on "
                        "playlistitems(playlistid,songid)");

        db_sqlite2_reload=0;
        break;

    case DB_SQL_EVENT_FULLRELOAD: /* either a fresh load or force load */
        db_sqlite2_exec(NULL,E_DBG,"drop index idx_path");
        db_sqlite2_exec(NULL,E_DBG,"drop index idx_songid");
        db_sqlite2_exec(NULL,E_DBG,"drop index idx_playlistid");

        db_sqlite2_exec(NULL,E_DBG,"drop table songs");
        //        db_sqlite2_exec(NULL,E_DBG,"drop table playlists");
        db_sqlite2_exec(NULL,E_DBG,"delete from playlists where not type=1 and not type=0");
        db_sqlite2_exec(NULL,E_DBG,"drop table playlistitems");
        db_sqlite2_exec(NULL,E_DBG,"drop table config");

        db_sqlite2_exec(NULL,E_DBG,"vacuum");

        db_sqlite2_exec(NULL,E_DBG,db_sqlite2_initial1);
        db_sqlite2_exec(NULL,E_DBG,db_sqlite2_initial2);

        db_sqlite2_reload=1;
        break;

    case DB_SQL_EVENT_SONGSCANSTART:
        if(db_sqlite2_reload) {
            db_sqlite2_exec(NULL,E_FATAL,"pragma synchronous = off");
            db_sqlite2_exec(NULL,E_FATAL,"begin transaction");
        } else {
            db_sqlite2_exec(NULL,E_DBG,"drop table updated");
            db_sqlite2_exec(NULL,E_FATAL,"create temp table updated (id int)");
            db_sqlite2_exec(NULL,E_DBG,"drop table plupdated");
            db_sqlite2_exec(NULL,E_FATAL,"create temp table plupdated(id int)");
        }
        break;

    case DB_SQL_EVENT_SONGSCANEND:
        if(db_sqlite2_reload) {
            db_sqlite2_exec(NULL,E_FATAL,"commit transaction");
            db_sqlite2_exec(NULL,E_FATAL,"pragma synchronous=normal");
            db_sqlite2_exec(NULL,E_FATAL,"create index idx_path on songs(path,idx)");
            db_sqlite2_exec(NULL,E_DBG,"delete from config where term='rescan'");
        }
        break;

    case DB_SQL_EVENT_PLSCANSTART:
        db_sqlite2_exec(NULL,E_FATAL,"pragma synchronous = off");
        db_sqlite2_exec(NULL,E_FATAL,"begin transaction");
        break;

    case DB_SQL_EVENT_PLSCANEND:
        db_sqlite2_exec(NULL,E_FATAL,"end transaction");
        db_sqlite2_exec(NULL,E_FATAL,"pragma synchronous=normal");

        if(db_sqlite2_reload) {
            db_sqlite2_exec(NULL,E_FATAL,"create index idx_songid on playlistitems(songid)");
            db_sqlite2_exec(NULL,E_FATAL,"create index idx_playlistid on playlistitems(playlistid,songid)");

        } else {
            db_sqlite2_exec(NULL,E_FATAL,"delete from songs where id not in (select id from updated)");
            db_sqlite2_exec(NULL,E_FATAL,"update songs set force_update=0");
            db_sqlite2_exec(NULL,E_FATAL,"drop table updated");

            db_sqlite2_exec(NULL,E_FATAL,"delete from playlists where "
                                         "((type=%d) OR (type=%d)) and "
                                         "id not in (select id from plupdated)",
                                         PL_STATICFILE,PL_STATICXML);
            db_sqlite2_exec(NULL,E_FATAL,"delete from playlistitems where "
                                         "playlistid not in (select distinct "
                                         "id from playlists)");
            db_sqlite2_exec(NULL,E_FATAL,"drop table plupdated");
        }
        db_sqlite2_reload=0;
        break;

    default:
        break;
    }

    return DB_E_SUCCESS;
}

/**
 * get the id of the last auto_update inserted item
 *
 * @returns autoupdate value
 */

int db_sqlite2_insert_id(void) {
    return sqlite_last_insert_rowid(db_sqlite2_handle());
}


char *db_sqlite2_initial1 =
"create table songs (\n"
"   id              INTEGER PRIMARY KEY NOT NULL,\n"      /* 0 */
"   path            VARCHAR(4096) NOT NULL,\n"
"   fname           VARCHAR(255) NOT NULL,\n"
"   title           VARCHAR(1024) DEFAULT NULL,\n"
"   artist          VARCHAR(1024) DEFAULT NULL,\n"
"   album           VARCHAR(1024) DEFAULT NULL,\n"        /* 5 */
"   genre           VARCHAR(255) DEFAULT NULL,\n"
"   comment         VARCHAR(4096) DEFAULT NULL,\n"
"   type            VARCHAR(255) DEFAULT NULL,\n"
"   composer        VARCHAR(1024) DEFAULT NULL,\n"
"   orchestra       VARCHAR(1024) DEFAULT NULL,\n"      /* 10 */
"   conductor       VARCHAR(1024) DEFAULT NULL,\n"
"   grouping        VARCHAR(1024) DEFAULT NULL,\n"
"   url             VARCHAR(1024) DEFAULT NULL,\n"
"   bitrate         INTEGER DEFAULT 0,\n"
"   samplerate      INTEGER DEFAULT 0,\n"               /* 15 */
"   song_length     INTEGER DEFAULT 0,\n"
"   file_size       INTEGER DEFAULT 0,\n"
"   year            INTEGER DEFAULT 0,\n"
"   track           INTEGER DEFAULT 0,\n"
"   total_tracks    INTEGER DEFAULT 0,\n"               /* 20 */
"   disc            INTEGER DEFAULT 0,\n"
"   total_discs     INTEGER DEFAULT 0,\n"
"   bpm             INTEGER DEFAULT 0,\n"
"   compilation     INTEGER DEFAULT 0,\n"
"   rating          INTEGER DEFAULT 0,\n"               /* 25 */
"   play_count      INTEGER DEFAULT 0,\n"
"   data_kind       INTEGER DEFAULT 0,\n"
"   item_kind       INTEGER DEFAULT 0,\n"
"   description     INTEGER DEFAULT 0,\n"
"   time_added      INTEGER DEFAULT 0,\n"               /* 30 */
"   time_modified   INTEGER DEFAULT 0,\n"
"   time_played     INTEGER DEFAULT 0,\n"
"   db_timestamp    INTEGER DEFAULT 0,\n"
"   disabled        INTEGER DEFAULT 0,\n"
"   sample_count    INTEGER DEFAULT 0,\n"               /* 35 */
"   force_update    INTEGER DEFAULT 0,\n"
"   codectype       VARCHAR(5) DEFAULT NULL,\n"
"   idx             INTEGER NOT NULL,\n"
"   has_video       INTEGER DEFAULT 0,\n"
"   contentrating   INTEGER DEFAULT 0,\n"                /* 40 */
"   bits_per_sample INTEGER DEFAULT 0,\n"
"   album_artist    VARCHAR(1024)\n"
");\n"
"create table playlistitems (\n"
"   id             INTEGER PRIMARY KEY NOT NULL,\n"
"   playlistid     INTEGER NOT NULL,\n"
"   songid         INTEGER NOT NULL\n"
");\n"
"create table config (\n"
"   term            VARCHAR(255)    NOT NULL,\n"
"   subterm         VARCHAR(255)    DEFAULT NULL,\n"
"   value           VARCHAR(1024)   NOT NULL\n"
");\n"
"insert into config values ('version','','13');\n";

char *db_sqlite2_initial2 =
"create table playlists (\n"
"   id             INTEGER PRIMARY KEY NOT NULL,\n"
"   title          VARCHAR(255) NOT NULL,\n"
"   type           INTEGER NOT NULL,\n"
"   items          INTEGER NOT NULL,\n"
"   query          VARCHAR(1024),\n"
"   db_timestamp   INTEGER NOT NULL,\n"
"   path           VARCHAR(4096),\n"
"   idx            INTEGER NOT NULL\n"
");\n"
"insert into playlists values (1,'Library',1,0,'1',0,'',0);\n";
