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

#include <rtconfig.h>
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
#include <signal.h>

#include <bcmnvram.h>
#include <bcmparams.h>
#include <utils.h>
#include <shutils.h>
#include <shared.h>

#include "sysdeps.h"
#include <linux/version.h>

#if defined(RTCONFIG_USB)
#include <usb_info.h>
#endif

#define IFUP (IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_MULTICAST)

#define USBCORE_MOD	"usbcore"
#if defined (RTCONFIG_USB_XHCI)
#if defined(RTCONFIG_ETRON_XHCI)
#define USB30_MOD	"etxhci-hcd"
#else
#define USB30_MOD	"xhci-hcd"
#endif
#else
#endif

#if defined(RTN56UB1) || defined(RTN56UB2) //for MT7621
#define USB20_MOD	"xhci-hcd"
#else
#define USB20_MOD	"ehci-hcd"
#endif

#if defined(RTAC58U) ||  defined(RTAC82U)     
#define USB_DWC3	"dwc3"
#define USB_DWC3_IPQ	"dwc3-ipq40xx"
#define USB_PHY1	"phy-qca-baldur"
#define USB_PHY2	"phy-qca-uniphy"
#endif

#define USBSTORAGE_MOD	"usb-storage"
#define SCSI_MOD	"scsi_mod"
#define SD_MOD		"sd_mod"
#define SG_MOD		"sg"
#ifdef LINUX26
#define USBOHCI_MOD	"ohci-hcd"
#define USBUHCI_MOD	"uhci-hcd"
#define USBPRINTER_MOD	"usblp"
#define SCSI_WAIT_MOD	"scsi_wait_scan"
#define USBFS		"usbfs"
#else
#define USBOHCI_MOD	"usb-ohci"
#define USBUHCI_MOD	"usb-uhci"
#define USBPRINTER_MOD	"printer"
#define USBFS		"usbdevfs"
#endif

#ifdef RTCONFIG_IPV6
extern char wan6face[];
#endif

/* services.c */
extern int g_reboot;
extern int wan_phyid;
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_JFFSV1) || defined(RTCONFIG_BRCM_NAND_JFFS2)
extern int jffs2_fail;
#endif

#ifdef RTCONFIG_BCMARM
#ifndef LINUX_KERNEL_VERSION
#define LINUX_KERNEL_VERSION LINUX_VERSION_CODE
#endif

static inline int before(int ver1, int ver2)
{
	return (ver1-ver2) < 0;
}

#define After(ver2, ver1)	before(ver1, ver2)
#endif

#if defined(LINUX30) || LINUX_VERSION_CODE > KERNEL_VERSION(2,6,34)
#define DAYS_PARAM	" --kerneltz --weekdays "
#else
#define DAYS_PARAM	" --days "
#endif

#define LOGNAME get_productid()
#define is_ap_mode() (nvram_match("sw_mode", "3"))
#ifdef RTCONFIG_USB_MODEM
#define is_phyconnected() (nvram_match("link_wan", "1") || nvram_match("link_wan1", "1"))
#else
#define is_phyconnected() (nvram_match("link_wan","1"))
#endif

#define NAT_RULES	"/tmp/nat_rules"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary) (sizeof(ary) / sizeof((ary)[0]))
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

//	#define DEBUG_IPTFILE
//	#define DEBUG_RCTEST
//	#define DEBUG_NOISY

