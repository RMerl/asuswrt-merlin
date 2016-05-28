/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
  a replacement for opendir/readdir/telldir/seekdir/closedir for BSD systems

  This is needed because the existing directory handling in FreeBSD
  and OpenBSD (and possibly NetBSD) doesn't correctly handle unlink()
  on files in a directory where telldir() has been used. On a block
  boundary it will occasionally miss a file when seekdir() is used to
  return to a position previously recorded with telldir().

  This also fixes a severe performance and memory usage problem with
  telldir() on BSD systems. Each call to telldir() in BSD adds an
  entry to a linked list, and those entries are cleaned up on
  closedir(). This means with a large directory closedir() can take an
  arbitrary amount of time, causing network timeouts as millions of
  telldir() entries are freed

  Note! This replacement code is not portable. It relies on getdents()
  always leaving the file descriptor at a seek offset that is a
  multiple of DIR_BUF_SIZE. If the code detects that this doesn't
  happen then it will abort(). It also does not handle directories
  with offsets larger than can be stored in a long,

  This code is available under other free software licenses as
  well. Contact the author.
*/

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#define DIR_BUF_BITS 9
#define DIR_BUF_SIZE (1<<DIR_BUF_BITS)

struct dir_buf {
	int fd;
	int nbytes, ofs;
	off_t seekpos;
	char buf[DIR_BUF_SIZE];
};

DIR *opendir(const char *dname)
{
	struct dir_buf *d;
	struct stat sb;
	d = malloc(sizeof(*d));
	if (d == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	d->fd = open(dname, O_RDONLY);
	if (d->fd == -1) {
		free(d);
		return NULL;
	}
	if (fstat(d->fd, &sb) < 0) {
		close(d->fd);
		free(d);
		return NULL;
	}
	if (!S_ISDIR(sb.st_mode)) {
		close(d->fd);
		free(d);   
		errno = ENOTDIR;
		return NULL;
	}
	d->ofs = 0;
	d->seekpos = 0;
	d->nbytes = 0;
	return (DIR *)d;
}

struct dirent *readdir(DIR *dir)
{
	struct dir_buf *d = (struct dir_buf *)dir;
	struct dirent *de;

	if (d->ofs >= d->nbytes) {
		d->seekpos = lseek(d->fd, 0, SEEK_CUR);
		d->nbytes = getdents(d->fd, d->buf, DIR_BUF_SIZE);
		d->ofs = 0;
	}
	if (d->ofs >= d->nbytes) {
		return NULL;
	}
	de = (struct dirent *)&d->buf[d->ofs];
	d->ofs += de->d_reclen;
	return de;
}

long telldir(DIR *dir)
{
	struct dir_buf *d = (struct dir_buf *)dir;
	if (d->ofs >= d->nbytes) {
		d->seekpos = lseek(d->fd, 0, SEEK_CUR);
		d->ofs = 0;
		d->nbytes = 0;
	}
	/* this relies on seekpos always being a multiple of
	   DIR_BUF_SIZE. Is that always true on BSD systems? */
	if (d->seekpos & (DIR_BUF_SIZE-1)) {
		abort();
	}
	return d->seekpos + d->ofs;
}

void seekdir(DIR *dir, long ofs)
{
	struct dir_buf *d = (struct dir_buf *)dir;
	d->seekpos = lseek(d->fd, ofs & ~(DIR_BUF_SIZE-1), SEEK_SET);
	d->nbytes = getdents(d->fd, d->buf, DIR_BUF_SIZE);
	d->ofs = 0;
	while (d->ofs < (ofs & (DIR_BUF_SIZE-1))) {
		if (readdir(dir) == NULL) break;
	}
}

void rewinddir(DIR *dir)
{
	seekdir(dir, 0);
}

int closedir(DIR *dir)
{
	struct dir_buf *d = (struct dir_buf *)dir;
	int r = close(d->fd);
	if (r != 0) {
		return r;
	}
	free(d);
	return 0;
}

#ifndef dirfd
/* darn, this is a macro on some systems. */
int dirfd(DIR *dir)
{
	struct dir_buf *d = (struct dir_buf *)dir;
	return d->fd;
}
#endif
