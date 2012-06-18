/*
 * $Id: mp3-scanner.c,v 1.1 2009-06-30 02:31:08 steven Exp $
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
#  include "config.h"
#endif

#define _POSIX_PTHREAD_SEMANTICS
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <id3tag.h>
#include <limits.h>
#include <restart.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>  /* htons and friends */
#include <sys/stat.h>
#include <dirent.h>      /* why here?  For osx 10.2, of course! */

#include "daapd.h"
#include "db-memory.h"
#include "err.h"
#include "mp3-scanner.h"
#include "playlist.h"

#ifndef HAVE_STRCASESTR
# include "strcasestr.h"
#endif

/*
 * Typedefs
 */

/**
 * Struct to keep info about the information gleaned from
 * the mp3 frame header.
 */
typedef struct tag_scan_frameinfo {
    int layer;               /**< 1, 2, or 3, representing Layer I, II, and III */
    int bitrate;             /**< Bitrate in kbps (128, 64, etc) */
    int samplerate;          /**< Samplerate (e.g. 44100) */
    int stereo;              /**< Any kind of stereo.. joint, dual mono, etc */

    int frame_length;        /**< Frame length in bytes - calculated */
    int crc_protected;       /**< Is the frame crc protected? */
    int samples_per_frame;   /**< Samples per frame - calculated field */
    int padding;             /**< Whether or not there is a padding sample */
    int xing_offset;         /**< Where the xing header should be relative to end of hdr */
    int number_of_frames;    /**< Number of frames in the song */

    int frame_offset;        /**< Where this frame was found */

    double version;          /**< MPEG version (e.g. 2.0, 2.5, 1.0) */

    int is_valid;
} SCAN_FRAMEINFO;


typedef struct tag_scan_id3header {
    unsigned char id[3];
    unsigned char version[2];
    unsigned char flags;
    unsigned char size[4];
} __attribute((packed)) SCAN_ID3HEADER;

#define MAYBEFREE(a) { if((a)) free((a)); };

/*
 * Globals
 */
int scan_br_table[5][16] = {
    { 0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0 }, /* MPEG1, Layer 1 */
    { 0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0 },    /* MPEG1, Layer 2 */
    { 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0 },     /* MPEG1, Layer 3 */
    { 0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0 },    /* MPEG2/2.5, Layer 1 */
    { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0 }          /* MPEG2/2.5, Layer 2/3 */
};

int scan_sample_table[3][4] = {
    { 44100, 48000, 32000, 0 },  /* MPEG 1 */
    { 22050, 24000, 16000, 0 },  /* MPEG 2 */
    { 11025, 12000, 8000, 0 }    /* MPEG 2.5 */
};



int scan_mode_foreground=1;

char *scan_winamp_genre[] = {
    "Blues",              // 0
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",               // 5
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",            // 10
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",                // 15
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",        // 20
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",        // 25
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",             // 30
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",              // 35
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "AlternRock",         // 40
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",         // 45
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",           // 50
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",              // 55
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",             // 60
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",            // 65
    "New Wave",
    "Psychadelic",
    "Rave",
    "Showtunes",
    "Trailer",            // 70
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",              // 75
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",               // 80
    "Folk/Rock",
    "National folk",
    "Swing",
    "Fast-fusion",
    "Bebob",              // 85
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",         // 90
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",          // 95
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Acoustic",
    "Humour",             // 100
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",             // 105 
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",             // 110
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",           // 115
    "Ballad",
    "Powder Ballad",
    "Rhythmic Soul",
    "Freestyle",
    "Duet",               // 120
    "Punk Rock",
    "Drum Solo",
    "A Capella",
    "Euro-House",
    "Dance Hall",         // 125
    "Goa",
    "Drum & Bass",
    "Club House",
    "Hardcore",
    "Terror",             // 130
    "Indie",
    "BritPop",
    "NegerPunk",
    "Polsk Punk",
    "Beat",               // 135
    "Christian Gangsta",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary C",     // 140
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Thrash Metal",
    "Anime",              // 145
    "JPop",
    "SynthPop",
    "Unknown"
};

#define WINAMP_GENRE_UNKNOWN 148


/*
 * Forwards
 */
static int scan_path(char *path);
static int scan_gettags(char *file, MP3FILE *pmp3);
static int scan_get_mp3tags(char *file, MP3FILE *pmp3);
static int scan_get_aactags(char *file, MP3FILE *pmp3);
static int scan_get_nultags(char *file, MP3FILE *pmp3) { return 0; };
static int scan_get_fileinfo(char *file, MP3FILE *pmp3);
static int scan_get_mp3fileinfo(char *file, MP3FILE *pmp3);
static int scan_get_aacfileinfo(char *file, MP3FILE *pmp3);
static int scan_get_nulfileinfo(char *file, MP3FILE *pmp3) { return 0; };
static int scan_get_urlfileinfo(char *file, MP3FILE *pmp3);

static int scan_freetags(MP3FILE *pmp3);
static void scan_static_playlist(char *path, struct dirent *pde, struct stat *psb);
static void scan_music_file(char *path, struct dirent *pde, struct stat *psb);

static int scan_decode_mp3_frame(unsigned char *frame, SCAN_FRAMEINFO *pfi);
static time_t mac_to_unix_time(int t);

#ifdef OGGVORBIS
extern int scan_get_oggfileinfo(char *filename, MP3FILE *pmp3);
#endif

/* 
 * Typedefs
 */

typedef struct {
    char*	suffix;
    int		(*tags)(char* file, MP3FILE* pmp3);
    int		(*files)(char* file, MP3FILE* pmp3);
} taghandler;

static taghandler taghandlers[] = {
    { "aac", scan_get_aactags, scan_get_aacfileinfo },
    { "mp4", scan_get_aactags, scan_get_aacfileinfo },
    { "m4a", scan_get_aactags, scan_get_aacfileinfo },
    { "m4p", scan_get_aactags, scan_get_aacfileinfo },
    { "mp3", scan_get_mp3tags, scan_get_mp3fileinfo },
    { "url", scan_get_nultags, scan_get_urlfileinfo },
#ifdef OGGVORBIS
    { "ogg", scan_get_nultags, scan_get_oggfileinfo },
#endif
    { NULL, 0 }
};


/**
 * Convert mac time to unix time (different epochs)
 *
 * param t time since mac epoch
 */
