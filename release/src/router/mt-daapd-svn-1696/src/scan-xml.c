/*
 * $Id: scan-xml.c 1545 2007-04-18 03:34:03Z rpedde $
 * Implementation file iTunes metainfo scanning
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

#include <ctype.h>
#include <limits.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "daapd.h"
#include "db-generic.h"
#include "err.h"
#include "mp3-scanner.h"
#include "os.h"
#include "rxml.h"
#include "redblack.h"
#include "util.h"

/* Forwards */
int scan_xml_playlist(char *filename);
void scan_xml_handler(int action,void* puser,char* info);
int scan_xml_preamble_section(int action,char *info);
int scan_xml_tracks_section(int action,char *info);
int scan_xml_playlists_section(int action,char *info);
void scan_xml_add_lookup(int itunes_index, int mtd_index);

/* Globals */
static char *scan_xml_itunes_version = NULL;
static char *scan_xml_itunes_base_path = NULL;
static char *scan_xml_itunes_decoded_base_path = NULL;
static char *scan_xml_real_base_path = NULL;
static char *scan_xml_file; /** < The actual file we are scanning */
static struct rbtree *scan_xml_db;

#define MAYBECOPY(a) if(mp3.a) pmp3->a = mp3.a
#define MAYBECOPYSTRING(a) if(mp3.a) { if(pmp3->a) free(pmp3->a); pmp3->a = mp3.a; mp3.a=NULL; }
#define MAYBEFREE(a) if((a)) { free((a)); (a)=NULL; }

/** iTunes xml values we are interested in */
static char *scan_xml_track_tags[] = {
    "Name",
    "Artist",
    "Album",
    "Genre",
    "Total Time",
    "Track Number",
    "Track Count",
    "Year",
    "Bit Rate",
    "Sample Rate",
    "Play Count",
    "Rating",
    "Disabled",
    "Disc Number",
    "Disc Count",
    "Compilation",
    "Location",
    "Date Added",
    "Comments",
    "Composer",
    "Album Artist",
    NULL
};

/** Indexes to the iTunes xml fields we are interested in */
#define SCAN_XML_T_UNKNOWN      -1
#define SCAN_XML_T_NAME          0
#define SCAN_XML_T_ARTIST        1
#define SCAN_XML_T_ALBUM         2
#define SCAN_XML_T_GENRE         3
#define SCAN_XML_T_TOTALTIME     4
#define SCAN_XML_T_TRACKNUMBER   5
#define SCAN_XML_T_TRACKCOUNT    6
#define SCAN_XML_T_YEAR          7
#define SCAN_XML_T_BITRATE       8
#define SCAN_XML_T_SAMPLERATE    9
#define SCAN_XML_T_PLAYCOUNT    10
#define SCAN_XML_T_RATING       11
#define SCAN_XML_T_DISABLED     12
#define SCAN_XML_T_DISCNO       13
#define SCAN_XML_T_DISCCOUNT    14
#define SCAN_XML_T_COMPILATION  15
#define SCAN_XML_T_LOCATION     16
#define SCAN_XML_T_DATE_ADDED   17
#define SCAN_XML_T_COMMENTS     18
#define SCAN_XML_T_COMPOSER     19
#define SCAN_XML_T_ALBUM_ARTIST 20

#ifndef TRUE
# define TRUE  1
# define FALSE 0
#endif

typedef struct scan_xml_rb_t {
    int itunes_index;
    int mtd_index;
} SCAN_XML_RB;


/**
 * translate an ISO 8601 iTunes xml to unix epoch format
 *
 * @param string iSO8601 format string to translate
 * @returns time in unix epoch format
 */
int scan_xml_datedecode(char *string) {
    struct tm date_time;
    time_t seconds;

    strptime(string,"%Y-%m-%dT%H:%M:%SZ",&date_time);
    seconds = timegm(&date_time);
    return (int)seconds;
}

/**
 * comparison for the red-black tree.  @see redblack.c
 *
 * @param pa one node to compare
 * @param pb other node to compare
 * @param cfg opaque pointer I'm not using
 */
int scan_xml_rb_compare(const void *pa, const void *pb, const void *cfg) {
    if(((SCAN_XML_RB*)pa)->itunes_index < ((SCAN_XML_RB*)pb)->itunes_index)
        return -1;
    if(((SCAN_XML_RB*)pb)->itunes_index < ((SCAN_XML_RB*)pa)->itunes_index)
        return 1;
    return 0;
}



/**
 * check to see if a file exists
 *
 * @param path path of file to test
 * @returns 1 if file exists, 0 otherwise
 */
