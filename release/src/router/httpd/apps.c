#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shared.h>

#ifdef RTCONFIG_USB
#include <disk_io_tools.h>

#include "apps.h"

char *alloc_string(const char *string){
	char *buf;
	int len;

	len = strlen(string);
	if((buf = (char *)malloc(len+1)) == NULL)
		return NULL;

	strcpy(buf, string);
	buf[len] = 0;

	return buf;
}

char *get_status_field(const char *target, const char *field){
	char *buf;
	char *ptr_head, *ptr_tail, backup;

	if((ptr_head = strstr(target, field)) == NULL)
		return NULL;

	ptr_head += strlen(field);

	if((ptr_tail = strchr(ptr_head, '\n')) == NULL)
		ptr_tail = ptr_head+strlen(ptr_head);

	backup = ptr_tail[0];
	ptr_tail[0] = '\0';
	buf = alloc_string(ptr_head);
	ptr_tail[0] = backup;

	return buf;
}

apps_info_t *initial_apps_data(){
	apps_info_t *new_apps_info;

	new_apps_info = (apps_info_t *)malloc(sizeof(apps_info_t));
	if(new_apps_info == NULL)
		return NULL;

	new_apps_info->name = NULL;
	new_apps_info->version = NULL;
	new_apps_info->new_version = NULL;
	new_apps_info->installed = NULL;
	new_apps_info->enabled = NULL;
	new_apps_info->source = NULL;
	new_apps_info->url = NULL;
	new_apps_info->description = NULL;
	new_apps_info->depends = NULL;
	new_apps_info->optional_utility = NULL;
	new_apps_info->new_optional_utility = NULL;
	new_apps_info->help_path = NULL;
	new_apps_info->new_file_name = NULL;
	new_apps_info->from_owner = NULL;

	new_apps_info->next = NULL;

	return new_apps_info;
}

void free_apps_list(apps_info_t **apps_info_list){
	apps_info_t *apps_info, *old_apps_info;

	if(apps_info_list == NULL)
		return;

	apps_info = *apps_info_list;
	while(apps_info != NULL){
		if(apps_info->name != NULL)
			free(apps_info->name);
		if(apps_info->version != NULL)
			free(apps_info->version);
		if(apps_info->new_version != NULL)
			free(apps_info->new_version);
		if(apps_info->installed != NULL)
			free(apps_info->installed);
		if(apps_info->enabled != NULL)
			free(apps_info->enabled);
		if(apps_info->source != NULL)
			free(apps_info->source);
		if(apps_info->url != NULL)
			free(apps_info->url);
		if(apps_info->description != NULL)
			free(apps_info->description);
		if(apps_info->depends != NULL)
			free(apps_info->depends);
		if(apps_info->optional_utility != NULL)
			free(apps_info->optional_utility);
		if(apps_info->new_optional_utility != NULL)
			free(apps_info->new_optional_utility);
		if(apps_info->help_path != NULL)
			free(apps_info->help_path);
		if(apps_info->new_file_name != NULL)
			free(apps_info->new_file_name);
		if(apps_info->from_owner != NULL)
			free(apps_info->from_owner);

		old_apps_info = apps_info;
		apps_info = apps_info->next;
		free(old_apps_info);
	}
}

