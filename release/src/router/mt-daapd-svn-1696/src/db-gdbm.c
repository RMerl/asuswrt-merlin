/*
 * $Id: $
 * simple gdbm database implementation
 *
 * Copyright (C) 2003-2006 Ron Pedde (rpedde@sourceforge.net)
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

#include <errno.h>
#include <gdbm.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daapd.h"
#include "err.h"
#include "mp3-scanner.h"
#include "redblack.h"
#include "db-generic.h"
#include "db-gdbm.h"

#ifndef GDBM_SYNC
# define GDBM_SYNC 0
#endif

#ifndef GDBM_NOLOCK
# define GDBM_NOLOCK 0
#endif


/* Typedefs */
/* Globals */
static pthread_mutex_t _gdbm_mutex = PTHREAD_MUTEX_INITIALIZER; /**< gdbm not reentrant */
static GDBM_FILE _gdbm_songs;

/* Forwards */
void _gdbm_lock(void);
void _gdbm_unlock(void);

/**
 * lock the db_mutex
 */
void _gdbm_lock(void) {
    int err;

    if((err=pthread_mutex_lock(&_gdbm_mutex))) {
        DPRINTF(E_FATAL,L_DB,"cannot lock gdbm lock: %s\n",strerror(err));
    }
}

/**
 * unlock the db_mutex
 */
void _gdbm_unlock(void) {
    int err;

    if((err=pthread_mutex_unlock(&_gdbm_mutex))) {
        DPRINTF(E_FATAL,L_DB,"cannot unlock gdbm lock: %s\n",strerror(err));
    }
}


/**
 * open the gdbm database
 *
 * @param pe error buffer
 * @param parameters db-specific parameter.  In this case,
 *                   the path to use
 * @returns DB_E_SUCCESS on success, DB_E_* otherwise.
 */
int db_gdbm_open(char **pe, char *parameters) {
    char db_path[PATH_MAX + 1];
    int result = DB_E_SUCCESS;

    snprintf(db_path,sizeof(db_path),"%s/%s",parameters,"songs.gdb");

    //    reload = reload ? GDBM_NEWDB : GDBM_WRCREAT;
    _gdbm_lock();
    _gdbm_songs = gdbm_open(db_path, 0, GDBM_WRCREAT | GDBM_SYNC | GDBM_NOLOCK,
                           0600,NULL);

    if(!_gdbm_songs) {
        /* let's try creating it! */
        _gdbm_songs = gdbm_open(db_path,0,GDBM_NEWDB | GDBM_SYNC | GDBM_NOLOCK,
                                0600,NULL);
    }

    if(!_gdbm_songs) {
        DPRINTF(E_FATAL,L_DB,"Could not open songs database (%s): %s\n",
                db_path,strerror(errno));
        db_get_error(pe,DB_E_DB_ERROR,gdbm_strerror(gdbm_errno));
        result = DB_E_DB_ERROR;
    }
    _gdbm_unlock();
    return result;
}

/**
 * Don't really have a db initialization separate from opening.
 */
int db_gdbm_init(int reload) {
    return TRUE;
}

/**
 * Close the database
 */
int db_gdbm_deinit(void) {
    _gdbm_lock();
    gdbm_close(_gdbm_songs);
    _gdbm_unlock();
    return TRUE;
}

/**
 * given an id, fetch the associated MP3FILE
 *
 * @param pe error buffer
 * @param id id of item to fetch
 * @returns pointer to MP3FILE.  Must be disposed of with db_dispose
 */
PACKED_MP3FILE *db_gdbm_fetch_item(char **pe, int id) {
    datum key, content;
    int content_size;
    PACKED_MP3FILE *ppmp3;
    char **char_array;
    char *element_ptr;
    int done=0;

    /* pull a row that looks like a sql row out of the table */
    key.dptr = (void*)&id;
    key.dsize = sizeof(unsigned int);

    _gdbm_lock();
    content = gdbm_fetch(_gdbm_songs,key);
    _gdbm_unlock();

    if(content.dptr) {
        /* have a "packed" row.... let's unpack it */
        ppmp3=(PACKED_MP3FILE*)malloc(sizeof(PACKED_MP3FILE));
        if(!ppmp3) {
            DPRINTF(E_FATAL,L_DB,"db_gdbm_fetch_item: malloc\n");
            return NULL;  /* lol */
        }
        memset(ppmp3,0x0,sizeof(PACKED_MP3FILE));
        content_size = content.dsize;
        element_ptr = content.dptr;
        char_array = (char**)(ppmp3);

        /* walk through and set each element */
        while(!done) {

        }


    }

    return NULL;
}
/**
 * start a full scan through the database
 *
 * @parm pe error buffer
 * @returns DB_E_SUCCESS or DB_E* on failure
 */
int db_gdbm_enum_start(char **pe) {
    return DB_E_DB_ERROR;
}

PACKED_MP3FILE *db_gdbm_enum_fetch(char **pe) {
    return NULL;
}

int db_gdbm_enum_end(char **pe) {
    return DB_E_DB_ERROR;
}

/* Required for read-write (fs scanning) support */
int db_gdbm_add(char **pe, MP3FILE *pmp3) {
    return DB_E_DB_ERROR;
}

extern int db_gdbm_delete(char **pe, int id) {
    return DB_E_DB_ERROR;
}