#ifdef DEBUG_NOISY
#define TRACE_PT(fmt, args...)		\
do {					\
	char psn[16];			\
	pid_t pid;			\
	pid = getpid();			\
	psname(pid, psn, sizeof(psn));	\
	_dprintf("[%d %s:%s +%ld] " fmt,\
		pid, psn, __func__, uptime(), ##args);	\
} while(0)
#else
#define TRACE_PT(args...) do { } while(0)
#endif

#ifdef RTCONFIG_RESTRICT_GUI
#define BIT_RES_GUI	"0x100"
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

#define NOTIFY_DIR "/tmp/notify"
#define NOTIFY_TYPE_USB "usb"

#define BOOT		0
#define REDIAL		1
#define CONNECTING	2

#define PPPOE0		0
#define PPPOE1		1

#define DSL_WAN_VID		3880		//0x0F28
#define DSL_WAN_VIF		"vlan3880"	

#define GOT_IP			0x01
#define RELEASE_IP		0x02
#define	GET_IP_ERROR		0x03
#define RELEASE_WAN_CONTROL	0x04
#define USB_DATA_ACCESS		0x05	//For WRTSL54GS
#define USB_CONNECT		0x06	//For WRTSL54GS
#define USB_DISCONNECT		0x07	//For WRTSL54GS

#define SERIAL_NUMBER_LENGTH	12	//ATE need

#define SET_LED(val)	do { } while(0)

typedef enum { IPT_TABLE_NAT, IPT_TABLE_FILTER, IPT_TABLE_MANGLE } ipt_table_t;

#define IFUP (IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_MULTICAST)

#define	IPT_V4	0x01
#define	IPT_V6	0x02
#define	IPT_ANY_AF	(IPT_V4 | IPT_V6)
#define	IPT_AF_IS_EMPTY(f)	((f & IPT_ANY_AF) == 0)

/* api-*.c */
extern uint32_t get_phy_status(uint32_t portmask);
extern uint32_t get_phy_speed(uint32_t portmask);
extern uint32_t set_phy_ctrl(uint32_t portmask, int ctrl);
extern uint32_t set_gpio(uint32_t gpio, uint32_t value);
extern uint32_t get_gpio(uint32_t gpio);
extern uint32_t gpio_dir(uint32_t gpio, int dir);


/* ate.c */
extern int asus_ate_command(const char *command, const char *value, const char *value2);
extern int ate_dev_status(void);
extern int isValidMacAddr(const char* mac);
extern int isValidCountryCode(const char *Ccode);
extern int isValidRegrev(const char *regrev);
extern int isValidSN(const char *sn);
extern int isNumber(const char *num);
extern int pincheck(const char *a);
extern int isValidChannel(int is_2G, char *channel);
extern int setPSK(const char *psk);
extern int getPSK(void);
#if defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM)
extern int start_envrams(void);
extern int chk_envrams_proc(void);
#endif
extern int ate_run_arpstrom(void);

/* tcode_rc.c */
#ifdef RTCONFIG_TCODE
extern int config_tcode(int type);
#ifdef RTAC68U
extern unsigned int hardware_flag();
#endif
#endif

/* ate-XXX.c */
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
extern int IS_ATE_FACTORY_MODE(void);
#endif

/* shared/boardapi.c */
extern int wanport_ctrl(int ctrl);
extern int lanport_ctrl(int ctrl);
extern int wanport_status(int wan_unit);

/* board API under sysdeps directory */
extern void set_factory_mode();
extern void ate_commit_bootlog(char *err_code);
extern int setAllLedOn(void);
extern int setAllLedOff(void);
extern int setATEModeLedOn(void);
extern int start_wps_method(void);
extern int stop_wps_method(void);
extern int is_wps_stopped(void);
extern int setMAC_2G(const char *mac);
extern int setMAC_5G(const char *mac);
#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
extern int getRegSpec(void);
extern int getRegDomain_2G(void);
extern int getRegDomain_5G(void);
extern int setRegSpec(const char *regSpec, int do_write);
extern int setRegDomain_2G(const char *cc);
extern int setRegDomain_5G(const char *cc);
#else
extern int getCountryCode_2G(void);
extern int setCountryCode_2G(const char *cc);
#endif
extern int setCountryCode_5G(const char *cc);
extern int setSN(const char *SN);
extern int getSN(void);
extern int setPIN(const char *pin);
extern int getPIN(void);
extern int set40M_Channel_2G(char *channel);
extern int set40M_Channel_5G(char *channel);
extern int ResetDefault(void);
extern int getBootVer(void);
extern int getMAC_2G(void);
extern int getMAC_5G(void);
extern int GetPhyStatus(int verbose);
extern int Get_ChannelList_2G(void);
extern int Get_ChannelList_5G(void);
extern void Get_fail_ret(void);
extern void Get_fail_reboot_log(void);
extern void Get_fail_dev_log(void);
#ifdef RTCONFIG_ODMPID
extern int setMN(const char *MN);
extern int getMN(void);
#endif
extern int check_imagefile(char *fname);
#if defined(RTCONFIG_RALINK)
extern int set_wantolan(void);
static inline void platform_start_ate_mode(void) { };
static inline int setWlOffLed(void) { return 0; }
#elif defined(RTCONFIG_QCA)
extern int get_imageheader_size(void);
extern void platform_start_ate_mode(void);
extern void set_uuid(void);
static inline int setWlOffLed(void) { return 0; }
#elif defined(CONFIG_BCMWL5)
static inline void platform_start_ate_mode(void) { };
extern int setWlOffLed(void);
#else
extern int setWlOffLed(void);
#error
#endif
#ifndef CONFIG_BCMWL5
extern int IS_ATE_FACTORY_MODE(void);
#endif

/* board API under sysdeps/ralink/ralink.c */
#ifdef RTCONFIG_RALINK
extern int FWRITE(const char *da, const char* str_hex);
extern int FREAD(unsigned int addr_sa, int len);
#if defined(RTN65U)
extern void ate_run_in(void);
#endif
extern int gen_ralink_config(int band, int is_iNIC);
extern int get_channel(int band);
extern int __need_to_start_wps_band(char *prefix);
extern int need_to_start_wps_band(int wps_band);
extern void start_wsc_pin_enrollee(void);
extern void stop_wsc(void);
extern void stop_wsc_both(void);
extern void start_wsc(void);
extern void wps_oob_both(void);
extern void wsc_user_commit();
extern int getrssi(int band);
extern int asuscfe(const char *PwqV, const char *IF);
extern int stainfo(int band);
extern int Set_SwitchPort_LEDs(const char *group, const char *action);
extern int ralink_mssid_mac_validate(const char *macaddr);
extern int getWscStatus(int unit);
extern int wl_WscConfigured(int unit);
extern int Get_Device_Flags(void);
extern int Set_Device_Flags(const char *flags_str);
extern const char *get_wifname(int band);
extern const char *get_wpsifname(void);
#endif

/* board API under sysdeps/qca/qca.c */
#if defined(RTCONFIG_QCA)
extern int FWRITE(const char *da, const char* str_hex);
extern int FREAD(unsigned int addr_sa, int len);
extern int gen_ath_config(int band, int is_iNIC,int subnet);
extern int __need_to_start_wps_band(char *prefix);
extern int need_to_start_wps_band(int wps_band);
extern int getEEPROM(unsigned char *outbuf, unsigned short *lenpt, char *area);
extern void hexdump(unsigned char *pt, unsigned short len);
extern void setCTL(const char *);
extern int verify_ctl_table(void);
#ifdef RTCONFIG_ATEUSB3_FORCE
extern int getForceU3(void);
extern int setForceU3(const char *val);
#endif
extern char *getStaMAC(void);
extern unsigned int getPapState(int unit);
typedef unsigned int	u_int;
extern u_int ieee80211_mhz2ieee(u_int freq);
#if defined(RTCONFIG_WIFI_QCA9557_QCA9882)
extern void Set_ART2(void);
extern void Get_EEPROM_X(char *command);
extern void Get_CalCompare(void);
#endif
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994) || defined(RTCONFIG_SOC_IPQ40XX)
extern void Set_Qcmbr(const char *value);
extern void Get_BData_X(const char *command);
extern int start_thermald(void);
#endif
extern int country_to_code(char *ctry, int band);
#endif	/* RTCONFIG_QCA */

/* board API under sysdeps/qca/ctl.c */
#if defined(RTCONFIG_QCA)
extern const void *req_fw_hook(const char *filename, size_t *new_size);
#else
static inline const void *req_fw_hook(const char *filename, size_t *new_size) { return NULL; }
#endif

