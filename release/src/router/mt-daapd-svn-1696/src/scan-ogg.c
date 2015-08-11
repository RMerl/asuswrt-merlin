/*
 * $Id: scan-ogg.c 1624 2007-08-01 06:32:15Z rpedde $
 * Ogg parsing routines.
 *
 *
 * Copyright 2002 Michael Smith <msmith@xiph.org>
 * Licensed under the GNU GPL, distributed with this program.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <vorbis/vorbisfile.h>

#include "daapd.h"
#include "err.h"
#include "io.h"
#include "mp3-scanner.h"

size_t scan_ogg_read(void *ptr, size_t size, size_t nmemb, void *datasource) {
    IOHANDLE hfile = (IOHANDLE)datasource;
    uint32_t bytes_read;
    
    bytes_read = (uint32_t)(size * nmemb);
    if(!io_read(hfile,ptr,&bytes_read))
        return -1;
        
    return (size_t)bytes_read;
}

int scan_ogg_seek(void *datasource, int64_t offset, int whence) {
    IOHANDLE hfile = (IOHANDLE)datasource;
    
    if(!io_setpos(hfile,(uint64_t)offset,whence))
        return -1;
    return 0;
}

int scan_ogg_close(void *datasource) {
    IOHANDLE hfile = (IOHANDLE)datasource;
    int retcode;
    
    retcode = io_close(hfile);
    io_dispose(hfile);
    
    return retcode ? 0 : EOF;
}

long scan_ogg_tell(void *datasource) {
    IOHANDLE hfile = (IOHANDLE)datasource;
    uint64_t pos;
    
    if(!io_getpos(hfile,&pos))
        return -1;
        
    return (long)pos;
}


/**
 * get ogg metainfo
 *
 * @param filename file to read metainfo for
 * @param pmp3 MP3FILE struct to fill with metainfo
 * @returns TRUE if file should be added to DB, FALSE otherwise
 */
int scan_get_ogginfo(char *filename, MP3FILE *pmp3) {
    IOHANDLE hfile;
    OggVorbis_File vf;
    vorbis_comment *comment = NULL;
    vorbis_info *vi = NULL;
    char *val;
    ov_callbacks callbacks = { scan_ogg_read, scan_ogg_seek, scan_ogg_close, scan_ogg_tell };

    
    hfile = io_new();
    if(!hfile) {
        DPRINTF(E_LOG,L_SCAN,"Could not create file handle\n");
        return FALSE;
    }
    
    if(!io_open(hfile,"file://%U",filename)) {
        DPRINTF(E_LOG,L_SCAN,"Error opening %s: %s", filename, io_errstr(hfile));
        return FALSE;
    }
    
    if(ov_open_callbacks(hfile,&vf,NULL,0,callbacks) < 0) {
        DPRINTF(E_LOG,L_SCAN,"Could not create oggvorbis file handler\n");
        return FALSE;
    }
    
    vi=ov_info(&vf,-1);
    if(vi) {
        DPRINTF(E_DBG,L_SCAN," Bitrates: %d %d %d\n",vi->bitrate_upper,
                vi->bitrate_nominal,vi->bitrate_lower);
        if(vi->bitrate_nominal) {
            pmp3->bitrate=vi->bitrate_nominal / 1000;
        } else if(vi->bitrate_upper) {
            pmp3->bitrate=vi->bitrate_upper / 1000;
        } else if(vi->bitrate_lower) {
            pmp3->bitrate=vi->bitrate_lower / 1000;
        }

        DPRINTF(E_DBG,L_SCAN," Bitrates: %d",pmp3->bitrate);
        pmp3->samplerate=vi->rate;
    }

    pmp3->song_length=(int)ov_time_total(&vf,-1) * 1000;

    comment = ov_comment(&vf, -1);
    if (comment != NULL) {
        if ((val = vorbis_comment_query(comment, "artist", 0)) != NULL)
            pmp3->artist = strdup(val);
        if ((val = vorbis_comment_query(comment, "title", 0)) != NULL)
            pmp3->title = strdup(val);
        if ((val = vorbis_comment_query(comment, "album", 0)) != NULL)
            pmp3->album = strdup(val);
        if ((val = vorbis_comment_query(comment, "genre", 0)) != NULL)
            pmp3->genre = strdup(val);
        if ((val = vorbis_comment_query(comment, "composer", 0)) != NULL)
            pmp3->composer = strdup(val);
        if ((val = vorbis_comment_query(comment, "comment", 0)) != NULL)
            pmp3->comment = strdup(val);
        if ((val = vorbis_comment_query(comment, "tracknumber", 0)) != NULL)
            pmp3->track = atoi(val);
        if ((val = vorbis_comment_query(comment, "discnumber", 0)) != NULL)
            pmp3->disc = atoi(val);
        if ((val = vorbis_comment_query(comment, "year", 0)) != NULL)
            pmp3->year = atoi(val);
    }
    ov_clear(&vf);
    return TRUE;
}
