/*
 * $Id: mp3-scanner.c 1670 2007-09-25 06:15:47Z rpedde $
 * Implementation file for mp3 scanner and monitor
 *
 * Ironically, this now scans file types other than mp3 files,
 * but the name is the same for historical purposes, not to mention
 * the fact that sf.net makes it virtually impossible to manage a cvs
 * root reasonably.  Perhaps one day soon they will move to subversion.
 *
 * /me crosses his fingers
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
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <id3tag.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#ifndef WIN32
# include <netinet/in.h>  /* htons and friends */
#endif
#include <sys/stat.h>

#include "daapd.h"
#include "conf.h"
#include "db-generic.h"
#include "err.h"
#include "io.h"
#include "mp3-scanner.h"
#include "os.h"
#include "restart.h"
#include "util.h"

/*
 * Typedefs
 */
typedef struct {
    char *suffix;
    int (*scanner)(char* file, MP3FILE* pmp3);
    char *type;         /* daap.songformat */
    char *codectype;    /* song.codectype */
    int has_video;      /* hack hack hack */
    char *description;  /* daap.songdescription */
} TAGHANDLER;


#define MAYBEFREE(a) { if((a)) free((a)); };
#ifndef S_ISDIR
# define S_ISDIR(a) (((a) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISLNK
# define S_ISLNK(a) (((a) & S_IFMT) == S_IFLNK)
#endif


/*
 * Forwards
 */
static int scan_path(char *path);
static int scan_get_info(char *file, MP3FILE *pmp3);
static int scan_freetags(MP3FILE *pmp3);
static void scan_music_file(char *path, char *fname,struct stat *psb, int is_compdir);

static TAGHANDLER *scan_gethandler(char *type);


/* EXTERNAL SCANNERS */

#ifdef OGGVORBIS
extern int scan_get_ogginfo(char *filename, MP3FILE *pmp3);
#endif

#ifdef FLAC
extern int scan_get_flacinfo(char *filename, MP3FILE *pmp3);
#endif

#ifdef MUSEPACK
extern int scan_get_mpcinfo(char *filename, MP3FILE *pmp3);
#endif

extern int scan_get_wmainfo(char *filename, MP3FILE *pmp3);
extern int scan_get_aacinfo(char *filename, MP3FILE *pmp3);
extern int scan_get_wavinfo(char *filename, MP3FILE *pmp3);
extern int scan_get_urlinfo(char *filename, MP3FILE *pmp3);
extern int scan_get_mp3info(char *filename, MP3FILE *pmp3);
extern int scan_get_aifinfo(char *filename, MP3FILE *pmp3);

/* playlist scanners */
extern int scan_xml_playlist(char *filename);
static int scan_static_playlist(char *path);

/* For known types, I'm gong to use the "official" apple
 * daap.songformat, daap.songdescription, and daap.songcodecsubtype.
 * If I we don't have "official" ones, we can make them up the
 * way we currently are:  using extension or whatver.
 *
 * This means that you can test to see if something is, say, an un-drmed
 * aac file by just testing for ->type "m4a", rather than checking every
 * different flavor of file extension.
 *
 * NOTE: Although they are represented here as strings, the codectype is
 * *really* an unsigned short.  So when it gets serialized, it gets
 * serialized as a short int. If you put something other than 3 or 4
 * characters as your codectype, you'll see strange results.
 *
 * FIXME: url != pls -- this method of dispatching handlers based on file type
 * is completely wrong.  There needs to be a separate type that gets carried
 * around with it, at least outside the database that says where the info
 * CAME FROM.
 *
 * This system is broken, and won't work with something like a .cue file
 */
