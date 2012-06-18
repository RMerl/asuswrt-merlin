//=========================================================================
// FILENAME	: taguilts.h
// DESCRIPTION	: Header for tagutils.c
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/*
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file is derived from mt-daap project.
 */

#ifndef _TAG_H_
#define _TAG_H_

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ROLE_NOUSE 0
#define ROLE_START 1
#define ROLE_ARTIST 1
#define ROLE_TRACKARTIST 2
#define ROLE_ALBUMARTIST 3
#define ROLE_BAND 4
#define ROLE_CONDUCTOR 5
#define ROLE_COMPOSER 6
#define ROLE_LAST 6
#define N_ROLE 7

struct song_metadata {
	int file_size;
	char *dirpath;
	char *path;
	char *basename;                         // basename is part of path
	char *type;
	int time_modified;

	char *image;                            // coverart
	int image_size;

	char *title;                            // TIT2
	char *album;                            // TALB
	char *genre;                            // TCON
	char *comment;                          // COMM

	char *contributor[N_ROLE];              // TPE1  (artist)
						// TCOM  (composer)
						// TPE3  (conductor)
						// TPE2  (orchestra)
	char *contributor_sort[N_ROLE];


	char *grouping;                         // TIT1
	int year;                               // TDRC
	int track;                              // TRCK
	int total_tracks;                       // TRCK
	int disc;                               // TPOS
	int total_discs;                        // TPOS
	int bpm;                                // TBPM
	char compilation;                       // YTCP

	int bitrate;
	int max_bitrate;
	int samplerate;
	int samplesize;
	int channels;
	int song_length;                        // TLEN
	int audio_size;
	int audio_offset;
	int vbr_scale;
	int lossless;
	int blockalignment;

	char *mime;				// MIME type
	char *dlna_pn;				// DLNA Profile Name

	char *tagversion;

	unsigned long album_id;
	unsigned long track_id;
	unsigned long genre_id;
	unsigned long contributor_id[N_ROLE];

	char *musicbrainz_albumid;
	char *musicbrainz_trackid;
	char *musicbrainz_artistid;
	char *musicbrainz_albumartistid;

	int is_plist;
	int plist_position;
	int plist_id;
};

#define WMA     0x161
#define WMAPRO  0x162
#define WMALSL  0x163

extern int scan_init(char *path);
extern void make_composite_tags(struct song_metadata *psong);
extern int readtags(char *path, struct song_metadata *psong, struct stat *stat, char *lang, char *type);
extern void freetags(struct song_metadata *psong);

extern int start_plist(const char *path, struct song_metadata *psong, struct stat *stat, char *lang, char *type);
extern int next_plist_track(struct song_metadata *psong, struct stat *stat, char *lang, char *type);

#endif