/* board API under sysdeps/init-broadcom.c sysdeps/broadcom sysdeps/tcode_brcm.c */
#ifdef CONFIG_BCMWL5
extern void wl_vif_hwaddr_set(const char *name);
extern int wlconf(char *ifname, int unit, int subunit);
extern int wl_send_dif_event(const char *ifname, uint32 event);
extern int check_wl_client(char *ifname, int unit, int subunit);
extern void wlconf_post(const char *ifname);
extern void wlconf_pre();
extern void init_wl_compact(void);
extern void check_afterburner(void);
extern void init_others(void);
extern int set_wltxpower();
extern int is_ure(int unit);
#ifdef RTCONFIG_BCMWL6
extern void led_bh_prep(int post);
extern int wl_check_chanspec();
extern void wl_check_5g_band_group();
#if defined(RTCONFIG_BCMARM) && defined(RTCONFIG_PROXYSTA)
extern void reset_psr_hwaddr();
#endif
#endif
#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) || defined(RTAC5300R)
extern void ldo_patch();
extern int wl_channel_valid(char *wif, int channel);
extern int wl_subband(char *wif, int idx);
extern void check_4366_dummy(void);
extern void check_4366_fabid(void);
#endif
extern void wl_dfs_radarthrs_config(char *ifname, int unit);
#ifdef RTCONFIG_BCM_7114
extern int wlcscan_core_escan(char *ofile, char *wif);
#endif
extern int setRegrev_2G(const char *regrev);
extern int setRegrev_5G(const char *regrev);
#if defined(RTAC3200) || defined(RTAC5300) || defined(RTAC5300R)
extern int setMAC_5G_2(const char *mac);
extern int getMAC_5G_2(void);
extern int Get_ChannelList_5G_2(void);
#endif
extern int setCommit(void);
extern int setWaitTime(const char *wtime);
extern int setWiFi2G(const char *act);
extern int setWiFi5G(const char *act);
extern int getCountryCode_5G(void);
extern int getRegrev_2G(void);
extern int getRegrev_5G(void);
extern int getSSID(int unit);
extern void check_wl_country();
extern void wl_dfs_support(int unit);
#if defined(RTAC3200) || defined(RTAC68U) || defined(RTAC5300) || defined(RTAC5300R) || defined(RTAC88U) || defined(RTAC3100)
extern void wl_disband5grp();
#endif
#ifdef RTCONFIG_BCMARM
extern int setWanLedMode1(void);
extern int setWanLedMode2(void);
extern void tweak_smp_affinity(int enable_samba);
#endif
#ifdef WLCLMLOAD
extern int download_clmblob_files();
#endif
#ifdef RTAC68U
extern void check_cfe_ac68u();
extern void update_cfe();
#endif
#ifdef RTAC3200
extern void update_cfe_ac3200();
extern void bsd_defaults(void);
#endif
#ifdef RTCONFIG_BCMWL6
extern int hw_vht_cap();
#endif
#endif

/* sysdeps/dsl-*.c */
extern int check_tc_firmware_crc(void);
extern int truncate_trx(void);
extern void do_upgrade_adsldrv(void);
extern int compare_linux_image(void);

// init.c
extern int init_main(int argc, char *argv[]);
extern int reboothalt_main(int argc, char *argv[]);
extern int console_main(int argc, char *argv[]);
extern int limit_page_cache_ratio(int ratio);
extern int init_nvram(void);
extern void wl_defaults(void);
extern void wl_defaults_wps(void);
extern void restore_defaults_module(char *prefix);
extern void clean_modem_state(int flag);
extern void restore_defaults_wifi(int all);
extern void clean_vlan_ifnames(void);
extern unsigned int num_of_mssid_support(unsigned int unit);
#ifdef RTCONFIG_BCMFA
extern void fa_nvram_adjust();
#endif

// format.c
extern void adjust_merlin_config();
extern void adjust_url_urlelist();
extern void adjust_ddns_config();
extern void adjust_access_restrict_config();

// interface.c
extern int _ifconfig(const char *name, int flags, const char *addr, const char *netmask, const char *dstaddr, int mtu);
static inline int ifconfig(const char *name, int flags, const char *addr, const char *netmask)
{
	return _ifconfig(name, flags, addr, netmask, NULL, 0);
}
extern int _ifconfig_get(const char *name, int *flags, char *addr, char *netmask, char *dstaddr, int *mtu);
extern int ifconfig_mtu(const char *name, int mtu);
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
#ifdef RTCONFIG_QCA_PLC_UTILS
extern int autodet_plc_main(int argc, char *argv[]);
#endif
extern int autodet_main(int argc, char *argv[]);
extern void start_wan(void);
extern void stop_wan(void);
extern int add_multi_routes(void);
extern int add_routes(char *prefix, char *var, char *ifname);
extern int del_routes(char *prefix, char *var, char *ifname);
extern void start_wan_if(int unit);
extern void stop_wan_if(int unit);
#ifdef RTCONFIG_IPV6
extern void stop_wan6(void);
#if 0
extern void start_ecmh(const char *wan_ifname);
extern void stop_ecmh(void);
#endif
extern void wan6_up(const char *wan_ifname);
extern void wan6_down(const char *wan_ifname);
extern void start_wan6(void);
extern void stop_wan6(void);
#endif
extern int do_dns_detect(int wan_unit);

// lan.c
extern void update_lan_state(int state, int reason);
extern void set_et_qos_mode(void);
extern void start_wl(void);
extern void stop_wl(void);
extern char *get_hwaddr(const char *ifname);
extern void start_lan(void);
extern void stop_lan(void);
extern void do_static_routes(int add);
extern void hotplug_net(void);
extern int radio_switch(int subunit);
extern int radio_main(int argc, char *argv[]);
extern int wldist_main(int argc, char *argv[]);
extern int update_lan_resolvconf(void);
extern void lan_up(char *lan_ifname);
extern void lan_down(char *lan_ifname);
extern void stop_lan_wl(void);
extern void start_lan_wl(void);
extern void restart_wl(void);
extern void lanaccess_mssid_ban(const char *ifname_in);
extern void lanaccess_wl(void);
extern void restart_wireless(void);
#ifdef RTCONFIG_BCM_7114
extern void stop_wl_bcm(void);
#endif
extern void start_wan_port(void);
extern void stop_wan_port(void);
extern void start_lan_port(int dt);
extern void stop_lan_port(void);
extern void start_lan_wlport(void);
extern void stop_lan_wlport(void);
extern int wl_dev_exist(void);
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
extern pid_t pid_from_file(char *pidfile);
#endif
extern int delay_main(int argc, char *argv[]);
#ifdef RTCONFIG_IPV6
extern void set_default_accept_ra(int flag);
extern void set_intf_ipv6_accept_ra(const char *ifname, int flag);
extern void set_intf_ipv6_dad(const char *ifname, int bridge, int flag);
extern void config_ipv6(int enable, int incl_wan);
#ifdef RTCONFIG_DUALWAN
extern void start_lan_ipv6(void);
extern void stop_lan_ipv6(void);
extern void restart_dnsmasq_ipv6(void);
#endif
extern void enable_ipv6(const char *ifname);
extern void disable_ipv6(const char *ifname);
extern void start_lan_ipv6(void);
extern void stop_lan_ipv6(void);
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
extern void start_lan_wlc(void);
extern void stop_lan_wlc(void);
#endif
#if defined(RTCONFIG_QCA)
extern void stavap_start(void);
extern void qca_wif_up(const char* wif);
extern void gen_qca_wifi_cfgs(void);
#endif

