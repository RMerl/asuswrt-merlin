#include <rtconfig.h>

#define DEBUG_USB

#ifdef DEBUG_USB
#define usb_dbg(fmt, args...) do{ \
		FILE *fp = fopen("/tmp/usb.log", "a+"); \
		if(fp){ \
			fprintf(fp, "[usb_dbg: %s] ", __FUNCTION__); \
			fprintf(fp, fmt, ## args); \
			fclose(fp); \
		} \
	}while(0)
#else
#define usb_dbg printf
#endif

#define MAX_WAIT_FILE 5
#define SCAN_PRINTER_NODE 2

#define SYS_MODULE "/sys/module"
#define SYS_BLOCK "/sys/block"
#define SYS_TTY "/sys/class/tty"
#define SYS_NET "/sys/class/net"
#define SYS_USB "/sys/class/usb"
#define SYS_SG "/sys/class/scsi_generic"
#define USB_DEVICE_PATH "/sys/bus/usb/devices"
#define SYS_RNDIS_PATH "/sys/module/rndis_host/drivers/usb:rndis_host"

#include <rtstate.h>

#define USB_EHCI_PORT_1 get_usb_ehci_port(0)
#define USB_EHCI_PORT_2 get_usb_ehci_port(1)
#define USB_OHCI_PORT_1 get_usb_ohci_port(0)
#define USB_OHCI_PORT_2 get_usb_ohci_port(1)
#define USB_EHCI_PORT_3 get_usb_ehci_port(2)
#define USB_OHCI_PORT_3 get_usb_ohci_port(2)

enum {
	DEVICE_TYPE_UNKNOWN=0,
	DEVICE_TYPE_DISK,
	DEVICE_TYPE_PRINTER,
	DEVICE_TYPE_SG,
	DEVICE_TYPE_CD,
	DEVICE_TYPE_MODEM,
	DEVICE_TYPE_BECEEM
};

extern int get_device_type_by_device(const char *device_name);
extern char *get_device_type_by_node(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_node_by_string(const char *target_string, char *ret, const int ret_size);
extern char *get_usb_node_by_device(const char *device_name, char *buf, const int buf_size);
extern char *get_usb_port_by_string(const char *target_string, char *buf, const int buf_size);
extern char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size);
extern char *get_interface_by_string(const char *target_string, char *ret, const int ret_size);
extern char *get_interface_by_device(const char *device_name, char *buf, const int buf_size);

extern char *get_usb_vid(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_pid(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_manufacturer(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_product(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_serial(const char *usb_node, char *buf, const int buf_size);
extern char *get_usb_speed(const char *usb_node, char *buf, const int buf_size);
extern int get_usb_interface_number(const char *usb_node);
extern char *get_usb_interface_class(const char *interface_name, char *buf, const int buf_size);
extern int get_interface_numendpoints(const char *interface_name);
extern int get_interface_Int_endpoint(const char *interface_name);

#ifdef RTCONFIG_USB_MODEM
#ifdef LINUX30
extern int hadWWANModule();
#endif
extern int hadOptionModule();
extern int hadSerialModule();
extern int hadACMModule();
extern int hadRNDISModule();
extern int isSerialNode(const char *device_name);
extern int isACMNode(const char *device_name);
extern int isSerialInterface(const char *interface_name);
extern int isACMInterface(const char *interface_name);
extern int isRNDISInterface(const char *interface_name);
#ifdef RTCONFIG_USB_BECEEM
extern int isGCTInterface(const char *interface_name);
extern int hadBeceemModule();
extern int hadGCTModule(void);
extern int isBeceemNode(const char *device_name);
#endif
extern int is_usb_modem_ready(void);
#endif

#ifdef RTCONFIG_USB_PRINTER
extern int hadPrinterModule();
extern int hadPrinterInterface(const char *usb_node);
extern int isPrinterInterface(const char *interface_name);
#endif

#ifdef RTCONFIG_USB
extern int isStorageInterface(const char *interface_name);
#endif

extern char *find_sg_of_device(const char *device_name, char *buf, const int buf_size);
