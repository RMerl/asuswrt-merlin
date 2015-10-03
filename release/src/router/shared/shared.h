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

/* btn_XXX_gpio, led_XXX_gpio */
#define GPIO_ACTIVE_LOW 0x1000
#define GPIO_BLINK_LED	0x2000
#define GPIO_PIN_MASK	0x00FF

#define GPIO_DIR_IN	0
#define GPIO_DIR_OUT	1

#ifdef RT4GAC55U
#define DEF_SECOND_WANIF	"usb"
#else
#define DEF_SECOND_WANIF	"none"
#endif

#define ACTION_LOCK		"/var/lock/action"

#ifdef RTCONFIG_PUSH_EMAIL
//#define logmessage logmessage_push
#define logmessage logmessage_normal	//tmp
#else
#define logmessage logmessage_normal
#endif

#ifdef RTCONFIG_DSL
#define SYNC_LOG_FILE "/tmp/adsl/sync_status_log.txt"
#define LOG_RECORD_FILE "/tmp/adsl/log_record.txt"
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

#if defined(RTCONFIG_DEFAULT_AP_MODE)
#define DUT_DOMAIN_NAME "ap.asus.com"
#else
#define DUT_DOMAIN_NAME "router.asus.com"
#endif
#define OLD_DUT_DOMAIN_NAME1 "www.asusnetwork.net"
#define OLD_DUT_DOMAIN_NAME2 "www.asusrouter.com"

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
#define LED_CTRL_HIPRIO 	"LED_CTRL_HIPRIO"

#ifdef RTCONFIG_IPV6
enum {
	IPV6_DISABLED = 0,
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

#define EXTEND_AIHOME_API_LEVEL		1
#define EXTEND_HTTPD_AIHOME_VER		0

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
#if defined(RTAC1200HP)
extern void led_5g_onoff(void);
#endif
extern void set_action(int a);
extern int check_action(void);
extern int wait_action_idle(int n);
extern int wl_client(int unit, int subunit);
extern const char *_getifaddr(char *ifname, int family, int flags, char *buf, int size);
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
extern char *enc_str(char *str, char *enc_buf);
extern char *dec_str(char *ec_str, char *dec_buf);

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
	MODEL_DSLAC68U,
	MODEL_EAN66,
	MODEL_RTN11P,
	MODEL_RTN300,
	MODEL_RTN13U,
	MODEL_RTN14U,
	MODEL_RTAC52U,
	MODEL_RTAC51U,
	MODEL_RTN54U,
	MODEL_RTAC54U,
	MODEL_RTN56UB1,
	MODEL_RTAC1200HP,
	MODEL_RTAC55U,
	MODEL_RTAC55UHP,
	MODEL_RT4GAC55U,
	MODEL_PLN12,
	MODEL_PLAC56,
	MODEL_PLAC66U,
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
	MODEL_RPAC68U,
	MODEL_RTAC87U,
	MODEL_RTAC56S,
	MODEL_RTAC56U,
	MODEL_RTAC53U,
	MODEL_RTAC3200,
	MODEL_RTAC88U,
	MODEL_RTAC3100,
	MODEL_RTAC5300,
	MODEL_RTN14UHP,
	MODEL_RTN10U,
	MODEL_RTN10P,
	MODEL_RTN10D1,
	MODEL_RTN10PV2,
	MODEL_RTAC1200G,
	MODEL_RTAC1200GP,
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
extern int process_exists(pid_t pid);
extern int module_loaded(const char *module);

// files.c
extern int check_if_dir_empty(const char *dirpath);
extern int file_lock(char *tag);
extern void file_unlock(int lockfd);


#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

#define ACTION_LOCK_FILE "/var/lock/a_w_l" // action write lock

extern unsigned long f_size(const char *path);
extern int f_exists(const char *file);
extern int d_exists(const char *path);
extern int f_read_excl(const char *path, void *buffer, int max);
extern int f_read(const char *file, void *buffer, int max);												// returns bytes read
extern int f_write_excl(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode);
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
#define BTN_LTE				11

enum led_id {
	LED_POWER = 0,
	LED_USB,
	LED_WPS,
	FAN,
	HAVE_FAN,
	LED_LAN,
	LED_WAN,
	LED_2G,
	LED_5G,
	LED_USB3,
#ifdef RTCONFIG_LAN4WAN_LED
	LED_LAN1,
	LED_LAN2,
	LED_LAN3,
	LED_LAN4,
#endif
	LED_TURBO,
	LED_WAN_RED,
#ifdef RTCONFIG_QTN
	BTN_QTN_RESET,	/* QTN platform uses led_control() to control this gpio. */
#endif
#ifdef RTCONFIG_LED_ALL
	LED_ALL,
#endif
#ifdef RT4GAC55U
	LED_LTE,
	LED_SIG1,
	LED_SIG2,
	LED_SIG3,
#endif
	LED_SWITCH,
	LED_5G_FORCED,	/* Will handle ledbh & nvram flag */