// firewall.c
extern int start_firewall(int wanunit, int lanunit);
extern void enable_ip_forward(void);
extern void convert_routes(void);
extern void start_default_filter(int lanunit);
extern int ipt_addr_compact(const char *s, int af, int strict);
extern void filter_setting(char *wan_if, char *wan_ip, char *lan_if, char *lan_ip, char *logaccept, char *logdrop);
#ifdef WEB_REDIRECT
extern void redirect_setting(void);
#endif
extern void ipt_account(FILE *fp, char *interface);

/* pc.c */
#ifdef RTCONFIG_PARENTALCTRL
extern int pc_main(int argc, char *argv[]);
extern int count_pc_rules(void);
extern void config_daytime_string(FILE *fp, char *logaccept, char *logdrop);
#endif
extern void redirect_nat_setting(void);
#if !defined(CONFIG_BCMWL5) && defined(RTCONFIG_DUALWAN)
extern void set_load_balance(void);
#else
static inline void set_load_balance(void) { }
#endif
extern void ip2class(char *lan_ip, char *netmask, char *buf);
#define IPTABLES_MARK_LB_SET(x)	((((x)&0xf)|0x8)<<4)			//mark for load-balance
#define IPTABLES_MARK_LB_MASK	IPTABLES_MARK_LB_SET(0xf)

// pc_block.c
#ifdef RTCONFIG_PARENTALCTRL
extern int pc_block_main(int argc, char *argv[]);
extern void config_blocking_redirect(FILE *fp);
#endif

// ppp.c
extern int ipup_main(int argc, char **argv);
extern int ipdown_main(int argc, char **argv);
extern int ippreup_main(int argc, char **argv);
#ifdef RTCONFIG_IPV6
extern int ip6up_main(int argc, char **argv);
extern int ip6down_main(int argc, char **argv);
#endif
extern int authfail_main(int argc, char **argv);
extern int ppp_ifunit(char *ifname);
extern int ppp_linkunit(char *linkname);

// pppd.c
extern char *ppp_escape(char *src, char *buf, size_t size);
extern char *ppp_safe_escape(char *src, char *buf, size_t size);
extern int start_pppd(int unit);
extern void stop_pppd(int unit);
extern int start_demand_ppp(int unit, int wait);
extern int start_pppoe_relay(char *wan_if);
extern void stop_pppoe_relay(void);

// vpnc.c
#ifdef RTCONFIG_VPNC
extern int vpnc_ipup_main(int argc, char **argv);
extern int vpnc_ipdown_main(int argc, char **argv);
extern int vpnc_ippreup_main(int argc, char **argv);
extern int vpnc_authfail_main(int argc, char **argv);
extern void update_vpnc_state(char *prefix, int state, int reason);
#endif

/*rc_ipsec.c*/
#ifdef RTCONFIG_IPSEC
extern void rc_ipsec_config_init();

#endif

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
extern int update_resolvconf(void);

/* qos.c */
extern void set_codel_patch(void);
extern void remove_codel_patch(void);
extern int start_iQos(void);
extern void stop_iQos(void);
extern void del_iQosRules(void);
extern int add_iQosRules(char *pcWANIF);
#ifdef CONFIG_BCMWL5
extern void del_EbtablesRules(void);
extern void add_EbtablesRules(void);
#endif
extern void ForceDisableWLan_bw(void);
extern int check_wl_guest_bw_enable();

/* rtstate.c */
extern void add_rc_support(char *feature);
extern void del_rc_support(char *feature);

// udhcpc.c
extern int udhcpc_wan(int argc, char **argv);
extern int udhcpc_lan(int argc, char **argv);
extern int start_udhcpc(char *wan_ifname, int unit, pid_t *ppid);
extern void stop_udhcpc(int unit);
extern int zcip_wan(int argc, char **argv);
extern int start_zcip(char *wan_ifname, int unit, pid_t *ppid);
extern void stop_zcip(int unit);

#ifdef RTCONFIG_IPV6
extern int dhcp6c_wan(int argc, char **argv);
extern int start_dhcp6c(void);
extern void stop_dhcp6c(void);
#ifdef RTCONFIG_6RELAYD
extern void stop_6relayd(void);
extern int start_6relayd(void);
#endif
#endif

#ifdef RTCONFIG_WPS
extern int wpsaide_main(int argc, char *argv[]);
#ifdef RTCONFIG_QCA
extern int get_wps_er_main(int argc, char *argv[]);
#endif
extern int stop_wpsaide();
extern int start_wpsaide();
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
#ifdef RTCONFIG_BCMARM
extern int mtd_erase_old(const char *mtdname);
extern int mtd_write_main_old(int argc, char *argv[]);
extern int mtd_unlock_erase_main_old(int argc, char *argv[]);
extern int mtd_write(const char *path, const char *mtd);
#else
extern int mtd_write_main(int argc, char *argv[]);
extern int mtd_unlock_erase_main(int argc, char *argv[]);
#endif

// jffs2.c
#if defined(RTCONFIG_UBIFS)
extern void start_ubifs(void);
extern void stop_ubifs(int stop);
static inline void start_jffs2(void) { start_ubifs(); }
static inline void stop_jffs2(int stop) { stop_ubifs(stop); }
#elif defined(RTCONFIG_JFFS2) || defined(RTCONFIG_JFFSV1) || defined(RTCONFIG_BRCM_NAND_JFFS2)
extern void start_jffs2(void);
extern void stop_jffs2(int stop);
#else
static inline void start_jffs2(void) { }
static inline void stop_jffs2(int stop) { }
#endif

