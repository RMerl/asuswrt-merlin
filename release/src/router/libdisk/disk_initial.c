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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <limits.h>
#include <dirent.h>

// From BusyBox and get volume's label.
#include <autoconf.h>
#include <volume_id_internal.h>
#include <shared.h>

#include "usb_info.h"
#include "disk_initial.h"

extern disk_info_t *read_disk_data(){
	disk_info_t *disk_info_list = NULL, *new_disk_info, **follow_disk_info_list;
	char *partition_info = read_whole_file(PARTITION_FILE);
	char *follow_info;
	char line[64], device_name[16];
	u32 major;
	disk_info_t *parent_disk_info;
	partition_info_t *new_partition_info, **follow_partition_list;
	unsigned long long device_size;

	if(partition_info == NULL){
		usb_dbg("Failed to open \"%s\"!!\n", PARTITION_FILE);
		return disk_info_list;
	}
	follow_info = partition_info;

	memset(device_name, 0, 16);
	while(get_line_from_buffer(follow_info, line, 64) != NULL){
		follow_info += strlen(line);

		if(sscanf(line, "%u %*u %llu %[^\n ]", &major, &device_size, device_name) != 3)
			continue;
		if(major != USB_DISK_MAJOR)
			continue;
		if(device_size == 1) // extend partition.
			continue;

		if(is_disk_name(device_name)){ // Disk
			follow_disk_info_list = &disk_info_list;
			while(*follow_disk_info_list != NULL)
				follow_disk_info_list = &((*follow_disk_info_list)->next);

			new_disk_info = create_disk(device_name, follow_disk_info_list);
		}
		else if(is_partition_name(device_name, NULL)){ // Partition
			// Found a partition device.
			// Find the parent disk.
			parent_disk_info = disk_info_list;
			while(1){
				if(parent_disk_info == NULL){
					usb_dbg("Error while parsing %s: found "
									"partition '%s' but haven't seen the disk device "
									"of which it is a part.\n", PARTITION_FILE, device_name);
					free(partition_info);
					return disk_info_list;
				}

				if(!strncmp(device_name, parent_disk_info->device, 3))
					break;

				parent_disk_info = parent_disk_info->next;
			}

			follow_partition_list = &(parent_disk_info->partitions);
			while(*follow_partition_list != NULL){
				if((*follow_partition_list)->partition_order == 0){
					free_partition_data(follow_partition_list);
					parent_disk_info->partitions = NULL;
					follow_partition_list = &(parent_disk_info->partitions);
				}
				else
					follow_partition_list = &((*follow_partition_list)->next);
			}

			new_partition_info = create_partition(device_name, follow_partition_list);
			if(new_partition_info != NULL)
				new_partition_info->disk = parent_disk_info;
		}
	}

	free(partition_info);
	return disk_info_list;
}

extern int is_disk_name(const char *device_name){
	if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
		return 0;

	if(isdigit(device_name[strlen(device_name)-1]))
		return 0;

	return 1;
}

