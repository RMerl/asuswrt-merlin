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
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <httpd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <bcmnvram_f.h>
#include <common.h>
#include <shared.h>
#include <rtstate.h>
#include <wlioctl.h>

#include <wlutils.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>


#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#include <disk_initial.h>
#include <disk_share.h>

#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#endif

#include "sysinfo.h"

unsigned int get_phy_temperature(int radio);
unsigned int get_wifi_clients(int radio, int querytype);

#define MBYTES 1024 / 1024

#define SI_WL_QUERY_ASSOC 1
#define SI_WL_QUERY_AUTHE 2
#define SI_WL_QUERY_AUTHO 3


int ej_show_sysinfo(int eid, webs_t wp, int argc, char_t ** argv)
{
	char *type;
	char result[2048];
	int retval = 0;
	struct sysinfo sys;
	char *tmp;

	strcpy(result,"None");

	if (ejArgs(argc, argv, "%s", &type) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return retval;
	}

	if (type) {
		if (strcmp(type,"cpu.model") == 0) {
			char *buffer = read_whole_file("/proc/cpuinfo");

			if (buffer) {
				tmp = strstr(buffer, "system type");
				if (tmp)
					sscanf(tmp, "system type  :  %[^\n]", result);
				free(buffer);
			}

		} else if(strcmp(type,"cpu.freq") == 0) {
			tmp = nvram_get("clkfreq");
			if (tmp)
				sscanf(tmp,"%[^,]s", result);

		} else if(strcmp(type,"memory.total") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.totalram/(float)MBYTES));
		} else if(strcmp(type,"memory.free") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.freeram/(float)MBYTES));
		} else if(strcmp(type,"memory.buffer") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.bufferram/(float)MBYTES));
		} else if(strcmp(type,"memory.swap.total") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.totalswap/(float)MBYTES));
		} else if(strcmp(type,"memory.swap.used") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",((sys.totalswap - sys.freeswap) / (float)MBYTES));
		} else if(strcmp(type,"cpu.load.1") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.loads[0] / (float)(1<<SI_LOAD_SHIFT)));
		} else if(strcmp(type,"cpu.load.5") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.loads[1] / (float)(1<<SI_LOAD_SHIFT)));
		} else if(strcmp(type,"cpu.load.15") == 0) {
			sysinfo(&sys);
			sprintf(result,"%.2f",(sys.loads[2] / (float)(1<<SI_LOAD_SHIFT)));
		} else if(strcmp(type,"nvram.total") == 0) {
			sprintf(result,"%d",NVRAM_SPACE);
		} else if(strcmp(type,"nvram.used") == 0) {
			char *buf;
			int size = 0;

			buf = malloc(NVRAM_SPACE);
			if (buf) {
				nvram_getall(buf, NVRAM_SPACE);
				tmp = buf;
				while (*tmp) tmp += strlen(tmp) +1;

				size = sizeof(struct nvram_header) + (int) tmp - (int) buf;
				free(buf);
			}
			sprintf(result,"%d",size);

		} else if(strcmp(type,"jffs.usage") == 0) {
			struct statvfs fiData;

			char *mount_info = read_whole_file("/proc/mounts");

			if ((mount_info) && (strstr(mount_info, "/jffs")) && (statvfs("/jffs",&fiData) == 0 )) {
				sprintf(result,"%.2f / %.2f MB",((fiData.f_blocks-fiData.f_bfree) * fiData.f_frsize / (float)MBYTES) ,(fiData.f_blocks * fiData.f_frsize / (float)MBYTES));
			} else {
				strcpy(result,"<i>Unmounted</i>");
			}

			if (mount_info) free(mount_info);

		} else if(strncmp(type,"temperature",11) == 0) {
			unsigned int temperature;
			int radio=0;

                        sscanf(type,"temperature.%d", &radio);
			temperature = get_phy_temperature(radio);
			if (temperature == 0)
				strcpy(result,"<i>disabled</i>");
			else
				sprintf(result,"%u&deg;C", temperature);

		} else if(strcmp(type,"conn.total") == 0) {
			FILE* fp;

			fp = fopen ("/proc/sys/net/ipv4/netfilter/ip_conntrack_count", "r");
			if (fp) {
				if (fgets(result, sizeof(result), fp) != NULL) {
					fclose(fp);
				}
			}
		} else if(strcmp(type,"conn.active") == 0) {
			char buf[256];
			FILE* fp;
			unsigned int established = 0;

			fp = fopen("/proc/net/nf_conntrack", "r");
			if (fp) {
				while (fgets(buf, sizeof(buf), fp) != NULL) {
				if (strstr(buf,"ESTABLISHED") || ((strstr(buf,"udp")) && (strstr(buf,"ASSURED"))))
					established++;
				}
				fclose(fp);
			}
			sprintf(result,"%u",established);

		} else if(strcmp(type,"conn.max") == 0) {
			FILE* fp;

			fp = fopen ("/proc/sys/net/ipv4/netfilter/ip_conntrack_max", "r");
			if (fp) {
				if (fgets(result, sizeof(result), fp) != NULL) {
					fclose(fp);
				}
			}
		} else if(strncmp(type,"conn.wifi",9) == 0) {
			int count, radio;
			char command[10];

			sscanf(type,"conn.wifi.%d.%9s", &radio, command);

			if (strcmp(command,"autho") == 0) {
				count = get_wifi_clients(radio,SI_WL_QUERY_AUTHO);
			} else if (strcmp(command,"authe") == 0) {
				count = get_wifi_clients(radio,SI_WL_QUERY_AUTHE);
			} else if (strcmp(command,"assoc") == 0) {
				count = get_wifi_clients(radio,SI_WL_QUERY_ASSOC);
			} else {
				count = 0;
			}
			if (count == -1)
				strcpy(result,"<i>off</i>");
			else
				sprintf(result,"%d",count);

		} else if(strcmp(type,"driver_version") == 0 ) {
			system("/usr/sbin/wl ver >/tmp/output.txt");

			char *buffer = read_whole_file("/tmp/output.txt");

			if (buffer) {
				if ((tmp = strstr(buffer, "\n")))
					strncpy(result, tmp+1, sizeof result);
				else
					strncpy(result, buffer, sizeof result);

				free(buffer);
				unlink("/tmp/output.txt");
			}
		} else if(strcmp(type,"cfe_version") == 0 ) {
			system("cat /dev/mtd0ro | grep bl_version >/tmp/output.txt");
			char *buffer = read_whole_file("/tmp/output.txt");

			if (buffer) {
				tmp = strstr(buffer, "bl_version=");

				if (tmp)
					sscanf(tmp, "bl_version=%s", result);
				else
					strcpy(result,"Unknown");
				free(buffer);

				unlink("/tmp/output.txt");
			}
		} else if(strncmp(type,"pid",3) ==0 ) {
			char service[32];
			sscanf(type, "pid.%31s", service);

			if (strlen(service))
				sprintf(result, "%d", pidof(service));

		} else if(strncmp(type,"vpnstatus",9) == 0 ) {
			int num = 0, pid;
			char service[10], buf[256];

			sscanf(type,"vpnstatus.%9[^.].%d", service, &num);

			if ((strlen(service)) && (num > 0) )
			{
				// Trigger OpenVPN to update the status file
				snprintf(buf, sizeof(buf), "vpn%s%d", service, num);
				if ((pid = pidof(buf)) > 0) {
					kill(pid, SIGUSR2);

					// Give it a chance to update the file
					sleep(1);

					// Read the status file and repeat it verbatim to the caller
					sprintf(buf,"/etc/openvpn/%s%d/status", service, num);
					char *buffer = read_whole_file(buf);
					if (buffer)
					{
						strncpy(result, buffer, sizeof(result));
						free(buffer);
					}
				}
			}
		} else if(strcmp(type,"ethernet") == 0 ) {
			int len, j;

			system("/usr/sbin/robocfg showports >/tmp/output.txt");

			char *buffer = read_whole_file("/tmp/output.txt");
			if (buffer) {
				len = strlen(buffer);

				for (j=0; (j < len); j++) {
					if (buffer[j] == '\n') buffer[j] = '>';
				}
				strncpy(result, buffer, sizeof result);

				free(buffer);
				unlink("/tmp/output.txt");
			}
		} else {
			strcpy(result,"Not implemented");
		}

	}

	retval += websWrite(wp, result);
	return retval;
}


