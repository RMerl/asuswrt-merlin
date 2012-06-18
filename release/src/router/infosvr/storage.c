#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>
#include <syslog.h>
#include "iboxcom.h"

#ifdef RTCONFIG_WEBDAV
int getStorageStatus(STORAGE_INFO_T *st)
{
	memset(st, sizeof(st), 0);
	st->MagicWord = EXTEND_MAGIC;
	st->ExtendCap = EXTEND_CAP_WEBDAV;
	if(nvram_get_int("enable_webdav")) 	
		st->u.wt.EnableWebDav = 1;
	else
		st->u.wt.EnableWebDav = 0;

	st->u.wt.HttpType = nvram_get_int("st_webdav_mode");
	st->u.wt.HttpPort = htons(nvram_get_int("webdav_http_port"));
	st->u.wt.HttpsPort = htons(nvram_get_int("webdav_https_port"));

	if(nvram_get_int("ddns_enable_x")) {
		st->u.wt.EnableDDNS = 1;
		snprintf(st->u.wt.HostName, sizeof(st->u.wt.HostName), nvram_safe_get("ddns_hostname_x"));
	}
	else {
		st->u.wt.EnableDDNS = 0;
	}

	// setup st->u.WANIPAddr
	st->u.wt.WANIPAddr = inet_network(get_wanip());

	st->u.wt.WANState = get_wanstate(); 
	st->u.wt.isNotDefault = nvram_get_int("x_Setting");
	return 0;
}
#else
#define MAXARRAY 64
#define MAXARRSIZE  64

#ifdef NOPARSER
int
parse_array(char *file, char *func, char *strarr)
{
	FILE *fp;
	char line[1024], *s, *l;
	int flag=0, len;
	int i;

	if (!(fp=fopen(file, "r+"))) return 0;
	
	while (fgets(line, sizeof(line), fp))
	{
		if (strstr(line, func)) flag=1;
		if (flag && strstr(line, "return"))
		{
			flag=2;
			break;
		}	
	}
	fclose(fp);

	if (flag!=2) return 0;

	l = strchr(line, ']');
	if (l) *l=0;
	l = strchr(line, '[');
	l++;
	s=l;
	len = strlen(s);
	while (strsep(&s, ","));

	i=0;
	/* Set token values */
	for (s = l; s < &l[len] && *s; s+=strlen(s)+1) 
	{
		strcpy(&strarr[i*MAXARRSIZE], s);
		i++;

		if (i==MAXARRAY) break;
	}
	return i;	
}

int isClaimedIDE(char *model)
{
	char tmparray[MAXARRAY][MAXARRSIZE];
	int tmpidx;
	unsigned long size=0;
	int i;

	tmpidx=parse_array("/tmp/diskstatus", "claimed_disk_interface_names", tmparray);

	for (i=0;i<tmpidx;i++)
	{
		printf("%d:%s\n", i, &tmparray[i][0]);
		if (strstr(&tmparray[i][0], "IDE")) break;
	}

	if (i==tmpidx) return 0;

	tmpidx=parse_array("/tmp/diskstatus", "claimed_disk_total_size", tmparray);
	if (i>=tmpidx) return 0;
	size = atoi(&tmparray[i][0]);
	
	tmpidx=parse_array("/tmp/diskstatus", "claimed_disk_model_info", tmparray);
	if (i>=tmpidx) return 0;
	strcpy(model, &tmparray[i][0]);
	return size;
}

int isBlankIDE(char *model)
{
	char tmparray[MAXARRAY][MAXARRSIZE];
	int tmpidx;
	unsigned long size=0;
	int i;

	tmpidx=parse_array("/tmp/diskstatus", "blank_disk_interface_names", tmparray);

	for (i=0;i<tmpidx;i++)
	{
		printf("%d:%s\n", i, &tmparray[i][0]);
		if (strstr(&tmparray[i][0], "IDE")) break;
	}

	if (i==tmpidx) return 0;

	tmpidx=parse_array("/tmp/diskstatus", "blank_disk_total_size", tmparray);
	if (i>=tmpidx) return 0;
	size = atoi(&tmparray[i][0]);
	
	tmpidx=parse_array("/tmp/diskstatus", "blank_disk_model_info", tmparray);
	if (i>=tmpidx) return 0;
	strcpy(model, &tmparray[i][0]);
	return size;
}


int isPrinter(char *model1, char *model2)
{
	char tmparray[MAXARRAY][MAXARRSIZE];
	int tmpidx;
	unsigned long size=0;
	int i;

	tmpidx=parse_array("/tmp/diskstatus", "printer_models", tmparray);
	if (tmpidx<=0) return 0;

	if (tmpidx>=1)
		strcpy(model1, &tmparray[0][0]);
	if (tmpidx>=2)
		strcpy(model2, &tmparray[1][0]);
	
	return tmpidx;
}
#endif

int is_apps_running()
{
	if (	pids("rtorrent") ||
		pids("dmathined") ||
		pids("giftd") ||
		pids("snarf")	)
	{
		return 1;
	}
	else
		return 0;
}

