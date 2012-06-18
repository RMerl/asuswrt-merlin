/*
 * $Id: dynamic-art.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Dynamically merge .jpeg data into an mp3 tag
 *
 * Copyright (C) 2004 Hiren Joshi (hirenj@mooh.org)
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


#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "configfile.h"
#include "err.h"
#include "playlist.h"
#include "restart.h"

#define BLKSIZE PIPE_BUF

int *da_get_current_tag_info(int file_fd);

/* For some reason, we need to lose 2 bytes from this image size
   This size is everything after the APIC text in this frame.
*/
/* APIC tag is made up from
   APIC		(4 bytes)
   xxxx		(4 bytes - Image size: raw + 16)
   \0\0\0		(3 bytes)
   image/jpeg	(10 bytes)
   \0\0\0		(3 bytes)
   Image-data
*/
#define id3v3_image_size(x) x + 14


/* This size is everything after the PIC in this frame
 */
/* PIC tag is made up of
   PIC			(3 bytes)
   xxx			(3 bytes - Image size: raw + 6)
   \0			(1 byte)
   JPG			(3 bytes)
   \0\0		(2 bytes)
   Image-data
*/

#define id3v2_image_size(x) x + 6


/* For some reason we need to fudge values - Why do we need 17 bytes here? */
#define id3v3_tag_size(x) id3v3_image_size(x) + 8
#define id3v2_tag_size(x) id3v2_image_size(x) + 6

/*
 * get_image_fd
 *
 * Get a file descriptor for a piece of cover art. 
 */
int da_get_image_fd(char *filename) {
    unsigned char buffer[255];
    char *path_end;
    int fd;
    strncpy(buffer,filename,255);
    path_end = strrchr(buffer,'/');
    strcpy(path_end+1,config.artfilename);
    fd = open(buffer,O_RDONLY);
    if(fd != -1) 
	DPRINTF(E_INF,L_ART,"Found image file %s (fd %d)\n",buffer,fd);

    return fd;
}

/*
 * get_current_tag_info
 *
 * Get the current tag from the file. We need to do this to determine whether we 
 * are dealing with an id3v2.2 or id3v2.3 tag so we can decide which artwork to 
 * attach to the song
 */
int *da_get_current_tag_info(int file_fd) {
    unsigned char buffer[10];
    int *tag_info;
	
    tag_info = (int *) calloc(2,sizeof(int));
	
    r_read(file_fd,buffer,10);
    if ( strncmp(buffer,"ID3", 3) == 0 ) {
	tag_info[0] = buffer[3];
	tag_info[1] = ( buffer[6] << 21 ) + ( buffer[7] << 14 ) + ( buffer[8] << 7 ) + buffer[9];
	return tag_info;
    } else {
	/* By default, we attach an id3v2.3 tag */
	lseek(file_fd,0,SEEK_SET);
	tag_info[0] = 2;
	tag_info[1] = 0;
	return tag_info;
    }
}

/*
 * attach_image
 *
 * Given an image, output and input mp3 file descriptor, attach the artwork found
 * at the image file descriptor to the mp3 and stream the new id3 header to the client
 * via the output file descriptor. If there is not id3 tag present, it will attach a tag
 * to contain the artwork.
 */
