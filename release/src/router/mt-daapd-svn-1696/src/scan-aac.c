/*
 * $Id: scan-aac.c 1671 2007-09-25 07:50:48Z rpedde $
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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifndef WIN32
#include <netinet/in.h>
#endif

#include "daapd.h"
#include "err.h"
#include "io.h"
#include "mp3-scanner.h"
#include "scan-aac.h"

/* Forwards */
time_t scan_aac_mac_to_unix_time(int t);

/**
 * Convert mac time to unix time (different epochs)
 *
 * @param t time since mac epoch
 * @returns time since unix epoch
 */
time_t scan_aac_mac_to_unix_time(int t) {
  struct timeval        tv;
  struct timezone       tz;

  gettimeofday(&tv, &tz);

  return (t - (365L * 66L * 24L * 60L * 60L + 17L * 60L * 60L * 24L) +
          (tz.tz_minuteswest * 60));
}

/**
 * Returns the offset of the atom specified by the given path or -1 if
 * not found. atom_path is a colon separated list of atoms specifying
 * a path from parent node to the target node. All paths must be specified
 * from the root.
 *
 * @param aac_fp the aac file being searched
 * @param atom_path the colon separated list of atoms
 * @param atom_length the size of the atom "drilled to"
 * @returns offset of the atom, or -1 if unsuccessful
 */
uint64_t scan_aac_drilltoatom(IOHANDLE hfile,char *atom_path,
                           unsigned int *atom_length) {
    uint64_t      atom_offset;
    uint64_t      file_size,pos;
    char          *cur_p, *end_p;
    char          atom_name[5];

    DPRINTF(E_SPAM,L_SCAN,"Searching for %s\n",atom_path);

    // FIXME: cache io_size for static files
    io_size(hfile, &file_size);
    io_setpos(hfile,0,SEEK_SET);

    end_p = atom_path;
    while (*end_p != '\0') {
            end_p++;
    }
    atom_name[4] = '\0';
    cur_p = atom_path;

    while (cur_p != NULL) {
        if ((end_p - cur_p) < 4) {
            return -1;
        }
        strncpy(atom_name, cur_p, 4);
        atom_offset = scan_aac_findatom(hfile, file_size,
                                        atom_name, atom_length);
        if (atom_offset == -1) {
            return -1;
        }

        io_getpos(hfile,&pos);
        DPRINTF(E_SPAM,L_SCAN,"Found %s atom at off %lld.\n",
                atom_name, pos - 8);

        cur_p = strchr(cur_p, ':');
        if (cur_p != NULL) {
            cur_p++;

            /* FIXME
             * Hack to deal with atoms that have extra data in addition
             * to having child atoms. This should be dealt in a better fashion
             * than this (table with skip offsets or a real mp4 parser.) */
            if (!strcmp(atom_name, "meta")) {
                io_setpos(hfile, 4, SEEK_CUR);
            } else if (!strcmp(atom_name, "stsd")) {
                io_setpos(hfile, 8, SEEK_CUR);
            } else if (!strcmp(atom_name, "mp4a")) {
                io_setpos(hfile, 28, SEEK_CUR);
            }
        }
    }

    io_getpos(hfile, &pos);
    return pos - 8;
}


/* FIXME: return TRUE/FALSE */

/**
 * Given a file, search for a particular aac atom.
 *
 * @param fin file handle of the open aac file
 * @param max_offset how far to search (probably size of container atom)
 * @param which_atom what atom name we are searching for
 * @param atom_size this will hold the size of the atom found
 */
uint64_t scan_aac_findatom(IOHANDLE hfile, uint64_t max_offset,
                           char *which_atom, unsigned int *atom_size) {
    uint64_t current_offset=0;
    uint32_t size;
    char atom[4];
    uint32_t bytes_read;

    while((current_offset + 8) < max_offset) {
        bytes_read = sizeof(uint32_t);
        if(!io_read(hfile,(unsigned char *)&size,&bytes_read) || (!bytes_read)) {
            return -1;
        }

        size=ntohl(size);

        if(size <= 7) { /* something not right */
            DPRINTF(E_LOG,L_SCAN,"Bad aac file: atom length too short searching for %s\n",
                    which_atom);
            return -1;
        }

        bytes_read = 4;
        if(!io_read(hfile,(unsigned char *)atom,&bytes_read) || (!bytes_read)) {
            return -1;
        }

        if(strncasecmp(atom,which_atom,4) == 0) {
            *atom_size=size;
            return current_offset;
        }

        io_setpos(hfile,size-8,SEEK_CUR);
        current_offset+=size;
    }

    DPRINTF(E_SPAM,L_SCAN,"Couldn't find atom %s as requested\n",which_atom);
    return -1;
}

/**
 * main aac scanning routing.
 *
 * @param filename file to scan
 * @param pmp3 pointer to the MP3FILE to fill with data
 * @returns FALSE if file should not be added to database, TRUE otherwise
 */