int scan_xml_is_file(char *path) {
    struct stat sb;

    if(os_stat(path,&sb))
        return 0;

    if(sb.st_mode & S_IFREG)
        return 1;

    return 0;
}

/**
 * convert an iTunes XML path to a physical path.  Note that
 * the supplied buffer (ppnew) must be PATH_MAX, or badness
 * could occur.
 *
 * @param pold the iTunes XML path
 * @param ppnew where the real physical path goes
 * @returns 1 on success, 0 on failure
 */
int scan_xml_translate_path(char *pold, char *pnew) {
    static int path_found=0;
    static int discard;
    char base_path[PATH_MAX];
    char working_path[PATH_MAX];
    char *current;
    char *pbase;
    char *ptemp;

    if((!pold)||(!strlen(pold)))
        return FALSE;

    current = pold + strlen(pold);

    /* see if it's a valid out-of-root path */
    if(strncmp(pold,"file://localhost/",17)==0) {
        /* it's a local path.. is it a valid one? */
        if(pold[18] == ':') {
            /* windows */
            realpath((char*)&pold[17],working_path);
        } else {
            realpath((char*)&pold[16],working_path);
        }

        if(scan_xml_is_file(working_path)) {
            strcpy(pnew,working_path);
            return TRUE;
        }
        /* not a real file... go ahead and brute force it */
    }

    ptemp = pold;
    while(*ptemp) {
        if(*ptemp == '/')
            *ptemp = PATHSEP;

        ptemp++;
    }
    strcpy(working_path,pold);

    if(!path_found) {
        DPRINTF(E_DBG,L_SCAN,"Translating %s, base %s\n",pold,scan_xml_file);

        /* let's try to find the path by brute force.
         * We'll assume that it is under the xml file somewhere
         */
        while(!path_found && ((current = strrchr(working_path,'/')) || (current = strrchr(working_path,'\\')))) {
            realpath(scan_xml_file,base_path);
            DPRINTF(E_DBG,L_SCAN,"New base: %s\n",base_path);
//            strcpy(base_path,scan_xml_file);
            pbase = strrchr(base_path,'/');
            if(!pbase) pbase = strrchr(base_path,'\\');
            if(!pbase) return FALSE;

            strcpy(pbase,pold + (current-working_path));
            if((base_path[strlen(base_path)-1] == '/') ||
                (base_path[strlen(base_path)-1] == '\\')) {
                base_path[strlen(base_path)-1] = '\0';
            }

            DPRINTF(E_DBG,L_SCAN,"Trying %s\n",base_path);
            if(scan_xml_is_file(base_path)) {
                path_found=1;
                discard = (int)(current - working_path);
                DPRINTF(E_DBG,L_SCAN,"Found it!\n");
            }
            *current='\0';
        }
        if(!current)
            return FALSE;
    }

    strcpy(base_path,scan_xml_file);
    pbase = strrchr(base_path,'/');
    if(!pbase) pbase = strrchr(base_path,'\\');
    if(!pbase) return FALSE;
    strcpy(pbase,pold + discard);

    if((base_path[strlen(base_path)-1] == '/') ||
        (base_path[strlen(base_path)-1] == '\\')) {
        base_path[strlen(base_path)-1] = '\0';
    }

    realpath(base_path,pnew);

    DPRINTF(E_DBG,L_SCAN,"Mapping %s to %s\n",pold,pnew);
    return TRUE;
}

/**
 * add a mapping from iTunes song index to the mt-daapd song
 * index.  This is so we can add resolve mt-daapd song indexes
 * when it comes time to build the iTunes playlist
 *
 * @param itunes_index the index from the itunes xml file
 * @param mtd_index the index from db_fetch_path
 */
void scan_xml_add_lookup(int itunes_index, int mtd_index) {
    SCAN_XML_RB *pnew;
    const void *val;

    pnew=(SCAN_XML_RB*)malloc(sizeof(SCAN_XML_RB));
    if(!pnew)
        DPRINTF(E_FATAL,L_SCAN,"malloc error in scan_xml_add_lookup\n");

    pnew->itunes_index = itunes_index;
    pnew->mtd_index = mtd_index;

    val = rbsearch((const void*)pnew,scan_xml_db);
    if(!val) {
        /* couldn't alloc the rb tree structure -- if we don't
         * die now, we are going to soon enough*/
        DPRINTF(E_FATAL,L_SCAN,"redblack tree insert error\n");
    }
}