int da_attach_image(int img_fd, int out_fd, int mp3_fd, int offset)
{
    long img_size;
    int tag_size;
    int *tag_info;
    unsigned char buffer[4];
    struct stat sb;

    fstat(img_fd,&sb);
    img_size=sb.st_size;

    DPRINTF(E_INF,L_ART,"Image appears to be %ld bytes\n",img_size);
    if(img_size < 1) {
	r_close(img_fd);
	return 0;
    }

    if (offset > (img_size + 24) ) {
	lseek(mp3_fd,(offset - img_size - 24),SEEK_SET);
	r_close(img_fd);
	return 0;
    }

    tag_info = da_get_current_tag_info(mp3_fd);
    tag_size = tag_info[1];

    DPRINTF(E_INF,L_ART,"Current tag size is %d bytes\n",tag_size);

    if (tag_info[0] == 3) {
	r_write(out_fd,"ID3\x03\0\0",6);
	tag_size += id3v3_tag_size(img_size);
    } else {
	r_write(out_fd,"ID3\x02\0\0",6);
	tag_size += id3v2_tag_size(img_size);
    }

    buffer[3] = tag_size & 0x7F;
    buffer[2] = ( tag_size & 0x3F80 ) >> 7;
    buffer[1] = ( tag_size & 0x1FC000 ) >> 14;
    buffer[0] = ( tag_size & 0xFE00000 ) >> 21;
	
    r_write(out_fd,buffer,4);
	
    if (tag_info[0] == 3) {
	r_write(out_fd,"APIC\0",5);
	img_size = id3v3_image_size(img_size);
    } else {
	r_write(out_fd,"PIC",3);
	img_size = id3v2_image_size(img_size);
    }
    buffer[0] = ( img_size & 0xFF0000 ) >> 16;
    buffer[1] = ( img_size & 0xFF00 ) >> 8;
    buffer[2] = ( img_size & 0xFF );
    r_write(out_fd,buffer,3);
    if (tag_info[0] == 3) {
	r_write(out_fd,"\0\0\0image/jpeg\0\0\0",16);
    } else {
	r_write(out_fd,"\0JPG\x00\0",6);
    }
    lseek(img_fd,0,SEEK_SET);
    copyfile(img_fd,out_fd);
    DPRINTF(E_INF,L_ART,"Done copying IMG %ld\n",img_size);
    r_close(img_fd);
    free(tag_info);
    return 0;
}


/**
 * Rewrites the stco atom. It goes through the chunk offset atom list
 * and adjusts them to take into account the 'covr' atom.
 * extra_size is the amount to adjust by.
 */
off_t da_aac_rewrite_stco_atom(off_t extra_size, int out_fd, FILE *aac_fp,
                               off_t last_pos)
{
    int           aac_fd;
    struct stat   sb;
    unsigned char buffer[4];
    off_t         file_size;
    int           atom_offset;
    int           atom_length;
    off_t         cur_pos;
    off_t         old_pos;
    int           i;
    unsigned int  num_entries;
    unsigned int  offset_entry;

    aac_fd = fileno(aac_fp);

    fstat(aac_fd, &sb);
    file_size = sb.st_size;

    /* Drill down to the 'stco' atom which contains offsets to chunks in
       the 'mdat' section. These offsets need to be readjusted. */
    atom_offset = aac_drilltoatom(aac_fp, "moov:trak:mdia:minf:stbl:stco",
				  &atom_length);
    if (atom_offset != -1) {
	/* Skip flags */
	fseek(aac_fp, 4, SEEK_CUR);

	old_pos = last_pos;
	cur_pos = ftell(aac_fp);

	/* Copy from last point to this point. */
	fseek(aac_fp, old_pos, SEEK_SET);
	fcopyblock(aac_fp, out_fd, cur_pos - old_pos);

	/* Read number of entries */
	fread(buffer, 1, 4, aac_fp);
	r_write(out_fd, buffer, 4);

	num_entries = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];

	DPRINTF(E_DBG, L_ART,"Readjusting %d 'stco' table offsets.\n", num_entries);
	/* PENDING: Error check on num_entries? */
	for (i = 0; i < num_entries; i++) {
	    fread(buffer, 1, 4, aac_fp);
	    offset_entry = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	    /* Adjust chunk offset. */
	    offset_entry += extra_size;
	    buffer[3] = offset_entry & 0xFF;
	    buffer[2] = (offset_entry >> 8) & 0xFF;
	    buffer[1] = (offset_entry >> 16) & 0xFF;
	    buffer[0] = (offset_entry >> 24) & 0xFF;
	    r_write(out_fd, buffer, 4);
	    offset_entry = 0;
	}
	return ftell(aac_fp);
    } else {
	DPRINTF(E_LOG, L_ART,"No 'stco' atom found.\n");
    }
    return last_pos;
}

