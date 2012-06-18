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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <limits.h>

#include "usb_info.h"
#include "disk_initial.h"

#define VERSION 1

extern char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size){
	int device_type = get_device_type_by_device(device_name);

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(buf, 0, buf_size);
	strcpy(buf, "1-1");

	return buf;
}

int main(int argc, char *argv[]){
	disk_info_t *disk_info, *disk_list;
	partition_info_t *partition_info;

	usb_dbg("%d: Using libdisk to get information:\n", VERSION);

	if(argc == 2){
		if(!isdigit(argv[1][strlen(argv[1])-1])){ // Disk
			usb_dbg("%d: Get disk(%s)'s information:\n", VERSION, argv[1]);

			create_disk(argv[1], &disk_info);

			print_disk(disk_info);

			free_disk_data(&disk_info);
		}
		else{
			usb_dbg("%d: Get partition(%s)'s information:\n", VERSION, argv[1]);

			create_partition(argv[1], &partition_info);

			print_partition(partition_info);

			free_partition_data(&partition_info);
		}
	}
	else{
		usb_dbg("%d: Get all Disk information:\n", VERSION);

		disk_list = read_disk_data();

		print_disks(disk_list);

		free_disk_data(&disk_list);
	}

	return 0;
}