extern disk_info_t *create_disk(const char *device_name, disk_info_t **new_disk_info){
	disk_info_t *follow_disk_info;
	u32 major, minor;
	u64 size_in_kilobytes = 0;
	int len;
	char buf[64], *port, *vendor, *model, *ptr;
	partition_info_t *new_partition_info, **follow_partition_list;

	if(new_disk_info == NULL){
		usb_dbg("Bad input!!\n");
		return NULL;
	}

	*new_disk_info = NULL; // initial value.

	if(device_name == NULL || !is_disk_name(device_name))
		return NULL;

	if(initial_disk_data(&follow_disk_info) == NULL){
		usb_dbg("No memory!!(follow_disk_info)\n");
		return NULL;
	}

	len = strlen(device_name);
	follow_disk_info->device = (char *)malloc(len+1);
	if(follow_disk_info->device == NULL){
		usb_dbg("No memory!!(follow_disk_info->device)\n");
		free_disk_data(&follow_disk_info);
		return NULL;
	}
	strcpy(follow_disk_info->device, device_name);
	follow_disk_info->device[len] = 0;

	if(!get_disk_major_minor(device_name, &major, &minor)){
		usb_dbg("Fail to get disk's major and minor: %s.\n", device_name);
		free_disk_data(&follow_disk_info);
		return NULL;
	}
	follow_disk_info->major = major;
	follow_disk_info->minor = minor;

	if(!get_disk_size(device_name, &size_in_kilobytes)){
		usb_dbg("Fail to get disk's size_in_kilobytes: %s.\n", device_name);
		free_disk_data(&follow_disk_info);
		return NULL;
	}
	follow_disk_info->size_in_kilobytes = size_in_kilobytes;

	if(!strncmp(device_name, "sd", 2)){
		// Get USB port.
		if(get_usb_port_by_device(device_name, buf, 64) == NULL){
			usb_dbg("Fail to get usb port: %s.\n", device_name);
			free_disk_data(&follow_disk_info);
			return NULL;
		}

		len = strlen(buf);
		if(len > 0){
			port = (char *)malloc(2);
			if(port == NULL){
				usb_dbg("No memory!!(port)\n");
				free_disk_data(&follow_disk_info);
				return NULL;
			}
			memset(port, 0, 2);

			int port_num = get_usb_port_number(buf);
			if(port_num != -1)
				sprintf(port, "%d", port_num);
			else
				strcpy(port, "0");

			follow_disk_info->port = port;
		}

		// start get vendor.
		if(get_disk_vendor(device_name, buf, 64) == NULL){
			usb_dbg("Fail to get disk's vendor: %s.\n", device_name);
			free_disk_data(&follow_disk_info);
			return NULL;
		}

		len = strlen(buf);
		if(len > 0){
			vendor = (char *)malloc(len+1);
			if(vendor == NULL){
				usb_dbg("No memory!!(vendor)\n");
				free_disk_data(&follow_disk_info);
				return NULL;
			}
			strncpy(vendor, buf, len);
			vendor[len] = 0;
			strntrim(vendor);

			follow_disk_info->vendor = vendor;
		}

		// start get model.
		if(get_disk_model(device_name, buf, 64) == NULL){
			usb_dbg("Fail to get disk's model: %s.\n", device_name);
			free_disk_data(&follow_disk_info);
			return NULL;
		}

		len = strlen(buf);
		if(len > 0){
			model = (char *)malloc(len+1);
			if(model == NULL){
				usb_dbg("No memory!!(model)\n");
				free_disk_data(&follow_disk_info);
				return NULL;
			}
			strncpy(model, buf, len);
			model[len] = 0;
			strntrim(model);

			follow_disk_info->model = model;
		}

		// get USB's tag
		memset(buf, 0, 64);
		len = 0;
		ptr = buf;
		if(vendor != NULL){
			len += strlen(vendor);
			strcpy(ptr, vendor);
			ptr += len;
		}
		if(model != NULL){
			if(len > 0){
				++len; // Add a space between vendor and model.
				strcpy(ptr, " ");
				++ptr;
			}
			len += strlen(model);
			strcpy(ptr, model);
			ptr += len;
		}

		if(len > 0){
			follow_disk_info->tag = (char *)malloc(len+1);
			if(follow_disk_info->tag == NULL){
				usb_dbg("No memory!!(follow_disk_info->tag)\n");
				free_disk_data(&follow_disk_info);
				return NULL;
			}
			strcpy(follow_disk_info->tag, buf);
			follow_disk_info->tag[len] = 0;
		}
		else{
			len = strlen(DEFAULT_USB_TAG);

			follow_disk_info->tag = (char *)malloc(len+1);
			if(follow_disk_info->tag == NULL){
				usb_dbg("No memory!!(follow_disk_info->tag)\n");
				free_disk_data(&follow_disk_info);
				return NULL;
			}
			strcpy(follow_disk_info->tag, DEFAULT_USB_TAG);
			follow_disk_info->tag[len] = 0;
		}

		follow_partition_list = &(follow_disk_info->partitions);
		while(*follow_partition_list != NULL)
			follow_partition_list = &((*follow_partition_list)->next);

		new_partition_info = create_partition(device_name, follow_partition_list);
		if(new_partition_info != NULL){
			new_partition_info->disk = follow_disk_info;

			++(follow_disk_info->partition_number);
			++(follow_disk_info->mounted_number);
			if(!strcmp(new_partition_info->device, follow_disk_info->device))
				new_partition_info->size_in_kilobytes = follow_disk_info->size_in_kilobytes-4;
		}
	}

	if(!strcmp(follow_disk_info->device, follow_disk_info->partitions->device))
		get_disk_partitionnumber(device_name, &(follow_disk_info->partition_number), &(follow_disk_info->mounted_number));

	*new_disk_info = follow_disk_info;

	return *new_disk_info;
}

