/*
 * $Id: db-memory.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Implementation for simple in-memory linked list db
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "err.h"
#include "mp3-scanner.h"

/*
 * Typedefs
 */
typedef struct tag_mp3record {
    MP3FILE mp3file;
    struct tag_mp3record *next;
} MP3RECORD;

typedef struct tag_playlistentry {
    unsigned int id;
    struct tag_playlistentry *next;
} DB_PLAYLISTENTRY;

typedef struct tag_playlist {
    unsigned int id;
    int songs;
    char *name;
    int is_smart;
    struct tag_playlistentry *nodes;
    struct tag_playlist *next;
} DB_PLAYLIST;

#define MAYBEFREE(a) { if((a)) free((a)); };

/*
 * Globals 
 */
MP3RECORD db_root;
DB_PLAYLIST db_playlists;
int db_version_no;
int db_update_mode=0;
int db_song_count;
int db_playlist_count=0;
pthread_rwlock_t db_rwlock; /* OSX doesn't have PTHREAD_RWLOCK_INITIALIZER */
pthread_once_t db_initlock=PTHREAD_ONCE_INIT;
/*
 * Forwards
 */

int db_start_initial_update(void);
int db_end_initial_update(void);
int db_is_empty(void);
int db_open(char *parameters);
int db_init();
int db_deinit(void);
int db_version(void);
int db_add(MP3FILE *mp3file);
int db_add_playlist(unsigned int playlistid, char *name, int is_smart);
int db_add_playlist_song(unsigned int playlistid, unsigned int itemid);

MP3RECORD *db_enum_begin(void);
MP3FILE *db_enum(MP3RECORD **current);
int db_enum_end(void);

DB_PLAYLIST *db_playlist_enum_begin(void);
int db_playlist_enum(DB_PLAYLIST **current);
int db_playlist_enum_end(void);
int db_scanning(void);

DB_PLAYLISTENTRY *db_playlist_items_enum_begin(int playlistid);
int db_playlist_items_enum(DB_PLAYLISTENTRY **current);
int db_playlist_items_enum_end(void);

int db_get_song_count(void);
int db_get_playlist_count(void);
int db_get_playlist_is_smart(int playlistid);
int db_get_playlist_entry_count(int playlistid);
char *db_get_playlist_name(int playlistid);

MP3FILE *db_find(int id);

void db_freerecord(MP3RECORD *mp3record);

/*
 * db_init_once
 *
 * Must dynamically initialize the rwlock, as Mac OSX 10.3 (at least)
 * doesn't have a static initializer for rwlocks
 */
void db_init_once(void) {
    pthread_rwlock_init(&db_rwlock,NULL);
}


/*
 * db_open
 *
 * We'll wait for db_init
 */
int db_open(char *parameters) {
    return 0;
}

/*
 * db_init
 *
 * Initialize the database.  For the in-memory db
 * the parameters are insignificant
 */
int db_init(void) {
    db_root.next=NULL;
    db_version_no=1;
    db_song_count=0;
    db_playlists.next=NULL;

    if(pthread_once(&db_initlock,db_init_once))
	return -1;
    
    pl_register();

    return 0;
}

/*
 * db_deinit
 *
 * Close the db, in this case freeing memory
 */
int db_deinit(void) {
    MP3RECORD *current;
    DB_PLAYLIST *plist;
    DB_PLAYLISTENTRY *pentry;

    while(db_root.next) {
	current=db_root.next;
	db_root.next=current->next;
	db_freerecord(current);
    }

    while(db_playlists.next) {
	plist=db_playlists.next;
	db_playlists.next=plist->next;
	free(plist->name);
	/* free all the nodes */
	while(plist->nodes) {
	    pentry=plist->nodes;
	    plist->nodes = pentry->next;
	    free(pentry);
	}
	free(plist);
    }

    return 0;
}

/*
 * db_version
 *
 * return the db version
 */
int db_version(void) {
    return db_version_no;
}

/*
 * db_scanning
 *
 * is the db doing a background scan?
 */
int db_scanning(void) {
    return 0;
}