	LED_ID_MAX,	/* last item */
};

enum led_fan_mode_id {
	LED_OFF = 0,
	LED_ON,
	FAN_OFF,
	FAN_ON,
	HAVE_FAN_OFF,
	HAVE_FAN_ON,

	LED_FAN_MODE_MAX,	/* last item */
};

static inline int have_usb3_led(int model)
{
	switch (model) {
		case MODEL_RTN18U:
		case MODEL_RTAC56U:
		case MODEL_RTAC56S:
		case MODEL_RTAC68U:
#ifndef RTCONFIG_ETRON_XHCI_USB3_LED
		case MODEL_RTAC55U:
		case MODEL_RTAC55UHP:
#endif
		case MODEL_DSLAC68U:
		case MODEL_RTAC3200:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
		case MODEL_RTAC5300:
			return 1;
	}
	return 0;
}

#define MAX_NR_WLVIF			3
enum iface_id {
	IFACE_INTERNET = 0,		/* INTERNET: Only one interface can be considered as INTERNET. */
	IFACE_WIRED,
	IFACE_BRIDGE,

	IFACE_WL0,			/* WIRELESS0 */
	IFACE_WL1,			/* WIRELESS1, may not exist. */

	IFACE_MAIN_IF_MAX,

	IFACE_WL0_0,					/* WIRELESS0.0 */
	IFACE_WL1_0 = IFACE_WL0_0 + MAX_NR_WLVIF,	/* WIRELESS1.0, may not exist. */

	IFACE_MAX,			/* last item */
};

struct if_stats {
	char ifname[IFNAMSIZ];
	unsigned long rx, tx;
};

/* INTERNET interface may varies at run-time.
 * The rx_shift and tx_shirt are used to report
 * continuously statistice information to upper-level code,
 * e.g. Web UI
 */
struct ifaces_stats {
	struct if_stats iface[IFACE_MAIN_IF_MAX];
	unsigned long rx_shift, tx_shift;
};


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary) (sizeof(ary) / sizeof((ary)[0]))
#endif
#if (defined(PLN12) || defined(PLAC56))
	PLC_WAKE,
	LED_POWER_RED,
	LED_2G_GREEN,
	LED_2G_ORANGE,
	LED_2G_RED,
	LED_5G_GREEN,
	LED_5G_ORANGE,
	LED_5G_RED,
#endif
#ifdef RTCONFIG_MMC_LED
	LED_MMC,
#endif
#ifdef RTCONFIG_RESET_SWITCH
	LED_RESET_SWITCH,
#endif
#ifdef RTAC5300
	RPM_FAN,	/* use to control FAN RPM (Hi/Lo) */
#endif

#if defined(RTCONFIG_HAS_5G)
#define MAX_NR_WL_IF			2
#else	/* ! RTCONFIG_HAS_5G */
#define MAX_NR_WL_IF			1	/* Single 2G */
#endif	/* ! RTCONFIG_HAS_5G */

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
static inline int __access_point_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP);
}

static inline int access_point_mode(void)
{
	return __access_point_mode(nvram_get_int("sw_mode"));
}

#if defined(RTCONFIG_WIRELESSREPEATER)
static inline int __repeater_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_REPEATER
#if defined(RTCONFIG_PROXYSTA)
		&& nvram_get_int("wlc_psta") != 1
#endif
		);
}
static inline int repeater_mode(void)
{
	return __repeater_mode(nvram_get_int("sw_mode"));
}
#else
static inline int __repeater_mode(int sw_mode) { return 0; }
static inline int repeater_mode(void) { return 0; }
#endif

#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
static inline int __mediabridge_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") == 1);
}
static inline int mediabridge_mode(void)
{
	return __mediabridge_mode(nvram_get_int("sw_mode"));
}
#else
static inline int __mediabridge_mode(int sw_mode) { return 0; }
static inline int mediabridge_mode(void) { return 0; }
#endif
#else
/* Should be Broadcom platform. */
static inline int __access_point_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP
#if defined(RTCONFIG_PROXYSTA)
		&& !nvram_get_int("wlc_psta")