static TAGHANDLER taghandlers[] = {
    { "aac", scan_get_aacinfo, "m4a", "mp4a", 0, "AAC audio file" },
    { "mp4", scan_get_aacinfo, "m4a", "mp4a", 0, "AAC audio file" },
    { "m4b", scan_get_aacinfo, "m4a", "mp4a", 0, "Protected AAC audio file" },
    { "m4a", scan_get_aacinfo, "m4a", "mp4a", 0, "AAC audio file" },
    { "m4p", scan_get_aacinfo, "m4p", "mp4a", 0, "AAC audio file" },
    { "mp3", scan_get_mp3info, "mp3", "mpeg", 0, "MPEG audio file" },
    { "wav", scan_get_wavinfo, "wav", "wav", 0, "WAV audio file" },
    { "aif", scan_get_aifinfo, "aif", "aif", 0, "AIFF audio file" },
    { "aiff",scan_get_aifinfo, "aif", "aif", 0, "AIFF audio file" },
    { "wma", scan_get_wmainfo, "wma", "wma", 0, "WMA audio file" },
    { "url", scan_get_urlinfo, "pls", NULL, 0, "Playlist URL" },
    { "pls", scan_get_urlinfo, "pls", NULL, 0, "Playlist URL" },
    { "m4v", scan_get_aacinfo, "m4v", "mp4v", 1, "MPEG-4 video file" },
    { "mp4", scan_get_aacinfo, "m4v", "mp4v", 1, "MPEG-4 video file" },
    { "mov", scan_get_aacinfo, "m4v", "mp4v", 1, "MPEG-4 video file" },
    { "mpeg4", scan_get_aacinfo, "m4v", "mp4v", 1, "MPEG-4 video file" },
#ifdef OGGVORBIS
    { "ogg", scan_get_ogginfo, "ogg", "ogg", 0, "Ogg Vorbis audio file" },
#endif
#ifdef FLAC
    { "flac", scan_get_flacinfo, "flac","flac", 0, "FLAC audio file" },
    { "fla", scan_get_flacinfo,  "flac","flac", 0, "FLAC audio file" },
#endif
#ifdef MUSEPACK
    { "mpc", scan_get_mpcinfo, "mpc", "mpc", 0, "Musepack audio file" },
    { "mpp", scan_get_mpcinfo, "mpc", "mpc", 0, "Musepack audio file" },
    { "mp+", scan_get_mpcinfo, "mpc", "mpc", 0, "Musepack audio file" },
#endif
    { NULL, NULL, NULL, NULL, 0, NULL }
};

typedef struct tag_playlistlist {
    char *path;
    struct tag_playlistlist *next;
} PLAYLISTLIST;

static PLAYLISTLIST scan_playlistlist = { NULL, NULL };

/**
 * add a playlist to the playlistlist.  The playlistlist is a
 * list of playlists that need to be processed once the current
 * scan is done.  THIS IS NOT REENTRANT, and it meant to be
 * called only inside the rescan loop.
 *
 * \param path path of the playlist to add
 */
void scan_add_playlistlist(char *path) {
    PLAYLISTLIST *plist;

    DPRINTF(E_SPAM,L_SCAN,"Adding playlist %s\n",path);

    if(!conf_get_int("scanning","process_playlists",1)) {
        DPRINTF(E_DBG,L_SCAN,"Skipping playlist %s (process_playlists)\n",path);
        return;
    }

    DPRINTF(E_DBG,L_SCAN,"Adding %s for deferred processing.\n",path);

    plist=(PLAYLISTLIST*)malloc(sizeof(PLAYLISTLIST));
    if(!plist) {
        DPRINTF(E_FATAL,L_SCAN,"Malloc error\n");
        return;
    }

    plist->path=strdup(path);
    plist->next=scan_playlistlist.next;
    scan_playlistlist.next=plist;
}

/**
 * process the playlistlist
 *
 */
