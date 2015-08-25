/*
 * $Id: scan-mpc.c 1484 2007-01-17 01:06:16Z rpedde $
 * Musepack tag parsing routines.
 *
 * Copyright (C) 2005 Sebastian Dröge <slomo@ubuntu.com>
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <taglib/tag_c.h>

#include "daapd.h"
#include "mp3-scanner.h"
#include "err.h"

/**
 * scan a musepack file for metainfo.
 *
 * @param filename file to read metainfo for
 * @param pmp3 MP3FILE structure to fill
 * @returns TRUE if file should be added to DB, FALSE otherwise
 */
int scan_get_mpcinfo(char *filename, MP3FILE *pmp3) {
    FILE *f;
    TagLib_File *file;
    TagLib_Tag *tag;
    const TagLib_AudioProperties *properties;
    char *val;
    int len;
    unsigned int i;

    /* open file with taglib */
    if ((file = taglib_file_new_type(filename, TagLib_File_MPC)) == NULL) {
        DPRINTF(E_WARN,L_SCAN,"Could not open %s with taglib\n", filename);
        return FALSE;
    }

    /* retrieve all tags */
    if ((tag = taglib_file_tag(file)) == NULL) {
        DPRINTF(E_WARN,L_SCAN,"Could not retrieve tags of %s\n", filename);
        taglib_file_free(file);

        return FALSE;
    }

    /* fill the MP3FILE structure with the tags */
    if ((val = taglib_tag_title(tag)) != NULL) {
        len = strlen(val);
        if ((pmp3->title = calloc(len + 1, 1)) != NULL)
            strncpy(pmp3->title, val, len);
        taglib_tag_free_strings(val);
    }
    if ((val = taglib_tag_artist(tag)) != NULL) {
        len = strlen(val);
        if ((pmp3->artist = calloc(len + 1, 1)) != NULL)
            strncpy(pmp3->artist, val, len);
        taglib_tag_free_strings(val);
    }
    if ((val = taglib_tag_album(tag)) != NULL) {
        len = strlen(val);
        if ((pmp3->album = calloc(len + 1, 1)) != NULL)
            strncpy(pmp3->album, val, len);
        taglib_tag_free_strings(val);
    }
    if ((val = taglib_tag_comment(tag)) != NULL) {
        len = strlen(val);
        if ((pmp3->comment = calloc(len + 1, 1)) != NULL)
            strncpy(pmp3->comment, val, len);
        taglib_tag_free_strings(val);
    }
    if ((val = taglib_tag_genre(tag)) != NULL) {
        len = strlen(val);
        if ((pmp3->genre = calloc(len + 1, 1)) != NULL)
            strncpy(pmp3->genre, val, len);
        taglib_tag_free_strings(val);
    }

    if ((i = taglib_tag_year(tag)) != 0)
        pmp3->year = i;
    if ((i = taglib_tag_track(tag)) != 0)
        pmp3->track = i;

    /* load the properties (like bitrate) from the file */
    if ((properties = taglib_file_audioproperties(file)) == NULL) {
        DPRINTF(E_WARN,L_SCAN,"Could not retrieve properties of %s\n", filename);
        return FALSE;
    }

    /* fill the properties in the MP3FILE structure */
    pmp3->song_length = taglib_audioproperties_length(properties) * 1000;
    pmp3->bitrate = taglib_audioproperties_bitrate(properties);
    pmp3->samplerate = taglib_audioproperties_samplerate(properties);

    taglib_file_free(file);

    return TRUE;
}
