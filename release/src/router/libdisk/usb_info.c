#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <shared.h>
#include "usb_info.h"

#ifdef RTN56U
#include <nvram/bcmnvram.h>
#include <shutils.h>
#else
#include <bcmnvram.h>
#endif

extern int get_device_type_by_device(const char *device_name){
	if(device_name == NULL || strlen(device_name) <= 0){
		usb_dbg("(%s): The device name is not correct.\n", device_name);
		return DEVICE_TYPE_UNKNOWN;
	}

#ifdef RTCONFIG_USB
	if(!strncmp(device_name, "sd", 2)
#ifdef SUPPORT_IDE_SATA_DISK
			|| !strncmp(device_name, "hd", 2)
#endif
			){
		return DEVICE_TYPE_DISK;
	}
#endif
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
	if(isSerialNode(device_name) || isACMNode(device_name)){
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

extern char *get_device_type_by_node(const char *usb_node, char *buf, const int buf_size){
	int interface_num, interface_count;
	char interface_name[16];
#ifdef RTCONFIG_USB_PRINTER
	int got_printer = 0;
#endif
#ifdef RTCONFIG_USB_MODEM
	int got_modem = 0;
#endif
#ifdef RTCONFIG_USB
	int got_disk = 0;
#endif
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
		if(isSerialInterface(interface_name) || isACMInterface(interface_name))
			++got_modem;
		else
#endif
#ifdef RTCONFIG_USB
		if(isStorageInterface(interface_name))
			++got_disk;
		else
#endif
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
			&&
#ifdef RTCONFIG_USB
			!got_disk
#else
			1
#endif
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
#ifdef RTCONFIG_USB
	if(got_disk > 0)
		strcpy(buf, "storage");
	else
#endif
		return NULL;

	return buf;
}

extern char *get_usb_node_by_string(const char *target_string, char *ret, const int ret_size){
	char usb_port[8], buf[16];
	char *ptr, *ptr_end;
	int len;

	memset(usb_port, 0, 8);
	if(get_usb_port_by_string(target_string, usb_port, 8) == NULL)
		return NULL;

	if((ptr = strstr(target_string, usb_port)) == NULL)
		return NULL;
	if(ptr != target_string)
		ptr += strlen(usb_port)+1;

	if((ptr_end = strchr(ptr, ':')) == NULL)
		return NULL;

	len = strlen(ptr)-strlen(ptr_end);
	if(len > 16)
		len = 16;

	memset(buf, 0, 16);
	strncpy(buf, ptr, len);

	if((ptr = strrchr(buf, '/')) == NULL)
		ptr = buf;
	else
		++ptr;

	len = strlen(ptr);
	if(len > ret_size)
		len = ret_size;

	memset(ret, 0, ret_size);
	strncpy(ret, ptr, len);

	return ret;
}

extern char *get_usb_node_by_device(const char *device_name, char *buf, const int buf_size){
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[4];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

#ifdef RTCONFIG_USB
	if(device_type == DEVICE_TYPE_DISK){
		memset(disk_name, 0, 4);
		strncpy(disk_name, device_name, 3);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
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

	if(get_usb_node_by_string(usb_path, buf, buf_size) == NULL){
		usb_dbg("(%s): Fail to get usb port: %s.\n", device_name, usb_path);
		return NULL;
	}

	return buf;
}

extern char *get_usb_port_by_string(const char *target_string, char *buf, const int buf_size){
	memset(buf, 0, buf_size);

	if(strstr(target_string, USB_EHCI_PORT_1))
		strcpy(buf, USB_EHCI_PORT_1);
	else if(strstr(target_string, USB_EHCI_PORT_2))
		strcpy(buf, USB_EHCI_PORT_2);
	else if(strstr(target_string, USB_OHCI_PORT_1))
		strcpy(buf, USB_OHCI_PORT_1);
	else if(strstr(target_string, USB_OHCI_PORT_2))
		strcpy(buf, USB_OHCI_PORT_2);
	else if(strstr(target_string, USB_EHCI_PORT_3))
		strcpy(buf, USB_EHCI_PORT_3);
	else if(strstr(target_string, USB_OHCI_PORT_3))
		strcpy(buf, USB_OHCI_PORT_3);
	else
		return NULL;

	return buf;
}

extern char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size){
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[4];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

#ifdef RTCONFIG_USB
	if(device_type == DEVICE_TYPE_DISK){
		memset(disk_name, 0, 4);
		strncpy(disk_name, device_name, 3);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
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

	if(get_usb_port_by_string(usb_path, buf, buf_size) == NULL){
		usb_dbg("(%s): Fail to get usb port: %s.\n", device_name, usb_path);
		return NULL;
	}

	return buf;
}

extern char *get_interface_by_string(const char *target_string, char *ret, const int ret_size){
	char buf[32], *ptr, *ptr_end, *ptr2, *ptr2_end;
	int len;

	if((ptr = strstr(target_string, USB_EHCI_PORT_1)) != NULL)
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

extern char *get_interface_by_device(const char *device_name, char *buf, const int buf_size){
	int device_type = get_device_type_by_device(device_name);
	char device_path[128], usb_path[PATH_MAX];
	char disk_name[4];

	if(device_type == DEVICE_TYPE_UNKNOWN)
		return NULL;

	memset(device_path, 0, 128);
	memset(usb_path, 0, PATH_MAX);

#ifdef RTCONFIG_USB
	if(device_type == DEVICE_TYPE_DISK){
		memset(disk_name, 0, 4);
		strncpy(disk_name, device_name, 3);
		sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
		if(realpath(device_path, usb_path) == NULL){
			usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
			return NULL;
		}
	}
	else
#endif
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

extern char *get_usb_vid(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/idVendor", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_usb_pid(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/idProduct", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_usb_manufacturer(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/manufacturer", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_usb_product(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/product", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_usb_serial(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/serial", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern char *get_usb_speed(const char *usb_node, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int len;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/speed", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return NULL;

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern int get_usb_interface_number(const char *usb_node){
	FILE *fp;
	char target_file[128], buf[8], *ptr;

	if(usb_node == NULL || get_usb_port_by_string(usb_node, buf, sizeof(buf)) == NULL)
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/bNumInterfaces", USB_DEVICE_PATH, usb_node);
	if((fp = fopen(target_file, "r")) == NULL)
		return 0;

	memset(buf, 0, sizeof(buf));
	ptr = fgets(buf, sizeof(buf), fp);
	fclose(fp);
	if(ptr == NULL)
		return 0;

	return atoi(buf);
}

extern char *get_usb_interface_class(const char *interface_name, char *buf, const int buf_size){
	FILE *fp;
	char check_usb_port[8], target_file[128], *ptr;
	int retry, len;

	if(interface_name == NULL || get_usb_port_by_string(interface_name, check_usb_port, sizeof(check_usb_port)) == NULL)
		return NULL;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/bInterfaceClass", USB_DEVICE_PATH, interface_name);
	retry = 0;
	while((fp = fopen(target_file, "r")) == NULL && retry < MAX_WAIT_FILE){
		++retry;
		sleep(1); // Sometimes the class file would be built slowly, so try again.
	}

	if(fp == NULL){
		usb_dbg("(%s): Fail to open the class file really!\n", interface_name);
		return NULL;
	}

	memset(buf, 0, buf_size);
	ptr = fgets(buf, buf_size, fp);
	fclose(fp);
	if(ptr == NULL)
		return NULL;

	len = strlen(buf);
	buf[len-1] = 0;

	return buf;
}

extern int get_interface_numendpoints(const char *interface_name){
	FILE *fp;
	char target_file[128], buf[8], *ptr;

	if(interface_name == NULL || get_usb_port_by_string(interface_name, buf, sizeof(buf)) == NULL)
		return 0;

	memset(target_file, 0, 128);
	sprintf(target_file, "%s/%s/bNumEndpoints", USB_DEVICE_PATH, interface_name);
	if((fp = fopen(target_file, "r")) == NULL)
		return 0;

	memset(buf, 0, sizeof(buf));
	ptr = fgets(buf, sizeof(buf), fp);
	fclose(fp);
	if(ptr == NULL)
		return 0;

	return atoi(buf);
}

extern int get_interface_Int_endpoint(const char *interface_name){
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
		return 0;
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
			return 0;

		if(!strncmp(buf, "03", 2)){
			got_Int = 1;
			break;
		}
		else if(end_count == bNumEndpoints)
			break;
	}
	closedir(interface_dir);

	return got_Int;
}

#ifdef RTCONFIG_USB_MODEM
#ifdef LINUX30
extern int hadWWANModule(){
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

extern int hadOptionModule(){
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

extern int hadSerialModule(){
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

extern int hadACMModule(){
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
extern int hadRNDISModule(){
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

int isSerialInterface(const char *interface_name)
{
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "ff"))
		return 0;

	return 1;
}

int isACMInterface(const char *interface_name)
{
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "02"))
		return 0;

	return 1;
}

int isRNDISInterface(const char *interface_name)
{
	char interface_class[4];
	char target_file[128];
	DIR *module_dir;

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

int is_usb_modem_ready(void)
{
	if(nvram_invmatch("modem_enable", "0")
			&& ((!strcmp(nvram_safe_get("usb_path1"), "modem") && strcmp(nvram_safe_get("usb_path1_act"), ""))
					|| (!strcmp(nvram_safe_get("usb_path2"), "modem") && strcmp(nvram_safe_get("usb_path2_act"), ""))
					)
			)
		return 1;
	else
		return 0;
}
#endif // RTCONFIG_USB_MODEM

#ifdef RTCONFIG_USB_PRINTER
extern int hadPrinterModule(){
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

extern int hadPrinterInterface(const char *usb_node){
	char check_usb_node[8], device_name[4];
	int printer_order, got_printer = 0;

	for(printer_order = 0; printer_order < SCAN_PRINTER_NODE; ++printer_order){
		memset(device_name, 0, 4);
		sprintf(device_name, "lp%d", printer_order);

		if(get_usb_node_by_device(device_name, check_usb_node, sizeof(check_usb_node)) == NULL)
			continue;

		if(!strcmp(usb_node, check_usb_node)){
			got_printer = 1;

			break;
		}
	}

	return got_printer;
}

extern int isPrinterInterface(const char *interface_name){
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "07"))
		return 0;

	return 1;
}
#endif // RTCONFIG_USB_PRINTER

#ifdef RTCONFIG_USB
extern int isStorageInterface(const char *interface_name){
	char interface_class[4];

	if(get_usb_interface_class(interface_name, interface_class, 4) == NULL)
		return 0;

	if(strcmp(interface_class, "08"))
		return 0;

	return 1;
}
#endif // RTCONFIG_USB

extern char *find_sg_of_device(const char *device_name, char *buf, const int buf_size){
	DIR *dp;
	struct dirent *file;
	char target_usb_port[8], check_usb_port[8];
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