// alert_mail.c
#ifdef RTCONFIG_PUSH_EMAIL
extern void alert_mail_service();
#endif

// watchdog.c
extern void led_control_normal(void);
extern void erase_nvram(void);
extern int init_toggle(void);
extern void btn_check(void);
extern int watchdog_main(int argc, char *argv[]);
extern int watchdog02_main(int argc, char *argv[]);
#ifdef SW_DEVLED
extern int sw_devled_main(int argc, char *argv[]);
#endif
extern int wdg_monitor_main(int argc, char *argv[]);
extern void init_wllc(void);
extern void rssi_check_unit(int unit);
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
extern void led_table_ctrl(int on_off);
#endif
extern void timecheck(void);

// usbled.c
extern int usbled_main(int argc, char *argv[]);
#ifdef RTCONFIG_FANCTRL
// phy_tempsense.c
extern int phy_tempsense_main(int argc, char *argv[]);
#endif
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
// psta_monitor.c
extern int psta_monitor_main(int argc, char *argv[]);
#endif

#ifdef RTCONFIG_QTN
extern int qtn_monitor_main(int argc, char *argv[]);
#endif

extern int radio_main(int argc, char *argv[]);

// ntp.c
extern int ntp_main(int argc, char *argv[]);

// btnsetup.c
extern int ots_main(int argc, char *argv[]);
extern void stop_ots(void);
extern int start_ots(void);
extern int rand_seed_by_time(void);

// common.c
extern char *conv_mac2(char *mac, char *buf);
extern void killall_tk_period_wait(const char *name, int wait);
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
extern int is_valid_char_for_volname(char c);
extern int is_valid_volname(const char *name);
extern void restart_lfp(void);
extern int get_meminfo_item(const char *name);
extern void setup_timezone(void);
extern int is_valid_hostname(const char *name);
extern int is_valid_domainname(const char *name);
extern void setup_ct_timeout(int connflag);
extern void setup_udp_timeout(int connflag);
extern void setup_ftp_conntrack(int port);
extern void setup_pt_conntrack(void);
extern void remove_conntrack(void);
extern int _pppstatus(const char *statusfile);
extern int pppstatus(int unit);
extern void time_zone_x_mapping(void);
extern void stop_if_misc(void);
extern int mssid_mac_validate(const char *macaddr);
#ifdef CONFIG_BCMWL5
extern int setup_dnsmq(int mode);
#endif

// usb.c
#ifdef RTCONFIG_USB
FILE* fopen_or_warn(const char *path, const char *mode);
extern void hotplug_usb(void);
extern void start_usb(void);
extern void remove_usb_module(void);
extern void stop_usb_program(int mode);
#ifndef RTCONFIG_ERPTEST
extern void stop_usb();
#else
extern void restart_usb(int f_stop);
extern void stop_usb(int f_force);
#endif
#ifdef RTCONFIG_USB_PRINTER
extern void start_lpd();
extern void stop_lpd();
extern void start_u2ec();
extern void stop_u2ec();
#endif
extern int ejusb_main(int argc, char *argv[]);
extern void webdav_account_default(void);
extern void remove_storage_main(int shutdn);
extern int start_usbled(void);
extern int stop_usbled(void);
extern void restart_nas_services(int stop, int start);
extern void stop_nas_services(int force);
extern int sd_partition_num();
#endif
#ifdef RTCONFIG_CROND
extern void start_cron(void);
extern void stop_cron(void);
#endif
extern void start_webdav(void);
#ifdef RTCONFIG_USB_PRINTER
extern void start_usblpsrv(void);
#endif
#ifdef RTCONFIG_SAMBASRV
extern void create_custom_passwd(void);
extern void stop_samba(void);
extern void start_samba(void);
#endif
#ifdef RTCONFIG_NFS
extern void start_nfsd(void);
extern void restart_nfsd(void);
extern void stop_nfsd(void);
#endif
#ifdef RTCONFIG_WEBDAV
extern void stop_webdav(void);
extern void stop_all_webdav(void);
#else
static inline void stop_all_webdav(void) { }
#endif
#ifdef RTCONFIG_FTP
extern void stop_ftpd(void);
extern void start_ftpd(void);
#endif
#ifdef RTCONFIG_CLOUDSYNC
extern void stop_cloudsync(int type);
extern void start_cloudsync(int fromUI);
#endif
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
extern int stop_app(void);
extern int start_app(void);
extern void usb_notify();
#endif

#ifdef RTCONFIG_DISK_MONITOR
extern void start_diskmon(void);
extern void stop_diskmon(void);
extern int diskmon_main(int argc, char *argv[]);
extern void remove_pool_error(const char *device, const char *flag);
#endif
extern int diskremove_main(int argc, char *argv[]);

#ifdef LINUX26
extern int start_sd_idle(void);
extern int stop_sd_idle(void);
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
extern void start_pptpd(void);
extern void stop_pptpd(void);

// vpnc.c
#ifdef RTCONFIG_VPNC
extern int start_vpnc(void);
extern void stop_vpnc(void);
#endif

#ifdef RTCONFIG_OPENVPN
extern void start_vpnclient(int clientNum);
extern void stop_vpnclient(int clientNum);
extern void start_vpnserver(int serverNum);
extern void stop_vpnserver(int serverNum);
extern void start_vpn_eas(void);
extern void stop_vpn_eas(void);
extern void run_vpn_firewall_scripts();
extern void write_vpn_dnsmasq_config(FILE*);
extern int write_vpn_resolv(FILE*);
extern void create_openvpn_passwd();
extern void update_ovpn_profie_remote();
extern int check_ovpn_server_enabled(int unit);
extern int check_ovpn_client_enabled(int unit);
extern void update_vpnrouting(int unit);
extern void reset_vpn_settings(int type, int unit);
extern void stop_vpn_all();
#endif

