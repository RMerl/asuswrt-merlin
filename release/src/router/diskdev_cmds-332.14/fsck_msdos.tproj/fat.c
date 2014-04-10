/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/param.h>

#include "ext.h"
#include "fsutil.h"

/*
 * The following value should be a multiple of the sector size in bytes.  The
 * Microsoft supported sector sizes are 512, 1024, 2048, and 4096, which means
 * this should be a multiple of 4096.  It should also be a minimum of 6K so
 * that a maximal FAT12 FAT can fit in a single chunk (so the code doesn't
 * have to handle a FAT entry crossing a chunk boundary).  It should be
 * large enough to make the I/O efficient, without occupying too much memory.
 */
#define FAT_CHUNK_SIZE 65536

static int checkclnum __P((struct bootblock *, int, cl_t, cl_t *));
static int clustdiffer __P((cl_t, cl_t *, cl_t *, int));
static int tryclear __P((struct bootblock *, struct fatEntry *, cl_t, cl_t *));
static int _readchunk(int fd, struct bootblock *boot, u_int32_t chunk, u_char *buffer);
static int _writechunks(int fd, struct bootblock *boot, u_int32_t chunk, u_char *buffer);

ssize_t	deblock_read(int d, void *buf, size_t nbytes);
ssize_t	deblock_write(int d, void *buf, size_t nbytes);



/*
 * Determine whether a volume is dirty, without reading the entire FAT.
 */
int isdirty(int fs, struct bootblock *boot, int fat)
{
       int             result;
       u_char  *buffer;
       off_t   offset;

       result = 1;             /* In case of error, assume volume was dirty */

       /* FAT12 volumes don't have a "clean" bit, so always assume they are dirty */
       if (boot->ClustMask == CLUST12_MASK)
               return 1;

       buffer = malloc(boot->BytesPerSec);
       if (buffer == NULL) {
               perr("No space for FAT sector");
               return 1;               /* Assume it was dirty */
       }

       offset = boot->ResSectors + fat * boot->FATsecs;
       offset *= boot->BytesPerSec;

       if (lseek(fs, offset, SEEK_SET) != offset) {
               perr("Unable to read FAT");
               goto ERROR;
       }

       if (deblock_read(fs, buffer, boot->BytesPerSec) != boot->BytesPerSec) {
               perr("Unable to read FAT");
               goto ERROR;
       }

       switch (boot->ClustMask) {
       case CLUST32_MASK:
               /* FAT32 uses bit 27 of FAT[1] */
               if ((buffer[7] & 0x08) != 0)
                       result = 0;             /* It's clean */
               break;
       case CLUST16_MASK:
               /* FAT16 uses bit 15 of FAT[1] */
               if ((buffer[3] & 0x80) != 0)
                       result = 0;             /* It's clean */
               break;
       }

ERROR:
       free(buffer);
       return result;
}



/*
 * Determine the length of a cluster chain by following its links in the FAT
 */
cl_t chainlength(struct bootblock *boot, struct fatEntry *fat, cl_t head)
{
    cl_t length;
    
    length = 0;
    while (head >= CLUST_FIRST && head < boot->NumClusters)
    {
        ++length;
        head = fat[head].next;
    }
    
    return length;
}

/*
 * Check a cluster number for valid value
 */
static int
checkclnum(boot, fat, cl, next)
	struct bootblock *boot;
	int fat;
	cl_t cl;
	cl_t *next;
{
	if (*next >= (CLUST_RSRVD&boot->ClustMask))
		*next |= ~boot->ClustMask;
	if (*next == CLUST_FREE) {
		boot->NumFree++;
		return FSOK;
	}
	if (*next == CLUST_BAD) {
		boot->NumBad++;
		return FSOK;
	}
	if (*next < CLUST_FIRST
	    || (*next >= boot->NumClusters && *next < CLUST_EOFS)) {
		pwarn("Cluster %u in FAT %d continues with %s cluster number %u\n",
		      cl, fat,
		      *next < CLUST_RSRVD ? "out of range" : "reserved",
		      *next&boot->ClustMask);
		if (ask(0, "Truncate")) {
			*next = CLUST_EOF;
			return FSFATMOD;
		}
		return FSERROR;
	}
	return FSOK;
}