void scan_process_playlistlist(void) {
    PLAYLISTLIST *pnext;
    char *ext;
    char *file;

    DPRINTF(E_LOG,L_SCAN,"Starting playlist scan\n");

    while(scan_playlistlist.next) {
        pnext=scan_playlistlist.next;

        if(!util_must_exit()) {
            DPRINTF(E_DBG,L_SCAN,"About to scan %s\n",pnext->path);
            ext=pnext->path;
            if(strrchr(pnext->path,'.')) {
                ext = strrchr(pnext->path,'.');
            }

            file=pnext->path;
            if(strrchr(pnext->path,PATHSEP)) {
                file = strrchr(pnext->path,PATHSEP) + 1;
            }

            if((!strcasecmp(file,"iTunes Music Library.xml")) ||
               (!strcasecmp(file,"iTunes Library.xml"))) {
                if(conf_get_int("scanning","process_itunes",1)) {
                    DPRINTF(E_INF,L_SCAN,"Scanning %s\n",pnext->path);
                    scan_xml_playlist(pnext->path);
                    DPRINTF(E_INF,L_SCAN,"Done Scanning %s\n",pnext->path);
                }
            } else if(!strcasecmp(ext,".m3u")) {
                if(conf_get_int("scanning","process_m3u",0)) {
                    DPRINTF(E_INF,L_SCAN,"Scanning %s\n",pnext->path);
                    scan_static_playlist(pnext->path);
                    DPRINTF(E_INF,L_SCAN,"Done Scanning %s\n",pnext->path);
                }
            }
        }

        free(pnext->path);
        scan_playlistlist.next=pnext->next;
        free(pnext);
    }
    DPRINTF(E_DBG,L_SCAN,"Finished playlist loop\n");
}

/*
 * scan_init
 *
 * This assumes the database is already initialized.
 *
 * Ideally, this would check to see if the database is empty.
 * If it is, it sets the database into bulk-import mode, and scans
 * the MP3 directory.
 *
 * If not empty, it would start a background monitor thread
 * and update files on a file-by-file basis
 */

int scan_init(char **patharray) {
    int err=0;
    int index=0;
    char resolved_path[PATH_MAX];

    DPRINTF(E_DBG,L_SCAN,"Starting scan_init\n");

    if(db_start_scan()) {
        DPRINTF(E_DBG,L_SCAN,"Error in db_start_scan()\n");
        return -1;
    }

    scan_playlistlist.next=NULL;

    while((patharray[index] != NULL) && (!util_must_exit())) {
        DPRINTF(E_DBG,L_SCAN,"Scanning for MP3s in %s\n",patharray[index]);
        realpath(patharray[index],resolved_path);
        err=scan_path(resolved_path);
        index++;
    }

    if(util_must_exit() || db_end_song_scan())
        return -1;

    if(!util_must_exit()) {
        DPRINTF(E_DBG,L_SCAN,"Processing playlists\n");
        scan_process_playlistlist();
    }

    if(db_end_scan())
        return -1;

    return err;
}

/**
 * check to see if a particular path is a complation path
 *
 * @param path path to check
 * @returns TRUE if it is a compilation path, FALSE otherwise
 */
int scan_is_compdir(char *path) {
    int current=0;
    char **compdirs;

    if(!conf_get_array("general","compdirs",&compdirs))
        return FALSE;

    while(compdirs[current]) {
        if(strcasestr(path,compdirs[current])) {
            conf_dispose_array(compdirs);
            return TRUE;
        }
        current++;
    }
    conf_dispose_array(compdirs);
    return FALSE;
}


/*
 * scan_path
 *
 * Do a brute force scan of a path, finding all the MP3 files there
 */
