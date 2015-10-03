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
#include <ctype.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <rtconfig.h>

#include "usb_info.h"
#include "disk_initial.h"
#include "disk_share.h"
#include <shared.h>
#include <linux/version.h>

#define SAMBA_CONF "/etc/smb.conf"

/* @return:
 * 	If mount_point is equal to one of partition of all disks case-insensitivity, return true.
 */
static int check_mount_point_icase(const disk_info_t *d_info, const partition_info_t *p_info, const disk_info_t *disk, const u32 part_nr, const char *m_point)
{
	int v = 0;
	const disk_info_t *d;
	const partition_info_t *p;

	if (!d_info || !p_info || !disk || part_nr > 15 || !m_point || *m_point == '\0')
		return 0;

	for (d = d_info; !v && d != NULL; d = d->next) {
		for (p = d->partitions; !v && p != NULL; p = p->next) {
			if (!p->mount_point || (d == disk && p->partition_order == part_nr))
				continue;

			if (strcasecmp(p->mount_point, m_point))
				continue;

			v = 1;
		}
	}

	return v;
}

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

/* For NETBIOS name,
 * 1. NetBIOS names are a sequence of alphanumeric characters.
 * 2. The hyphen ("-") and full-stop (".") characters may also be used
 *     in the NetBIOS name, but not as the first or last character.
 * 3. The NetBIOS name is 16 ASCII characters, however Microsoft limits
 *     the host name to 15 characters and reserves the 16th character
 *     as a NetBIOS Suffix
 */
int
is_valid_netbios_name(const char *name)
{
	int i, valid = 1;
	size_t len;

	if (!name)
		return 0;

	len = strlen(name);
	if (!len || len > 15)
		return 0;

	for (i = 0; valid && i < len; ++i) {
		if (isalnum(name[i]))
			continue;
		else if ((name[i] == '-' || name[i] == '.') && (i > 0 && i < (len - 1)))
			continue;

		valid = 0;
	}

	return valid;
}