unsigned int get_phy_temperature(int radio)
{
	int ret = 0;
	unsigned int *temp;
	char buf[WLC_IOCTL_SMLEN];
	char *interface;

	strcpy(buf, "phy_tempsense");

	if (radio == 2) {
		interface = "eth1";
	} else if (radio == 5) {
		interface = "eth2";
	} else {
		return 0;
	}

	if ((ret = wl_ioctl(interface, WLC_GET_VAR, buf, sizeof(buf)))) {
		return 0;
	} else {
		temp = (unsigned int *)buf;
		return *temp / 2 + 20;
	}
}


unsigned int get_wifi_clients(int radio, int querytype)
{
	char *name;
	struct maclist *clientlist;
	int max_sta_count, maclist_size;
	int val, count = 0;

	if (radio == 2) {
		name = "eth1";
	} else if (radio == 5) {
		name = "eth2";
	} else {
		return 0;
	}

	wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
	if (val == 1)
		return -1;	// Radio is disabled

	/* buffers and length */
	max_sta_count = 128;
	maclist_size = sizeof(clientlist->count) + max_sta_count * sizeof(struct ether_addr);
	clientlist = malloc(maclist_size);

	if (!clientlist)
		return 0;

	if (querytype == SI_WL_QUERY_AUTHE) {
		strcpy((char*)clientlist, "authe_sta_list");
		if (wl_ioctl(name, WLC_GET_VAR, clientlist, maclist_size))
			goto exit;

	} else if (querytype == SI_WL_QUERY_AUTHO) {
		strcpy((char*)clientlist, "autho_sta_list");
		if (wl_ioctl(name, WLC_GET_VAR, clientlist, maclist_size))
			goto exit;

	} else if (querytype == SI_WL_QUERY_ASSOC) {
		clientlist->count = max_sta_count;
		if (wl_ioctl(name, WLC_GET_ASSOCLIST, clientlist, maclist_size))
			goto exit;
	} else {
		goto exit;
	}

	count = clientlist->count;

exit:
	free(clientlist);
	return count;
}
