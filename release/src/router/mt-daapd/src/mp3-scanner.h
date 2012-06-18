/*
 * $Id: mp3-scanner.h,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Header file for mp3 scanner and monitor
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

#ifndef _MP3_SCANNER_H_
#define _MP3_SCANNER_H_

#include <sys/types.h>

typedef struct tag_mp3file {
    char *path;
    char *fname;
    char *title;     /* TIT2 */
    char *artist;    /* TPE1 */
    char *album;     /* TALB */
    char *genre;     /* TCON */
    char *comment;   /* COMM */
    char *type;
    char *composer;  /* TCOM */
    char *orchestra; /* TPE2 */
    char *conductor; /* TPE3 */
    char *grouping;  /* TIT1 */
    char *url;       /* daap.songdataurl (asul) */

    int bitrate;
    int samplerate;
    int song_length;
    int file_size;
    int year;        /* TDRC */
    
    int track;       /* TRCK */
    int total_tracks;

    int disc;        /* TPOS */
    int total_discs;

    int time_added;
    int time_modified;
    int time_played;
    int db_timestamp;
    
    int bpm;         /* TBPM */

    int got_id3;
//    unsigned int id;
	unsigned long int	id;
    /* generated fields */
    char* description;		/* long file type */
    int item_kind;		/* song or movie */
    int data_kind;              /* dmap.datakind (asdk) */

    char compilation;
} MP3FILE;

extern int scan_init(char *path);
extern void make_composite_tags(MP3FILE *song);

/* this should be refactored out of here... */
extern off_t aac_drilltoatom(FILE *aac_fp, char *atom_path, unsigned int *atom_length);

#endif /* _MP3_SCANNER_H_ */