/**
 * Find the mt-daapd index that corresponds with a particular
 * itunes song id
 *
 * @param itunes_index index from the iTunes xml file
 * @returns the mt-daapd index
 */
int scan_xml_get_index(int itunes_index, int *mtd_index) {
    SCAN_XML_RB rb;
    SCAN_XML_RB *prb;

    rb.itunes_index = itunes_index;
    prb = (SCAN_XML_RB*) rbfind((void*)&rb,scan_xml_db);
    if(prb) {
        *mtd_index = prb->mtd_index;
        DPRINTF(E_SPAM,L_SCAN,"Matching %d to %d\n",itunes_index,*mtd_index);
        return TRUE;
    }

    return FALSE;
}


/**
 * get the tag index of a particular tag
 *
 * @param tag tag to determine tag index for
 */
int scan_xml_get_tagindex(char *tag) {
    char **ptag = scan_xml_track_tags;
    int index=0;

    while(*ptag && (strcasecmp(tag,*ptag) != 0)) {
        ptag++;
        index++;
    }

    if(*ptag)
        return index;

    return SCAN_XML_T_UNKNOWN;
}

/**
 * urldecode a string, returning a string pointer which must
 * be freed by the calling function or NULL on error (ENOMEM)
 *
 * \param string string to convert
 * \param space as plus whether to convert '+' chars to spaces (no, for iTunes)
 */
char *scan_xml_urldecode(char *string, int space_as_plus) {
    char *pnew;
    char *src,*dst;
    int val=0;

    pnew=(char*)malloc(strlen(string)+1);
    if(!pnew)
        return NULL;

    src=string;
    dst=pnew;

    while(*src) {
        switch(*src) {
        case '+':
            if(space_as_plus) {
                *dst++=' ';
            } else {
                *dst++=*src;
            }
            src++;
            break;
        case '%':
            /* this is hideous */
            src++;
            if(*src) {
                if((*src <= '9') && (*src >='0'))
                    val=(*src - '0');
                else if((tolower(*src) <= 'f')&&(tolower(*src) >= 'a'))
                    val=10+(tolower(*src) - 'a');
                src++;
            }
            if(*src) {
                val *= 16;
                if((*src <= '9') && (*src >='0'))
                    val+=(*src - '0');
                else if((tolower(*src) <= 'f')&&(tolower(*src) >= 'a'))
                    val+=(10+(tolower(*src) - 'a'));
                src++;
            }
            *dst++=val;
            break;
        default:
            *dst++=*src++;
            break;
        }
    }

    *dst='\0';
    return pnew;
}

/**
 * scan an iTunes xml music database file, augmenting
 * the metainfo with that found in the xml file
 *
 * @param filename xml file to parse
 * @returns TRUE if playlist parsed successfully, FALSE otherwise
 */
int scan_xml_playlist(char *filename) {
    char *working_base;
    const void *val;
    int retval=TRUE;
    SCAN_XML_RB *lookup_ptr;
    SCAN_XML_RB lookup_val;

    RXMLHANDLE xml_handle;

    MAYBEFREE(scan_xml_itunes_version);
    MAYBEFREE(scan_xml_itunes_base_path);
    MAYBEFREE(scan_xml_itunes_decoded_base_path);
    MAYBEFREE(scan_xml_real_base_path);

    scan_xml_file = filename;

    /* initialize the redblack tree */
    if((scan_xml_db = rbinit(scan_xml_rb_compare,NULL)) == NULL) {
        DPRINTF(E_LOG,L_SCAN,"Could not initialize red/black tree\n");
        return FALSE;
    }

    /* find the base dir of the itunes playlist itself */
    working_base = strdup(filename);
    if(strrchr(working_base,'/')) {
        *(strrchr(working_base,'/') + 1) = '\x0';
        scan_xml_real_base_path = strdup(working_base);
    } else {
        scan_xml_real_base_path = strdup("/");
    }
    free(working_base);

    DPRINTF(E_SPAM,L_SCAN,"Parsing xml file: %s\n",filename);

    if(!rxml_open(&xml_handle,filename,scan_xml_handler,NULL)) {
        DPRINTF(E_LOG,L_SCAN,"Error opening xml file %s: %s\n",
                filename,rxml_errorstring(xml_handle));
    } else {
        if(!rxml_parse(xml_handle)) {
            retval=FALSE;
            DPRINTF(E_LOG,L_SCAN,"Error parsing xml file %s: %s\n",
                    filename,rxml_errorstring(xml_handle));
        }
    }

    rxml_close(xml_handle);

    /* destroy the redblack tree */
    val = rblookup(RB_LUFIRST,NULL,scan_xml_db);
    while(val) {
        lookup_val.itunes_index = ((SCAN_XML_RB*)val)->itunes_index;
        lookup_ptr = (SCAN_XML_RB *)rbdelete((void*)&lookup_val,scan_xml_db);
        if(lookup_ptr)
            free(lookup_ptr);
        val = rblookup(RB_LUFIRST,NULL,scan_xml_db);
    }

    rbdestroy(scan_xml_db);

    MAYBEFREE(scan_xml_itunes_version);
    MAYBEFREE(scan_xml_itunes_base_path);
    MAYBEFREE(scan_xml_itunes_decoded_base_path);
    MAYBEFREE(scan_xml_real_base_path);

    return retval;
}