int scan_path(char *path) {
    DIR *current_dir;
    char de[sizeof(struct dirent) + MAXNAMLEN + 1]; /* extra for solaris */
    struct dirent *pde;
    int err;
    char relative_path[PATH_MAX];
    char mp3_path[PATH_MAX];
    struct stat sb;
    char *extensions;
    int is_compdir;
    int follow_symlinks = 0;

    follow_symlinks = conf_get_int("scanning","follow_symlinks",1);
    extensions = conf_alloc_string("general","extensions",".mp3,.m4a,.m4p");

    if((current_dir=opendir(path)) == NULL) {
        DPRINTF(E_WARN,L_SCAN,"opendir: %s\n",strerror(errno));
        free(extensions);
        return -1;
    }

    is_compdir=scan_is_compdir(path);

    while(1) {
        if(util_must_exit()) {
            DPRINTF(E_WARN,L_SCAN,"Stop req.  Aborting scan of %s.\n",path);
            closedir(current_dir);
            free(extensions);
            return 0;
        }

        pde=(struct dirent *)&de;

        err=readdir_r(current_dir,(struct dirent *)&de,&pde);
        if(err == -1) {
            DPRINTF(E_DBG,L_SCAN,"Error on readdir_r: %s\n",strerror(errno));
            err=errno;
            closedir(current_dir);
            free(extensions);
            errno=err;
            return -1;
        }

        if(!pde)
            break;

        if(!strcmp(pde->d_name,".") || !strcmp(pde->d_name,".."))
            continue;

        snprintf(relative_path,PATH_MAX,"%s/%s",path,pde->d_name);

        if(!os_lstat(relative_path,&sb)) {
            if(S_ISLNK(sb.st_mode) && !follow_symlinks)
                continue;
        }

        mp3_path[0] = '\x0';
        realpath(relative_path,mp3_path);
        DPRINTF(E_DBG,L_SCAN,"Found %s\n",relative_path);
        if(os_stat(mp3_path,&sb)) {
            DPRINTF(E_INF,L_SCAN,"Error statting %s: %s\n",mp3_path,strerror(errno));
        } else {
            if(S_ISDIR(sb.st_mode)) {  /* follow dir */
                if(conf_get_int("scanning","ignore_appledouble",1) &&
                   ((strcasecmp(pde->d_name,".AppleDouble") == 0) ||
                    (strcasecmp(pde->d_name,".AppleDesktop") == 0))) {
                    DPRINTF(E_DBG,L_SCAN,"Skipping appledouble folder\n");
                } else if(conf_get_int("scanning","ignore_dotfiles",0) &&
                          pde->d_name[0] == '.') {
                    DPRINTF(E_DBG,L_SCAN,"Skipping dotfile\n");
                } else {
                    DPRINTF(E_DBG,L_SCAN,"Found %s.. recursing\n",pde->d_name);
                    scan_path(mp3_path);
               }
            } else {
                scan_filename(mp3_path, is_compdir, extensions);
            }
        }
    }

    closedir(current_dir);
    free(extensions);
    return 0;
}

/**
 * Scan a file as a static playlist
 *
 * @param path path to playlist
 */

