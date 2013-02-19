/*
 * Router rc control script
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: rc.h,v 1.39 2005/03/29 02:00:06 honor Exp $
 */

#ifndef __RC_H__
#define __RC_H__

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h> // !!TB
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <net/if.h>

#include <bcmnvram.h>
#include <bcmparams.h>
#include <utils.h>
#include <shutils.h>
#include <shared.h>

#ifdef RTCONFIG_IPV6
extern char wan6face[];
#endif

#define LOGNAME get_productid()
#define is_ap_mode() (nvram_match("sw_mode", "3"))
#ifdef RTCONFIG_USB_MODEM
#define is_phyconnected() (nvram_match("link_wan", "1") || nvram_match("link_wan1", "1"))
#else
#define is_phyconnected() (nvram_match("link_wan","1"))
#endif

#define NAT_RULES	"/tmp/nat_rules"

#ifdef RTCONFIG_OLD_PARENTALCTRL
int nvram_set_by_seq(char *name, unsigned int seq, char *value);
char * nvram_get_by_seq(char *name, unsigned int seq);
int parental_ctrl(void);
#endif	/* RTCONFIG_OLD_PARENTALCTRL */

//	#define DEBUG_IPTFILE
//	#define DEBUG_RCTEST
//	#define DEBUG_NOISY

#ifdef DEBUG_NOISY
#define TRACE_PT(args...) do { _dprintf("[%d:%s +%ld] ", getpid(), __FUNCTION__, uptime()); _dprintf(args); } while(0)
#else
#define TRACE_PT(args...) do { } while(0)
#endif

#define MAX_NO_BRIDGE 	2
#define MAX_NO_MSSID	4
#define PROC_SCSI_ROOT	"/proc/scsi"
#define USB_STORAGE	"usb-storage"

#ifndef MAX_WAIT_FILE
#define MAX_WAIT_FILE 5
#endif
#define PPP_DIR "/tmp/ppp/peers"
#define PPP_CONF_FOR_3G "/tmp/ppp/peers/3g"

#ifdef RTCONFIG_USB_BECEEM
#define BECEEM_DIR "/tmp/Beceem_firmware"
#define WIMAX_CONF "/tmp/wimax.conf"
#define WIMAX_LOG "/tmp/wimax.log"
#endif

#define BOOT		0
#define REDIAL		1
#define CONNECTING	2

#define PPPOE0		0
#define PPPOE1		1

#define GOT_IP			0x01
#define RELEASE_IP		0x02
#define	GET_IP_ERROR		0x03
#define RELEASE_WAN_CONTROL	0x04
#define USB_DATA_ACCESS		0x05	//For WRTSL54GS
#define USB_CONNECT		0x06	//For WRTSL54GS
#define USB_DISCONNECT		0x07	//For WRTSL54GS

#define SERIAL_NUMBER_LENGTH	12	//ATE need
/*
// ?
#define SET_LED(val) \
{ \
	int filep; \
	if(check_hw_type() == BCM4704_BCM5325F_CHIP) { \
		if ((filep = open("/dev/ctmisc", O_RDWR,0))) \
		{ \
			ioctl(filep, val, 0); \
			close(filep); \
		} \
	} \
}
*/

#define SET_LED(val)	do { } while(0)

typedef enum { IPT_TABLE_NAT, IPT_TABLE_FILTER, IPT_TABLE_MANGLE } ipt_table_t;

#define IFUP (IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_MULTICAST)

#define	IPT_V4	0x01
#define	IPT_V6	0x02
#define	IPT_ANY_AF	(IPT_V4 | IPT_V6)
#define	IPT_AF_IS_EMPTY(f)	((f & IPT_ANY_AF) == 0)

// init.c
extern int init_main(int argc, char *argv[]);
extern int reboothalt_main(int argc, char *argv[]);
extern int console_main(int argc, char *argv[]);

// interface.c
extern int _ifconfig(const char *name, int flags, const char *addr, const char *netmask, const char *dstaddr);
#define ifconfig(name, flags, addr, netmask) _ifconfig(name, flags, addr, netmask, NULL)
extern int route_add(char *name, int metric, char *dst, char *gateway, char *genmask);
extern int route_del(char *name, int metric, char *dst, char *gateway, char *genmask);
extern int start_vlan(void);
extern int stop_vlan(void);
extern int config_vlan(void);
extern void config_loopback(void);
#ifdef RTCONFIG_IPV6
extern int ipv6_mapaddr4(struct in6_addr *addr6, int ip6len, struct in_addr *addr4, int ip4mask);
#endif

