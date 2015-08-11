/*
 * $Id: db-generic.c 1674 2007-10-07 04:39:30Z rpedde $
 * Generic db implementation for specific sql backend
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

#define _XOPEN_SOURCE 500  /** unix98?  pthread_once_t, etc */

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "daapd.h"
#include "db-generic.h"
#include "err.h"
#include "mp3-scanner.h"

#include "db-sql.h"

#define DB_VERSION 1
#define MAYBEFREE(a) { if((a)) free((a)); };

/** pointers to database-specific functions */
typedef struct tag_db_functions {
    char *name;
    int(*dbs_open)(char **, char *);
    int(*dbs_init)(int);
    int(*dbs_deinit)(void);
    int(*dbs_add)(char **, MP3FILE*, int*);
    int(*dbs_add_playlist)(char **, char *, int, char *,char *, int, int *);
    int(*dbs_add_playlist_item)(char **, int, int);
    int(*dbs_delete_playlist)(char **, int);
    int(*dbs_delete_playlist_item)(char **, int, int);
    int(*dbs_edit_playlist)(char **, int, char*, char*);
    int(*dbs_playcount_increment)(char **, int);
    int(*dbs_enum_start)(char **, DBQUERYINFO *);
    int(*dbs_enum_fetch_row)(char **, PACKED_MP3FILE *, DBQUERYINFO *);
    int(*dbs_enum_reset)(char **, DBQUERYINFO *);
    int(*dbs_enum_end)(char **);
    int(*dbs_force_rescan)(char **);
    int(*dbs_start_scan)(void);
    int(*dbs_end_song_scan)(void);
    int(*dbs_end_scan)(void);
    int(*dbs_get_count)(char **, int *, CountType_t);
    MP3FILE*(*dbs_fetch_item)(char **, int);
    MP3FILE*(*dbs_fetch_path)(char **, char *,int);
    M3UFILE*(*dbs_fetch_playlist)(char **, char *, int);
    void(*dbs_dispose_item)(MP3FILE*);
    void(*dbs_dispose_playlist)(M3UFILE*);
}DB_FUNCTIONS;

/** All supported backend databases, and pointers to the db specific implementations */
DB_FUNCTIONS db_functions[] = {
#ifdef HAVE_LIBSQLITE
    {
        "sqlite",
        db_sql_open_sqlite2,
        db_sql_init,
        db_sql_deinit,
        db_sql_add,
        db_sql_add_playlist,
        db_sql_add_playlist_item,
        db_sql_delete_playlist,
        db_sql_delete_playlist_item,
        db_sql_edit_playlist,
        db_sql_playcount_increment,
        db_sql_enum_start,
        db_sql_enum_fetch_row,
        db_sql_enum_reset,
        db_sql_enum_end,
        db_sql_force_rescan,
        db_sql_start_scan,
        db_sql_end_song_scan,
        db_sql_end_scan,
        db_sql_get_count,
        db_sql_fetch_item,
        db_sql_fetch_path,
        db_sql_fetch_playlist,
        db_sql_dispose_item,
        db_sql_dispose_playlist
    },
#endif
#ifdef HAVE_LIBSQLITE3
    {
        "sqlite3",
        db_sql_open_sqlite3,
        db_sql_init,
        db_sql_deinit,
        db_sql_add,
        db_sql_add_playlist,
        db_sql_add_playlist_item,
        db_sql_delete_playlist,
        db_sql_delete_playlist_item,
        db_sql_edit_playlist,
        db_sql_playcount_increment,
        db_sql_enum_start,
        db_sql_enum_fetch_row,
        db_sql_enum_reset,
        db_sql_enum_end,
        db_sql_force_rescan,
        db_sql_start_scan,
        db_sql_end_song_scan,
        db_sql_end_scan,
        db_sql_get_count,
        db_sql_fetch_item,
        db_sql_fetch_path,
        db_sql_fetch_playlist,
        db_sql_dispose_item,
        db_sql_dispose_playlist
    },
#endif
    { NULL,NULL }
};
char *db_error_list[] = {
    "Success",
    "Misc SQL Error: %s",
    "Duplicate Playlist: %s",
    "Missing playlist spec",
    "Cannot add playlist items to a playlist of that type",
    "No rows returned",
    "Invalid playlist id: %d",
    "Invalid song id: %d",
    "Parse error: %s",
    "No backend database support for type: %s",
    "Could not initialize thread pool",
    "Passed buffer too small for result",
    "Wrong db schema.  Use mtd-update to upgrade the db.",
    "Database error: %s",
    "Malloc error",
    "Path not found"
};