extern disk_info_t *initial_disk_data(disk_info_t **disk_info_list){
	disk_info_t *follow_disk;

	if(disk_info_list == NULL)
		return NULL;

	*disk_info_list = (disk_info_t *)malloc(sizeof(disk_info_t));
	if(*disk_info_list == NULL)
		return NULL;

	follow_disk = *disk_info_list;

	follow_disk->tag = NULL;
	follow_disk->vendor = NULL;
	follow_disk->model = NULL;
	follow_disk->device = NULL;
	follow_disk->major = (u32)0;
	follow_disk->minor = (u32)0;
	follow_disk->port = NULL;
	follow_disk->partition_number = (u32)0;
	follow_disk->mounted_number = (u32)0;
	follow_disk->size_in_kilobytes = (u64)0;
	follow_disk->partitions = NULL;
	follow_disk->next = NULL;

	return follow_disk;
}

extern void free_disk_data(disk_info_t **disk_info_list){
	disk_info_t *follow_disk, *old_disk;

	if(disk_info_list == NULL)
		return;

	follow_disk = *disk_info_list;
	while(follow_disk != NULL){
		if(follow_disk->tag != NULL)
			free(follow_disk->tag);
		if(follow_disk->vendor != NULL)
			free(follow_disk->vendor);
		if(follow_disk->model != NULL)
			free(follow_disk->model);
		if(follow_disk->device != NULL)
			free(follow_disk->device);
		if(follow_disk->port != NULL)
			free(follow_disk->port);

		free_partition_data(&(follow_disk->partitions));

		old_disk = follow_disk;
		follow_disk = follow_disk->next;
		free(old_disk);
	}
}

extern int get_disk_major_minor(const char *disk_name, u32 *major, u32 *minor){
	FILE *fp;
	char target_file[128], buf[8], *ptr;

	if(major == NULL || minor == NULL)
		return 0;

	*major = 0; // initial value.
	*minor = 0; // initial value.

	if(disk_name == NULL || !is_disk_name(disk_name))
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/dev", SYS_BLOCK, disk_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return 0;

	memset(buf, 0, 8);
	ptr = fgets(buf, 8, fp);
	fclose(fp);
	if(ptr == NULL)
		return 0;

	if((ptr = strchr(buf, ':')) == NULL)
		return 0;

	ptr[0] = '\0';
	*major = (u32)strtol(buf, NULL, 10);
	*minor = (u32)strtol(ptr+1, NULL, 10);

	return 1;
}

extern int get_disk_size(const char *disk_name, u64 *size_in_kilobytes){
	FILE *fp;
	char target_file[128], buf[16], *ptr;

	if(size_in_kilobytes == NULL)
		return 0;

	*size_in_kilobytes = 0; // initial value.

	if(disk_name == NULL || !is_disk_name(disk_name))
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/size", SYS_BLOCK, disk_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return 0;

	memset(buf, 0, 16);
	ptr = fgets(buf, 16, fp);
	fclose(fp);
	if(ptr == NULL)
		return 0;

	*size_in_kilobytes = ((u64)strtoll(buf, NULL, 10))/2;

	return 1;
}