time_t mac_to_unix_time(int t) {
  struct timeval        tv;
  struct timezone       tz;
  
  gettimeofday(&tv, &tz);
  
  return (t - (365L * 66L * 24L * 60L * 60L + 17L * 60L * 60L * 24L) +
          (tz.tz_minuteswest * 60));
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

int scan_init(char *path) {
    int err;

    scan_mode_foreground=0;
    if(db_is_empty()) {
	scan_mode_foreground=1;
    }

    if(db_start_initial_update()) 
	return -1;

    DPRINTF(E_DBG,L_SCAN,"%s scanning for MP3s in %s\n",
	    scan_mode_foreground ? "Foreground" : "Background",
	    path);

    err=scan_path(path);

    if(db_end_initial_update())
	return -1;

    scan_mode_foreground=0;

    return err;
}

/*
 * scan_path
 *
 * Do a brute force scan of a path, finding all the MP3 files there
 */
int scan_path(char *path) {
    DIR *current_dir;
    char de[sizeof(struct dirent) + MAXNAMLEN + 1]; /* overcommit for solaris */
    struct dirent *pde;
    int err;
    char mp3_path[PATH_MAX];
    struct stat sb;
    int modified_time;

    if((current_dir=opendir(path)) == NULL) {
	return -1;
    }

    while(1) {
	if(config.stop) {
	    DPRINTF(E_WARN,L_SCAN,"Stop detected.  Aborting scan of %s.\n",path);
	    closedir(current_dir);
	    return 0;
	}

	pde=(struct dirent *)&de;

	err=readdir_r(current_dir,(struct dirent *)de,&pde);
	if(err == -1) {
	    DPRINTF(E_DBG,L_SCAN,"Error on readdir_r: %s\n",strerror(errno));
	    err=errno;
	    closedir(current_dir);
	    errno=err;
	    return -1;
	}

	if(!pde)
	    break;
	
	if(pde->d_name[0] == '.') /* skip hidden and directories */
	    continue;

	snprintf(mp3_path,PATH_MAX,"%s/%s",path,pde->d_name);
	DPRINTF(E_DBG,L_SCAN,"Found %s\n",mp3_path);
	if(stat(mp3_path,&sb)) {
	    DPRINTF(E_WARN,L_SCAN,"Error statting: %s\n",strerror(errno));
	} else {
	    if(sb.st_mode & S_IFDIR) { /* dir -- recurse */
		DPRINTF(E_DBG,L_SCAN,"Found dir %s... recursing\n",pde->d_name);
		scan_path(mp3_path);
	    } else {
		/* process the file */
		if(strlen(pde->d_name) > 4) {
		    if((strcasecmp(".m3u",(char*)&pde->d_name[strlen(pde->d_name) - 4]) == 0) &&
		       config.process_m3u){
			/* we found an m3u file */
			scan_static_playlist(path, pde, &sb);
		    } else if (strcasestr(config.extensions,
					  (char*)&pde->d_name[strlen(pde->d_name) - 4])) {
			
			/* only scan if it's been changed, or empty db */
			modified_time=sb.st_mtime;
			DPRINTF(E_DBG,L_SCAN,"FS Mod time: %d\n",modified_time);
			DPRINTF(E_DBG,L_SCAN,"DB Mod time: %d\n",db_last_modified(sb.st_ino));
			if((scan_mode_foreground) || 
			   !db_exists(sb.st_ino) ||
			   db_last_modified(sb.st_ino) < modified_time) {
			    scan_music_file(path,pde,&sb);
			} else {
			    DPRINTF(E_DBG,L_SCAN,"Skipping file... not modified\n");
			}
		    }
		}
	    }
	}
    }

    closedir(current_dir);
    return 0;
}

/*
 * scan_static_playlist
 *
 * Scan a file as a static playlist
 */
void scan_static_playlist(char *path, struct dirent *pde, struct stat *psb) {
    char playlist_path[PATH_MAX];
    char m3u_path[PATH_MAX];
    char linebuffer[PATH_MAX];
    int fd;
    int playlistid;
    struct stat sb;

    DPRINTF(E_WARN,L_SCAN|L_PL,"Processing static playlist: %s\n",pde->d_name);
    
    /* see if we should update it */
    if(db_playlist_last_modified(psb->st_ino) == psb->st_mtime)
	return;

    db_delete_playlist(psb->st_ino);

    strcpy(m3u_path,pde->d_name);
    snprintf(playlist_path,sizeof(playlist_path),"%s/%s",path,pde->d_name);
    m3u_path[strlen(pde->d_name) - 4] = '\0';
    playlistid=psb->st_ino;
    fd=open(playlist_path,O_RDONLY);
    if(fd != -1) {
	db_add_playlist(playlistid,m3u_path,psb->st_mtime,0);
	DPRINTF(E_INF,L_SCAN|L_PL,"Added playlist as id %d\n",playlistid);

	memset(linebuffer,0x00,sizeof(linebuffer));
	while(readline(fd,linebuffer,sizeof(linebuffer)) > 0) {
	    while((linebuffer[strlen(linebuffer)-1] == '\n') ||
		  (linebuffer[strlen(linebuffer)-1] == '\r'))   /* windows? */
		linebuffer[strlen(linebuffer)-1] = '\0';

	    if((linebuffer[0] == ';') || (linebuffer[0] == '#'))
		continue;

	    /* FIXME - should chomp trailing comments */

	    /* otherwise, assume it is a path */
	    if(linebuffer[0] == '/') {
		strcpy(m3u_path,linebuffer);
	    } else {
		snprintf(m3u_path,sizeof(m3u_path),"%s/%s",path,linebuffer);
	    }

	    DPRINTF(E_DBG,L_SCAN|L_PL,"Checking %s\n",m3u_path);

	    /* might be valid, might not... */
	    if(!stat(m3u_path,&sb)) {
		/* FIXME: check to see if valid inode! */
		db_add_playlist_song(playlistid,sb.st_ino);
	    } else {
		DPRINTF(E_WARN,L_SCAN|L_PL,"Playlist entry %s bad: %s\n",
			m3u_path,strerror(errno));
	    }
	}
	close(fd);
    }

    DPRINTF(E_WARN,L_SCAN|L_PL,"Done processing playlist\n");
}

/*
 * scan_music_file
 *
 * scan a particular file as a music file
 */
void scan_music_file(char *path, struct dirent *pde, struct stat *psb) {
    MP3FILE mp3file;
    char mp3_path[PATH_MAX];

    snprintf(mp3_path,sizeof(mp3_path),"%s/%s",path,pde->d_name);

    /* we found an mp3 file */
    DPRINTF(E_INF,L_SCAN,"Found music file: %s\n",pde->d_name);
    
    memset((void*)&mp3file,0,sizeof(mp3file));
    mp3file.path=mp3_path;
    mp3file.fname=pde->d_name;
    if(strlen(pde->d_name) > 4)
	mp3file.type=strdup(strrchr(pde->d_name, '.') + 1);
    
    /* FIXME: assumes that st_ino is a u_int_32 
       DWB: also assumes that the library is contained entirely within
       one file system 
    */
    mp3file.id=psb->st_ino;
    
    /* Do the tag lookup here */
    if(!scan_gettags(mp3file.path,&mp3file) && 
       !scan_get_fileinfo(mp3file.path,&mp3file)) {
	make_composite_tags(&mp3file);
	/* fill in the time_added.  I'm not sure of the logic in this.
	   My thinking is to use time created, but what is that?  Best
	   guess would be earliest of st_mtime and st_ctime...
	*/
	mp3file.time_added=psb->st_mtime;
	if(psb->st_ctime < mp3file.time_added)
	    mp3file.time_added=psb->st_ctime;
        mp3file.time_modified=psb->st_mtime;

	DPRINTF(E_DBG,L_SCAN," Date Added: %d\n",mp3file.time_added);

	db_add(&mp3file);
	pl_eval(&mp3file); /* FIXME: move to db_add? */
    } else {
	DPRINTF(E_WARN,L_SCAN,"Skipping %s - scan_gettags failed\n",pde->d_name);
    }
    
    scan_freetags(&mp3file);
}

/*
 * scan_aac_findatom
 *
 * Find an AAC atom
 */
long scan_aac_findatom(FILE *fin, long max_offset, char *which_atom, int *atom_size) {
    long current_offset=0;
    int size;
    char atom[4];

    while(current_offset < max_offset) {
	if(fread((void*)&size,1,sizeof(int),fin) != sizeof(int))
	    return -1;

	size=ntohl(size);

	if(size <= 7) /* something not right */
	    return -1;

	if(fread(atom,1,4,fin) != 4) 
	    return -1;

	if(strncasecmp(atom,which_atom,4) == 0) {
	    *atom_size=size;
	    return current_offset;
	}

	fseek(fin,size-8,SEEK_CUR);
	current_offset+=size;
    }

    return -1;
}

/*
 * scan_get_aactags
 *
 * Get tags from an AAC (m4a) file
 */
int scan_get_aactags(char *file, MP3FILE *pmp3) {
    FILE *fin;
    long atom_offset;
    int atom_length;

    long current_offset=0;
    int current_size;
    char current_atom[4];
    char *current_data;
    unsigned short us_data;
    int genre;
    int len;

    if(!(fin=fopen(file,"rb"))) {
	DPRINTF(E_INF,L_SCAN,"Cannot open file %s for reading\n",file);
	return -1;
    }

    fseek(fin,0,SEEK_SET);

    atom_offset = aac_drilltoatom(fin, "moov:udta:meta:ilst", &atom_length);
    if(atom_offset != -1) {
	/* found the tag section - need to walk through now */
      
	while(current_offset < atom_length) {
	    if(fread((void*)&current_size,1,sizeof(int),fin) != sizeof(int))
		break;
			
	    current_size=ntohl(current_size);
			
	    if(current_size <= 7) /* something not right */
		break;

	    if(fread(current_atom,1,4,fin) != 4) 
		break;
			
	    len=current_size-7;  /* for ill-formed too-short tags */
	    if(len < 22)
		len=22;

	    current_data=(char*)malloc(len);  /* extra byte */
	    memset(current_data,0x00,len);

	    if(fread(current_data,1,current_size-8,fin) != current_size-8) 
		break;

	    if(!memcmp(current_atom,"\xA9" "nam",4)) { /* Song name */
		pmp3->title=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "ART",4)) {
		pmp3->artist=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "alb",4)) {
		pmp3->album=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "cmt",4)) {
		pmp3->comment=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "wrt",4)) {
		pmp3->composer=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "grp",4)) {
		pmp3->grouping=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"\xA9" "gen",4)) {
		/* can this be a winamp genre??? */
		pmp3->genre=strdup((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"tmpo",4)) {
		us_data=*((unsigned short *)&current_data[16]);
		us_data=ntohs(us_data);
		pmp3->bpm=us_data;
	    } else if(!memcmp(current_atom,"trkn",4)) {
		us_data=*((unsigned short *)&current_data[18]);
		us_data=ntohs(us_data);

		pmp3->track=us_data;

		us_data=*((unsigned short *)&current_data[20]);
		us_data=ntohs(us_data);

		pmp3->total_tracks=us_data;
	    } else if(!memcmp(current_atom,"disk",4)) {
		us_data=*((unsigned short *)&current_data[18]);
		us_data=ntohs(us_data);

		pmp3->disc=us_data;

		us_data=*((unsigned short *)&current_data[20]);
		us_data=ntohs(us_data);

		pmp3->total_discs=us_data;
	    } else if(!memcmp(current_atom,"\xA9" "day",4)) {
		pmp3->year=atoi((char*)&current_data[16]);
	    } else if(!memcmp(current_atom,"gnre",4)) {
		genre=(int)(*((char*)&current_data[17]));
		genre--;
			    
		if((genre < 0) || (genre > WINAMP_GENRE_UNKNOWN))
		    genre=WINAMP_GENRE_UNKNOWN;

		pmp3->genre=strdup(scan_winamp_genre[genre]);
	    } else if (!memcmp(current_atom, "cpil", 4)) {
		pmp3->compilation = current_data[16];
	    }

	    free(current_data);
	    current_offset+=current_size;
	}
    }

    fclose(fin);
    return 0;  /* we'll return as much as we got. */
}