// wanduck.c
#if defined(RTCONFIG_FAILOVER_LED)
extern int update_failover_led(void);
#else
static inline int update_failover_led(void) { return 0; }
#endif
#if defined(RTCONFIG_LANWAN_LED)
extern int update_wan_leds(int wan_unit);
#else
static inline int update_wan_leds(int wan_unit) { update_failover_led(); return 0; }
#endif
extern int wanduck_main(int argc, char *argv[]);

// tcpcheck.c
extern int setupsocket(int sock);
extern void wakeme(int sig);
extern void goodconnect(int i);
extern void failedconnect(int i);
extern void subtime(struct timeval *a, struct timeval *b, struct timeval *res);
extern void setupset(fd_set *theset, int *numfds);
extern void waitforconnects();
extern int tcpcheck_main(int argc, char *argv[]);

// readmem.c
#ifdef BUILD_READMEM
extern int readmem_main(int argc, char *argv[]);
#endif

// usb_devices.c
#ifdef RTCONFIG_USB
extern int asus_sd(const char *device_name, const char *action);
extern int asus_lp(const char *device_name, const char *action);
extern int asus_sr(const char *device_name, const char *action);
extern int asus_tty(const char *device_name, const char *action);
extern int asus_usb_interface(const char *device_name, const char *action);
extern int asus_sg(const char *device_name, const char *action);
extern int asus_usbbcm(const char *device_name, const char *action);
#endif
#ifdef RTCONFIG_USB_MODEM
extern int is_create_file_dongle(const unsigned int vid, const unsigned int pid);
#ifdef RTCONFIG_USB_BECEEM
extern int is_beceem_dongle(const int mode, const unsigned int vid, const unsigned int pid);
extern int is_samsung_dongle(const int mode, const unsigned int vid, const unsigned int pid);
extern int is_gct_dongle(const int mode, const unsigned int vid, const unsigned int pid);
extern int write_beceem_conf(const char *eth_node);
extern int write_gct_conf(void);
#endif
extern int is_android_phone(const int mode, const unsigned int vid, const unsigned int pid);
extern int is_storage_cd(const int mode, const unsigned int vid, const unsigned int pid);
extern int write_3g_conf(FILE *fp, int dno, int aut, const unsigned int vid, const unsigned int pid);
extern int init_3g_param(const char *port_path, const unsigned int vid, const unsigned int pid);
extern int write_3g_ppp_conf(void);
#endif

#ifdef RTCONFIG_DSL
//dsl.c
extern void dsl_configure(int req);
extern void start_dsl(void);
extern void stop_dsl(void);
extern void remove_dsl_autodet(void);
extern void dsl_defaults(void);
#endif

//services.c
void start_Tor_proxy(void);
void stop_Tor_proxy(void);
extern void write_static_leases(char *file);
#ifdef RTCONFIG_DHCP_OVERRIDE
extern int restart_dnsmasq(int need_link_DownUp);
#endif
extern void start_dnsmasq(void);
extern void stop_dnsmasq(void);
extern void reload_dnsmasq(void);
extern int ddns_updated_main(int argc, char *argv[]);
#ifdef RTCONFIG_IPV6
extern void add_ip6_lanaddr(void);
extern void start_ipv6_tunnel(void);
extern void stop_ipv6_tunnel(void);
extern void start_ipv6(void);
extern void stop_ipv6(void);
extern void ipv6_sysconf(const char *ifname, const char *name, int value);
extern int ipv6_getconf(const char *ifname, const char *name);
#ifdef RTCONFIG_DUALWAN
extern void restart_dnsmasq_ipv6();
#endif
#endif
extern int wps_band_radio_off(int wps_band);
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_HSPOT
extern void stop_hspotap(void);
extern int start_hspotap(void);
#endif
extern void start_igmp_proxy(void);
extern void stop_igmp_proxy(void);
#if defined(BCM_BSD)
extern int start_bsd(void);
extern void stop_bsd(void);
#endif
#ifdef BCM_ASPMD
extern int start_aspmd(void);
extern int stop_aspmd(void);
#endif /* BCM_ASPMD */
extern int start_eapd(void);
extern void stop_eapd(void);
extern int start_nas(void);
extern void stop_nas(void);
#ifdef RTCONFIG_BCMWL6
extern void stop_acsd(void);
extern int start_acsd();
extern void set_acs_ifnames();
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
extern int stop_psta_monitor();
extern int start_psta_monitor();
#endif
#endif
void start_cstats(int new);
void restart_cstats(void);
void stop_cstats(void);
extern void setup_leds();
int ddns_custom_updated_main(int argc, char *argv[]);

// dnsfilter.c
#ifdef RTCONFIG_DNSFILTER
extern void dnsfilter_settings(FILE *fp, char *lan_ip);
extern void dnsfilter6_settings(FILE *fp, char *lan_if, char *lan_ip);
extern void dnsfilter_setup_dnsmasq(FILE *fp);
#endif

// lan.c
#ifdef RTCONFIG_TIMEMACHINE
extern int start_timemachine(void);
extern void stop_timemachine(void);
extern int start_afpd(void);
extern void stop_afpd(void);
extern int start_cnid_metad(void);
extern void stop_cnid_metad(void);
extern int start_avahi_daemon(void);
extern void stop_avahi_daemon(void);
#endif
#ifdef RTCONFIG_QUAGGA
extern void stop_quagga(void);
extern int start_quagga(void);
#endif
#if defined(RTCONFIG_MDNS)
extern int generate_mdns_config(void);
extern int generate_afpd_service_config(void);
extern int generate_adisk_service_config(void);
extern int generate_itune_service_config(void);
extern int start_mdns(void);
extern void stop_mdns(void);
extern void restart_mdns(void);
#endif
extern void start_snooper(void);
extern void stop_snooper(void);
#ifdef RTCONFIG_DUALWAN
extern int restart_dualwan(void);
#endif
#ifdef __CONFIG_NORTON__
extern int start_norton(void);
extern int stop_norton(void);
#endif

#ifdef RTCONFIG_MEDIA_SERVER
void force_stop_dms(void);
void stop_mt_daapd(void);
void start_dms(void);
void start_mt_daapd(void);
void set_invoke_later(int flag);
int get_invoke_later(void);
#endif	/* RTCONFIG_MEDIA_SERVER */