/*
 * db_start_initial_update
 *
 * Set the db to bulk import mode
 */
int db_start_initial_update(void) {
    db_update_mode=1;
    return 0;
}

/*
 * db_end_initial_update
 *
 * Take the db out of bulk import mode
 */
int db_end_initial_update(void) {
    db_update_mode=0;
    return 0;
}

/*
 * db_is_empty
 *
 * See if the db is empty or not -- that is, should
 * the scanner start up in bulk update mode or in
 * background update mode
 */
int db_is_empty(void) {
    return !db_root.next;
}


/*
 * db_add_playlist
 *
 * Add a new playlist
 */
int db_add_playlist(unsigned int playlistid, char *name, int is_smart) {
    int err;
    DB_PLAYLIST *pnew;
    
    pnew=(DB_PLAYLIST*)malloc(sizeof(DB_PLAYLIST));
    if(!pnew)
	return -1;

    pnew->name=strdup(name);
    pnew->id=playlistid;
    pnew->is_smart=is_smart;
    pnew->nodes=NULL;
    pnew->songs=0;

    if(!pnew->name) {
	free(pnew);
	return -1;
    }

    DPRINTF(E_DBG,L_DB,"Adding new playlist %s\n",name);

    if((err=pthread_rwlock_wrlock(&db_rwlock))) {
	DPRINTF(E_WARN,L_DB,"cannot lock wrlock in db_add\n");
	free(pnew->name);
	free(pnew);
	errno=err;
	return -1;
    }

    /* we'll update playlist count when we add a song! */
    //    db_playlist_count++;
    pnew->next=db_playlists.next;
    db_playlists.next=pnew;

    if(!db_update_mode) {
	db_version_no++;
    }

    pthread_rwlock_unlock(&db_rwlock);
    DPRINTF(E_DBG,L_DB,"Added playlist\n");
    return 0;
}

/*
 * db_add_playlist_song
 *
 * Add a song to a particular playlist
 */
int db_add_playlist_song(unsigned int playlistid, unsigned int itemid) {
    DB_PLAYLIST *current;
    DB_PLAYLISTENTRY *pnew;
    int err;
    
    pnew=(DB_PLAYLISTENTRY*)malloc(sizeof(DB_PLAYLISTENTRY));
    if(!pnew)
	return -1;

    pnew->id=itemid;
    pnew->next=NULL;

    DPRINTF(E_DBG,L_DB,"Adding new playlist item\n"); 

    if((err=pthread_rwlock_wrlock(&db_rwlock))) {
	DPRINTF(E_WARN,L_DB,"cannot lock wrlock in db_add\n");
	free(pnew);
	errno=err;
	return -1;
    }

    current=db_playlists.next;
    while(current && (current->id != playlistid))
	current=current->next;

    if(!current) {
	DPRINTF(E_WARN,L_DB,"Could not find playlist attempting to add to\n");
	pthread_rwlock_unlock(&db_rwlock);
	free(pnew);
	return -1;
    }

    if(!current->songs)
	db_playlist_count++;

    current->songs++;
    pnew->next = current->nodes;
    current->nodes = pnew;

    if(!db_update_mode) {
	db_version_no++;
    }

    pthread_rwlock_unlock(&db_rwlock);
    DPRINTF(E_DBG,L_DB,"Added playlist item\n");
    return 0;
}

/*
 * db_add
 *
 * add an MP3 file to the database.
 */