/*
 * Read one "chunk" of the FAT into the caller's buffer.
 *
 * Inputs:
 *	fd	Raw device's file descriptor
 *	chunk	Chunk number within the FAT
 *	buffer	Buffer where chunk should be stored
 *
 * Result is non-zero if there was an error.
 */
static int _readchunk(int fd, struct bootblock *boot, u_int32_t chunk, u_char *buffer)
{
    int		activeFAT;	/* Which FAT we're actively using */
    size_t	length;		/* Number of bytes in this chunk */
    off_t	offset;		/* Offset of given chunk in active FAT (from start of volume) */
    
    /* Calculate size of this chunk.  The last chunk may be shorter. */
    length = boot->FATsecs * boot->BytesPerSec;		/* Size of entire FAT, in bytes */
    length -= chunk * FAT_CHUNK_SIZE;			/* Start of current chunk to end of FAT */
    if (length > FAT_CHUNK_SIZE)			/* Not last chunk? */
        length = FAT_CHUNK_SIZE;			/* Then use a whole chunk */

    /*
     * If the FAT is an exact multiple of FAT_CHUNK_SIZE, we may get asked to read a
     * chunk number that is out of range.  If that case, length will be zero.  We
     * can just return without doing anything since the caller will fall out of
     * their loop anyway.
     */
    if (length == 0)
        return 0;

    /* Find offset of current chunk within active FAT */
    activeFAT = boot->ValidFat >= 0 ? boot->ValidFat : 0;
    offset = boot->ResSectors + activeFAT * boot->FATsecs;	/* Starting sector of active FAT */
    offset *= boot->BytesPerSec;			/* Byte offset of active FAT */
    offset += chunk * FAT_CHUNK_SIZE;		/* Byte offset of current chunk */

    if (lseek(fd, offset, SEEK_SET) != offset)
    {
        perr("Unable to seek FAT");
        return 1;
    }
    
    if (deblock_read(fd, buffer, length) != length)
    {
        perr("Unable to read FAT");
        return 1;
    }

    return 0;
}

/*
 * Write one "chunk" of the FAT from the caller's buffer to all FATs.
 *
 * Inputs:
 *	fd	Raw device's file descriptor
 *	chunk	Which chunk number within the FAT
 *	buffer	Buffer where chunk is stored
 *
 * Result is non-zero if there was an error.
 */
static int _writechunks(int fd, struct bootblock *boot, u_int32_t chunk, u_char *buffer)
{
    int		i;		/* Which FAT we're writing to */
    off_t	offset;		/* Offset of current chunk in current FAT */
    size_t	length;		/* Number of bytes in this chunk */
    
    /* Calculate size of this chunk.  The last chunk may be shorter. */
    length = boot->FATsecs * boot->BytesPerSec;		/* Size of entire FAT, in bytes */
    length -= chunk * FAT_CHUNK_SIZE;			/* Start of current chunk to end of FAT */
    if (length > FAT_CHUNK_SIZE)			/* Not last chunk? */
        length = FAT_CHUNK_SIZE;			/* Then use a whole chunk */

    /* Loop over each FAT */
    for (i=0; i < boot->FATs; ++i)
    {
        /* Find offset of current chunk within current FAT */
        offset = boot->ResSectors + i * boot->FATsecs;	/* Starting sector of current FAT */
        offset *= boot->BytesPerSec;			/* Byte offset of current FAT */
        offset += chunk * FAT_CHUNK_SIZE;		/* Byte offset of current chunk */
        
        if (lseek(fd, offset, SEEK_SET) != offset)
        {
            perr("Unable to seek FAT");
            return 1;
        }
        
        if (deblock_write(fd, buffer, length) != length)
        {
            perr("Unable to write FAT");
            return 1;
        }
    }

    return 0;
}