/*
 * scan_gettags
 *
 * Scan an mp3 file for id3 tags using libid3tag
 */
int scan_gettags(char *file, MP3FILE *pmp3) {
    taghandler *hdl;

    /* dispatch to appropriate tag handler */
    for(hdl = taghandlers ; hdl->suffix ; ++hdl)
	if(!strcasecmp(hdl->suffix, pmp3->type))
	    break;

    if(hdl->tags)
	return hdl->tags(file, pmp3);

    /* maybe this is an extension that we've manually
     * specified in the config file, but don't know how
     * to extract tags from.  Ogg, maybe.
     */

    return 0;
}


int scan_get_mp3tags(char *file, MP3FILE *pmp3) {
    struct id3_file *pid3file;
    struct id3_tag *pid3tag;
    struct id3_frame *pid3frame;
    int err;
    int index;
    int used;
    unsigned char *utf8_text;
    int genre=WINAMP_GENRE_UNKNOWN;
    int have_utf8;
    int have_text;
    id3_ucs4_t const *native_text;
    char *tmp;
    int got_numeric_genre;

    if(strcasecmp(pmp3->type,"mp3"))  /* can't get tags for non-mp3 */
	return 0;

    pid3file=id3_file_open(file,ID3_FILE_MODE_READONLY);
    if(!pid3file) {
	DPRINTF(E_WARN,L_SCAN,"Cannot open %s\n",file);
	return -1;
    }

    pid3tag=id3_file_tag(pid3file);
    
    if(!pid3tag) {
	err=errno;
	id3_file_close(pid3file);
	errno=err;
	DPRINTF(E_WARN,L_SCAN,"Cannot get ID3 tag for %s\n",file);
	return -1;
    }

    index=0;
    while((pid3frame=id3_tag_findframe(pid3tag,"",index))) {
	used=0;
	utf8_text=NULL;
	native_text=NULL;
	have_utf8=0;
	have_text=0;

	if(!strcmp(pid3frame->id,"YTCP")) { /* for id3v2.2 */
	    pmp3->compilation = 1;
	    DPRINTF(E_DBG,L_SCAN,"Compilation: %d\n", pmp3->compilation);
	}

	if(((pid3frame->id[0] == 'T')||(strcmp(pid3frame->id,"COMM")==0)) &&
	   (id3_field_getnstrings(&pid3frame->fields[1]))) 
	    have_text=1;

	if(have_text) {
	    native_text=id3_field_getstrings(&pid3frame->fields[1],0);

	    if(native_text) {
		have_utf8=1;
		utf8_text=id3_ucs4_utf8duplicate(native_text);
		MEMNOTIFY(utf8_text);

		if(!strcmp(pid3frame->id,"TIT2")) { /* Title */
		    used=1;
		    pmp3->title = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Title: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TPE1")) {
		    used=1;
		    pmp3->artist = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Artist: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TALB")) {
		    used=1;
		    pmp3->album = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Album: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TCOM")) {
		    used=1;
		    pmp3->composer = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Composer: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TIT1")) {
		    used=1;
		    pmp3->grouping = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Grouping: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TPE2")) {
		    used=1;
		    pmp3->orchestra = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Orchestra: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TPE3")) {
		    used=1;
		    pmp3->conductor = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Conductor: %s\n",utf8_text);
		} else if(!strcmp(pid3frame->id,"TCON")) {
		    used=1;
		    pmp3->genre = utf8_text;
		    got_numeric_genre=0;
		    DPRINTF(E_DBG,L_SCAN," Genre: %s\n",utf8_text);
		    if(pmp3->genre) {
			if(!strlen(pmp3->genre)) {
			    genre=WINAMP_GENRE_UNKNOWN;
			    got_numeric_genre=1;
			} else if (isdigit(pmp3->genre[0])) {
			    genre=atoi(pmp3->genre);
			    got_numeric_genre=1;
			} else if ((pmp3->genre[0] == '(') && (isdigit(pmp3->genre[1]))) {
			    genre=atoi((char*)&pmp3->genre[1]);
			    got_numeric_genre=1;
			} 

			if(got_numeric_genre) {
			    if((genre < 0) || (genre > WINAMP_GENRE_UNKNOWN))
				genre=WINAMP_GENRE_UNKNOWN;
			    free(pmp3->genre);
			    pmp3->genre=strdup(scan_winamp_genre[genre]);
			}
		    }
		} else if(!strcmp(pid3frame->id,"COMM")) {
		    used=1;
		    pmp3->comment = utf8_text;
		    DPRINTF(E_DBG,L_SCAN," Comment: %s\n",pmp3->comment);
		} else if(!strcmp(pid3frame->id,"TPOS")) {
		    tmp=(char*)utf8_text;
		    strsep(&tmp,"/");
		    if(tmp) {
			pmp3->total_discs=atoi(tmp);
		    }
		    pmp3->disc=atoi((char*)utf8_text);
		    DPRINTF(E_DBG,L_SCAN," Disc %d of %d\n",pmp3->disc,pmp3->total_discs);
		} else if(!strcmp(pid3frame->id,"TRCK")) {
		    tmp=(char*)utf8_text;
		    strsep(&tmp,"/");
		    if(tmp) {
			pmp3->total_tracks=atoi(tmp);
		    }
		    pmp3->track=atoi((char*)utf8_text);
		    DPRINTF(E_DBG,L_SCAN," Track %d of %d\n",pmp3->track,pmp3->total_tracks);
		} else if(!strcmp(pid3frame->id,"TDRC")) {
		    pmp3->year = atoi(utf8_text);
		    DPRINTF(E_DBG,L_SCAN," Year: %d\n",pmp3->year);
		} else if(!strcmp(pid3frame->id,"TLEN")) {
		    pmp3->song_length = atoi(utf8_text); /* now in ms */
		    DPRINTF(E_DBG,L_SCAN," Length: %d\n", pmp3->song_length);
		} else if(!strcmp(pid3frame->id,"TBPM")) {
		    pmp3->bpm = atoi(utf8_text);
		    DPRINTF(E_DBG,L_SCAN,"BPM: %d\n", pmp3->bpm);
		} else if(!strcmp(pid3frame->id,"TCMP")) { /* for id3v2.3 */
                    pmp3->compilation = (char)atoi(utf8_text);
                    DPRINTF(E_DBG,L_SCAN,"Compilation: %d\n", pmp3->compilation);
                }
	    }
	}

	/* can check for non-text tags here */
	if((!used) && (have_utf8) && (utf8_text))
	    free(utf8_text);

	/* v2 COMM tags are a bit different than v1 */
	if((!strcmp(pid3frame->id,"COMM")) && (pid3frame->nfields == 4)) {
	    /* Make sure it isn't a application-specific comment...
	     * This currently includes the following:
	     *
	     * iTunes_CDDB_IDs
	     * iTunNORM
	     * 
	     * If other apps stuff crap into comment fields, then we'll ignore them
	     * here.
	     */
	    native_text=id3_field_getstring(&pid3frame->fields[2]);
	    if(native_text) {
		utf8_text=id3_ucs4_utf8duplicate(native_text);
		if((utf8_text) && (strncasecmp(utf8_text,"iTun",4) != 0)) {
		    /* it's a real comment */
		    if(utf8_text)
			free(utf8_text);

		    native_text=id3_field_getfullstring(&pid3frame->fields[3]);
		    if(native_text) {
			if(pmp3->comment)
			    free(pmp3->comment);
			utf8_text=id3_ucs4_utf8duplicate(native_text);
			if(utf8_text) {
			    pmp3->comment=utf8_text;
			    MEMNOTIFY(pmp3->comment);
			}
		    }
		} else {
		    if(utf8_text)
			free(utf8_text);
		}
	    }
	}

	index++;
    }

    id3_file_close(pid3file);
    DPRINTF(E_DBG,L_SCAN,"Got id3 tag successfully\n");
    return 0;
}

