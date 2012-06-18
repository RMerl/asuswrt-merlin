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
#include "disk_share.h"

#include <semaphore_mfp.h>

#define VERSION 1

#include <bcmnvram.h>
#include <shutils.h>
#include <stdarg.h>

/*#define vstrsep(buf, sep, args...) _vstrsep(buf, sep, args, NULL)
int _vstrsep(char *buf, const char *sep, ...)
{
	va_list ap;
	char **p;
	int n;

	n = 0;
	va_start(ap, sep);
	while ((p = va_arg(ap, char **)) != NULL) {
		if ((*p = strsep(&buf, sep)) == NULL) break;
		++n;
	}
	va_end(ap);
	return n;
}

// Success: return value is account number. Fail: return value is -1.
int get_account_list(int *acc_num, char ***account_list) {
	char **tmp_account_list, **tmp_account;
	int len, i, j;
	char *nv, *nvp, *b;
	char *account, *passwd;

	*acc_num = atoi(nvram_safe_get("acc_num"));
	if(*acc_num <= 0)
		return 0;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
dbg("b=%s.\n", b);
			if(vstrsep(b, ">", &account, &passwd) != 2)
				continue;

			tmp_account = (char **)malloc(sizeof(char *)*(i+1));
			if(tmp_account == NULL){
				usb_dbg("Can't malloc \"tmp_account\".\n");
				free(nv);
				return -1;
			}

			len = strlen(account);
			tmp_account[i] = (char *)malloc(sizeof(char)*(len+1));
			if(tmp_account[i] == NULL){
				usb_dbg("Can't malloc \"tmp_account[%d]\".\n", i);
				free(tmp_account);
				free(nv);
				return -1;
			}
			strcpy(tmp_account[i], account);
			tmp_account[i][len] = 0;

			if(i != 0){
				for(j = 0; j < i; ++j)
					tmp_account[j] = tmp_account_list[j];

				free(tmp_account_list);
				tmp_account_list = tmp_account;
			}
			else
				tmp_account_list = tmp_account;

			if(++i >= *acc_num)
				break;
		}
		free(nv);

		*account_list = tmp_account_list;

		return *acc_num;
	}

	if(nv)
		free(nv);

	return 0;
}

int add_account(const char *const account, const char *const password){
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num;
	int result, len;
	char nvram_value[PATH_MAX], *ptr;

	if(account == NULL || strlen(account) <= 0){
		usb_dbg("No input, \"account\".\n");
		return -1;
	}
	if(password == NULL || strlen(password) <= 0){
		usb_dbg("No input, \"password\".\n");
		return -1;
	}
	if(test_if_exist_account(account)){
		usb_dbg("\"%s\" is already created.\n", account);
		return -1;
	}

	acc_num = atoi(nvram_safe_get("acc_num"));
	if(acc_num < 0)
		acc_num = 0;
	if(acc_num >= MAX_ACCOUNT_NUM){
		usb_dbg("Too many accounts are created.\n");
		return -1;
	}

	memset(nvram_value, 0, PATH_MAX);
	strcpy(nvram_value, nvram_safe_get("acc_list"));
	len = strlen(nvram_value);
	if(len > 0){
		ptr = nvram_value+len;
		sprintf(ptr, "<%s>%s", account, password);
		nvram_set("acc_list", nvram_value);

		sprintf(nvram_value, "%d", acc_num+1);
		nvram_set("acc_num", nvram_value);
	}
	else{
		sprintf(nvram_value, "%s>%s", account, password);
		nvram_set("acc_list", nvram_value);

		nvram_set("acc_num", "1");
	}

	spinlock_lock(SPINLOCK_NVRAMCommit);
	nvram_commit();
	spinlock_unlock(SPINLOCK_NVRAMCommit);

	disk_list = read_disk_data();
	if(disk_list == NULL){
		usb_dbg("Couldn't read the disk list out.\n");
		return -1;
	}

	for(follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next){			
		for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if(follow_partition->mount_point == NULL)
				continue;
			
			result = initial_var_file(account, follow_partition->mount_point);
			if(result != 0)
				usb_dbg("Can't initial the var file of \"%s\".\n", account);
		}
	}
	free_disk_data(&disk_list);

	return 0;
}

int del_account(const char *const account){
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num, i, len;
	char nvram_value[PATH_MAX], *ptr;
	char *var_file;
	char *nv, *nvp, *b;
	char *tmp_account, *tmp_passwd;

	if(account == NULL || strlen(account) <= 0){
		usb_dbg("No input, \"account\".\n");
		return -1;
	}

	acc_num = atoi(nvram_safe_get("acc_num"));
	if(acc_num <= 0)
		return 0;

	memset(nvram_value, 0, PATH_MAX);
	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_account, &tmp_passwd) != 2)
				continue;

			if(!strcmp(account, tmp_account)){
				if(--acc_num == 0){
					nvram_set("acc_num", "0");
					nvram_set("acc_list", "");

					free(nv);
					return 0;
				}

				continue;
			}

			len = strlen(nvram_value);
			if(len > 0){
				ptr = nvram_value+len;
				sprintf(ptr, "<%s>%s", tmp_account, tmp_passwd);
			}
			else
				sprintf(nvram_value, "%s>%s", tmp_account, tmp_passwd);

			if(++i >= acc_num)
				break;
		}
		free(nv);
	}
	nvram_set("acc_list", nvram_value);

	memset(nvram_value, 0, PATH_MAX);
	sprintf(nvram_value, "%d", i);
	nvram_set("acc_num", nvram_value);

	if(i <= 0){
		nvram_set("st_samba_mode", "1");
		nvram_set("st_ftp_mode", "1");
#ifdef RTCONFIG_WEBDAV_OLD
		nvram_set("st_webdav_mode", "1");
#endif
	}

	spinlock_lock(SPINLOCK_NVRAMCommit);
	nvram_commit();
	spinlock_unlock(SPINLOCK_NVRAMCommit);

	disk_list = read_disk_data();
	if(disk_list == NULL){
		usb_dbg("Couldn't read the disk list out.\n");
		return -1;
	}

	for(follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next){			
		for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if(follow_partition->mount_point == NULL)
				continue;

			if(get_var_file_name(account, follow_partition->mount_point, &var_file)){
				usb_dbg("Can't malloc \"var_file\".\n");
				free_disk_data(&disk_list);
				return -1;
			}

			unlink(var_file);

			free(var_file);
		}
	}
	free_disk_data(&disk_list);

	return 0;
}

// "new_account" can be the same with "account" and only change the password!
int mod_account(const char *const account, const char *const new_account, const char *const new_password){
	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num;
	int i, len;
	char nvram_value[PATH_MAX], *ptr;
	char *var_file, *new_var_file;
	char *nv, *nvp, *b;
	char *tmp_account, *tmp_passwd;

	if(account == NULL || strlen(account) <= 0){
		usb_dbg("No input, \"account\".\n");
		return -1;
	}
	if(new_account == NULL || strlen(new_account) <= 0){
		usb_dbg("No input, \"new_account\".\n");
		return -1;
	}
	if(new_password == NULL || strlen(new_password) <= 0){
		usb_dbg("No input, \"new_password\".\n");
		return -1;
	}
	if(!test_if_exist_account(account)){
		usb_dbg("\"%s\" is not existed.\n", account);
		return -1;
	}
	if(test_if_exist_account(new_account)){
		usb_dbg("\"%s\" is already created.\n", new_account);
		return -1;
	}

	acc_num = atoi(nvram_safe_get("acc_num"));
	if(acc_num <= 0)
		return 0;

	memset(nvram_value, 0, PATH_MAX);
	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_account, &tmp_passwd) != 2)
				continue;

			len = strlen(nvram_value);
			if(!strcmp(account, tmp_account)){
				if(len == 0)
					sprintf(nvram_value, "%s>%s", new_account, new_password);
				else{
					ptr = nvram_value+len;
					sprintf(ptr, "<%s>%s", new_account, new_password);
				}
			}
			else{
				if(len == 0)
					sprintf(nvram_value, "%s>%s", tmp_account, tmp_passwd);
				else{
					ptr = nvram_value+len;
					sprintf(ptr, "<%s>%s", tmp_account, tmp_passwd);
				}
			}

			if(++i >= acc_num)
				break;
		}
		free(nv);
	}
	nvram_set("acc_list", nvram_value);

	spinlock_lock(SPINLOCK_NVRAMCommit);
	nvram_commit();
	spinlock_unlock(SPINLOCK_NVRAMCommit);

	if(!strcmp(new_account, account))
		goto rerun;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		usb_dbg("Couldn't read the disk list out.\n");
		return -1;
	}

	for(follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next){			
		for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if(follow_partition->mount_point == NULL)
				continue;

			if(get_var_file_name(account, follow_partition->mount_point, &var_file)){
				usb_dbg("Can't malloc \"var_file\".\n");
				free_disk_data(&disk_list);
				return -1;
			}

			if(get_var_file_name(new_account, follow_partition->mount_point, &new_var_file)){
				usb_dbg("Can't malloc \"new_var_file\".\n");
				free_disk_data(&disk_list);
				return -1;
			}

			rename(var_file, new_var_file);

			free(var_file);
			free(new_var_file);
		}
	}

rerun:

	return 0;
}//*/