/*
 * Read a FAT and decode it into internal format
 */
int
readfat(fs, boot, no, fp)
	int fs;				// file descriptor to raw device
	struct bootblock *boot;
	int no;				// number of active FATs
	struct fatEntry **fp;		// returned array
{
	struct fatEntry *fat;		// fatEntry for current cluster
	u_char *buffer;			// one "chunk" of the FAT
        u_char *p;			// pointer to current cluster's FAT entry in chunk
	cl_t cl;			// current cluster
	int ret = FSOK;
        u_int32_t chunk;		// offset of current FAT chunk
        
	boot->NumFree = boot->NumBad = 0;

        buffer = malloc(FAT_CHUNK_SIZE);
        if (buffer == NULL)
        {
            perr("No space for FAT");
            return FSFATAL;
        }
        
        /* Read the first chunk */
        chunk = 0;
        if (_readchunk(fs, boot, chunk, buffer))
        {
            free(buffer);
            return FSFATAL;
        }
        
	fat = calloc(boot->NumClusters, sizeof(struct fatEntry));
	if (fat == NULL) {
		perr("No space for FAT");
		free(buffer);
		return FSFATAL;
	}

	if (buffer[0] != boot->Media
	    || buffer[1] != 0xff || buffer[2] != 0xff
	    || (boot->ClustMask == CLUST16_MASK && buffer[3] != 0xff)
	    || (boot->ClustMask == CLUST32_MASK
		&& ((buffer[3]&0x0f) != 0x0f
		    || buffer[4] != 0xff || buffer[5] != 0xff
		    || buffer[6] != 0xff || (buffer[7]&0x0f) != 0x0f))) {
		const char *msg;

		/* Windows 95 OSR2 (and possibly any later) changes
		 * the FAT signature to 0xXXffff7f for FAT16 and to
		 * 0xXXffff0fffffff07 for FAT32 upon boot, to know that the
		 * filesystem is dirty if it doesn't reboot cleanly.
		 * Check this special condition before errorring out.
		 */
		if (buffer[0] == boot->Media && buffer[1] == 0xff
		    && buffer[2] == 0xff
		    && ((boot->ClustMask == CLUST16_MASK && buffer[3] == 0x7f)
			|| (boot->ClustMask == CLUST32_MASK
			    && (buffer[3]&0x0F) == 0x0f && buffer[4] == 0xff
			    && buffer[5] == 0xff && buffer[6] == 0xff
			    && (buffer[7]&0x0F) == 0x07)))
			ret |= FSDIRTY;
		else {
			/* just some odd byte sequence in FAT */
				
			switch (boot->ClustMask) {
			case CLUST32_MASK:
				msg = "%s (%02x%02x%02x%02x%02x%02x%02x%02x)\n";
				break;
			case CLUST16_MASK:
				msg = "%s (%02x%02x%02x%02x)\n";
				break;
			default:
				msg = "%s (%02x%02x%02x)\n";
				break;
			}

			pwarn(msg, "FAT starts with odd byte sequence",
			      buffer[0], buffer[1], buffer[2], buffer[3],
			      buffer[4], buffer[5], buffer[6], buffer[7]);
	
			if (ask(1, "Correct"))
				ret |= FSFIXFAT;
		}
	}
	switch (boot->ClustMask) {
	case CLUST32_MASK:
		p = buffer + 8;
		break;
	case CLUST16_MASK:
		p = buffer + 4;
		break;
	default:
		p = buffer + 3;
		break;
	}
	for (cl = CLUST_FIRST; cl < boot->NumClusters;) {
		switch (boot->ClustMask) {
		case CLUST32_MASK:
			fat[cl].next = p[0] + (p[1] << 8)
				       + (p[2] << 16) + (p[3] << 24);
			fat[cl].next &= boot->ClustMask;
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			p += 4;
			break;
		case CLUST16_MASK:
			fat[cl].next = p[0] + (p[1] << 8);
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			p += 2;
			break;
		default:
			fat[cl].next = (p[0] + (p[1] << 8)) & 0x0fff;
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			if (cl >= boot->NumClusters)
				break;
			fat[cl].next = ((p[1] >> 4) + (p[2] << 4)) & 0x0fff;
			ret |= checkclnum(boot, no, cl, &fat[cl].next);
			cl++;
			p += 3;
			break;
		}
                
                if (p == buffer + FAT_CHUNK_SIZE)
                {
                    /* Time to read the next chunk of the FAT */
                    ++chunk;
                    if (_readchunk(fs, boot, chunk, buffer))
                    {
                        free(buffer);
                        free(fat);
                        return FSFATAL;
                    }
                    p = buffer;
                }
	}