int
getStorageStatus(STORAGE_INFO_T *st)
{
//	char tmparray[MAXARRAY][MAXARRSIZE];
	char *username, *password;
	char *apps_ver, *apps_model;

	st->Capability=0;

	unsigned long apps_status=0;

	// count apps_status
	if (swap_check())
	{
		apps_status|=APPS_STATUS_SWAP;
	}
	if (nvram_match("apps_comp", "1"))
	{
		apps_status|=APPS_STATUS_FILECOMPLETENESS;
	}
	if (nvram_match("apps_disk_free", "1"))
	{
		apps_status|=APPS_STATUS_FREESPACE;
	}
	if (pids("smbd"))
	{
		apps_status|=APPS_STATUS_SAMBAMODE;
	}
	if (is_apps_running())
	{
		apps_status|=APPS_STATUS_RUNNING;
	}
	if (nvram_match("usb_path1", "storage") || nvram_match("usb_path2", "storage"))
	{
		apps_status|=APPS_STATUS_DISCONPORT;
	}
	if (nvram_match("apps_dl", "1"))
	{
		apps_status|=APPS_STATUS_DMMODE;
	}
	if (nvram_match("apps_status_checked", "1"))
	{
		apps_status|=APPS_STATUS_CHECKED;
	}
	if (nvram_match("usb_mnt_first_path_port", "1"))
	{
		apps_status|=APPS_STATUS_USBPORT1;
	}
	if (strcmp(nvram_safe_get("mnt_type"), "ntfs") == 0)
	{
		apps_status|=APPS_STATUS_FS_NTFS;
	}
	if(strcmp(nvram_safe_get("dm_block"), "1") == 0)
	{
		apps_status|=APPS_STATUS_BLOCKED;
	}
	if(!strcmp(nvram_safe_get("st_samba_mode"), "2") || !strcmp(nvram_safe_get("st_samba_mode"), "4"))
	{
		apps_status|=APPS_STATUS_SMBUSER;
	}
	if(strcmp(nvram_safe_get("slow_disk"), "1") == 0)
	{
		apps_status|=APPS_SLOW_DISK;
	}
	st->AppsStatus=apps_status;	// disable for tmp

	printf("\n[infosvr] apps status : %x\n", apps_status);	// tmp test
	if(nvram_invmatch("apps_pool", "") && nvram_invmatch("apps_share", ""))
	{
		st->Capability = (unsigned long) atoi(nvram_safe_get("apps_caps"));
		strcpy(st->AppsPool, nvram_safe_get("usb_mnt_first_path"));
		strcpy(st->AppsShare, nvram_safe_get("apps_share"));
	}
	else
	{
		st->Capability=0;
		st->AppsPool[0]=0;
		st->AppsShare[0]=0;
	}

	st->DiskSize=0;
	st->DiskStatus=DISK_STATUS_NONE;

	st->DiskModel[0]=0;	// disable for tmp

	memset(st->PrinterModel1, 0, 32);
	memset(st->PrinterModel2, 0, 32);
	if (strncmp(nvram_safe_get("st_ftp_modex"), "2", 1) == 0)
	{
		username = nvram_safe_get("acc_username0");
		password = nvram_safe_get("acc_password0");
		strncpy(st->PrinterModel1, username, strlen(username));
		strncpy(st->PrinterModel2, password, strlen(password));
	}
	else
	{
		strncpy(st->PrinterModel1, "anonymous", 9);
	}

	// copy apps_version to DiskModel
	apps_ver = nvram_get("apps_version");
	if(!apps_ver)
	{
		printf("get empty apps ver!");	// tmp test
		apps_ver = "0.0.0.0";
	}
	strncpy(st->DiskModel, apps_ver, strlen(apps_ver));	// tmp test
	printf("set apps version as %s\n", apps_ver);	// tmp test
	// copy apps_model_name to Apps_Model_Name
	apps_model = nvram_safe_get("apps_model_name");
	if(!apps_model)
	{
		printf("get empty apps model\n");	// tmp test
		apps_model = "no_model";
	}
	strncpy(st->Apps_Model_Name, apps_model, strlen(apps_model));	// tmp test
	printf("set apps model as %s\n", apps_model);	// tmp test
#ifdef NOPARSER
	// get from page
	system("wget http://127.0.0.1/diskstatus.asp -O /tmp/diskstatus");
	// find claimed ide
	if ((st->DiskSize=isClaimedIDE(st->DiskModel)))
	{
		st->DiskStatus=DISK_STATUS_CLAIMED;
		printf("chk 14: is claimed IDE\n");	// tmp test
	}
	else if ((st->DiskSize=isBlankIDE(st->DiskModel)))
	{	
		st->DiskStatus=DISK_STATUS_BLANK;
	}
	isPrinter(st->PrinterModel1, st->PrinterModel2);
#endif

	printf("Storage: %d %s\n", st->DiskStatus, st->AppsShare);	// tmp test
	return 0;
}
#endif