int main(int argc, char *argv[]){
	char *command;
	int i, num;
	char **list;
	int layer, right;
	char *mount_path, *share;
	int result;

	if((command = rindex(argv[0], '/')) != NULL)
		++command;
	else
		command = argv[0];

	if(!strcmp(command, "get_account_list")){
		if(get_account_list(&num, &list) <= 0)
			usb_dbg("Can't get account list.\n");
		else{
			for(i = 0; i < num; ++i)
				usb_dbg("%dth account: %s.\n", i+1, list[i]);

			free_2_dimension_list(&num, &list);
		}
	}
	else if(!strcmp(command, "get_folder_list")){
		if(argc != 2)
			usb_dbg("Usage: get_folder_list mount_path\n");
		else if(get_folder_list(argv[1], &num, &list) < 0)
			usb_dbg("Can't get folder list.\n");
		else{
			for(i = 0; i < num; ++i)
				usb_dbg("%dth folder: %s.\n", i+1, list[i]);

			free_2_dimension_list(&num, &list);
		}
	}
	else if(!strcmp(command, "get_all_folder")){
		if(argc != 2)
			usb_dbg("Usage: get_all_folder mount_path\n");
		else if(get_all_folder(argv[1], &num, &list) < 0)
			usb_dbg("Can't get all folder.\n");
		else{
			for(i = 0; i < num; ++i)
				usb_dbg("%dth folder: %s.\n", i+1, list[i]);

			free_2_dimension_list(&num, &list);
		}
	}
	else if(!strcmp(command, "get_var_file_name")){
		char *file_name;

		if(argc != 3)
			usb_dbg("Usage: get_var_file_name account mount_path\n");
		else if(get_var_file_name(argv[1], argv[2], &file_name))
			usb_dbg("Can't get the var file name with %s in %s.\n", argv[1], argv[2]);
		else{
			usb_dbg("done: %s.\n", file_name);
			free(file_name);
		}
	}
	else if(!strcmp(command, "initial_folder_list")){
		if(argc != 2)
			usb_dbg("Usage: initial_folder_list mount_path\n");
		else if(initial_folder_list(argv[1]) < 0)
			usb_dbg("Can't initial folder list in %s.\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "initial_var_file")){
		char *target_acc = NULL;

		if(argc != 3){
			usb_dbg("Usage: initial_var_file account mount_path\n");
			return -1;
		}

		if(strcmp(argv[1], "NULL"))
			target_acc = argv[1];

		if(initial_var_file(target_acc, argv[2]) < 0){
			if(target_acc == NULL)
				usb_dbg("Can't initial share's var file in %s.\n", argv[2]);
			else
				usb_dbg("Can't initial %s's var file in %s.\n", target_acc, argv[2]);
		}
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "initial_all_var_file")){
		if(argc != 2)
			usb_dbg("Usage: initial_all_var_file mount_path\n");
		else if(initial_all_var_file(argv[1]) < 0)
			usb_dbg("Can't initial all var file in %s.\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "test_of_var_files")){
		if(argc != 2)
			usb_dbg("Usage: test_of_var_files mount_path\n");
		else if(test_of_var_files(argv[1]) < 0)
			usb_dbg("Can't test_of_var_files in %s.\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "create_if_no_var_files")){
		if(argc != 2)
			usb_dbg("Usage: create_if_no_var_files mount_path\n");
		else if(create_if_no_var_files(argv[1]) < 0)
			usb_dbg("Can't create var files in %s.\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "modify_if_exist_new_folder")){
		if(argc != 3)
			usb_dbg("Usage: modify_if_exist_new_folder account mount_path\n");
		else if(modify_if_exist_new_folder(argv[1], argv[2]) < 0)
			usb_dbg("Can't fix %s's var files in %s.\n", argv[1], argv[2]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "get_permission")){
		if(argc != 5)
			usb_dbg("Usage: get_permission account mount_path folder [cifs|ftp|dms]\n");
		else if((right = get_permission(argv[1], argv[2], argv[3], argv[4])) < 0)
			usb_dbg("%s can't get %s's %s permission in %s.\n", argv[1], argv[3], argv[4], argv[2]);
		else
			usb_dbg("done: %d.\n", right);
	}
	else if(!strcmp(command, "set_permission")){
		if(argc != 6)
			usb_dbg("Usage: set_permission account mount_path folder [cifs|ftp|dms] [0~3]\n");
		else if(set_permission(argv[1], argv[2], argv[3], argv[4], atoi(argv[5])) < 0)
			usb_dbg("%s can't set %s's %s permission to be %s in %s.\n", argv[1], argv[3], argv[4], argv[5], argv[2]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "add_account")){
		if(argc != 3)
			usb_dbg("Usage: add_account account password\n");
		else if(add_account(argv[1], argv[2]) < 0)
			usb_dbg("Can't add account(%s:%s).\n", argv[1], argv[2]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "del_account")){
		if(argc != 2)
			usb_dbg("Usage: del_account account\n");
		else if(del_account(argv[1]) < 0)
			usb_dbg("Can't del account(%s).\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "mod_account")){
		if(argc != 4)
			usb_dbg("Usage: mod_account account new_account new_password\n");
		else if(mod_account(argv[1], argv[2], argv[3]) < 0)
			usb_dbg("Can't mod account(%s) to (%s:%s).\n", argv[1], argv[2], argv[3]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "test_if_exist_account")){
		if(argc != 2)
			usb_dbg("Usage: test_if_exist_account account\n");
		else if(test_if_exist_account(argv[1]) < 0)
			usb_dbg("Can't test if %s is existed.\n", argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "add_folder")){
		if(argc != 4){
			usb_dbg("Usage: add_folder account mount_path folder\n");
			return 0;
		}
		
		if(!strcmp(argv[1], "NULL"))
			result = add_folder(NULL, argv[2], argv[3]);
		else
			result = add_folder(argv[1], argv[2], argv[3]);
		if(result < 0)
			usb_dbg("Can't add folder(%s) in %s.\n", argv[3], argv[2]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "del_folder")){
		if(argc != 3)
			usb_dbg("Usage: del_folder mount_path folder\n");
		else if(del_folder(argv[1], argv[2]) < 0)
			usb_dbg("Can't del folder(%s) in %s.\n", argv[2], argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "mod_folder")){
		if(argc != 4)
			usb_dbg("Usage: mod_folder mount_path folder new_folder\n");
		else if(mod_folder(argv[1], argv[2], argv[3]) < 0)
			usb_dbg("Can't mod folder(%s) to (%s) in %s.\n", argv[2], argv[3], argv[1]);
		else
			usb_dbg("done.\n");
	}
	else if(!strcmp(command, "test_if_exist_share")){
		if(argc != 3)
			usb_dbg("Usage: test_if_exist_share mount_path folder\n");
		else if(test_if_exist_share(argv[1], argv[2]))
			usb_dbg("%s is existed in %s.\n", argv[2], argv[1]);
		else
			usb_dbg("%s is NOT existed in %s.\n", argv[2], argv[1]);
	}
	else if(!strcmp(command, "how_many_layer")){
		if(argc != 2)
			usb_dbg("Usage: how_many_layer path\n");
		else if((layer = how_many_layer(argv[1], &mount_path, &share)) < 0)
			usb_dbg("Can't count the layer with %s.\n", argv[1]);
		else
			usb_dbg("done: %d layers, share=%s, mount_path=%s.\n", layer, share, mount_path);
	}
	else
		usb_dbg("test_share(ver. %d): Need to link the command name from test_share first.\n", VERSION);

	return 0;
}