/**
 * Insert a 'covr' atom.
 * extra_size is the size of the atom used to adjust all parent atoms.
 */
off_t da_aac_insert_covr_atom(off_t extra_size, int out_fd, FILE *aac_fp,
                              off_t last_pos, off_t file_size, int img_fd)
{
    int           aac_fd;
    struct stat   sb;
    off_t         old_pos;
    unsigned char buffer[4];
    int           atom_offset;
    int           atom_length;
    off_t         cur_pos;
    char          *cp;
    unsigned char img_type_flag = 0;

    /* Figure out image file type since this needs to be encoded in the atom. */
    cp = strrchr(config.artfilename, '.');
    if (cp) {
	if (!strcasecmp(cp, ".jpeg") || !strcasecmp(cp, ".jpg")) {
	    img_type_flag = 0x0d;
	}
	else if (!strcasecmp(cp, ".png")) {
	    img_type_flag = 0x0e;
	} else {
	    DPRINTF(E_LOG,L_ART, "Image type '%s' not supported.\n", cp);
	    return 0;
	}
    } else {
	DPRINTF(E_LOG, L_ART, "No file extension for image file.\n");
    }

    aac_fd = fileno(aac_fp);
    fstat(aac_fd, &sb);
    file_size = sb.st_size;
    rewind(aac_fp);

    atom_offset = scan_aac_findatom(aac_fp, file_size, "moov", &atom_length);
    if (atom_offset != -1) {
	atom_offset = scan_aac_findatom(aac_fp, atom_length - 8, "udta", &atom_length);
	if (atom_offset != -1) {
	    old_pos = last_pos;
	    cur_pos = ftell(aac_fp) - 8;
	    DPRINTF(E_INF,L_ART,"Found udta atom at %ld.\n", cur_pos);
	    fseek(aac_fp, old_pos, SEEK_SET);
	    fcopyblock(aac_fp, out_fd, cur_pos - old_pos);

	    /* Write out new length */
	    atom_length += extra_size;
	    buffer[3] = atom_length & 0xFF;
	    buffer[2] = ( atom_length >> 8 ) & 0xFF;
	    buffer[1] = ( atom_length >> 16 ) & 0xFF;
	    buffer[0] = ( atom_length >> 24 ) & 0xFF;
	    r_write(out_fd, buffer, 4);

	    cur_pos += 4;
	    fseek(aac_fp, 8, SEEK_CUR);

	    atom_offset = scan_aac_findatom(aac_fp, atom_length - 8, "meta", &atom_length);
	    if (atom_offset != -1) {
		old_pos = cur_pos;
		cur_pos = ftell(aac_fp) - 8;
		DPRINTF(E_INF,L_ART,"Found meta atom at %ld.\n", cur_pos);
		fseek(aac_fp, old_pos, SEEK_SET);
		fcopyblock(aac_fp, out_fd, cur_pos - old_pos);

		/* Write out new length */
		atom_length += extra_size;
		buffer[3] = atom_length & 0xFF;
		buffer[2] = ( atom_length >> 8 ) & 0xFF;
		buffer[1] = ( atom_length >> 16 ) & 0xFF;
		buffer[0] = ( atom_length >> 24 ) & 0xFF;
		r_write(out_fd, buffer, 4);

		cur_pos += 4;
		fseek(aac_fp, 12, SEEK_CUR); /* "meta" atom hack. */

		atom_offset = scan_aac_findatom(aac_fp, atom_length - 8, "ilst", &atom_length);
		if (atom_offset != -1) {
		    old_pos = cur_pos;
		    cur_pos = ftell(aac_fp) - 8;
		    DPRINTF(E_INF,L_ART,"Found ilst atom at %ld.\n", cur_pos);
		    fseek(aac_fp, old_pos, SEEK_SET);
		    fcopyblock(aac_fp, out_fd, cur_pos - old_pos);

		    old_pos = cur_pos + 4;
		    cur_pos += atom_length;

		    /* Write out new length */
		    atom_length += extra_size;
		    buffer[3] = atom_length & 0xFF;
		    buffer[2] = ( atom_length >> 8 ) & 0xFF;
		    buffer[1] = ( atom_length >> 16 ) & 0xFF;
		    buffer[0] = ( atom_length >> 24 ) & 0xFF;
		    r_write(out_fd, buffer, 4);

		    /* Copy all 'ilst' children (all the MP4 'tags'). We will append
		       at the end. */
		    fseek(aac_fp, old_pos, SEEK_SET);
		    fcopyblock(aac_fp, out_fd, cur_pos - old_pos);
		    cur_pos = ftell(aac_fp);

		    /* Write out 'covr' atom */
		    atom_length = extra_size;
		    buffer[3] = atom_length & 0xFF;
		    buffer[2] = ( atom_length >> 8 ) & 0xFF;
		    buffer[1] = ( atom_length >> 16 ) & 0xFF;
		    buffer[0] = ( atom_length >> 24 ) & 0xFF;
		    r_write(out_fd, buffer, 4);
            
		    r_write(out_fd, "covr", 4);

		    /* Write out 'data' atom */
		    atom_length = extra_size - 8;
		    buffer[3] = atom_length & 0xFF;
		    buffer[2] = ( atom_length >> 8 ) & 0xFF;
		    buffer[1] = ( atom_length >> 16 ) & 0xFF;
		    buffer[0] = ( atom_length >> 24 ) & 0xFF;
		    r_write(out_fd, buffer, 4);

		    r_write(out_fd, "data", 4);

		    /* Write out 'data' flags */
		    buffer[3] = img_type_flag;
		    buffer[2] = 0;
		    buffer[1] = 0;
		    buffer[0] = 0;
		    r_write(out_fd, buffer, 4);

		    /* Reserved? Zero in any case. */
		    buffer[3] = 0;
		    buffer[2] = 0;
		    buffer[1] = 0;
		    buffer[0] = 0;

		    r_write(out_fd, buffer, 4);

		    /* Now ready for the image stream. Copy it over. */
		    lseek(img_fd,0,SEEK_SET);
		    copyfile(img_fd,out_fd);
		    last_pos = cur_pos;
		} else {
		    DPRINTF(E_LOG, L_ART,"No 'ilst' atom found.\n");
		}
	    } else {
		DPRINTF(E_LOG,L_ART, "No 'meta' atom found.\n");
	    }
	} else {
	    DPRINTF(E_LOG,L_ART, "No 'udta' atom found.\n");
	}
    } else {
	DPRINTF(E_LOG,L_ART, "No 'moov' atom found.\n");
    }

    /* Seek to position right after 'udta' atom. Let main() stream out the
       rest. */
    lseek(aac_fd, last_pos, SEEK_SET);

    return last_pos;
}