apps_info_t *get_apps_list(char *argv){
	apps_info_t *apps_info_list = NULL, **follow_apps_info_list = NULL, *follow_apps_info;
	char *apps_info;
	char *pkg_head, *pkg_tail;
	char info_name[128];
	FILE *fp;
	unsigned long file_size = 0;
	int pid;
	char *cmd[] = {"/usr/sbin/app_update.sh", NULL};
	char line[128], buf[4096];
	char *tmp_apps_name;
	int got_apps;
	char *STATUS;

	if(!argv || strcmp(argv, APP_OWNER_OTHERS)){
		// Get the newest version of the installed packages,
		// and information of the non-installed packages from APPS_LIST_ASUS.
		if((fp = fopen(APPS_LIST_ASUS, "r")) == NULL)
			return apps_info_list;

		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if(file_size <= 0){
_dprintf("httpd: get the Zero size of the ASUS APP list.\n");
			fclose(fp);
			_eval(cmd, NULL, 0, &pid);
			return apps_info_list;
		}

		memset(line, 0, sizeof(line));
		while(fgets(line, 128, fp) != NULL){
			if((tmp_apps_name = get_status_field(line, FIELD_PACKAGE)) == NULL)
				continue;

			memset(buf, 0, sizeof(buf));
			pkg_tail = pkg_head = buf;
			do{
				sprintf(pkg_tail, "%s", line);
				pkg_tail += strlen(line);

				memset(line, 0, sizeof(line));
			}while(fgets(line, 128, fp) != NULL && strlen(line) > 1);

			follow_apps_info = apps_info_list;
			got_apps = 0;
			while(follow_apps_info != NULL){
				if(!strcmp(follow_apps_info->name, tmp_apps_name)){
					got_apps = 1;
					break;
				}

				follow_apps_info = follow_apps_info->next;
			}
			free(tmp_apps_name);

			// Installed package.
			if(got_apps){
				follow_apps_info->new_version = get_status_field(pkg_head, FIELD_VERSION);
				follow_apps_info->new_optional_utility = get_status_field(pkg_head, FIELD_OPTIONALUTILITY);
				follow_apps_info->new_file_name = get_status_field(pkg_head, FIELD_FILENAME);
				if(follow_apps_info->from_owner != NULL)
					free(follow_apps_info->from_owner);
				follow_apps_info->from_owner = alloc_string(APP_OWNER_ASUS);
			}
			// Non-installed package.
			else{
				follow_apps_info_list = &apps_info_list;
				while(*follow_apps_info_list != NULL)
					follow_apps_info_list = &((*follow_apps_info_list)->next);

				*follow_apps_info_list = initial_apps_data();

				(*follow_apps_info_list)->name = get_status_field(pkg_head, FIELD_PACKAGE);
				(*follow_apps_info_list)->new_version = get_status_field(pkg_head, FIELD_VERSION);
				(*follow_apps_info_list)->installed = alloc_string(FIELD_NO);
				(*follow_apps_info_list)->enabled = alloc_string(FIELD_NO);
				(*follow_apps_info_list)->source = get_status_field(pkg_head, FIELD_SOURCE);
				(*follow_apps_info_list)->url = get_status_field(pkg_head, FIELD_URL);
				(*follow_apps_info_list)->description = get_status_field(pkg_head, FIELD_DESCRIPTION);
				(*follow_apps_info_list)->depends = get_status_field(pkg_head, FIELD_DEPENDS);
				(*follow_apps_info_list)->new_optional_utility = get_status_field(pkg_head, FIELD_OPTIONALUTILITY);
				(*follow_apps_info_list)->help_path = get_status_field(pkg_head, FIELD_HELPPATH);
				(*follow_apps_info_list)->new_file_name = get_status_field(pkg_head, FIELD_FILENAME);
				(*follow_apps_info_list)->from_owner = alloc_string(APP_OWNER_ASUS);
			}

			memset(line, 0, sizeof(line));
		}
		fclose(fp);
	}

	if(!argv || strcmp(argv, APP_OWNER_ASUS)){
		// Get the newest version of the installed packages,
		// and information of the non-installed packages from APPS_LIST_OLEG.
		if((fp = fopen(APPS_LIST_OLEG, "r")) == NULL)
			return apps_info_list;

		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if(file_size <= 0){
_dprintf("httpd: get the Zero size of the third-party APP list.\n");
			fclose(fp);
			_eval(cmd, NULL, 0, &pid);
			return apps_info_list;
		}

		memset(line, 0, sizeof(line));
		while(fgets(line, 128, fp) != NULL){
			if((tmp_apps_name = get_status_field(line, FIELD_PACKAGE)) == NULL)
				continue;

			memset(buf, 0, sizeof(buf));
			pkg_tail = pkg_head = buf;
			do{
				sprintf(pkg_tail, "%s", line);
				pkg_tail += strlen(line);

				memset(line, 0, sizeof(line));
			}while(fgets(line, 128, fp) != NULL && strlen(line) > 1);

			follow_apps_info = apps_info_list;
			got_apps = 0;
			while(follow_apps_info != NULL){
				if(!strcmp(follow_apps_info->name, tmp_apps_name)){
					got_apps = 1;
					break;
				}

				follow_apps_info = follow_apps_info->next;
			}
			free(tmp_apps_name);

			// Installed package.
			if(got_apps){
				follow_apps_info->new_version = get_status_field(pkg_head, FIELD_VERSION);
				follow_apps_info->new_optional_utility = get_status_field(pkg_head, FIELD_OPTIONALUTILITY);
				if(follow_apps_info->from_owner != NULL)
					free(follow_apps_info->from_owner);
				follow_apps_info->from_owner = alloc_string(APP_OWNER_OLEG);
			}
			// Non-installed package.
			else{
				follow_apps_info_list = &apps_info_list;
				while(*follow_apps_info_list != NULL)
					follow_apps_info_list = &((*follow_apps_info_list)->next);

				*follow_apps_info_list = initial_apps_data();

				(*follow_apps_info_list)->name = get_status_field(pkg_head, FIELD_PACKAGE);
				(*follow_apps_info_list)->new_version = get_status_field(pkg_head, FIELD_VERSION);
				(*follow_apps_info_list)->installed = alloc_string(FIELD_NO);
				(*follow_apps_info_list)->enabled = alloc_string(FIELD_NO);
				(*follow_apps_info_list)->source = get_status_field(pkg_head, FIELD_SOURCE);
				(*follow_apps_info_list)->url = get_status_field(pkg_head, FIELD_URL);
				(*follow_apps_info_list)->description = get_status_field(pkg_head, FIELD_DESCRIPTION);
				(*follow_apps_info_list)->depends = get_status_field(pkg_head, FIELD_DEPENDS);
				(*follow_apps_info_list)->new_optional_utility = get_status_field(pkg_head, FIELD_OPTIONALUTILITY);
				(*follow_apps_info_list)->help_path = get_status_field(pkg_head, FIELD_HELPPATH);
				(*follow_apps_info_list)->new_file_name = get_status_field(pkg_head, FIELD_FILENAME);
				(*follow_apps_info_list)->from_owner = alloc_string(APP_OWNER_OLEG);
			}

			memset(line, 0, sizeof(line));
		}
		fclose(fp);
	}

	// Get the name and version of the installed packages from APPS_STATUS.
	if((fp = fopen(APPS_STATUS, "r")) == NULL)
		return apps_info_list;

	memset(line, 0, sizeof(line));
	while(fgets(line, 128, fp) != NULL){
		if((tmp_apps_name = get_status_field(line, FIELD_PACKAGE)) == NULL)
			continue;

		memset(buf, 0, sizeof(buf));
		pkg_tail = pkg_head = buf;
		do{
			sprintf(pkg_tail, "%s", line);
			pkg_tail += strlen(line);

			memset(line, 0, sizeof(line));
		}while(fgets(line, 128, fp) != NULL && strlen(line) > 1);

		if((STATUS = get_status_field(pkg_head, FIELD_STATUS)) == NULL)
			continue;
		if(strstr(STATUS, "not-installed")){
			free(STATUS);
			continue;
		}
		free(STATUS);

		follow_apps_info = apps_info_list;
		got_apps = 0;
		while(follow_apps_info != NULL){
			if(!strcmp(follow_apps_info->name, tmp_apps_name)){
				got_apps = 1;
				break;
			}

			follow_apps_info = follow_apps_info->next;
		}
		free(tmp_apps_name);

		// Installed package.
		if(got_apps){
			follow_apps_info->version = get_status_field(pkg_head, FIELD_VERSION);
			if(follow_apps_info->installed != NULL)
				free(follow_apps_info->installed);
			follow_apps_info->installed = alloc_string(FIELD_YES);
		}
		// Non-installed package.
		else if(!argv || !strcmp(argv, APP_OWNER_ALL)){
			follow_apps_info_list = &apps_info_list;
			while(*follow_apps_info_list != NULL)
				follow_apps_info_list = &((*follow_apps_info_list)->next);

			*follow_apps_info_list = initial_apps_data();

			(*follow_apps_info_list)->name = get_status_field(pkg_head, FIELD_PACKAGE);
			(*follow_apps_info_list)->version = get_status_field(pkg_head, FIELD_VERSION);
			(*follow_apps_info_list)->installed = alloc_string(FIELD_YES);
			(*follow_apps_info_list)->from_owner = alloc_string(APP_OWNER_OTHERS);
		}

		memset(line, 0, sizeof(line));
	}
	fclose(fp);

	follow_apps_info = apps_info_list;
	while(follow_apps_info != NULL){
		// Get the source, description and depends of the installed packages from the control in APPS_INFO.
		memset(info_name, 0, sizeof(info_name));
		sprintf(info_name, "%s/%s.control", APPS_INFO, follow_apps_info->name);
		apps_info = read_whole_file(info_name);
		if(apps_info != NULL){
			if((tmp_apps_name = get_status_field(apps_info, FIELD_ENABLED)) != NULL){
				if(follow_apps_info->enabled != NULL)
					free(follow_apps_info->enabled);
				follow_apps_info->enabled = get_status_field(apps_info, FIELD_ENABLED);
				free(tmp_apps_name);
			}
			follow_apps_info->optional_utility = get_status_field(apps_info, FIELD_OPTIONALUTILITY);

			free(apps_info);
		}

		follow_apps_info = follow_apps_info->next;
	}

	return apps_info_list;
}