/*
 * scan_freetags
 *
 * Free up the tags that were dynamically allocated
 */
int scan_freetags(MP3FILE *pmp3) {
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

    return 0;
}


/*
 * scan_get_fileinfo
 *
 * Dispatch to actual file info handlers
 */
int scan_get_fileinfo(char *file, MP3FILE *pmp3) {
    FILE *infile;
    off_t file_size;

    taghandler *hdl;

    /* dispatch to appropriate tag handler */
    for(hdl = taghandlers ; hdl->suffix ; ++hdl)
	if(!strcmp(hdl->suffix, pmp3->type))
	    break;

    if(hdl->files)
	return hdl->files(file, pmp3);

    /* a file we don't know anything about... ogg or aiff maybe */
    if(!(infile=fopen(file,"rb"))) {
	DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading\n",file);
	return -1;
    }

    /* we can at least get this */
    fseek(infile,0,SEEK_END);
    file_size=ftell(infile);
    fseek(infile,0,SEEK_SET);

    pmp3->file_size=file_size;

    fclose(infile);
    return 0;
}

/*
 * aac_drilltoatom
 *
 * Returns the offset of the atom specified by the given path or -1 if
 * not found. atom_path is a colon separated list of atoms specifying
 * a path from parent node to the target node. All paths must be specified
 * from the root.
 */
