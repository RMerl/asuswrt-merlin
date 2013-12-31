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
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#include <rc.h>
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#define TIMEMACHINE_BACKUP_NAME	"Backups.backupdb"
#define CNID_DIR_NAME		"CNID"
#define AFP_CONFIG_PATH		"/tmp/netatalk"
#define AFP_LOCK_PATH		"/var/lock/netatalk"
#define AFP_CONFIG_FN		"afp.conf"
#define AVAHI_CONFIG_PATH	"/tmp/avahi"
#define AVAHI_SERVICES_PATH	"/tmp/avahi/services"
#define AVAHI_CONFIG_FN		"avahi-daemon.conf"
#define AVAHI_AFPD_SERVICE_FN	"afpd.service"
#define AVAHI_ADISK_SERVICE_FN	"adisk.service"
#define TIMEMACHINE_TOKEN_FILE  ".tm_tokenfile.txt"

int generate_afp_config(char *mpname)
{
	FILE *fp;
	char afp_config[80];
	char et0macaddr[18];
	int ret = 0;

	sprintf(afp_config, "%s/%s", AFP_CONFIG_PATH, AFP_CONFIG_FN);

	strcpy(et0macaddr, nvram_safe_get("et0macaddr"));

	/* Generate afp configuration file */
	if (!(fp = fopen(afp_config, "w"))) {
		perror(afp_config);
		return -1;
	}

	/* Set [Global] configuration */
	fprintf(fp, "[Global]\n");
	fprintf(fp, "uam list = uams_dhx.so,uams_dhx2.so\n");
	fprintf(fp, "save password = no\n");
	fprintf(fp, "vol dbpath = %s/%s\n", mpname, CNID_DIR_NAME);
	//fprintf(fp, "server quantum = 32000\n");
	fprintf(fp, "sleep time = 1\n");
	fprintf(fp, "hostname = %s-%c%c%c%c.local\n", nvram_safe_get("model"),et0macaddr[12],et0macaddr[13],et0macaddr[15],et0macaddr[16]);
	fprintf(fp, "signature = %s-%c%c%c%c.local\n",nvram_safe_get("model"),et0macaddr[12],et0macaddr[13],et0macaddr[15],et0macaddr[16]);
	if (nvram_match("tm_debug", "1")){
		fprintf(fp, "log file = %s/netatalk.log\n", mpname);
		//fprintf(fp, "log file = /var/log/netatalk.log\n");
		fprintf(fp, "log level = default:maxdebug \n");	
	}

	/* Set the configuration of timemachine folder */
	fprintf(fp, "\n[%s]\n", TIMEMACHINE_BACKUP_NAME);
	fprintf(fp, "path = %s\n", mpname);
	fprintf(fp, "cnid scheme = dbd\n");
	fprintf(fp, "ea = auto\n");
	fprintf(fp, "time machine = yes\n");
	if(nvram_get_int("tm_vol_size") > 0)
		fprintf(fp, "vol size limit = %s\n", nvram_get("tm_vol_size"));

	fclose(fp);

	return ret;
}

int start_afpd()
{
	char afp_config[80];
	int ret = 0;

	sprintf(afp_config, "%s/%s", AFP_CONFIG_PATH, AFP_CONFIG_FN);

	// Execute afpd daemon
	//ret = eval("afpd", "-F", afp_config);
	system("afpd -F /tmp/netatalk/afp.conf");

	_dprintf("start_afpd: ret= %d\n", ret);

	return ret;
}

void stop_afpd()
{
	if (getpid() != 1) {
		notify_rc("stop_afpd");
	}

	//killall_tk("afpd");
	system("killall -SIGKILL afpd");
	return;
}

int start_cnid_metad()
{
	char afp_config[80];
	int ret = 0;

	sprintf(afp_config, "%s/%s", AFP_CONFIG_PATH, AFP_CONFIG_FN);

	// Execute cnid_metad daemon
	//ret = eval("cnid_metad", "-F", afp_config);
	system("cnid_metad -F /tmp/netatalk/afp.conf");

	_dprintf("start_cnid_metad: ret= %d\n", ret);

	return ret;
}

