/*
 * $Id: scan-wav.c 1689 2007-10-23 03:57:20Z rpedde $
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

#include <ctype.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "daapd.h"
#include "err.h"
#include "io.h"
#include "mp3-scanner.h"

#define GET_WAV_INT32(p) ((((uint32_t)((p)[3])) << 24) |   \
                          (((uint32_t)((p)[2])) << 16) |   \
                          (((uint32_t)((p)[1])) << 8) |    \
                          (((uint32_t)((p)[0]))))

#define GET_WAV_INT16(p) ((((uint32_t)((p)[1])) << 8) |    \
                          (((uint32_t)((p)[0]))))

#define XLAT(c) (((toupper((c)) >= 'A') && (toupper((c)) <= 'Z')) ? (c) : '_' )

/**
 * Get info from the actual wav headers.  Since there is no
 * metainfo in .wav files (or at least know standard I
 * know about), this merely gets duration, bitrate, etc.
 *
 * @param filename file to scan
 * @param pmp3 MP3FILE struct to be filled
 * @returns TRUE if song should be added to database, FALSE otherwise
 */
int scan_get_wavinfo(char *filename, MP3FILE *pmp3) {
    IOHANDLE hfile;
    uint32_t len;
    unsigned char hdr[12];
    unsigned char fmt[16];
    uint32_t chunk_data_length;
    uint32_t format_data_length = 0;
    uint32_t compression_code = 0;
    uint32_t channel_count = 0;
    uint32_t sample_rate = 0;
    uint32_t sample_bit_length = 0;
    uint32_t bit_rate;
    uint32_t data_length = 0;
    uint32_t sec, ms;

    uint32_t current_offset;
    uint32_t block_len;

    int found_fmt = FALSE;
    int found_data = FALSE;

    DPRINTF(E_DBG,L_SCAN,"Getting WAV file info\n");

    if(!(hfile = io_new())) {
        DPRINTF(E_LOG,L_SCAN,"Could not create file handle\n");
        return FALSE;
    }

    if(!io_open(hfile,"file://%U",filename)) {
        DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading: %s\n",filename,
            io_errstr(hfile));
        io_dispose(hfile);
        return FALSE;
    }

    len = 12;
    if(!io_read(hfile,hdr,&len) || (len != 12)) {
        DPRINTF(E_WARN,L_SCAN,"Could not read wav header from %s\n",filename);
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    /* I've found some wav files that have INFO tags
     * in them... */
    if (strncmp((char*)hdr + 0, "RIFF", 4) ||
        strncmp((char*)hdr + 8, "WAVE", 4)) {
        DPRINTF(E_WARN,L_SCAN,"Invalid wav header in %s\n",filename);
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    chunk_data_length = GET_WAV_INT32(hdr + 4);

    /* now, walk through the chunks */
    current_offset = 12;

    while(!found_fmt || !found_data) {
        len = 8;
        if(!io_read(hfile,hdr,&len) || (len != 8)) {
            io_close(hfile);
            io_dispose(hfile);
            DPRINTF(E_WARN,L_SCAN,"Error reading block: %s\n",filename);
            return FALSE;
        }

        block_len = GET_WAV_INT32(hdr + 4);

        DPRINTF(E_DBG,L_SCAN,"Read block %02x%02x%02x%02x (%c%c%c%c) of "
                "size %08x\n",hdr[0],hdr[1],hdr[2],hdr[3],
                XLAT(hdr[0]),XLAT(hdr[1]),XLAT(hdr[2]),XLAT(hdr[3]),block_len);

        if(block_len < 0) {
            io_close(hfile);
            io_dispose(hfile);
            DPRINTF(E_WARN,L_SCAN,"Bad block len: %s\n",filename);
            return FALSE;
        }

        if(strncmp((char*)&hdr,"fmt ",4) == 0) {
            found_fmt = TRUE;
            DPRINTF(E_DBG,L_SCAN,"Found 'fmt ' header\n");
            len = 16;
            if(!io_read(hfile,fmt,&len) || (len != 16)) {
                io_close(hfile);
                io_dispose(hfile);
                DPRINTF(E_WARN,L_SCAN,"Bad .wav file: can't read fmt: %s\n",
                        filename);
                return FALSE;
            }

            format_data_length = block_len;
            compression_code = GET_WAV_INT16(fmt);
            channel_count = GET_WAV_INT16(fmt+2);
            sample_rate = GET_WAV_INT32(fmt + 4);
            sample_bit_length = GET_WAV_INT16(fmt + 14);
            DPRINTF(E_DBG,L_SCAN,"Compression code: %d\n",compression_code);
            DPRINTF(E_DBG,L_SCAN,"Channel count:    %d\n",channel_count);
            DPRINTF(E_DBG,L_SCAN,"Sample Rate:      %d\n",sample_rate);
            DPRINTF(E_DBG,L_SCAN,"Sample bit length %d\n",sample_bit_length);

        } else if (strncmp((char*)&hdr,"data",4) == 0) {
            DPRINTF(E_DBG,L_SCAN,"Found 'data' header\n");
            data_length = block_len;
            found_data = TRUE;
        }

        io_setpos(hfile,current_offset + block_len + 8,SEEK_SET);
        current_offset += (block_len + 8);
    }

    io_close(hfile);
    io_dispose(hfile);

    if (((format_data_length != 16) && (format_data_length != 18)) ||
        (compression_code != 1) ||
        (channel_count < 1)) {
        DPRINTF(E_WARN,L_SCAN,"Invalid wav header in %s\n",filename);
        return FALSE;
    }

    bit_rate = sample_rate * channel_count * ((sample_bit_length + 7) / 8) * 8;
    if(!bit_rate) {
        DPRINTF(E_WARN,L_SCAN,"Couldn't get bitrate\n");
        return FALSE;
    }

    pmp3->bitrate = bit_rate / 1000;
    pmp3->samplerate = sample_rate;
    sec = data_length / (bit_rate / 8);
    ms = ((data_length % (bit_rate / 8)) * 1000) / (bit_rate / 8);
    pmp3->song_length = (sec * 1000) + ms;

    return TRUE;
}

