/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _DISK_IO_TOOLS_
#define _DISK_IO_TOOLS_

#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#if defined(WL500)
#define POOL_MOUNT_ROOT "/tmp/harddisk"
#define BASE_LAYER 2
//#elif defined(RTN56U)
//#define POOL_MOUNT_ROOT "/media"	// for n13u
//#define BASE_LAYER 1
#else
#define POOL_MOUNT_ROOT "/tmp/mnt"
#define BASE_LAYER 2
#endif

#define MOUNT_LAYER BASE_LAYER+1

#define WWW_MOUNT_ROOT "/www"

extern int mkdir_if_none(const char *dir);
extern int delete_file_or_dir(char *target);

extern int test_if_mount_point_of_pool(const char *dirname);
extern int test_if_System_folder(const char *const dirname);
extern int test_mounted_disk_size_status(char *diskpath);

extern void strntrim(char *str);
/*extern void write_escaped_value(FILE *fp, const char *value);//*/

#endif // _DISK_IO_TOOLS_