#define XML_STATE_PREAMBLE  0
#define XML_STATE_TRACKS    1
#define XML_STATE_PLAYLISTS 2
#define XML_STATE_ERROR     3


/**
 * handle new xml events, and dispatch it to the
 * appropriate handler.  This is a callback from the
 * xml parser.
 *
 * @param action what event (RXML_EVT_OPEN, etc)
 * @param puser opaqe data object passed in open (unused)
 * @param info char data associated with the event
 */
void scan_xml_handler(int action,void* puser,char* info) {
    static int state;

    if(util_must_exit())
        return;

    switch(action) {
    case RXML_EVT_OPEN: /* file opened */
        state = XML_STATE_PREAMBLE;
        /* send this event to all dispatches to allow them
         * to reset
         */
        scan_xml_preamble_section(action,info);
        scan_xml_tracks_section(action,info);
        scan_xml_playlists_section(action,info);
        break;
    case RXML_EVT_BEGIN:
    case RXML_EVT_END:
    case RXML_EVT_TEXT:
        switch(state) {
        case XML_STATE_PREAMBLE:
            state=scan_xml_preamble_section(action,info);
            break;
        case XML_STATE_TRACKS:
            state=scan_xml_tracks_section(action,info);
            break;
        case XML_STATE_PLAYLISTS:
            state=scan_xml_playlists_section(action,info);
            break;
        default:
            break;
        }
    default:
        break;
    }
}

#define SCAN_XML_PRE_NOTHING   0
#define SCAN_XML_PRE_VERSION   1
#define SCAN_XML_PRE_PATH      2
#define SCAN_XML_PRE_TRACKS    3
#define SCAN_XML_PRE_PLAYLISTS 4

/**
 * collect preamble data... version, library id, etc.
 *
 * @param action xml action (RXML_EVT_TEXT, etc)
 * @param info text data associated with event
 */
int scan_xml_preamble_section(int action, char *info) {
    static int expecting_next;
    static int done;

    switch(action) {
    case RXML_EVT_OPEN: /* initialization */
        expecting_next=0;
        done=0;
        break;

    case RXML_EVT_END:
        if(expecting_next == SCAN_XML_PRE_TRACKS) { /* end of tracks tag */
            expecting_next=0;
            DPRINTF(E_DBG,L_SCAN,"Scanning tracks\n");
            return XML_STATE_TRACKS;
        }
        if(expecting_next == SCAN_XML_PRE_PLAYLISTS) {
            expecting_next=0;
            DPRINTF(E_DBG,L_SCAN,"Scanning playlists\n");
            return XML_STATE_PLAYLISTS;
        }
        break;

    case RXML_EVT_TEXT: /* scan for the tags we expect */
        if(!expecting_next) {
            if(strcmp(info,"Application Version") == 0) {
                expecting_next = SCAN_XML_PRE_VERSION;
            } else if (strcmp(info,"Music Folder") == 0) {
                expecting_next = SCAN_XML_PRE_PATH;
            } else if (strcmp(info,"Tracks") == 0) {
                expecting_next = SCAN_XML_PRE_TRACKS;
            } else if (strcmp(info,"Playlists") == 0) {
                expecting_next = SCAN_XML_PRE_PLAYLISTS;
            }
        } else {
            /* we were expecting someting! */
            switch(expecting_next) {
            case SCAN_XML_PRE_VERSION:
                if(!scan_xml_itunes_version) {
                    scan_xml_itunes_version=strdup(info);
                    DPRINTF(E_DBG,L_SCAN,"iTunes Version: %s\n",info);
                }
                break;
            case SCAN_XML_PRE_PATH:
                if(!scan_xml_itunes_base_path) {
                    scan_xml_itunes_base_path=strdup(info);
                    scan_xml_itunes_decoded_base_path=scan_xml_urldecode(info,0);
                    DPRINTF(E_DBG,L_SCAN,"iTunes base path: %s\n",info);
                }
                break;
            default:
                break;
            }
            expecting_next=0;
        }
        break; /* RXML_EVT_TEXT */

    default:
        break;
    }

    return XML_STATE_PREAMBLE;
}


