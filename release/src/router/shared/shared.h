#ifndef __SHARED_H__
#define __SHARED_H__

#include <rtconfig.h>
#include <netinet/in.h>
#include <stdint.h>
#include <errno.h>
#include <net/if.h>
#include <rtstate.h>
#include <stdarg.h>

#ifdef RTCONFIG_USB
#include <mntent.h>	// !!TB
#endif

#ifdef RTCONFIG_PUSH_EMAIL
#define logmessage logmessage_push
#else
#define logmessage logmessage_normal
#endif

#define Y2K			946684800UL		// seconds since 1970

#define ASIZE(array)	(sizeof(array) / sizeof(array[0]))

#ifdef LINUX26
#define	MTD_DEV(arg)	"/dev/mtd"#arg
#define	MTD_BLKDEV(arg)	"/dev/mtdblock"#arg
#define	DEV_GPIO(arg)	"/dev/gpio"#arg
#else
#define	MTD_DEV(arg)	"/dev/mtd/"#arg
#define	MTD_BLKDEV(arg)	"/dev/mtdblock/"#arg
#define	DEV_GPIO(arg)	"/dev/gpio/"#arg
#endif

//version.c
extern const char *rt_version;
extern const char *rt_serialno;
extern const char *rt_extendno;
extern const char *rt_buildname;
extern const char *rt_buildinfo;
extern const char *rt_swpjverno;

#ifdef DEBUG_NOISY
#define _dprintf		cprintf
#define csprintf		cprintf
#else
#define _dprintf(args...)	do { } while(0)
#define csprintf(args...)	do { } while(0)
#endif

#define ASUS_STOP_COMMIT	"asus_stop_commit"

#ifdef RTCONFIG_IPV6
enum {
	IPV6_DISABLED = 0,
	IPV6_NATIVE,
	IPV6_NATIVE_DHCP,
	IPV6_6TO4,
	IPV6_6IN4,
	IPV6_6RD,
	IPV6_MANUAL
};

#ifndef RTF_UP
/* Keep this in sync with /usr/src/linux/include/linux/route.h */
#define RTF_UP		0x0001  /* route usable			*/
#define RTF_GATEWAY     0x0002  /* destination is a gateway	*/
#define RTF_HOST	0x0004  /* host entry (net otherwise)	*/
#define RTF_REINSTATE   0x0008  /* reinstate route after tmout	*/
#define RTF_DYNAMIC     0x0010  /* created dyn. (by redirect)	*/
#define RTF_MODIFIED    0x0020  /* modified dyn. (by redirect)	*/
#endif
#ifndef RTF_DEFAULT
#define	RTF_DEFAULT	0x00010000	/* default - learned via ND	*/
#define	RTF_ADDRCONF	0x00040000	/* addrconf route - RA		*/
#define	RTF_CACHE	0x01000000	/* cache entry			*/
#endif
#define IPV6_MASK (RTF_GATEWAY|RTF_HOST|RTF_DEFAULT|RTF_ADDRCONF|RTF_CACHE)
#endif

#define GIF_LINKLOCAL  0x0001  /* return link-local addr */
#define GIF_PREFIXLEN  0x0002  /* return addr & prefix */

enum {
	ACT_IDLE,
	ACT_TFTP_UPGRADE_UNUSED,
	ACT_WEB_UPGRADE,
	ACT_WEBS_UPGRADE_UNUSED,
	ACT_SW_RESTORE,
	ACT_HW_RESTORE,
	ACT_ERASE_NVRAM,
	ACT_NVRAM_COMMIT,
	ACT_REBOOT,
	ACT_UNKNOWN
};

typedef struct {
	int count;
	struct {
		struct in_addr addr;
		unsigned short port;
	} dns[3];
} dns_list_t;

typedef struct {
	int count;
	struct {
		char name[IFNAMSIZ + 1];
		char ip[sizeof("xxx.xxx.xxx.xxx") + 1];
	} iface[2];
} wanface_list_t;

extern char *read_whole_file(const char *target);
extern char *get_line_from_buffer(const char *buf, char *line, const int line_size);
extern char *get_upper_str(const char *const str, char **target);
extern int upper_strcmp(const char *const str1, const char *const str2);
extern int upper_strncmp(const char *const str1, const char *const str2, int count);
extern char *upper_strstr(const char *const str, const char *const target);

