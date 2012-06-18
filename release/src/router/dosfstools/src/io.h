/* io.h - Virtual disk input/output

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

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#ifndef _IO_H
#define _IO_H

#include <sys/types.h>		/* for loff_t */

loff_t llseek(int fd, loff_t offset, int whence);

/* lseek() analogue for large offsets. */

void fs_open(char *path, int rw);

/* Opens the file system PATH. If RW is zero, the file system is opened
   read-only, otherwise, it is opened read-write. */

void fs_read(loff_t pos, int size, void *data);

/* Reads SIZE bytes starting at POS into DATA. Performs all applicable
   changes. */

int fs_test(loff_t pos, int size);

/* Returns a non-zero integer if SIZE bytes starting at POS can be read without
   errors. Otherwise, it returns zero. */

void fs_write(loff_t pos, int size, void *data);

/* If write_immed is non-zero, SIZE bytes are written from DATA to the disk,
   starting at POS. If write_immed is zero, the change is added to a list in
   memory. */

int fs_close(int write);

/* Closes the file system, performs all pending changes if WRITE is non-zero
   and removes the list of changes. Returns a non-zero integer if the file
   system has been changed since the last fs_open, zero otherwise. */

int fs_changed(void);

/* Determines whether the file system has changed. See fs_close. */

extern unsigned device_no;

/* Major number of device (0 if file) and size (in 512 byte sectors) */

#endif