#endif
		);
}

static inline int access_point_mode(void)
{
	return __access_point_mode(nvram_get_int("sw_mode"));
}

#if defined(RTCONFIG_WIRELESSREPEATER)
static inline int __repeater_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_REPEATER);
}
static inline int repeater_mode(void)
{
	return __repeater_mode(nvram_get_int("sw_mode"));
}
#else
static inline int __repeater_mode(int sw_mode) { return 0; }
static inline int repeater_mode(void) { return 0; }
#endif

#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
static inline int __mediabridge_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1);
}
static inline int mediabridge_mode(void)
{
	return __mediabridge_mode(nvram_get_int("sw_mode"));
}
#else
static inline int __mediabridge_mode(int sw_mode) { return 0; }
static inline int mediabridge_mode(void) { return 0; }
#endif
#endif	/* RTCONFIG_RALINK || RTCONFIG_QCA */

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
	(void) band;
	return -1;
#else
	return band;
#endif
}

#ifdef RTCONFIG_DUALWAN
static inline int dualwan_unit__usbif(int unit)
{
	return (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB);
}

static inline int dualwan_unit__nonusbif(int unit)
{
	int type = get_dualwan_by_unit(unit);
	return (type == WANS_DUALWAN_IF_WAN || type == WANS_DUALWAN_IF_DSL || type == WANS_DUALWAN_IF_LAN);
}
extern int get_usbif_dualwan_unit(void);
extern int get_primaryif_dualwan_unit(void);
#else
static inline int dualwan_unit__usbif(int unit)
{
#ifdef RTCONFIG_USB_MODEM
	return (unit == WAN_UNIT_SECOND);
#else
	return 0;
#endif
}

static inline int dualwan_unit__nonusbif(int unit)
{
	return (unit == WAN_UNIT_FIRST);
}
static inline int get_usbif_dualwan_unit(void)
{
#ifdef RTCONFIG_USB_MODEM
	return WAN_UNIT_SECOND;
#else
	return -1;
#endif
}
static inline int get_primaryif_dualwan_unit(void)
{
	return wan_primary_ifunit();
}
#endif

#if defined RTCONFIG_RALINK
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "ra", 2) == 0 && !strchr(ifname, '0');
}
#elif defined(RTCONFIG_QCA)
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "ath00",5)==0 || strncmp(ifname, "ath10",5)==0;
}
#else
/* Broadcom platform. */
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.');
}
#endif

extern int init_gpio(void);
extern int set_pwr_usb(int boolOn);
extern int button_pressed(int which);
extern int led_control(int which, int mode);
extern int led_control_atomic(int which, int mode);

/* api-*.c */
extern uint32_t gpio_dir(uint32_t gpio, int dir);
extern uint32_t set_gpio(uint32_t gpio, uint32_t value);
extern uint32_t get_gpio(uint32_t gpio);
extern int get_switch_model(void);
extern uint32_t get_phy_speed(uint32_t portmask);
#if defined(RTCONFIG_RALINK_MT7620)
extern int get_mt7620_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#elif defined(RTCONFIG_RALINK_MT7621)
extern int get_mt7621_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#endif
/* sysdeps/ralink/ *.c */
#if defined(RTCONFIG_RALINK)
extern int rtkswitch_ioctl(int val, int val2);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
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
#if defined(RTCONFIG_RALINK_MT7620)
extern int __mt7620_wan_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#elif defined(RTCONFIG_RALINK_MT7620)
extern int __mt7621_wan_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#endif

#elif defined(RTCONFIG_QCA)
extern char *wif_to_vif(char *wif);
extern int config_rtkswitch(int argc, char *argv[]);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern void ATE_qca8337_port_status(void);
#else
#define wif_to_vif(wif) (wif)
extern int config_rtkswitch(int argc, char *argv[]);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
#endif

#if defined(RTCONFIG_QCA)
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern int rtkswitch_WanPort_linkUp(void);
extern int rtkswitch_WanPort_linkDown(void);
extern int rtkswitch_LanPort_linkUp(void);
extern int rtkswitch_LanPort_linkDown(void);
extern int rtkswitch_AllPort_linkUp(void);
extern int rtkswitch_AllPort_linkDown(void);
extern int rtkswitch_Reset_Storm_Control(void);
extern int get_qca8337_PHY_power(int port);
#endif

