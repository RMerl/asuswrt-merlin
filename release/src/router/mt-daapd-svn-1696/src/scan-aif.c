/*
 * $Id: $
 *
 * Copyright (C) 2006 Ron Pedde (ron@pedde.com)
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
# include <stdint.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>

#include "daapd.h"
#include "err.h"
#include "io.h"
#include "mp3-scanner.h"

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
# define _PACKED __attribute((packed))
#else
# define _PACKED
#endif

#pragma pack(1)
typedef struct tag_aif_chunk_header {
    char       id[4];
    uint32_t   len;
} _PACKED AIF_CHUNK_HEADER;

typedef struct tag_iff_header {
    char       id[4];
    uint32_t   length;
    char       type[4];
} _PACKED AIF_IFF_HEADER;

typedef struct tag_aif_comm {
    int16_t     channels;
    uint32_t    sample_frames;
    int16_t     sample_size;
    uint8_t     sample_rate[10];
} _PACKED AIF_COMM;
#pragma pack()

uint32_t aif_from_be32(uint32_t *native) {
    uint32_t result;
    uint8_t *data = (uint8_t *)native;

    result = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    return result;
}

uint16_t aif_from_be16(uint16_t *native) {
    uint16_t result;
    uint8_t *data = (uint8_t *)native;

    result = data[0] << 8 | data[1];
    return result;
}


/**
 * parse a COMM block -- should be sitting at the bottom half of
 * a comm block
 *
 * @param infile file to read
 * @return TRUE on success, FALSE otherwise
 */
int scan_aif_parse_comm(IOHANDLE hfile, MP3FILE *pmp3) {
    AIF_COMM comm;
    int sec;
    int ms;
    uint32_t len;
    
    len = sizeof(AIF_COMM);
    if(!io_read(hfile,(unsigned char *)&comm, &len) || !len) {
        DPRINTF(E_WARN,L_SCAN,"Error reading aiff file -- bad COMM block\n");
        return FALSE;
    }

    /* we'll brute the sample rate */
    pmp3->samplerate = aif_from_be32((uint32_t*)&comm.sample_rate[2]) >> 16;
    if(!pmp3->samplerate)
        return TRUE;

    pmp3->bitrate = pmp3->samplerate * comm.channels *
        ((comm.sample_size + 7)/8)*8;

    sec = (int)(pmp3->file_size / (pmp3->bitrate / 8));
    ms = (int)(((pmp3->file_size % (pmp3->bitrate / 8)) * 1000) / (pmp3->bitrate/8));
    pmp3->song_length = (sec * 1000) + ms;

    pmp3->bitrate /= 1000;

    return TRUE;
}

/**
 * Get info from the actual aiff headers.  Since there is no
 * metainfo in .wav files (or at least know standard I
 * know about), this merely gets duration, bitrate, etc.
 *
 * @param filename file to scan
 * @param pmp3 MP3FILE struct to be filled
 * @returns TRUE if song should be added to database, FALSE otherwise
 */
int scan_get_aifinfo(char *filename, MP3FILE *pmp3) {
    IOHANDLE hfile;
    int done=0;
    AIF_CHUNK_HEADER chunk;
    AIF_IFF_HEADER iff_header;
    uint64_t current_pos = 0;
    uint32_t len;

    DPRINTF(E_DBG,L_SCAN,"Getting AIFF file info\n");

    hfile = io_new();
    if(!hfile) {
        DPRINTF(E_WARN,L_SCAN,"Error allocating file handle\n");
        return FALSE;
    }
    
    if(!io_open(hfile,"file://%U",filename)) {
        DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading: %s\n",filename,
            io_errstr(hfile));
        io_dispose(hfile);
        return FALSE;
    }

    /* first, verify we have a valid iff header */
    len = sizeof(AIF_IFF_HEADER);
    if(!io_read(hfile,(unsigned char *)&iff_header,&len) || !len) {
        DPRINTF(E_WARN,L_SCAN,"Error reading %s -- bad iff header\n",filename);
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    if((strncmp(iff_header.id,"FORM",4) != 0) ||
       (strncmp(iff_header.type,"AIFF",4) != 0)) {
        DPRINTF(E_WARN,L_SCAN,"File %s is not an AIFF file\n",filename);
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    /* loop around, processing chunks */
    while(!done) {
        len = sizeof(AIF_CHUNK_HEADER);
        if(!io_read(hfile, (unsigned char *)&chunk, &len) || !len) {
            done=1;
            break;
        }

        /* fixup */
        chunk.len = aif_from_be32(&chunk.len);

        DPRINTF(E_DBG,L_SCAN,"Got chunk %c%c%c%c\n",chunk.id[0],
                chunk.id[1],chunk.id[2],chunk.id[3]);

        io_getpos(hfile,&current_pos);

        /* process the chunk */
        if(strncmp(chunk.id,"COMM",4)==0) {
            if(!scan_aif_parse_comm(hfile,pmp3)) {
                DPRINTF(E_INF,L_SCAN,"Error reading COMM block: %s\n",filename);
                io_close(hfile);
                io_dispose(hfile);
                return FALSE;
            }
        }

        io_setpos(hfile, current_pos, SEEK_SET);
        io_setpos(hfile, chunk.len, SEEK_CUR);
    }

    io_close(hfile);
    io_dispose(hfile);
    return TRUE;
}