int scan_static_playlist(char *path) {
    char base_path[PATH_MAX];
    char file_path[PATH_MAX];
    char real_path[PATH_MAX];
    char linebuffer[PATH_MAX];
    IOHANDLE hfile;
    int playlistid;
    M3UFILE *pm3u;
    MP3FILE *pmp3;
    struct stat sb;
    char *current;
    char *perr;
    char *ptr;
    uint32_t len;

    DPRINTF(E_WARN,L_SCAN|L_PL,"Processing static playlist: %s\n",path);
    if(os_stat(path,&sb)) {
        DPRINTF(E_INF,L_SCAN,"Error statting %s: %s\n",path,strerror(errno));
        return FALSE;
    }

    if(((current=strrchr(path,'/')) == NULL) &&
       ((current=strrchr(path,'\\')) == NULL)) {
        current = path;
    } else {
        current++;
    }

    /* temporarily use base_path for m3u name */
    strcpy(base_path,current);
    if((current=strrchr(base_path,'.'))) {
        *current='\x0';
    }

    pm3u = db_fetch_playlist(NULL,path,0);
    if(pm3u && (pm3u->db_timestamp > sb.st_mtime)) {
        /* already up-to-date */
        DPRINTF(E_DBG,L_SCAN,"Playlist already up-to-date\n");
        db_dispose_playlist(pm3u);
        return TRUE;
    }

    if(pm3u) {
        DPRINTF(E_DBG,L_SCAN,"Playlist needs updated\n");
        /* welcome to texas, y'all */
        db_delete_playlist(NULL,pm3u->id);
    }

    if(!(hfile = io_new())) {
        DPRINTF(E_LOG,L_SCAN,"Cannot create file handle\n");
        return FALSE;
    }

    if(io_open(hfile,"file://%U?ascii=1",path)) {
        if(db_add_playlist(&perr,base_path,PL_STATICFILE,NULL,path,
                           0,&playlistid) != DB_E_SUCCESS) {
            DPRINTF(E_LOG,L_SCAN,"Error adding m3u %s: %s\n",path,perr);
            free(perr);
            db_dispose_playlist(pm3u);
            io_dispose(hfile);
            return FALSE;
        }
        /* now get the *real* base_path */
        strcpy(base_path,path);
        ptr = base_path;
        while(*ptr) {
            if((*ptr == '/') || (*ptr == '\\'))
                *ptr = PATHSEP;
            ptr++;
        }

        if((current=strrchr(base_path,PATHSEP))){
            *(current+1) = '\x0';
        } /* else something is fubar */

        DPRINTF(E_INF,L_SCAN|L_PL,"Added playlist as id %d\n",playlistid);

        memset(linebuffer,0x00,sizeof(linebuffer));
        io_buffer(hfile);

        len = sizeof(linebuffer);
        while(io_readline(hfile,(unsigned char *)linebuffer,&len) && len) {
            while((linebuffer[strlen(linebuffer)-1] == '\n') ||
                  (linebuffer[strlen(linebuffer)-1] == '\r'))
                linebuffer[strlen(linebuffer)-1] = '\0';

            if((linebuffer[0] == ';') || (linebuffer[0] == '#')) {
                len = sizeof(linebuffer);
                continue;
            }

            ptr = linebuffer;
            while(*ptr) {
                if((*ptr == '/') || (*ptr == '\\'))
                    *ptr = PATHSEP;
                ptr++;
            }

            // otherwise, assume it is a path
            // FIXME: Fixups for an absolute path without a drive letter
            if((linebuffer[0] == PATHSEP) || (linebuffer[1] == ':')) {
                strcpy(file_path,linebuffer);
            } else {
                snprintf(file_path,sizeof(file_path),"%s%s",base_path,linebuffer);
            }

            realpath(file_path,real_path);
            DPRINTF(E_DBG,L_SCAN|L_PL,"Checking %s\n",real_path);

            // might be valid, might not...
            if((pmp3=db_fetch_path(&perr,real_path,0))) {
                /* FIXME:  better error handling */
                DPRINTF(E_DBG,L_SCAN|L_PL,"Resolved %s to %d\n",real_path,
                        pmp3->id);
                db_add_playlist_item(NULL,playlistid,pmp3->id);
                db_dispose_item(pmp3);
            } else {
                DPRINTF(E_WARN,L_SCAN|L_PL,"Playlist entry %s bad: %s\n",
                        linebuffer,perr);
                free(perr);
            }

            len = sizeof(linebuffer);
        }
        if(!len)
            DPRINTF(L_SCAN,E_LOG,"Error reading playlist: %s\n",io_errstr(hfile));
        else
            DPRINTF(L_SCAN,E_LOG,"Finished processing playlist.  Len: %d\n",len);
        io_close(hfile);
    }

    io_dispose(hfile);
    db_dispose_playlist(pm3u);
    DPRINTF(E_WARN,L_SCAN|L_PL,"Done processing playlist\n");
    return TRUE;
}


/**
 * here, we want to scan a file and add it (if necessary)
 * to the database.
 *
 * @param path path of file to scan
 * @param compdir whether or not this is a compdir:
 *        should be SCAN_TEST_COMPDIR if called form outisde
 *        mp3-scanner.c
 */