#define XML_TRACK_ST_INITIAL               0
#define XML_TRACK_ST_MAIN_DICT             1
#define XML_TRACK_ST_EXPECTING_TRACK_ID    2
#define XML_TRACK_ST_EXPECTING_TRACK_DICT  3
#define XML_TRACK_ST_TRACK_INFO            4
#define XML_TRACK_ST_TRACK_DATA            5

/**
 * collect track data for each track in the itunes playlist
 *
 * @param action xml action (RXML_EVT_TEXT, etc)
 * @param info text data associated with event
 */
#define MAYBESETSTATE_TR(a,b,c) { if((action==(a)) && \
                                  (strcmp(info,(b)) == 0)) { \
                                   state = (c); \
                                   return XML_STATE_TRACKS; \
                             }}

int scan_xml_tracks_section(int action, char *info) {
    static int state;
    static int current_track_id;
    static int current_field;
    static int is_streaming;
    static MP3FILE mp3;
    static char *song_path=NULL;
    char real_path[PATH_MAX];
    MP3FILE *pmp3;
    int added_id;

    if(action == RXML_EVT_OPEN) {
        state = XML_TRACK_ST_INITIAL;
        memset((void*)&mp3,0,sizeof(MP3FILE));
        song_path = NULL;
        return 0;
    }

    /* walk through the states */
    switch(state) {
    case XML_TRACK_ST_INITIAL:
        /* expection only a <dict> */
        MAYBESETSTATE_TR(RXML_EVT_BEGIN,"dict",XML_TRACK_ST_MAIN_DICT);
        return XML_STATE_ERROR;
        break;

    case XML_TRACK_ST_MAIN_DICT:
        /* either get a <key>, or a </dict> */
        MAYBESETSTATE_TR(RXML_EVT_BEGIN,"key",XML_TRACK_ST_EXPECTING_TRACK_ID);
        if ((action == RXML_EVT_END) && (strcasecmp(info,"dict") == 0)) {
            return XML_STATE_PREAMBLE;
        }
        return XML_STATE_ERROR;
        break;

    case XML_TRACK_ST_EXPECTING_TRACK_ID:
        /* this is somewhat loose  - <key>id</key> */
        MAYBESETSTATE_TR(RXML_EVT_BEGIN,"key",XML_TRACK_ST_EXPECTING_TRACK_ID);
        MAYBESETSTATE_TR(RXML_EVT_END,"key",XML_TRACK_ST_EXPECTING_TRACK_DICT);
        if (action == RXML_EVT_TEXT) {
            current_track_id = atoi(info);
            DPRINTF(E_DBG,L_SCAN,"Scanning iTunes id #%d\n",current_track_id);
        } else {
            return XML_STATE_ERROR;
        }
        break;

    case XML_TRACK_ST_EXPECTING_TRACK_DICT:
        /* waiting for a dict */
        MAYBESETSTATE_TR(RXML_EVT_BEGIN,"dict",XML_TRACK_ST_TRACK_INFO);
        return XML_STATE_ERROR;
        break;

    case XML_TRACK_ST_TRACK_INFO:
        /* again, kind of loose */
        MAYBESETSTATE_TR(RXML_EVT_BEGIN,"key",XML_TRACK_ST_TRACK_INFO);
        MAYBESETSTATE_TR(RXML_EVT_END,"key",XML_TRACK_ST_TRACK_DATA);
        if(action == RXML_EVT_TEXT) {
            current_field=scan_xml_get_tagindex(info);
            if(current_field == SCAN_XML_T_DISABLED) {
                mp3.disabled = 1;
            } else if(current_field == SCAN_XML_T_COMPILATION) {
                mp3.compilation = 1;
            }
        } else if((action == RXML_EVT_END) && (strcmp(info,"dict")==0)) {
            state = XML_TRACK_ST_MAIN_DICT;
            /* but more importantly, we gotta process the track */
            is_streaming = 0;
            if((song_path) && strncasecmp(song_path,"http://",7) == 0)
                is_streaming = 1;

            if((!is_streaming)&&scan_xml_translate_path(song_path,real_path)) {
                /* FIXME: Error handling */
                pmp3=db_fetch_path(NULL,real_path,0);
                if(!pmp3) {
                    /* file doesn't exist... let's add it? */
                    scan_filename(real_path,SCAN_TEST_COMPDIR,NULL);
                    pmp3=db_fetch_path(NULL,real_path,0);
                }
                if(pmp3) {
                    /* Update the existing record with the
                     * updated stuff we got from the iTunes xml file
                     */
                    MAYBECOPYSTRING(title);
                    MAYBECOPYSTRING(artist);
                    MAYBECOPYSTRING(album);
                    MAYBECOPYSTRING(genre);
                    MAYBECOPYSTRING(comment);
                    MAYBECOPYSTRING(composer);
                    MAYBECOPY(song_length);
                    MAYBECOPY(track);
                    MAYBECOPY(total_tracks);
                    MAYBECOPY(year);
                    MAYBECOPY(bitrate);
                    MAYBECOPY(samplerate);
                    MAYBECOPY(play_count);
                    MAYBECOPY(rating);
                    MAYBECOPY(disc);
                    MAYBECOPY(total_discs);
                    MAYBECOPY(time_added);
                    MAYBECOPY(disabled);
                    MAYBECOPYSTRING(album_artist);

                    /* must add to the red-black tree */
                    scan_xml_add_lookup(current_track_id,pmp3->id);

                    make_composite_tags(pmp3);
                    db_add(NULL,pmp3,NULL);
                    db_dispose_item(pmp3);
                }
            } else if(is_streaming) {
                /* add/update a http:// url */
                pmp3=db_fetch_path(NULL,scan_xml_file,current_track_id);
                if(!pmp3) {
                    /* gotta add it! */
                    DPRINTF(E_DBG,L_SCAN,"Adding %s\n",song_path);
                    pmp3 = calloc(sizeof(MP3FILE),1);

                    if(!pmp3)
                        DPRINTF(E_FATAL,L_SCAN,
                                "malloc: scan_xml_tracks_section\n");
                } else {
                    DPRINTF(E_DBG,L_SCAN,"updating %s\n",song_path);
                }
                pmp3->url = strdup(song_path);
                pmp3->type = strdup("pls");
                pmp3->description = strdup("Playlist URL");
                pmp3->data_kind = 1;
                pmp3->item_kind = 2;

                pmp3->path = strdup(scan_xml_file);
                pmp3->index = current_track_id;

                MAYBECOPYSTRING(title);
                MAYBECOPYSTRING(artist);
                MAYBECOPYSTRING(album);
                MAYBECOPYSTRING(genre);
                MAYBECOPYSTRING(comment);
                MAYBECOPY(bitrate);
                MAYBECOPY(samplerate);
                MAYBECOPY(play_count);
                MAYBECOPY(rating);
                MAYBECOPY(time_added);
                MAYBECOPY(disabled);
                MAYBECOPYSTRING(album_artist);

                make_composite_tags(pmp3);
                if(db_add(NULL,pmp3,&added_id) == DB_E_SUCCESS) {
                    scan_xml_add_lookup(current_track_id,added_id);
                    DPRINTF(E_DBG,L_SCAN,"Added %s\n",song_path);
                } else {
                    DPRINTF(E_DBG,L_SCAN,"Error adding %s\n",song_path);
                }

                db_dispose_item(pmp3);
            }

            /* cleanup what's left */
            MAYBEFREE(mp3.title);
            MAYBEFREE(mp3.artist);
            MAYBEFREE(mp3.album);
            MAYBEFREE(mp3.genre);
            MAYBEFREE(mp3.comment);
            MAYBEFREE(mp3.album_artist);
            MAYBEFREE(song_path);

            memset((void*)&mp3,0,sizeof(MP3FILE));
        } else {
            return XML_STATE_ERROR;
        }
        break;

    case XML_TRACK_ST_TRACK_DATA:
        if(action == RXML_EVT_BEGIN) {
            break;
        } else if(action == RXML_EVT_TEXT) {
            if(current_field == SCAN_XML_T_NAME) {
                mp3.title = strdup(info);
            } else if(current_field == SCAN_XML_T_ARTIST) {
                mp3.artist = strdup(info);
            } else if(current_field == SCAN_XML_T_ALBUM) {
                mp3.album = strdup(info);
            } else if(current_field == SCAN_XML_T_GENRE) {
                mp3.genre = strdup(info);
            } else if(current_field == SCAN_XML_T_TOTALTIME) {
                mp3.song_length = atoi(info);
            } else if(current_field == SCAN_XML_T_TRACKNUMBER) {
                mp3.track = atoi(info);
            } else if(current_field == SCAN_XML_T_TRACKCOUNT) {
                mp3.total_tracks = atoi(info);
            } else if(current_field == SCAN_XML_T_YEAR) {
                mp3.year = atoi(info);
            } else if(current_field == SCAN_XML_T_BITRATE) {
                mp3.bitrate = atoi(info);
            } else if(current_field == SCAN_XML_T_SAMPLERATE) {
                mp3.samplerate = atoi(info);
            } else if(current_field == SCAN_XML_T_PLAYCOUNT) {
                mp3.play_count = atoi(info);
            } else if(current_field == SCAN_XML_T_RATING) {
                mp3.rating = atoi(info);
            } else if(current_field == SCAN_XML_T_DISCNO) {
                mp3.disc = atoi(info);
            } else if(current_field == SCAN_XML_T_DISCCOUNT) {
                mp3.total_discs = atoi(info);
            } else if(current_field == SCAN_XML_T_LOCATION) {
                song_path = scan_xml_urldecode(info,0);
                DPRINTF(E_DBG,L_SCAN,"scan_path: %s\n",song_path);
            } else if(current_field == SCAN_XML_T_DATE_ADDED) {
                mp3.time_added = scan_xml_datedecode(info);
            } else if(current_field == SCAN_XML_T_COMMENTS) {
                mp3.comment = strdup(info);
            } else if(current_field == SCAN_XML_T_COMPOSER) {
                mp3.composer = strdup(info);
            } else if(current_field == SCAN_XML_T_ALBUM_ARTIST) {
                mp3.album_artist = strdup(info);
            }
        } else if(action == RXML_EVT_END) {
            state = XML_TRACK_ST_TRACK_INFO;
        } else {
            return XML_STATE_ERROR;
        }
        break;
    default:
        return XML_STATE_ERROR;
    }

    return XML_STATE_TRACKS;
}


