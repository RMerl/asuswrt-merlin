/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/netlink.h>

#include "../hotplug2.h"
#include "../mem_utils.h"
#include "../parser_utils.h"
#include "../filemap_utils.h"
#include "../hotplug2_utils.h"

inline char *get_uevent_string(char **environ, unsigned long *uevent_string_len) {
	char *uevent_string;
	char *tmp;
	unsigned long offset;
	
	*uevent_string_len = 4;
	offset = *uevent_string_len - 1;
	uevent_string = xmalloc(*uevent_string_len);
	strcpy(uevent_string, "add");
	uevent_string[offset] = '@';
	offset++;
	
	for (; *environ != NULL; environ++) {
		*uevent_string_len += strlen(*environ) + 1;
		uevent_string = xrealloc(uevent_string, *uevent_string_len);
		strcpy(&uevent_string[offset], *environ);
		offset = *uevent_string_len;
	}
	
	/* 64 + 7 ('SEQNUM=') + 1 ('\0') */
	tmp = xmalloc(72);
	memset(tmp, 0, 72);
	snprintf(tmp, 72, "SEQNUM=%llu", get_kernel_seqnum());
	
	*uevent_string_len += strlen(tmp) + 1;
	uevent_string = xrealloc(uevent_string, *uevent_string_len);
	strcpy(&uevent_string[offset], tmp);
	offset = *uevent_string_len;
	
	free(tmp);
	
	return uevent_string;
}

int dispatch_event(int netlink_socket, char **environ) {
	char *uevent_string;
	unsigned long uevent_string_len;
	
	uevent_string = get_uevent_string(environ, &uevent_string_len);
	if (uevent_string == NULL) {
		ERROR("dispatch_event", "Unable to create uevent string.");
		return 1;
	}
	
	if (send(netlink_socket, uevent_string, uevent_string_len, 0) == -1) {
		ERROR("dispatch_event","send failed: %s.", strerror(errno));
		free(uevent_string);
		return 1;
	}
	
	free(uevent_string);
	
	return 0;
}

char *join_to_fourhex(unsigned char hex0, unsigned char hex1) {
	char *tmp;
	
	tmp = xmalloc(5);
	if (hex0 != 0) {
		snprintf(tmp, 4, "%x%2x", hex0, hex1);
	} else {
		snprintf(tmp, 4, "%x", hex1);
	}
	
	return tmp;
}