int db_add(MP3FILE *mp3file) {
    int err;
    int g;
    MP3RECORD *pnew;

    DPRINTF(E_DBG,L_DB,"Adding %s\n",mp3file->path);

    if((pnew=(MP3RECORD*)malloc(sizeof(MP3RECORD))) == NULL) {
	free(pnew);
	errno=ENOMEM;
	return -1;
    }

    memset(pnew,0,sizeof(MP3RECORD));

    memcpy(&pnew->mp3file,mp3file,sizeof(MP3FILE));

    g=(int) pnew->mp3file.path=strdup(mp3file->path);
    g = g && (pnew->mp3file.fname=strdup(mp3file->fname));

    if(mp3file->title)
	g = g && (pnew->mp3file.title=strdup(mp3file->title));
    
    if(mp3file->artist)
	g = g && (pnew->mp3file.artist=strdup(mp3file->artist));

    if(mp3file->album)
	g = g && (pnew->mp3file.album=strdup(mp3file->album));

    if(mp3file->genre)
	g = g && (pnew->mp3file.genre=strdup(mp3file->genre));

    if(mp3file->comment)
	g = g && (pnew->mp3file.comment=strdup(mp3file->comment));

    if(mp3file->type)
	g = g && (pnew->mp3file.type=strdup(mp3file->type));

    if(mp3file->composer)
	g = g && (pnew->mp3file.composer=strdup(mp3file->composer));

    if(mp3file->orchestra)
	g = g && (pnew->mp3file.orchestra=strdup(mp3file->orchestra));

    if(mp3file->conductor)
	g = g && (pnew->mp3file.conductor=strdup(mp3file->conductor));

    if(mp3file->grouping)
	g = g && (pnew->mp3file.grouping=strdup(mp3file->grouping));

    if(!g) {
	DPRINTF(E_WARN,L_DB,"Malloc error in db_add\n");
	db_freerecord(pnew);
	errno=ENOMEM;
	return -1;
    }

    if((err=pthread_rwlock_wrlock(&db_rwlock))) {
	DPRINTF(E_WARN,L_DB,"cannot lock wrlock in db_add\n");
	db_freerecord(pnew);
	errno=err;
	return -1;
    }

    make_composite_tags(&pnew->mp3file);

    pnew->next=db_root.next;
    db_root.next=pnew;

    if(!db_update_mode) {
	db_version_no++;
    }

    db_song_count++;
    
    pthread_rwlock_unlock(&db_rwlock);
    DPRINTF(E_DBG,L_DB,"Added file\n");
    return 0;
}

/*
 * db_freerecord
 *
 * free a complete mp3record
 */
void db_freerecord(MP3RECORD *mp3record) {
    MAYBEFREE(mp3record->mp3file.path);
    MAYBEFREE(mp3record->mp3file.fname);
    MAYBEFREE(mp3record->mp3file.title);
    MAYBEFREE(mp3record->mp3file.artist);
    MAYBEFREE(mp3record->mp3file.album);
    MAYBEFREE(mp3record->mp3file.genre);
    MAYBEFREE(mp3record->mp3file.comment);
    MAYBEFREE(mp3record->mp3file.type);
    MAYBEFREE(mp3record->mp3file.composer);
    MAYBEFREE(mp3record->mp3file.orchestra);
    MAYBEFREE(mp3record->mp3file.conductor);
    MAYBEFREE(mp3record->mp3file.grouping);
    MAYBEFREE(mp3record->mp3file.description);
    free(mp3record);
}

/*
 * db_enum_begin
 *
 * Begin to walk through an enum of 
 * the database.
 *
 * this should be done quickly, as we'll be holding
 * a reader lock on the db
 */
MP3RECORD *db_enum_begin(void) {
    int err;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return NULL;
    }

    return db_root.next;
}

/*
 * db_playlist_enum_begin
 *
 * Start enumerating playlists
 */
DB_PLAYLIST *db_playlist_enum_begin(void) {
    int err;
    DB_PLAYLIST *current;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return NULL;
    }

    /* find first playlist with a song in it! */
    current=db_playlists.next;
    while(current && (!current->songs))
	current=current->next;

    return current;
}

/*
 * db_playlist_items_enum_begin
 *
 * Start enumerating playlist items
 */
DB_PLAYLISTENTRY *db_playlist_items_enum_begin(int playlistid) {
    DB_PLAYLIST *current;
    int err;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return NULL;
    }

    current=db_playlists.next;
    while(current && (current->id != playlistid))
	current=current->next;
    
    if(!current)
	return NULL;

    return current->nodes;
}


/*
 * db_enum
 *
 * Walk to the next entry
 */
MP3FILE *db_enum(MP3RECORD **current) {
    MP3FILE *retval;

    if(*current) {
	retval=&((*current)->mp3file);
	*current=(*current)->next;
	return retval;
    }
    return NULL;
}