#ifdef RTCONFIG_WIRELESSREPEATER
void start_wlcscan(void);
void stop_wlcscan(void);
void start_wlcconnect(void);
void stop_wlcconnect(void);
void repeater_nat_setting(void);
void repeater_filter_setting(int mode);
#if defined(RTCONFIG_RALINK)
void apcli_start(void);
int site_survey_for_channel(int n, const char *wif, int *HT_EXT);
#endif
#endif	/* RTCONFIG_WIRELESSREPEATER */

#ifdef RTCONFIG_PARENTALCTRL
extern void stop_pc_block(void);
extern void start_pc_block(void);
#endif
#ifdef RTCONFIG_HTTPS
extern int check_rsasign(char *fname);
#endif

#if defined(RTCONFIG_CHILLISPOT) || defined(RTCONFIG_COOVACHILLI)
extern void chilli_addif2br0(const char *ifname);
extern void main_config(void);
extern void chilli_config(void);
extern void hotspotsys_config(void);
extern void stop_chilli(void);
extern void bridge_ifByP(char *ifs);
extern void Checkifnames(char *nvifnames, char *ifname);
extern void bridge_ifByA(char *ifs);
extern void DN2tmpfile(char *name);
extern void start_chilli(void);
#endif

#ifdef RTCONFIG_CAPTIVE_PORTAL
extern void set_captive_portal_wl(void);
#endif

//wireless.c
extern int wlcscan_main(void);
extern void repeater_pap_disable(void);
extern int wlcconnect_main(void);
#if defined(RTCONFIG_BLINK_LED)
extern void update_wifi_led_state_in_wlcmode(void);
#else
static inline void update_wifi_led_state_in_wlcmode(void) { }
#endif

//speedtest.c
extern int speedtest_main(int argc, char **argv);
extern int speedtest();
extern void wan_bandwidth_detect(void);

#ifdef RTCONFIG_BWDPI
extern int bwdpi_main(int argc, char **argv);
extern int bwdpi_check_main(int argc, char **argv);
extern int bwdpi_wred_alive_main(int argc, char **argv);
extern int show_wrs_main(int argc, char **argv);
extern int rsasign_sig_check_main(int argc, char *argv[]);
extern int bwdpi_db_10_main(int argc, char **argv);
extern void stop_bwdpi_check();
extern void start_bwdpi_check();
extern void stop_bwdpi_wred_alive();
extern void extract_data(char *path, FILE *fp);
extern int merge_log(char *path, int len);
extern void stop_dpi_engine_service(int forced);
extern void start_dpi_engine_service();
extern void setup_wrs_conf();
extern void auto_sig_check();
extern void sqlite_db_check();
extern int tm_qos_main(char *cmd);
extern int qosd_main(char *cmd);
extern int wrs_main(char *cmd);
extern int stat_main(char *mode, char *name, char *dura, char *date);
extern int clear_user_domain(void);
extern int web_history_main(char *MAC, char *page);
extern int wrs_app_main(char *cmd);
extern int set_cc(char *cmd);
extern int set_vp(char *cmd);
extern int get_vp(char *cmd);
extern int data_collect_main(char *cmd, char *path);
extern int device_main(char *MAC);
extern int device_info_main(char *MAC);
extern int wrs_url_main();
extern int rewrite_main(char *path1, char *path2, char *path3);
extern int check_filesize_main(char *path, char *size);
extern int extract_data_main(char *path);
extern int get_anomaly_main(char *cmd);
extern int get_app_patrol_main();
#endif

// tcode_rc.c
extern int config_tcode(int type);

// hour_monitor.c
extern int hour_monitor_main(int argc, char **argv);
extern int hour_monitor_function_check();
extern void check_hour_monitor_service();
extern void hm_traffic_analyzer_save();
extern void hm_traffic_limiter_save();

// traffic_limiter.c
#ifdef RTCONFIG_TRAFFIC_LIMITER
extern void traffic_limiter_sendSMS(const char *type, int unit);
extern void traffic_limiter_limitdata_check(void);
extern int traffic_limiter_wanduck_check(int unit);
extern void reset_traffic_limiter_counter(int force);
extern void init_traffic_limiter(void);
#endif


#ifdef RTCONFIG_INTERNAL_GOBI
extern int lteled_main(int argc, char **argv);
extern int start_lteled(void);
extern void stop_lteled(void);
#endif
#ifdef RTCONFIG_TOR
extern void start_Tor_proxy(void);
#endif
#ifdef RTCONFIG_CLOUDCHECK
void stop_cloudcheck(void);
void start_cloudcheck(void);
#endif

#ifdef RTCONFIG_IPERF
// monitor.c
extern int monitor_main(int argc, char *argv[]);
#endif

#ifdef RTCONFIG_TR069
extern int start_tr(void);
extern void stop_tr(void);
extern int dhcpc_lease_main(int argc, char *argv[]);
#endif

#if ((defined(RTCONFIG_USER_LOW_RSSI) && defined(RTCONFIG_BCMARM)) || defined(RTCONFIG_NEW_USER_LOW_RSSI))
extern void stop_roamast(void);
extern void start_roamast(void);
extern int roam_assistant_main(int argc, char *argv[]);
#endif

#ifdef RTCONFIG_DHCP_OVERRIDE
extern int detectWAN_arp_main(int argc, char **argv);
#endif

#ifdef RTCONFIG_PUSH_EMAIL
extern void am_send_mail(int type, char *path);
#endif

#if defined(RTCONFIG_KEY_GUARD)
extern void stop_keyguard(void);
extern void start_keyguard(void);
extern int keyguard_main(int argc, char *argv[]);	
void start_ecoguard(void)
#endif
extern void start_ecoguard(void);