void generate_pci_events(int netlink_socket) {
	unsigned char pci_desc_info[256];
	char pci_dir_name[256];
	char pci_dev_name[512];
	
	char **environ;
	int i;
	
	DIR *procdir, *pcidir;
	FILE *item;
	struct dirent *cur_bus, *cur_device;
	
	procdir = opendir("/proc/bus/pci");
	if (procdir == NULL) {
		ERROR("pci coldplug", "Unable to open procfs PCI interface.");
		return;
	}
	
	while ((cur_bus = readdir(procdir)) != NULL) {
		if (cur_bus->d_type != DT_DIR)
			continue;
		
		if (cur_bus->d_name[0] == '.')
			continue;
		
		memset(pci_dir_name, 0, 256);
		snprintf(pci_dir_name, 255, "/proc/bus/pci/%s", cur_bus->d_name);
		pcidir = opendir(pci_dir_name);
		
		while ((cur_device = readdir(pcidir)) != NULL) {
			if (cur_device->d_type != DT_REG)
				continue;
			
			if (cur_device->d_name[0] == '.')
				continue;
			
			memset(pci_dev_name, 0, 256);
			snprintf(pci_dev_name, 511, "/proc/bus/pci/%s/%s", cur_bus->d_name, cur_device->d_name);
			item = fopen(pci_dev_name, "rb");
			
			if (item == NULL) {
				ERROR("pci coldplug", "Missing PCI entry.");
				continue;
			}
			
			fread(pci_desc_info, 256, 1, item);
			fclose(item);
			
			
			environ = xmalloc(sizeof(char*) * 8);
			environ[0] = NULL;
			environ[1] = NULL;
			environ[2] = NULL;
			environ[3] = NULL;
			environ[4] = NULL;
			environ[5] = NULL;
			environ[6] = NULL;
			environ[7] = NULL;
			
			environ[0] = strdup("ACTION=add");
			environ[1] = strdup("SUBSYSTEM=pci");
			
			environ[2] = xmalloc(21 + strlen(cur_bus->d_name) + strlen(cur_device->d_name));
			strcpy(environ[2], "PCI_SLOT_NAME=0000:");
			strcat(environ[2], cur_bus->d_name);
			strcat(environ[2], ":");
			strcat(environ[2], cur_device->d_name);
			
			environ[3] = xmalloc(17);
			snprintf(environ[3], 17, "PCI_ID=%02X%02X:%02X%02X", pci_desc_info[1], pci_desc_info[0], pci_desc_info[3], pci_desc_info[2]);
			
			environ[4] = xmalloc(24);
			snprintf(environ[4], 24, "PCI_SUBSYS_ID=%02X%02X:%02X%02X", pci_desc_info[45], pci_desc_info[44], pci_desc_info[47], pci_desc_info[46]);
			
			environ[5] = xmalloc(17);
			snprintf(environ[5], 17, "PCI_CLASS=%x%02X%02X", pci_desc_info[11], pci_desc_info[10], pci_desc_info[9]);
			
			environ[6] = xmalloc(64);
			snprintf(environ[6], 64, "MODALIAS=pci:v0000%02X%02Xd0000%02X%02Xsv0000%02X%02Xsd0000%02X%02Xbc%02Xsc%02Xi%02X",
			pci_desc_info[1], pci_desc_info[0], pci_desc_info[3], pci_desc_info[2],
			pci_desc_info[45], pci_desc_info[44], pci_desc_info[47], pci_desc_info[46],
			pci_desc_info[11], pci_desc_info[10], pci_desc_info[9]);
			
			dispatch_event(netlink_socket, environ);
			
			for (i = 0; environ[i] != NULL; i++)
				free(environ[i]);
			free(environ);
		}
		
		closedir(pcidir);
	}
	
	closedir(procdir);
}