/*
 * attach_image
 *
 * Given an image, output and input aac file descriptor, attach the artwork
 * found at the image file descriptor to the aac and stream the new atom to
 * the client via the output file descriptor.
 * Currently, if the meta info atoms are non-existent, this will fail.
 */
off_t da_aac_attach_image(int img_fd, int out_fd, int aac_fd, int offset)
{
    off_t         img_size;
    int           atom_length;
    unsigned int  extra_size;
    off_t         file_size;
    unsigned char buffer[4];
    struct stat   sb;
    FILE          *aac_fp;
    off_t         stco_atom_pos;
    off_t         ilst_atom_pos;
    off_t         last_pos;

    fstat(img_fd, &sb);
    img_size = sb.st_size;

    DPRINTF(E_INF,L_ART,"Image size (in bytes): %ld.\n", img_size);

    /* PENDING: We can be stricter here by checking the shortest header between
       PNG and JPG and using that length. */
    if (img_size < 1) {
	r_close(img_fd);
	return 0;
    }

    /* Include extra bytes for 'covr' atom length (4) and type (4) and its
       'data' item length (4) and type (4) plus 4 bytes for the 'data' atom's
       flags and 4 reserved(?) bytes. */
    extra_size = img_size + 24;

    fstat(aac_fd, &sb);
    file_size = sb.st_size;

    aac_fp = fdopen(dup(aac_fd), "r");

    stco_atom_pos = aac_drilltoatom(aac_fp, "moov:trak:mdia:minf:stbl:stco",
				    &atom_length);
    ilst_atom_pos = aac_drilltoatom(aac_fp, "moov:udta:meta:ilst",
				    &atom_length);
    last_pos = aac_drilltoatom(aac_fp, "mdat", &atom_length);

    if (last_pos != -1) {
	if (offset >= last_pos) {
	    /* Offset is in the actual music data so don't bother processing 
	       meta data. */
	    return 0;
	}
    } else {
	DPRINTF(E_LOG,L_ART, "No 'mdat' atom.\n");
	return 0;
    }

    rewind(aac_fp);

    /* Re-adjust length of 'moov' atom. */
    last_pos = scan_aac_findatom(aac_fp, file_size, "moov", &atom_length);
    if (last_pos != -1) {
	/* Copy everything from up to this atom */
	rewind(aac_fp);
	fcopyblock(aac_fp, out_fd, last_pos);

	/* Write out new length. */
	atom_length += extra_size;
	buffer[3] = atom_length & 0xFF;
	buffer[2] = ( atom_length >> 8 ) & 0xFF;
	buffer[1] = ( atom_length >> 16 ) & 0xFF;
	buffer[0] = ( atom_length >> 24 ) & 0xFF;
	r_write(out_fd, buffer, 4);

	last_pos += 4;
    } else {
	DPRINTF(E_LOG,L_ART, "Could not find 'moov' atom.\n");
	return 0;
    }

    if (stco_atom_pos < ilst_atom_pos) {
	last_pos = da_aac_rewrite_stco_atom(extra_size, out_fd, aac_fp, last_pos);
	last_pos = da_aac_insert_covr_atom(extra_size, out_fd, aac_fp, last_pos,
					   file_size, img_fd);
    } else {
	last_pos = da_aac_insert_covr_atom(extra_size, out_fd, aac_fp, last_pos,
					   file_size, img_fd);
	last_pos = da_aac_rewrite_stco_atom(extra_size, out_fd, aac_fp, last_pos);
    }

    /* Seek to position right after last atom. Let main() stream out the rest. */
    lseek(aac_fd, last_pos, SEEK_SET);

    r_close(img_fd);
    fclose(aac_fp);

    return last_pos;
}