off_t aac_drilltoatom(FILE *aac_fp, char *atom_path, unsigned int *atom_length)
{
    long          atom_offset;
    off_t         file_size;
    char          *cur_p, *end_p;
    char          atom_name[5];

    fseek(aac_fp, 0, SEEK_END);
    file_size = ftell(aac_fp);
    rewind(aac_fp);

    end_p = atom_path;
    while (*end_p != '\0')
	{
	    end_p++;
	}
    atom_name[4] = '\0';
    cur_p = atom_path;

    while (cur_p != NULL)
	{
	    if ((end_p - cur_p) < 4)
		{
		    return -1;
		}
	    strncpy(atom_name, cur_p, 4);
	    atom_offset = scan_aac_findatom(aac_fp, file_size, atom_name, atom_length);
	    if (atom_offset == -1)
		{
		    return -1;
		}
	    DPRINTF(E_DBG,L_SCAN,"Found %s atom at off %ld.\n", atom_name, ftell(aac_fp) - 8);
	    cur_p = strchr(cur_p, ':');
	    if (cur_p != NULL)
		{
		    cur_p++;

		    /* PENDING: Hack to deal with atoms that have extra data in addition
		       to having child atoms. This should be dealt in a better fashion
		       than this (table with skip offsets or an actual real mp4 parser.) */
		    if (!strcmp(atom_name, "meta")) {
			fseek(aac_fp, 4, SEEK_CUR);
		    } else if (!strcmp(atom_name, "stsd")) {
			fseek(aac_fp, 8, SEEK_CUR);
		    } else if (!strcmp(atom_name, "mp4a")) {
			fseek(aac_fp, 28, SEEK_CUR);
		    }
		}
	}

    return ftell(aac_fp) - 8;
}

/*
 * scan_get_urlfileinfo
 *
 * Get info from a "url" file -- a media stream file
 */
int scan_get_urlfileinfo(char *file, MP3FILE *pmp3) {
    FILE *infile;
    char *head, *tail;
    char linebuffer[256];

    DPRINTF(E_DBG,L_SCAN,"Getting URL file info\n");

    if(!(infile=fopen(file,"rb"))) {
	DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading\n",file);
	return -1;
    }

    fgets(linebuffer,sizeof(linebuffer),infile);
    while((linebuffer[strlen(linebuffer)-1] == '\n') ||
	  (linebuffer[strlen(linebuffer)-1] == '\r')) {
	linebuffer[strlen(linebuffer)-1] = '\0';
    }

    head=linebuffer;
    tail=strchr(head,',');
    if(!tail) {
	DPRINTF(E_LOG,L_SCAN,"Badly formatted .url file - must be bitrate,descr,url\n");
	fclose(infile);
	return -1;
    }

    pmp3->bitrate=atoi(head);
    head=++tail;
    tail=strchr(head,',');
    if(!tail) {
	DPRINTF(E_LOG,L_SCAN,"Badly formatted .url file - must be bitrate,descr,url\n");
	fclose(infile);
	return -1;
    }

    *tail++='\0';
    
    pmp3->title=strdup(head);
    pmp3->url=strdup(tail);
    fclose(infile);

    DPRINTF(E_DBG,L_SCAN,"  Title:    %s\n",pmp3->title);
    DPRINTF(E_DBG,L_SCAN,"  Bitrate:  %d\n",pmp3->bitrate);
    DPRINTF(E_DBG,L_SCAN,"  URL:      %s\n",pmp3->url);

    return 0;
}

/*
 * scan_get_aacfileinfo
 *
 * Get info from the actual aac headers
 */
