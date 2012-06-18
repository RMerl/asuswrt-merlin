/*
 * $Id: scan-flac.c 1688 2007-10-23 03:43:17Z rpedde $
 * Implementation file for server side format conversion.
 *
 * Copyright (C) 2005 Timo J. Rinne (tri@iki.fi)
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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>      /* why here?  For osx 10.2, of course! */
#endif
#ifndef WIN32
# include <netinet/in.h>  /* htons and friends */
#endif

#include <sys/stat.h>

#include "daapd.h"
#include "restart.h"
#include "err.h"
#include "mp3-scanner.h"

#include <FLAC/metadata.h>


#define GET_VORBIS_COMMENT(comment, name, len)  (char*)                 \
        (((strncasecmp(name, (char*)(comment).entry, strlen(name)) == 0) && \
          ((comment).entry[strlen(name)] == '=')) ?                     \
         ((*(len) = (comment).length - (strlen(name) + 1)),             \
          (&((comment).entry[strlen(name) + 1]))) :                     \
         NULL)

/**
 * scan a flac file for metainfo.
 *
 * @param filename file to read metainfo for
 * @param pmp3 MP3FILE structure to fill
 * @returns TRUE if file should be added to DB, FALSE otherwise
 */
int scan_get_flacinfo(char *filename, MP3FILE *pmp3) {
    FLAC__Metadata_Chain *chain;
    FLAC__Metadata_Iterator *iterator;
    FLAC__StreamMetadata *block;
    int found=0;
    unsigned int sec, ms;
    int i;
    char *val;
    size_t len;
    char tmp;

    chain = FLAC__metadata_chain_new();
    if (! chain) {
        DPRINTF(E_WARN,L_SCAN,"Cannot allocate FLAC metadata chain\n");
        return FALSE;
    }
    if (! FLAC__metadata_chain_read(chain, filename)) {
        DPRINTF(E_WARN,L_SCAN,"Cannot read FLAC metadata from %s\n", filename);
        FLAC__metadata_chain_delete(chain);
        return FALSE;
    }

    iterator = FLAC__metadata_iterator_new();
    if (! iterator) {
        DPRINTF(E_WARN,L_SCAN,"Cannot allocate FLAC metadata iterator\n");
        FLAC__metadata_chain_delete(chain);
        return FALSE;
    }

    FLAC__metadata_iterator_init(iterator, chain);
    do {
        block = FLAC__metadata_iterator_get_block(iterator);

        if (block->type == FLAC__METADATA_TYPE_STREAMINFO) {
            sec = (unsigned int)(block->data.stream_info.total_samples /
                                 block->data.stream_info.sample_rate);
            ms = (unsigned int)(((block->data.stream_info.total_samples %
                                  block->data.stream_info.sample_rate) * 1000) /
                                block->data.stream_info.sample_rate);
            if ((sec == 0) && (ms == 0))
                break; /* Info is crap, escape div-by-zero. */
            pmp3->song_length = (sec * 1000) + ms;
            pmp3->bitrate = (uint32_t)((pmp3->file_size) / (((sec * 1000) + ms) / 8));
            pmp3->samplerate = block->data.stream_info.sample_rate;
            pmp3->bits_per_sample = block->data.stream_info.bits_per_sample;
            pmp3->sample_count = block->data.stream_info.total_samples;

            found |= 1;
            if(found == 3)
                break;
        }

        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            for (i = 0; i < (int)block->data.vorbis_comment.num_comments; i++) {
                if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                              "ARTIST", &len))) {
                    if ((pmp3->artist = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->artist, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "TITLE", &len))) {
                    if ((pmp3->title = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->title, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "ALBUMARTIST", &len))) {
                    if ((pmp3->album_artist = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->album_artist, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "ALBUM", &len))) {
                    if ((pmp3->album = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->album, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "GENRE", &len))) {
                    if ((pmp3->genre = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->genre, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "COMPOSER", &len))) {
                    if ((pmp3->composer = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->composer, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "COMMENT", &len))) {
                    if(pmp3->comment)
                        free(pmp3->comment); /* was description */
                    if ((pmp3->comment = calloc(len + 1, 1)) != NULL)
                        strncpy(pmp3->comment, val, len);
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "DESCRIPTION", &len))) {
                    if(!pmp3->comment) {
                        if ((pmp3->comment = calloc(len + 1, 1)) != NULL)
                            strncpy(pmp3->comment, val, len);
                    }
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "TRACKNUMBER", &len))) {
                    tmp = *(val + len);
                    *(val + len) = '\0';
                    pmp3->track = atoi(val);
                    *(val + len) = tmp;
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "DISCNUMBER", &len))) {
                    tmp = *(val + len);
                    *(val + len) = '\0';
                    pmp3->disc = atoi(val);
                    *(val + len) = tmp;
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "YEAR", &len))) {
                    tmp = *(val + len);
                    *(val + len) = '\0';
                    pmp3->year = atoi(val);
                    *(val + len) = tmp;
                } else if ((val = GET_VORBIS_COMMENT(block->data.vorbis_comment.comments[i],
                                                     "DATE", &len))) {
                    tmp = *(val + len);
                    *(val + len) = '\0';
                    pmp3->year = atoi(val);
                    *(val + len) = tmp;
                }
            }
            found |= 2;
            if(found == 3)
                break;
        }
    } while (FLAC__metadata_iterator_next(iterator));

    if (!found) {
        DPRINTF(E_WARN,L_SCAN,"Cannot find FLAC metadata in %s\n", filename);
    }

    FLAC__metadata_iterator_delete(iterator);
    FLAC__metadata_chain_delete(chain);
    return TRUE;
}
