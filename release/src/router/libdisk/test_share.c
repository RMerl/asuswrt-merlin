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
#include <dirent.h>

#include "usb_info.h"
#include "disk_initial.h"
#include "disk_share.h"

#define VERSION 1

#include <bcmnvram.h>
#include <shutils.h>
#include <shared.h>
#include <stdarg.h>

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

	if(!strcmp(command, "set_file_integrity")){
		if(argc != 2)
			usb_dbg("Usage: set_file_integrity filename\n");
		else{
			set_file_integrity(argv[1]);
		}
	}
	else if(!strcmp(command, "check_file_integrity")){
		if(argc != 2)
			usb_dbg("Usage: check_file_integrity filename\n");
		else if(check_file_integrity(argv[1])){
			usb_dbg("Ok.\n");
		}
		else{
			usb_dbg("Broken.\n");
		}
	}
	else if(!strcmp(command, "get_account_list")){
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
	else if(!strcmp(command, "test_size")){
		unsigned long file_size = f_size(argv[1]);
		_dprintf("size: %lu.\n", file_size);
	}
	else
		usb_dbg("test_share(ver. %d): Need to link the command name from test_share first.\n", VERSION);

	return 0;
}