int scan_get_aacfileinfo(char *file, MP3FILE *pmp3) {
    FILE *infile;
    long atom_offset;
    int atom_length;
    int sample_size;
    int samples;
    unsigned int bit_rate;
    off_t file_size;
    int ms;
    unsigned char buffer[2];
    int time = 0;

    DPRINTF(E_DBG,L_SCAN,"Getting AAC file info\n");

    if(!(infile=fopen(file,"rb"))) {
	DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading\n",file);
	return -1;
    }

    fseek(infile,0,SEEK_END);
    file_size=ftell(infile);
    fseek(infile,0,SEEK_SET);

    pmp3->file_size=file_size;

    /* now, hunt for the mvhd atom */
    atom_offset = aac_drilltoatom(infile, "moov:mvhd", &atom_length);
    if(atom_offset != -1) {
        fseek(infile, 4, SEEK_CUR);
        fread((void *)&time, sizeof(int), 1, infile);
        time = ntohl(time);
        pmp3->time_added = mac_to_unix_time(time);

        fread((void *)&time, sizeof(int), 1, infile);
        time = ntohl(time);
        pmp3->time_modified = mac_to_unix_time(time);
	fread((void*)&sample_size,1,sizeof(int),infile);
	fread((void*)&samples,1,sizeof(int),infile);

	sample_size=ntohl(sample_size);
	samples=ntohl(samples);

	/* avoid overflowing on large sample_sizes (90000) */
	ms=1000;
	while((ms > 9) && (!(sample_size % 10))) {
	    sample_size /= 10;
	    ms /= 10;
	}

	/* DWB: use ms time instead of sec */
	pmp3->song_length=(int)((samples * ms) / sample_size);

	DPRINTF(E_DBG,L_SCAN,"Song length: %d seconds\n", pmp3->song_length / 1000);
    }

    pmp3->bitrate = 0;

    /* Get the sample rate from the 'mp4a' atom (timescale). This is also
       found in the 'mdhd' atom which is a bit closer but we need to 
       navigate to the 'mp4a' atom anyways to get to the 'esds' atom. */
    atom_offset = aac_drilltoatom(infile, "moov:trak:mdia:minf:stbl:stsd:mp4a", &atom_length);
    if (atom_offset != -1) {
	fseek(infile, atom_offset + 32, SEEK_SET);

	/* Timescale here seems to be 2 bytes here (the 2 bytes before it are
	   "reserved") though the timescale in the 'mdhd' atom is 4. Not sure how
	   this is dealt with when sample rate goes higher than 64K. */
	fread(buffer, sizeof(unsigned char), 2, infile);

	pmp3->samplerate = (buffer[0] << 8) | (buffer[1]);

	/* Seek to end of atom. */
	fseek(infile, 2, SEEK_CUR);

	/* Get the bit rate from the 'esds' atom. We are already positioned
	   in the parent atom so just scan ahead. */
	atom_offset = scan_aac_findatom(infile, atom_length - (ftell(infile) - atom_offset), "esds", &atom_length);

	if (atom_offset != -1) {
	    fseek(infile, atom_offset + 22, SEEK_CUR);

	    fread((void *)&bit_rate, sizeof(unsigned int), 1, infile);

	    pmp3->bitrate = ntohl(bit_rate) / 1000;
	} else {
	    DPRINTF(E_DBG,L_SCAN, "Could not find 'esds' atom to determine bit rate.\n");
	}
      
    } else {
	DPRINTF(E_DBG,L_SCAN, "Could not find 'mp4a' atom to determine sample rate.\n");
    }

    /* Fallback if we can't find the info in the atoms. */
    if (pmp3->bitrate == 0) {
	/* calculate bitrate from song length... Kinda cheesy */
	DPRINTF(E_DBG,L_SCAN, "Could not find 'esds' atom. Calculating bit rate.\n");

	atom_offset=aac_drilltoatom(infile,"mdat",&atom_length);

	if ((atom_offset != -1) && (pmp3->song_length)) {
	    pmp3->bitrate = atom_length / ((pmp3->song_length / 1000) * 128);
	}

    }

    fclose(infile);
    return 0;
}


/**
 * Decode an mp3 frame header.  Determine layer, bitrate, 
 * samplerate, etc, and fill in the passed structure.
 *
 * @param frame 4 byte mp3 frame header
 * @param pfi pointer to an allocated SCAN_FRAMEINFO struct
 * @return 0 on success (valid frame), -1 otherwise
 */
int scan_decode_mp3_frame(unsigned char *frame, SCAN_FRAMEINFO *pfi) {
    int ver;
    int layer_index;
    int sample_index;
    int bitrate_index;
    int samplerate_index;

    if((frame[0] != 0xFF) || (frame[1] < 224)) {
	pfi->is_valid=0;
	return -1;
    }

    ver=(frame[1] & 0x18) >> 3;
    pfi->layer = 4 - ((frame[1] & 0x6) >> 1);

    layer_index=-1;
    sample_index=-1;

    switch(ver) {
    case 0:
	pfi->version = 2.5;
	sample_index=2;
	if(pfi->layer == 1)
	    layer_index = 3;
	if((pfi->layer == 2) || (pfi->layer == 3))
	    layer_index = 4;
	break;
    case 2:                 
	pfi->version = 2.0;
	sample_index=1;
	if(pfi->layer == 1)
	    layer_index=3;
	if((pfi->layer == 2) || (pfi->layer == 3))
	    layer_index=4;
	break;
    case 3:
	pfi->version = 1.0;
	sample_index=0;
	if(pfi->layer == 1)
	    layer_index = 0;
	if(pfi->layer == 2)
	    layer_index = 1;
	if(pfi->layer == 3)
	    layer_index = 2;
	break;
    }

    if((layer_index < 0) || (layer_index > 4)) {
	pfi->is_valid=0;
	return -1;
    }

    if((sample_index < 0) || (sample_index > 2)) {
	pfi->is_valid=0;
	return -1;
    }

    if(pfi->layer==1) pfi->samples_per_frame=384;
    if(pfi->layer==2) pfi->samples_per_frame=1152;
    if(pfi->layer==3) {
	if(pfi->version == 1.0) {
	    pfi->samples_per_frame=1152;
	} else {
	    pfi->samples_per_frame=576;
	}
    }

    bitrate_index=(frame[2] & 0xF0) >> 4;
    samplerate_index=(frame[2] & 0x0C) >> 2;

    if((bitrate_index == 0xF) || (bitrate_index==0x0)) {
	pfi->is_valid=0;
	return -1;
    }

    if(samplerate_index == 3) {
	pfi->is_valid=0;
	return -1;
    }
	
    pfi->bitrate = scan_br_table[layer_index][bitrate_index];
    pfi->samplerate = scan_sample_table[sample_index][samplerate_index];

    if((frame[3] & 0xC0 >> 6) == 3) 
	pfi->stereo = 0;
    else
	pfi->stereo = 1;

    if(frame[2] & 0x02) { /* Padding bit set */
	pfi->padding=1;
    } else {
	pfi->padding=0;
    }

    if(pfi->version == 1.0) {
	if(pfi->stereo) {
	    pfi->xing_offset=32;
	} else {
	    pfi->xing_offset=17;
	}
    } else {
	if(pfi->stereo) {
	    pfi->xing_offset=17;
	} else {
	    pfi->xing_offset=9;
	}
    }

    pfi->crc_protected=(frame[1] & 0xFE);

    if(pfi->layer == 1) {
	pfi->frame_length = (12 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding) * 4;
    } else {
	pfi->frame_length = 144 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding;
    }

    if((pfi->frame_length > 2880) || (pfi->frame_length <= 0)) {
	pfi->is_valid=0;
	return -1;
    }

    pfi->is_valid=1;
    return 0;
}

/**
 * Scan 10 frames from the middle of the file and determine an
 * average bitrate from that.  It might not be as accurate as a full
 * frame count, but it's probably Close Enough (tm)
 *
 * @param infile file to scan for average bitrate
 * @param pfi pointer to frame info struct to put the bitrate into
 */