/* Globals */
static DB_FUNCTIONS *db_current=&db_functions[0];     /**< current database backend */
static int db_revision_no=2;                          /**< current revision of the db */
static pthread_once_t db_initlock=PTHREAD_ONCE_INIT;  /**< to initialize the rwlock */
static int db_is_scanning=0;
static pthread_rwlock_t db_rwlock;                    /**< pthread r/w sync for the database */

/* Forwards */
static void db_writelock(void);
static void db_readlock(void);
static int db_unlock(void);
static void db_init_once(void);
static void db_utf8_validate(MP3FILE *pmp3);
static int db_utf8_validate_string(char *string);
static void db_trim_strings(MP3FILE *pmp3);
static void db_trim_string(char *string);

/*
 * db_readlock
 *
 * If this fails, something is so amazingly hosed, we might just as well
 * terminate.
 */
void db_readlock(void) {
    int err;

    DPRINTF(E_SPAM,L_LOCK,"entering db_readlock\n");
    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
        DPRINTF(E_FATAL,L_DB,"cannot lock rdlock: %s\n",strerror(err));
    }
    DPRINTF(E_SPAM,L_LOCK,"db_readlock acquired\n");
}

/*
 * db_writelock
 *
 * same as above
 */
void db_writelock(void) {
    int err;

    DPRINTF(E_SPAM,L_LOCK,"entering db_writelock\n");
    if((err=pthread_rwlock_wrlock(&db_rwlock))) {
        DPRINTF(E_FATAL,L_DB,"cannot lock rwlock: %s\n",strerror(err));
    }
    DPRINTF(E_SPAM,L_LOCK,"db_writelock acquired\n");
}

/*
 * db_unlock
 *
 * useless, but symmetrical
 */
int db_unlock(void) {
    DPRINTF(E_SPAM,L_LOCK,"releasing db lock\n");
    return pthread_rwlock_unlock(&db_rwlock);
}

/**
 * Build an error string
 */
void db_get_error(char **pe, int error, ...) {
    va_list ap;
    char errbuf[1024];

    if(!pe)
        return;

    va_start(ap, error);
    vsnprintf(errbuf, sizeof(errbuf), db_error_list[error], ap);
    va_end(ap);

    DPRINTF(E_SPAM,L_MISC,"Raising error: %s\n",errbuf);

    *pe = strdup(errbuf);
}

/**
 * Must dynamically initialize the rwlock, as Mac OSX 10.3 (at least)
 * doesn't have a static initializer for rwlocks
 */
void db_init_once(void) {
    pthread_rwlock_init(&db_rwlock,NULL);
}

/**
 * Open the database.  This is done before we drop privs, that
 * way if the database only has root perms, then it can still
 * be opened.
 *
 * \param parameters This is backend-specific (mysql, sqlite, etc)
 */
int db_open(char **pe, char *type, char *parameters) {
    int result;

    DPRINTF(E_DBG,L_DB,"Opening database\n");

    if(pthread_once(&db_initlock,db_init_once))
        return -1;

    db_current = &db_functions[0];
    if(type) {
        while((db_current->name) && (strcasecmp(db_current->name,type))) {
            db_current++;
        }

        if(!db_current->name) {
            /* end of list -- no match */
            db_get_error(pe,DB_E_BADPROVIDER,type);
            return DB_E_BADPROVIDER;
        }
    }

    result=db_current->dbs_open(pe, parameters);


    DPRINTF(E_DBG,L_DB,"Results: %d\n",result);
    return result;
}

/**
 * Initialize the database, including marking it for full reload if necessary.
 *
 * \param reload whether or not to do a full reload of the database
 */
int db_init(int reload) {
    return db_current->dbs_init(reload);
}

/**
 * Close the database.
 */
int db_deinit(void) {
    return db_current->dbs_deinit();
}

/**
 * return the current db revision.  this is mostly to determine
 * when its time to send an updated version to the client
 */
int db_revision(void) {
    int revision;

    db_readlock();
    revision=db_revision_no;
    db_unlock();

    return revision;
}

/**
 * is the db currently in scanning mode?
 */
int db_scanning(void) {
    return db_is_scanning;
}

/**
 * add (or update) a file
 */
int db_add(char **pe, MP3FILE *pmp3, int *id) {
    int retval;

    db_writelock();
    db_utf8_validate(pmp3);
    db_trim_strings(pmp3);
    retval=db_current->dbs_add(pe,pmp3,id);
    db_revision_no++;
    db_unlock();

    return retval;
}

/**
 * add a playlist
 *
 * \param name name of playlist to add
 * \param type type of playlist to add: 0 - static, 1 - smart, 2 - m3u
 * \param clause where clause (if type 1)
 * \param playlistid returns the id of the playlist created
 * \returns 0 on success, error code otherwise
 */