void generate_usb_events(int netlink_socket) {
	unsigned char usb_desc_info[52];
	char usb_dir_name[256];
	char usb_dev_name[512];
	
	char **environ;
	char *t1, *t2, *t3;
	int i;
	
	DIR *procdir, *usbdir;
	FILE *item;
	struct dirent *cur_bus, *cur_device;
	
	procdir = opendir("/proc/bus/usb");
	if (procdir == NULL) {
		ERROR("usb coldplug", "Unable to open procfs usb interface.");
		return;
	}
	
	while ((cur_bus = readdir(procdir)) != NULL) {
		if (cur_bus->d_type != DT_DIR)
			continue;
		
		if (cur_bus->d_name[0] == '.')
			continue;
		
		memset(usb_dir_name, 0, 256);
		snprintf(usb_dir_name, 255, "/proc/bus/usb/%s", cur_bus->d_name);
		usbdir = opendir(usb_dir_name);
		
		while ((cur_device = readdir(usbdir)) != NULL) {
			if (cur_device->d_type != DT_REG)
				continue;
			
			if (cur_device->d_name[0] == '.')
				continue;
			
			memset(usb_dev_name, 0, 256);
			snprintf(usb_dev_name, 511, "/proc/bus/usb/%s/%s", cur_bus->d_name, cur_device->d_name);
			item = fopen(usb_dev_name, "rb");
			
			if (item == NULL) {
				ERROR("usb coldplug", "Missing usb entry.");
				continue;
			}
			
			fread(usb_desc_info, 52, 1, item);
			fclose(item);
			
			environ = xmalloc(sizeof(char*) * 8);
			environ[7] = NULL;
			
			environ[0] = strdup("ACTION=add");
			environ[1] = strdup("SUBSYSTEM=usb");
			
			environ[2] = xmalloc(8 + strlen(usb_dev_name));
			strcpy(environ[2], "DEVICE=");
			strcat(environ[2], usb_dev_name);
			
			environ[3] = xmalloc(24);
			memset(environ[3], 0, 24);
			t1 = join_to_fourhex(usb_desc_info[9], usb_desc_info[8]);
			t2 = join_to_fourhex(usb_desc_info[11], usb_desc_info[10]);
			t3 = join_to_fourhex(usb_desc_info[13], usb_desc_info[12]);
			
			snprintf(environ[3], 23, "PRODUCT=%s/%s/%s", t1, t2, t3);
			
			free(t1); free(t2); free(t3);
			
			environ[4] = xmalloc(14);
			memset(environ[4], 0, 14);
			snprintf(environ[4], 13, "TYPE=%x/%x/%x", usb_desc_info[4], usb_desc_info[5], usb_desc_info[6]);
			
			environ[5] = xmalloc(55);
			memset(environ[5], 0, 55);
			
			if (usb_desc_info[32] != 0x0 && usb_desc_info[33] != 0x0 && usb_desc_info[34] != 0x0) {
				snprintf(environ[5], 55, "MODALIAS=usb:v%02X%02Xp%02X%02Xd%02X%02Xdc%02Xdsc%02Xdp%02Xic%02Xisc%02Xip%02X", 
				usb_desc_info[9], usb_desc_info[8], usb_desc_info[11], usb_desc_info[10], usb_desc_info[13], usb_desc_info[12],
				usb_desc_info[4], usb_desc_info[5], usb_desc_info[6],
				usb_desc_info[32], usb_desc_info[33], usb_desc_info[34]);
				
				environ[6] = xmalloc(24);
				memset(environ[6], 0, 24);
				snprintf(environ[6], 24, "INTERFACE=%x/%x/%x", usb_desc_info[32], usb_desc_info[33], usb_desc_info[34]);
			} else {
				snprintf(environ[5], 55, "MODALIAS=usb:v%02X%02Xp%02X%02Xd%02X%02Xdc%02Xdsc%02Xdp%02Xic*isc*ip*", 
				usb_desc_info[9], usb_desc_info[8], usb_desc_info[11], usb_desc_info[10], usb_desc_info[13], usb_desc_info[12],
				usb_desc_info[4], usb_desc_info[5], usb_desc_info[6]);
				
				environ[6] = NULL;
			}

			dispatch_event(netlink_socket, environ);
			
			for (i = 0; environ[i] != NULL; i++)
				free(environ[i]);
			free(environ);
		}
		
		closedir(usbdir);
	}
	
	closedir(procdir);
}

void generate_isapnp_events(int netlink_socket) {
	struct filemap_t isapnpmap;
	char **environ;
	char *line, *nline, *nptr;
	char *token;
	char *device;
	int i;
	
	if (map_file("/proc/bus/isapnp/devices", &isapnpmap)) {
		ERROR("isapnp coldplug", "Unable to open procfs isapnp interface.");
		return;
	}
	
	nptr = isapnpmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		token = dup_token(nline, &nline, isspace);
		if (!token)
			continue;
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token) 
			continue;
		
		if (strlen(token) < 14) {
			free(token);
			continue;
		}
		
		environ = xmalloc(sizeof(char*) * 4);
		environ[3] = NULL;
		
		environ[0] = strdup("ACTION=add");
		environ[1] = strdup("SUBSYSTEM=pnp");
		
		environ[2] = xmalloc(14);
		strcpy(environ[2], "MODALIAS=pnp:");
		for (device = &token[7]; *device != '\0'; device += 7) {
			environ[2] = xrealloc(environ[2], 14 + device - token);
			memcpy(environ[2] + (device - token) + 6, device, 7);
			environ[2][13 + device - token] = '\0';
		}
		
		dispatch_event(netlink_socket, environ);
		
		for (i = 0; environ[i] != NULL; i++)
			free(environ[i]);
		free(environ);
		
		free(token);
		free(line);
	}
	
	unmap_file(&isapnpmap);
}

int main(int argc, char *argv[], char **environ) {
	int netlink_socket;
	
	netlink_socket = init_netlink_socket(NETLINK_CONNECT);
	if (netlink_socket == -1) {
		ERROR("netlink init","Unable to open netlink socket.");
		return 1;
	}
	
	generate_pci_events(netlink_socket);
	generate_usb_events(netlink_socket);
	generate_isapnp_events(netlink_socket);

	close(netlink_socket);
	return 0;
}