void scan_get_average_bitrate(FILE *infile, SCAN_FRAMEINFO *pfi) {
    off_t file_size;
    unsigned char frame_buffer[2900];
    unsigned char header[4];
    int index=0;
    int found=0;
    off_t pos;
    SCAN_FRAMEINFO fi;
    int frame_count=0;
    int bitrate_total=0;

    DPRINTF(E_DBG,L_SCAN,"Starting averaging bitrate\n");

    fseek(infile,0,SEEK_END);
    file_size=ftell(infile);

    pos=file_size/2;

    /* now, find the first frame */
    fseek(infile,pos,SEEK_SET);
    if(fread(frame_buffer,1,sizeof(frame_buffer),infile) != sizeof(frame_buffer)) 
	return;

    while(!found) {
	while((frame_buffer[index] != 0xFF) && (index < (sizeof(frame_buffer)-4)))
	    index++;

	if(index >= (sizeof(frame_buffer)-4)) { /* largest mp3 frame is 2880 bytes */
	    DPRINTF(E_DBG,L_SCAN,"Could not find frame... quitting\n");
	    return;
	}
	    
	if(!scan_decode_mp3_frame(&frame_buffer[index],&fi)) { 
	    /* see if next frame is valid */
	    fseek(infile,pos + index + fi.frame_length,SEEK_SET);
	    if(fread(header,1,sizeof(header),infile) != sizeof(header)) {
		DPRINTF(E_DBG,L_SCAN,"Could not read frame header\n");
		return;
	    }

	    if(!scan_decode_mp3_frame(header,&fi))
		found=1;
	}
	
	if(!found)
	    index++;
    }

    pos += index;

    /* found first frame.  Let's move */
    while(frame_count < 10) {
	fseek(infile,pos,SEEK_SET);
	if(fread(header,1,sizeof(header),infile) != sizeof(header)) {
	    DPRINTF(E_DBG,L_SCAN,"Could not read frame header\n");
	    return;
	}
	if(scan_decode_mp3_frame(header,&fi)) {
	    DPRINTF(E_DBG,L_SCAN,"Invalid frame header while averaging\n");
	    return;
	}
    
	bitrate_total += fi.bitrate;
	frame_count++;
	pos += fi.frame_length;
    }

    DPRINTF(E_DBG,L_SCAN,"Old bitrate: %d\n",pfi->bitrate);
    pfi->bitrate = bitrate_total/frame_count;
    DPRINTF(E_DBG,L_SCAN,"New bitrate: %d\n",pfi->bitrate);

    return;
}

/**
 * do a full frame-by-frame scan of the file, counting frames
 * as we go to try and get a more accurate song length estimate.
 * If the song turns out to be CBR, then we'll not set the frame
 * length.  Instead we'll use the file size estimate, since it is 
 * more consistent with iTunes.
 *
 * @param infile file to scan for frame count
 * @param pfi pointer to frame info struct to put framecount into
 */
void scan_get_frame_count(FILE *infile, SCAN_FRAMEINFO *pfi) {
    int pos;
    int frames=0;
    unsigned char frame_buffer[4];
    SCAN_FRAMEINFO fi;
    off_t file_size;
    int err=0;
    int cbr=1;
    int last_bitrate=0;

    DPRINTF(E_DBG,L_SCAN,"Starting frame count\n");
    
    fseek(infile,0,SEEK_END);
    file_size=ftell(infile);

    pos=pfi->frame_offset;

    while(1) {
	err=1;
	DPRINTF(E_SPAM,L_SCAN,"Seeking to %d\n",pos);

	fseek(infile,pos,SEEK_SET);
	if(fread(frame_buffer,1,sizeof(frame_buffer),infile) == sizeof(frame_buffer)) {
	    /* check for valid frame */
	    if(!scan_decode_mp3_frame(frame_buffer,&fi)) {
		frames++;
		pos += fi.frame_length;
		err=0;

		if((last_bitrate) && (fi.bitrate != last_bitrate))
		    cbr=0;
		last_bitrate=fi.bitrate;

		/* no point in brute scan of a cbr file... */
		if(cbr && (frames > 100)) {
		    DPRINTF(E_DBG,L_SCAN,"File appears to be CBR... quitting frame count\n");
		    return;
		}
	    }
	}

	if(err) {
	    if(pos > (file_size - 4096)) {  /* probably good enough */
		pfi->number_of_frames=frames;
		DPRINTF(E_DBG,L_SCAN,"Estimated frame count: %d\n",frames);
		return;
	    } else {
		DPRINTF(E_DBG,L_SCAN,"Frame count aborted on error.  Pos=%d, Count=%d\n",
			pos, frames);
		return;
	    }
	}
    }
}


/**
 * Get information from the file headers itself -- like
 * song length, bit rate, etc.
 *
 * @param file File to get info for
 * @param pmp3 where to put the found information
 */