extern void chld_reap(int sig);
extern int get_wan_proto(void);
#ifdef RTCONFIG_IPV6
extern int get_ipv6_service(void);
#define ipv6_enabled()	(get_ipv6_service() != IPV6_DISABLED)
extern const char *ipv6_router_address(struct in6_addr *in6addr);
#if 1 /* temporary till httpd route table redo */
extern void ipv6_set_flags(char *flagstr, int flags);
#endif
extern const char *ipv6_gateway_address(void);
#else
#define ipv6_enabled()	(0)
#endif
extern void notice_set(const char *path, const char *format, ...);
extern const dns_list_t *get_dns(void);
extern void set_action(int a);
extern int check_action(void);
extern int wait_action_idle(int n);
extern int wl_client(int unit, int subunit);
extern const char *getifaddr(char *ifname, int family, int flags);
extern long uptime(void);
extern char *wl_nvname(const char *nv, int unit, int subunit);
extern int get_radio(int unit, int subunit);
extern void set_radio(int on, int unit, int subunit);
extern int nvram_get_int(const char *key);
extern int nvram_set_int(const char *key, int value);
//	extern long nvram_xget_long(const char *name, long min, long max, long def);
#ifdef RTCONFIG_SSH
extern int nvram_get_file(const char *key, const char *fname, int max);
extern int nvram_set_file(const char *key, const char *fname, int max);
#endif
extern int nvram_contains_word(const char *key, const char *word);
extern int nvram_is_empty(const char *key);
extern void nvram_commit_x(void);
extern int connect_timeout(int fd, const struct sockaddr *addr, socklen_t len, int timeout);
extern int mtd_getinfo(const char *mtdname, int *part, int *size);
#if defined(RTCONFIG_UBIFS)
extern int ubi_getinfo(const char *ubiname, int *dev, int *part, int *size);
#endif
extern int foreach_wif(int include_vifs, void *param,
	int (*func)(int idx, int unit, int subunit, void *param));

//shutils.c
extern void dbgprintf (const char * format, ...); //Ren
extern void cprintf(const char *format, ...);
extern int _eval(char *const argv[], const char *path, int timeout, int *ppid);

// usb.c
#ifdef RTCONFIG_USB
extern char *detect_fs_type(char *device);
extern struct mntent *findmntents(char *file, int swp,
	int (*func)(struct mntent *mnt, uint flags), uint flags);
extern int find_label_or_uuid(char *dev_name, char *label, char *uuid);
extern void add_remove_usbhost(char *host, int add);

#define DEV_DISCS_ROOT	"/dev/discs"

/* Flags used in exec_for_host calls
 */
#define EFH_1ST_HOST	0x00000001	/* func is called for the 1st time for this host */
#define EFH_1ST_DISC	0x00000002	/* func is called for the 1st time for this disc */
#define EFH_HUNKNOWN	0x00000004	/* host is unknown */
#define EFH_USER	0x00000008	/* process is user-initiated - either via Web GUI or a script */
#define EFH_SHUTDN	0x00000010	/* exec_for_host is called at shutdown - system is stopping */
#define EFH_HP_ADD	0x00000020	/* exec_for_host is called from "add" hotplug event */
#define EFH_HP_REMOVE	0x00000040	/* exec_for_host is called from "remove" hotplug event */
#define EFH_PRINT	0x00000080	/* output partition list to the web response */

typedef int (*host_exec)(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags);
extern int exec_for_host(int host, int obsolete, uint flags, host_exec func);
extern int is_no_partition(const char *discname);
#endif //RTCONFIG_USB

// id.c
enum {
	MODEL_UNKNOWN,
	MODEL_DSLN55U,
	MODEL_EAN66,
	MODEL_RTN13U,
	MODEL_RTN14U,
	MODEL_RTAC52U,
	MODEL_RTN36U3,
	MODEL_RTN56U,
	MODEL_RTN65U,
	MODEL_RTN67U,
	MODEL_RTN12,
	MODEL_RTN12B1,
	MODEL_RTN12C1,
	MODEL_RTN12D1,
	MODEL_RTN12VP,
	MODEL_RTN12HP,
	MODEL_RTN12HP_B1,
	MODEL_APN12,
	MODEL_APN12HP,
	MODEL_RTN16,
	MODEL_RTN18U,
	MODEL_RTN15U,
	MODEL_RTN53,
	MODEL_RTN66U,
	MODEL_RTAC66U,
	MODEL_RTAC68U,
	MODEL_RTAC56U,
	MODEL_RTAC53U,
	MODEL_RTN14UHP,
	MODEL_RTN10U,
	MODEL_RTN10P,
	MODEL_RTN10D1,
	MODEL_RTN10PV2,
	MODEL_GENERIC
};

enum {
	SWITCH_UNKNOWN,
	SWITCH_BCM5325,
	SWITCH_BCM53115,
	SWITCH_BCM53125,
	SWITCH_BCM5301x,
	SWITCH_GENERIC
};

#define RTCONFIG_NVRAM_VER "1"

/* NOTE: Do not insert new entries in the middle of this enum,
 * always add them to the end! The numeric Hardware ID value is
 * stored in the configuration file, and is used to determine
 * whether or not this config file can be restored on the router.
 */