void scan_filename(char *path, int compdir, char *extensions) {
    int is_compdir=compdir;
    char mp3_path[PATH_MAX];
    struct stat sb;
    char *fname;
    char *ext;
    char *all_ext = extensions;
    int mod_time;
    MP3FILE *pmp3;

    if(compdir == 2) {
        /* need to really figure it out */
        is_compdir = scan_is_compdir(path);
    }

    if(!all_ext) {
        all_ext = conf_alloc_string("general","extensions",".mp3,.m4a,.m4p");
    }

    realpath(path,mp3_path);
    fname = strrchr(mp3_path,PATHSEP);
    if(!fname) {
        fname = mp3_path;
    } else {
        fname++;
    }

    if(conf_get_int("scanning","ignore_dotfiles",0)) {
        if(fname[0] == '.')
            return;
        if(strncmp(fname,":2e",3) == 0)
           return;
    }

    if(conf_get_int("scanning","ignore_appledouble",1)) {
        if(strncmp(fname,"._",2) == 0)
            return;
    }


    if(os_stat(mp3_path,&sb)) {
        DPRINTF(E_INF,L_SCAN,"Error statting: %s\n",strerror(errno));
    } else {
        /* we assume this is regular file */
        if(strlen(fname) > 2) {
            ext = strrchr(fname, '.');
            if(ext && ((int)strlen(ext) > 1)) {
                if(strcasecmp(".m3u",ext) == 0) {
                    scan_add_playlistlist(mp3_path);
                } else if(strcasecmp(".xml",ext) == 0) {
                    scan_add_playlistlist(mp3_path);
                } else if(strcasestr(all_ext, ext)) {
                    mod_time = (int)sb.st_mtime;
                    pmp3 = db_fetch_path(NULL,mp3_path,0);

                    if((!pmp3) || (pmp3->db_timestamp < mod_time) ||
                       (pmp3->force_update)) {
                        scan_music_file(path,fname,&sb,is_compdir);
                    } else {
                        DPRINTF(E_DBG,L_SCAN,"Skipping file, not modified\n");
                    }
                    db_dispose_item(pmp3);
                }
            }
        }
    }

    if((all_ext) && (!extensions)) free(all_ext);
    return;
}


/*
 * scan_music_file
 *
 * scan a particular file as a music file
 */
void scan_music_file(char *path, char *fname,
                     struct stat *psb, int is_compdir) {
    MP3FILE mp3file;
    char *current=NULL;
    char *type;
    TAGHANDLER *ptaghandler;
    char fdescr[50];

    /* we found an mp3 file */
    DPRINTF(E_INF,L_SCAN,"Found music file: %s\n",fname);

    memset((void*)&mp3file,0,sizeof(mp3file));
    mp3file.path=strdup(path);
    mp3file.fname=strdup(fname);
    mp3file.file_size = psb->st_size;

    if((fname) && (strlen(fname) > 1) && (fname[strlen(fname)-1] != '.')) {
        type = strrchr(fname, '.') + 1;
        if(type && *type) {
            /* see if there is "official" format and info for it */
            ptaghandler=scan_gethandler(type);
            if(ptaghandler) {
                /* yup, use the official format */
                mp3file.type=strdup(ptaghandler->type);
                if(ptaghandler->description)
                    mp3file.description=strdup(ptaghandler->description);

                if(ptaghandler->codectype)
                    mp3file.codectype=strdup(ptaghandler->codectype);

                DPRINTF(E_DBG,L_SCAN,"Codec type: %s\n",mp3file.codectype);
            } else {
                /* just dummy up songformat, codectype and description */
                mp3file.type=strdup(type);
                mp3file.codectype = strdup("unkn");
                mp3file.song_length = 10 * 60 * 1000; /* 10 min */

                /* upper-case types cause some problems */
                current=mp3file.type;
                while(*current) {
                    *current=tolower(*current);
                    current++;
                }

                sprintf(fdescr,"%s audio file",mp3file.type);
                mp3file.description = strdup(fdescr);
                /* we'll just dodge the codectype */
            }
        }
    }

    /* Do the tag lookup here */
    if(scan_get_info(mp3file.path,&mp3file)) {
        if(is_compdir)
            mp3file.compilation = 1;
        make_composite_tags(&mp3file);
        /* fill in the time_added.  I'm not sure of the logic in this.
           My thinking is to use time created, but what is that?  Best
           guess would be earliest of st_mtime and st_ctime...
        */
        mp3file.time_added=(int) psb->st_mtime;
        if(psb->st_ctime < mp3file.time_added)
            mp3file.time_added=(int) psb->st_ctime;
        mp3file.time_modified=(int) psb->st_mtime;

        DPRINTF(E_DBG,L_SCAN," Date Added: %d\n",mp3file.time_added);

        DPRINTF(E_DBG,L_SCAN," Codec: %s\n",mp3file.codectype);

        /* FIXME: error handling */
        db_add(NULL,&mp3file,NULL);
    } else {
        DPRINTF(E_WARN,L_SCAN,"Skipping %s - scan failed\n",mp3file.path);
    }

    scan_freetags(&mp3file);
}