int scan_get_mp3fileinfo(char *file, MP3FILE *pmp3) {
    FILE *infile;
    SCAN_ID3HEADER *pid3;
    SCAN_FRAMEINFO fi;
    unsigned int size=0;
    off_t fp_size=0;
    off_t file_size;
    unsigned char buffer[1024];
    int index;

    int xing_flags;
    int found;

    int first_check;
    char frame_buffer[4];

    if(!(infile=fopen(file,"rb"))) {
	DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading\n",file);
	return -1;
    }

    memset((void*)&fi,0x00,sizeof(fi));

    fseek(infile,0,SEEK_END);
    file_size=ftell(infile);
    fseek(infile,0,SEEK_SET);

    pmp3->file_size=file_size;

    if(fread(buffer,1,sizeof(buffer),infile) != sizeof(buffer)) {
	if(ferror(infile)) {
	    DPRINTF(E_LOG,L_SCAN,"Error reading: %s\n",strerror(errno));
	} else {
	    DPRINTF(E_LOG,L_SCAN,"Short file: %s\n",file);
	}
	fclose(infile);
	return -1;
    }

    pid3=(SCAN_ID3HEADER*)buffer;

    found=0;
    fp_size=0;

    if(strncmp(pid3->id,"ID3",3)==0) {
	/* found an ID3 header... */
	DPRINTF(E_DBG,L_SCAN,"Found ID3 header\n");
	size = (pid3->size[0] << 21 | pid3->size[1] << 14 | 
		pid3->size[2] << 7 | pid3->size[3]);
	fp_size=size + sizeof(SCAN_ID3HEADER);
	first_check=1;
	DPRINTF(E_DBG,L_SCAN,"Header length: %d\n",size);
    }

    index = 0;

    /* Here we start the brute-force header seeking.  Sure wish there
     * weren't so many crappy mp3 files out there
     */

    while(!found) {
	fseek(infile,fp_size,SEEK_SET);
	DPRINTF(E_DBG,L_SCAN,"Reading in new block at %d\n",(int)fp_size);
	if(fread(buffer,1,sizeof(buffer),infile) < sizeof(buffer)) {
	    DPRINTF(E_LOG,L_SCAN,"Short read: %s\n",file);
	    fclose(infile);
	    return 0;
	}

	index=0;
	while(!found) {
	    while((buffer[index] != 0xFF) && (index < (sizeof(buffer)-50)))
		index++;

	    if((first_check) && (index)) {
		fp_size=0;
		DPRINTF(E_DBG,L_SCAN,"Bad header... dropping back for full frame search\n");
		first_check=0;
		break;
	    }

	    if(index > sizeof(buffer) - 50) {
		fp_size += index;
		DPRINTF(E_DBG,L_SCAN,"Block exhausted\n");
		break;
	    }

	    if(!scan_decode_mp3_frame(&buffer[index],&fi)) {
		DPRINTF(E_DBG,L_SCAN,"valid header at %d\n",index);
		if(strncasecmp((char*)&buffer[index+fi.xing_offset+4],"XING",4) == 0) {
		    /* no need to check further... if there is a xing header there,
		     * this is definately a valid frame */
		    found=1;
		    fp_size += index;
		} else {
		    /* No Xing... check for next frame */
		    DPRINTF(E_DBG,L_SCAN,"Found valid frame at %04x\n",(int)fp_size+index);
		    DPRINTF(E_DBG,L_SCAN,"Checking at %04x\n",(int)fp_size+index+fi.frame_length);
		    fseek(infile,fp_size + index + fi.frame_length,SEEK_SET);
		    if(fread(frame_buffer,1,sizeof(frame_buffer),infile) == sizeof(frame_buffer)) {
			if(!scan_decode_mp3_frame(frame_buffer,&fi)) {
			    found=1;
			    fp_size += index;
			} 
		    } else {
			DPRINTF(E_LOG,L_SCAN,"Could not read frame header: %s\n",file);
			fclose(infile);
			return 0;
		    }

		    if(!found) {
			DPRINTF(E_DBG,L_SCAN,"Didn't pan out.\n");
		    }
		}
	    }
	    
	    if(!found) {
		index++;
		if (first_check) {
		    /* if the header info was wrong about where the data started,
		     * then start a brute-force scan from the beginning of the file.
		     * don't want to just scan forward, because we might have already 
		     * missed the xing header
		     */
		    DPRINTF(E_DBG,L_SCAN,"Bad header... dropping back for full frame search\n");
		    first_check=0;
		    fp_size=0;
		    break;
		}
	    }
	}
    }

    file_size -= fp_size;
    fi.frame_offset=fp_size;

    if(scan_decode_mp3_frame(&buffer[index],&fi)) {
	fclose(infile);
	DPRINTF(E_LOG,L_SCAN,"Could not find sync frame: %s\n",file);
	DPRINTF(E_LOG,L_SCAN,"If this is a valid mp3 file that plays in "
		"other applications, please email me at rpedde@users.sourceforge.net "
		"and tell me you got this error.  Thanks");
	return 0;
    }

    DPRINTF(E_DBG,L_SCAN," MPEG Version: %0.1g\n",fi.version);
    DPRINTF(E_DBG,L_SCAN," Layer: %d\n",fi.layer);
    DPRINTF(E_DBG,L_SCAN," Sample Rate: %d\n",fi.samplerate);
    DPRINTF(E_DBG,L_SCAN," Bit Rate: %d\n",fi.bitrate);
	
    /* now check for an XING header */
    if(strncasecmp((char*)&buffer[index+fi.xing_offset+4],"XING",4) == 0) {
	DPRINTF(E_DBG,L_SCAN,"Found Xing header\n");
	xing_flags=*((int*)&buffer[index+fi.xing_offset+4+4]);
	xing_flags=ntohs(xing_flags);

	DPRINTF(E_DBG,L_SCAN,"Xing Flags: %02X\n",xing_flags);

	if(xing_flags & 0x1) {
	    /* Frames field is valid... */
	    fi.number_of_frames=*((int*)&buffer[index+fi.xing_offset+4+8]);
	    fi.number_of_frames=ntohs(fi.number_of_frames);
	}
    }

    if((config.scan_type != 0) &&
       (fi.number_of_frames == 0) &&
       (!pmp3->song_length)) {
	/* We have no good estimate of song time, and we want more
	 * aggressive scanning */
	DPRINTF(E_DBG,L_SCAN,"Starting aggressive file length scan\n");
	if(config.scan_type == 1) {
	    /* get average bitrate */
	    scan_get_average_bitrate(infile, &fi);
	} else {
	    /* get full frame count */
	    scan_get_frame_count(infile, &fi);
	}
    }

    pmp3->bitrate=fi.bitrate;
    pmp3->samplerate=fi.samplerate;
    
    /* guesstimate the file length */
    if(!pmp3->song_length) { /* could have gotten it from the tag */
	/* DWB: use ms time instead of seconds, use doubles to
	   avoid overflow */
	if(!fi.number_of_frames) { /* not vbr */
	    pmp3->song_length = (int) ((double) file_size * 8. /
				       (double) fi.bitrate);

	} else {
	    pmp3->song_length = (int) ((double)(fi.number_of_frames*fi.samples_per_frame*1000.)/
				       (double) fi.samplerate);
	}

    }
    DPRINTF(E_DBG,L_SCAN," Song Length: %d\n",pmp3->song_length);

    fclose(infile);
    return 0;
}

/**
 * Manually build tags.  Set artist to computer/orchestra
 * if there is already no artist.  Perhaps this could be 
 * done better, but I'm not sure what else to do here.
 *
 * @param song MP3FILE of the file to build composite tags for
 */
void make_composite_tags(MP3FILE *song)
{
    int len;
    char fdescr[50];

    len=0;

    if(!song->artist && (song->orchestra || song->conductor)) {
	if(song->orchestra)
	    len += strlen(song->orchestra);
	if(song->conductor)
	    len += strlen(song->conductor);

	len += 3;

	song->artist=(char*)calloc(len, 1);
	if(song->artist) {
	    if(song->orchestra)
		strcat(song->artist,song->orchestra);

	    if(song->orchestra && song->conductor)
		strcat(song->artist," - ");

	    if(song->conductor)
		strcat(song->artist,song->conductor);
	}
    }

    sprintf(fdescr,"%s audio file",song->type);
    song->description = strdup(fdescr);

    if(song->url) {
	song->description = strdup("Playlist URL");
	song->data_kind=1;
	/* bit of a hack for the roku soundbridge - type *has* to be pls */
	if(song->type)
	    free(song->type);
	song->type = strdup("pls");
    } else {
	song->data_kind=0;
    }

    if(!song->title)
	song->title = strdup(song->fname);

    /* Ogg used to be set as an item_kind of 4.  Dunno why */
    song->item_kind = 2;
}