// wan.c
extern int wan_prefix(char *ifname, char *prefix);
extern int wan_ifunit(char *wan_ifname);
extern int wanx_ifunit(char *wan_ifname);
extern int preset_wan_routes(char *wan_ifname);
extern int found_default_route(int wan_unit);
extern int autodet_main(int argc, char *argv[]);

// firewall.c
extern int start_firewall(int wanunit, int lanunit);
extern void enable_ip_forward(void);

// ppp.c
extern int ipup_main(int argc, char **argv);
extern int ipdown_main(int argc, char **argv);
#ifdef RTCONFIG_IPV6
extern int ip6up_main(int argc, char **argv);
extern int ip6down_main(int argc, char **argv);
#endif
extern int authup_main(int argc, char **argv);
extern int authdown_main(int argc, char **argv);
extern int pppevent_main(int argc, char **argv);
extern int set_pppoepid_main(int argc, char **argv);	// by tallest 1219
extern int pppoe_down_main(int argc, char **argv);		// by tallest 0407

// pppd.c
extern int start_pppd(int unit);
extern void start_pppoe_relay(char *wan_if);

// network.c
extern void set_host_domain_name(void);
extern void start_lan(void);
extern void stop_lan(void);
extern void do_static_routes(int add);
extern void start_wl(void);
extern void wan_up(char *wan_ifname);
#ifdef RTCONFIG_IPV6
extern void wan6_up(const char *wan_ifname);
#endif
extern void wan_down(char *wan_ifname);
extern void update_wan_state(char *prefix, int state, int reason);
#ifdef OVERWRITE_DNS
extern int update_resolvconf();
#endif

// udhcpc.c
extern int udhcpc_wan(int argc, char **argv);
extern int udhcpc_lan(int argc, char **argv);
extern int start_udhcpc(char *wan_ifname, int unit, pid_t *ppid);
extern int zcip_wan(int argc, char **argv);
extern int start_zcip(char *wan_ifname);

#ifdef RTCONFIG_IPV6
extern int dhcp6c_state_main(int argc, char **argv);
extern void start_dhcp6c(void);
extern void stop_dhcp6c(void);
extern int ipv6aide_main(int argc, char *argv[]);
#endif

#ifdef RTCONFIG_WPS
extern int wpsaide_main(int argc, char *argv[]);
#endif

// auth.c
extern int start_auth(int unit, int wan_up);
extern int stop_auth(int unit, int wan_down);
extern int restart_auth(int unit);
#ifdef RTCONFIG_EAPOL
extern int wpacli_main(int argc, char *argv[]);
#endif

// mtd.c
extern int mtd_erase(const char *mtdname);
extern int mtd_unlock(const char *mtdname);
extern int mtd_write_main(int argc, char *argv[]);
extern int mtd_unlock_erase_main(int argc, char *argv[]);

// watchdog.c
extern int watchdog_main(int argc, char *argv[]);

// wpsfix.c
#ifdef RTCONFIG_RALINK
extern int wpsfix_main(int argc, char *argv[]);
#endif

// usbled.c
extern int usbled_main(int argc, char *argv[]);
#ifdef RTCONFIG_FANCTRL
// phy_tempsense.c
extern int phy_tempsense_main(int argc, char *argv[]);
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
// psta_monitor.c
extern int psta_monitor_main(int argc, char *argv[]);
#endif
#endif
extern int radio_main(int argc, char *argv[]);

// ntp.c
extern int ntp_main(int argc, char *argv[]);

// btnsetup.c
extern int ots_main(int argc, char *argv[]);

// common.c
extern in_addr_t inet_addr_(const char *cp);    // oleg patch
extern void usage_exit(const char *cmd, const char *help) __attribute__ ((noreturn));
#define modprobe(mod, args...) ({ char *argv[] = { "modprobe", "-s", mod, ## args, NULL }; _eval(argv, NULL, 0, NULL); })
extern int modprobe_r(const char *mod);
#define xstart(args...)	_xstart(args, NULL)
extern int _xstart(const char *cmd, ...);
extern void run_nvscript(const char *nv, const char *arg1, int wtime);
extern void run_userfile (char *folder, char *extension, const char *arg1, int wtime);
extern void setup_conntrack(void);
extern void inc_mac(char *mac, int plus);
extern void set_mac(const char *ifname, const char *nvname, int plus);
extern const char *default_wanif(void);
//	extern const char *default_wlif(void);
extern void simple_unlock(const char *name);
extern void simple_lock(const char *name);
extern void killall_tk(const char *name);
extern void kill_pidfile_tk(const char *pidfile);
extern long fappend(FILE *out, const char *fname);
extern long fappend_file(const char *path, const char *fname);
extern void logmessage(char *logheader, char *fmt, ...);
extern char *trim_r(char *str);
extern void run_custom_script(char *name, char *args);
extern void run_custom_script_blocking(char *name, char *args);