	free(buffer);
	*fp = fat;
	return ret;
}

/*
 * Get type of reserved cluster
 */
char *
rsrvdcltype(cl)
	cl_t cl;
{
	if (cl == CLUST_FREE)
		return "free";
	if (cl < CLUST_BAD)
		return "reserved";
	if (cl > CLUST_BAD)
		return "as EOF";
	return "bad";
}

static int
clustdiffer(cl, cp1, cp2, fatnum)
	cl_t cl;
	cl_t *cp1;
	cl_t *cp2;
	int fatnum;
{
	if (*cp1 == CLUST_FREE || *cp1 >= CLUST_RSRVD) {
		if (*cp2 == CLUST_FREE || *cp2 >= CLUST_RSRVD) {
			if ((*cp1 != CLUST_FREE && *cp1 < CLUST_BAD
			     && *cp2 != CLUST_FREE && *cp2 < CLUST_BAD)
			    || (*cp1 > CLUST_BAD && *cp2 > CLUST_BAD)) {
				pwarn("Cluster %u is marked %s with different indicators, ",
				      cl, rsrvdcltype(*cp1));
				if (ask(1, "fix")) {
					*cp2 = *cp1;
					return FSFATMOD;
				}
				return FSFATAL;
			}
			pwarn("Cluster %u is marked %s in FAT 0, %s in FAT %d\n",
			      cl, rsrvdcltype(*cp1), rsrvdcltype(*cp2), fatnum);
			if (ask(0, "use FAT 0's entry")) {
				*cp2 = *cp1;
				return FSFATMOD;
			}
			if (ask(0, "use FAT %d's entry", fatnum)) {
				*cp1 = *cp2;
				return FSFATMOD;
			}
			return FSFATAL;
		}
		pwarn("Cluster %u is marked %s in FAT 0, but continues with cluster %u in FAT %d\n",
		      cl, rsrvdcltype(*cp1), *cp2, fatnum);
		if (ask(0, "Use continuation from FAT %d", fatnum)) {
			*cp1 = *cp2;
			return FSFATMOD;
		}
		if (ask(0, "Use mark from FAT 0")) {
			*cp2 = *cp1;
			return FSFATMOD;
		}
		return FSFATAL;
	}
	if (*cp2 == CLUST_FREE || *cp2 >= CLUST_RSRVD) {
		pwarn("Cluster %u continues with cluster %u in FAT 0, but is marked %s in FAT %d\n",
		      cl, *cp1, rsrvdcltype(*cp2), fatnum);
		if (ask(0, "Use continuation from FAT 0")) {
			*cp2 = *cp1;
			return FSFATMOD;
		}
		if (ask(0, "Use mark from FAT %d", fatnum)) {
			*cp1 = *cp2;
			return FSFATMOD;
		}
		return FSERROR;
	}
	pwarn("Cluster %u continues with cluster %u in FAT 0, but with cluster %u in FAT %d\n",
	      cl, *cp1, *cp2, fatnum);
	if (ask(0, "Use continuation from FAT 0")) {
		*cp2 = *cp1;
		return FSFATMOD;
	}
	if (ask(0, "Use continuation from FAT %d", fatnum)) {
		*cp1 = *cp2;
		return FSFATMOD;
	}
	return FSERROR;
}

/*
 * Compare two FAT copies in memory. Resolve any conflicts and merge them
 * into the first one.
 */