int db_add_playlist(char **pe, char *name, int type, char *clause, char *path, int index, int *playlistid) {
    int retval;

    db_writelock();
    retval=db_current->dbs_add_playlist(pe,name,type,clause,path,index,playlistid);
    if(retval == DB_E_SUCCESS)
        db_revision_no++;
    db_unlock();

    return retval;
}

/**
 * add a song to a static playlist
 *
 * \param playlistid playlist to add song to
 * \param songid song to add to playlist
 * \returns 0 on success, DB_E_ code otherwise
 */
int db_add_playlist_item(char **pe, int playlistid, int songid) {
    int retval;

    db_writelock();
    retval=db_current->dbs_add_playlist_item(pe,playlistid,songid);
    if(retval == DB_E_SUCCESS)
        db_revision_no++;
    db_unlock();

    return retval;
}

/**
 * delete a playlist
 *
 * \param playlistid id of the playlist to delete
 * \returns 0 on success, error code otherwise
 */
int db_delete_playlist(char **pe, int playlistid) {
    int retval;

    db_writelock();
    retval=db_current->dbs_delete_playlist(pe,playlistid);
    if(retval == DB_E_SUCCESS)
        db_revision_no++;
    db_unlock();

    return retval;
}

/**
 * delete an item from a playlist
 *
 * \param playlistid id of the playlist to delete
 * \param songid id of the song to delete
 * \returns 0 on success, error code otherwise
 */
int db_delete_playlist_item(char **pe, int playlistid, int songid) {
    int retval;

    db_writelock();
    retval=db_current->dbs_delete_playlist_item(pe,playlistid,songid);
    if(retval == DB_E_SUCCESS)
        db_revision_no++;
    db_unlock();

    return retval;
}

/**
 * edit a playlist
 *
 * @param id playlist id to edit
 * @param name new name of playlist
 * @param clause new where clause
 */
int db_edit_playlist(char **pe, int id, char *name, char *clause) {
    int retval;

    db_writelock();

    retval = db_current->dbs_edit_playlist(pe, id, name, clause);
    db_unlock();
    return retval;
}


/**
 * increment the playcount info for a particular song
 * (play_count and time_played).
 *
 * @param pe error string
 * @param id id of song to incrmrent
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int db_playcount_increment(char **pe, int id) {
    int retval;

    db_writelock();
    retval = db_current->dbs_playcount_increment(pe, id);
    db_unlock();

    return retval;
}

/**
 * start a db enumeration, based info in the DBQUERYINFO struct
 *
 * \param pinfo pointer to DBQUERYINFO struction
 * \returns 0 on success, -1 on failure
 */
int db_enum_start(char **pe, DBQUERYINFO *pinfo) {
    int retval;

    db_writelock();
    retval=db_current->dbs_enum_start(pe, pinfo);

    if(retval) {
        db_unlock();
        return retval;
    }

    return 0;
}

/**
 * fetch the next item int he result set started by the db enum.  this
 * will be in native packed row format
 */
int db_enum_fetch_row(char **pe, PACKED_MP3FILE *row, DBQUERYINFO *pinfo) {
    return db_current->dbs_enum_fetch_row(pe, row, pinfo);
}


/**
 * reset the enum, without coming out the the db_writelock
 */
int db_enum_reset(char **pe, DBQUERYINFO *pinfo) {
    return db_current->dbs_enum_reset(pe,pinfo);
}

/**
 * finish the enumeration
 */
int db_enum_end(char **pe) {
    int retval;

    retval=db_current->dbs_enum_end(pe);
    db_unlock();
    return retval;
}


/**
 * fetch a MP3FILE struct given an id.  This will be done
 * mostly only by the web interface, and when streaming a song
 *
 * \param id id of the item to get details for
 */
MP3FILE *db_fetch_item(char **pe, int id) {
    MP3FILE *retval;

    db_readlock();
    retval=db_current->dbs_fetch_item(pe, id);
    db_unlock();

    return retval;
}

MP3FILE *db_fetch_path(char **pe, char *path,int index) {
    MP3FILE *retval;

    db_readlock();
    retval=db_current->dbs_fetch_path(pe,path, index);
    db_unlock();

    return retval;
}

M3UFILE *db_fetch_playlist(char **pe, char *path, int index) {
    M3UFILE *retval;

    db_readlock();
    retval=db_current->dbs_fetch_playlist(pe,path,index);
    db_unlock();

    return retval;
}

int db_force_rescan(char **pe) {
    int retval;
    db_writelock();
    retval = db_current->dbs_force_rescan(pe);
    db_unlock();

    return retval;
}

int db_start_scan(void) {
    int retval;

    db_writelock();
    retval=db_current->dbs_start_scan();
    db_is_scanning=1;
    db_unlock();

    return retval;
}

