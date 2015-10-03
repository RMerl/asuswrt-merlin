#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <shared.h>
#include "usb_info.h"
#include "disk_initial.h"
#include <shutils.h>

//#ifdef RTN56U
//#include <nvram/bcmnvram.h>
//#else
#include <bcmnvram.h>
//#endif

int find_str_host_info(const char *str, int *host, int *channel, int *id, int *lun)
{
	int order = 0;
	char word[PATH_MAX], *next, *ptr;

	ptr = (char *)str;

	foreach_58(word, ptr, next){
		switch(order){
			case 0:
				if(host != NULL)
					*host = atoi(word);
				break;
			case 1:
				if(channel != NULL)
					*channel = atoi(word);
				break;
			case 2:
				if(id != NULL)
					*id = atoi(word);
				break;
			case 3:
				if(lun != NULL)
					*lun = atoi(word);
				break;
		}

		++order;
	}

	if(order != 4){
		usb_dbg("(%s): Didn't get the scsi info enough.\n", str);
		return -1;
	}

	return 0;
}

int find_disk_host_info(const char *dev, int *host, int *channel, int *id, int *lun)
{
	DIR *disk_dir;
	struct dirent *dp;
	char *ptr, buf[128];

	memset(buf, 0, 128);
	sprintf(buf, "/sys/block/%s/device", dev);

	if((disk_dir = opendir(buf)) == NULL){
		usb_dbg("(%s): The path isn't existed: %s.\n", dev, buf);
		return -1;
	}

	while((dp = readdir(disk_dir))){
		if(!strncmp(dp->d_name, "scsi_disk:", 10)){
			ptr = dp->d_name+10;
			find_str_host_info(ptr, host, channel, id, lun);

			break;
		}
	}
	closedir(disk_dir);

	return 0;
}

int get_device_type_by_device(const char *device_name)
{
	if(device_name == NULL || strlen(device_name) <= 0){
		usb_dbg("(%s): The device name is not correct.\n", device_name);
		return DEVICE_TYPE_UNKNOWN;
	}

	if(isStorageDevice(device_name)
#ifdef BCM_MMC
			|| isMMCDevice(device_name) // SD card.
#endif
			){
		return DEVICE_TYPE_DISK;
	}
#ifdef RTCONFIG_USB_PRINTER
	if(!strncmp(device_name, "lp", 2)){
		return DEVICE_TYPE_PRINTER;
	}
#endif
#ifdef RTCONFIG_USB_MODEM
	if(!strncmp(device_name, "sg", 2)){
		return DEVICE_TYPE_SG;
	}
	if(!strncmp(device_name, "sr", 2)){
		return DEVICE_TYPE_CD;
	}
	if(isTTYNode(device_name)){
		return DEVICE_TYPE_MODEM;
	}
#endif
#ifdef RTCONFIG_USB_BECEEM
	if(isBeceemNode(device_name)){
		return DEVICE_TYPE_BECEEM;
	}
#endif

	return DEVICE_TYPE_UNKNOWN;
}

char *get_device_type_by_node(const char *usb_node, char *buf, const int buf_size)
{
	int interface_num, interface_count;
	char interface_name[16];
#ifdef RTCONFIG_USB_PRINTER
	int got_printer = 0;
#endif
#ifdef RTCONFIG_USB_MODEM
	int got_modem = 0;
#endif
	int got_disk = 0;
	int got_others = 0;

	interface_num = get_usb_interface_number(usb_node);
	if(interface_num <= 0)
		return NULL;

	for(interface_count = 0; interface_count < interface_num; ++interface_count){
		memset(interface_name, 0, sizeof(interface_name));
		sprintf(interface_name, "%s:1.%d", usb_node, interface_count);

#ifdef RTCONFIG_USB_PRINTER
		if(isPrinterInterface(interface_name))
			++got_printer;
		else
#endif
#ifdef RTCONFIG_USB_MODEM
		if(isSerialInterface(interface_name, 0, 0, 0) || isACMInterface(interface_name, 0, 0, 0))
			++got_modem;
		else
#endif
		if(isStorageInterface(interface_name))
			++got_disk;
		else
			++got_others;
	}

	if(
#ifdef RTCONFIG_USB_PRINTER
			!got_printer
#else
			1
#endif
			&&
#ifdef RTCONFIG_USB_MODEM
			!got_modem
#else
			1
#endif
			&& !got_disk
			)
		return NULL;

	memset(buf, 0, buf_size);
#ifdef RTCONFIG_USB_PRINTER
	if(got_printer > 0) // Top priority
		strcpy(buf, "printer");
	else
#endif
#ifdef RTCONFIG_USB_MODEM
	if(got_modem > 0) // 2nd priority
		strcpy(buf, "modem");
	else
#endif
	if(got_disk > 0)
		strcpy(buf, "storage");
	else
		return NULL;

	return buf;
}