enum {
	HW_BCM4702,
	HW_BCM4712,
	HW_BCM5325E,
	HW_BCM4704_BCM5325F,
	HW_BCM5352E,
	HW_BCM5354G,
	HW_BCM4712_BCM5325E,
	HW_BCM4704_BCM5325F_EWC,
	HW_BCM4705L_BCM5325E_EWC,
	HW_BCM5350,
	HW_BCM5356,
	HW_BCM4716,
	HW_BCM4718,
	HW_BCM4717,
	HW_BCM5365,
	HW_BCM4785,
	HW_UNKNOWN
};

#define SUP_SES			(1 << 0)
#define SUP_BRAU		(1 << 1)
#define SUP_AOSS_LED		(1 << 2)
#define SUP_WHAM_LED		(1 << 3)
#define SUP_HPAMP		(1 << 4)
#define SUP_NONVE		(1 << 5)
#define SUP_80211N		(1 << 6)
#define SUP_1000ET		(1 << 7)

extern int check_hw_type(void);
//	extern int get_hardware(void) __attribute__ ((weak, alias ("check_hw_type")));
extern int get_model(void);
extern char *get_modelid(int model);
extern char *get_productid(void);
extern int get_switch(void);
extern int supports(unsigned long attr);

// pids.c
extern int pids(char *appname);

// process.c
extern char *psname(int pid, char *buffer, int maxlen);
extern int pidof(const char *name);
extern int killall(const char *name, int sig);
extern int ppid(int pid);
extern int process_exists(pid_t pid);
extern int module_loaded(const char *module);

// files.c
extern int check_if_dir_empty(const char *dirpath);
extern int file_lock(char *tag);
extern void file_unlock(int lockfd);


#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

extern unsigned long f_size(const char *path);
extern int f_exists(const char *file);
extern int d_exists(const char *path);
extern int f_read(const char *file, void *buffer, int max);												// returns bytes read
extern int f_write(const char *file, const void *buffer, int len, unsigned flags, unsigned cmode);		//
extern int f_read_string(const char *file, char *buffer, int max);										// returns bytes read, not including term; max includes term
extern int f_write_string(const char *file, const char *buffer, unsigned flags, unsigned cmode);		//
extern int f_read_alloc(const char *path, char **buffer, int max);
extern int f_read_alloc_string(const char *path, char **buffer, int max);
extern int f_wait_exists(const char *name, int max);
extern int f_wait_notexists(const char *name, int max);


// button & led
#define BTN_RESET			0
#define BTN_WPS				1
#define BTN_FAN				2
#define BTN_HAVE_FAN			3
#define BTN_WIFI_SW			4
#define BTN_SWMODE_SW_ROUTER		5
#define BTN_SWMODE_SW_REPEATER		6
#define BTN_SWMODE_SW_AP		7
#define BTN_WIFI_TOG			8
#define BTN_TURBO			9
#define BTN_LED				0xA

#define LED_POWER			0
#define LED_USB				1
#define LED_WPS				2
#define FAN				3
#define HAVE_FAN			4
#define LED_LAN				5
#define LED_WAN				6
#ifdef RTCONFIG_LED_ALL
#define LED_ALL				0xFE
#endif
#define LED_2G				7
#define LED_5G				8
#define LED_USB3			9
#ifdef RTCONFIG_LAN4WAN_LED
#define LED_LAN1        10
#define LED_LAN2        11
#define LED_LAN3        12
#define LED_LAN4        13
#endif
#define LED_TURBO			14
#define LED_SWITCH			20

#define	LED_OFF				0
#define	LED_ON				1
#define FAN_OFF				2
#define FAN_ON				3
#define HAVE_FAN_OFF			4
#define	HAVE_FAN_ON			5

#define MAX_NR_WL_IF			2
static inline int get_wps_multiband(void)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	return nvram_get_int("wps_multiband");
#else
	return 0;
#endif
}

static inline int get_radio_band(int band)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	return -1;
#else
	return band;
#endif
}

extern int init_gpio(void);
extern int set_pwr_usb(int boolOn);
extern int button_pressed(int which);
extern int led_control(int which, int mode);

/* api-*.c */
extern uint32_t gpio_dir(uint32_t gpio, int dir);
extern uint32_t set_gpio(uint32_t gpio, uint32_t value);
extern uint32_t get_gpio(uint32_t gpio);
extern int get_switch_model(void);
extern uint32_t get_phy_speed(uint32_t portmask);
#if defined(RTN14U) || defined(RTAC52U)
extern int get_wan_bytecount(int dir, unsigned long *count);
extern int mt7620_wan_bytecount(int dir, unsigned long *count);
#endif