extern char *get_disk_vendor(const char *disk_name, char *buf, const int buf_size){
	FILE *fp;
	char target_file[128], *ptr;
	int len;

	if(buf_size <= 0)
		return NULL;

	if(disk_name == NULL || !is_disk_name(disk_name))
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/device/vendor", SYS_BLOCK, disk_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_disk_model(const char *disk_name, char *buf, const int buf_size){
	FILE *fp;
	char target_file[128], *ptr;
	int len;

	if(buf_size <= 0)
		return NULL;

	if(disk_name == NULL || !is_disk_name(disk_name))
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/device/model", SYS_BLOCK, disk_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern int get_disk_partitionnumber(const char *string, u32 *partition_number, u32 *mounted_number){
	char disk_name[8];
	char target_path[128];
	DIR *dp;
	struct dirent *file;
	int len;
	char *mount_info = NULL, target[8];

	if(string == NULL)
		return 0;

	if(partition_number == NULL)
		return 0;

	*partition_number = 0; // initial value.
	if(mounted_number != NULL){
		*mounted_number = 0; // initial value.
		mount_info = read_whole_file(MOUNT_FILE);
	}

	len = strlen(string);
	if(!is_disk_name(string)){
		while(isdigit(string[len-1]))
			--len;
	}
	else if(mounted_number != NULL && mount_info != NULL){
		memset(target, 0, 8);
		sprintf(target, "%s ", string);
		if(strstr(mount_info, target) != NULL)
			++(*mounted_number);
	}
	memset(disk_name, 0, 8);
	strncpy(disk_name, string, len);

	memset(target_path, 0, 128);
	sprintf(target_path, "%s/%s", SYS_BLOCK, disk_name);
	if((dp = opendir(target_path)) == NULL){
		if(mount_info != NULL)
			free(mount_info);
		return 0;
	}

	len = strlen(disk_name);
	while((file = readdir(dp)) != NULL){
		if(file->d_name[0] == '.')
			continue;

		if(!strncmp(file->d_name, disk_name, len)){
			++(*partition_number);

			if(mounted_number == NULL || mount_info == NULL)
				continue;

			memset(target, 0, 8);
			sprintf(target, "%s ", file->d_name);
			if(strstr(mount_info, target) != NULL)
				++(*mounted_number);
		}
	}
	closedir(dp);
	if(mount_info != NULL)
		free(mount_info);

	return 1;
}

extern int is_partition_name(const char *device_name, u32 *partition_order){
	int order;
	u32 partition_number;

	if(partition_order != NULL)
		*partition_order = 0;

	if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
		return 0;

	// get the partition number in the device_name
	order = (u32)strtol(device_name+3, NULL, 10);
	if(order <= 0 || order == LONG_MIN || order == LONG_MAX)
		return 0;

	if(!get_disk_partitionnumber(device_name, &partition_number, NULL))
		return 0;

	if(partition_order != NULL)
		*partition_order = order;

	return 1;
}

int find_partition_label(const char *dev_name, char *label){
	struct volume_id id;
	char dev_path[128];

	memset(dev_path, 0, 128);
	sprintf(dev_path, "/dev/%s", dev_name);

	memset(&id, 0x00, sizeof(id));
	if((id.fd = open(dev_path, O_RDONLY)) < 0)
		return 0;

	if(label) *label = 0;

	volume_id_get_buffer(&id, 0, SB_BUFFER_SIZE);

	if(volume_id_probe_linux_swap(&id) == 0 || id.error)
		goto ret;
	if(volume_id_probe_ext(&id) == 0 || id.error)
		goto ret;
	if(volume_id_probe_vfat(&id) == 0 || id.error)
		goto ret;
	if(volume_id_probe_ntfs(&id) == 0 || id.error)
		goto ret;
ret:
	volume_id_free_buffer(&id);
	if(label && (*id.label != 0))
		strcpy(label, id.label);
	close(id.fd);
	return (label && *label != 0);
}