int db_end_song_scan(void) {
    int retval;

    db_writelock();
    retval=db_current->dbs_end_song_scan();
    db_unlock();

    return retval;
}

int db_end_scan(void) {
    int retval;

    db_writelock();
    retval=db_current->dbs_end_scan();
    db_is_scanning=0;
    db_unlock();

    return retval;
}

void db_dispose_item(MP3FILE *pmp3) {
    db_current->dbs_dispose_item(pmp3);
}

void db_dispose_playlist(M3UFILE *pm3u) {
    db_current->dbs_dispose_playlist(pm3u);
}

int db_get_count(char **pe, int *count, CountType_t type) {
    int retval;

    db_readlock();
    retval=db_current->dbs_get_count(pe,count,type);
    db_unlock();

    return retval;
}


/*
 * FIXME: clearly a stub
 */
int db_get_song_count(char **pe, int *count) {
    return db_get_count(pe, count, countSongs);
}

int db_get_playlist_count(char **pe, int *count) {
    return db_get_count(pe, count, countPlaylists);
}


/**
 * check the strings in a MP3FILE to ensure they are
 * valid utf-8.  If they are not, the string will be corrected
 *
 * \param pmp3 MP3FILE to verify for valid utf-8
 */
void db_utf8_validate(MP3FILE *pmp3) {
    int is_invalid=0;

    /* we won't bother with path and fname... those were culled with the
     * scan.  Even if they are invalid (_could_ they be?), then we
     * won't be able to open the file if we change them.  Likewise,
     * we won't do type or description, as these can't be bad, or they
     * wouldn't have been scanned */

    is_invalid = db_utf8_validate_string(pmp3->title);
    is_invalid |= db_utf8_validate_string(pmp3->artist);
    is_invalid |= db_utf8_validate_string(pmp3->album);
    is_invalid |= db_utf8_validate_string(pmp3->genre);
    is_invalid |= db_utf8_validate_string(pmp3->comment);
    is_invalid |= db_utf8_validate_string(pmp3->composer);
    is_invalid |= db_utf8_validate_string(pmp3->orchestra);
    is_invalid |= db_utf8_validate_string(pmp3->conductor);
    is_invalid |= db_utf8_validate_string(pmp3->grouping);
    is_invalid |= db_utf8_validate_string(pmp3->url);

    if(is_invalid) {
        DPRINTF(E_LOG,L_SCAN,"Invalid UTF-8 in %s\n",pmp3->path);
    }
}

/**
 * check a string to verify it is valid utf-8.  The passed
 * string will be in-place modified to be utf-8 clean by substituting
 * the character '?' for invalid utf-8 codepoints
 *
 * \param string string to clean
 */
int db_utf8_validate_string(char *string) {
    char *current = string;
    int run,r_current;
    int retval=0;

    if(!string)
        return 0;

     while(*current) {
        if(!((*current) & 0x80)) {
            current++;
        } else {
            run=0;

            /* it's a lead utf-8 character */
            if((*current & 0xE0) == 0xC0) run=1;
            if((*current & 0xF0) == 0xE0) run=2;
            if((*current & 0xF8) == 0xF0) run=3;

            if(!run) {
                /* high bit set, but invalid */
                *current++='?';
                retval=1;
            } else {
                r_current=0;
                while((r_current != run) && (*(current + r_current + 1)) &&
                      ((*(current + r_current + 1) & 0xC0) == 0x80)) {
                    r_current++;
                }

                if(r_current != run) {
                    *current++ = '?';
                    retval=1;
                } else {
                    current += (1 + run);
                }
            }
        }
    }

    return retval;
}

/**
 * Trim the spaces off the string values.  It throws off
 * browsing when there are some with and without spaces.
 * This should probably be better fixed by having clean tags,
 * but seemed simple enough, and it does make sense that
 * while we are cleaning tags for, say, utf-8 hygene we might
 * as well get this too.
 *
 * @param pmp3 mp3 struct to fix
 */
void db_trim_strings(MP3FILE *pmp3) {
    db_trim_string(pmp3->title);
    db_trim_string(pmp3->artist);
    db_trim_string(pmp3->album);
    db_trim_string(pmp3->genre);
    db_trim_string(pmp3->comment);
    db_trim_string(pmp3->composer);
    db_trim_string(pmp3->orchestra);
    db_trim_string(pmp3->conductor);
    db_trim_string(pmp3->grouping);
    db_trim_string(pmp3->url);
}

/**
 * trim trailing spaces in a string.  Used by db_trim_strings
 *
 * @param string string to trim
 */
void db_trim_string(char *string) {
    if(!string)
        return;

    while(strlen(string) && (string[strlen(string) - 1] == ' '))
        string[strlen(string) - 1] = '\0';
}