int scan_get_aacinfo(char *filename, MP3FILE *pmp3) {
    IOHANDLE hfile;
    uint64_t atom_offset, pos;
    uint32_t bytes_read;
    unsigned int atom_length;

    long current_offset=0;
    uint32_t current_size;
    char current_atom[4];
    char *current_data;
    unsigned short us_data;
    int genre;
    int len;

    uint32_t sample_size;
    uint32_t samples;
    uint32_t bit_rate;
    int ms;
    unsigned char buffer[2];
    uint32_t time = 0;


    hfile = io_new();
    if(!hfile)
        DPRINTF(E_FATAL,L_SCAN,"Malloc error in scan_get_aacinfo\n");

    if(!io_open(hfile,"file://%U",filename)) {
        DPRINTF(E_INF,L_SCAN,"Cannot open file %s for reading: %s\n",filename,
            io_errstr(hfile));
        io_dispose(hfile);
        return FALSE;
    }

    atom_offset=scan_aac_drilltoatom(hfile, "moov:udta:meta:ilst", &atom_length);
    if(atom_offset != -1) {
        /* found the tag section - need to walk through now */
        while(current_offset < (uint64_t)atom_length) {
            bytes_read =  sizeof(uint32_t);
            if(!io_read(hfile,(unsigned char *)&current_size,&bytes_read) || !bytes_read) {
                DPRINTF(E_LOG,L_SCAN,"Error reading mp4 atoms: %s\n",io_errstr(hfile));
                io_dispose(hfile);
                return FALSE;
            }

            current_size=ntohl(current_size);

            DPRINTF(E_SPAM,L_SCAN,"Current size: %d\n",current_size);

            if(current_size <= 7) { /* something not right */
                DPRINTF(E_LOG,L_SCAN,"mp4 atom too small. Bad aac tags?\n");
                io_dispose(hfile);
                return FALSE;
            }


            bytes_read = 4;
            if(!io_read(hfile,(unsigned char *)current_atom,&bytes_read) || !bytes_read) {
                DPRINTF(E_LOG,L_SCAN,"Error reading mp4 atoms: %s\n",io_errstr(hfile));
                io_dispose(hfile);
                return FALSE;
            }

            DPRINTF(E_SPAM,L_SCAN,"Current Atom: %c%c%c%c\n",
                    current_atom[0],current_atom[1],current_atom[2],
                    current_atom[3]);

            if(current_size > 4096) { /* Does this break anything? */
                /* too big!  cover art, maybe? */
                io_setpos(hfile,current_size - 8, SEEK_CUR);
                DPRINTF(E_SPAM,L_SCAN,"Atom too big... skipping\n");
            } else {
                len=current_size-7;  /* for ill-formed too-short tags */
                if(len < 22) {
                    len=22;
                }

                current_data=(char*)malloc(len);  /* extra byte */
                memset(current_data,0x00,len);

                bytes_read = current_size - 8;
                if(!io_read(hfile,(unsigned char *)current_data,&bytes_read) || (!bytes_read)) {
                    DPRINTF(E_LOG,L_SCAN,"Error reading mp4 data: %s\n",io_errstr(hfile));
                    free(current_data);
                    io_dispose(hfile);
                    return FALSE;
                }

                if(!memcmp(current_atom,"\xA9" "nam",4)) { /* Song name */
                    pmp3->title=strdup((char*)&current_data[16]);
                } else if(!memcmp(current_atom,"aART",4)) {
                    pmp3->album_artist=strdup((char*)&current_data[16]);
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
            }
            current_offset+=current_size;
        }
    }

    /* got the tag info, now let's get bitrate, etc */
    atom_offset = scan_aac_drilltoatom(hfile, "moov:mvhd", &atom_length);
    if(atom_offset != -1) {
        io_setpos(hfile,4,SEEK_CUR);

        /* FIXME: error handling */
        bytes_read = sizeof(uint32_t);
        if(!io_read(hfile,(unsigned char *)&time, &bytes_read)) {
            DPRINTF(E_LOG,L_SCAN,"Error reading time from moov:mvhd: %s\n",
                    io_errstr(hfile));
            io_dispose(hfile);
            return FALSE;
        }

        time = ntohl(time);
        pmp3->time_added = (int)scan_aac_mac_to_unix_time(time);

        bytes_read = sizeof(uint32_t);
        if(!io_read(hfile,(unsigned char *)&time, &bytes_read)) {
            DPRINTF(E_LOG,L_SCAN,"Error reading time from moov:mvhd: %s\n",
                    io_errstr(hfile));
            io_dispose(hfile);
            return FALSE;
        }

        time = ntohl(time);
        pmp3->time_modified = (int)scan_aac_mac_to_unix_time(time);

        bytes_read = sizeof(uint32_t);
        if(!io_read(hfile,(unsigned char *)&sample_size,&bytes_read)) {
            DPRINTF(E_LOG,L_SCAN,"Error reading sample_size from moov:mvhd: %s\n",
                    io_errstr(hfile));
            io_dispose(hfile);
            return FALSE;
        }

        bytes_read = sizeof(uint32_t);
        if(!io_read(hfile,(unsigned char*)&samples, &bytes_read)) {
            DPRINTF(E_LOG,L_SCAN,"Error reading samples from moov:mvhd: %s\n",
                    io_errstr(hfile));
            io_dispose(hfile);
            return FALSE;
        }

        sample_size=ntohl(sample_size);
        samples=ntohl(samples);

        /* avoid overflowing on large sample_sizes (90000) */
        ms=1000;
        while((ms > 9) && (!(sample_size % 10))) {
            sample_size /= 10;
            ms /= 10;
        }

        /* DWB: use ms time instead of sec */
        pmp3->song_length=(uint32_t)((samples * ms) / sample_size);
        DPRINTF(E_DBG,L_SCAN,"Song length: %d seconds\n",
                pmp3->song_length / 1000);
    }

    pmp3->bitrate = 0;

    /* see if it is aac or alac */
    atom_offset = scan_aac_drilltoatom(hfile,
                                       "moov:trak:mdia:minf:stbl:stsd:alac",
                                       &atom_length);

    if(atom_offset != -1) {
        /* should we still pull samplerate, etc from the this atom? */
        if(pmp3->codectype) {
            free(pmp3->codectype);
        }
        pmp3->codectype=strdup("alac");
    }

    /* Get the sample rate from the 'mp4a' atom (timescale). This is also
       found in the 'mdhd' atom which is a bit closer but we need to
       navigate to the 'mp4a' atom anyways to get to the 'esds' atom. */
    atom_offset=scan_aac_drilltoatom(hfile,
                                     "moov:trak:mdia:minf:stbl:stsd:mp4a",
                                     &atom_length);
    if(atom_offset == -1) {
        atom_offset=scan_aac_drilltoatom(hfile,
                                         "moov:trak:mdia:minf:stbl:stsd:drms",
                                         &atom_length);
    }

    if (atom_offset != -1) {
        io_setpos(hfile, atom_offset + 32, SEEK_SET);

        /* Timescale here seems to be 2 bytes here (the 2 bytes before it are
         * "reserved") though the timescale in the 'mdhd' atom is 4. Not sure
         * how this is dealt with when sample rate goes higher than 64K. */
        bytes_read = 2;
        if(!io_read(hfile, (unsigned char *)buffer, &bytes_read)) {
            DPRINTF(E_LOG,L_SCAN,"Error reading timescale from drms atom: %s\n",
                    io_errstr(hfile));
            io_dispose(hfile);
            return FALSE;
        }

        pmp3->samplerate = (buffer[0] << 8) | (buffer[1]);

        /* Seek to end of atom. */
        io_setpos(hfile, 2, SEEK_CUR);

        /* Get the bit rate from the 'esds' atom. We are already positioned
           in the parent atom so just scan ahead. */
        io_getpos(hfile,&pos);
        atom_offset = scan_aac_findatom(hfile,
                                        atom_length-(pos-atom_offset),
                                        "esds", &atom_length);

        if (atom_offset != -1) {
            /* Roku Soundbridge seems to believe anything above 320K is
             * an ALAC encoded m4a.  We'll lie on their behalf.
             */
            io_setpos(hfile, atom_offset + 22, SEEK_CUR);

            bytes_read = sizeof(unsigned int);
            if(!io_read(hfile, (unsigned char *)&bit_rate, &bytes_read)) {
                DPRINTF(E_LOG,L_SCAN,"Error reading bitrate from esds: %s\n",
                        io_errstr(hfile));
                io_dispose(hfile);
                return FALSE;
            }

            pmp3->bitrate = ntohl(bit_rate) / 1000;
            DPRINTF(E_DBG,L_SCAN,"esds bitrate: %d\n",pmp3->bitrate);

            if(pmp3->bitrate > 320) {
                pmp3->bitrate = 320;
            }
        } else {
            DPRINTF(E_DBG,L_SCAN, "Couldn't find 'esds' atom for bit rate.\n");
        }
    } else {
        DPRINTF(E_DBG,L_SCAN, "Couldn't find 'mp4a' atom for sample rate.\n");
    }

    /* Fallback if we can't find the info in the atoms. */
    if (pmp3->bitrate == 0) {
        /* calculate bitrate from song length... Kinda cheesy */
        DPRINTF(E_DBG,L_SCAN, "Guesstimating bit rate.\n");
        atom_offset=scan_aac_drilltoatom(hfile,"mdat",&atom_length);
        if ((atom_offset != -1) && (pmp3->song_length >= 1000)) {
            pmp3->bitrate = atom_length / ((pmp3->song_length / 1000) * 128);
        }
    }

    io_close(hfile);
    io_dispose(hfile);
    return TRUE;  /* we'll return as much as we got. */
}