void stop_cnid_metad()
{
	if (getpid() != 1) {
		notify_rc("stop_cnid_metad");
	}

	//killall_tk("cnid_metad");
	system("killall -SIGKILL cnid_metad");
	return;
}
#if 0
int generate_avahi_config()
{
	FILE *fp;
	char avahi_config[80];
	char et0macaddr[18];
	int ret = 0;

	sprintf(avahi_config, "%s/%s", AVAHI_CONFIG_PATH, AVAHI_CONFIG_FN);
	strcpy(et0macaddr, nvram_safe_get("et0macaddr"));
	/* Generate avahi configuration file */
	if (!(fp = fopen(avahi_config, "w"))) {
		perror(avahi_config);
		return -1;
	}

	/* Set [server] configuration */
	fprintf(fp, "[Server]\n");
	fprintf(fp, "host-name=%s-%c%c%c%c\n", nvram_safe_get("model"),et0macaddr[12],et0macaddr[13],et0macaddr[15],et0macaddr[16]);
	fprintf(fp, "use-ipv4=yes\n");
	fprintf(fp, "use-ipv6=no\n");
	fprintf(fp, "deny-interfaces=%s\n", nvram_safe_get("wan0_ifname"));
	fprintf(fp, "ratelimit-interval-usec=1000000\n");
	fprintf(fp, "ratelimit-burst=1000\n");

	/* Set [wide-area] configuration */
	fprintf(fp, "\n[wide-area]\n");
	fprintf(fp, "enable-wide-area=yes\n");

	/* Set [rlimits] configuration */
	fprintf(fp, "\n[rlimits]\n");
	fprintf(fp, "rlimit-core=0\n");
	fprintf(fp, "rlimit-data=4194304\n");
	fprintf(fp, "rlimit-fsize=0\n");
	fprintf(fp, "rlimit-nofile=768\n");
	fprintf(fp, "rlimit-stack=4194304\n");
	fprintf(fp, "rlimit-nproc=3\n");

	fclose(fp);

	return ret;
}

int generate_afpd_service_config()
{
	FILE *fp;
	char afpd_service_config[80];
	int ret = 0;

	sprintf(afpd_service_config, "%s/%s", AVAHI_SERVICES_PATH, AVAHI_AFPD_SERVICE_FN);

	/* Generate afpd service configuration file */
	if (!(fp = fopen(afpd_service_config, "w"))) {
		perror(afpd_service_config);
		return -1;
	}

	fprintf(fp, "<service-group>\n");
	fprintf(fp, "<name replace-wildcards=\"yes\">%%h</name>\n");
	fprintf(fp, "<service>\n");
	fprintf(fp, "<type>_afpovertcp._tcp</type>\n");
	fprintf(fp, "<port>548</port>\n");
	fprintf(fp, "</service>\n");
	fprintf(fp, "<service>\n");
	fprintf(fp, "<type>_device-info._tcp</type>\n");
	fprintf(fp, "<port>0</port>\n");
	fprintf(fp, "<txt-record>model=Xserve</txt-record>\n");
	fprintf(fp, "</service>\n");
	fprintf(fp, "</service-group>\n");

	fclose(fp);

	return ret;
}

int generate_adisk_service_config()
{
	FILE *fp;
	char adisk_service_config[80];
	int ret = 0;

	sprintf(adisk_service_config, "%s/%s", AVAHI_SERVICES_PATH, AVAHI_ADISK_SERVICE_FN);

	/* Generate adisk service configuration file */
	if (!(fp = fopen(adisk_service_config, "w"))) {
		perror(adisk_service_config);
		return -1;
	}

	fprintf(fp, "<service-group>\n");
	fprintf(fp, "<name replace-wildcards=\"yes\">%%h</name>\n");
	fprintf(fp, "<service>\n");
	fprintf(fp, "<type>_adisk._tcp</type>\n");
	fprintf(fp, "<port>9</port>\n");
	fprintf(fp, "<txt-record>dk0=adVN=%s,adVF=0x81</txt-record>\n", TIMEMACHINE_BACKUP_NAME);
	fprintf(fp, "</service>\n");
	fprintf(fp, "</service-group>\n");

	fclose(fp);

	return ret;
}
#endif
int generate_timemachine_token_file(char *mpname)
{
	FILE *fp;
	char timemachine_token_file[80];
	int ret = 0;

	sprintf(timemachine_token_file, "%s", mpname);

	/* Generate afp configuration file */
	if (!(fp = fopen(timemachine_token_file, "w"))) {
		perror(timemachine_token_file);
		return -1;
	}

	fclose(fp);

	return ret;
}
#if 0