int check_existed_share(const char *string)
{
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

int get_list_strings_count(char **list, int size, char *str)
{
	int i, count = 0;

	for (i = 0; i < size; i++)
		if (strcmp(list[i], str) == 0) count++;
	return count;
}

int main(int argc, char *argv[])
{
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
	int dup, same_m_pt = 0;
	char unique_share_name[PATH_MAX];

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
	p_computer_name = nvram_get("computer_name") && is_valid_netbios_name(nvram_get("computer_name")) ? nvram_get("computer_name") : get_productid();
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

	// account mode
	if(nvram_match("st_samba_mode", "2") || nvram_match("st_samba_mode", "4")
			|| (nvram_match("st_samba_mode", "1") && nvram_get("st_samba_force_mode") == NULL)
			){
		fprintf(fp, "security = USER\n");
		fprintf(fp, "guest ok = no\n");
		fprintf(fp, "map to guest = Bad User\n");
	}
	// share mode
	else if (nvram_match("st_samba_mode", "1") || nvram_match("st_samba_mode", "3")) {
#if 0
//#if defined(RTCONFIG_TFAT) || defined(RTCONFIG_TUXERA_NTFS) || defined(RTCONFIG_TUXERA_HFS)
		if(nvram_get_int("enable_samba_tuxera") == 1){
			fprintf(fp, "auth methods = guest\n");
			fprintf(fp, "guest account = admin\n");
			fprintf(fp, "map to guest = Bad Password\n");
			fprintf(fp, "guest ok = yes\n");
		}
		else{
			fprintf(fp, "security = SHARE\n");
			fprintf(fp, "guest only = yes\n");
		}
#else
		fprintf(fp, "security = SHARE\n");
		fprintf(fp, "guest only = yes\n");
#endif
	}
	else{
		usb_dbg("samba mode: no\n");
		goto confpage;
	}

	fprintf(fp, "encrypt passwords = yes\n");
	fprintf(fp, "pam password change = no\n");
	fprintf(fp, "null passwords = yes\n");		// ASUS add
#ifdef RTCONFIG_SAMBA_MODERN
	if (nvram_get_int("smbd_enable_smb2"))
		fprintf(fp, "max protocol = SMB2\n");
	else
		fprintf(fp, "max protocol = NT1\n");

	fprintf(fp, "passdb backend = smbpasswd\n");
	fprintf(fp, "smb encrypt = disabled\n");
	fprintf(fp, "smb passwd file = /etc/samba/smbpasswd\n");
#endif
#if 0
#ifdef RTCONFIG_RECVFILE
	fprintf(fp, "use recvfile = yes\n");
#endif
#endif
	fprintf(fp, "force directory mode = 0777\n");
	fprintf(fp, "force create mode = 0777\n");

	/* max users */
	if (strcmp(nvram_safe_get("st_max_user"), "") != 0)
		fprintf(fp, "max connections = %s\n", nvram_safe_get("st_max_user"));

        /* remove socket options due to NIC compatible issue */
	if(!nvram_get_int("stop_samba_speedup")){
#ifdef RTCONFIG_BCMARM
#ifdef RTCONFIG_BCM_7114
		fprintf(fp, "socket options = IPTOS_LOWDELAY TCP_NODELAY SO_RCVBUF=131072 SO_SNDBUF=131072\n");
#endif
#else
		fprintf(fp, "socket options = TCP_NODELAY SO_KEEPALIVE SO_RCVBUF=65536 SO_SNDBUF=65536\n");
#endif
	}
	fprintf(fp, "obey pam restrictions = no\n");
	fprintf(fp, "use spnego = no\n");		// ASUS add
	fprintf(fp, "client use spnego = no\n");	// ASUS add
//	fprintf(fp, "client use spnego = yes\n");  // ASUS add
	fprintf(fp, "disable spoolss = yes\n");		// ASUS add
	fprintf(fp, "host msdfs = no\n");		// ASUS add
	fprintf(fp, "strict allocate = No\n");		// ASUS add
//	fprintf(fp, "mangling method = hash2\n");	// ASUS add
	fprintf(fp, "bind interfaces only = yes\n");    // ASUS add

#ifndef RTCONFIG_BCMARM
	fprintf(fp, "interfaces = lo br0 %s\n", (is_routing_enabled() && nvram_get_int("smbd_wanac")) ? nvram_safe_get("wan0_ifname") : "");
#else
	fprintf(fp, "interfaces = br0 %s/%s %s\n", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"), (is_routing_enabled() && nvram_get_int("smbd_wanac")) ? nvram_safe_get("wan0_ifname") : "");
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
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

	fprintf(fp, "map archive = no\n");
	fprintf(fp, "map hidden = no\n");
	fprintf(fp, "map read only = no\n");
	fprintf(fp, "map system = no\n");
	fprintf(fp, "store dos attributes = yes\n");
	fprintf(fp, "dos filemode = yes\n");
	fprintf(fp, "oplocks = yes\n");
	fprintf(fp, "level2 oplocks = yes\n");
	fprintf(fp, "kernel oplocks = no\n");
	fprintf(fp, "wide links = no\n");

	// If we only want name services then skip share definition
	if (nvram_match("enable_samba", "0"))
		goto confpage;

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		usb_dbg("Couldn't get disk list when writing smb.conf!\n");
		goto confpage;
	}

	/* share */
	if (nvram_match("st_samba_mode", "0") || !strcmp(nvram_safe_get("st_samba_mode"), "")) {
		;
	}
	else if (nvram_match("st_samba_mode", "1") && nvram_match("st_samba_force_mode", "1")) {
		usb_dbg("samba mode: share\n");

		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;
				
				strcpy(unique_share_name, follow_partition->mount_point);
				do {
					dup = check_mount_point_icase(disks_info, follow_partition, follow_disk, follow_partition->partition_order, unique_share_name);
					if (dup)
						sprintf(unique_share_name, "%s(%d)", follow_partition->mount_point, ++same_m_pt);
				} while (dup);
				mount_folder = strrchr(unique_share_name, '/')+1;

				fprintf(fp, "[%s]\n", mount_folder);
				fprintf(fp, "comment = %s's %s\n", follow_disk->tag, mount_folder);
				fprintf(fp, "veto files = /.__*.txt*/asus_lighttpdpasswd/\n");
				fprintf(fp, "path = %s\n", follow_partition->mount_point);
				fprintf(fp, "writeable = yes\n");

				fprintf(fp, "dos filetimes = yes\n");
				fprintf(fp, "fake directory create times = yes\n");
			}
		}
	}
	else if (nvram_match("st_samba_mode", "2")) {
		usb_dbg("samba mode: share\n");

		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point == NULL)
					continue;

				strcpy(unique_share_name, follow_partition->mount_point);
				do {
					dup = check_mount_point_icase(disks_info, follow_partition, follow_disk, follow_partition->partition_order, unique_share_name);
					if (dup)
						sprintf(unique_share_name, "%s(%d)", follow_partition->mount_point, ++same_m_pt);
				} while (dup);
				mount_folder = strrchr(unique_share_name, '/')+1;

				node_layer = get_permission(NULL, follow_partition->mount_point, NULL, "cifs");
				if(node_layer == 3){
					fprintf(fp, "[%s]\n", mount_folder);
					fprintf(fp, "comment = %s's %s\n", follow_disk->tag, mount_folder);
					fprintf(fp, "path = %s\n", follow_partition->mount_point);
					fprintf(fp, "writeable = yes\n");

					fprintf(fp, "dos filetimes = yes\n");
					fprintf(fp, "fake directory create times = yes\n");
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
							int count = get_list_strings_count(folder_list, sh_num, folder_list[n]);
							if ((!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) && (count <= 1)) {
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

							fprintf(fp, "dos filetimes = yes\n");
							fprintf(fp, "fake directory create times = yes\n");
						}
					}

					free_2_dimension_list(&sh_num, &folder_list);
				}
			}
		}
	}
	else if (nvram_match("st_samba_mode", "3")) {
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
						int count = get_list_strings_count(folder_list, sh_num, folder_list[n]);
						if ((!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) && (count <= 1)) {
							fprintf(fp, "[%s]\n", folder_list[n]);
						} else {
							fprintf(fp, "[%s (at %s)]\n", folder_list[n], mount_folder);
						}
						fprintf(fp, "comment = %s's %s in %s\n", mount_folder, folder_list[n], follow_disk->tag);
						fprintf(fp, "path = %s/%s\n", follow_partition->mount_point, folder_list[n]);
					}

					fprintf(fp, "dos filetimes = yes\n");
					fprintf(fp, "fake directory create times = yes\n");

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
	else if (nvram_match("st_samba_mode", "4")
			|| (nvram_match("st_samba_mode", "1") && nvram_get("st_samba_force_mode") == NULL)
			) {
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

					int count = get_list_strings_count(folder_list, sh_num, folder_list[n]);
					if ((!strcmp(nvram_safe_get("smbd_simpler_naming"), "1")) && (count <= 1)) {
						fprintf(fp, "[%s]\n", folder_list[n]);
					} else {
						fprintf(fp, "[%s (at %s)]\n", folder_list[n], mount_folder);
					}
					fprintf(fp, "comment = %s's %s in %s\n", mount_folder, folder_list[n], follow_disk->tag);
					fprintf(fp, "path = %s/%s\n", follow_partition->mount_point, folder_list[n]);

					fprintf(fp, "dos filetimes = yes\n");
					fprintf(fp, "fake directory create times = yes\n");

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

		append_custom_config("smb.conf", fp);
		fclose(fp);

		use_custom_config("smb.conf", SAMBA_CONF);
		run_postconf("smb", SAMBA_CONF);
	}

	free_disk_data(&disks_info);
	return 0;
}