extern partition_info_t *create_partition(const char *device_name, partition_info_t **new_part_info){
	partition_info_t *follow_part_info;
	char label[128];
	u32 partition_order = 0;
	u64 size_in_kilobytes = 0, total_kilobytes = 0, used_kilobytes = 0;
	char buf1[PATH_MAX], buf2[64], buf3[PATH_MAX]; // options of mount info needs more buffer size.
	int len;

	if(new_part_info == NULL){
		usb_dbg("Bad input!!\n");
		return NULL;
	}

	*new_part_info = NULL; // initial value.

	if(device_name == NULL || get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
		return NULL;

	if(!is_disk_name(device_name) && !is_partition_name(device_name, &partition_order))
		return NULL;

	if(initial_part_data(&follow_part_info) == NULL){
		usb_dbg("No memory!!(follow_part_info)\n");
		return NULL;
	}

	len = strlen(device_name);
	follow_part_info->device = (char *)malloc(len+1);
	if(follow_part_info->device == NULL){
		usb_dbg("No memory!!(follow_part_info->device)\n");
		free_partition_data(&follow_part_info);
		return NULL;
	}
	strncpy(follow_part_info->device, device_name, len);
	follow_part_info->device[len] = 0;

	if(find_partition_label(device_name, label)){
		strntrim(label);
		len = strlen(label);
		follow_part_info->label = (char *)malloc(len+1);
		if(follow_part_info->label == NULL){
			usb_dbg("No memory!!(follow_part_info->label)\n");
			free_partition_data(&follow_part_info);
			return NULL;
		}
		strncpy(follow_part_info->label, label, len);
		follow_part_info->label[len] = 0;
	}

	follow_part_info->partition_order = partition_order;

	if(read_mount_data(device_name, buf1, PATH_MAX, buf2, 64, buf3, PATH_MAX)){
		len = strlen(buf1);
		follow_part_info->mount_point = (char *)malloc(len+1);
		if(follow_part_info->mount_point == NULL){
			usb_dbg("No memory!!(follow_part_info->mount_point)\n");
			free_partition_data(&follow_part_info);
			return NULL;
		}
		strncpy(follow_part_info->mount_point, buf1, len);
		follow_part_info->mount_point[len] = 0;

		len = strlen(buf2);
		follow_part_info->file_system = (char *)malloc(len+1);
		if(follow_part_info->file_system == NULL){
			usb_dbg("No memory!!(follow_part_info->file_system)\n");
			free_partition_data(&follow_part_info);
			return NULL;
		}
		strncpy(follow_part_info->file_system, buf2, len);
		follow_part_info->file_system[len] = 0;

		len = strlen(buf3);
		follow_part_info->permission = (char *)malloc(len+1);
		if(follow_part_info->permission == NULL){
			usb_dbg("No memory!!(follow_part_info->permission)\n");
			free_partition_data(&follow_part_info);
			return NULL;
		}
		strncpy(follow_part_info->permission, buf3, len);
		follow_part_info->permission[len] = 0;

		if(get_mount_size(follow_part_info->mount_point, &total_kilobytes, &used_kilobytes)){
			follow_part_info->size_in_kilobytes = total_kilobytes;
			follow_part_info->used_kilobytes = used_kilobytes;
		}
	}
	else{
		/*if(is_disk_name(device_name)){	// Disk
			free_partition_data(&follow_part_info);
			return NULL;
		}
		else{//*/
			len = strlen(PARTITION_TYPE_UNKNOWN);
			follow_part_info->file_system = (char *)malloc(len+1);
			if(follow_part_info->file_system == NULL){
				usb_dbg("No memory!!(follow_part_info->file_system)\n");
				free_partition_data(&follow_part_info);
				return NULL;
			}
			strncpy(follow_part_info->file_system, PARTITION_TYPE_UNKNOWN, len);
			follow_part_info->file_system[len] = 0;

			get_partition_size(device_name, &size_in_kilobytes);
			follow_part_info->size_in_kilobytes = size_in_kilobytes;
		//}
	}

	*new_part_info = follow_part_info;

	return *new_part_info;
}

extern partition_info_t *initial_part_data(partition_info_t **part_info_list){
	partition_info_t *follow_part;

	if(part_info_list == NULL)
		return NULL;

	*part_info_list = (partition_info_t *)malloc(sizeof(partition_info_t));
	if(*part_info_list == NULL)
		return NULL;

	follow_part = *part_info_list;

	follow_part->device = NULL;
	follow_part->label = NULL;
	follow_part->partition_order = (u32)0;
	follow_part->mount_point = NULL;
	follow_part->file_system = NULL;
	follow_part->permission = NULL;
	follow_part->size_in_kilobytes = (u64)0;
	follow_part->used_kilobytes = (u64)0;
	follow_part->disk = NULL;
	follow_part->next = NULL;

	return follow_part;
}

extern void free_partition_data(partition_info_t **partition_info_list){
	partition_info_t *follow_partition, *old_partition;

	if(partition_info_list == NULL)
		return;

	follow_partition = *partition_info_list;
	while(follow_partition != NULL){
		if(follow_partition->device != NULL)
			free(follow_partition->device);
		if(follow_partition->label != NULL)
			free(follow_partition->label);
		if(follow_partition->mount_point != NULL)
			free(follow_partition->mount_point);
		if(follow_partition->file_system != NULL)
			free(follow_partition->file_system);
		if(follow_partition->permission != NULL)
			free(follow_partition->permission);

		follow_partition->disk = NULL;

		old_partition = follow_partition;
		follow_partition = follow_partition->next;
		free(old_partition);
	}
}

extern int get_partition_size(const char *partition_name, u64 *size_in_kilobytes){
	FILE *fp;
	char disk_name[4];
	char target_file[128], buf[16], *ptr;

	if(size_in_kilobytes == NULL)
		return 0;

	*size_in_kilobytes = 0; // initial value.

	if(!is_partition_name(partition_name, NULL))
		return 0;

	strncpy(disk_name, partition_name, 3);
	disk_name[3] = 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/%s/size", SYS_BLOCK, disk_name, partition_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return 0;

	memset(buf, 0, 16);
	ptr = fgets(buf, 16, fp);
	fclose(fp);
	if(ptr == NULL)
		return 0;

	*size_in_kilobytes = ((u64)strtoll(buf, NULL, 10))/2;

	return 1;
}

extern int read_mount_data(const char *device_name
		, char *mount_point, int mount_len
		, char *type, int type_len
		, char *right, int right_len
		){
	char *mount_info = read_whole_file(MOUNT_FILE);
	char *start, line[256];
	char target[8];

	if(mount_point == NULL || mount_len <= 0
			|| type == NULL || type_len <= 0
			|| right == NULL || right_len <= 0
			){
		usb_dbg("Bad input!!\n");
		return 0;
	}

	if(mount_info == NULL){
		usb_dbg("Failed to open \"%s\"!!\n", MOUNT_FILE);
		return 0;
	}

	memset(target, 0, 8);
	sprintf(target, "%s ", device_name);

	if((start = strstr(mount_info, target)) == NULL){
		//usb_dbg("disk_initial:: %s: Failed to execute strstr()!\n", device_name);
		free(mount_info);
		return 0;
	}

	start += strlen(target);

	if(get_line_from_buffer(start, line, 256) == NULL){
		usb_dbg("%s: Failed to execute get_line_from_buffer()!\n", device_name);
		free(mount_info);
		return 0;
	}

	memset(mount_point, 0, mount_len);
	memset(type, 0, type_len);
	memset(right, 0, right_len);

	if(sscanf(line, "%s %s %[^\n ]", mount_point, type, right) != 3){
		usb_dbg("%s: Failed to execute sscanf()!\n", device_name);
		free(mount_info);
		return 0;
	}

	right[2] = 0;

	free(mount_info);
	return 1;
}

extern int get_mount_path(const char *const pool, char *mount_path, int mount_len){
	char type[64], right[PATH_MAX];

	return read_mount_data(pool, mount_path, mount_len, type, 64, right, PATH_MAX);
}

extern int get_mount_size(const char *mount_point, u64 *total_kilobytes, u64 *used_kilobytes){
	u64 total_size, free_size, used_size;
	struct statfs fsbuf;

	if(total_kilobytes == NULL || used_kilobytes == NULL)
		return 0;

	*total_kilobytes = 0;
	*used_kilobytes = 0;

	if(statfs(mount_point, &fsbuf))
		return 0;

	total_size = (u64)((u64)fsbuf.f_blocks*(u64)fsbuf.f_bsize);
	free_size = (u64)((u64)fsbuf.f_bfree*(u64)fsbuf.f_bsize);
	used_size = total_size-free_size;

	*total_kilobytes = total_size/1024;
	*used_kilobytes = used_size/1024;

	return 1;
}

extern char *get_disk_name(const char *string, char *buf, const int buf_size){
	int len;

	if(string == NULL || buf_size <= 0)
		return NULL;

	if(!is_disk_name(string) && !is_partition_name(string, NULL))
		return NULL;

	len = strlen(string);
	if(!is_disk_name(string)){
		while(isdigit(string[len-1]))
			--len;
	}

	if(len > buf_size)
		return NULL;

	memset(buf, 0, buf_size);
	strncpy(buf, string, len);

	return buf;
}

extern void print_disk(const disk_info_t *const disk_info){
	if(disk_info == NULL){
		usb_dbg("No disk!\n");
		return;
	}

	usb_dbg("Disk:\n");
	usb_dbg("              tag: %s.\n", disk_info->tag);
	usb_dbg("           vendor: %s.\n", disk_info->vendor);
	usb_dbg("            model: %s.\n", disk_info->model);
	usb_dbg("           device: %s.\n", disk_info->device);
	usb_dbg("            major: %u.\n", disk_info->major);
	usb_dbg("            minor: %u.\n", disk_info->minor);
	usb_dbg(" partition_number: %u.\n", disk_info->partition_number);
	usb_dbg("   mounted_number: %u.\n", disk_info->mounted_number);
	usb_dbg("size_in_kilobytes: %llu.\n", disk_info->size_in_kilobytes);

	print_partitions(disk_info->partitions);
}

extern void print_disks(const disk_info_t *const disk_list){
	disk_info_t *follow_disk;

	if(disk_list == NULL){
		usb_dbg("No disk list!\n");
		return;
	}

	for(follow_disk = (disk_info_t *)disk_list; follow_disk != NULL; follow_disk = follow_disk->next)
		print_disk(follow_disk);
}

extern void print_partition(const partition_info_t *const partition_info){
	if(partition_info == NULL){
		usb_dbg("No partition!\n");
		return;
	}

	usb_dbg("Partition:\n");
	if(partition_info->disk != NULL)
		usb_dbg("      Parent disk: %s.\n", partition_info->disk->device);
	usb_dbg("           device: %s.\n", partition_info->device);
	usb_dbg("            label: %s.\n", partition_info->label);
	usb_dbg("  partition_order: %u.\n", partition_info->partition_order);
	usb_dbg("      mount_point: %s.\n", partition_info->mount_point);
	usb_dbg("      file_system: %s.\n", partition_info->file_system);
	usb_dbg("       permission: %s.\n", partition_info->permission);
	usb_dbg("size_in_kilobytes: %llu.\n", partition_info->size_in_kilobytes);
	usb_dbg("   used_kilobytes: %llu.\n", partition_info->used_kilobytes);
}

extern void print_partitions(const partition_info_t *const partition_list){
	partition_info_t *follow_partition;

	if(partition_list == NULL){
		usb_dbg("No partition list!\n");
		return;
	}

	for(follow_partition = (partition_info_t *)partition_list; follow_partition != NULL; follow_partition = follow_partition->next)
		print_partition(follow_partition);
}
