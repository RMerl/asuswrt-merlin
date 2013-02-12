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
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <rtconfig.h>

#include "usb_info.h"
#include "disk_initial.h"
#include "disk_share.h"

#include <linux/version.h>

#define SAMBA_CONF "/etc/smb.conf"

int
is_invalid_char_for_hostname(char c)
{
	int ret = 0;

	if (c < 0x20)
		ret = 1;
	else if (c >= 0x21 && c <= 0x2c)
		ret = 1;
	else if (c >= 0x2e && c <= 0x2f)
		ret = 1;
	else if (c >= 0x3a && c <= 0x40)
		ret = 1;
#if 0
	else if (c >= 0x5b && c <= 0x60)
		ret = 1;
#else	/* allow '_' */
	else if (c >= 0x5b && c <= 0x5e)
		ret = 1;
	else if (c == 0x60)
		ret = 1;
#endif
	else if (c >= 0x7b)
		ret = 1;
#if 0
	printf("%c (0x%02x) is %svalid for hostname\n", c, c, (ret == 0) ? "  " : "in");
#endif
	return ret;
}

int
is_valid_hostname(const char *name)
{
	int ret = 1, len, i;

	if (!name)
		return 0;

	len = strlen(name);
	if (len == 0)
	{
		ret = 0;
		goto ENDERR;
	}

	for (i = 0; i < len ; i++)
		if (is_invalid_char_for_hostname(name[i]))
		{
			ret = 0;
			break;
		}

ENDERR:
#if 0
	printf("%s is %svalid for hostname\n", name, (ret == 1) ? "  " : "in");
#endif
	return ret;
}

int check_existed_share(const char *string){
	FILE *tp;
	char buf[PATH_MAX], target[256];

	if((tp = fopen(SAMBA_CONF, "r")) == NULL)
		return 0;

	if(string == NULL || strlen(string) <= 0)
		return 0;

	memset(target, 0, 256);
	sprintf(target, "[%s]", string);

	memset(buf, 0, PATH_MAX);
	while(fgets(buf, sizeof(buf), tp) != NULL){
		if(strstr(buf, target)){
			fclose(tp);
			return 1;
		}
	}

	fclose(tp);
	return 0;
}

