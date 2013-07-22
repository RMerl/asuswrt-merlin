/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Author: Adrian Hunter
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/statvfs.h>

int main(int argc, char *argv[])
{
	char *dir_name = ".";
	uint64_t free_space;
	struct statvfs fs_info;

	if (argc > 1) {
		if (strncmp(argv[1], "--help", 6) == 0 ||
				strncmp(argv[1], "-h", 2) == 0) {
			printf(	"Usage is: "
				"free_space [directory]\n"
				"\n"
				"Display the free space of the file system "
				"of the directory given\n"
				"or the current directory if no "
				"directory is given.\nThe value output is "
				"in bytes.\n"
				);
			return 1;
		}
		dir_name = argv[1];
	}
	if (statvfs(dir_name, &fs_info) == -1)
		return 1;

	free_space = (uint64_t) fs_info.f_bavail * (uint64_t) fs_info.f_frsize;

	printf("%llu\n", (unsigned long long) free_space);

	return 0;
}