#define XML_PL_ST_INITIAL                0
#define XML_PL_ST_EXPECTING_PL           1
#define XML_PL_ST_EXPECTING_PL_DATA      2
#define XML_PL_ST_EXPECTING_PL_VALUE     3
#define XML_PL_ST_EXPECTING_PL_TRACKLIST 4

#define XML_PL_NEXT_VALUE_NONE          0
#define XML_PL_NEXT_VALUE_NAME          1
#define XML_PL_NEXT_VALUE_ID            2

#define MAYBESETSTATE_PL(a,b,c) { if((action==(a)) && \
                                  (strcmp(info,(b)) == 0)) { \
                                   state = (c); \
                                   return XML_STATE_PLAYLISTS; \
                             }}

/**
 * collect playlist data for each playlist in the itunes xml file
 * this again is implemented as a sloppy state machine, and assumes
 * that the playlist items are after all the playlist metainfo.
 *
 * @param action xml action (RXML_EVT_TEXT, etc)
 * @param info text data associated with event
 */
int scan_xml_playlists_section(int action, char *info) {
    static int state = XML_PL_ST_INITIAL;
    static int next_value=0;         /** < what's next song info id or name */
    static int native_plid=0;        /** < the iTunes playlist id */
    static int current_id=0;         /** < the mt-daapd playlist id */
    static char *current_name=NULL;  /** < the iTunes playlist name */
    static int dont_scan=0;          /** < playlist we don't want */
    int native_track_id;             /** < the iTunes id of the track */
    int track_id;                    /** < the mt-daapd track id */

    M3UFILE *pm3u;

    /* do initialization */
    if(action == RXML_EVT_OPEN) {
        state = XML_PL_ST_INITIAL;
        if(current_name)
            free(current_name);
        current_name = NULL;
        dont_scan=0;
        return 0;
    }

    switch(state) {
    case XML_PL_ST_INITIAL:
        /* expecting <array> or error */
        MAYBESETSTATE_PL(RXML_EVT_BEGIN,"array",XML_PL_ST_EXPECTING_PL);
        return XML_STATE_ERROR;
    case XML_PL_ST_EXPECTING_PL:
        /* either a new playlist, or end of playlist list */
        dont_scan=0;
        MAYBESETSTATE_PL(RXML_EVT_BEGIN,"dict",XML_PL_ST_EXPECTING_PL_DATA);
        if((action == RXML_EVT_END) && (strcasecmp(info,"array") == 0))
            return XML_STATE_PREAMBLE;
        return XML_STATE_ERROR;
    case XML_PL_ST_EXPECTING_PL_DATA:
        /* either a key/data pair, or an array, signaling start of playlist
         * or the end of the dict (end of playlist data) */
        MAYBESETSTATE_PL(RXML_EVT_BEGIN,"key",XML_PL_ST_EXPECTING_PL_DATA);
        MAYBESETSTATE_PL(RXML_EVT_END,"key",XML_PL_ST_EXPECTING_PL_VALUE);
        MAYBESETSTATE_PL(RXML_EVT_END,"dict",XML_PL_ST_EXPECTING_PL);
        if(action == RXML_EVT_TEXT) {
            next_value=XML_PL_NEXT_VALUE_NONE;
            if(strcasecmp(info,"Name") == 0) {
                next_value = XML_PL_NEXT_VALUE_NAME;
            } else if(strcasecmp(info,"Playlist ID") == 0) {
                next_value = XML_PL_NEXT_VALUE_ID;
            } else if(strcasecmp(info,"Master") == 0) {
                /* No point adding the master library... we have one */
                dont_scan=1;
            }
            return XML_STATE_PLAYLISTS;
        }
        return XML_STATE_ERROR;
    case XML_PL_ST_EXPECTING_PL_VALUE:
        /* any tag, value we are looking for, any close tag */
        if((action == RXML_EVT_BEGIN) && (strcasecmp(info,"array") == 0)) {
            /* we are about to get track list... must register the playlist */
            current_id=0;
            if(dont_scan == 0) {
                DPRINTF(E_DBG,L_SCAN,"Creating playlist for %s\n",current_name);
                /* we won't actually use the iTunes pl_id, as it seems
                 *  to change for no good reason.  We'll hash the name,
                 * instead. */

                /* delete the old one first */
                /* FIXME: Error handling */
                DPRINTF(E_DBG,L_SCAN,"Converting native plid (%d) to %d\n",
                        native_plid, util_djb_hash_str(current_name));
                native_plid = util_djb_hash_str(current_name);

                pm3u = db_fetch_playlist(NULL,scan_xml_file,native_plid);
                if(pm3u) {
                    db_delete_playlist(NULL,pm3u->id);
                    db_dispose_playlist(pm3u);
                }
                if(db_add_playlist(NULL,current_name,PL_STATICXML,NULL,
                                   scan_xml_file,native_plid,
                                   &current_id) != DB_E_SUCCESS)
                {
                    DPRINTF(E_LOG,L_SCAN,"err adding playlist %s\n",current_name);
                    current_id=0;
                }
            }
            dont_scan=0;
            state=XML_PL_ST_EXPECTING_PL_TRACKLIST;
            MAYBEFREE(current_name);
            return XML_STATE_PLAYLISTS;
        }
        if(action == RXML_EVT_BEGIN)
            return XML_STATE_PLAYLISTS;
        if(action == RXML_EVT_END) {
            state = XML_PL_ST_EXPECTING_PL_DATA;
            return XML_STATE_PLAYLISTS;
        }
        if(action == RXML_EVT_TEXT) {
            /* got the value we were hoping for */
            if(next_value == XML_PL_NEXT_VALUE_NAME) {
                MAYBEFREE(current_name);
                current_name = strdup(info);
                DPRINTF(E_DBG,L_SCAN,"Found playlist: %s\n",current_name);
                /* disallow specific playlists */
                if(strcasecmp(current_name,"Party Shuffle") == 0) {
                    dont_scan=1;
                }
            } else if(next_value == XML_PL_NEXT_VALUE_ID) {
                native_plid = atoi(info);
            }
            return XML_STATE_PLAYLISTS;
        }
        return XML_STATE_ERROR;

    case XML_PL_ST_EXPECTING_PL_TRACKLIST:
        if((strcasecmp(info,"dict") == 0) || (strcasecmp(info,"key") == 0))
            return XML_STATE_PLAYLISTS;
        MAYBESETSTATE_PL(RXML_EVT_END,"array",XML_PL_ST_EXPECTING_PL_DATA);
        if(action == RXML_EVT_TEXT) {
            if(strcasecmp(info,"Track ID") != 0) {
                native_track_id = atoi(info);
                DPRINTF(E_DBG,L_SCAN,"Adding itunes track #%s\n",info);
                /* add it to the current playlist (current_id) */
                if(current_id && scan_xml_get_index(native_track_id, &track_id)) {
                    /* FIXME: Error handling */
                    db_add_playlist_item(NULL,current_id,track_id);
                }
            }

            return XML_STATE_PLAYLISTS;
        }
        return XML_STATE_PLAYLISTS;

    default:
        return XML_STATE_ERROR;
    }

    return XML_STATE_PLAYLISTS;
}