int main(int argc, char *argv[]) {
	FILE *fp;
	char *nv;
	int n=0;
	char *p_computer_name = NULL;
	disk_info_t *follow_disk, *disks_info = NULL;
	partition_info_t *follow_partition;
	char *mount_folder;
	int result, node_layer, samba_right;
	int sh_num;
	char **folder_list = NULL;
	int acc_num;
	char **account_list;
	
	unlink("/var/log.samba");
	
	if ((fp=fopen(SAMBA_CONF, "r"))) {
		fclose(fp);
		unlink(SAMBA_CONF);
	}
	
	if((fp = fopen(SAMBA_CONF, "w")) == NULL)
		goto confpage;
	
	fprintf(fp, "[global]\n");
	if (nvram_safe_get("st_samba_workgroup"))
		fprintf(fp, "workgroup = %s\n", nvram_safe_get("st_samba_workgroup"));

#if 0
	if (nvram_safe_get("computer_name")) {
		fprintf(fp, "netbios name = %s\n", nvram_safe_get("computer_name"));
		fprintf(fp, "server string = %s\n", nvram_safe_get("computer_name"));
	}
#else
	p_computer_name = nvram_get("computer_name") && is_valid_hostname(nvram_get("computer_name")) ? nvram_get("computer_name") : get_productid();
	if (p_computer_name) {
		fprintf(fp, "netbios name = %s\n", p_computer_name);
		fprintf(fp, "server string = %s\n", p_computer_name);
	}
#endif

	fprintf(fp, "unix charset = UTF8\n");		// ASUS add
	fprintf(fp, "display charset = UTF8\n");	// ASUS add
	fprintf(fp, "log file = /var/log.samba\n");
	fprintf(fp, "log level = 0\n");
	fprintf(fp, "max log size = 5\n");
	
	/* share mode */
	if (!strcmp(nvram_safe_get("st_samba_mode"), "1") || !strcmp(nvram_safe_get("st_samba_mode"), "3")) {
		fprintf(fp, "security = SHARE\n");
		fprintf(fp, "guest only = yes\n");
	}
	else if (!strcmp(nvram_safe_get("st_samba_mode"), "2") || !strcmp(nvram_safe_get("st_samba_mode"), "4")) {
		fprintf(fp, "security = USER\n");
		fprintf(fp, "guest ok = no\n");
		fprintf(fp, "map to guest = Bad User\n");
	}
	else{
		usb_dbg("samba mode: no\n");
		goto confpage;
	}
	
	fprintf(fp, "encrypt passwords = yes\n");
	fprintf(fp, "pam password change = no\n");
	fprintf(fp, "null passwords = yes\n");		// ASUS add
	
	fprintf(fp, "force directory mode = 0777\n");
	fprintf(fp, "force create mode = 0777\n");
	
	/* max users */
	if (strcmp(nvram_safe_get("st_max_user"), "") != 0)
		fprintf(fp, "max connections = %s\n", nvram_safe_get("st_max_user"));
	
	fprintf(fp, "socket options = TCP_NODELAY SO_KEEPALIVE SO_RCVBUF=32768 SO_SNDBUF=32768\n");
	fprintf(fp, "obey pam restrictions = no\n");
	fprintf(fp, "use spne go = no\n");		// ASUS add
	fprintf(fp, "client use spnego = no\n");	// ASUS add
	// fprintf(fp, "client use spnego = yes\n");  // ASUS add
	fprintf(fp, "disable spoolss = yes\n");		// ASUS add
	fprintf(fp, "host msdfs = no\n");		// ASUS add
	fprintf(fp, "strict allocate = No\n");		// ASUS add
//	fprintf(fp, "mangling method = hash2\n");	// ASUS add
	fprintf(fp, "bind interfaces only = yes\n");	// ASUS add
	fprintf(fp, "interfaces = lo br0 %s\n", ( ( (!nvram_match("sw_mode", "3") && !nvram_match("sw_mode", "1")) || (!strcmp(nvram_safe_get("smbd_bind_wan"), "1")) ) ? nvram_safe_get("wan0_ifname") : "") );
	//	fprintf(fp, "dns proxy = no\n");				// J--
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
	fprintf(fp, "use sendfile = no\n");
#else
	fprintf(fp, "use sendfile = yes\n");
#endif
	if (!strcmp(nvram_safe_get("smbd_wins"), "1")) {
		fprintf(fp, "wins support = yes\n");
	}

	if (!strcmp(nvram_safe_get("smbd_master"), "1")) {
		fprintf(fp, "os level = 255\n");
		fprintf(fp, "domain master = yes\n");
		fprintf(fp, "local master = yes\n");
		fprintf(fp, "preferred master = yes\n");
	}

//	fprintf(fp, "domain master = no\n");				// J++
//	fprintf(fp, "wins support = no\n");				// J++
//	fprintf(fp, "printable = no\n");				// J++
//	fprintf(fp, "browseable = yes\n");				// J++
//	fprintf(fp, "security mask = 0777\n");				// J++
//	fprintf(fp, "force security mode = 0\n");			// J++
//	fprintf(fp, "directory security mask = 0777\n");		// J++
//	fprintf(fp, "force directory security mode = 0\n");		// J++

	fprintf(fp, "map archive = no\n");
	fprintf(fp, "map hidden = no\n");
	fprintf(fp, "map read only = no\n");
	fprintf(fp, "map system = no\n");
	fprintf(fp, "store dos attributes = yes\n");
	fprintf(fp, "dos filemode = yes\n");
	fprintf(fp, "dos filetimes = yes\n");
	fprintf(fp, "dos filetime resolution = yes\n");

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		usb_dbg("Couldn't get disk list when writing smb.conf!\n");
		goto confpage;
	}

	/* share */
	if (!strcmp(nvram_safe_get("st_samba_mode"), "0") || !strcmp(nvram_safe_get("st_samba_mode"), "")) {
		;
	}
	else if (!strcmp(nvram_safe_get("st_samba_mode"), "1")) {
		usb_dbg("samba mode: share\n");
		
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;
				
				mount_folder = strrchr(follow_partition->mount_point, '/')+1;
				
				fprintf(fp, "[%s]\n", mount_folder);
				fprintf(fp, "comment = %s's %s\n", follow_disk->tag, mount_folder);
				fprintf(fp, "path = %s\n", follow_partition->mount_point);
				fprintf(fp, "writeable = yes\n");
			}
		}
	}
	else if (!strcmp(nvram_safe_get("st_samba_mode"), "2")) {
		usb_dbg("samba mode: share\n");
		
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;
				
				mount_folder = strrchr(follow_partition->mount_point, '/')+1;
				
				node_layer = get_permission(NULL, follow_partition->mount_point, NULL, "cifs");
				if(node_layer == 3){
					fprintf(fp, "[%s]\n", mount_folder);
					fprintf(fp, "comment = %s's %s\n", follow_disk->tag, mount_folder);
					fprintf(fp, "path = %s\n", follow_partition->mount_point);
					fprintf(fp, "writeable = yes\n");
				}
				else{
					//result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
					result = get_folder_list(follow_partition->mount_point, &sh_num, &folder_list);
					if (result < 0){
						free_2_dimension_list(&sh_num, &folder_list);
						continue;
					}
					
					for (n = 0; n < sh_num; ++n){
						samba_right = get_permission(NULL, follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right < 0 || samba_right > 3)
							samba_right = DEFAULT_SAMBA_RIGHT;
						
						if(samba_right > 0){
							if (!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) {
								fprintf(fp, "[%s]\n", folder_list[n]);
							} else {
								fprintf(fp, "[%s (at %s)]\n", folder_list[n], mount_folder);
							}
							fprintf(fp, "comment = %s's %s in %s\n", mount_folder, folder_list[n], follow_disk->tag);
							fprintf(fp, "path = %s/%s\n", follow_partition->mount_point, folder_list[n]);
							if(samba_right == 3)
								fprintf(fp, "writeable = yes\n");
							else
								fprintf(fp, "writeable = no\n");
						}
					}
					
					free_2_dimension_list(&sh_num, &folder_list);
				}
			}
		}
	}
	else if (!strcmp(nvram_safe_get("st_samba_mode"), "3")) {
		usb_dbg("samba mode: user\n");
		
		// get the account list
		if (get_account_list(&acc_num, &account_list) < 0) {
			usb_dbg("Can't read the account list.\n");
			free_2_dimension_list(&acc_num, &account_list);
			goto confpage;
		}
		
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;
				
				mount_folder = strrchr(follow_partition->mount_point, '/')+1;
				
				// 1. get the folder list
				if (get_folder_list(follow_partition->mount_point, &sh_num, &folder_list) < 0) {
					free_2_dimension_list(&sh_num, &folder_list);
				}
				
				// 2. start to get every share
				for (n = -1; n < sh_num; ++n) {
					int i, first;
					
					if(n == -1){
						fprintf(fp, "[%s]\n", mount_folder);
						fprintf(fp, "comment = %s's %s\n", follow_disk->tag, mount_folder);
						fprintf(fp, "path = %s\n", follow_partition->mount_point);
					}
					else{
						if (!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) {
							fprintf(fp, "[%s]\n", folder_list[n]);
						} else {
							fprintf(fp, "[%s (at %s)]\n", folder_list[n], mount_folder);
						}
						fprintf(fp, "comment = %s's %s in %s\n", mount_folder, folder_list[n], follow_disk->tag);
						fprintf(fp, "path = %s/%s\n", follow_partition->mount_point, folder_list[n]);
					}
					
					fprintf(fp, "valid users = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						if(n == -1)
							samba_right = get_permission(account_list[i], follow_partition->mount_point, NULL, "cifs");
						else
							samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "invalid users = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						if(n == -1)
							samba_right = get_permission(account_list[i], follow_partition->mount_point, NULL, "cifs");
						else
							samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right >= 1)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "read list = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						if(n == -1)
							samba_right = get_permission(account_list[i], follow_partition->mount_point, NULL, "cifs");
						else
							samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right < 1)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "write list = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						if(n == -1)
							samba_right = get_permission(account_list[i], follow_partition->mount_point, NULL, "cifs");
						else
							samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right < 2)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
				}
				
				free_2_dimension_list(&sh_num, &folder_list);
			}
		}
		
		free_2_dimension_list(&acc_num, &account_list);
	}
	else if (!strcmp(nvram_safe_get("st_samba_mode"), "4")) {
		usb_dbg("samba mode: user\n");
		
		// get the account list
		if (get_account_list(&acc_num, &account_list) < 0) {
			usb_dbg("Can't read the account list.\n");
			free_2_dimension_list(&acc_num, &account_list);
			goto confpage;
		}
		
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;
				
				mount_folder = strrchr(follow_partition->mount_point, '/')+1;
				
				// 1. get the folder list
				if (get_folder_list(follow_partition->mount_point, &sh_num, &folder_list) < 0) {
					free_2_dimension_list(&sh_num, &folder_list);
				}
				
				// 2. start to get every share
				for (n = 0; n < sh_num; ++n) {
					int i, first;
					
					if (!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) {
						fprintf(fp, "[%s]\n", folder_list[n]);
					} else {
						fprintf(fp, "[%s (at %s)]\n", folder_list[n], mount_folder);
					}
					fprintf(fp, "comment = %s's %s in %s\n", mount_folder, folder_list[n], follow_disk->tag);
					fprintf(fp, "path = %s/%s\n", follow_partition->mount_point, folder_list[n]);
					
					fprintf(fp, "valid users = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						if(n == -1)
							samba_right = get_permission(account_list[i], follow_partition->mount_point, NULL, "cifs");
						else
							samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "invalid users = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right >= 1)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "read list = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right < 1)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
					
					fprintf(fp, "write list = ");
					first = 1;
					for (i = 0; i < acc_num; ++i) {
						samba_right = get_permission(account_list[i], follow_partition->mount_point, folder_list[n], "cifs");
						if (samba_right < 2)
							continue;
						
						if (first == 1)
							first = 0;
						else
							fprintf(fp, ", ");
						
						fprintf(fp, "%s", account_list[i]);
					}
					fprintf(fp, "\n");
				}
				
				free_2_dimension_list(&sh_num, &folder_list);
			}
		}
		
		free_2_dimension_list(&acc_num, &account_list);
	}
	
confpage:
	if(fp != NULL) {

		if (check_if_file_exist("/jffs/configs/smb.conf.add")) {
			char *addendum = read_whole_file("/jffs/configs/smb.conf.add");
			if (addendum) {
				fwrite(addendum, 1, strlen(addendum), fp);
				free(addendum);
			}

        	}
		fclose(fp);

		if (check_if_file_exist("/jffs/configs/smb.conf")) {
			eval("cp","/jffs/configs/smb.conf",SAMBA_CONF,NULL);
		}
	}

	free_disk_data(&disks_info);
	return 0;
}