/* sysdeps/ralink/ *.c */
#ifdef RTCONFIG_RALINK
extern int rtkswitch_ioctl(int val, int val2);
extern unsigned int rtkswitch_wanPort_phyStatus(void);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern int rtkswitch_WanPort_linkUp(void);
extern int rtkswitch_WanPort_linkDown(void);
extern int rtkswitch_LanPort_linkUp(void);
extern int rtkswitch_LanPort_linkDown(void);
extern int rtkswitch_AllPort_linkUp(void);
extern int rtkswitch_AllPort_linkDown(void);
extern int rtkswitch_Reset_Storm_Control(void);
extern int ralink_gpio_read_bit(int idx);
extern int ralink_gpio_write_bit(int idx, int value);
extern char *wif_to_vif(char *wif);
extern int ralink_gpio_init(unsigned int idx, int dir);
extern int config_rtkswitch(int argc, char *argv[]);
extern int get_channel_list_via_driver(int unit, char *buffer, int len);
extern int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len);
#else
#define wif_to_vif(wif) (wif)
#endif

// base64.c
extern int base64_encode(unsigned char *in, char *out, int inlen);		// returns amount of out buffer used
extern int base64_decode(const char *in, unsigned char *out, int inlen);	// returns amount of out buffer used
extern int base64_encoded_len(int len);
extern int base64_decoded_len(int len);										// maximum possible, not actual

/* boardapi.c */
extern int lanport_status(void);

/* discover.c */
extern int discover_all(void);

// strings.c
extern int char_to_ascii_safe(const char *output, const char *input, int outsize);
extern void char_to_ascii(const char *output, const char *input);
extern int ascii_to_char_safe(const char *output, const char *input, int outsize);
extern void ascii_to_char(const char *output, const char *input);
extern const char *find_word(const char *buffer, const char *word);
extern int remove_word(char *buffer, const char *word);

// file.c
extern int check_if_file_exist(const char *file);
extern int check_if_dir_exist(const char *file);
extern int check_if_dir_writable(const char *dir);

/* misc.c */
extern char *get_productid(void);
extern void logmessage_normal(char *logheader, char *fmt, ...);
extern char *get_logfile_path(void);
extern char *get_syslog_fname(unsigned int idx);
#if defined(RTCONFIG_SSH) || defined(RTCONFIG_HTTPS)
extern int nvram_get_file(const char *key, const char *fname, int max);
extern int nvram_set_file(const char *key, const char *fname, int max);
#endif
extern int free_caches(const char *clean_mode, const int clean_time, const unsigned int threshold);
extern unsigned int netdev_calc(char *ifname, char *ifname_desc, unsigned long *rx, unsigned long *tx, char *ifname_desc2, unsigned long *rx2, unsigned long *tx2);
extern int update_6rd_info(void);
extern const char *get_wanip(void);
extern int is_private_subnet(const char *ip);
extern const char *get_wanface(void);
extern const char *get_wanip(void);
extern int is_intf_up(const char* ifname);
extern uint32_t crc_calc(uint32_t crc, const char *buf, int len);
#ifdef RTCONFIG_IPV6
extern const char *get_wan6face(void);
extern int get_ipv6_service(void);
extern const char *ipv6_router_address(struct in6_addr *in6addr);
extern const char *ipv6_address(const char *ipaddr6);
extern const char *ipv6_prefix(struct in6_addr *in6addr);
extern void reset_ipv6_linklocal_addr(const char *ifname, int flush);
extern int with_ipv6_linklocal_addr(const char *ifname);
#if 1 /* temporary till httpd route table redo */
extern void ipv6_set_flags(char *flagstr, int flags);
extern char* INET6_rresolve(struct sockaddr_in6 *sin6, int numeric);
#endif
extern const char *ipv6_gateway_address(void);
#endif
#ifdef RTCONFIG_OPENVPN
extern char *get_parsed_crt(const char *name, char *buf);
extern int set_crt_parsed(const char *name, char *file_path);
#endif

/* mt7620.c */
extern void ATE_mt7620_esw_port_status(void);

/* notify_rc.c */
extern int notify_rc(const char *event_name);
extern int notify_rc_after_wait(const char *event_name);
extern int notify_rc_after_period_wait(const char *event_name, int wait);
extern int notify_rc_and_wait(const char *event_name);
extern int notify_rc_and_wait_1min(const char *event_name);
extern int notify_rc_and_wait_2min(const char *event_name);

/* rtstate.c */
extern char *get_wanx_ifname(int unit);
extern int get_lanports_status(void);
extern int set_wan_primary_ifunit(const int unit);
#ifdef RTCONFIG_IPV6
extern char *get_wan6_ifname(int unit);
#endif
#ifdef RTCONFIG_USB
extern char *get_usb_xhci_port(int port);
extern char *get_usb_ehci_port(int port);
extern char *get_usb_ohci_port(int port);
extern int get_usb_port_number(const char *usb_port);
extern int get_usb_port_host(const char *usb_port);
#endif

/* semaphore.c */
extern void init_spinlock(void);

#endif