int copyblock(int fromfd, int tofd, size_t size) {
    char buf[BLKSIZE];
    int  bytesread;
    int  totalbytes = 0;
    int  blocksize = BLKSIZE;
    int  bytesleft;

    while (totalbytes < size) {
	bytesleft = size - totalbytes;
	if (bytesleft < BLKSIZE) {
	    blocksize = bytesleft;
	} else {
	    blocksize = BLKSIZE;
	}
	if ((bytesread = r_read(fromfd, buf, blocksize)) < 0)
	    return -1;
	if (bytesread == 0)
	    return totalbytes;
	if (r_write(tofd, buf, bytesread) < 0)
	    return -1;
	totalbytes += bytesread;
    }
    return totalbytes;
}

int fcopyblock(FILE *fromfp, int tofd, size_t size) {
    char buf[BLKSIZE];
    int  bytesread;
    int  totalbytes = 0;
    int  blocksize = BLKSIZE;
    int  bytesleft;

    while (totalbytes < size) {
	bytesleft = size - totalbytes;
	if (bytesleft < BLKSIZE) {
	    blocksize = bytesleft;
	} else {
	    blocksize = BLKSIZE;
	}
	if ((bytesread = fread(buf, 1, blocksize, fromfp)) < blocksize) {
	    if (ferror(fromfp))
		return -1;
	}
	if (r_write(tofd, buf, bytesread) < 0)
	    return -1;

	if (feof(fromfp))
	    return 0;
	totalbytes += bytesread;
    }
    return totalbytes;
}