// base64.c
extern int base64_encode(unsigned char *in, char *out, int inlen);		// returns amount of out buffer used
extern int base64_decode(const char *in, unsigned char *out, int inlen);	// returns amount of out buffer used
extern int base64_encoded_len(int len);
extern int base64_decoded_len(int len);										// maximum possible, not actual

/* boardapi.c */
extern int extract_gpio_pin(const char *gpio);
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
extern int str_escape_quotes(const char *output, const char *input, int outsize);

// file.c
extern int check_if_file_exist(const char *file);
extern int check_if_dir_exist(const char *file);
extern int check_if_dir_writable(const char *dir);

/* misc.c */
extern char *get_productid(void);
extern void logmessage_normal(char *logheader, char *fmt, ...);
extern char *get_logfile_path(void);
extern char *get_syslog_fname(unsigned int idx);
#ifdef RTCONFIG_USB_MODEM
extern char *get_modemlog_fname(void);
#endif
#if defined(RTCONFIG_SSH) || defined(RTCONFIG_HTTPS)
extern int nvram_get_file(const char *key, const char *fname, int max);
extern int nvram_set_file(const char *key, const char *fname, int max);
#endif
extern int free_caches(const char *clean_mode, const int clean_time, const unsigned int threshold);
extern void iface_name_str(enum iface_id id, char *str);
extern int load_ifaces_stats(struct ifaces_stats *old, struct ifaces_stats *new, char *exclude);
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
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
#define OVPN_FS_PATH	"/jffs/openvpn"
#define MAX_OVPN_CLIENT	5
#else
#define MAX_OVPN_CLIENT	1
#endif
extern char *get_parsed_crt(const char *name, char *buf, size_t buf_len);
extern int set_crt_parsed(const char *name, char *file_path);
extern int ovpn_crt_is_empty(const char *name);
#endif
extern int get_wifi_unit(char *wif);
extern int is_psta(int unit);
extern int is_psr(int unit);
extern int psta_exist();
extern int psta_exist_except(int unit);
extern int psr_exist();
extern int psr_exist_except(int unit);
extern unsigned int netdev_calc(char *ifname, char *ifname_desc, unsigned long *rx, unsigned long *tx, char *ifname_desc2, unsigned long *rx2, unsigned long *tx2);
extern int check_bwdpi_nvram_setting();
extern int get_iface_hwaddr(char *name, unsigned char *hwaddr);
#define xstart(args...)	_xstart(args, NULL)
extern int _xstart(const char *cmd, ...);
extern void run_custom_script(char *name, char *args);
extern void run_custom_script_blocking(char *name, char *args);
extern void run_postconf(char *name, char *config);
extern void use_custom_config(char *config, char *target);
extern void append_custom_config(char *config, FILE *fp);

/* mt7620.c */
#if defined(RTCONFIG_RALINK_MT7620)
extern void ATE_mt7620_esw_port_status(void);
#elif defined(RTCONFIG_RALINK_MT7621)
extern void ATE_mt7621_esw_port_status(void);
#endif

/* notify_rc.c */
extern int notify_rc(const char *event_name);
extern int notify_rc_after_wait(const char *event_name);
extern int notify_rc_after_period_wait(const char *event_name, int wait);
extern int notify_rc_and_wait(const char *event_name);
extern int notify_rc_and_wait_1min(const char *event_name);
extern int notify_rc_and_wait_2min(const char *event_name);
extern int notify_rc_and_period_wait(const char *event_name, int wait);

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
#ifdef RTCONFIG_DUALWAN
extern void set_wanscap_support(char *feature);
extern void add_wanscap_support(char *feature);
extern int get_wans_dualwan(void) ;
extern int get_dualwan_by_unit(int unit) ;
extern int get_dualwan_primary(void);
extern int get_dualwan_secondary(void) ;
#else
static inline int get_wans_dualwan(void) { return WANS_DUALWAN_IF_WAN; }
static inline int get_dualwan_by_unit(int unit) {
#ifdef RTCONFIG_USB_MODEM
	return ((unit == WAN_UNIT_FIRST)? WANS_DUALWAN_IF_WAN:WANS_DUALWAN_IF_USB);
#else
	return ((unit == WAN_UNIT_FIRST)? WANS_DUALWAN_IF_WAN:WANS_DUALWAN_IF_NONE);
#endif
}
#endif
extern void set_lan_phy(char *phy);
extern void add_lan_phy(char *phy);
extern void set_wan_phy(char *phy);
extern void add_wan_phy(char *phy);