// ssh.c

// usb.c
#ifdef RTCONFIG_USB
FILE* fopen_or_warn(const char *path, const char *mode);
extern void hotplug_usb(void);
extern void start_usb(void);
extern void stop_usb(void);
extern void start_lpd();
extern void stop_lpd();
extern void start_u2ec();
extern void stop_u2ec();
int ejusb_main(int argc, const char *argv[]);

#ifdef LINUX26
extern int start_sd_idle(void);
extern int stop_sd_idle(void);
#endif
#endif

#ifdef RTCONFIG_CIFS
extern void start_cifs(void);
extern void stop_cifs(void);
extern int mount_cifs_main(int argc, char *argv[]);
#else
static inline void start_cifs(void) {};
static inline void stop_cifs(void) {};
#endif

// linkmonitor.c
extern int linkmonitor_main(int argc, char *argv[]);

// vpn.c
#ifdef RTCONFIG_OPENVPN
extern void start_vpnclient(int clientNum);
extern void stop_vpnclient(int clientNum);
extern void start_vpnserver(int serverNum);
extern void stop_vpnserver(int serverNum);
extern void start_vpn_eas();
extern void run_vpn_firewall_scripts();
extern void write_vpn_dnsmasq_config(FILE*);
extern int write_vpn_resolv(FILE*);
#else
/*
static inline void start_vpnclient(int clientNum) {}
static inline void stop_vpnclient(int clientNum) {}
static inline void start_vpnserver(int serverNum) {}
static inline void stop_vpnserver(int serverNum) {}
static inline void run_vpn_firewall_scripts() {}
static inline void write_vpn_dnsmasq_config(FILE*) {}
*/
static inline void start_vpn_eas() { }
#define write_vpn_resolv(f) (0)
#endif

#endif

// wanduck.c
extern int wanduck_main(int argc, char *argv[]);

// tcpcheck.c
extern int setupsocket(int sock);
extern void wakeme(int sig);
extern void goodconnect(int i);
extern void failedconnect(int i);
extern void subtime(struct timeval *a, struct timeval *b, struct timeval *res);
extern void setupset(fd_set *theset, int *numfds);
extern void waitforconnects();
extern int tcpcheck_main(int argc, const char *argv[]);

// usb_devices.c
#ifdef RTCONFIG_USB
extern int asus_sd(const char *device_name, const char *action);
extern int asus_lp(const char *device_name, const char *action);
extern int asus_sg(const char *device_name, const char *action);
extern int asus_sr(const char *device_name, const char *action);
extern int asus_tty(const char *device_name, const char *action);
extern int asus_usbbcm(const char *device_name, const char *action);
extern int asus_usb_interface(const char *device_name, const char *action);
#ifdef RTCONFIG_DISK_MONITOR
extern int diskmon_main(int argc, const char *argv[]);
extern void start_diskmon(void);
extern void stop_diskmon(void);
#endif
#endif

//service.c
extern void setup_leds();
extern void write_static_leases(char *file);
#ifdef RTCONFIG_DNSMASQ
extern void restart_dnsmasq();
#else
extern int restart_dns();
#endif
extern int ddns_updated_main(int argc, char *argv[]);
#ifdef RTCONFIG_IPV6
extern void start_ipv6_tunnel(void);
extern void stop_ipv6_tunnel(void);
extern void start_radvd(void);
extern void stop_radvd(void);
extern void start_dhcp6s(void);
extern void stop_dhcp6s(void);
extern void start_ipv6(void);
extern void stop_ipv6(void);
#endif
#ifdef CONFIG_BCMWL5
extern void set_acs_ifnames();
#endif
extern int service_main(int argc, char *argv[]);

#ifdef BTN_SETUP
enum BTNSETUP_STATE
{
	BTNSETUP_NONE=0,
	BTNSETUP_DETECT,
	BTNSETUP_START,
	BTNSETUP_DATAEXCHANGE,
	BTNSETUP_DATAEXCHANGE_FINISH,
	BTNSETUP_DATAEXCHANGE_EXTEND,
	BTNSETUP_FINISH
};
#endif

#define RTCONFIG_CROND y