int
comparefat(boot, first, second, fatnum)
	struct bootblock *boot;
	struct fatEntry *first;
	struct fatEntry *second;
	int fatnum;
{
	cl_t cl;
	int ret = FSOK;

	for (cl = CLUST_FIRST; cl < boot->NumClusters; cl++)
		if (first[cl].next != second[cl].next)
			ret |= clustdiffer(cl, &first[cl].next, &second[cl].next, fatnum);
	return ret;
}

void
clearchain(boot, fat, head)
	struct bootblock *boot;
	struct fatEntry *fat;
	cl_t head;
{
	cl_t p, q;
        
	for (p = head; p >= CLUST_FIRST && p < boot->NumClusters; p = q) {
		if (fat[p].head != head)
			break;
		q = fat[p].next;
		fat[p].next = fat[p].head = CLUST_FREE;
                fat[p].in_use = 0;
	}
}

int
tryclear(boot, fat, head, trunc)
	struct bootblock *boot;
	struct fatEntry *fat;
	cl_t head;
	cl_t *trunc;
{
	if (ask(0, "Clear chain starting at %u", head)) {
		clearchain(boot, fat, head);
		return FSFATMOD;
	} else if (ask(0, "Truncate")) {
		*trunc = CLUST_EOF;
		return FSFATMOD;
	} else
		return FSERROR;
}

/*
 * Check a complete FAT in-memory for crosslinks
 */
int
checkfat(boot, fat)
	struct bootblock *boot;
	struct fatEntry *fat;
{
	cl_t head, p, h, n;
	int ret = 0;
	int conf;

	/*
	 * pass 1: figure out the cluster chains.
	 */
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untravelled chain */
		if (fat[head].head != 0		/* cluster already belongs to some chain */
		    || fat[head].next == CLUST_FREE
		    || fat[head].next == CLUST_BAD)
			continue;		/* skip it. */

		/* follow the chain and mark all clusters on the way */
		for (p = head;
		     p >= CLUST_FIRST && p < boot->NumClusters;
		     p = fat[p].next) {
			fat[p].head = head;
		}
	}

	/*
	 * pass 2: check for crosslinked chains (we couldn't do this in pass 1 because
	 * we didn't know the real start of the chain then - would have treated partial
	 * chains as interlinked with their main chain)
	 */
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untravelled chain */
		if (fat[head].head != head)
			continue;

		/* follow the chain to its end (hopefully) */
		for (p = head;
		     (n = fat[p].next) >= CLUST_FIRST && n < boot->NumClusters;
		     p = n)
			if (fat[n].head != head)
				break;
		if (n >= CLUST_EOFS)
			continue;

		if (n == CLUST_FREE || n >= CLUST_RSRVD) {
			pwarn("Cluster chain starting at %u ends with cluster marked %s\n",
			      head, rsrvdcltype(n));
			ret |= tryclear(boot, fat, head, &fat[p].next);
			continue;
		}
		if (n < CLUST_FIRST || n >= boot->NumClusters) {
			pwarn("Cluster chain starting at %u ends with cluster out of range (%u)\n",
			      head, n);
			ret |= tryclear(boot, fat, head, &fat[p].next);
			continue;
		}
		pwarn("Cluster chains starting at %u and %u are linked at cluster %u\n",
		      head, fat[n].head, n);
		conf = tryclear(boot, fat, head, &fat[p].next);
		if (ask(0, "Clear chain starting at %u", h = fat[n].head)) {
			if (conf == FSERROR) {
				/*
				 * Transfer the common chain to the one not cleared above.
				 */
				for (p = n;
				     p >= CLUST_FIRST && p < boot->NumClusters;
				     p = fat[p].next) {
					if (h != fat[p].head) {
						/*
						 * Have to reexamine this chain.
						 */
						head--;
						break;
					}
					fat[p].head = head;
				}
			}
			clearchain(boot, fat, h);
			conf |= FSFATMOD;
		}
		ret |= conf;
	}

	return ret;
}

/*
 * Write out FATs encoding them from the internal format
 */