int start_avahi_daemon()
{
	int ret = 0;

	generate_avahi_config();
	generate_afpd_service_config();
	generate_adisk_service_config();

	// Execute avahi_daemon daemon
	//ret = eval("avahi-daemon");
	system("avahi-daemon &");

	_dprintf("start_avahi_daemon: ret= %d\n", ret);

	return ret;
}

void stop_avahi_daemon()
{
	if (getpid() != 1) {
		notify_rc("stop_avahi_daemon");
	}

	//killall_tk("avahi-daemon");
	system("killall -SIGKILL avahi-daemon");
	return;
}

#endif
char *find_mountpoint(char *device_name)
{
	FILE *procpt;
	char line[256], devname[48], mpname[48], system_type[10], mount_mode[128];
	int dummy1, dummy2;
	static char mount_point_name[32];
									       
	if ((procpt = fopen("/proc/mounts", "r")) != NULL)
	while (fgets(line, sizeof(line), procpt))
	{
		memset(mpname, 0x0, sizeof(mpname));
		if (sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
			continue;
		sprintf(mount_point_name, "/tmp/mnt/%s", device_name);		
		if (!strcmp( mpname, mount_point_name))
		{
			//nvram_set("tm_partition_num",devname);
			break;
		}
	}

	if (procpt)
		fclose(procpt);

	if(strlen(mount_point_name) > 0)
		return mount_point_name;
	return NULL;
}

char *find_exchange_mountpoint(char *device_name)
{
	FILE *procpt;
	char line[256], devname[48], mpname[48], system_type[10], mount_mode[128];
	int dummy1, dummy2;
	static char mount_point_name[32];
									       
	if ((procpt = fopen("/proc/mounts", "r")) != NULL)
	while (fgets(line, sizeof(line), procpt))
	{
		memset(mpname, 0x0, sizeof(mpname));
		if (sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
			continue;
		
		if (!strcmp( devname, device_name))
		{
			nvram_set("tm_device_name",mpname+9);
			nvram_set("tm_partition_num",devname);
			sprintf(mount_point_name, "%s", mpname);
			break;
		}
	}

	if (procpt)
		fclose(procpt);

	if(strlen(mount_point_name) > 0)
		return mount_point_name;
	return NULL;
}

void find_tokenfile_partition()
{
	char token_path[80];
	FILE *procpt;
	char line[256], devname[48], mpname[48], system_type[10], mount_mode[128];
	int dummy1, dummy2;
							       
	if ((procpt = fopen("/proc/mounts", "r")) != NULL)
	while (fgets(line, sizeof(line), procpt))
	{
		memset(mpname, 0x0, sizeof(mpname));
		if (sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
			continue;

		sprintf(token_path, "%s/%s", mpname, TIMEMACHINE_TOKEN_FILE);
		if(check_if_file_exist(token_path)){
			find_backup_mac_date(mpname);
		}
	}

	if (procpt)
		fclose(procpt);
}

void find_backup_mac_date(char *mpname)
{
	
	DIR *dir;
	struct dirent *ptr;
	char dir_path[80];
	char backup_path[80];
	char mac_name[48];
	char mp_name[48];
	char test_log[100];

	char change[] = "\n";

	sprintf(mp_name,"%s", mpname);
	write_timemachine_tokeninfo(mp_name);

	sprintf(dir_path,"%s/%s", mpname, TIMEMACHINE_BACKUP_NAME);
	sprintf(test_log,"dir_path = %s",dir_path);
	logmessage("Timemachine", test_log);
	if(!(dir = opendir(dir_path)))
		return;
	while((ptr = readdir(dir))!=NULL)
    	{
	if (!strncmp(ptr->d_name, "loop", 4) ||
	    !strncmp(ptr->d_name, "mtdblock", 8) ||
	    !strncmp(ptr->d_name, "ram", 3) ||
	    !strcmp(ptr->d_name, ".AppleDouble")||
	    !strcmp(ptr->d_name, ".") ||
	    !strcmp(ptr->d_name, "..")
	   )
		continue;
        	sprintf(backup_path,"%s/%s", dir_path, ptr->d_name);
        	sprintf(mac_name,"%s", ptr->d_name);		
		sprintf(test_log,"mac_name = %s",mac_name);
		logmessage("Timemachine", test_log);
		write_timemachine_tokeninfo(mac_name);
    	}
   	closedir(dir);

	//find date
	char history_file[128];
	char line[256], string_line_tmp[96], backup_time[48], backup_time_t[48], *string_line;
	sprintf(history_file,"%s/com.apple.TimeMachine.SnapshotHistory.plist", backup_path);
	FILE *fp_t;
	if ((fp_t = fopen(history_file, "r")) != NULL)
	while (fgets(line, sizeof(line), fp_t))
	{
		if ((string_line = strstr(line, "<string>")) != NULL){
			sprintf(string_line_tmp,"%s",string_line);
			sscanf(string_line_tmp, "<string>%s</string>", backup_time_t);
			strncpy(backup_time, backup_time_t, 17);
			//sprintf(test_log,"backup_time = %s",backup_time);
			//logmessage("Timemachine", test_log);
			write_timemachine_tokeninfo(backup_time);
		}
	}
	if (fp_t)
		fclose(fp_t);
	write_timemachine_tokeninfo(change);
}

void write_timemachine_tokeninfo(char *mpname)
{
	char test_log[100];

	FILE *fp;
	fp = fopen("/tmp/timemachine_info", "a");
	char *tmp = " ";
	if (fp==NULL) return;

	fputs(mpname, fp);
	fputs(tmp, fp);
	fclose(fp);
}

void clear_timemachine_tokeninfo()
{
	FILE *fp;

	if (!(fp = fopen("/tmp/timemachine_info", "w"))) {
		return -1;
	}
	
	fclose(fp);
}

int start_timemachine()
{
	int ret = 0;
	char cnid_path[80];
	char backup_path[80];
	char token_path[80];
	char test_log[100];
	char *mount_point_name;

	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	char usb1_vid[8], usb1_pid[8], usb1_serial[64];
	char usb2_vid[8], usb2_pid[8], usb2_serial[64];

	snprintf(prefix, sizeof(prefix), "usb_path%s", "1");
	memset(usb1_vid, 0, 8);
	strncpy(usb1_vid, nvram_safe_get(strcat_r(prefix, "_vid", tmp)), 8);
	memset(usb1_pid, 0, 8);
	strncpy(usb1_pid, nvram_safe_get(strcat_r(prefix, "_pid", tmp)), 8);
	memset(usb1_serial, 0, 64);
	strncpy(usb1_serial, nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 8);

	snprintf(prefix, sizeof(prefix), "usb_path%s", "2");
	memset(usb2_vid, 0, 8);
	strncpy(usb2_vid, nvram_safe_get(strcat_r(prefix, "_vid", tmp)), 8);
	memset(usb2_pid, 0, 8);
	strncpy(usb2_pid, nvram_safe_get(strcat_r(prefix, "_pid", tmp)), 8);
	memset(usb2_serial, 0, 64);
	strncpy(usb2_serial, nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 8);

	//clear_timemachine_tokeninfo();
	//find_tokenfile_partition();

	if(!nvram_match("timemachine_enable", "1"))
		return -1;

	if(nvram_safe_get("tm_device_name") == NULL)
	{
		_dprintf("Timemachine: no device name to backup\n");
		logmessage("Timemachine", "no device name to backup");
		return -1;
	}
#if 1
	if(nvram_match("tm_ui_setting", "1")){
		logmessage("Timemachine", "User select disk start TimeMachine");
		mount_point_name = find_mountpoint(nvram_safe_get("tm_device_name"));
		nvram_set("tm_ui_setting", "0");
	}else{
	//check mountpoint exchange or not
	if(!nvram_match("tm_usb_path_vid", "")
		&& (nvram_match("tm_usb_path_vid",usb1_vid)) 
		&& (nvram_match("tm_usb_path_pid",usb1_pid)) 
		&& (nvram_match("tm_usb_path_serial",usb1_serial)))
	{
		sprintf(test_log, "Time Machine interface1 change %s -> %s", nvram_safe_get("tm_device_name"),nvram_safe_get("tm_partition_num"));
		mount_point_name = find_mountpoint(nvram_safe_get("tm_device_name"));
		logmessage("Timemachine", test_log);
	}
	else if(!nvram_match("tm_usb_path_vid", "")
		&& (nvram_match("tm_usb_path_vid",usb2_vid)) 
		&& (nvram_match("tm_usb_path_pid",usb2_pid))
		&& (nvram_match("tm_usb_path_serial",usb2_serial)))
	{
		sprintf(test_log, "tm_device_name interface2 change %s -> %s", nvram_safe_get("tm_device_name"),nvram_safe_get("tm_partition_num"));
		mount_point_name = find_mountpoint(nvram_safe_get("tm_device_name"));
		logmessage("Timemachine", test_log);
	}else{
		logmessage("Timemachine", "No disk to auto start TimeMachine");
		mount_point_name = find_mountpoint(nvram_safe_get("tm_device_name"));
	}
	}
#endif

	if(mount_point_name == NULL)
	{
		_dprintf("Timemachine: can not find the correct mount point name\n");
		logmessage("Timemachine", "can not find the correct mount point name");
		return -1;
	}

	if(!check_if_dir_exist(mount_point_name))
	{
		_dprintf("Timemachine: mount point name doesn't exist\n");
		logmessage("Timemachine", "mount point name doesn't exist");
		return -1;
	}

	// Create directory for use by afpd daemon and configuration file
	//if(!check_if_dir_exist(AFP_LOCK_PATH)) mkdir(AFP_LOCK_PATH, 0777);
	//if(!check_if_dir_exist(AFP_CONFIG_PATH)) mkdir(AFP_CONFIG_PATH, 0777);
	mkdir_if_none(AFP_LOCK_PATH);
	mkdir_if_none(AFP_CONFIG_PATH);

	// Create directory for use by avahi daemon and configuration file
	//if(!check_if_dir_exist(AVAHI_CONFIG_PATH)) mkdir(AVAHI_CONFIG_PATH, 0777);
	//if(!check_if_dir_exist(AVAHI_SERVICES_PATH)) mkdir(AVAHI_SERVICES_PATH, 0777);
	mkdir_if_none(AVAHI_CONFIG_PATH);
	mkdir_if_none(AVAHI_SERVICES_PATH);

	// Create directory for use by cnid_metad and cnid_dbd daemon
	sprintf(cnid_path, "%s/%s", mount_point_name, CNID_DIR_NAME);
	sprintf(backup_path, "%s", mount_point_name);

	//Create token file
	//sprintf(token_path, "%s/%s", mount_point_name, TIMEMACHINE_TOKEN_FILE);
	//if(!check_if_file_exist(token_path))
	//	ret = generate_timemachine_token_file(token_path);

	//if(!check_if_dir_exist(cnid_path)) mkdir(cnid_path, 0777);
	//if(!check_if_dir_exist(backup_path)) mkdir(backup_path, 0777);
	mkdir_if_none(cnid_path);
	//mkdir_if_none(backup_path);

	ret = generate_afp_config(mount_point_name);
	start_afpd();
	start_cnid_metad();
	restart_mdns();

	logmessage("Timemachine", "daemon is started");

	return ret;
}

void stop_timemachine()
{
	stop_afpd();
	stop_cnid_metad();
	restart_mdns();
	logmessage("Timemachine", "daemon is stoped");
}


