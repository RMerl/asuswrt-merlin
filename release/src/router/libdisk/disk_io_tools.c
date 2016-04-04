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

#include <shared.h>

#include "usb_info.h"
#include "disk_io_tools.h"

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
	int ret;

	if(check_if_dir_exist(target))
		ret = rmdir(target);
	else
		ret = unlink(target);

	return ret;
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

	if(!check_if_dir_exist(dirname))
		return 0;

	return layer;
}

extern int test_if_System_folder(const char *const dirname){
	const char *const MS_System_folder[] = {"SYSTEM VOLUME INFORMATION", "RECYCLER", "RECYCLED", "$RECYCLE.BIN", NULL};
	const char *const Linux_System_folder[] = {"lost+found", NULL};
	const char *const Mac_System_folder[] = {"Backups.backupdb", "CNID", NULL};
	const char *const ASUS_System_folder[] = {"asusware", NULL};
	int i;
	char *ptr;

	for(i = 0; MS_System_folder[i] != NULL; ++i){
		if(!upper_strcmp(dirname, MS_System_folder[i]))
			return 1;
	}

	for(i = 0; Linux_System_folder[i] != NULL; ++i){
		if(!upper_strcmp(dirname, Linux_System_folder[i]))
			return 1;
	}

	for(i = 0; Mac_System_folder[i] != NULL; ++i){
		if(!upper_strcmp(dirname, Mac_System_folder[i]))
			return 1;
	}

	i = strlen(dirname);
	ptr = (char *)dirname+i-16;
	if(i >= 16 && !strcmp(ptr, "Mac.sparsebundle"))
		return 1;

	for(i = 0; ASUS_System_folder[i] != NULL; ++i){
		if(!upper_strncmp(dirname, ASUS_System_folder[i], strlen(ASUS_System_folder[i])))
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

/*extern void write_escaped_value(FILE *fp, const char *value){
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
}//*/