int
writefat(fs, boot, fat, correct_fat)
	int fs;
	struct bootblock *boot;
	struct fatEntry *fat;
	int correct_fat;
{
	u_char *buffer;		/* Start of current FAT chunk */
        u_char *p;		/* Current cluster within the buffer */
	cl_t cl;		/* Current cluster number */
        u_int32_t chunk;	/* Current chunk number */
        int activeFAT;		/* Number of active FAT */

        activeFAT = boot->ValidFat >= 0 ? boot->ValidFat : 0;
        
        /* Allocate a buffer for the current FAT chunk */
	buffer = malloc(FAT_CHUNK_SIZE);
	if (buffer == NULL) {
		perr("No space for FAT");
		return FSFATAL;
	}

        /*
         * Pre-read the first chunk of the FAT.  We do this for FAT32 volumes
         * so we can preserve the upper 4 reserved bits of each FAT entry.
         * We also do this if we were supposed to preserve the first two
         * entries (the "signature").
         */
        chunk = 0;
        if (boot->ClustMask == CLUST32_MASK || !correct_fat)
        {
            if (_readchunk(fs, boot, chunk, buffer))
            {
                free(buffer);
                return FSFATAL;
            }
        }
        
	boot->NumFree = 0;
	p = buffer;
	if (correct_fat) {
		*p++ = (u_char)boot->Media;
		*p++ = 0xff;
		*p++ = 0xff;
		switch (boot->ClustMask) {
		case CLUST16_MASK:
			*p++ = 0xff;
			break;
		case CLUST32_MASK:
			*p++ = 0x0f;
			*p++ = 0xff;
			*p++ = 0xff;
			*p++ = 0xff;
			*p++ = 0x0f;
			break;
		}
	} else {
                /*
                 * Keep the first two FAT entries (the "signature") unchanged.
                 * Note that the first chunk was already read into buffer above,
                 * so all we have to do is skip over the first two entries.
                 */
		switch (boot->ClustMask) {
		case CLUST32_MASK:
			p += 8;
			break;
		case CLUST16_MASK:
			p += 4;
			break;
		default:
			p += 3;
			break;
		}
	}
			
	for (cl = CLUST_FIRST; cl < boot->NumClusters; cl++) {
		switch (boot->ClustMask) {
		case CLUST32_MASK:
			if (fat[cl].next == CLUST_FREE)
				boot->NumFree++;
			*p++ = (u_char)fat[cl].next;
			*p++ = (u_char)(fat[cl].next >> 8);
			*p++ = (u_char)(fat[cl].next >> 16);
			*p &= 0xf0;
			*p++ |= (fat[cl].next >> 24)&0x0f;
			break;
		case CLUST16_MASK:
			if (fat[cl].next == CLUST_FREE)
				boot->NumFree++;
			*p++ = (u_char)fat[cl].next;
			*p++ = (u_char)(fat[cl].next >> 8);
			break;
		default:
			if (fat[cl].next == CLUST_FREE)
				boot->NumFree++;
			if (cl + 1 < boot->NumClusters
			    && fat[cl + 1].next == CLUST_FREE)
				boot->NumFree++;
			*p++ = (u_char)fat[cl].next;
			*p++ = (u_char)((fat[cl].next >> 8) & 0xf)
			       |(u_char)(fat[cl+1].next << 4);
			*p++ = (u_char)(fat[++cl].next >> 4);
			break;
		}
                
                /*
                 * If we are at the end of the chunk, we need to write it to all
                 * of the FATs.  And if this is a FAT32 volume, we need to pre-read
                 * the next chunk (to preserve the reserved bits).
                 */
                if (p == buffer + FAT_CHUNK_SIZE)
                {
                    /* Write out the current chunk */
                    if (_writechunks(fs, boot, chunk, buffer))
                    {
                        free(buffer);
                        return FSFATAL;
                    }
                    
                    /* Move to the next chunk and reset "p" */
                    ++chunk;
                    p = buffer;
                    
                    /* For FAT32, pre-read the next chunk */
                    if (boot->ClustMask == CLUST32_MASK)
                    {
                        if (_readchunk(fs, boot, chunk, buffer))
                        {
                            free(buffer);
                            return FSFATAL;
                        }
                    }
                }
	}
        
        /*
         * If the last chunk was short, we need to write it out now.
         * If the FAT was a multiple of the chunk size, the last chunk
         * was written out just before falling out of the loop above,
         * in which case p == buffer.
         */
        if (p != buffer)	/* Partial chunk left to write? */
        {
            if (_writechunks(fs, boot, chunk, buffer))
            {
                free(buffer);
                return FSFATAL;
            }
        }
        
	free(buffer);
	return FSOK;
}

