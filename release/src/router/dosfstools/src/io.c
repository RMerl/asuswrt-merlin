/* io.c - Virtual disk input/output

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.

   On Debian systems, the complete text of the GNU General Public License
   can be found in /usr/share/common-licenses/GPL-3 file.
*/

/*
 * Thu Feb 26 01:15:36 CET 1998: Martin Schulze <joey@infodrom.north.de>
 *	Fixed nasty bug that caused every file with a name like
 *	xxxxxxxx.xxx to be treated as bad name that needed to be fixed.
 */

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fd.h>

#include "dosfsck.h"
#include "common.h"
#include "io.h"

typedef struct _change {
    void *data;
    loff_t pos;
    int size;
    struct _change *next;
} CHANGE;

static CHANGE *changes, *last;
static int fd, did_change = 0;

unsigned device_no;

#ifdef __DJGPP__
#include "volume.h"		/* DOS lowlevel disk access functions */
loff_t llseek(int fd, loff_t offset, int whence)
{
    if ((whence != SEEK_SET) || (fd == 4711))
	return -1;		/* only those supported */
    return VolumeSeek(offset);
}

#define open OpenVolume
#define close CloseVolume
#define read(a,b,c) ReadVolume(b,c)
#define write(a,b,c) WriteVolume(b,c)
#else
loff_t llseek(int fd, loff_t offset, int whence)
{
    return (loff_t) lseek64(fd, (off64_t) offset, whence);
}
#endif

void fs_open(char *path, int rw)
{
    struct stat stbuf;

    if ((fd = open(path, rw ? O_RDWR : O_RDONLY)) < 0) {
	perror("open");
	exit(6);
    }
    changes = last = NULL;
    did_change = 0;

#ifndef _DJGPP_
    if (fstat(fd, &stbuf) < 0)
	pdie("fstat %s", path);
    device_no = S_ISBLK(stbuf.st_mode) ? (stbuf.st_rdev >> 8) & 0xff : 0;
#else
    if (IsWorkingOnImageFile()) {
	if (fstat(GetVolumeHandle(), &stbuf) < 0)
	    pdie("fstat image %s", path);
	device_no = 0;
    } else {
	/* return 2 for floppy, 1 for ramdisk, 7 for loopback  */
	/* used by boot.c in Atari mode: floppy always FAT12,  */
	/* loopback / ramdisk only FAT12 if usual floppy size, */
	/* harddisk always FAT16 on Atari... */
	device_no = (GetVolumeHandle() < 2) ? 2 : 1;
	/* telling "floppy" for A:/B:, "ramdisk" for the rest */
    }
#endif
}

/**
 * Read data from the partition, accounting for any pending updates that are
 * queued for writing.
 *
 * @param[in]   pos     Byte offset, relative to the beginning of the partition,
 *                      at which to read
 * @param[in]   size    Number of bytes to read
 * @param[out]  data    Where to put the data read
 */
void fs_read(loff_t pos, int size, void *data)
{
    CHANGE *walk;
    int got;

    if (llseek(fd, pos, 0) != pos)
	pdie("Seek to %lld", pos);
    if ((got = read(fd, data, size)) < 0)
	pdie("Read %d bytes at %lld", size, pos);
    if (got != size)
	die("Got %d bytes instead of %d at %lld", got, size, pos);
    for (walk = changes; walk; walk = walk->next) {
	if (walk->pos < pos + size && walk->pos + walk->size > pos) {
	    if (walk->pos < pos)
		memcpy(data, (char *)walk->data + pos - walk->pos, min(size,
								       walk->size
								       - pos +
								       walk->pos));
	    else
		memcpy((char *)data + walk->pos - pos, walk->data,
		       min(walk->size, size + pos - walk->pos));
	}
    }
}

int fs_test(loff_t pos, int size)
{
    void *scratch;
    int okay;

    if (llseek(fd, pos, 0) != pos)
	pdie("Seek to %lld", pos);
    scratch = alloc(size);
    okay = read(fd, scratch, size) == size;
    free(scratch);
    return okay;
}

void fs_write(loff_t pos, int size, void *data)
{
    CHANGE *new;
    int did;

    if (write_immed) {
	did_change = 1;
	if (llseek(fd, pos, 0) != pos)
	    pdie("Seek to %lld", pos);
	if ((did = write(fd, data, size)) == size)
	    return;
	if (did < 0)
	    pdie("Write %d bytes at %lld", size, pos);
	die("Wrote %d bytes instead of %d at %lld", did, size, pos);
    }
    new = alloc(sizeof(CHANGE));
    new->pos = pos;
    memcpy(new->data = alloc(new->size = size), data, size);
    new->next = NULL;
    if (last)
	last->next = new;
    else
	changes = new;
    last = new;
}

static void fs_flush(void)
{
    CHANGE *this;
    int size;

    while (changes) {
	this = changes;
	changes = changes->next;
	if (llseek(fd, this->pos, 0) != this->pos)
	    fprintf(stderr,
		    "Seek to %lld failed: %s\n  Did not write %d bytes.\n",
		    (long long)this->pos, strerror(errno), this->size);
	else if ((size = write(fd, this->data, this->size)) < 0)
	    fprintf(stderr, "Writing %d bytes at %lld failed: %s\n", this->size,
		    (long long)this->pos, strerror(errno));
	else if (size != this->size)
	    fprintf(stderr, "Wrote %d bytes instead of %d bytes at %lld."
		    "\n", size, this->size, (long long)this->pos);
	free(this->data);
	free(this);
    }
}

int fs_close(int write)
{
    CHANGE *next;
    int changed;

    changed = ! !changes;
    if (write)
	fs_flush();
    else
	while (changes) {
	    next = changes->next;
	    free(changes->data);
	    free(changes);
	    changes = next;
	}
    if (close(fd) < 0)
	pdie("closing file system");
    return changed || did_change;
}

int fs_changed(void)
{
    return ! !changes || did_change;
}