/* semaphore.c */
extern void init_spinlock(void);

#if defined(RTCONFIG_WPS_LED)
static inline int wps_led_control(int onoff)
{
	return led_control(LED_WPS, onoff);
}

static inline int __wps_led_control(int onoff)
{
	return led_control(LED_WPS, onoff);
}
#else
static inline int wps_led_control(int onoff)
{
	if (nvram_get_int("led_pwr_gpio") != nvram_get_int("led_wps_gpio"))
		return led_control(LED_WPS, onoff);
	else
		return led_control(LED_POWER, onoff);
}

/* If individual WPS LED absents, don't do anything. */
static inline int __wps_led_control(int onoff) { return 0; }
#endif

enum {
	YADNS_DISABLED = -1,
	YADNS_BASIC,
	YADNS_SAFE,
	YADNS_FAMILY,
	YADNS_COUNT,
	YADNS_FIRST = YADNS_DISABLED + 1,
};

#ifdef RTCONFIG_YANDEXDNS
#define YADNS_DNSPORT 1253
int get_yandex_dns(int family, int mode, char **server, int max_count);
#endif

#if defined(RTCONFIG_WANRED_LED)
static inline int wan_red_led_control(int onoff)
{
	return led_control(LED_WAN_RED, onoff);
}
#else
static inline int wan_red_led_control(int onoff) { return 0; }
#endif

/* bled.c */
#if defined(RTCONFIG_BLINK_LED)
extern int __config_netdev_bled(const char *led_gpio, const char *ifname, unsigned int min_blink_speed, unsigned int interval);
extern int config_netdev_bled(const char *led_gpio, const char *ifname);
extern int set_bled_udef_pattern(const char *led_gpio, unsigned int interval, const char *pattern);
extern int set_bled_normal_mode(const char *led_gpio);
extern int set_bled_udef_pattern_mode(const char *led_gpio);
extern int start_bled(unsigned int gpio_nr);
extern int stop_bled(unsigned int gpio_nr);
extern int del_bled(unsigned int gpio_nr);
extern int append_netdev_bled_if(const char *led_gpio, const char *ifname);
extern int remove_netdev_bled_if(const char *led_gpio, const char *ifname);
extern int __config_swports_bled(const char *led_gpio, unsigned int port_mask, unsigned int min_blink_speed, unsigned int interval, int sleep);
extern int update_swports_bled(const char *led_gpio, unsigned int port_mask);
extern int __config_usbbus_bled(const char *led_gpio, char *bus_list, unsigned int min_blink_speed, unsigned int interval);
extern int is_swports_bled(const char *led_gpio);
#if (defined(PLN12) || defined(PLAC56))
extern void set_wifiled(int mode);
#endif

static inline void enable_wifi_bled(char *ifname)
{
	int unit;
	int v = LED_ON;

	if (!ifname || *ifname == '\0')
		return;
	unit = get_wifi_unit(ifname);
	if (unit < 0 || unit > 1) {
		return;
	}

	if (!guest_wlif(ifname)) {
#if defined(RTCONFIG_QCA)
		v = LED_OFF;	/* WiFi not ready. Don't turn on WiFi LED here. */		
#endif
#if defined(RTAC1200HP) || defined(RTN56UB1)
		if(!get_radio(1, 0) && unit==1) //*5G WiFi not ready. Don't turn on WiFi GPIO LED . */
		 	v=LED_OFF;
#endif		
#if defined(RTN56UB1)
		if(!get_radio(0, 0) && unit==0) //*2G WiFi not ready. Don't turn on WiFi GPIO LED . */
		 	v=LED_OFF;
#endif		
		led_control((!unit)? LED_2G:LED_5G, v);
	} else {
		append_netdev_bled_if((!unit)? "led_2g_gpio":"led_5g_gpio", ifname);
	}
}

static inline void disable_wifi_bled(char *ifname)
{
	int unit;

	if (!ifname || *ifname == '\0')
		return;
	unit = get_wifi_unit(ifname);
	if (unit < 0 || unit > 1)
		return;

	if (!guest_wlif(ifname)) {
		led_control((!unit)? LED_2G:LED_5G, LED_OFF);
	} else {
		remove_netdev_bled_if((!unit)? "led_2g_gpio":"led_5g_gpio", ifname);
	}
}

static inline int config_swports_bled(const char *led_gpio, unsigned int port_mask)
{
	unsigned int min_blink_speed = 10;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_swports_bled(led_gpio, port_mask, min_blink_speed, interval, 0);
}