/*
 * Check a complete in-memory FAT for lost cluster chains
 */
int
checklost(dosfs, boot, fat)
	int dosfs;
	struct bootblock *boot;
	struct fatEntry *fat;
{
	cl_t head;
	int mod = FSOK;
	int ret;
	
	for (head = CLUST_FIRST; head < boot->NumClusters; head++) {
		/* find next untravelled chain */
		if (fat[head].head != head
		    || fat[head].next == CLUST_FREE
		    || (fat[head].next >= CLUST_RSRVD
			&& fat[head].next < CLUST_EOFS)
		    || (fat[head].in_use))
			continue;

		pwarn("Lost cluster chain at cluster %u\n%d Cluster(s) lost\n",
		      head, chainlength(boot, fat, head));
		mod |= ret = reconnect(dosfs, boot, fat, head);
		if (mod & FSFATAL)
			break;
		if (ret == FSERROR && ask(0, "Clear")) {
			clearchain(boot, fat, head);
			mod |= FSFATMOD;
		}
	}
	finishlf();

	if (boot->FSInfo) {
		ret = 0;
		if (boot->FSFree != boot->NumFree) {
			pwarn("Free space in FSInfo block (%d) not correct (%d)\n",
			      boot->FSFree, boot->NumFree);
			if (ask(1, "fix")) {
				boot->FSFree = boot->NumFree;
				ret = 1;
			}
		}
		if (boot->NumFree &&
                        (boot->FSNext < CLUST_FIRST ||
                        boot->FSNext >= boot->NumClusters ||
                        fat[boot->FSNext].next != CLUST_FREE))
                {
			pwarn("Next free cluster in FSInfo block (%u) not free\n",
			      boot->FSNext);
			if (ask(1, "fix"))
				for (head = CLUST_FIRST; head < boot->NumClusters; head++)
					if (fat[head].next == CLUST_FREE) {
						boot->FSNext = head;
						ret = 1;
						break;
					}
		}
		if (ret)
			mod |= writefsinfo(dosfs, boot);
	}

	return mod;
}

#define DEBLOCK_SIZE 		(MAXPHYSIO>>2)
ssize_t	deblock_read(int d, void *buf, size_t nbytes) {
    ssize_t		totbytes = 0, readbytes;
    char		*b = buf;

    while (nbytes > 0) {
        size_t 		rbytes = nbytes < DEBLOCK_SIZE? nbytes : DEBLOCK_SIZE;
        readbytes = read(d, b, rbytes);
        if (readbytes < 0)
            return readbytes;
        else if (readbytes == 0)
            break;
        else {
            nbytes-=readbytes;
            totbytes += readbytes;
            b += readbytes;
        }
    }

    return totbytes;
}

ssize_t	deblock_write(int d, void *buf, size_t nbytes) {
    ssize_t		totbytes = 0, writebytes;
    char		*b = buf;

    while (nbytes > 0) {
        size_t 		wbytes = nbytes < DEBLOCK_SIZE ? nbytes : DEBLOCK_SIZE;
        writebytes = write(d, b, wbytes);
        if (writebytes < 0)
            return writebytes;
        else if (writebytes == 0)
            break;
        else {
            nbytes-=writebytes;
            totbytes += writebytes;
            b += writebytes;
        }
    }

    return totbytes;
}