/*
 * db_playlist_enum
 *
 * walk to the next entry
 */
int db_playlist_enum(DB_PLAYLIST **current) {
    int retval;
    DB_PLAYLIST *p;

    if(*current) {
	retval = (*current)->id;
	p=*current;
	p=p->next;
	while(p && (!p->songs))
	    p=p->next;

	*current=p;
	return retval;
    }
    return -1;
}

/*
 * db_playlist_items_enum
 *
 * walk to the next entry
 */
int db_playlist_items_enum(DB_PLAYLISTENTRY **current) {
    int retval;

    if(*current) {
	retval = (*current)->id;
	*current=(*current)->next;
	return retval;
    }

    return -1;
}

/*
 * db_enum_end
 *
 * quit walking the database (and give up reader lock)
 */
int db_enum_end(void) {
    return pthread_rwlock_unlock(&db_rwlock);
}

/*
 * db_playlist_enum_end
 *
 * quit walking the database
 */
int db_playlist_enum_end(void) {
    return pthread_rwlock_unlock(&db_rwlock);
}

/*
 * db_playlist_items_enum_end
 *
 * Quit walking the database
 */
int db_playlist_items_enum_end(void) {
    return pthread_rwlock_unlock(&db_rwlock);
}

/*
 * db_find
 *
 * Find a MP3FILE entry based on file id
 */
MP3FILE *db_find(int id) {
    MP3RECORD *current=db_root.next;
    while((current) && (current->mp3file.id != id)) {
	current=current->next;
    }

    if(!current)
	return NULL;

    return &current->mp3file;
}

/*
 * db_get_playlist_count
 *
 * return the number of playlists
 */
int db_get_playlist_count(void) {
    return db_playlist_count;
}

/*
 * db_get_song_count
 *
 * return the number of songs in the database.  Used for the /database
 * request
 */
int db_get_song_count(void) {
    return db_song_count;
}

/*
 * db_get_playlist_is_smart
 *
 * return whether or not the playlist is a "smart" playlist
 */
int db_get_playlist_is_smart(int playlistid) {
    DB_PLAYLIST *current;
    int err;
    int result;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return -1;	
    }

    current=db_playlists.next;
    while(current && (current->id != playlistid))
	current=current->next;

    if(!current) {
	result=0;
    } else {
	result=current->is_smart;
    }

    pthread_rwlock_unlock(&db_rwlock);
    return result;
}


/*
 * db_get_playlist_entry_count
 *
 * return the number of songs in a particular playlist
 */
int db_get_playlist_entry_count(int playlistid) {
    int count;
    DB_PLAYLIST *current;
    int err;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return -1;	
    }

    current=db_playlists.next;
    while(current && (current->id != playlistid))
	current=current->next;

    if(!current) {
	count = -1;
    } else {
	count = current->songs;
    }

    pthread_rwlock_unlock(&db_rwlock);
    return count;
}

/*
 * db_get_playlist_name
 *
 * return the name of a playlist
 *
 * FIXME: Small race here
 */
char *db_get_playlist_name(int playlistid) {
    char *name;
    DB_PLAYLIST *current;
    int err;

    if((err=pthread_rwlock_rdlock(&db_rwlock))) {
	DPRINTF(E_FATAL,L_DB,"Cannot lock rwlock\n");
	errno=err;
	return NULL;
    }

    current=db_playlists.next;
    while(current && (current->id != playlistid))
	current=current->next;

    if(!current) {
	name = NULL;
    } else {
	name = current->name;
    }

    pthread_rwlock_unlock(&db_rwlock);
    return name;
}


/*
 * db_exists
 *
 * Check if a particular id is in the database
 */
int db_exists(int id) {
    MP3FILE *pmp3;

    pmp3=db_find(id);

    return pmp3 ? 1 : 0;
}

/*
 * db_last_modified
 *
 * See when the file was modified (according to the database)
 *
 * This is merely a stub for the in-memory db
 */
int db_last_modified(int id) {
    return 0;
}


