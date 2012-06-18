/* Metadata extraction
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __METADATA_H__
#define __METADATA_H__

typedef struct metadata_s {
	char *title;
	char *artist;
	char *creator;
	char *album;
	char *genre;
	char *comment;
	char *channels;
	char *bitrate;
	char *frequency;
	char *bps;
	char *resolution;
	char *rotation;
	char *duration;
	char *date;
	char *mime;
	char *dlna_pn;
} metadata_t;

typedef enum {
  AAC_INVALID   =  0,
  AAC_MAIN      =  1, /* AAC Main */
  AAC_LC        =  2, /* AAC Low complexity */
  AAC_SSR       =  3, /* AAC SSR */
  AAC_LTP       =  4, /* AAC Long term prediction */
  AAC_HE        =  5, /* AAC High efficiency (SBR) */
  AAC_SCALE     =  6, /* Scalable */
  AAC_TWINVQ    =  7, /* TwinVQ */
  AAC_CELP      =  8, /* CELP */
  AAC_HVXC      =  9, /* HVXC */
  AAC_TTSI      = 12, /* TTSI */
  AAC_MS        = 13, /* Main synthetic */
  AAC_WAVE      = 14, /* Wavetable synthesis */
  AAC_MIDI      = 15, /* General MIDI */
  AAC_FX        = 16, /* Algorithmic Synthesis and Audio FX */
  AAC_LC_ER     = 17, /* AAC Low complexity with error recovery */
  AAC_LTP_ER    = 19, /* AAC Long term prediction with error recovery */
  AAC_SCALE_ER  = 20, /* AAC scalable with error recovery */
  AAC_TWINVQ_ER = 21, /* TwinVQ with error recovery */
  AAC_BSAC_ER   = 22, /* BSAC with error recovery */
  AAC_LD_ER     = 23, /* AAC LD with error recovery */
  AAC_CELP_ER   = 24, /* CELP with error recovery */
  AAC_HXVC_ER   = 25, /* HXVC with error recovery */
  AAC_HILN_ER   = 26, /* HILN with error recovery */
  AAC_PARAM_ER  = 27, /* Parametric with error recovery */
  AAC_SSC       = 28, /* AAC SSC */
  AAC_HE_L3     = 31, /* Reserved : seems to be HeAAC L3 */
} aac_object_type_t;

typedef enum {
	NONE,
	EMPTY,
	VALID
} ts_timestamp_t;

int
ends_with(const char * haystack, const char * needle);

char *
modifyString(char * string, const char * before, const char * after, short like);

void
check_for_captions(const char * path, sqlite_int64 detailID);

sqlite_int64
GetFolderMetadata(const char * name, const char * path, const char * artist, const char * genre, sqlite3_int64 album_art);

sqlite_int64
GetAudioMetadata(const char * path, char * name);

sqlite_int64
GetImageMetadata(const char * path, char * name);

sqlite_int64
GetVideoMetadata(const char * path, char * name);

#endif