int printf_apps_info(const apps_info_t *follow_apps_info){
	while(follow_apps_info != NULL){
		apps_dbg("            name: %s.\n", follow_apps_info->name);
		apps_dbg("         version: %s.\n", follow_apps_info->version);
		apps_dbg("     new version: %s.\n", follow_apps_info->new_version);
		apps_dbg("       installed: %s.\n", follow_apps_info->installed);
		apps_dbg("         enabled: %s.\n", follow_apps_info->enabled);
		apps_dbg("          source: %s.\n", follow_apps_info->source);
		apps_dbg("             url: %s.\n", follow_apps_info->url);
		apps_dbg("     description: %s.\n", follow_apps_info->description);
		apps_dbg("         depends: %s.\n", follow_apps_info->depends);
		apps_dbg("optional_utility: %s.\n", follow_apps_info->optional_utility);
		apps_dbg("new_optional_utility: %s.\n", follow_apps_info->new_optional_utility);
		apps_dbg("       help_path: %s.\n", follow_apps_info->help_path);
		apps_dbg("   new_file_name: %s.\n", follow_apps_info->new_file_name);
		apps_dbg("      from_owner: %s.\n\n", follow_apps_info->from_owner);

		follow_apps_info = follow_apps_info->next;
	}

	return 0;
}

#ifdef APPS
int main(int argc, char *argv[]){
	apps_info_t *apps_info_list = get_apps_list(argv[1]);

	printf_apps_info(apps_info_list);

	free_apps_list(&apps_info_list);

	return 0;
}
#endif
#endif