#ifdef RTCONFIG_CAPTIVE_PORTAL
extern void start_uam_srv(void);
#endif

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
#ifdef RTCONFIG_DHDAP
extern int start_dhd_monitor(void);
extern int stop_dhd_monitor(void);
#endif
#endif
extern int start_nat_rules(void);
extern int stop_nat_rules(void);
extern void stop_syslogd(void);
extern void stop_klogd(void);
extern int start_syslogd(void);
extern int start_klogd(void);
extern int start_logger(void);
extern void start_dfs(void);
extern void handle_notifications(void);
extern void stop_watchdog(void);
extern void stop_watchdog02(void);
extern int restart_dualwan(void);
extern int start_watchdog(void);
#if defined(RTAC1200G) || defined(RTAC1200GP)
extern void stop_wdg_monitor(void);
extern int start_wdg_monitor(void);
#endif
#ifdef SW_DEVLED
extern void stop_sw_devled(void);
extern int start_sw_devled(void);
#endif
extern int get_apps_name(const char *string);
extern int run_app_script(const char *pkg_name, const char *pkg_action);
extern void set_hostname(void);
extern int _start_telnetd(int force);
extern int start_telnetd(void);
extern void stop_telnetd(void);
#ifdef RTCONFIG_SSH
extern int start_sshd(void);
extern void stop_sshd(void);
#endif
#ifdef RTCONFIG_WTFAST
extern void start_wtfast(void);
extern void stop_wtfast(void);
#endif
extern void start_hotplug2(void);
extern void stop_services(void);
extern void stop_services_mfg(void);
extern void stop_logger(void);
extern void setup_passwd(void);
extern void create_passwd(void);
extern int start_services(void);
extern void check_services(void);
extern int no_need_to_start_wps(void);
extern int wps_band_radio_off(int wps_band);
extern void check_wps_enable();
extern int start_wanduck(void);
extern void stop_wanduck(void);
extern void stop_ntpc(void);
extern int start_ntpc(void);
#ifdef RTCONFIG_UPNPC
extern int start_miniupnpc(void);
extern void stop_miniupnpc(void);
#endif
#ifdef RTCONFIG_BONJOUR
extern int start_netmonitor(void);
extern void stop_netmonitor(void);
#endif
extern void stop_networkmap(void);
extern int start_networkmap(int bootwait);
extern int stop_wps(void);
extern int start_wps(void);
extern void stop_upnp(void);
extern void start_upnp(void);
extern void reload_upnp(void);
extern void stop_ddns(void);
extern int start_ddns(void);
extern void refresh_ntpc(void);
extern void start_hotplug2(void);
extern void stop_hotplug2(void);
extern void stop_lltd(void);
extern void stop_httpd(void);
extern void stop_rstats(void);
extern void stop_autodet(void);
extern void start_autodet(void);
extern int wps_band_ssid_broadcast_off(int wps_band);
#ifdef RTCONFIG_QCA_PLC_UTILS
extern void start_plcdet(void);
#endif
extern void start_httpd(void);
extern int wl_wpsPincheck(char *pin_string);
extern int start_wps_pbc(int unit);
#if defined(RTCONFIG_RALINK)
extern int exec_8021x_start(int band, int is_iNIC);
extern int exec_8021x_stop(int band, int is_iNIC);
extern int start_8021x(void);
extern int stop_8021x(void);
#endif
#ifdef RTCONFIG_LLDP
extern void stop_lldpd(void);
extern int start_lldpd(void);
#endif
extern int firmware_check_main(int argc, char *argv[]);
#ifdef RTCONFIG_HTTPS
extern int rsasign_check_main(int argc, char *argv[]);
extern char *pwdec(const char *input);
extern char *pwdec_dsl(char *input);
#endif
extern int service_main(int argc, char *argv[]);
#ifdef RTCONFIG_DSL
extern int check_tc_upgrade(void);
extern int start_tc_upgrade(void);
#ifdef RTCONFIG_DSL_TCLINUX
extern void start_dsl_autodet(void);
extern void stop_dsl_autodet(void);
extern void stop_dsl_diag(void);
extern int start_dsl_diag(void);
#endif
#endif
#ifdef RTCONFIG_PUSH_EMAIL
extern void start_DSLsendmail(void);
#ifdef RTCONFIG_DSL_TCLINUX
extern void start_DSLsenddiagmail(void);
#endif
#endif
#ifdef RTCONFIG_SNMPD
extern void start_snmpd(void);
extern void stop_snmpd(void);
#endif
#ifdef RTCONFIG_TUNNEL
extern void start_mastiff();
extern void stop_mastiff();
#endif

#if defined(RTCONFIG_TCODE) && defined(CONFIG_BCMWL5)
extern char *cfe_nvram_get(const char *name);
static INLINE int
cfe_nvram_match(char *name, char *match) {
	const char *value = cfe_nvram_get(name);
	return (value && !strcmp(value, match));
}
extern char *cfe_nvram_safe_get(const char *name);
extern char *cfe_nvram_get_raw(const char *name);
extern char *cfe_nvram_safe_get_raw(const char *name);
extern int cfe_nvram_set(const char *name);
extern int refresh_cfe_nvram();
extern int factory_debug();
#if !(defined(RTCONFIG_CFEZ) && defined(RTCONFIG_BCMARM))
extern char *ATE_BRCM_PREFIX(void);
extern int ATE_BRCM_SET(const char *name, const char *value);
extern void ATE_BRCM_COMMIT(void);
#endif
#endif

#if defined(RTCONFIG_LACP)
void config_lacp(void);
#endif

#if defined(CONFIG_BCMWL5) && defined(RTCONFIG_DUALWAN)
int dualwan_control(int argc, char *argv[]);
#endif

#ifdef RTCONFIG_PORT_BASED_VLAN
// vlan.c
extern int vlan_enable(void);
extern int check_if_exist_vlan_ifnames(char *ifname);
extern void start_vlan_ifnames(void);
extern void stop_vlan_ifnames(void);
extern void start_vlan_wl_ifnames(void);
extern void restart_vlan_wl(void);
extern void vlan_lanaccess_wl(void);
extern void set_port_based_vlan_config(char *interface);
extern void set_vlan_ifnames(int index, int wlmap, char *subnet_name, char *vlan_if);
extern void stop_vlan_wl_ifnames(void);
extern void start_vlan_wl(void);
#endif

// timemachine.c
extern void find_backup_mac_date(char *mpname);
extern void write_timemachine_tokeninfo(char *mpname);

// erp_monitor.c
#if !(defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) || defined(RTCONFIG_REALTEK))
extern int erp_monitor_main(int argc, char **argv);
extern void stop_erp_monitor();
extern void start_erp_monitor();
#endif

#endif	/* __RC_H__ */