static inline int config_swports_bled_sleep(const char *led_gpio, unsigned int port_mask)
{
	unsigned int min_blink_speed = 10;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_swports_bled(led_gpio, port_mask, min_blink_speed, interval, 1);
}

static inline int config_usbbus_bled(const char *led_gpio, char *bus_list)
{
	unsigned int min_blink_speed = 50;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_usbbus_bled(led_gpio, bus_list, min_blink_speed, interval);
}
#else	/* !RTCONFIG_BLINK_LED */
static inline int __config_netdev_bled(const char *led_gpio, const char *ifname, unsigned int min_blink_speed, unsigned int interval) { return 0; }
static inline int config_netdev_bled(const char *led_gpio, const char *ifname) { return 0; }
static inline int set_bled_udef_pattern(const char *led_gpio, unsigned int interval, const char *pattern) { return 0; }
static inline int set_bled_normal_mode(const char *led_gpio) { return 0; }
static inline int set_bled_udef_pattern_mode(const char *led_gpio) { return 0; }
static inline int start_bled(unsigned int gpio_nr) { return 0; }
static inline int stop_bled(unsigned int gpio_nr) { return 0; }
static inline int chg_bled_state(unsigned int gpio_nr) { return 0; }
static inline int del_bled(unsigned int gpio_nr) { return 0; }
static inline int append_netdev_bled_if(const char *led_gpio, const char *ifname) { return 0; }
static inline int remove_netdev_bled_if(const char *led_gpio, const char *ifname) { return 0; }
static inline int __config_swports_bled(const char *led_gpio, unsigned int port_mask, unsigned int min_blink_speed, unsigned int interval, int sleep) { return 0; }
static inline int update_swports_bled(const char *led_gpio, unsigned int port_mask) { return 0; }
static inline int __config_usbbus_bled(const char *led_gpio, char *bus_list, unsigned int min_blink_speed, unsigned int interval) { return 0; }

static inline void enable_wifi_bled(char *ifname) { }
static inline void disable_wifi_bled(char *ifname) { }
static inline int config_swports_bled(const char *led_gpio, unsigned int port_mask) { return 0; }
static inline int config_swports_bled_sleep(const char *led_gpio, unsigned int port_mask) { return 0; }
static inline int config_usbbus_bled(const char *led_gpio, char *bus_list) { return 0; }
static inline int is_swports_bled(const char *led_gpio) { return 0; }

#endif	/* RTCONFIG_BLINK_LED */

#if defined(RTCONFIG_USB)
static inline int is_usb3_port(char *usb_node)
{
	if (!usb_node)
		return 0;

	if (strstr(usb_node, get_usb_xhci_port(0)) || strstr(usb_node, get_usb_xhci_port(1)))
		return 1;

#if defined(RTAC55U) || defined(RTAC55UHP)
	/* RT-AC55U equips external Etron XHCI host and enables QCA9557 internal EHCI host.
	 * To make sure port1 maps to physical USB3 port and port2 maps to physical USB2 port respectively,
	 * one of xhci usb bus is put to first item of ehci_ports whether USB2-only mode is enabled or not.
	 * Thus, we have to check first item of ehci_ports here.
	 */
	if (strstr(usb_node, get_usb_ehci_port(0)))
		return 1;
#endif

	return 0;
}
#endif

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR

#define MIB_P0_PAGE 0x20       /* port 0 */
#define MIB_RX_REG 0x88
#define MIB_TX_REG 0x00

#if defined(RTN18U) || defined(RTAC56U) || defined(RTAC56S) || defined(RTAC68U) || defined(RTAC3200) || defined(DSL_AC68U)
#define CPU_PORT "5"
#define WAN0DEV "vlan2"
#endif

#ifdef RTAC5300
#define CPU_PORT "7"
#define WAN0DEV "vlan2"
#endif

#if defined(RTAC88U) || defined(RTAC3100)/* || defined(RTAC5300)*/
#ifdef RTCONFIG_EXT_RTL8365MB
#define CPU_PORT "7"
#define WAN0DEV "vlan2"
#else
#define CPU_PORT "5"
#define WAN0DEV "vlan2"
#endif
#endif

#ifdef RTAC87U
#define CPU_PORT "7"   /* RT-AC87U */
#define RGMII_PORT "5" /* RT-AC87U */
#define WAN0DEV "vlan2"
#endif
#endif	/* RTCONFIG_BCM5301X_TRAFFIC_MONITOR */

#endif	/* !__SHARED_H__ */