/**
 * fetch the taghandler for this file type
 */
TAGHANDLER *scan_gethandler(char *type) {
    TAGHANDLER *phdl = taghandlers;

    while((phdl->suffix) && (strcasecmp(phdl->suffix,type)))
        phdl++;

    if(phdl->suffix)
        return phdl;

    return NULL;
}

/*
 * scan_freetags
 *
 * Free up the tags that were dynamically allocated
 */
int scan_freetags(MP3FILE *pmp3) {
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
    MAYBEFREE(pmp3->codectype);
    MAYBEFREE(pmp3->album_artist);

    return 0;
}


/**
 * Dispatch to actual file info handlers.  In addition (and this
 * is kinda hackish, it pokes has_video into the file if it's marked
 * as having video in the taghandler.  This should really be done in
 * the metainfo parser, but it's easier to hack m4v up here and leverage
 * the existing aac parser than add video handling to the parser.
 * anyone know an easy way to tell if a mpeg4 file has a video stream
 * or not?
 *
 * @param file file to read file metainfo for
 * @param pmp3 struct to stuff with info gleaned
 */
int scan_get_info(char *file, MP3FILE *pmp3) {
    TAGHANDLER *hdl;
    int retval;

    /* dispatch to appropriate tag handler */
    hdl = scan_gethandler(pmp3->type);
    if(hdl && hdl->scanner) {
        retval = hdl->scanner(file,pmp3);
        if(retval && hdl->has_video) {
            pmp3->has_video = 1;
        }
        return retval;
    }

    return TRUE;
}

/**
 * Manually build tags.  Set artist to computer/orchestra
 * if there is already no artist.  Perhaps this could be
 * done better, but I'm not sure what else to do here.
 *
 * @param song MP3FILE of the file to build composite tags for
 */
void make_composite_tags(MP3FILE *song) {
    int len;
    char *ptmp;
    char *sep = " - ";
    char *va_artist = "Various Artists";

    if(song->genre && (strlen(song->genre) == 0)) {
        free(song->genre);
        song->genre = NULL;
    }

    if(song->artist && (strlen(song->artist) == 0)) {
        free(song->artist);
        song->artist = NULL;
    }

    if(song->title && (strlen(song->title) == 0)) {
        free(song->title);
        song->title = NULL;
    }

    if(!song->artist) {
        if (song->orchestra && song->conductor) {
            len = (int)strlen(song->orchestra) +
                  (int)strlen(sep) +
                  (int)strlen(song->conductor);
            ptmp = (char*)malloc(len + 1);
            if(ptmp) {
                sprintf(ptmp,"%s%s%s",song->orchestra, sep, song->conductor);
                song->artist = ptmp;
            }
        } else if(song->orchestra) {
            song->artist = strdup(song->orchestra);
        } else if (song->conductor) {
            song->artist = strdup(song->conductor);
        }
    }

    if(song->compilation && song->artist && song->title &&
       (conf_get_int("scanning","concat_compilations",0))) {
        len = (int)strlen(song->artist) +
              (int)strlen(sep) +
              (int)strlen(song->title);
        ptmp = (char*)malloc(len + 1);
        if(ptmp) {
            sprintf(ptmp,"%s%s%s",song->artist, sep, song->title);
            free(song->title);
            song->title = ptmp;

            if(va_artist) {
                ptmp = strdup(va_artist);
                if(ptmp) {
                    free(song->artist);
                    song->artist = ptmp;
                }
            }
        }
    }

    /* verify we have tags for the big 4 */
    if(conf_get_int("daap","empty_strings",0)) {
        if(!song->artist) song->artist = strdup("Unknown");
        if(!song->album) song->album = strdup("Unknown");
        if(!song->genre) song->genre = strdup("Unknown");
    }
    if(!song->title) song->title = strdup(song->fname);

    if(song->url)
        song->data_kind=1;
    else
        song->data_kind=0;

    song->item_kind = 2; /* music, I think. */
}