char *get_usb_node_by_string(const char *target_string, char *ret, const int ret_size)
{
	char usb_port[32], buf[16];
	char *ptr, *ptr2, *ptr3;
	int len;

	memset(usb_port, 0, sizeof(usb_port));
	if(get_usb_port_by_string(target_string, usb_port, sizeof(usb_port)) == NULL)
		return NULL;
	if((ptr = strstr(target_string, usb_port)) == NULL)
		return NULL;
	if(ptr != target_string)
		ptr += strlen(usb_port)+1;

	if((ptr2 = strchr(ptr, ':')) == NULL)
		return NULL;
	ptr3 = ptr2;
	*ptr3 = 0;

	if((ptr2 = strrchr(ptr, '/')) == NULL)
		ptr2 = ptr;
	else
		ptr = ptr2+1;

	len = strlen(ptr);
	if(len > 16)
		len = 16;

	memset(buf, 0, sizeof(buf));
	strncpy(buf, ptr, len);

	len = strlen(buf);
	if(len > ret_size)
		len = ret_size;

	memset(ret, 0, ret_size);
	strncpy(ret, buf, len);

	*ptr3 = ':';

	return ret;
}

char *get_usb_node_by_device(const char *device_name, char *buf, const int buf_size)
{
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[16];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

	if(device_type == DEVICE_TYPE_DISK){
		get_disk_name(device_name, disk_name, 16);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#ifdef RTCONFIG_USB_PRINTER
	if(device_type == DEVICE_TYPE_PRINTER){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_MODEM
	if(device_type == DEVICE_TYPE_SG){
		sprintf(device_path, "%s/%s/device", SYS_SG, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_CD){
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_MODEM){
		sprintf(device_path, "%s/%s/device", SYS_TTY, device_name);
		if(realpath(device_path, usb_path) == NULL){
			sleep(1); // Sometimes link would be built slowly, so try again.

			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_BECEEM
	if(device_type == DEVICE_TYPE_BECEEM){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
		return NULL;

#ifdef BCM_MMC
	if(isMMCDevice(device_name)){ // SD card.
		snprintf(buf, buf_size, "%s", SDCARD_PORT);
	}
	else
#endif
	if(get_usb_node_by_string(usb_path, buf, buf_size) == NULL){
		usb_dbg("(%s): Fail to get usb node: %s.\n", device_name, usb_path);
		return NULL;
	}

	return buf;
}

char *get_usb_port_by_string(const char *target_string, char *buf, const int buf_size)
{
	memset(buf, 0, buf_size);

	if(strstr(target_string, USB_XHCI_PORT_1))
		strcpy(buf, USB_XHCI_PORT_1);
	else if(strstr(target_string, USB_XHCI_PORT_2))
		strcpy(buf, USB_XHCI_PORT_2);
	else if(strstr(target_string, USB_EHCI_PORT_1))
		strcpy(buf, USB_EHCI_PORT_1);
	else if(strstr(target_string, USB_EHCI_PORT_2))
		strcpy(buf, USB_EHCI_PORT_2);
	else if(strstr(target_string, USB_OHCI_PORT_1))
		strcpy(buf, USB_OHCI_PORT_1);
	else if(strstr(target_string, USB_OHCI_PORT_2))
		strcpy(buf, USB_OHCI_PORT_2);
#ifdef BCM_MMC
	else if(strstr(target_string, SDCARD_PORT))
		strcpy(buf, SDCARD_PORT);
#endif
	else if(strstr(target_string, USB_EHCI_PORT_3))
		strcpy(buf, USB_EHCI_PORT_3);
	else if(strstr(target_string, USB_OHCI_PORT_3))
		strcpy(buf, USB_OHCI_PORT_3);
	else
		return NULL;

	return buf;
}

char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size)
{
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[16];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

	if(device_type == DEVICE_TYPE_DISK){
		get_disk_name(device_name, disk_name, 16);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#ifdef RTCONFIG_USB_PRINTER
	if(device_type == DEVICE_TYPE_PRINTER){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_MODEM
	if(device_type == DEVICE_TYPE_SG){
		sprintf(device_path, "%s/%s/device", SYS_SG, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_CD){
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_MODEM){
		sprintf(device_path, "%s/%s/device", SYS_TTY, device_name);
		if(realpath(device_path, usb_path) == NULL){
			sleep(1); // Sometimes link would be built slowly, so try again.

			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_BECEEM
	if(device_type == DEVICE_TYPE_BECEEM){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
		return NULL;

#ifdef BCM_MMC
	if(isMMCDevice(device_name)){ // SD card.
		snprintf(buf, buf_size, "%s", SDCARD_PORT);
	}
	else
#endif
	if(get_usb_port_by_string(usb_path, buf, buf_size) == NULL){
		usb_dbg("(%s): Fail to get usb port: %s.\n", device_name, usb_path);
		return NULL;
	}

	return buf;
}

char *get_interface_by_string(const char *target_string, char *ret, const int ret_size)
{
	char buf[32], *ptr, *ptr_end, *ptr2, *ptr2_end;
	int len;

	if((ptr = strstr(target_string, USB_XHCI_PORT_1)) != NULL)
		ptr += strlen(USB_XHCI_PORT_1);
	else if((ptr = strstr(target_string, USB_XHCI_PORT_2)) != NULL)
		ptr += strlen(USB_XHCI_PORT_2);
	else if((ptr = strstr(target_string, USB_EHCI_PORT_1)) != NULL)
		ptr += strlen(USB_EHCI_PORT_1);
	else if((ptr = strstr(target_string, USB_EHCI_PORT_2)) != NULL)
		ptr += strlen(USB_EHCI_PORT_2);
	else if((ptr = strstr(target_string, USB_OHCI_PORT_1)) != NULL)
		ptr += strlen(USB_OHCI_PORT_1);
	else if((ptr = strstr(target_string, USB_OHCI_PORT_2)) != NULL)
		ptr += strlen(USB_OHCI_PORT_2);
	else if((ptr = strstr(target_string, USB_EHCI_PORT_3)) != NULL)
		ptr += strlen(USB_EHCI_PORT_3);
	else if((ptr = strstr(target_string, USB_OHCI_PORT_3)) != NULL)
		ptr += strlen(USB_OHCI_PORT_3);
	else
		return NULL;
	++ptr;

	if((ptr_end = strchr(ptr, ':')) == NULL)
		ptr_end = ptr+strlen(ptr);

	if((ptr2_end = strchr(ptr_end, '/')) == NULL)
		ptr2_end = ptr_end+strlen(ptr_end);

	len = strlen(ptr)-strlen(ptr2_end);
	if(len > 32)
		len = 32;
	memset(buf, 0, 32);
	strncpy(buf, ptr, len);

	if((ptr2 = strrchr(buf, '/')) == NULL)
		ptr2 = buf;
	else
		++ptr2;

	if((len = ptr_end-ptr) < 0)
		len = ptr-ptr_end;

	memset(ret, 0, ret_size);
	strcpy(ret, ptr2); // ex: 1-1:1.0/~

	return ret;
}

char *get_interface_by_device(const char *device_name, char *buf, const int buf_size)
{
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[16];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

	if(device_type == DEVICE_TYPE_DISK){
		get_disk_name(device_name, disk_name, 16);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#ifdef RTCONFIG_USB_PRINTER
	if(device_type == DEVICE_TYPE_PRINTER){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_MODEM
	if(device_type == DEVICE_TYPE_SG){
		sprintf(device_path, "%s/%s/device", SYS_SG, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_CD){
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, device_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
	if(device_type == DEVICE_TYPE_MODEM){
		sprintf(device_path, "%s/%s/device", SYS_TTY, device_name);
		if(realpath(device_path, usb_path) == NULL){
			sleep(1); // Sometimes link would be built slowly, so try again.

			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
#ifdef RTCONFIG_USB_BECEEM
	if(device_type == DEVICE_TYPE_BECEEM){
		sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
		if(realpath(device_path, usb_path) == NULL){
			if(realpath(device_path, usb_path) == NULL){
				usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
				return NULL;
			}
		}
	}
	else
#endif
		return NULL;

	if(get_interface_by_string(usb_path, buf, buf_size) == NULL){
		usb_dbg("(%s): Fail to get usb port: %s.\n", device_name, usb_path);
		return NULL;
	}

	return buf;
}

char *get_path_by_node(const char *usb_node, char *buf, const int buf_size){
	char usb_port[32], *hub_path;
	int port_num = 0, len;

	if(usb_node == NULL || buf == NULL || buf_size <= 0)
		return NULL;

	// Get USB port.
	if(get_usb_port_by_string(usb_node, usb_port, sizeof(usb_port)) == NULL)
		return NULL;

	port_num = get_usb_port_number(usb_port);
	if(port_num == 0)
		return NULL;

	if(strlen(usb_node) > (len = strlen(usb_port))){
		hub_path = (char *)usb_node+len;
		snprintf(buf, buf_size, "%d%s", port_num, hub_path);
	}
	else
		snprintf(buf, buf_size, "%d", port_num);

	return buf;
}

static FILE *open_usb_target(const char *usb_node, const char *target, const int wait)
{
	FILE *fp;
	char usb_port[32], target_file[128];
	int retry = wait;

	if(usb_node == NULL || target == NULL ||
	   get_usb_port_by_string(usb_node, usb_port, sizeof(usb_port)) == NULL)
		return NULL;

	snprintf(target_file, sizeof(target_file), "%s/%s/%s", USB_DEVICE_PATH, usb_node, target);

	while((fp = fopen(target_file, "r")) == NULL && retry-- > 0)
		sleep(1);

	return fp;
}

unsigned int get_usb_vid(const char *usb_node)
{
	FILE *fp;
	unsigned int val;

	if((fp = open_usb_target(usb_node, "idVendor", 0)) == NULL)
		return 0;

	if(fscanf(fp, "%x", &val) < 1)
		val = 0;
	fclose(fp);

	return val;
}

unsigned int get_usb_pid(const char *usb_node)
{
	FILE *fp;
	unsigned int val;

	if((fp = open_usb_target(usb_node, "idProduct", 0)) == NULL)
		return 0;

	if(fscanf(fp, "%x", &val) < 1)
		val = 0;
	fclose(fp);

	return val;
}

char *get_usb_manufacturer(const char *usb_node, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	if((fp = open_usb_target(usb_node, "manufacturer", 0)) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

char *get_usb_product(const char *usb_node, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	if((fp = open_usb_target(usb_node, "product", 0)) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

char *get_usb_serial(const char *usb_node, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	if((fp = open_usb_target(usb_node, "serial", 0)) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

char *get_usb_speed(const char *usb_node, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	if((fp = open_usb_target(usb_node, "speed", 0)) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

int get_usb_interface_number(const char *usb_node)
{
	FILE *fp;
	int val;

	if((fp = open_usb_target(usb_node, "bNumInterfaces", 0)) == NULL)
		return 0;

	if(fscanf(fp, "%d", &val) < 1)
		val = 0;
	fclose(fp);

	return val;
}

char *get_usb_interface_class(const char *interface_name, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	// Sometimes the class file would be built slowly, so try again during MAX_WAIT_FILE
	if((fp = open_usb_target(interface_name, "bInterfaceClass", MAX_WAIT_FILE)) == NULL){
		usb_dbg("(%s): Fail to open the class file really!\n", interface_name);
		return NULL;
	}

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

char *get_usb_interface_subclass(const char *interface_name, char *buf, const int buf_size)
{
	FILE *fp;
	char *ptr;

	// Sometimes the class file would be built slowly, so try again during MAX_WAIT_FILE
	if((fp = open_usb_target(interface_name, "bInterfaceSubClass", MAX_WAIT_FILE)) == NULL){
		usb_dbg("(%s): Fail to open the class file really!\n", interface_name);
		return NULL;
	}

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	return strsep(&ptr, "\r\n");
}

int get_interface_numendpoints(const char *interface_name)
{
	FILE *fp;
	int val;

	if((fp = open_usb_target(interface_name, "bNumEndpoints", 0)) == NULL)
		return 0;

	if(fscanf(fp, "%d", &val) < 1)
		val = 0;
	fclose(fp);

	return val;
}

int get_interface_Int_endpoint(const char *interface_name)
{
	FILE *fp;
	char interface_path[128], bmAttributes_file[128], buf[8], *ptr;
	DIR *interface_dir = NULL;
	struct dirent *end_name;
	int bNumEndpoints, end_count, got_Int = 0;

	if(interface_name == NULL || get_usb_port_by_string(interface_name, buf, sizeof(buf)) == NULL){
		usb_dbg("(%s): The device is not a interface.\n", interface_name);
		return 0;
	}

	memset(interface_path, 0, 128);
	sprintf(interface_path, "%s/%s", USB_DEVICE_PATH, interface_name);
	if((interface_dir = opendir(interface_path)) == NULL){
		usb_dbg("(%s): Fail to open dir: %s.\n", interface_name, interface_path);
		return 0;
	}

	// Get bNumEndpoints.
	bNumEndpoints = get_interface_numendpoints(interface_name);
	if(bNumEndpoints <= 0){
		usb_dbg("(%s): No endpoints: %d.\n", interface_name, bNumEndpoints);
		goto leave;
	}
	else if(bNumEndpoints == 1){ // ex: GL04P
		usb_dbg("(%s): It's a little impossible to be the control interface with a endpoint.\n", interface_name);
		goto leave;
	}

	end_count = 0;
	while((end_name = readdir(interface_dir)) != NULL){
		if(strncmp(end_name->d_name, "ep_", 3))
			continue;

		++end_count;

		memset(bmAttributes_file, 0, 128);
		sprintf(bmAttributes_file, "%s/%s/bmAttributes", interface_path, end_name->d_name);

		if((fp = fopen(bmAttributes_file, "r")) == NULL){
			usb_dbg("(%s): Fail to open file: %s.\n", interface_name, bmAttributes_file);
			continue;
		}

		memset(buf, 0, sizeof(buf));
		ptr = fgets(buf, sizeof(buf), fp);
		fclose(fp);
		if(ptr == NULL)
			goto leave;

		if(!strncmp(buf, "03", 2)){
			got_Int = 1;
			break;
		}
		else if(end_count == bNumEndpoints)
			break;
	}

leave:
	closedir(interface_dir);

	return got_Int;
}

#ifdef RTCONFIG_USB_MODEM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
int hadWWANModule(void)
{
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/usb_wwan", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}
#endif

int hadOptionModule(void)
{
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/option", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}

int hadSerialModule(void)
{
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/usbserial", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}

int hadACMModule(void)
{
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/cdc_acm", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}

// return 1 when there is a bound device.
int hadRNDISModule(){
	DIR *module_dir;
	struct dirent *file;
	char buf[32];

	if((module_dir = opendir(SYS_RNDIS_PATH)) == NULL)
		return 0;

	while((file = readdir(module_dir)) != NULL){
		if(!strcmp(file->d_name, ".") || !strcmp(file->d_name, ".."))
			continue;

		if(get_interface_by_string(file->d_name, buf, 32)){
			closedir(module_dir);
			return 1;
		}
	}
	closedir(module_dir);

	return 0;
}

#ifdef RTCONFIG_USB_BECEEM
int hadBeceemModule(void){
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/drxvi314", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}

int hadGCTModule(void){
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/tun", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}
#endif

int isTTYNode(const char *device_name)
{
	if(strncmp(device_name, "tty", 3))
		return 0;

	return 1;
}

int isSerialNode(const char *device_name)
{
	if(strstr(device_name, "ttyUSB") == NULL)
		return 0;

	return 1;
}

int isACMNode(const char *device_name)
{
	if(strstr(device_name, "ttyACM") == NULL)
		return 0;

	return 1;
}

#ifdef RTCONFIG_USB_BECEEM
int isBeceemNode(const char *device_name)
{
	if(strstr(device_name, "usbbcm") == NULL)
		return 0;

	return 1;
}
#endif

int isSerialInterface(const char *interface_name, const int specifics, const unsigned int vid, const unsigned int pid)
{
	char interface_class[4];

	if(specifics){
		// HTC M8
		if(vid == 0x0bb4 && (pid == 0x0f64 || pid == 0x0f60))
			return 0;
	}

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "ff"))
		return 0;

	return 1;
}

int isACMInterface(const char *interface_name, const int specifics, const unsigned int vid, const unsigned int pid)
{
	char interface_class[4], interface_subclass[4];

	if(specifics){
		// HTC M8
		if(vid == 0x0bb4 && (pid == 0x0f64 || pid == 0x0f60))
			return 0;
	}

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(get_usb_interface_subclass(interface_name, interface_subclass, 4) == NULL)
		return 0;

	// mbim interface is class: 02, subclass: 0e.
	if(strcmp(interface_class, "02") || !strcmp(interface_subclass, "0e"))
		return 0;

	return 1;
}

int isRNDISInterface(const char *interface_name, const unsigned int vid, const unsigned int pid)
{
	char interface_class[4];
	char target_file[128];
	DIR *module_dir;

	if(vid == 0x1076 && (pid == 0x8002 || pid == 0x8003))
		return 1;

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "02") && strcmp(interface_class, "e0"))
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s", SYS_RNDIS_PATH, interface_name);
	if((module_dir = opendir(target_file)) == NULL)
		return 0;

	closedir(module_dir);

	return 1;
}

int isCDCETHInterface(const char *interface_name)
{
	char interface_class[4], interface_subclass[4];
	char target_file[128];
	DIR *module_dir;

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "02"))
		return 0;

	if(get_usb_interface_subclass(interface_name, interface_subclass, 4) == NULL)
		return 0;

	if(strcmp(interface_subclass, "06"))
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s", SYS_CDCETH_PATH, interface_name);
	if((module_dir = opendir(target_file)) == NULL)
		return 0;

	closedir(module_dir);

	return 1;
}

int isNCMInterface(const char *interface_name)
{
	char interface_class[4], interface_subclass[4];
	char target_file[128];
	DIR *module_dir;

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "02"))
		return 0;

	if(get_usb_interface_subclass(interface_name, interface_subclass, 4) == NULL)
		return 0;

	if(strcmp(interface_subclass, "0d"))
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s", SYS_NCM_PATH, interface_name);
	if((module_dir = opendir(target_file)) == NULL)
		return 0;

	closedir(module_dir);

	return 1;
}

#ifdef RTCONFIG_USB_BECEEM
int isGCTInterface(const char *interface_name){
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "0a"))
		return 0;

	return 1;
}
#endif

// 0: no modem, 1: has modem, 2: has modem but system isn't ready.
int is_usb_modem_ready(void)
{
	char prefix[32], tmp[32];
	char usb_act[8];
	char usb_node[32], port_path[8];
	char modem_type[32];

	if(nvram_match("modem_enable", "0"))
		return 0;

	snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(usb_node) <= 0)
		return 0;

	if(get_path_by_node(usb_node, port_path, 8) == NULL)
		return 0;

	snprintf(prefix, 32, "usb_path%s", port_path);
	snprintf(usb_act, 8, "%s", nvram_safe_get(strcat_r(prefix, "_act", tmp)));

	if(!strcmp(nvram_safe_get("usb_modem_act_type"), ""))
		eval("find_modem_type.sh");
	snprintf(modem_type, 32, "%s", nvram_safe_get("usb_modem_act_type"));

	if(nvram_match(prefix, "modem") && strlen(usb_act) != 0){
#if 0
		// for the router dongle: Huawei E353, E3131.
		if((!strncmp(usb_act, "eth", 3) && strcmp(modem_type, "rndis")) // LU-150: ethX with RNDIS
				|| (!strncmp(usb_act, "usb", 3) && !strcmp(modem_type, "ncm"))
				){
			if(!strncmp(nvram_safe_get("lan_ipaddr"), "192.168.1.", 10))
				return 2;
		}
#endif

		return 1;
	}

	return 0;
}
#endif // RTCONFIG_USB_MODEM

#ifdef RTCONFIG_USB_PRINTER
int hadPrinterModule(void)
{
	char target_file[128];
	DIR *module_dir;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/usblp", SYS_MODULE);
	if((module_dir = opendir(target_file)) != NULL){
		closedir(module_dir);
		return 1;
	}
	else
		return 0;
}

int hadPrinterInterface(const char *usb_node)
{
	char check_usb_node[32], device_name[4];
	int printer_order, got_printer = 0;

	for(printer_order = 0; printer_order < MAX_USB_PRINTER_NUM; ++printer_order){
		memset(device_name, 0, 4);
		sprintf(device_name, "lp%d", printer_order);

		if(get_usb_node_by_device(device_name, check_usb_node, 32) == NULL)
			continue;

		if(!strcmp(usb_node, check_usb_node)){
			got_printer = 1;

			break;
		}
	}

	return got_printer;
}

int isPrinterInterface(const char *interface_name)
{
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "07"))
		return 0;

	return 1;
}
#endif // RTCONFIG_USB_PRINTER

int isStorageInterface(const char *interface_name)
{
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "08"))
		return 0;

	return 1;
}

int isStorageDevice(const char *device_name){
	if(!strncmp(device_name, "sd", 2))
		return 1;

	return 0;
}

#ifdef BCM_MMC
int isMMCDevice(const char *device_name){
	if(!strncmp(device_name, "mmcblk", 6))
		return 1;

	return 0;
}
#endif

char *find_sg_of_device(const char *device_name, char *buf, const int buf_size)
{
	DIR *dp;
	struct dirent *file;
	char target_usb_port[32], check_usb_port[32];
	int got_sg = 0;

	if(get_usb_port_by_device(device_name, target_usb_port, sizeof(target_usb_port)) == NULL)
		return NULL;

	if((dp = opendir(SYS_SG)) == NULL)
		return NULL;

	while((file = readdir(dp)) != NULL){
		if(!strcmp(file->d_name, ".") || !strcmp(file->d_name, ".."))
			continue;

		if(strncmp(file->d_name, "sg", 2))
			continue;

		if(get_usb_port_by_device(file->d_name, check_usb_port, sizeof(check_usb_port)) == NULL)
			return NULL;

		if(!strcmp(target_usb_port, check_usb_port)){
			memset(buf, 0, buf_size);
			strncpy(buf, file->d_name, buf_size);
			got_sg = 1;
			break;
		}
	}
	closedir(dp);

	if(!got_sg)
		return NULL;

	return buf;
}
