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
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "usb_info.h"
#include "disk_io_tools.h"

extern char *read_whole_file(const char *target){
	FILE *fp;
	char *buffer, *new_str;
	int i;
	unsigned int read_bytes = 0;
	unsigned int each_size = 1024;

	if((fp = fopen(target, "r")) == NULL)
		return NULL;

	buffer = (char *)malloc(sizeof(char)*each_size);
	if(buffer == NULL){
		usb_dbg("No memory \"buffer\".\n");
		fclose(fp);
		return NULL;
	}
	memset(buffer, 0, each_size);

	while ((i = fread(buffer+read_bytes, each_size * sizeof(char), 1, fp)) == each_size){
		read_bytes += each_size;
		new_str = (char *)malloc(sizeof(char)*(each_size+read_bytes));
		if(new_str == NULL){
			usb_dbg("No memory \"new_str\".\n");
			free(buffer);
			fclose(fp);
			return NULL;
		}
		memset(new_str, 0, sizeof(char)*(each_size+read_bytes));
		memcpy(new_str, buffer, read_bytes);

		free(buffer);
		buffer = new_str;
	}

	fclose(fp);
	return buffer;
}

extern char *get_line_from_buffer(const char *buf, char *line, const int line_size){
	int buf_len, len;
	char *ptr;

	if(buf == NULL || (buf_len = strlen(buf)) <= 0)
		return NULL;

	if((ptr = strchr(buf, '\n')) == NULL)
		ptr = (char *)(buf+buf_len);

	if((len = ptr-buf) < 0)
		len = buf-ptr;
	++len; // include '\n'.

	memset(line, 0, line_size);
	strncpy(line, buf, len);

	return line;
}

/*extern int mkdir_if_none(char *dir){
	DIR *dp = opendir(dir);
	if(dp != NULL){
		closedir(dp);
		return 0;
	}

	umask(0000);
	return (!mkdir(dir, 0777))?1:0;
}//*/
extern int mkdir_if_none(const char *path){
	DIR *dp;
	char cmd[PATH_MAX];

	if(!(dp = opendir(path))){
		memset(cmd, 0, PATH_MAX);
		sprintf(cmd, "mkdir -m 0777 -p '%s'", (char *)path);
		system(cmd);
		return 1;
	}
	closedir(dp);

	return 0;
}

extern int delete_file_or_dir(char *target){
	if(test_if_dir(target))
		rmdir(target);
	else
		unlink(target);

	return 0;
}

extern int test_if_file(const char *file){
	FILE *fp = fopen(file, "r");

	if(fp == NULL)
		return 0;

	fclose(fp);
	return 1;
}

extern int test_if_dir(const char *dir){
	DIR *dp = opendir(dir);

	if(dp == NULL)
		return 0;

	closedir(dp);
	return 1;
}

extern int test_if_mount_point_of_pool(const char *dirname){
	char *mount_dir, *follow_info;
	int layer = 0;

	follow_info = (char *)dirname;
	while((follow_info = index(follow_info, '/')) != NULL){
		++follow_info;
		++layer;
	}

	if(layer != MOUNT_LAYER)
		return 0;

	mount_dir = rindex(dirname, '/');
	if(mount_dir == NULL)
		return 0;

	++mount_dir;
	if(mount_dir[0] == 0 || isspace(mount_dir[0]))
		return 0;

	if(mount_dir[0] == '.')
		return 0;

	if(!test_if_dir(dirname))
		return 0;

	return layer;
}

extern int test_if_System_folder(const char *const dirname){
	const char *const MS_System_folder[] = {"SYSTEM VOLUME INFORMATION", "RECYCLER", "RECYCLED", "$RECYCLE.BIN", NULL};
	const char *const Linux_System_folder[] = {"lost+found", NULL};
	int i;

	for(i = 0; MS_System_folder[i] != NULL; ++i){
		if(!upper_strcmp(dirname, MS_System_folder[i]))
			return 1;
	}

	for(i = 0; Linux_System_folder[i] != NULL; ++i){
		if(!upper_strcmp(dirname, Linux_System_folder[i]))
			return 1;
	}

	return 0;
}

extern int test_mounted_disk_size_status(char *diskpath){
	struct statfs fsbuf;
	unsigned long long block_size;

	if(statfs(diskpath, &fsbuf)){
		perror("*** statfs fail! - in test_mounted_disk_size_status()");
		return 0;
	}

	block_size = fsbuf.f_bsize;
	if(block_size*fsbuf.f_bfree/(1<<20) < (unsigned long long)33)
		return 1;
	else if(block_size*fsbuf.f_blocks/(1<<20) > (unsigned long long)256)
		return 2;
	else
		return 3;
}

extern void strntrim(char *str){
	register char *start, *end;
	int len;

	if(str == NULL)
		return;

	len = strlen(str);
	start = str;
	end = start+len-1;

	while(start < end && isspace(*start))
		++start;
	while(start <= end && isspace(*end))
		--end;

	end++;

	if((int)(end-start) < len){
		memcpy(str, start, (end-start));
		str[end-start] = 0;
	}

	return;
}

extern void write_escaped_value(FILE *fp, const char *value){
	const char *follow_value;

	follow_value = value;
	while (*follow_value != 0){
		if (*follow_value == '\''){
			fputc('\\', fp);
			fputc('\'', fp);
		}
		else if (*follow_value == '\"'){
			fputc('\\', fp);
			fputc('\"', fp);
		}
		else if (*follow_value == '\?'){
			fputc('\\', fp);
			fputc('\?', fp);
		}
		else if (*follow_value == '\\'){
			fputc('\\', fp);
			fputc('\\', fp);
		}
		else if (*follow_value == '\a'){
			fputc('\\', fp);
			fputc('a', fp);
		}
		else if (*follow_value == '\b'){
			fputc('\\', fp);
			fputc('b', fp);
		}
		else if (*follow_value == '\f'){
			fputc('\\', fp);
			fputc('f', fp);
		}
		else if (*follow_value == '\n'){
			fputc('\\', fp);
			fputc('n', fp);
		}
		else if (*follow_value == '\r'){
			fputc('\\', fp);
			fputc('r', fp);
		}
		else if (*follow_value == '\t'){
			fputc('\\', fp);
			fputc('\t', fp);
		}
		else if (*follow_value == '\v'){
			fputc('\\', fp);
			fputc('\v', fp);
		}
		else if (isprint(*follow_value)){
			fputc(*follow_value, fp);
		}
		else if (((unsigned char)(*follow_value) && 0x80) != 0){
			fputc(*follow_value, fp);
		}
		else{
			fprintf(fp, "\\%03o", (unsigned)(unsigned char)(*follow_value));
		}
		++follow_value;
	}
}
