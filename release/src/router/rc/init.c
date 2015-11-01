/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"

#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <errno.h>
#include <paths.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/klog.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#include <wlutils.h>
#include <bcmdevs.h>

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#include <shutils.h>

#include <shared.h>
#include <stdio.h>
#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#include <disk_share.h>
#endif

#ifdef RTCONFIG_BCMFA
#include <etioctl.h>

#define ARPHRD_ETHER		1	/* ARP Header */
#define CTF_FA_DISABLED		0
#define CTF_FA_BYPASS		1
#define CTF_FA_NORMAL		2
#define CTF_FA_SW_ACC		3
#define FA_ON(mode)		(mode == CTF_FA_BYPASS || mode == CTF_FA_NORMAL)

int fa_mode;
void fa_mode_init();
#endif
#ifdef RTCONFIG_GMAC3
static void gmac3_restore_defaults();
#endif

#define SHELL "/bin/sh"

static int fatalsigs[] = {
	SIGILL,
	SIGABRT,
	SIGFPE,
	SIGPIPE,
	SIGBUS,
	SIGSYS,
	SIGTRAP,
	SIGPWR
};

static int initsigs[] = {
	SIGHUP,
	SIGUSR1,
	SIGUSR2,
	SIGINT,
	SIGQUIT,
	SIGALRM,
	SIGTERM
};

static char *defenv[] = {
	"TERM=vt100",
	"HOME=/",
	//"PATH=/usr/bin:/bin:/usr/sbin:/sbin",
	"PATH=/opt/usr/bin:/opt/bin:/opt/usr/sbin:/opt/sbin:/usr/bin:/bin:/usr/sbin:/sbin",
	"SHELL=" SHELL,
	"USER=root",
#ifdef RTCONFIG_DMALLOC
/*
 * # dmalloc -DV
 * Debug Tokens:
 * none -- no functionality (0)
 * log-stats -- log general statistics (0x1)
 * log-non-free -- log non-freed pointers (0x2)
 * log-known -- log only known non-freed (0x4)
 * log-trans -- log memory transactions (0x8)
 * log-admin -- log administrative info (0x20)
 * log-bad-space -- dump space from bad pnt (0x100)
 * log-nonfree-space -- dump space from non-freed pointers (0x200)
 * log-elapsed-time -- log elapsed-time for allocated pointer (0x40000)
 * log-current-time -- log current-time for allocated pointer (0x80000)
 * check-fence -- check fence-post errors (0x400)
 * check-heap -- check heap adm structs (0x800)
 * check-blank -- check mem overwritten by alloc-blank, free-blank (0x2000)
 * check-funcs -- check functions (0x4000)
 * check-shutdown -- check heap on shutdown (0x8000)
 * catch-signals -- shutdown program on SIGHUP, SIGINT, SIGTERM (0x20000)
 * realloc-copy -- copy all re-allocations (0x100000)
 * free-blank -- overwrite freed memory with \0337 byte (0xdf) (0x200000)
 * error-abort -- abort immediately on error (0x400000)
 * alloc-blank -- overwrite allocated memory with \0332 byte (0xda) (0x800000)
 * print-messages -- write messages to stderr (0x2000000)
 * catch-null -- abort if no memory available (0x4000000)
 *
 * debug=0x4e48503 = log_stats, log-non-free, log-bad-space, log-elapsed-time, check-fence, check-shutdown, free-blank, error-abort, alloc-blank, catch-null
 * debug=0x3 = log-stats, log-non-free
 */
	"DMALLOC_OPTIONS=debug=0x3,inter=100,log=/jffs/dmalloc_%d.log",
#endif
	NULL
};

extern struct nvram_tuple router_defaults[];
extern struct nvram_tuple router_state_defaults[];

unsigned int num_of_mssid_support(unsigned int unit)
{
	char tmp_vifnames[] = "wlx_vifnames";
	char word[256], *next;
	int subunit;
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	if (nvram_match("wl_mssid", "0"))
		return 0;
#endif
	snprintf(tmp_vifnames, sizeof(tmp_vifnames), "wl%d_vifnames", unit);

	subunit = 0;
	foreach (word, nvram_safe_get(tmp_vifnames), next) {
		subunit++;
	}
	dbG("[mssid] support [%d] mssid\n", subunit);
	return subunit;
}

#ifdef RTCONFIG_WPS
static void
wps_restore_defaults(void)
{
	char macstr[128];
	int i;

	/* cleanly up nvram for WPS */
	nvram_unset("wps_config_state");
	nvram_unset("wps_proc_status");
	nvram_unset("wps_config_method");

	nvram_unset("wps_restart");
	nvram_unset("wps_proc_mac");

	nvram_unset("wps_sta_devname");
	nvram_unset("wps_sta_mac");

	nvram_unset("wps_pinfail");
	nvram_unset("wps_pinfail_mac");
	nvram_unset("wps_pinfail_name");
	nvram_unset("wps_pinfail_state");

	nvram_unset("wps_enr_ssid");
	nvram_unset("wps_enr_bssid");
	nvram_unset("wps_enr_wsec");

	nvram_set("wps_device_name", get_productid());
	nvram_set("wps_modelnum", get_productid());

#ifdef RTCONFIG_RGMII_BRCM5301X
	strcpy(macstr, nvram_safe_get("lan_hwaddr"));
#else
	strcpy(macstr, nvram_safe_get("et0macaddr"));
#endif
	if (strlen(macstr))
		for (i = 0; i < strlen(macstr); i++)
			macstr[i] = tolower(macstr[i]);
	nvram_set("boardnum", nvram_get("serial_no") ? : macstr);
}
#endif /* RTCONFIG_WPS */

static void
virtual_radio_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXX_mssid_";
	int i, j, len;

	nvram_unset("unbridged_ifnames");
	nvram_unset("ure_disable");

	/* Delete dynamically generated variables */
	for (i = 0; i < MAX_NVPARSE; i++) {
		sprintf(prefix, "wl%d_", i);
		nvram_unset(strcat_r(prefix, "vifs", tmp));
		nvram_unset(strcat_r(prefix, "ssid", tmp));
		nvram_unset(strcat_r(prefix, "guest", tmp));
		nvram_unset(strcat_r(prefix, "ure", tmp));
		nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
		nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
		nvram_unset(strcat_r(prefix, "wds", tmp));
		nvram_unset(strcat_r(prefix, "wds_timeout", tmp));
		sprintf(prefix, "lan%d_", i);
		nvram_unset(strcat_r(prefix, "ifname", tmp));
		nvram_unset(strcat_r(prefix, "ifnames", tmp));
		nvram_unset(strcat_r(prefix, "gateway", tmp));
		nvram_unset(strcat_r(prefix, "proto", tmp));
		nvram_unset(strcat_r(prefix, "ipaddr", tmp));
		nvram_unset(strcat_r(prefix, "netmask", tmp));
		nvram_unset(strcat_r(prefix, "lease", tmp));
		nvram_unset(strcat_r(prefix, "stp", tmp));
		nvram_unset(strcat_r(prefix, "hwaddr", tmp));
		sprintf(prefix, "dhcp%d_", i);
		nvram_unset(strcat_r(prefix, "start", tmp));
		nvram_unset(strcat_r(prefix, "end", tmp));

		/* clear virtual versions */
		for (j = 0; j < 16; j++) {
			sprintf(prefix, "wl%d.%d_", i, j);
			nvram_unset(strcat_r(prefix, "ssid", tmp));
			nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
			nvram_unset(strcat_r(prefix, "guest", tmp));
			nvram_unset(strcat_r(prefix, "closed", tmp));
			nvram_unset(strcat_r(prefix, "wpa_psk", tmp));
			nvram_unset(strcat_r(prefix, "auth", tmp));
			nvram_unset(strcat_r(prefix, "wep", tmp));
			nvram_unset(strcat_r(prefix, "auth_mode", tmp));
			nvram_unset(strcat_r(prefix, "crypto", tmp));
			nvram_unset(strcat_r(prefix, "akm", tmp));
			nvram_unset(strcat_r(prefix, "hwaddr", tmp));
			nvram_unset(strcat_r(prefix, "bss_enabled", tmp));
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			nvram_unset(strcat_r(prefix, "bss_maxassoc", tmp));
			nvram_unset(strcat_r(prefix, "wme_bss_disable", tmp));
#endif
			nvram_unset(strcat_r(prefix, "ifname", tmp));
			nvram_unset(strcat_r(prefix, "unit", tmp));
			nvram_unset(strcat_r(prefix, "ap_isolate", tmp));
			nvram_unset(strcat_r(prefix, "macmode", tmp));
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			nvram_unset(strcat_r(prefix, "maclist", tmp));
			nvram_unset(strcat_r(prefix, "maxassoc", tmp));
#endif
			nvram_unset(strcat_r(prefix, "mode", tmp));
			nvram_unset(strcat_r(prefix, "radio", tmp));
			nvram_unset(strcat_r(prefix, "radius_ipaddr", tmp));
			nvram_unset(strcat_r(prefix, "radius_port", tmp));
			nvram_unset(strcat_r(prefix, "radius_key", tmp));
			nvram_unset(strcat_r(prefix, "key", tmp));
			nvram_unset(strcat_r(prefix, "key1", tmp));
			nvram_unset(strcat_r(prefix, "key2", tmp));
			nvram_unset(strcat_r(prefix, "key3", tmp));
			nvram_unset(strcat_r(prefix, "key4", tmp));
			nvram_unset(strcat_r(prefix, "wpa_gtk_rekey", tmp));
			nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
			nvram_unset(strcat_r(prefix, "bridge", tmp));
			nvram_unset(strcat_r(prefix, "infra", tmp));
			nvram_unset(strcat_r(prefix, "net_reauth", tmp));
			nvram_unset(strcat_r(prefix, "pmk_cache", tmp));
			nvram_unset(strcat_r(prefix, "preauth", tmp));
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			nvram_unset(strcat_r(prefix, "sta_retry_time", tmp));
			nvram_unset(strcat_r(prefix, "wfi_enable", tmp));
			nvram_unset(strcat_r(prefix, "wfi_pinmode", tmp));
#endif
			nvram_unset(strcat_r(prefix, "wme", tmp));
			nvram_unset(strcat_r(prefix, "wmf_bss_enable", tmp));
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			nvram_unset(strcat_r(prefix, "wps_mode", tmp));
#endif
		}
	}

	/* Delete dynamically generated variables */
	for (i = 0; i < DEV_NUMIFS; i++) {
		sprintf(prefix, "wl%d_wds", i);
		len = strlen(prefix);
		for (j = 0; j < MAX_NVPARSE; j++) {
			sprintf(&prefix[len], "%d", j);
			nvram_unset(prefix);
		}
	}
}

void
misc_ioctrl(void)
{
#if defined(RTAC87U) || defined(RTAC3200) || defined(RTAC5300) || defined(RTAC88U) || defined(RTAC3100)
	/* default WAN_RED on  */
	char buf[16];

	snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") != 0){
		logmessage("DualWAN", "skip misc_ioctrl()");
		return;
	}
	if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
		led_control(LED_WAN, LED_ON);
		eval("et", "robowr", "0", "0x18", "0x01fe");
		eval("et", "robowr", "0", "0x1a", "0x01fe");
	}
#endif
}

/* assign none-exist value */
void
wl_defaults(void)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXX_", tmp[100], tmp2[100];
	char pprefix[]="wlXXXXXX_", *pssid;
	char word[256], *next;
	int unit, subunit;
	char wlx_vifnames[64], wl_vifnames[64], lan_ifnames[128];
	int subunit_x = 0;
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	int inic_vlan_id = INIC_VLAN_ID_START;
	char nic_lan_ifnames[64];
	char *pNic;
#endif
	unsigned int max_mssid;

	memset(wlx_vifnames, 0, sizeof(wlx_vifnames));
	memset(wl_vifnames, 0, sizeof(wl_vifnames));
	memset(lan_ifnames, 0, sizeof(lan_ifnames));
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	memset(nic_lan_ifnames, 0, sizeof(nic_lan_ifnames));
	pNic = nic_lan_ifnames;
#endif

	if (!nvram_get("wl_country_code"))
		nvram_set("wl_country_code", "");

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		for (t = router_defaults; t->name; t++) {
#ifdef CONFIG_BCMWL5
			if (!strncmp(t->name, "wl", 2) && strncmp(t->name, "wl_", 3) && strncmp(t->name, "wlc", 3) && !strcmp(&t->name[4], "nband"))
				nvram_set(t->name, t->value);
#endif
			if (strncmp(t->name, "wl_", 3)!=0) continue;
#ifdef CONFIG_BCMWL5
			if (!strcmp(&t->name[3], "nband") && nvram_match(strcat_r(prefix, &t->name[3], tmp), "-1"))
				nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
#endif
			if (!nvram_get(strcat_r(prefix, &t->name[3], tmp))) {
				/* Add special default value handle here */
#ifdef RTCONFIG_EMF
				/* Wireless IGMP Snooping */
				if (strncmp(&t->name[3], "igs", sizeof("igs")) == 0) {
					char *value = nvram_get(strcat_r(prefix, "wmf_bss_enable", tmp2));
					nvram_set(tmp, (value && *value) ? value : t->value);
				} else
#endif
				nvram_set(tmp, t->value);
			}
		}

		unit++;
	}

//	virtual_radio_restore_defaults();

	unit = 0;
	max_mssid = num_of_mssid_support(unit);
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		memset(wlx_vifnames, 0x0, 32);
		snprintf(pprefix, sizeof(pprefix), "wl%d_", unit);
		/* including primary ssid */
		for (subunit = 1; subunit < max_mssid+1; subunit++)
		{
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
#ifdef RTCONFIG_WIRELESSREPEATER
			if (nvram_get_int("sw_mode") == SW_MODE_REPEATER) {
				if (nvram_get_int("wlc_band")==unit && subunit==1) {
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
				}
				else {
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
				}
			}
#endif
			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				subunit_x++;
				if (strlen(wlx_vifnames))
					sprintf(wlx_vifnames, "%s %s", wlx_vifnames, get_wlifname(unit, subunit, subunit_x, tmp));
				else
					sprintf(wlx_vifnames, "%s", get_wlifname(unit, subunit, subunit_x, tmp));
			}
			else
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");

//			if (!nvram_get(strcat_r(prefix, "ifname", tmp)))
			{
				snprintf(tmp2, sizeof(tmp2), "wl%d.%d", unit, subunit);
//				nvram_set(strcat_r(prefix, "ifname", tmp), tmp2);
				nvram_set(strcat_r(prefix, "ifname", tmp), get_wlifname(unit, subunit, subunit_x, tmp2));
			}

			if (!nvram_get(strcat_r(prefix, "unit", tmp)))
			{
				snprintf(tmp2, sizeof(tmp2), "%d.%d", unit, subunit);
				nvram_set(strcat_r(prefix, "unit", tmp), tmp2);
			}

			if (!nvram_get(strcat_r(prefix, "ssid", tmp))) {
				pssid = nvram_default_get(strcat_r(pprefix, "ssid", tmp));
				if (strlen(pssid))
					sprintf(tmp2, "%s_Guest%d", pssid, subunit);
				else sprintf(tmp2, "ASUS_Guest%d", subunit);
				nvram_set(strcat_r(prefix, "ssid", tmp), tmp2);
			}

			if (!nvram_get(strcat_r(prefix, "auth_mode_x", tmp)))
			{
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "open");
				nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");
				nvram_set(strcat_r(prefix, "akm", tmp), "");
				nvram_set(strcat_r(prefix, "auth", tmp), "0");
				nvram_set(strcat_r(prefix, "wep_x", tmp), "0");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
#endif
				nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
				nvram_set(strcat_r(prefix, "key", tmp), "1");
				nvram_set(strcat_r(prefix, "key1", tmp), "");
				nvram_set(strcat_r(prefix, "key2", tmp), "");
				nvram_set(strcat_r(prefix, "key3", tmp), "");
				nvram_set(strcat_r(prefix, "key4", tmp), "");
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), "");

				nvram_set(strcat_r(prefix, "ap_isolate", tmp), "0");
				nvram_set(strcat_r(prefix, "bridge", tmp), "");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), "128");
#endif
				nvram_set(strcat_r(prefix, "closed", tmp), "0");
				nvram_set(strcat_r(prefix, "infra", tmp), "1");
				nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "maxassoc", tmp), "128");
#endif
				nvram_set(strcat_r(prefix, "mode", tmp), "ap");
				nvram_set(strcat_r(prefix, "net_reauth", tmp), "3600");
				nvram_set(strcat_r(prefix, "preauth", tmp), "");
				nvram_set(strcat_r(prefix, "radio", tmp), "1");
				nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), "");
				nvram_set(strcat_r(prefix, "radius_key", tmp), "");
				nvram_set(strcat_r(prefix, "radius_port", tmp), "1812");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "sta_retry_time", tmp), "5");
				nvram_set(strcat_r(prefix, "wfi_enable", tmp), "0");
				nvram_set(strcat_r(prefix, "wfi_pinmode", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wme", tmp), "auto");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), "0");
#endif
#ifdef RTCONFIG_EMF
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp),
					nvram_get(strcat_r(pprefix, "wmf_bss_enable", tmp)));
#else
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), "0");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");
#endif
				nvram_set(strcat_r(prefix, "expire", tmp), "0");
				nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");
			}
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
			if (nvram_get_int("sw_mode") == SW_MODE_ROUTER)		// Only limite access of Guest network in Router mode
			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				if (strcmp(word, "rai0") == 0)
				{
					int vlan_id;
					if (nvram_match(strcat_r(prefix, "lanaccess", tmp), "off"))
						vlan_id = inic_vlan_id++;	// vlan id for no LAN access
					else
						vlan_id = 0;			// vlan id allow LAN access
					if (vlan_id)
						pNic += sprintf(pNic, "vlan%d ", vlan_id);
				}
			}
#endif
		}

		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (strlen(wlx_vifnames))
		{
			nvram_set(strcat_r(prefix, "vifs", tmp), wlx_vifnames);
			if (strlen(wl_vifnames))
				sprintf(wl_vifnames, "%s %s", wl_vifnames, wlx_vifnames);
			else
				sprintf(wl_vifnames, "%s", wlx_vifnames);
		}
		else
			nvram_set(strcat_r(prefix, "vifs", tmp), "");

		unit++;
		subunit_x = 0;
	}

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	nvram_set("nic_lan_ifnames", nic_lan_ifnames); //reset value
	if (strlen(nic_lan_ifnames))
	{
		sprintf(wl_vifnames, "%s %s", wl_vifnames, nic_lan_ifnames);	//would be add to "lan_ifnames" in following code.
	}
#endif
	if (strlen(wl_vifnames))
	{
		strcpy(lan_ifnames, nvram_safe_get("lan_ifnames"));
		foreach (word, wl_vifnames, next)
			add_to_list(word, lan_ifnames, sizeof(lan_ifnames));

		nvram_set("lan_ifnames", lan_ifnames);
	}
}

/* for WPS Reset */
void
wl_default_wps(int unit)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXX_", tmp[100];

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	for (t = router_defaults; t->name; t++) {
		if (strncmp(t->name, "wl_", 3) &&
			strncmp(t->name, prefix, 4))
			continue;
		if (!strcmp(t->name, "wl_ifname"))	// not to clean wl%d_ifname
			continue;
		if (!strcmp(t->name, "wl_vifnames"))	// not to clean wl%d_vifnames for guest network
			continue;
		if (!strcmp(t->name, "wl_hwaddr"))	// not to clean wl%d_hwaddr
			continue;

		if (!strncmp(t->name, "wl_", 3))
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
		else
			nvram_set(t->name, t->value);
	}
}

void
restore_defaults_wifi(int all)
{
	char *macp = NULL;
	unsigned char mac_binary[6];
	unsigned char ssidbase[16];
	unsigned char ssid[32];
	char word[256], *next;
	int unit, subunit;
	unsigned int max_mssid;
	char prefix[]="wlXXXXXX_", tmp[100];

#if !defined(RTCONFIG_QCA) && !defined(RTCONFIG_RALINK)
#ifdef RTAC1200G
	if (strncmp(nvram_safe_get("territory_code"), "US", 2) && 
	    strncmp(nvram_safe_get("territory_code"), "CA", 2))
		return;
#else 
	if (strncmp(nvram_safe_get("territory_code"), "KR", 2))
		return;
#endif
#endif

	if (!strlen(nvram_safe_get("wifi_psk")))
		return;

	macp = get_lan_hwaddr();
	ether_atoe(macp, mac_binary);
	sprintf((char *)ssidbase, "%s_%02X", get_productid(), mac_binary[5]);

	unit = 0;
	max_mssid = num_of_mssid_support(unit);

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		sprintf((char *) ssid, "%s%s", ssidbase, unit ? (unit == 2 ? "_5G-2" : "_5G") : "_2G");
		nvram_set(strcat_r(prefix, "ssid", tmp), (char *) ssid);
		nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
		nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
		nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wifi_psk"));

		if (all)
		for (subunit = 1; subunit < max_mssid+1; subunit++) {
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

			sprintf((char *) ssid, "%s%s_Guest", ssidbase, unit ? (unit == 2 ? "_5G-2" : "_5G") : "_2G");
			if (subunit > 1)
				sprintf((char *) ssid, "%s%d", (char *) ssid, subunit);
			nvram_set(strcat_r(prefix, "ssid", tmp), (char *) ssid);
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
			nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wifi_psk"));
		}

		unit++;
	}

	nvram_set("w_Setting", "1");
}

void
wl_defaults_wps(void)
{
	char word[256], *next;
	int unit;

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		wl_default_wps(unit);
		unit++;
	}

#ifdef RTCONFIG_TCODE
	restore_defaults_wifi(0);
#endif
}

/* assign none-exist value or inconsist value */
void
lan_defaults(void)
{
	if (nvram_get_int("sw_mode")==SW_MODE_ROUTER && nvram_invmatch("lan_proto", "static"))
		nvram_set("lan_proto", "static");
}

/* assign none-exist value */
void
wan_defaults(void)
{
	struct nvram_tuple *t;
	char prefix[]="wanXXXXXX_", tmp[100];
	char word[256], *next;
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		nvram_set(strcat_r(prefix, "ifname", tmp), "");

		for(t = router_defaults; t->name; ++t){
			if(strncmp(t->name, "wan_", 4)!=0) continue;

			if(!nvram_get(strcat_r(prefix, &t->name[4], tmp))){
				//_dprintf("_set %s = %s\n", tmp, t->value);
				nvram_set(tmp, t->value);
			}
		}
	}

	unit = WAN_UNIT_FIRST;
	foreach(word, nvram_safe_get("wan_ifnames"), next){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(dualwan_unit__nonusbif(unit))
			nvram_set(strcat_r(prefix, "ifname", tmp), word);

		++unit;
	}
}

void
restore_defaults_module(char *prefix)
{
	struct nvram_tuple *t;

	if (strcmp(prefix, "wl_")==0) wl_defaults();
	else if (strcmp(prefix, "wan_")==0) wan_defaults();
	else {
		for (t = router_defaults; t->name; t++) {
			if (strncmp(t->name, prefix, sizeof(prefix))!=0) continue;
			nvram_set(t->name, t->value);
		}
	}
}

int restore_defaults_g = 0;

#ifdef RTCONFIG_USB
// increase this number each time need to reset usb related setting
// acc_num/acc_username/acc_password to acc_num/acc_list
#define USBCTRLVER 1

void usbctrl_default()
{
	char tmp[256], *ptr;
	int buf_len = 0;
	int usbctrlver;
	int acc_num, acc_num_ret, i;
	char *acc_user, *acc_password;
	char acc_nvram_username[32], acc_nvram_password[32];
	char **account_list;

	usbctrlver = nvram_get_int("usbctrlver");
	if (usbctrlver != USBCTRLVER || nvram_get("acc_username0") != NULL) {
		nvram_set_int("usbctrlver", USBCTRLVER);
		acc_num = nvram_get_int("acc_num");
		acc_num_ret = 0;

		char ascii_passwd[64];

		memset(ascii_passwd, 0, 64);
		char_to_ascii_safe(ascii_passwd, nvram_safe_get("http_passwd"), 64);
		buf_len = snprintf(tmp, sizeof(tmp), "%s>%s", nvram_safe_get("http_username"), ascii_passwd); // default account.
		ptr = tmp+buf_len;
		acc_num_ret = 1;

		for(i = 0; i < acc_num; i++) {
			sprintf(acc_nvram_username, "acc_username%d", i);
			if ((acc_user = nvram_get(acc_nvram_username)) == NULL) continue;

			sprintf(acc_nvram_password, "acc_password%d", i);
			if ((acc_password = nvram_get(acc_nvram_password)) == NULL) continue;
			memset(ascii_passwd, 0, 64);
			char_to_ascii_safe(ascii_passwd, acc_password, 64);

			if (strcmp(acc_user, "admin")) {
				buf_len += snprintf(ptr, 256-buf_len, "<%s>%s", acc_user, ascii_passwd);
				ptr = tmp+buf_len;
				acc_num_ret++;
			}

			nvram_unset(acc_nvram_username);
			nvram_unset(acc_nvram_password);
		}

		nvram_set_int("acc_num", acc_num_ret);
		nvram_set("acc_list", tmp);
	}

	acc_num = get_account_list(&acc_num, &account_list);
	if (acc_num >= 0 && acc_num != nvram_get_int("acc_num"))
		nvram_set_int("acc_num", acc_num);
	free_2_dimension_list(&acc_num, &account_list);
}
#endif

#define RESTORE_DEFAULTS() \
	(!nvram_match("restore_defaults", "0") || \
	 !nvram_match("nvramver", RTCONFIG_NVRAM_VER))	// nvram version mismatch

#ifdef RTCONFIG_USB_MODEM
void clean_modem_state(int flag){
	// Need to unset after the SIM is removed.
	// auto APN
	nvram_unset("usb_modem_auto_lines");
	nvram_unset("usb_modem_auto_running");
	nvram_unset("usb_modem_auto_imsi");
	nvram_unset("usb_modem_auto_country");
	nvram_unset("usb_modem_auto_isp");
	nvram_unset("usb_modem_auto_apn");
	nvram_unset("usb_modem_auto_spn");
	nvram_unset("usb_modem_auto_dialnum");
	nvram_unset("usb_modem_auto_user");
	nvram_unset("usb_modem_auto_pass");

	nvram_unset("usb_modem_act_signal");
	nvram_unset("usb_modem_act_operation");
	nvram_unset("usb_modem_act_imsi");
	nvram_unset("usb_modem_act_iccid");
	nvram_unset("usb_modem_act_auth");
	nvram_unset("usb_modem_act_auth_pin");
	nvram_unset("usb_modem_act_auth_puk");
	nvram_unset("modem_sim_order");
	nvram_unset("modem_bytes_rx");
	nvram_unset("modem_bytes_tx");
	nvram_unset("modem_bytes_rx_reset");
	nvram_unset("modem_bytes_tx_reset");

	nvram_unset("modem_sms_alert_send");
	nvram_unset("modem_sms_limit_send");

	if(flag == 2)
		return;

	if(flag){
		nvram_unset("usb_modem_act_path");
		nvram_unset("usb_modem_act_type");
		nvram_unset("usb_modem_act_dev");
	}

	// modem.
	nvram_unset("usb_modem_act_int");
	nvram_unset("usb_modem_act_bulk");
	nvram_unset("usb_modem_act_vid");
	nvram_unset("usb_modem_act_pid");
	nvram_unset("usb_modem_act_sim");
	nvram_unset("usb_modem_act_imei");
	nvram_unset("usb_modem_act_tx");
	nvram_unset("usb_modem_act_rx");
	nvram_unset("usb_modem_act_hwver");
	nvram_unset("usb_modem_act_swver");
	nvram_unset("usb_modem_act_band");
	nvram_unset("usb_modem_act_scanning");
	nvram_unset("usb_modem_act_startsec");
	nvram_unset("usb_modem_act_simdetect");

	// modem state.
	nvram_unset("g3state_pin");
	nvram_unset("g3state_z");
	nvram_unset("g3state_q0");
	nvram_unset("g3state_cd");
	nvram_unset("g3state_class");
	nvram_unset("g3state_mode");
	nvram_unset("g3state_apn");
	nvram_unset("g3state_dial");
	nvram_unset("g3state_conn");

	// modem error.
	nvram_unset("g3err_pin");
	nvram_unset("g3err_apn");
	nvram_unset("g3err_conn");
	nvram_unset("g3err_imsi");
}
#endif

void
misc_defaults(int restore_defaults)
{
#ifdef RTCONFIG_WEBDAV
	webdav_account_default();
#endif

	nvram_set("success_start_service", "0");

#if defined(RTAC66U) || defined(BCM4352)
	nvram_set("led_5g", "0");
#endif
#if !defined(RTCONFIG_HAS_5G)
	nvram_unset("wl1_ssid");
#endif	/* ! RTCONFIG_HAS_5G */

#ifdef RTCONFIG_USB
	if (nvram_match("smbd_cpage", ""))
	{
		if (nvram_match("wl0_country_code", "CN"))
			nvram_set("smbd_cpage", "936");
		else if (nvram_match("wl0_country_code", "TW"))
			nvram_set("smbd_cpage", "950");
	}
#endif
	/* reset ntp status */
	nvram_set("svc_ready", "0");
#ifdef RTCONFIG_QTN
	nvram_unset("qtn_ready");
#endif
	nvram_set("mfp_ip_requeue", "");
#ifdef RTAC68U
	nvram_set_int("auto_upgrade", 0);
	nvram_unset("fw_check_period");
#endif

	if (restore_defaults)
	{
		nvram_set("jffs2_clean_fs", "1");
#ifdef RTCONFIG_QTN
		nvram_set("qtn_restore_defaults", "1");
#endif
	}
#ifdef RTCONFIG_QTN
	else nvram_unset("qtn_restore_defaults");
#endif

#ifdef WEB_REDIRECT
	nvram_set("freeze_duck", "0");
#endif
	nvram_unset("ateCommand_flag");
}

/* ASUS use erase nvram to reset default only */
static void
restore_defaults(void)
{
	struct nvram_tuple *t;
	int restore_defaults;
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	int unit;
	int i;
	FILE *fp;

	nvram_unset(ASUS_STOP_COMMIT);
	nvram_unset(LED_CTRL_HIPRIO);

	// Restore defaults if nvram version mismatch
	restore_defaults = RESTORE_DEFAULTS();

	/* Restore defaults if told to or OS has changed */
	if (!restore_defaults)
	{
		restore_defaults = !nvram_match("restore_defaults", "0");
#if defined(RTN56U)
		/* upgrade from firmware 1.x.x.x */
		if (nvram_get("HT_AutoBA") != NULL)
		{
			nvram_unset("HT_AutoBA");
			restore_defaults = 1;
		}
#endif
	}

	if (restore_defaults) {
		fprintf(stderr, "\n## Restoring defaults... ##\n");
//		logmessage(LOGNAME, "Restoring defaults...");	// no use
	}

	restore_defaults_g = restore_defaults;

	if (restore_defaults) {
#ifdef RTCONFIG_WPS
		wps_restore_defaults();
#endif
		virtual_radio_restore_defaults();
	}

#ifdef RTCONFIG_USB
	usbctrl_default();
#endif

	/* Restore defaults */
	for (t = router_defaults; t->name; t++) {
		if (restore_defaults || !nvram_get(t->name)) {
#if 0
			// add special default value handle here
			if (!strcmp(t->name, "computer_name") ||
				!strcmp(t->name, "dms_friendly_name") ||
				!strcmp(t->name, "daapd_friendly_name"))
				nvram_set(t->name, get_productid());
			else if (strcmp(t->name, "ct_max")==0) {
				// handled in init_nvram already
			}
			else
#endif
			nvram_set(t->name, t->value);
		}
	}

	nvram_set_int("wlready", 0);
	nvram_set_int("init_wl_re", 0);
	wl_defaults();
	lan_defaults();
	wan_defaults();
#ifdef RTCONFIG_DSL
	dsl_defaults();
#endif
#ifdef RTAC3200
	bsd_defaults();
#endif
#ifdef RTCONFIG_DHDAP
	for (i = 0; i < MAX_NVPARSE; i++) {
		snprintf(tmp, sizeof(tmp), "wl%d_cfg_maxassoc", i);
		nvram_unset(tmp);
	}
#endif

	if (restore_defaults) {
		uint memsize = 0;
		char pktq_thresh[8] = {0};
		char et_pktq_thresh[8] = {0};

		memsize = get_meminfo_item("MemTotal");

		if (memsize <= 32768) {
			/* Set to 512 as long as onboard memory <= 32M */
			sprintf(pktq_thresh, "512");
			sprintf(et_pktq_thresh, "512");
		}
		else if (memsize <= 65536) {
			/* We still have to set the thresh to prevent oom killer */
			sprintf(pktq_thresh, "1024");
			sprintf(et_pktq_thresh, "1536");
		}
		else {
			sprintf(pktq_thresh, "1024");
			/* Adjust the et thresh to 3300 as long as memory > 64M.
			 * The thresh value is based on benchmark test.
			 */
			sprintf(et_pktq_thresh, "3300");
		}

		nvram_set("wl_txq_thresh", pktq_thresh);
		nvram_set("et_txq_thresh", et_pktq_thresh);
#if defined(__CONFIG_USBAP__)
		nvram_set("wl_rpcq_rxthresh", pktq_thresh);
#endif
#ifdef RTCONFIG_BCMARM
#ifdef RTCONFIG_USB_XHCI
		if ((nvram_get_int("wlopmode") == 7) || factory_debug())
			nvram_set("usb_usb3", "1");
#endif
#endif
#ifdef RTCONFIG_GMAC3
		/* Delete dynamically generated variables */
		gmac3_restore_defaults();
#endif
#ifdef RTCONFIG_TCODE
		restore_defaults_wifi(1);
#endif
	}

	/* Commit values */
	if (restore_defaults) {
		nvram_commit();
		fprintf(stderr, "done\n");
	}

	if (!nvram_match("extendno_org", nvram_safe_get("extendno")))
		nvram_commit();

	if (nvram_get("wl_TxPower")) {
#ifdef RTCONFIG_RALINK
		nvram_set_int("wl0_txpower", nvram_get_int("wl0_TxPower"));
		nvram_set_int("wl1_txpower", nvram_get_int("wl1_TxPower"));
#else
		nvram_set_int("wl0_txpower", min(100 * nvram_get_int("wl0_TxPower") / 80, 100));
		nvram_set_int("wl1_txpower", min(100 * nvram_get_int("wl1_TxPower") / 80, 100));
#endif
		nvram_unset("wl_TxPower");
		nvram_unset("wl0_TxPower");
		nvram_unset("wl1_TxPower");

		nvram_commit();
	}

	/* default for state control variables */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		for (t = router_state_defaults; t->name; t++) {
			if (!strncmp(t->name, "wan_", 4))
				nvram_set(strcat_r(prefix, &t->name[4], tmp), t->value);
			else if (unit == WAN_UNIT_FIRST) // let non-wan nvram to be set one time.
				nvram_set(t->name, t->value);
		}
	}
	nvram_set_int("link_internet", 0);

	// default for USB state control variables. {
#ifdef RTCONFIG_USB
	for(i = 1; i <= MAX_USB_PORT; ++i) { // MAX USB port number is 3.
		snprintf(prefix, sizeof(prefix), "usb_led%d", i);
		nvram_unset(prefix);
	}

	/* Unset all usb_path related nvram variables, e.g. usb_path1, usb_path2.4.3, usb_path_. */
	if ((fp = popen("nvram show|grep \"^usb_path[_1-9]\"|grep -v \"_diskmon\"", "r")) != NULL) {
		char *p, var[256];

		var[0] = '\0';
		while (fgets(var, sizeof(var), fp)) {
			if (strncmp(var, "usb_path", 8))
				continue;

			if ((p = strchr(var, '=')) != NULL)
				*p = '\0';

			nvram_unset(var);
		}
		fclose(fp);
	}

#ifdef RTCONFIG_USB_MODEM
#ifndef RT4GAC55U
	// can't support all kinds of modem.
	nvram_set("modem_mode", "0");
#endif

	clean_modem_state(1);
	nvram_unset("usb_modem_act_reset"); // only be unset at boot.
	nvram_unset("usb_modem_act_reset_path"); // only be unset at boot.

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	int sim_num = atoi(nvram_safe_get("modem_sim_num"));
	for(i = 1; i <= sim_num; ++i){
		snprintf(prefix, sizeof(prefix), "modem_sim_imsi%d", i);
		nvram_unset(prefix);
	}
#endif
#endif
#endif	/* RTCONFIG_USB */
	// default for USB state control variables. }

	/* some default state values is model deps, so handled here*/
	switch(get_model()) {
#ifdef RTCONFIG_BCMARM
#ifdef RTAC56U
		case MODEL_RTAC56U:
			if(chk_ate_ccode())
				fix_ate_ccode();
#endif
		case MODEL_RTAC56S:
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.2.4")))	// since 1.0.2.5
				nvram_set("reboot_time", "140");// default is 70 sec

			if (!After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.2.6"))) {	// before 1.0.2.7
				nvram_set("wl0_itxbf",	"0");
				nvram_set("wl1_itxbf",	"0");
				nvram_set("wl_itxbf",	"0");
			}

			break;
		
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
			nvram_set("reboot_time", "140");
			break;
		case MODEL_RTAC3200:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
			nvram_set("reboot_time", "80");

			break;
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
#ifdef RTCONFIG_DUAL_TRX
			nvram_set("reboot_time", "90");		// default is 70 sec
#else
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.1.6")))	// since 1.0.1.7
				nvram_set("reboot_time", "140");// default is 70 sec
			else
				nvram_set("reboot_time", "80");	// extend default to 80
#endif
			break;
		case MODEL_RTAC87U:
			nvram_set("reboot_time", "160");
			break;
		case MODEL_RTN18U:
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("2.0.0.5")))
				nvram_set("reboot_time", "140");// default is 70 sec
			break;
		case MODEL_DSLAC68U:
			nvram_set("reboot_time", "140");	// default is 70 sec
			break;
#endif
		case MODEL_RTN14UHP:
			nvram_set("reboot_time", "85");		// default is 70 sec
			break;
		case MODEL_RTN10U:
			nvram_set("reboot_time", "85");		// default is 70 sec
			break;
#ifdef RTCONFIG_RALINK
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
			nvram_set("reboot_time", "75");		// default is 70 sec
			break;
#else
		case MODEL_RTAC52U:
			nvram_set("reboot_time", "80");		// default is 70 sec
			break;
		case MODEL_RTN56UB1:
		case MODEL_RTN54U:
		case MODEL_RTAC54U:
		case MODEL_RTAC1200HP:
		case MODEL_RTAC51U:
		case MODEL_RTN65U:
			nvram_set("reboot_time", "90");		// default is 70 sec
			break;
#endif
#endif
#ifdef RTCONFIG_QCA
		case MODEL_RT4GAC55U:
		case MODEL_RTAC55U:
		case MODEL_RTAC55UHP:
		case MODEL_PLN12:
		case MODEL_PLAC56:
		case MODEL_PLAC66U:
			nvram_set("reboot_time", "80"); // default is 70 sec
			break;
#endif
		default:
			break;
	}

	misc_defaults(restore_defaults);
}

/* Set terminal settings to reasonable defaults */
static void set_term(int fd)
{
	struct termios tty;

	tcgetattr(fd, &tty);

	/* set control chars */
	tty.c_cc[VINTR]  = 3;	/* C-c */
	tty.c_cc[VQUIT]  = 28;	/* C-\ */
	tty.c_cc[VERASE] = 127; /* C-? */
	tty.c_cc[VKILL]  = 21;	/* C-u */
	tty.c_cc[VEOF]   = 4;	/* C-d */
	tty.c_cc[VSTART] = 17;	/* C-q */
	tty.c_cc[VSTOP]  = 19;	/* C-s */
	tty.c_cc[VSUSP]  = 26;	/* C-z */

	/* use line dicipline 0 */
	tty.c_line = 0;

	/* Make it be sane */
	tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
	tty.c_cflag |= CREAD|HUPCL|CLOCAL;


	/* input modes */
	tty.c_iflag = ICRNL | IXON | IXOFF;

	/* output modes */
	tty.c_oflag = OPOST | ONLCR;

	/* local modes */
	tty.c_lflag =
		ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;

	tcsetattr(fd, TCSANOW, &tty);
}

static int console_init(void)
{
	int fd;

	/* Clean up */
	ioctl(0, TIOCNOTTY, 0);
	close(0);
	close(1);
	close(2);
	setsid();

	/* Reopen console */
	if ((fd = open(_PATH_CONSOLE, O_RDWR)) < 0) {
		/* Avoid debug messages is redirected to socket packet if no exist a UART chip, added by honor, 2003-12-04 */
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_WRONLY);
		open("/dev/null", O_WRONLY);
		perror(_PATH_CONSOLE);
		return errno;
	}
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	ioctl(0, TIOCSCTTY, 1);
	tcsetpgrp(0, getpgrp());
	set_term(0);

	return 0;
}

static pid_t run_shell(int timeout, int nowait)
{
	pid_t pid;
	int sig;

	/* Wait for user input */
	if (waitfor(STDIN_FILENO, timeout) <= 0) return 0;

	switch (pid = fork()) {
	case -1:
		perror("fork");
		return 0;
	case 0:
		/* Reset signal handlers set for parent process */
		for (sig = 0; sig < (_NSIG-1); sig++)
			signal(sig, SIG_DFL);

		/* Reopen console */
		console_init();

		/* Now run it.  The new program will take over this PID,
		 * so nothing further in init.c should be run. */
		execve(SHELL, (char *[]) { SHELL, NULL }, defenv);

		/* We're still here?  Some error happened. */
		perror(SHELL);
		exit(errno);
	default:
		if (nowait) {
			return pid;
		}
		else {
			waitpid(pid, NULL, 0);
			return 0;
		}
	}
}

int console_main(int argc, char *argv[])
{
	for (;;) run_shell(0, 0);

	return 0;
}

static void shutdn(int rb)
{
	int i;
	int act;
	sigset_t ss;

	_dprintf("shutdn rb=%d\n", rb);

	sigemptyset(&ss);
	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++)
		sigaddset(&ss, fatalsigs[i]);
	for (i = 0; i < sizeof(initsigs) / sizeof(initsigs[0]); i++)
		sigaddset(&ss, initsigs[i]);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	for (i = 30; i > 0; --i) {
		if (((act = check_action()) == ACT_IDLE) ||
		     (act == ACT_REBOOT) || (act == ACT_UNKNOWN)) break;
		_dprintf("Busy with %d. Waiting before shutdown... %d\n", act, i);
		sleep(1);
	}
	set_action(ACT_REBOOT);

	stop_wan();

	_dprintf("TERM\n");
	kill(-1, SIGTERM);
	sleep(3);
	sync();

	_dprintf("KILL\n");
	kill(-1, SIGKILL);
	sleep(1);
	sync();

	// TODO LED Status for LED
	setAllLedOff();

	reboot(rb ? RB_AUTOBOOT : RB_HALT_SYSTEM);

	do {
		sleep(1);
	} while (1);
}

static void handle_fatalsigs(int sig)
{
	_dprintf("fatal sig=%d\n", sig);
	shutdn(-1);
}

/* Fixed the race condition & incorrect code by using sigwait()
 * instead of pause(). But SIGCHLD is a problem, since other
 * code: 1) messes with it and 2) depends on CHLD being caught so
 * that the pid gets immediately reaped instead of left a zombie.
 * Pidof still shows the pid, even though it's in zombie state.
 * So this SIGCHLD handler reaps and then signals the mainline by
 * raising ALRM.
 */
static void handle_reap(int sig)
{
	chld_reap(sig);
	raise(SIGALRM);
}


#ifdef RTCONFIG_SWMODE_SWITCH
init_swmode()
{
	if (!nvram_get_int("swmode_switch")) return;

	if (button_pressed(BTN_SWMODE_SW_REPEATER)) {
		nvram_set_int("sw_mode", SW_MODE_REPEATER);
		dbg("%s: swmode: repeater", LOGNAME);
		if (nvram_match("x_Setting", "0")) {
			nvram_set("lan_proto", "dhcp");
		}
	}else if (button_pressed(BTN_SWMODE_SW_AP)) {
		nvram_set_int("sw_mode", SW_MODE_AP);
		dbg("%s: swmode: ap", LOGNAME);
		if (nvram_match("x_Setting", "0")) {
			nvram_set("lan_proto", "dhcp");
		}
	}else if (button_pressed(BTN_SWMODE_SW_ROUTER)) {
		dbg("%s: swmode: router", LOGNAME);
		nvram_set_int("sw_mode", SW_MODE_ROUTER);
		nvram_set("lan_proto", "static");
	}else{
		dbg("%s: swmode: unknow swmode", LOGNAME);
	}
}
#endif

#if 0
void conf_swmode_support(int model)
{
	switch (model) {
		case MODEL_RTAC66U:
		case MODEL_RTAC53U:
			nvram_set("swmode_support", "router repeater ap psta");
			dbg("%s: swmode: router repeater ap psta\n", LOGNAME);
			break;
		case MODEL_APN12HP:
			nvram_set("swmode_support", "repeater ap");
			dbg("%s: swmode: repeater ap\n", LOGNAME);
			break;
		default:
			nvram_set("swmode_support", "router repeater ap");
			dbg("%s: swmode: router repeater ap\n", LOGNAME);
			break;
	}
}
#endif
char *the_wan_phy()
{
#ifdef RTCONFIG_BCMFA
	if (FA_ON(fa_mode))
		return "vlan2";
	else
#endif
		return "eth0";
}

/**
 * Generic wan_ifnames / lan_ifnames / wl_ifnames / wl[01]_vifnames initialize helper
 * @wan:	WAN interface.
 * @lan:	LAN interface.
 * @wl2g:	2.4G interface.
 * @wl5g:	5G interface, can be NULL.
 * @usb:	USB interface, can be NULL.
 * @ap_lan:	LAN interface in AP mode.
 * 		If model-specific code don't need to bridge WAN and LAN interface in AP mode,
 * 		customize interface name of LAN interface in this parameter.
 * 		e.g. RT-N65U:
 * 		Home gateway mode:	WAN = vlan2, LAN = vlan1.
 * 		AP mode:		WAN = N/A,   LAN = vlan1, switch is reconfigured by the "rtkswitch 8 100" command.
 * @dw_wan:	WAN interface in dual-wan mode.
 * 		e.g. RT-AC68U:
 * 		Default WAN interface of RT-AC68U is eth0.
 * 		If one of LAN ports is configured as a WAN port and WAN port is enabled,
 * 		WAN interface becomes "vlanX" (X = nvram_get("switch_wan0tagid")) or "vlan2".
 * @dw_lan:	LAN interface of LAN port that is used as a WAN interface in dual-wan mode
 * 		In most cases, it should be "vlan3".
 * 		e.g. RT-AC68U (1st WAN/2nd WAN):
 * 		WAN/none, WAN/usb, usb/WAN:
 * 			WAN:
 * 				(1) vlanX (X = nvram_get("switch_wan0tagid")), or
 * 				(2) the_wan_phy() = eth0 (X doesn't exist)
 *
 * 		LAN/none, LAN/usb, usb/LAN:
 * 			LAN:
 * 				the_wan_phy() = eth0
 *
 * 		WAN/LAN, LAN/WAN:
 * 			WAN:
 * 				(1) vlanX (X = nvram_get("switch_wan0tagid")), or
 * 				(2) vlan2
 * 			LAN:
 * 				vlan3
 *
 * 		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
 * @force_dwlan:If true, always choose dw_lan as LAN interface that is used as WAN, even WAN/LAN are enabled both.
 *
 * @return:
 * 	-1:	invalid parameter.
 *	0:	success
 */
#if !defined(CONFIG_BCMWL5) && !defined(RTN56U)
static int set_basic_ifname_vars(char *wan, char *lan, char *wl2g, char *wl5g, char *usb, char *ap_lan, char *dw_wan, char *dw_lan, int force_dwlan)
{
	int sw_mode = nvram_get_int("sw_mode");
	char buf[128];
#if defined(RTCONFIG_DUALWAN)
	int unit, type;
	int enable_dw_wan = 0;
#endif
	if (!wan || *wan == '\0' || !lan || *lan == '\0' || !wl2g || *wl2g == '\0'
	    || (sw_mode != SW_MODE_AP && sw_mode != SW_MODE_REPEATER && (!ap_lan || *ap_lan == '\0')))
	{
		_dprintf("%s: invalid parameter. (wan %p, lan %p, wl2g %p, ap_lan %p, sw_mode %d)\n",
			__func__, wan, lan, wl2g, ap_lan, sw_mode);
		return -1;
	}
	if (wl5g && *wl5g == '\0')
		wl5g = NULL;
	if (usb && *usb == '\0')
		usb = "usb";
	if (ap_lan && *ap_lan == '\0')
		ap_lan = NULL;

	/* wl[01]_vifnames are virtual id only for ui control */
	if (wl5g) {
		sprintf(buf, "%s %s", wl2g, wl5g);
		nvram_set("wl_ifnames", buf);

		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
	} else {
		nvram_set("wl_ifnames", wl2g);
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
	}

	switch (sw_mode) {
	case SW_MODE_AP:
		if (!ap_lan) {
			sprintf(buf, "%s %s %s", lan, wan, wl2g);
			set_lan_phy(buf);
			if (wl5g)
				add_lan_phy(wl5g);
		} else {
			sprintf(buf, "%s %s", ap_lan, wl2g);
			set_lan_phy(buf);
			if (wl5g)
				add_lan_phy(wl5g);
		}
		set_wan_phy("");
		break;

#if defined(RTCONFIG_WIRELESSREPEATER)
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	case SW_MODE_REPEATER:
#if defined(RTCONFIG_PROXYSTA)
		if (nvram_get_int("wlc_psta") == 1)
		{
			/* media bridge mode */
			if (!ap_lan) {
				sprintf(buf, "%s %s %s", lan, wan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
			} else {
				sprintf(buf, "%s %s", ap_lan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
			}
			set_wan_phy("");
		}
		else
#endif
		{
			/* repeater mode */
			if (!ap_lan) {
				sprintf(buf, "%s %s", lan, wl2g);	/* ignore wan interface */
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
			} else {
				sprintf(buf, "%s %s", ap_lan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
			}
			set_wan_phy("");
		}
		break;
#elif defined(RTCONFIG_QTN)
		/* FIXME */
#else
		/* Broadcom platform */
		/* FIXME */
#endif	/* RTCONFIG_RALINK || RTCONFIG_QCA */
#endif	/* RTCONFIG_WIRELESSREPEATER */

	default:
		/* router/default */
		set_lan_phy(lan);
#if defined(RTCONFIG_DUALWAN)
		if (!dw_wan || *dw_wan == '\0')
			dw_wan = wan;
		if (!dw_lan || *dw_lan == '\0')
			dw_lan = lan;

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
		/* wandevs is not necessary on Ralink/MTK platform. */
#else
		if (!force_dwlan) {
			if (get_wans_dualwan() & WANSCAP_WAN && get_wans_dualwan() & WANSCAP_LAN) {
				sprintf(buf, "%s %s", dw_wan, dw_lan);
				nvram_set("wandevs", buf);
			} else {
				nvram_set("wandevs", "et0");	/* FIXME: et0 or eth0 */
			}
		}
#endif

		if (!(get_wans_dualwan() & WANSCAP_2G))
			add_lan_phy(wl2g);
		if (wl5g && !(get_wans_dualwan() & WANSCAP_5G))
			add_lan_phy(wl5g);

		set_wan_phy("");
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			char *wphy = NULL;

			type = get_dualwan_by_unit(unit);
			switch (type) {
			case WANS_DUALWAN_IF_LAN:
				if (force_dwlan || (get_wans_dualwan() & WANSCAP_WAN)
#if defined(RTCONFIG_RALINK)
				    || (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))
#endif
				   )
				{
					wphy = dw_lan;
				} else
					wphy = wan;	/* NOTICE */
				break;
			case WANS_DUALWAN_IF_2G:
				wphy = wl2g;
				break;
			case WANS_DUALWAN_IF_5G:
				if (wl5g)
					wphy = wl5g;
				break;
			case WANS_DUALWAN_IF_WAN:
#if (defined(RTCONFIG_RALINK) && !defined(RTCONFIG_RALINK_MT7620) && !defined(RTCONFIG_RALINK_MT7621))
#else
				/* Broadcom, QCA, MTK (use MT7620/MT7621 ESW) platform */
				if (nvram_get("switch_wantag")
				    && !nvram_match("switch_wantag", "")
				    && !nvram_match("switch_wantag", "none"))
				{
					sprintf(buf, "vlan%s", nvram_safe_get("switch_wan0tagid"));
					wphy = buf;
				}
				else
#endif
				if (get_wans_dualwan() & WANSCAP_LAN) {
					wphy = dw_wan;
#if defined(RTCONFIG_DUALWAN)
					enable_dw_wan = 1;
#endif
				} else
					wphy = wan;
				break;
			case WANS_DUALWAN_IF_USB:
				wphy = usb;
				break;
			case WANS_DUALWAN_IF_NONE:
				break;
			default:
				_dprintf("%s: Unknown DUALWAN type %d\n", __func__, type);
			}

			if (wphy)
				add_wan_phy(wphy);
		}
#else
		add_lan_phy(wl2g);
		if (wl5g)
			add_lan_phy(wl5g);
		set_wan_phy(wan);
#endif
	}
#if (defined(RTCONFIG_DUALWAN) && defined(RTCONFIG_QCA))
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
#else /* RT-AC55U || 4G-AC55U */
	if(enable_dw_wan) {
		nvram_set("vlan2hwname", "et0");
	}
	else {
		nvram_unset("vlan2hwname");
	}
#endif
#endif

	_dprintf("%s: WAN %s LAN %s 2G %s 5G %s USB %s AP_LAN %s "
		"DW_WAN %s DW_LAN %s force_dwlan %d, sw_mode %d\n",
		__func__, wan, lan, wl2g, wl5g? wl5g:"N/A", usb,
		ap_lan? ap_lan:"N/A", dw_wan, dw_lan, force_dwlan, sw_mode);
	_dprintf("wan_ifnames: %s\n", nvram_safe_get("wan_ifnames"));
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else
	_dprintf("wandevs: %s\n", nvram_safe_get("wandevs"));
#endif

	return 0;
}
#endif

// intialized in this area
//  lan_ifnames
//  wan_ifnames
//  wl_ifnames
//  btn_xxxx_gpio
//  led_xxxx_gpio

int init_nvram(void)
{
	int model = get_model();
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_QCA)
	int unit = 0;
#endif
#if defined(RTCONFIG_DUALWAN)
	char wan_if[10];
#endif

#if defined (CONFIG_BCMWL5) && defined(RTCONFIG_TCODE)
	refresh_cfe_nvram();
#endif

	TRACE_PT("init_nvram for %d\n", model);

	/* set default value */
	nvram_set("rc_support", "");
	nvram_set_int("btn_rst_gpio", 0xff);
	nvram_set_int("btn_wps_gpio", 0xff);
	nvram_set_int("btn_radio_gpio", 0xff);
	nvram_set_int("led_pwr_gpio", 0xff);
	nvram_set_int("led_wps_gpio", 0xff);
	nvram_set_int("pwr_usb_gpio", 0xff);
	nvram_set_int("pwr_usb_gpio2", 0xff);
	nvram_set_int("led_usb_gpio", 0xff);
	nvram_set_int("led_lan_gpio", 0xff);
	nvram_set_int("led_wan_gpio", 0xff);
	nvram_set_int("led_2g_gpio", 0xff);
	nvram_set_int("led_5g_gpio", 0xff);
	nvram_set_int("led_all_gpio", 0xff);
	nvram_set_int("led_turbo_gpio", 0xff);
	nvram_set_int("led_usb3_gpio", 0xff);
	nvram_set_int("fan_gpio", 0xff);
	nvram_set_int("have_fan_gpio", 0xff);
#ifdef RTCONFIG_SWMODE_SWITCH
	nvram_set_int("btn_swmode1_gpio", 0xff);
	nvram_set_int("btn_swmode2_gpio", 0xff);
	nvram_set_int("btn_swmode3_gpio", 0xff);
	nvram_set_int("swmode_switch", 0);
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	nvram_set_int("btn_wifi_gpio", 0xff);
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	nvram_set_int("btn_wltog_gpio", 0xff);
#endif
#ifdef RTCONFIG_TURBO
	nvram_set_int("btn_turbo_gpio", 0xff);
#endif
#ifdef RTCONFIG_LED_BTN
	nvram_set_int("btn_led_gpio", 0xff);
#endif
	nvram_unset("led_wan_red_gpio");
	nvram_unset("btn_lte_gpio");
	nvram_unset("led_lte_gpio");
	nvram_unset("led_sig1_gpio");
	nvram_unset("led_sig2_gpio");
	nvram_unset("led_sig3_gpio");
	/* In the order of physical placement */
	nvram_set("ehci_ports", "");
	nvram_set("ohci_ports", "");
	nvram_unset("xhci_ports");

	/* treat no phy, no internet as disconnect */
	nvram_set("web_redirect", "3");
#if 0
	conf_swmode_support(model);
#endif
#ifdef RTCONFIG_OPENVPN
	nvram_set("vpn_server1_state", "0");
	nvram_set("vpn_server2_state", "0");
	nvram_set("vpn_client1_state", "0");
	nvram_set("vpn_client2_state", "0");
	nvram_set("vpn_client3_state", "0");
	nvram_set("vpn_client4_state", "0");
	nvram_set("vpn_client5_state", "0");
	nvram_set("vpn_server1_errno", "0");
	nvram_set("vpn_server2_errno", "0");
	nvram_set("vpn_client1_errno", "0");
	nvram_set("vpn_client2_errno", "0");
	nvram_set("vpn_client3_errno", "0");
	nvram_set("vpn_client4_errno", "0");
	nvram_set("vpn_client5_errno", "0");
	nvram_set("vpn_upload_state", "");
	if(!nvram_is_empty("vpn_server_clientlist")) {
		nvram_set("vpn_serverx_clientlist", nvram_safe_get("vpn_server_clientlist"));
		nvram_unset("vpn_server_clientlist");
	}
#endif

#ifdef RTCONFIG_DSL_TCLINUX
	nvram_set("dsllog_opmode", "");
	nvram_set("dsllog_adsltype", "");
	nvram_set("dsllog_snrmargindown", "0");
	nvram_set("dsllog_snrmarginup", "0");
	nvram_set("dsllog_attendown", "0");
	nvram_set("dsllog_attenup", "0");
	nvram_set("dsllog_wanlistmode", "0");
	nvram_set("dsllog_dataratedown", "0");
	nvram_set("dsllog_datarateup", "0");
	nvram_set("dsllog_attaindown", "0");
	nvram_set("dsllog_attainup", "0");
	nvram_set("dsllog_powerdown", "0");
	nvram_set("dsllog_powerup", "0");
	nvram_set("dsllog_crcdown", "0");
	nvram_set("dsllog_crcup", "0");
	nvram_set("dsllog_farendvendorid", "");//far end vendor
	nvram_set("dsllog_tcm", "");//TCM, Trellis Coded Modulation
	nvram_set("dsllog_pathmodedown", "");//downstream path mode
	nvram_set("dsllog_interleavedepthdown", "");//downstream interleave depth
	nvram_set("dsllog_pathmodeup", "");//upstream path mode
	nvram_set("dsllog_interleavedepthup", "");//upstream interleave depth
	nvram_set("dsllog_vdslcurrentprofile", "");//VDSL current profile
#endif

#ifdef RTCONFIG_PUSH_EMAIL
	nvram_set("fb_state", "");
#endif
	nvram_unset("usb_buildin");

#ifdef RTCONFIG_PROXYSTA
#ifdef PXYSTA_DUALBAND
	nvram_set("wlc_band_ex", "");
#endif
	nvram_set("dpsta_ifnames", "");
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if (nvram_get_int("sw_mode") != SW_MODE_REPEATER)
		nvram_unset("ure_disable");
#endif

	/* initialize this value to check fw upgrade status */
	nvram_set_int("upgrade_fw_status", FW_INIT);

#ifdef RTCONFIG_DEFAULT_AP_MODE
	if (nvram_get_int("sw_mode") == SW_MODE_AP && nvram_match("x_Setting", "0")) {
		nvram_set("lan_ipaddr", nvram_safe_get("lan_ipaddr_rt"));
		nvram_set("lan_netmask", nvram_safe_get("lan_netmask_rt"));
	}
#endif

	switch (model) {
#ifdef RTCONFIG_RALINK
	case MODEL_EAN66:
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "eth2 ra0 rai0");
		nvram_set("wan_ifnames", "");
		nvram_set("wl_ifnames","ra0 rai0");
		nvram_set("wl0_vifnames", "");
		nvram_set("wl1_vifnames", "");
		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		//if (!nvram_get("ct_max"))
		//	nvram_set("ct_max", "30000");
		if (nvram_get("sw_mode")==NULL||nvram_get_int("sw_mode")==SW_MODE_ROUTER)
			nvram_set_int("sw_mode", SW_MODE_REPEATER);
		add_rc_support("mssid 2.4G 5G update");
		break;

#ifdef RTN65U
	case MODEL_RTN65U:
		nvram_set("boardflags", "0x100");	// although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "rai0", "ra0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_LED_ALL
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("wl0_HT_TxStream", "2");	// for 300 Mbps RT3352 2T2R 2.4G
		nvram_set("wl0_HT_RxStream", "2");	// for 300 Mbps RT3352 2T2R 2.4G
		nvram_set("wl1_HT_TxStream", "3");	// for 450 Mbps RT3353 3T3R 2.4G/5G
		nvram_set("wl1_HT_RxStream", "3");	// for 450 Mbps RT3883 3T3R 2.4G/5G
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN65U */

#if defined(RTN67U)
	case MODEL_RTN67U:
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("eth3", "eth2", "ra0", NULL, "usb", NULL, NULL, NULL, 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb2_gpio", 28|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_LED_ALL
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
#endif
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("wl0_HT_TxStream", "3");	// for 450 Mbps RT3353 3T3R 2.4G
		nvram_set("wl0_HT_RxStream", "3");	// for 450 Mbps RT3883 3T3R 2.4G
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN67U */

#if defined(RTN36U3)
	case MODEL_RTN36U3:
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("eth3", "eth2", "ra0", NULL, "usb", NULL, NULL, NULL, 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN36U3 */

#if defined(RTN56U)
	case MODEL_RTN56U:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth3", "eth2", "rai0", "ra0", "usb", NULL, NULL, NULL, 0);
#else
		if (nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "eth2 eth3 rai0 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "eth2 rai0 ra0");
			nvram_set("wan_ifnames", "eth3");
		}
		nvram_set("wl_ifnames","rai0 ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#endif

#ifdef RTCONFIG_N56U_SR2
		nvram_set_int("btn_rst_gpio", 25|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_N56U_SR2
		nvram_set_int("led_lan_gpio", 31|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
#ifndef RTCONFIG_N56U_SR2
		eval("rtkswitch", "11");		// for SR3 LAN LED
#endif
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("no_phddns");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* RTN56U */

#if defined(RTN14U)
	case MODEL_RTN14U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", NULL, "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 41|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 40|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
//		if (!nvram_get("ct_max"))
		nvram_set("ct_max", "300000"); // force

		if (nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl"); //for hwnat only
		add_rc_support("manual_stb");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		break;
#endif

#if defined(RTN11P) || defined(RTN300)
	case MODEL_RTN11P:
	case MODEL_RTN300:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", NULL, NULL, "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 44|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 39|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 72|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);

		nvram_set("ct_max", "300000"); // force

		if (nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("rawifi");
		add_rc_support("switchctrl"); //for hwnat only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		break;
#endif

#if defined(RTAC52U)
	case MODEL_RTAC52U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", "rai0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wltog_gpio",13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio",  8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif

#if defined(RTAC51U)
	case MODEL_RTAC51U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", "rai0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif	/* RTAC51U */

#if defined(RTN56UB1)
	case MODEL_RTN56UB1:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface

		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("eth3", "vlan1", "ra0", "rai0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio",  6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 14|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 7|GPIO_ACTIVE_LOW);
		eval("rtkswitch", "11");

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");
		config_netdev_bled("led_5g_gpio", "rai0");
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		//add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");

		break;

#endif	/* RTN56UB1 */
#if defined(RTN54U) || defined(RTAC54U)
	case MODEL_RTN54U:
	case MODEL_RTAC54U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", "rai0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
#if defined(RTAC54U)
		add_rc_support("11AC");
#endif
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTN54U or RTAC54U*/
#if defined(RTAC1200HP)
	case MODEL_RTAC1200HP:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ra0", "rai0", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio",  62|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  61|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 67|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  65|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  65|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 70|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 69|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 68|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 66|GPIO_ACTIVE_LOW);
#endif
		eval("rtkswitch", "11");


		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");
		config_netdev_bled("led_5g_gpio", "rai0");
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC1200HP */
#ifdef RTCONFIG_DSL
	case MODEL_DSLN55U:
		nvram_set("lan_ifname", "br0");

		if (nvram_get_int("sw_mode")==SW_MODE_AP) {
			// switch need to well-set for this config
			nvram_set("lan_ifnames", "eth2.2 rai0 ra0");
			nvram_set("wan_ifnames", "");
		}
		else { // router/default
			nvram_set("lan_ifnames", "eth2.2 rai0 ra0");
			nvram_set("wan_ifnames", "eth2.1.1");
		}

#ifdef RTCONFIG_DUALWAN
		// Support dsl, lan, dsl/usb, lan/usb, dsl/lan, usb/lan only, so far
		if (get_dualwan_secondary()==WANS_DUALWAN_IF_NONE) {
			// dsl, lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1");
			}
			else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4");
			}
		}
		else if (get_dualwan_secondary()==WANS_DUALWAN_IF_LAN) {
			//dsl/lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1 eth2.4");
			}
			/* Paul add 2013/1/24 */
			//usb/lan
			else {
				nvram_set("wan_ifnames", "usb eth2.4");
			}
		}
		else {
			//dsl/usb, lan/usb
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1 usb");
			}
			else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4 usb");
			}
		}
#endif

		nvram_set("wl_ifnames","rai0 ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 25|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIRELESS_SWITCH
		nvram_set_int("btn_wifi_gpio", 1|GPIO_ACTIVE_LOW);
#endif

		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G usbX2");
		add_rc_support("rawifi");
		add_rc_support("dsl");
#if defined(RTCONFIG_WIRELESS_SWITCH) || defined(RTCONFIG_WIFI_TOG_BTN)
		add_rc_support("wifi_hw_sw");
#endif
		add_rc_support("pwrctrl");
		add_rc_support("no_phddns");

		//Ren: set "spectrum_supported" in /router/dsl_drv_tool/tp_init/tp_init_main.c
		if (nvram_match("spectrum_supported", "1"))
		{
			add_rc_support("spectrum"); //Ren
		}

		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif // RTCONFIG_DSL

#if defined(RTN13U)
	case MODEL_RTN13U:
		nvram_set("boardflags", "0x310"); // although it is not used in ralink driver
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "vlan0 ra0");
		nvram_set("wan_ifnames", "vlan1");
		nvram_set("wl_ifnames","ra0");
		nvram_set("wan_ports", "4");
		nvram_set("vlan0hwname", "et0"); // although it is not used in ralink driver
		nvram_set("vlan1hwname", "et0");

		nvram_set_int("btn_rst_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7|GPIO_ACTIVE_LOW);
		break;
#endif	/* RTN13U */
#endif // RTCONFIG_RALINK

#if defined(RTAC55U) || defined(RTAC55UHP)
	case MODEL_RTAC55U:	/* fall through */
	case MODEL_RTAC55UHP:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("eth0", "eth1", "ath0", "ath1", "usb", "eth1", "vlan2", "vlan3", 0);

		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio",  4);
		nvram_set_int("led_lan_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 15);
		nvram_set_int("led_2g_gpio", 13);
#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
		/* SR1 */
		nvram_set("led_usb3_gpio", "etron");
#else
		nvram_set_int("led_5g_gpio", 0);
		nvram_set_int("led_usb3_gpio", 1);
#endif
		nvram_set_int("led_pwr_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_red_gpio", 14);
#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 21);
#endif

		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");

#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
		/* SR1 */
#else
		config_netdev_bled("led_5g_gpio", "ath1");
#endif

		config_usbbus_bled("led_usb_gpio", "3 4");
		config_usbbus_bled("led_usb3_gpio", "1 2");

		/* Etron xhci host:
		 *	USB2 bus: 1-1
		 *	USB3 bus: 2-1
		 * Internal ehci host:
		 * 	USB2 bus: 3-1
		 */
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC55U | RTAC55UHP */

#if defined(RT4GAC55U)
	case MODEL_RT4GAC55U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("eth0", "eth1", "ath0", "ath1", "usb", "eth1", "vlan2", "vlan3", 0);

		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_lte_gpio", 21|GPIO_ACTIVE_LOW);
#if defined(RTAC55U_SR1)
		nvram_set_int("led_usb_gpio",  4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio" , 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio" , 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lte_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig1_gpio",18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig2_gpio",23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig3_gpio",22|GPIO_ACTIVE_LOW);
#else	/* RTAC55U_SR2 */
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lte_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio" ,  4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio" , 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig1_gpio",19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig2_gpio",20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig3_gpio",15|GPIO_ACTIVE_LOW);
#endif	/* RTAC55U_SR */

#ifdef RTCONFIG_WIRELESS_SWITCH
		nvram_set_int("btn_wifi_gpio", 1|GPIO_ACTIVE_LOW);
		add_rc_support("wifi_hw_sw");
#endif	/* RTCONFIG_WIRELESS_SWITCH */

		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		config_usbbus_bled("led_usb_gpio", "1 2");
		nvram_set("ehci_ports", "1-1 2-1");
		nvram_set("usb_buildin", "2");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("gobi");
		break;
#endif	/* RT4GAC55U */

#if defined(PLN12)
	case MODEL_PLN12:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ath0", NULL, NULL, "vlan1", NULL, "vlan3", 0);
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 15);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_orange_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_red_gpio", 17|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled_sleep("led_lan_gpio", 0);
		//config_netdev_bled("led_2g_green_gpio", "ath0");
		//config_netdev_bled("led_2g_orange_gpio", "ath0");
		config_netdev_bled("led_2g_red_gpio", "ath0");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("swmode_switch");
#ifdef RTCONFIG_DHCP_OVERRIDE
		add_rc_support("dhcp_override");

		if (nvram_match("dhcp_enable_x", "1") || nvram_match("x_Setting", "0"))
			nvram_set("dnsqmode", "2");
		else
			nvram_set("dnsqmode", "1");
#endif
		add_rc_support("plc");
		nvram_set("plc_ready", "0");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		/* avoid inserting unnecessary kernel modules */
		nvram_set("nf_ftp", "0");
		nvram_set("nf_pptp", "0");
		break;
#endif	/* PLN12 */

#if defined(PLAC56)
	case MODEL_PLAC56:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ath0", "ath1", NULL, "vlan1", NULL, "vlan3", 0);
		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");

		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("plc_wake_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_red_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_red_gpio", 7|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_green_gpio", "ath0");
		//config_netdev_bled("led_2g_red_gpio", "ath0");
		config_netdev_bled("led_5g_green_gpio", "ath1");
		//config_netdev_bled("led_5g_red_gpio", "ath1");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("swmode_switch");
		add_rc_support("11AC");
#ifdef RTCONFIG_DHCP_OVERRIDE
		add_rc_support("dhcp_override");

		if (nvram_match("dhcp_enable_x", "1") || nvram_match("x_Setting", "0"))
			nvram_set("dnsqmode", "2");
		else
			nvram_set("dnsqmode", "1");
#endif
		add_rc_support("plc");
		nvram_set("plc_ready", "0");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		/* avoid inserting unnecessary kernel modules */
		nvram_set("nf_ftp", "0");
		nvram_set("nf_pptp", "0");
		break;
#endif	/* PLAC56 */

#if defined(PLAC66U)
	case MODEL_PLAC66U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		set_basic_ifname_vars("vlan2", "vlan1", "ath0", "ath1", "usb", "vlan1", NULL, "vlan3", 0);

		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		//nvram_set_int("btn_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_netdev_bled("led_lan_gpio", "eth0");
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		nvram_set("plc_ready", "0");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "3");
		nvram_set("wl0_HT_RxStream", "3");
		nvram_set("wl1_HT_TxStream", "3");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* PLAC66U */

#ifdef CONFIG_BCMWL5
#ifndef RTCONFIG_BCMWL6
#ifdef RTN10U
	case MODEL_RTN10U:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth0", "vlan0", "eth1", NULL, "usb", NULL, "vlan1", "vlan2", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
#endif

		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set_int("led_usb_gpio", 8);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");
		if (nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		add_rc_support("2.4G mssid usbX1 nomedia small_fw");
		break;
#endif

#if defined(RTN10P) || defined(RTN10D1) || defined(RTN10PV2)
	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth0", "vlan0", "eth1", NULL, "usb", NULL, "vlan1", "vlan2", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
#endif

		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set("sb/1/maxp2ga0", "0x52");
		nvram_set("sb/1/maxp2ga1", "0x52");
		if (!nvram_get("ct_max")) {
			if (model == MODEL_RTN10D1)
				nvram_set_int("ct_max", 1024);
		}
		if (nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}

		add_rc_support("2.4G mssid");
#ifdef RTCONFIG_KYIVSTAR
		add_rc_support("kyivstar");
#endif
		break;
#endif

	case MODEL_APN12HP:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3 wl0.4 wl0.5 wl0.6 wl0.7");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set("sb/1/ledbh0", "11");
		nvram_set("sb/1/ledbh1", "11");
		nvram_set("sb/1/ledbh2", "11");
		nvram_set("sb/1/ledbh3", "2");
		nvram_set("sb/1/ledbh5", "11");
		nvram_set("sb/1/ledbh6", "11");
		add_rc_support("pwrctrl");
		nvram_set_int("et_swleds", 0);
		nvram_set("productid", "AP-N12");
#ifdef RTCONFIG_WL_AUTO_CHANNEL
		if (nvram_match("AUTO_CHANNEL", "1")) {
			nvram_set("wl_channel", "6");
			nvram_set("wl0_channel", "6");
		}
#endif
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);	/* does HP have it */
		nvram_set_int("sb/1/ledbh5", 7);			/* is active_high? set 7 then */
		if (!nvram_match("hardware_version", "RTN12HP_B1-2.0.1.5"))     //6691 PA
			add_rc_support("pwrctrl");

#ifdef RTCONFIG_WL_AUTO_CHANNEL
		if (nvram_match("AUTO_CHANNEL", "1")) {
			nvram_set("wl_channel", "6");
			nvram_set("wl0_channel", "6");
		}
#endif
		if (nvram_match("wl0_country_code", "US"))
		{
			if (nvram_match("wl0_country_rev", "37"))
			{
				nvram_set("wl0_country_rev", "16");
			}

			if (nvram_match("sb/1/regrev", "37"))
			{
				nvram_set("sb/1/regrev", "16");
			}
		}
		if (nvram_match("wl0_country_code", "EU"))
		{
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12D1:
	case MODEL_RTN12VP:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh5", 7);
		nvram_set("sb/1/maxp2ga0", "0x52");
		nvram_set("sb/1/maxp2ga1", "0x52");
		nvram_set("sb/1/cck2gpo", "0x0");
		nvram_set("sb/1/ofdm2gpo0", "0x2000");
		nvram_set("sb/1/ofdm2gpo1", "0x6442");
		nvram_set("sb/1/ofdm2gpo", "0x64422000");
		nvram_set("sb/1/mcs2gpo0", "0x2200");
		nvram_set("sb/1/mcs2gpo1", "0x6644");
		nvram_set("sb/1/mcs2gpo2", "0x2200");
		nvram_set("sb/1/mcs2gpo3", "0x6644");
		nvram_set("sb/1/mcs2gpo4", "0x4422");
		nvram_set("sb/1/mcs2gpo5", "0x8866");
		nvram_set("sb/1/mcs2gpo6", "0x4422");
		nvram_set("sb/1/mcs2gpo7", "0x8866");
		if (model == MODEL_RTN12VP)
			add_rc_support("update");
		if (model == MODEL_RTN12D1) {
			if (nvram_match("regulation_domain", "US") && nvram_match("sb/1/regrev", "0")) {
				nvram_set("sb/1/regrev", "2");
			}
		}
		if(nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12C1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("et_swleds", 0x0f);
		nvram_set_int("sb/1/ledbh4", 2);
		nvram_set_int("sb/1/ledbh5", 11);
		nvram_set_int("sb/1/ledbh6", 11);
#ifdef RTCONFIG_SWMODE_SWITCH
		nvram_set_int("btn_swmode1_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode2_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode3_gpio", 8|GPIO_ACTIVE_LOW);
		add_rc_support("swmode_switch");
		nvram_set_int("swmode_switch", 1);
#endif
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12B1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh5", 2);
#ifdef RTCONFIG_SWMODE_SWITCH
		nvram_set_int("btn_swmode1_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode2_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode3_gpio", 8|GPIO_ACTIVE_LOW);
		add_rc_support("swmode_switch");
		nvram_set_int("swmode_switch", 1);
#endif
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12:
		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set("boardflags", "0x310");
		/* fall through to common N12* init */
	case_MODEL_RTN12X:
		nvram_set("lan_ifname", "br0");
		//nvram_set("lan_ifnames", "vlan0 eth1");
		//nvram_set("wan_ifnames", "eth0");
		//nvram_set("wandevs", "et0");
		nvram_set("wl_ifnames", "eth1");

	if (model == MODEL_RTN12D1 || model == MODEL_RTN12HP || model == MODEL_RTN12HP_B1)
	{
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan1 vlan2");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wandevs", "et0");
#endif
	}
	else
	{
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wandevs", "et0");
	}
		break;

#ifdef RTN14UHP
	case MODEL_RTN14UHP:
		nvram_unset("odmpid");
		nvram_unset("et_dispatch_mode");
		nvram_unset("wl_dispatch_mode");
		nvram_set("lan_ifname", "br0");
		add_rc_support("pwrctrl");
#ifdef RTCONFIG_LAN4WAN_LED
		if (nvram_match("odmpid", "TW")) {
			add_rc_support("lanwan_led2");
		}
#endif

#if 0
		set_basic_ifname_vars("eth0", "vlan0", "eth1", NULL, "usb", NULL, NULL, "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan0");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						if (!nvram_match("switch_wan0tagid", ""))
							sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						else
							sprintf(wan_if, "eth0");
						add_wan_phy(wan_if);
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan1");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
#endif

		nvram_set_int("btn_rst_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 25|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 6);
#ifdef RTCONFIG_LAN4WAN_LED
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan1_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan2_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan3_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan4_gpio", 3|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_WL_AUTO_CHANNEL
		if (nvram_match("AUTO_CHANNEL", "1")) {
			nvram_set("wl_channel", "6");
			nvram_set("wl0_channel", "6");
		}
#endif
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G mssid media usbX1 update");
		break;
#endif

#ifdef RTN15U
	case MODEL_RTN15U:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", NULL, "usb", NULL, NULL, "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan1");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						if (!nvram_match("switch_wan0tagid", ""))
							sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						else
							sprintf(wan_if, "eth0");
						add_wan_phy(wan_if);
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
#endif

		nvram_set_int("btn_rst_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 9);
		nvram_set_int("led_lan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh0", 0x82);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "15000");
		add_rc_support("2.4G mssid usbX1 nomedia");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;
#endif

#ifdef RTN16
	case MODEL_RTN16:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", NULL, "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "");
#endif

		nvram_set("wl0_vifnames", "wl0.1");	/* Only one gueset network? */

		nvram_set_int("btn_rst_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-2 1-1");
		nvram_set("ohci_ports", "2-2 2-1");
		nvram_set("boardflags", "0x310");
		nvram_set("sb/1/boardflags", "0x310");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("2.4G update usbX2 mssid");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;
#endif

#ifdef RTN53
	case MODEL_RTN53:
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "vlan0 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 1|GPIO_ACTIVE_LOW);
		/* change lan interface to vlan0 */
		nvram_set("vlan0hwname", "et0");
		nvram_set("landevs", "vlan0 wl0 wl1");
		nvram_unset("vlan2ports");
		nvram_unset("vlan2hwname");
		/* end */
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");

		add_rc_support("2.4G 5G mssid no5gmssid small_fw");
#ifdef RTCONFIG_WLAN_LED
		add_rc_support("led_2g");
		nvram_set("led_5g", "1");
#endif
		break;
#endif
#endif	// RTCONFIG_BCMWL6

#ifdef RTN18U
	case MODEL_RTN18U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", NULL, "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
#endif

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 13);
#endif
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 7|GPIO_ACTIVE_LOW);

		if (nvram_match("bl_version", "3.0.0.7")) {
			nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 6|GPIO_ACTIVE_LOW);
		}
		else if (nvram_match("bl_version", "1.0.0.0")) {
			nvram_set_int("led_usb_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 6|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 9|GPIO_ACTIVE_LOW);
			nvram_set_int("led_2g_gpio", 10|GPIO_ACTIVE_LOW);
		}
		else{
			nvram_set_int("led_usb_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 6|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 9|GPIO_ACTIVE_LOW);
		}

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif

		if (nvram_match("bl_version", "1.0.0.0"))
			add_rc_support("led_2g");
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
		break;
#endif

#ifdef DSL_AC68U
	case MODEL_DSLAC68U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1");
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_DSL && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan100 vlan4");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
						add_wan_phy("vlan4");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_DSL)
						add_wan_phy("vlan100");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "vlan100 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", "vlan100");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "vlan100");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_wan_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);	// 4360's fake led 5g
		nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1");
		nvram_set("ohci_ports", "3-1");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1");
			nvram_set("ohci_ports", "3-1");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1");
			nvram_set("ohci_ports", "2-1");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX1");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("dsl");
		add_rc_support("vdsl");
		add_rc_support("feedback");
		add_rc_support("spectrum");

		if (	nvram_match("wl1_country_code", "EU") &&
				nvram_match("wl1_country_rev", "13")) {
			nvram_set("0:ed_thresh2g", "-70");
			nvram_set("0:ed_thresh5g", "-70");
			nvram_set("1:ed_thresh2g", "-72");
			nvram_set("1:ed_thresh5g", "-72");
		}

		if (nvram_match("x_Setting", "0")) {
			char ver[64];
			snprintf(ver, sizeof(ver), "%s.%s_%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno"));
			nvram_set("innerver", ver);
		}
		break;
#endif

#ifdef RTAC3200
	case MODEL_RTAC3200:
		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1 wl2");

#ifdef RTCONFIG_DUALWAN
#ifdef RTCONFIG_GMAC3
		if (nvram_match("gmac3_enable", "1"))
			nvram_set("wandevs", "et2");
		else
#endif
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth2");
		if (!(get_wans_dualwan()&WANSCAP_5G)) {
			add_lan_phy("eth1");
			add_lan_phy("eth3");
		}

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy(the_wan_phy());
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth2");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G) {
					add_wan_phy("eth1");
					add_wan_phy("eth3");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						if (!nvram_match("switch_wan0tagid", "")) {
							sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							add_wan_phy(wan_if);
						}
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy(the_wan_phy());
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth2 eth1 eth3");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth2 eth1 eth3");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 4|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 15);			// active high
#endif
		nvram_set_int("rst_hw_gpio", 17|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("smart_connect");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
		break;
#endif

#ifdef RTAC68U
	case MODEL_RTAC68U:
		check_cfe_ac68u();
		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", "")) {
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#endif

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_usb_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);	// 4360's fake led 5g
		nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_TURBO
		nvram_set_int("led_turbo_gpio", 4|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 5);	// active high
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif

		break;
#endif

#if defined(RTAC1200G) || defined(RTAC1200GP)
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1");
		nvram_set("lan_ifnames", "vlan1 eth2 eth3");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth2 eth3");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 10|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_5g_gpio", 11);	// active high
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);

		nvram_unset("xhci_ports");
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX1");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("WIFI_LOGO");
		if (model == MODEL_RTAC1200GP)
			add_rc_support("update");
		if (model == MODEL_RTAC1200G) {
			add_rc_support("noitunes");
			add_rc_support("nodm");
			add_rc_support("noftp");
			add_rc_support("noaidisk");
		}

		break;
#endif

#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
	case MODEL_RTAC5300:
		nvram_set("2:ledbh9", "0x7");
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");
	case MODEL_RTAC88U:
	case MODEL_RTAC3100:

//		ldo_patch();

		nvram_set("0:ledbh9", "0x7");
		nvram_set("1:ledbh9", "0x7");
#ifdef RTCONFIG_RGMII_BRCM5301X
		if(nvram_get_int("gmac3_enable")!=1){
			nvram_unset("et0macaddr");
			nvram_unset("et0mdcport");
			nvram_unset("et0phyaddr");
			nvram_set("vlan1hwname", "et1");
			nvram_set("vlan2hwname", "et1");
			nvram_set("et1mdcport", "0");
			nvram_set("et1phyaddr", "30");
		}
		nvram_set("rgmii_port", "5");
#else
		nvram_unset("et1macaddr");
		nvram_unset("et1mdcport");
		nvram_unset("et1phyaddr");
		nvram_unset("rgmii_port");
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("et0mdcport", "0");
		nvram_set("et0phyaddr", "30");
#endif
		nvram_set("lan_ifname", "br0");
		if(model == MODEL_RTAC5300)
			nvram_set("landevs", "vlan1 wl0 wl1 wl2");
		else
			nvram_set("landevs", "vlan1 wl0 wl1");

#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", nvram_safe_get("vlan2hwname"));

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G)) {
				add_lan_phy("eth2");
				if(model == MODEL_RTAC5300)
					add_lan_phy("eth3");
			}

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", "")) {
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", nvram_safe_get("vlan2hwname"));
			if(model == MODEL_RTAC5300)
				nvram_set("lan_ifnames", "vlan1 eth1 eth2 eth3");
			else
				nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		if(model == MODEL_RTAC5300)
			nvram_set("lan_ifnames", "vlan1 eth1 eth2 eth3");
		else
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		if(model == MODEL_RTAC5300)
			nvram_set("wl_ifnames", "eth1 eth2 eth3");
		else
			nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

		/* gpio */
		/* HW reset, 2 | LOW */
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_led_gpio", 4);	// active high(on)
		nvram_set_int("led_wan_gpio", 5);
		/* MDC_4709 RGMII, 6 */
		/* MDIO_4709 RGMII, 7 */
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("reset_switch_gpio", 10);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		/* UART1_RX,  12 */
		/* UART1_TX,  13 */
		/* UART1_CTS, 14*/
		/* UART1_RTS, 15 */
		nvram_set_int("led_usb_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb3_gpio", 17|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_mmc_gpio",  17|GPIO_ACTIVE_LOW);	/* abort */
		nvram_set_int("led_wps_gpio", 19|GPIO_ACTIVE_LOW);
		if(model == MODEL_RTAC5300) {
			nvram_set_int("btn_wltog_gpio", 20|GPIO_ACTIVE_LOW);
			nvram_set_int("btn_wps_gpio", 18|GPIO_ACTIVE_LOW);
			nvram_set_int("fan_gpio", 15);
			nvram_set_int("rpm_fan_gpio", 14|GPIO_ACTIVE_LOW);
		} else {
			nvram_set_int("btn_wltog_gpio", 18|GPIO_ACTIVE_LOW);
			nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		}
		nvram_set_int("led_lan_gpio", 21|GPIO_ACTIVE_LOW);	/* FAN CTRL: reserved */
		/* PA 5V/3.3V switch, 22 */
		/* SDIO_EN_1P8, 23 | HIGH */
#ifdef RTCONFIG_TURBO
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
		nvram_set("ehci_irq", "111");
		nvram_set("xhci_irq", "112");
#ifdef RTCONFIG_MMC_LED
		nvram_set("mmc_irq", "177");
#endif
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
#if defined(RTAC5300)
		add_rc_support("smart_connect");
#endif
		break;
#endif

#ifdef RPAC68U
	case MODEL_RPAC68U:
		/*
		if (nvram_get("sw_mode")==NULL||nvram_get_int("sw_mode")==SW_MODE_ROUTER) {
			printf("\n[rc] reset sw_mode as RP mode\n");
			nvram_set_int("sw_mode", SW_MODE_REPEATER);
		}
		*/

		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1");

#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", "")) {
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

		nvram_set("0:ledbh0", "0x7");
		nvram_set("0:ledbh9", "0x7");
		nvram_set("0:ledbh10", "0x7");
		nvram_set("1:ledbh0", "0x7");
		nvram_set("1:ledbh9", "0x7");
		nvram_set("1:ledbh10", "0x7");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);

#ifdef RTCONFIG_WIFIPWR
		nvram_set_int("pwr_2g_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("pwr_5g_gpio", 13|GPIO_ACTIVE_LOW);
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1");
		nvram_set("ohci_ports", "3-1");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1");
			nvram_set("ohci_ports", "3-1");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1");
			nvram_set("ohci_ports", "2-1");
		}
#endif
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");

		break;
#endif

#ifdef RTAC87U
	case MODEL_RTAC87U:
		if(nvram_match("QTNTELNETSRV","")) nvram_set("QTNTELNETSRV", "0");

		nvram_set("vlan1hwname", "et1");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0");
		nvram_set("rgmii_port", "5");
#ifdef RTCONFIG_QTN
		nvram_set("qtn_ntp_ready", "0");
#endif

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_HW_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			init_dualwan();
		}
		else{
			nvram_set("wandevs", "et1");
			nvram_set("lan_ifnames", "vlan1 eth1 wifi0");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 wifi0");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 wifi0");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		/* WAN2 MAC = 3rd 5G GuestNetwork */
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN){
			nvram_set("wl1_vifnames", "wl1.1 wl1.2");
			nvram_set("wl1.3_bss_enabled", "0");
		}
		nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
		nvram_set("wl1_country_rev", nvram_safe_get("1:regrev"));
		nvram_set("wl1_ifname", "wifi0");
		nvram_set("wl1_phytype", "v");
#endif

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("reset_qtn_gpio", 8|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 4);	// active high
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
#ifdef RTCONFIG_WPS_DUALBAND
	nvram_set_int("wps_band", 0);
	nvram_set_int("wps_dualband", 1);
#else
	nvram_set_int("wps_dualband", 0);
#endif
#if 0
		if (nvram_match("x_Setting", "0") && nvram_get_int("ac87uopmode") == 5) {
			nvram_set("wl1_txbf", "0");
			nvram_set("wl1_itxbf", "0");
		}
#endif

		break;
#endif

#if defined(RTAC56S) || defined(RTAC56U)
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("0:ledbh3", "0x87");	  /* since 163.42 */
		nvram_set("1:ledbh10", "0x87");
		nvram_set("landevs", "vlan1 wl0 wl1");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", "")) {
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#endif

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("pwr_usb_gpio2", 10|GPIO_ACTIVE_LOW);	// Use at the first shipment of RT-AC56S,U.
#else
		nvram_set_int("pwr_usb_gpio", 9);
		nvram_set_int("pwr_usb_gpio2", 10);	// Use at the first shipment of RT-AC56S,U.
#endif
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);	// change led gpio(usb2/usb3) to sync the outer case
		nvram_set_int("led_wan_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 4|GPIO_ACTIVE_LOW);	// actually, this is high active, and will power off all led when active; to fake LOW_ACTIVE to sync boardapi
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);	// 4352's fake led 5g
		nvram_set_int("led_usb3_gpio", 0|GPIO_ACTIVE_LOW);	// change led gpio(usb2/usb3) to sync the outer case
		nvram_set_int("btn_wps_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 7|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_TURBO
		nvram_set_int("btn_turbo_gpio", 5);
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
		break;
#endif

#if defined(RTN66U) || defined(RTAC66U)
	case MODEL_RTN66U:
		nvram_set_int("btn_radio_gpio", 13|GPIO_ACTIVE_LOW);	// ? should not used by Louis
		if (nvram_match("pci/1/1/ccode", "JP") && nvram_match("pci/1/1/regrev", "42") && !nvram_match("watchdog", "20000")) {
			_dprintf("force set watchdog as 20s\n");
			nvram_set("watchdog", "20000");
			nvram_commit();
			reboot(0);
		}

	case MODEL_RTAC66U:
		nvram_set("lan_ifname", "br0");
		if (nvram_match("regulation_domain_5G", "EU") && !nvram_match("watchdog", "0")) {
			nvram_set("watchdog", "0");
			nvram_commit();
			reboot(0);
		}

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#endif

		nvram_set_int("btn_rst_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 13|GPIO_ACTIVE_LOW);	// tmp use
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("fan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("have_fan_gpio", 6|GPIO_ACTIVE_LOW);
		if (nvram_get("regulation_domain")) {
			nvram_set("ehci_ports", "1-1.2 1-1.1 1-1.4");
			nvram_set("ohci_ports", "2-1.2 2-1.1 2-1.4");
		}
		else {
			nvram_set("ehci_ports", "1-1.2 1-1.1");
			nvram_set("ohci_ports", "2-1.2 2-1.1");
		}
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
#ifdef RTCONFIG_PUSH_EMAIL
//		add_rc_support("feedback");
#endif
		break;
#endif

#ifdef RTAC53U
	case MODEL_RTAC53U:
		nvram_set("lan_ifname", "br0");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
#ifdef RTCONFIG_DUALWAN
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							if (!nvram_match("switch_wan0tagid", ""))
								sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#endif
		nvram_set("sb/1/ledbh3", "0x87");
		nvram_set("0:ledbh9", "0x87");
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio",22|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1.4");
		nvram_set("ohci_ports", "2-1.4");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("2.4G 5G mssid usbX1 update");
		break;
#endif

#endif // CONFIG_BCMWL5
	default:
		_dprintf("############################ unknown model #################################\n");
		break;
	}

#if (defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA)) && !defined(RTCONFIG_DUALWAN)
	if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) {
		char wan_if[10];
		if (!nvram_match("switch_wan0tagid", "") && !nvram_match("switch_wan0tagid", "0"))
			sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
		else
			sprintf(wan_if, "eth0");

		nvram_set("wan_ifnames", wan_if);
	}
#endif

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	char word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		nvram_set_int(strcat_r(prefix, "unit", tmp), unit);
		nvram_set(strcat_r(prefix, "ifname", tmp), word);

		unit++;
	}
#endif

#ifdef RTCONFIG_IPV6
	add_rc_support("ipv6");
#endif

#ifdef RTCONFIG_FANCTRL
	if ( button_pressed(BTN_HAVE_FAN) )
		add_rc_support("fanctrl");
#endif

#ifdef RTCONFIG_PARENTALCTRL
	add_rc_support("PARENTAL2");
#endif

#ifdef RTCONFIG_YANDEXDNS
	/* 0: disable, 1: full, -1: partial without qis */
	int yadns_support = 1;
	switch (model) {
	case MODEL_RTN11P:
	case MODEL_RTN300:
		yadns_support = nvram_match("reg_spec", "CE") && nvram_match("wl_reg_2g", "2G_CH13");	//for CE & RU area. (but IN could also be included)
		break;
	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
		yadns_support = (nvram_match("regulation_domain", "RU"));
			//||(nvram_match("regulation_domain", "EU") && nvram_match("sb/1/regrev", "5"));
		break;
	}
	if (yadns_support == 0)
		nvram_set_int("yadns_enable_x", 0);
	else if (yadns_support < 0)
		add_rc_support("yadns_hideqis");
	else
		add_rc_support("yadns");
#endif

#ifdef RTCONFIG_DNSFILTER
	add_rc_support("dnsfilter");
#endif
#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN
	add_rc_support("dualwan");

#ifdef RTCONFIG_DSL
	set_wanscap_support("dsl");
#else
	set_wanscap_support("wan");
#endif
#ifdef RTCONFIG_USB_MODEM
	add_wanscap_support("usb");
#endif
	add_wanscap_support("lan");
#endif // RTCONFIG_DUALWAN

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	add_rc_support("pptpd");
#endif

#ifdef RTCONFIG_USB
#ifdef RTCONFIG_USB_PRINTER
	add_rc_support("printer");
#endif

#ifdef RTCONFIG_USB_MODEM
	// TODO: hide USB Modem UI in 3.0.0.1
	if (strcmp(nvram_safe_get("firmver"), "3.0.0.1")!=0)
		add_rc_support("modem");

#ifdef RTCONFIG_USB_BECEEM
	add_rc_support("wimax");
#endif
#endif

#ifdef RTCONFIG_OPENVPN
	add_rc_support("openvpnd");
	//nvram_set("vpnc_proto", "disable");
#endif

#ifdef RTCONFIG_PUSH_EMAIL
	add_rc_support("email");
#endif

#ifdef RTCONFIG_SSH
	add_rc_support("ssh");
#endif

#ifdef RTCONFIG_WEBDAV
	add_rc_support("webdav");
#endif

#ifdef RTCONFIG_CLOUDSYNC
	add_rc_support("rrsut");	//syn server
	add_rc_support("cloudsync");

	char ss_support_value[1024]="\0";

#ifdef RTCONFIG_CLOUDSYNC
	strcat(ss_support_value, "asuswebstorage ");
#endif

#ifdef RTCONFIG_SWEBDAVCLIENT
	strcat(ss_support_value, "webdav ");
#endif

#ifdef RTCONFIG_DROPBOXCLIENT
	strcat(ss_support_value, "dropbox ");
#endif

#ifdef RTCONFIG_FTPCLIENT
	strcat(ss_support_value, "ftp ");
#endif

#ifdef RTCONFIG_SAMBACLIENT
	strcat(ss_support_value, "samba ");
#endif

#ifdef RTCONFIG_FLICKRCLIENT
	strcat(ss_support_value, "flickr ");
#endif

	nvram_set("ss_support", ss_support_value);
#endif

#ifdef RTCONFIG_ISP_METER
	add_rc_support("ispmeter");
#endif

#ifdef RTCONFIG_MEDIA_SERVER
	add_rc_support("media");
#endif

#ifdef RTCONFIG_APP_PREINSTALLED
	add_rc_support("appbase");
#endif // RTCONFIG_APP_PREINSTALLED

#ifdef RTCONFIG_APP_NETINSTALLED
	add_rc_support("appnet");

// DSL-N55U will follow N56U to have DM
//#ifdef RTCONFIG_RALINK
//#ifdef RTCONFIG_DSL
//	if (model==MODEL_DSLN55U)
//		add_rc_support("nodm");
//#endif
//#endif
#endif // RTCONFIG_APP_NETINSTALLED

#ifdef RTCONFIG_TIMEMACHINE
	add_rc_support("timemachine");
#endif

#ifdef RTCONFIG_FINDASUS
	add_rc_support("findasus");
#endif

#ifdef RTCONFIG_BWDPI
	add_rc_support("bwdpi");

	// tmp to add default nvram
	if(nvram_match("wrs_mals_enable", ""))
		nvram_set("wrs_mals_enable", "0");
	if(nvram_get_int("bwdpi_coll_intl") == 0)
		nvram_set("bwdpi_coll_intl", "1800");
#endif

#ifdef RTCONFIG_TRAFFIC_CONTROL
	add_rc_support("traffic_control");
	nvram_unset("traffic_control_realtime_0");
	nvram_unset("traffic_control_realtime_1");
	nvram_unset("traffic_control_count");
#endif

#ifdef RTCONFIG_ADBLOCK
	add_rc_support("adBlock");
#endif

#ifdef RTCONFIG_SNMPD
	add_rc_support("snmp");
#endif

#ifdef RTCONFIG_TOR
	add_rc_support("tor");
#endif

#ifdef RTCONFIG_DISK_MONITOR
	add_rc_support("diskutility");
#endif

#endif // RTCONFIG_USB

#ifdef RTCONFIG_HTTPS
	add_rc_support("HTTPS");
#endif

#ifdef RTCONFIG_VPNC
	add_rc_support("vpnc");
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
#ifndef RTCONFIG_DISABLE_REPEATER_UI
	add_rc_support("repeater");
#endif
#endif
#ifdef RTCONFIG_PROXYSTA
	add_rc_support("psta");
#endif
#ifdef RTCONFIG_BCMWL6
	add_rc_support("wl6");
#ifdef RTCONFIG_OPTIMIZE_XBOX
	add_rc_support("optimize_xbox");
#endif
#endif
#ifdef RTCONFIG_SFP
	add_rc_support("sfp");
#endif
#ifdef RTCONFIG_4M_SFP
	add_rc_support("sfp4m");
#endif
#ifdef RTCONFIG_8M_SFP
	add_rc_support("sfp8m");
#endif
	//add_rc_support("ruisp");

#ifdef RTCONFIG_WIFI_TOG_BTN
	add_rc_support("wifi_tog_btn");
#endif

#ifdef RTCONFIG_NFS
	add_rc_support("nfsd");
#endif

#ifdef RTCONFIG_IGD2
	add_rc_support("igd2");
#endif
#ifdef RTCONFIG_WPSMULTIBAND
	add_rc_support("wps_multiband");
#endif

#if (defined(RTCONFIG_USER_LOW_RSSI) || defined(RTCONFIG_NEW_USER_LOW_RSSI))
	add_rc_support("user_low_rssi");
#endif

#ifdef RTCONFIG_NTFS
	nvram_set("usb_ntfs_mod", "");
#ifdef RTCONFIG_OPEN_NTFS3G
	nvram_set("usb_ntfs_mod", "open");
#elif defined(RTCONFIG_PARAGON_NTFS)
	add_rc_support("sparse");
	nvram_set("usb_ntfs_mod", "paragon");
#elif defined(RTCONFIG_TUXERA_NTFS)
	add_rc_support("sparse");
	nvram_set("usb_ntfs_mod", "tuxera");
#endif
#endif

#ifdef RTCONFIG_HFS
	nvram_set("usb_hfs_mod", "");
#ifdef RTCONFIG_KERNEL_HFSPLUS
	nvram_set("usb_hfs_mod", "open");
#elif defined(RTCONFIG_PARAGON_HFS)
	nvram_set("usb_hfs_mod", "paragon");
#elif defined(RTCONFIG_TUXERA_HFS)
	nvram_set("usb_hfs_mod", "tuxera");
#endif
#endif

#ifdef RTCONFIG_BCMFA
	add_rc_support("bcmfa");
#endif

#ifdef RTCONFIG_ROG
	add_rc_support("rog");
#endif

#ifdef RTCONFIG_TCODE
	add_rc_support("tcode");
#endif

#ifdef RTCONFIG_JFFS2USERICON
	add_rc_support("usericon");
#endif

#ifdef RTCONFIG_TR069
	add_rc_support("tr069");

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)
	add_rc_support("jffs2");
#endif
#endif

#ifdef RTCONFIG_WPS_ALLLED_BTN
	nvram_set_int("AllLED", 1);
	add_rc_support("cfg_wps_btn");
#endif

#ifdef RTCONFIG_STAINFO
	add_rc_support("stainfo");
#endif

#ifdef RTCONFIG_CLOUDCHECK
	add_rc_support("cloudcheck");
#endif

#ifdef RTCONFIG_GETREALIP
	add_rc_support("realip");
#endif
#ifdef RTCONFIG_TCODE
	config_tcode();
#endif
#ifdef RTCONFIG_LACP
	add_rc_support("lacp");
#endif
#ifdef RTCONFIG_KEY_GUARD
	add_rc_support("keyGuard");
#endif
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	if(!nvram_match("wifi_psk", ""))
		add_rc_support("defpsk");
#endif
#ifdef RTCONFIG_REBOOT_SCHEDULE
	add_rc_support("reboot_schedule");
	// tmp to add default nvram
	if(nvram_match("reboot_schedule_enable", ""))
		nvram_set("reboot_schedule_enable", "0");
	if(nvram_match("reboot_schedule", ""))
		nvram_set("reboot_schedule", "00000000000");
#endif
#ifdef RTCONFIG_WTFAST
	add_rc_support("wtfast");
#endif
#ifdef RTCONFIG_WIFILOGO
	add_rc_support("wifilogo");
#endif
	return 0;
}

int init_nvram2(void)
{
	char *macp = NULL;
	unsigned char mac_binary[6];
	char friendly_name[32];

	macp = get_lan_hwaddr();
	ether_atoe(macp, mac_binary);
#ifdef RTAC1200GP
	sprintf(friendly_name, "%s-%02X%02X", "RT-AC1200GPLUS", mac_binary[4], mac_binary[5]);
#else
	sprintf(friendly_name, "%s-%02X%02X", get_productid(), mac_binary[4], mac_binary[5]);
#endif
	if (restore_defaults_g)
	{
		nvram_set("computer_name", friendly_name);
		nvram_set("dms_friendly_name", friendly_name);
		nvram_set("daapd_friendly_name", friendly_name);

		nvram_commit();
	}

#ifdef RTCONFIG_VPNC
	nvram_set_int("vpnc_state_t", 0);
	nvram_set_int("vpnc_sbstate_t", 0);
#endif

	return 0;
}

void force_free_caches()
{
#ifdef RTCONFIG_RALINK
	int model = get_model();

	if (model != MODEL_RTN65U)
		free_caches(FREE_MEM_PAGE, 2, 0);
#elif defined(RTCONFIG_QCA)
	/* do nothing */
#else
	int model = get_model();

	if (model==MODEL_RTN53||model==MODEL_RTN10D1) {
		free_caches(FREE_MEM_PAGE, 2, 0);
	}
#endif

#ifdef RTCONFIG_BCMARM
	f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
#endif
}

#ifdef RTN65U
/* Set PCIe ASPM policy */
int set_pcie_aspm(void)
{
	const char *aspm_policy = "powersave";

	if (nvram_get_int("aspm_disable"))
		aspm_policy = "performance";

	return f_write_string("/sys/module/pcie_aspm/parameters/policy", aspm_policy, 0, 0);
}
#endif //RTN65U

int limit_page_cache_ratio(int ratio)
{
	int min = 5;
	char p[] = "100XXX";

	if (ratio > 100 || ratio < min)
		return -1;

	if (ratio < 90) {
#if defined(RTCONFIG_OPEN_NTFS3G) && (LINUX_KERNEL_VERSION < KERNEL_VERSION(3,0,0))
		min = 90;
#endif

		if (get_meminfo_item("MemTotal") <= 64)
			min = 90;
	}

	if (ratio < min)
		ratio = min;

	sprintf(p, "%d", ratio);
	return f_write_string("/proc/sys/vm/pagecache_ratio", p, 0, 0);
}

#ifdef RTCONFIG_BCMFA
#if 0
static int
build_ifnames(char *type, char *names, int *size)
{
	char name[32], *next;
	int len = 0;
	int s;

	/* open a raw scoket for ioctl */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return -1;

	foreach(name, type, next) {
		struct ifreq ifr;
		int i, unit;
		char var[32], *mac;
		unsigned char ea[ETHER_ADDR_LEN];

		/* vlan: add it to interface name list */
		if (!strncmp(name, "vlan", 4)) {
			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", name);
			continue;
		}

		/* others: proceed only when rules are met */
		for (i = 1; i <= DEV_NUMIFS; i ++) {
			/* ignore i/f that is not ethernet */
			ifr.ifr_ifindex = i;
			if (ioctl(s, SIOCGIFNAME, &ifr))
				continue;
			if (ioctl(s, SIOCGIFHWADDR, &ifr))
				continue;
			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
				continue;
			if (!strncmp(ifr.ifr_name, "vlan", 4))
				continue;
			/* wl: use unit # to identify wl */
			if (!strncmp(name, "wl", 2)) {
				if (wl_probe(ifr.ifr_name) ||
				    wl_ioctl(ifr.ifr_name, WLC_GET_INSTANCE, &unit, sizeof(unit)) ||
				    unit != atoi(&name[2]))
					continue;
			}
			/* et/il: use mac addr to identify et/il */
			else if (!strncmp(name, "et", 2) || !strncmp(name, "il", 2)) {
				snprintf(var, sizeof(var), "%smacaddr", name);
				if (!(mac = nvram_get(var)) || !ether_atoe(mac, ea) ||
				    bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
					continue;
			}
			/* mac address: compare value */
			else if (ether_atoe(name, ea) &&
				!bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
				;
			/* others: ignore */
			else
				continue;
			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", ifr.ifr_name);
		}
	}

	close(s);

	*size = len;
	return 0;
}
#endif

/* Override the "0 5u" to "0 5" to backward compatible with old image */
static void
fa_override_vlan2ports()
{
	char port[] = "XXXX", *nvalue;
	char *next, *cur, *ports, *u;
	int len;

	ports = nvram_get("vlan2ports");
	nvalue = malloc(strlen(ports) + 2);
	if (!nvalue) {
		_dprintf("Memory allocate failed!\n");
		return;
	}
	memset(nvalue, 0, strlen(ports) + 2);

	/* search last port include 'u' */
	for (cur = ports; cur; cur = next) {
		/* tokenize the port list */
		while (*cur == ' ')
			cur ++;
		next = strstr(cur, " ");
		len = next ? next - cur : strlen(cur);
		if (!len)
			break;
		if (len > sizeof(port) - 1)
			len = sizeof(port) - 1;
		strncpy(port, cur, len);
		port[len] = 0;

		/* prepare new value */
		if ((u = strchr(port, 'u')))
			*u = '\0';
		strcat(nvalue, port);
		strcat(nvalue, " ");
	}

	/* Remove last " " */
	len = strlen(nvalue);
	if (len) {
		nvalue[len-1] = '\0';
		nvram_set("vlan2ports", nvalue);
	}
}

void
fa_mode_adjust()
{
	char *wan_proto;
	char buf[16];

	snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));

	if (nvram_get_int("sw_mode") == SW_MODE_ROUTER || nvram_get_int("sw_mode") == SW_MODE_AP) {
		if (!nvram_match("ctf_disable_force", "1")
			&& nvram_get_int("ctf_fa_cap")
			&& !nvram_match("gmac3_enable", "1")
 			&& !nvram_get_int("qos_enable")
		) {
			nvram_set_int("ctf_fa_mode", CTF_FA_NORMAL);
		}else{
			nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
		}
	}else{ /* repeater mode */
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
	}

	if (nvram_match("x_Setting", "0") ||
	  (nvram_get_int("sw_mode") == SW_MODE_REPEATER)) {
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
	} else if (nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
		if (!strcmp(buf, "wan none") || !strcmp(buf, "wan usb"))
			wan_proto = nvram_safe_get("wan0_proto");
		else // for (usb wan).
			wan_proto = nvram_safe_get("wan1_proto");

		if ((strcmp(wan_proto, "dhcp") && strcmp(wan_proto, "static"))
#ifdef RTCONFIG_BWDPI
		|| check_bwdpi_nvram_setting()
#endif
		) {
			nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
			_dprintf("wan_proto:%s not support FA mode...\n", wan_proto);
		}
	}
	if(!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")){
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
		dbG("IPTV environment, disable FA mode\n");
	}

#if defined(RTCONFIG_RGMII_BCM_FA) || defined(RTAC5300) || defined(RTAC3100)
	if (nvram_get_int("lan_stp") == 1){
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
		logmessage("CTF", "STP enabled, disable FA");
	}
	if (nvram_get_int("sw_mode") != SW_MODE_ROUTER) {
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
		logmessage("CTF", "Not router mode, disable FA");
	}
#endif

#ifdef RTCONFIG_DSL	//TODO
	nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
#endif
}

void
fa_mode_init()
{
	fa_mode_adjust();

	fa_mode = atoi(nvram_safe_get("ctf_fa_mode"));
	switch (fa_mode) {
		case CTF_FA_BYPASS:
		case CTF_FA_NORMAL:
			break;
		default:
			fa_mode = CTF_FA_DISABLED;
			break;
	}
}

void
fa_nvram_adjust()	/* before insmod et */
{
#ifdef RTCONFIG_DUALWAN
	char buf[16];
	snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") && strcmp(buf, "wan usb") && strcmp(buf, "usb wan"))
		return;
#endif
	fa_mode = nvram_get_int("ctf_fa_mode");
	if (FA_ON(fa_mode)) {

		_dprintf("\nFA on.\n");

		/* Set et2macaddr, et2phyaddr as same as et0macaddr, et0phyaddr */
		if (!nvram_get("vlan2ports") || !nvram_get("wandevs"))  {
			_dprintf("Insufficient envram, cannot do FA override\n");
			return;
		}

		/* adjusted */
		if (!strcmp(nvram_get("wandevs"), "vlan2") &&
		!strchr(nvram_get("vlan2ports"), 'u'))
			return;

		/* The vlan2ports will be change to "0 8u" dynamically by
		 * robo_fa_imp_port_upd. Keep nvram vlan2ports unchange.
		 */
		fa_override_vlan2ports();

		/* Override wandevs to "vlan2" */
		nvram_set("wandevs", "vlan2");
		nvram_set("wan_ifnames", "vlan2");
		if (nvram_match("wan0_ifname", "eth0")) nvram_set("wan0_ifname", "vlan2");

		if (!RESTORE_DEFAULTS()) {
			_dprintf("Override FA nvram...\n");
			nvram_commit();
		}
	}
	else {
		_dprintf("\nFA off.\n");
	}
}

void
chk_etfa()	/* after insmod et */
{
	if (FA_ON(fa_mode)) {
		if (
			!nvram_get_int("ctf_fa_cap") ||
			nvram_match("ctf_disable", "1") || nvram_match("ctf_disable_force", "1")) { // in case UI chaos
			_dprintf("\nChip Not Support FA Mode or ctf disabled!\n");

			nvram_unset("ctf_fa_mode");
			nvram_commit();
			reboot(RB_AUTOBOOT);
			return;
		}
	} else {
		if (!nvram_get_int("ctf_fa_cap"))
			nvram_unset("ctf_fa_mode");
	}
}
#endif /* RTCONFIG_BCMFA */

#ifdef RTCONFIG_GMAC3
#define GMAC3_ENVRAM_BACKUP(name)				\
do {								\
	char *var, bvar[NVRAM_MAX_PARAM_LEN];			\
	if ((var = nvram_get(name)) != NULL) {			\
		snprintf(bvar, sizeof(bvar), "old_%s", name);   \
		nvram_set(bvar, var);				\
	}							\
} while (0)

/* Override GAMC3 nvram */
static void
gmac3_override_nvram()
{
	char var[32], *lists, *next;
	char newlists[NVRAM_MAX_PARAM_LEN];

	/* back up old embedded nvram */
#if defined(RTAC5300) || defined(RTAC88U)
	if(strcmp(nvram_safe_get("et1macaddr"), "00:00:00:00:00:00") !=0 )
		GMAC3_ENVRAM_BACKUP("et1macaddr");
	if(strcmp(nvram_safe_get("vlan1hwname"), "et1") == 0 )
		GMAC3_ENVRAM_BACKUP("vlan1hwname");
	if(strcmp(nvram_safe_get("vlan1ports"), " 8u") == NULL)
		GMAC3_ENVRAM_BACKUP("vlan1ports");
	if(strcmp(nvram_safe_get("vlan2hwname"), "et1") ==0 )
		GMAC3_ENVRAM_BACKUP("vlan2hwname");
	if(strcmp(nvram_safe_get("vlan2ports"), " 8u") == NULL)
		GMAC3_ENVRAM_BACKUP("vlan2ports");
	if(strcmp(nvram_safe_get("wandevs"), "et1") ==0 )
		GMAC3_ENVRAM_BACKUP("wandevs");
#else
	GMAC3_ENVRAM_BACKUP("et0macaddr");
	GMAC3_ENVRAM_BACKUP("et1macaddr");
	GMAC3_ENVRAM_BACKUP("et2macaddr");
	GMAC3_ENVRAM_BACKUP("vlan1hwname");
	GMAC3_ENVRAM_BACKUP("vlan1ports");
	GMAC3_ENVRAM_BACKUP("vlan2hwname");
	GMAC3_ENVRAM_BACKUP("vlan2ports");
	GMAC3_ENVRAM_BACKUP("wandevs");
#endif
	GMAC3_ENVRAM_BACKUP("et0mdcport");
	GMAC3_ENVRAM_BACKUP("et1mdcport");
	GMAC3_ENVRAM_BACKUP("et2mdcport");
	GMAC3_ENVRAM_BACKUP("et0phyaddr");
	GMAC3_ENVRAM_BACKUP("et1phyaddr");
	GMAC3_ENVRAM_BACKUP("et2phyaddr");

	/* change mac, mdcport, phyaddr */
#if defined(RTAC5300) || defined(RTAC3100) || defined(RTAC88U)
	if(strcmp(nvram_safe_get("et1macaddr"), "00:00:00:00:00:00") !=0 )
		nvram_set("et2macaddr", nvram_get("et1macaddr"));
	nvram_set("et2mdcport", nvram_get("et1mdcport"));
	nvram_set("et2phyaddr", nvram_get("et1phyaddr"));
	nvram_set("et0mdcport", nvram_get("et1mdcport"));
	nvram_set("et0phyaddr", nvram_get("et1phyaddr"));
	nvram_set("et0macaddr", "00:00:00:00:00:00");
	nvram_set("et1macaddr", "00:00:00:00:00:00");
#else
	nvram_set("et2macaddr", nvram_get("et0macaddr"));
	nvram_set("et2mdcport", nvram_get("et0mdcport"));
	nvram_set("et2phyaddr", nvram_get("et0phyaddr"));
	nvram_set("et1mdcport", nvram_get("et0mdcport"));
	nvram_set("et1phyaddr", nvram_get("et0phyaddr"));
	nvram_set("et0macaddr", "00:00:00:00:00:00");
	nvram_set("et1macaddr", "00:00:00:00:00:00");
#endif

	/* change vlan ports */
	if (!(lists = nvram_get("vlan1ports"))) {
		_dprintf("Default vlan1ports is not specified, override GMAC3 defaults...\n");
		nvram_set("vlan1ports", "1 2 3 4 5 7 8*");
	} else {
		strncpy(newlists, lists, sizeof(newlists));
		newlists[sizeof(newlists)-1] = '\0';

		/* search first port include '*' or 'u' and remove it */
		foreach(var, lists, next) {
			if (strchr(var, '*') || strchr(var, 'u')) {
				remove_from_list(var, newlists, sizeof(newlists));
				break;
			}
		}

		/* add port 5, 7 and 8* */
		add_to_list("5", newlists, sizeof(newlists));
		add_to_list("7", newlists, sizeof(newlists));
		add_to_list("8*", newlists, sizeof(newlists));
		nvram_set("vlan1ports", newlists);
	}

	if (!(lists = nvram_get("vlan2ports"))) {
		_dprintf("Default vlan2ports is not specified, override GMAC3 defaults...\n");
		nvram_set("vlan2ports", "0 8u");
	} else {
		strncpy(newlists, lists, sizeof(newlists));
		newlists[sizeof(newlists)-1] = '\0';

		/* search first port include '*' or 'u' and remove it */
		foreach(var, lists, next) {
			if (strchr(var, '*') || strchr(var, 'u')) {
				remove_from_list(var, newlists, sizeof(newlists));
				break;
			}
		}

		/* add port 8u */
		add_to_list("8u", newlists, sizeof(newlists));
		nvram_set("vlan2ports", newlists);
	}

	/* change wandevs vlan hw name */
	nvram_set("wandevs", "et2");
	nvram_set("vlan1hwname", "et2");
	nvram_set("vlan2hwname", "et2");

	/* landevs should be have wl1 wl2 */

	/* set fwd_wlandevs from lan_ifnames */
	if (!(lists = nvram_get("lan_ifnames"))) {
		/* should not be happened */
		_dprintf("lan_ifnames is not exist, override GMAC3 defaults...\n");
		nvram_set("fwd_wlandevs", "eth1 eth2 eth3");
	} else {
		strncpy(newlists, lists, sizeof(newlists));
		newlists[sizeof(newlists)-1] = '\0';

		/* remove ifname if it's not a wireless interrface */
		foreach(var, lists, next) {
			if (wl_probe(var)) {
				remove_from_list(var, newlists, sizeof(newlists));
				continue;
			}
		}
		nvram_set("fwd_wlandevs", newlists);
	}

	/* set fwddevs */
#if defined(RTAC5300)
	nvram_set("fwddevs", "fwd0 fwd1");
#else	/* RTAC88U */
	nvram_set("fwddevs", "fwd1");
	nvram_set("fwd_cpumap", "d:x:2:163:1 d:l:5:169:1");
	nvram_set("fwd_wlandevs", "eth1 eth2");
#endif
}

#define GMAC3_ENVRAM_RESTORE(name)				\
do {								\
	char *var, bvar[NVRAM_MAX_PARAM_LEN];			\
	snprintf(bvar, sizeof(bvar), "old_%s", name);		\
	if ((var = nvram_get(bvar))) {				\
		nvram_set(name, var);				\
		nvram_unset(bvar);				\
	}							\
	else {							\
		nvram_unset(name);				\
	}							\
} while (0)

static void
gmac3_restore_nvram()
{
	/* back up old embedded nvram */
	GMAC3_ENVRAM_RESTORE("et0macaddr");
#if defined(RTAC5300)|| defined(RTAC88U)
	if(strcmp(nvram_safe_get("old_et1macaddr"), "00:00:00:00:00:00") != 0 )
		GMAC3_ENVRAM_RESTORE("et1macaddr");
#else
	GMAC3_ENVRAM_RESTORE("et1macaddr");
#endif
	GMAC3_ENVRAM_RESTORE("et2macaddr");
	GMAC3_ENVRAM_RESTORE("et0mdcport");
	GMAC3_ENVRAM_RESTORE("et1mdcport");
	GMAC3_ENVRAM_RESTORE("et2mdcport");
	GMAC3_ENVRAM_RESTORE("et0phyaddr");
	GMAC3_ENVRAM_RESTORE("et1phyaddr");
	GMAC3_ENVRAM_RESTORE("et2phyaddr");
	GMAC3_ENVRAM_RESTORE("vlan1ports");
	GMAC3_ENVRAM_RESTORE("vlan2ports");
	GMAC3_ENVRAM_RESTORE("vlan1hwname");
	GMAC3_ENVRAM_RESTORE("vlan2hwname");
	GMAC3_ENVRAM_RESTORE("wandevs");

	nvram_unset("fwd_wlandevs");
	nvram_unset("fwddevs");
}

static void
gmac3_restore_defaults()
{
	/* nvram variables will be changed when gmac3_enable */
	if (!strcmp(nvram_safe_get("wandevs"), "et2") &&
	nvram_get("fwd_wlandevs") && nvram_get("fwddevs")) {
		gmac3_restore_nvram();

		/* Handle the case of user disable GMAC3 and do restore defaults. */
		if (nvram_invmatch("gmac3_enable", "1"))
			nvram_set("gmac3_reboot", "1");
	}
}

void
gmac3_nvram_adjust()
{
	int fa_mode;
	bool _reboot = FALSE;
	bool gmac3_enable, gmac3_configured = FALSE;

	/* gmac3_enable nvram control everything */
	gmac3_enable = nvram_match("gmac3_enable", "1") ? TRUE : FALSE;

	/* nvram variables will be changed when gmac3_enable */
	if (!strcmp(nvram_safe_get("wandevs"), "et2") &&
#if defined(RTAC5300) || defined(RTAC88U)
	    nvram_get("fwd_wlandevs") && (strlen(nvram_get("fwd_wlandevs")) > 3) &&
	    nvram_get("fwddevs"))
#else
	    nvram_get("fwd_wlandevs") &&
	    nvram_get("fwddevs"))
#endif
		gmac3_configured = TRUE;

	fa_mode = nvram_get_int("ctf_fa_mode");

	if (fa_mode == CTF_FA_NORMAL || fa_mode == CTF_FA_BYPASS) {
		_dprintf("GMAC3 based forwarding not compatible with ETFA - AuX port...\n");
		return;
	}

	if (et_capable(NULL, "gmac3")) {
		if (gmac3_enable) {
			_dprintf("\nGMAC3 on.\n");

			if (gmac3_configured)
				return;
			/* The vlan2ports will be change to "0 8u" dynamically by
			 * robo_fa_imp_port_upd. Keep nvram vlan2ports unchange.
			 */
			gmac3_override_nvram();
			_dprintf("Override GMAC3 nvram...\n");
			_reboot = TRUE;
		} else {
			_dprintf("\nGMAC3 off.\n");
#if defined(RTAC5300) || defined(RTAC88U)
			if (gmac3_configured ||
				(strlen(nvram_safe_get("fwd_wlandevs")) > 0 || strlen(nvram_safe_get("fwddevs") ) > 0)) {
#else
			if (gmac3_configured){
#endif
				gmac3_restore_nvram();
				_reboot = TRUE;
			}
		}
	} else {
		_dprintf("GMAC3 not supported...\n");

		if (gmac3_enable) {
			nvram_unset("gmac3_enable");
			_reboot = TRUE;
		}

		/* GMAC3 is not capable */
		if (gmac3_configured) {
			gmac3_restore_nvram();
			_reboot = TRUE;
		}
	}

	if (nvram_get("gmac3_reboot")) {
		nvram_unset("gmac3_reboot");
		_reboot = TRUE;
	}

	if (_reboot) {
		nvram_commit();
		_dprintf("GMAC3 nvram overridden, rebooting...\n");
		reboot(RB_AUTOBOOT);
	}
}
#endif

static void sysinit(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i, r, retry = 0;
	DIR *d;
	struct dirent *de;
	char s[256];
	char t[256];
	int ratio = 30;
	int min_free_kbytes_check= 0;
#if !defined(RTCONFIG_QCA)
	int model;
#endif

#define MKNOD(name,mode,dev)		if (mknod(name,mode,dev))		perror("## mknod " name)
#define MOUNT(src,dest,type,flag,data)	if (mount(src,dest,type,flag,data))	perror("## mount " src)
#define MKDIR(pathname,mode)		if (mkdir(pathname,mode))		perror("## mkdir " pathname)

	MOUNT("proc", "/proc", "proc", 0, NULL);
	MOUNT("tmpfs", "/tmp", "tmpfs", 0, NULL);

#ifdef LINUX26
#ifndef DEVTMPFS
	MOUNT("devfs", "/dev", "tmpfs", MS_MGC_VAL | MS_NOATIME, NULL);
#endif
	MKNOD("/dev/null", S_IFCHR | 0666, makedev(1, 3));
	MKNOD("/dev/console", S_IFCHR | 0600, makedev(5, 1));
	MOUNT("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
	MKDIR("/dev/shm", 0777);
	MKDIR("/dev/pts", 0777);
	MOUNT("devpts", "/dev/pts", "devpts", MS_MGC_VAL, NULL);
#endif

	init_devs(); // for system dependent part

	if (console_init()) noconsole = 1;

	stime(&tm);

	static const char *mkd[] = {
		"/tmp/etc", "/tmp/var", "/tmp/home",
#ifdef RTCONFIG_USB
		POOL_MOUNT_ROOT,
		NOTIFY_DIR,
		NOTIFY_DIR"/"NOTIFY_TYPE_USB,
#endif
		"/tmp/share", "/var/webmon", // !!TB
		"/var/log", "/var/run", "/var/tmp", "/var/lib", "/var/lib/misc",
		"/var/spool", "/var/spool/cron", "/var/spool/cron/crontabs",
		"/tmp/var/wwwext", "/tmp/var/wwwext/cgi-bin",	// !!TB - CGI support
		NULL
	};
	umask(0);
	for (i = 0; mkd[i]; ++i) {
		mkdir(mkd[i], 0755);
	}

	// LPRng support
	mkdir("/var/state", 0777);
	mkdir("/var/state/parport", 0777);
	mkdir("/var/state/parport/svr_statue", 0777);

	mkdir("/var/lock", 0777);
	mkdir("/var/tmp/dhcp", 0777);
	mkdir("/home/root", 0700);
	chmod("/tmp", 0777);
#ifdef RTCONFIG_USB
	chmod(POOL_MOUNT_ROOT, 0777);
#endif
	f_write("/etc/hosts", NULL, 0, 0, 0644);			// blank
	f_write("/etc/fstab", NULL, 0, 0, 0644);			// !!TB - blank
	f_write("/tmp/settings", NULL, 0, 0, 0644);

	//umask(022);

	if ((d = opendir("/rom/etc")) != NULL) {
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') continue;
			snprintf(s, sizeof(s), "%s/%s", "/rom/etc", de->d_name);
			snprintf(t, sizeof(t), "%s/%s", "/etc", de->d_name);
			symlink(s, t);
		}
		closedir(d);
	}
	symlink("/proc/mounts", "/etc/mtab");

	/* /etc/resolv.conf compatibility */
	unlink("/etc/resolv.conf");
	symlink("/tmp/resolv.conf", "/etc/resolv.conf");

#if defined(RTAC55U) || defined(RTAC55UHP)
	ratio = 20;
#endif
	limit_page_cache_ratio(ratio);

#ifdef RTN65U
	set_pcie_aspm();
#endif

#ifdef RTCONFIG_SAMBASRV
	if ((d = opendir("/usr/codepages")) != NULL) {
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') continue;
			snprintf(s, sizeof(s), "/usr/codepages/%s", de->d_name);
			snprintf(t, sizeof(t), "/usr/share/%s", de->d_name);
			symlink(s, t);
		}
		closedir(d);
	}
#endif

#if defined(RTCONFIG_BLINK_LED)
	modprobe("bled");
#endif

#ifdef LINUX26
	do {
		r = eval("mdev", "-s");
		if (r || retry)
			_dprintf("mdev coldplug terminated. (ret %d retry %d)\n", r, retry);
	} while (r && retry++ < 10);
	start_hotplug2();

	static const char *dn[] = {
		"null", "zero", "random", "urandom", "full", "ptmx", "nvram",
		NULL
	};
	for (i = 0; dn[i]; ++i) {
		snprintf(s, sizeof(s), "/dev/%s", dn[i]);
		chmod(s, 0666);
	}
	chmod("/dev/gpio", 0660);
#endif

	setup_passwd();

	set_action(ACT_IDLE);

	for (i = 0; defenv[i]; ++i) {
		putenv(defenv[i]);
	}

	if (!noconsole) {
		printf("\n\nHit ENTER for console...\n\n");
		run_shell(1, 0);
	}

	dbg("firmware version: %s.%s_%s\n", rt_version, rt_serialno, rt_extendno);

	nvram_set("extendno_org", nvram_safe_get("extendno"));

	init_syspara();// for system dependent part (befor first get_model())

#ifdef RTCONFIG_RALINK
	model = get_model();
	// avoid the process like fsck to devour the memory.
	// ex: when DUT ran fscking, restarting wireless would let DUT crash.
	if ((model == MODEL_RTN56U) || (model == MODEL_DSLN55U) || (model == MODEL_RTAC52U) || (model == MODEL_RTAC51U)
	      ||(model == MODEL_RTN14U) ||(model == MODEL_RTN54U) ||(model == MODEL_RTAC54U) ||(model == MODEL_RTAC1200HP) || (model == MODEL_RTN56UB1))
		{
			f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
			min_free_kbytes_check = 1;
		}
	else	{
		 	f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
			min_free_kbytes_check = 0;
		}
#elif defined(RTCONFIG_QCA)
	f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
	min_free_kbytes_check = 1;
#else
	model = get_model();
	// At the Broadcom platform, restarting wireless won't use too much memory.
	if (model==MODEL_RTN53) {
		f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
		min_free_kbytes_check = 0;
	}
	else if (model==MODEL_RTAC53U) {
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
		min_free_kbytes_check = 1;
	}
#endif
#ifdef RTCONFIG_16M_RAM_SFP
	f_write_string("/proc/sys/vm/min_free_kbytes", "512", 0, 0);
	min_free_kbytes_check = 0;
#endif
#ifdef RTCONFIG_BCMARM
//	f_write_string("/proc/sys/vm/min_free_kbytes", "14336", 0, 0);
	// fix _dma_rxfill error under stress test
	f_write_string("/proc/sys/vm/min_free_kbytes", "20480", 0, 0);
	min_free_kbytes_check = 1;
#endif
#ifdef RTCONFIG_USB
	if(min_free_kbytes_check<1){
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
	}
#endif

	force_free_caches();

	// autoreboot after kernel panic
	f_write_string("/proc/sys/kernel/panic", "3", 0, 0);
	f_write_string("/proc/sys/kernel/panic_on_oops", "3", 0, 0);

	// be precise about vm commit
#ifdef CONFIG_BCMWL5
	f_write_string("/proc/sys/vm/overcommit_memory", "2", 0, 0);

	if (model==MODEL_RTN53 ||
		model==MODEL_RTN10U ||
		model==MODEL_RTN12B1 || model==MODEL_RTN12C1 ||
		model==MODEL_RTN12D1 || model==MODEL_RTN12VP || model==MODEL_RTN12HP || model==MODEL_RTN12HP_B1 ||
		model==MODEL_RTN15U) {

		f_write_string("/proc/sys/vm/panic_on_oom", "1", 0, 0);
		f_write_string("/proc/sys/vm/overcommit_ratio", "100", 0, 0);
	}
#endif

#ifdef RTCONFIG_IPV6
	// disable IPv6 by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/disable_ipv6", "1", 0, 0);
	// disable IPv6 DAD by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/accept_dad", "0", 0, 0);
	// increase ARP cache
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh1", "512", 0, 0);
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh2", "1024", 0, 0);
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh3", "2048", 0, 0);
#endif

#if defined(RTCONFIG_PSISTLOG) || defined(RTCONFIG_JFFS2LOG)
	f_write_string("/proc/sys/net/unix/max_dgram_qlen", "150", 0, 0);
#endif

	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++) {
		signal(fatalsigs[i], handle_fatalsigs);
	}
	signal(SIGCHLD, handle_reap);

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_RALINK
	check_upgrade_from_old_ui();
#endif
#endif

#ifdef RTCONFIG_ATEUSB3_FORCE
	post_syspara();	// make sure usb_usb3 is fix-up in accordance with the force USB3 parameter.
#endif
#ifdef RTCONFIG_BCMFA
	fa_mode_init();
#endif
	init_nvram();  // for system indepent part after getting model
	restore_defaults(); // restore default if necessary
	init_nvram2();
#ifdef RTCONFIG_ATEUSB3_FORCE
	post_syspara(); // adjust nvram variable after restore_defaults
#endif

#ifdef RTCONFIG_ALLNOWL
	nvram_set("nowl", "1");				// tmp disable wl on 4709c
	nvram_set("ctf_disable_force", "1");		// tmp disable ctf on 4709c
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
#ifdef RTCONFIG_BCMARM
	nvram_set("apps_ipkg_old", "0");
	nvram_set("apps_install_folder", "asusware.arm");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/arm/stable");
#elif defined(RTCONFIG_QCA)
	nvram_set("apps_ipkg_old", "0");
	nvram_set("apps_install_folder", "asusware.mipsbig");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsbig/stable");
#else
	if(!strcmp(get_productid(), "VSL-N66U")){
		nvram_set("apps_ipkg_old", "0");
		nvram_set("apps_install_folder", "asusware.mipsbig");
		nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsbig/stable");
	}
	else{ // mipsel
		nvram_set("apps_install_folder", "asusware");
		if (nvram_match("apps_ipkg_old", "1"))
#ifdef RTCONFIG_HTTPS
			nvram_set("apps_ipkg_server", "https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT");
#else
			nvram_set("apps_ipkg_server", "http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT");
#endif
		else
			nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsel/stable");
	}
#endif
#endif

#if !defined(CONFIG_BCMWL5)	//Broadcom set this in check_wl_territory_code()
	void handle_location_code_for_wl(void);
	handle_location_code_for_wl();
#endif	/* CONFIG_BCMWL5 */

	init_gpio();   // for system dependent part
#ifdef RTCONFIG_SWMODE_SWITCH
	init_swmode(); // need to check after gpio initized
#endif
#ifdef RTCONFIG_BCMARM
	init_others();
#endif
	init_switch(); // for system dependent part
#if defined(RTCONFIG_QCA)
	set_uuid();
#if defined(QCA_WIFI_INS_RM)
	if(nvram_get_int("sw_mode")!=2)
#endif
	load_wifi_driver();
#endif
	if(!nvram_match("nowl", "1")) init_wl(); // for system dependent part
	klogctl(8, NULL, nvram_get_int("console_loglevel"));

	setup_conntrack();
	setup_timezone();
	if (!noconsole) xstart("console");

#ifdef RTCONFIG_BCMFA
	chk_etfa();
#endif
#ifdef RTCONFIG_GMAC3
	/* Ajuest GMAC3 NVRAM variables */
	gmac3_nvram_adjust();
#endif

#ifdef RTCONFIG_TUNNEL
	nvram_set("aae_support", "1");
#endif
}

#if defined(RTCONFIG_TEMPROOTFS)
static void sysinit2(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i;

	MOUNT("proc", "/proc", "proc", 0, NULL);
	MOUNT("tmpfs", "/tmp", "tmpfs", 0, NULL);

#ifdef LINUX26
	MOUNT("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
	MOUNT("devpts", "/dev/pts", "devpts", MS_MGC_VAL, NULL);
#endif

	if (console_init()) noconsole = 1;

	stime(&tm);

	f_write("/tmp/settings", NULL, 0, 0, 0644);
	symlink("/proc/mounts", "/etc/mtab");

	set_action(ACT_IDLE);

	if (!noconsole) {
		printf("\n\nHit ENTER for new console...\n\n");
		run_shell(1, 0);
	}

	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++) {
		signal(fatalsigs[i], handle_fatalsigs);
	}
	signal(SIGCHLD, handle_reap);

	if (!noconsole) xstart("console");
}
#else
static inline void sysinit2(void) { }
#endif

void
Alarm_Led(void) {
	while(1) {
		led_control(LED_POWER, LED_OFF);
		sleep(1);
		led_control(LED_POWER, LED_ON);
		sleep(1);
	}
}

int init_main(int argc, char *argv[])
{
	int state, i;
	sigset_t sigset;
	int rc_check, dev_check, boot_check; //Power on/off test
	int boot_fail, dev_fail, dev_fail_count, total_fail_check = 0;
	char reboot_log[128], dev_log[128];
	int ret, led_on_cut;

#if defined(RTCONFIG_TEMPROOTFS)
	if (argc >= 2 && !strcmp(argv[1], "reboot")) {
		state = SIGTERM;
		sysinit2();
	} else
#endif
	{
		sysinit();

		sigemptyset(&sigset);
		for (i = 0; i < sizeof(initsigs) / sizeof(initsigs[0]); i++) {
			sigaddset(&sigset, initsigs[i]);
		}
		sigprocmask(SIG_BLOCK, &sigset, NULL);

		start_jffs2();

#if defined(RTCONFIG_PSISTLOG)
	{ //set soft link for some app such as 3ginfo.sh
		char *syslog[]  = { "/tmp/syslog.log", "/tmp/syslog.log-1" };
		char *filename;
		int i;
		for(i = 0; i < 2; i++)
		{
			filename = get_syslog_fname(i);
			if(strcmp(filename, syslog[i]) != 0)
				eval("ln", "-s", filename, syslog[i]);
		}
	}
#endif

#ifdef RTN65U
		extern void asm1042_upgrade(int);
		asm1042_upgrade(1);	// check whether upgrade firmware of ASM1042
#endif

		run_custom_script("init-start", NULL);
		use_custom_config("fstab", "/etc/fstab");
		run_postconf("fstab", "/etc/fstab");
		state = SIGUSR2;	/* START */
	}

	for (;;) {
		TRACE_PT("main loop signal/state=%d\n", state);

		switch (state) {
		case SIGUSR1:		/* USER1: service handler */
			handle_notifications();
#ifdef RTCONFIG_16M_RAM_SFP
			force_free_caches();
#endif
			if (g_reboot) {
				/* If handle_notifications() run reboot rc_services,
				 * fall through to below statments to reboot.
				 */
				state = SIGTERM;
			} else
				break;

		case SIGHUP:		/* RESTART */
		case SIGINT:		/* STOP */
		case SIGQUIT:		/* HALT */
		case SIGTERM:		/* REBOOT */
#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		_dprintf("modem data: save the data during the signal %d.\n", state);
		eval("modem_status.sh", "bytes+");
#endif

#ifdef RTCONFIG_DSL_TCLINUX
			eval("req_dsl_drv", "reboot");
#endif
			stop_services();

			if (!g_reboot)
				stop_wan();

			stop_lan();
			stop_vlan();
			stop_logger();

			if ((state == SIGTERM /* REBOOT */) ||
				(state == SIGQUIT /* HALT */)) {
#ifdef RTCONFIG_USB
				remove_storage_main(1);
				if (!g_reboot) {
#if !defined(RTN56UB1)
					stop_usb();
					stop_usbled();
#endif
				}
#endif
				shutdn(state == SIGTERM /* REBOOT */);
				sync(); sync(); sync();
				exit(0);
			}
			if (state == SIGINT /* STOP */) {
				break;
			}

			// SIGHUP (RESTART) falls through

		case SIGUSR2:		/* START */
			start_logger();
			if (nvram_match("Ate_power_on_off_enable", "1")) {
				rc_check = nvram_get_int("Ate_rc_check");
				boot_check = nvram_get_int("Ate_boot_check");
				boot_fail = nvram_get_int("Ate_boot_fail");
				total_fail_check = nvram_get_int("Ate_total_fail_check");
dbG("rc/boot/total chk= %d/%d/%d, boot/total fail= %d/%d\n", rc_check,boot_check,total_fail_check, boot_fail,total_fail_check);
				if ( nvram_get_int("Ate_FW_err") >= nvram_get_int("Ate_fw_fail")) {
					ate_commit_bootlog("6");
					dbG("*** Bootloader read FW fail: %d ***\n", nvram_get_int("Ate_FW_err"));
					Alarm_Led();
				}

				if (rc_check != boot_check) {
					if (nvram_get("Ate_reboot_log")==NULL)
						sprintf(reboot_log, "%d,", rc_check);
					else
						sprintf(reboot_log, "%s%d,", nvram_get("Ate_reboot_log"), rc_check);
					nvram_set("Ate_reboot_log", reboot_log);
					nvram_set_int("Ate_boot_fail", ++boot_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
dbg("boot/continue fail= %d/%d\n", nvram_get_int("Ate_boot_fail"),nvram_get_int("Ate_continue_fail"));
					if (boot_fail >= nvram_get_int("Ate_continue_fail")) { //Show boot fail!!
						ate_commit_bootlog("4");
						dbG("*** boot fail continuelly: %s ***\n", reboot_log);
						while(1) {
							led_control(LED_POWER, LED_OFF);
							sleep(1);
							led_control(LED_POWER, LED_ON);
							sleep(1);
						}
					}
					else
					dbg("rc OK\n");
				}

				if (total_fail_check == nvram_get_int("Ate_total_fail")) {
					ate_commit_bootlog("5");
					dbG("*** Total reboot fail: %d times!!!! ***\n", total_fail_check);
					while(1) {
						led_control(LED_POWER, LED_OFF);
						sleep(1);
						led_control(LED_POWER, LED_ON);
						sleep(1);
					}
				}

				rc_check++;
				nvram_set_int("Ate_rc_check", rc_check);
				nvram_commit();
				dbG("*** Start rc: %d\n",rc_check);
			}
#ifdef RTCONFIG_USB
			start_sd_idle();
#endif
#ifdef RTCONFIG_IPV6
			if ( !(ipv6_enabled() && is_routing_enabled()) )
				f_write_string("/proc/sys/net/ipv6/conf/all/disable_ipv6", "1", 0, 0);
#endif
			start_vlan();
#ifdef RTCONFIG_DSL
			start_dsl();
#endif
			start_lan();
#ifdef RTCONFIG_QTN
			start_qtn();
			sleep(5);
#endif
			misc_ioctrl();

			start_services();
#ifdef CONFIG_BCMWL5
			if (restore_defaults_g)
			{
				restore_defaults_g = 0;

				switch (get_model()) {
				case MODEL_RTAC66U:
				case MODEL_RTAC56S:
				case MODEL_RTAC56U:
				case MODEL_RTAC68U:
				case MODEL_DSLAC68U:
				case MODEL_RTAC87U:
				case MODEL_RTN12HP:
				case MODEL_RTN12HP_B1:
				case MODEL_RTN66U:
				case MODEL_RTN18U:
				//case MODEL_RTAC5300:
				//case MODEL_RTAC3100:
				//case MODEL_RTAC88U:
				//case MODEL_RTAC1200G:
					set_wltxpower();
					break;
				default:
					if (nvram_contains_word("rc_support", "pwrctrl"))
						set_wltxpower();
					break;
				}

				restart_wireless();
#ifdef RTCONFIG_QTN
				/* add here since tweak wireless-related flow */
				/* RT-AC87U#818, RT-AC87U#819 */
				start_qtn_monitor();
#endif
			}
			else
#endif
			{
				start_wl();
				lanaccess_wl();
				nvram_set_int("wlready", 1);
			}
			start_wan();

#ifdef RTCONFIG_QTN	/* AP and Repeater mode, workaround to infosvr, RT-AC87U bug#38, bug#44, bug#46 */
			if (nvram_get_int("sw_mode") == SW_MODE_REPEATER ||
				nvram_get_int("sw_mode") == SW_MODE_AP)
			{
				eval("ifconfig", "br0:0", "169.254.39.3", "netmask", "255.255.255.0");
				if (!nvram_match("QTN_RPC_CLIENT", ""))
					eval("ifconfig", "br0:0", nvram_safe_get("QTN_RPC_CLIENT"), "netmask", "255.255.255.0");
				else
					eval("ifconfig", "br0:0", "169.254.39.1", "netmask", "255.255.255.0");
			}
#endif

#ifdef REMOVE
// TODO: is it a special case need to handle?
			if (wds_enable()) {
				/* Restart NAS one more time - for some reason without
				 * this the new driver doesn't always bring WDS up.
				 */
				stop_nas();
				start_nas();
			}
#endif
#ifdef RTCONFIG_RALINK
			if(nvram_match("Ate_wan_to_lan", "1"))
			{
				printf("\n\n## ATE mode:set WAN to LAN... ##\n\n");
				set_wantolan();
				modprobe_r("hw_nat");
				modprobe("hw_nat");
				stop_wanduck();
				killall_tk("udhcpc");
			}
#endif

			if (nvram_match("Ate_power_on_off_enable", "3")|| //Show alert light
				nvram_match("Ate_power_on_off_enable", "4")||
				nvram_match("Ate_power_on_off_enable", "5")  ) {
				start_telnetd();
				while(1) {
					led_control(LED_POWER, LED_OFF);
					sleep(1);
					led_control(LED_POWER, LED_ON);
					sleep(1);
				}
			}

			//For 66U normal boot & check device
			if (((get_model()==MODEL_RTN66U) || (get_model()==MODEL_RTAC66U) || (get_model()==MODEL_RTAC5300))
			&& nvram_match("Ate_power_on_off_enable", "0")) {
			    if (nvram_get_int("dev_fail_reboot")!=0) {
				ate_dev_status();
				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					dev_fail_count = nvram_get_int("dev_fail_count");
					dev_fail_count++;
					if (dev_fail_count < nvram_get_int("dev_fail_reboot")) {
						nvram_set_int("dev_fail_count", dev_fail_count);
						nvram_commit();
						dbG("device failed %d times, reboot...\n",dev_fail_count);
						sleep(1);
						reboot(RB_AUTOBOOT);
					}
					else {
						nvram_set("dev_fail_count", "0");
						nvram_commit();
					}
				}
				else {
					if (!nvram_match("dev_fail_count", "0")) {
						nvram_set("dev_fail_count", "0");
						nvram_commit();
					}
				}
			    }
			}

			if (nvram_match("Ate_power_on_off_enable", "1")) {
				dev_check = nvram_get_int("Ate_dev_check");
				dev_fail = nvram_get_int("Ate_dev_fail");
				ate_dev_status();

				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					nvram_set_int("Ate_dev_fail", ++dev_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
					if (nvram_get("Ate_dev_log")==NULL)
						sprintf(dev_log, "%d,", dev_check);
					else
						sprintf(dev_log, "%s%d,", nvram_get("Ate_dev_log"), dev_check);
					nvram_set("Ate_dev_log", dev_log);
					dbG("dev Fail at %d, status= %s\n", dev_check, nvram_get("Ate_dev_status"));
					dbG("dev/total fail= %d/%d\n", dev_fail, total_fail_check);
					if (dev_fail== nvram_get_int("Ate_continue_fail")) {
						ate_commit_bootlog("3");
						dbG("*** dev fail continuelly: %s ***\n", dev_log);
						while(1) {
						led_control(LED_POWER, LED_OFF);
						sleep(1);
						led_control(LED_POWER, LED_ON);
						sleep(1);
						}
					}
				}
				else {
					dev_check++;
					nvram_set_int("Ate_dev_check", dev_check);
					nvram_set("Ate_dev_fail", "0");
					dbG("*** dev Up: %d\n", dev_check);
				}

				boot_check = nvram_get_int("Ate_boot_check");
				boot_check++;
				nvram_set_int("Ate_boot_check", boot_check);
				nvram_set_int("Ate_rc_check", boot_check);
				nvram_set("Ate_boot_fail", "0");

				if (boot_check < nvram_get_int("Ate_reboot_count")) {
					int delay = nvram_get_int("Ate_reboot_delay");
					nvram_commit();
					dbG("System boot up %d times,  delay %d secs & reboot...\n", boot_check, delay);
					sleep(delay);
					reboot(RB_AUTOBOOT);
				}
				else {
					dbG("System boot up success %d times\n", boot_check);
#ifdef RTCONFIG_BCMARM
					ate_commit_bootlog("2");
					for(led_on_cut=0; led_on_cut<6; led_on_cut++) {
						sleep(5);
						setATEModeLedOn();
					}
#else
					ate_commit_bootlog("2");
					for(led_on_cut=0; led_on_cut<6; led_on_cut++) {
						sleep(5);
						eval("ATE", "Set_StartATEMode"); /* Enter ATE Mode to keep led on */
						if (get_model() == MODEL_RTAC53U) {
							led_control(LED_POWER, LED_ON);
							led_control(LED_USB, LED_ON);
						}
						else
							setAllLedOn();
					}
#endif
#if defined(RTCONFIG_RALINK) && defined(RTN65U)
					ate_run_in();
#endif
					eval("arpstorm");
				}
			}

/*#ifdef RTCONFIG_USB_MODEM
			if (is_usb_modem_ready() != 1 && nvram_match("ctf_disable_modem", "1")) {
				nvram_set("ctf_disable_modem", "0");
				nvram_commit();
				notify_rc_and_wait("reboot");
				return 0;
			}
#endif// */
			if (nvram_get_int("led_disable")==1)
				setup_leds();

#ifdef RTCONFIG_USBRESET
			set_pwr_usb(1);
#else
#if defined (RTCONFIG_USB_XHCI)
			if (nvram_get_int("usb_usb3") == 0 &&
				nvram_get("pwr_usb_gpio") &&
				(nvram_get_int("pwr_usb_gpio") & 0xFF) != 0xFF)
			{
#ifndef RTCONFIG_XHCIMODE
				_dprintf("USB2: reset USB power to call up the device on the USB 3.0 port.\n");
				set_pwr_usb(0);
				sleep(2);
				set_pwr_usb(1);
#endif
			}
#endif
#endif
#ifdef RTCONFIG_USB
			int fd = -1;
			fd = file_lock("usb");  // hold off automount processing
			start_usb();
			start_usbled();
			file_unlock(fd);	// allow to process usb hotplug events
			/*
			 * On RESTART some partitions can stay mounted if they are busy at the moment.
			 * In that case USB drivers won't unload, and hotplug won't kick off again to
			 * remount those drives that actually got unmounted. Make sure to remount ALL
			 * partitions here by simulating hotplug event.
			 */
			if (state == SIGHUP /* RESTART */)
				add_remove_usbhost("-1", 1);

#ifdef RTCONFIG_USB_PRINTER
			start_usblpsrv();
#endif
#ifdef RTCONFIG_BCMARM
			hotplug_usb_init();
#endif
#endif

			nvram_set("success_start_service", "1");

			force_free_caches();

if (nvram_match("commit_test", "1")) {
	int x=0;
	while(1) {
		x++;
		dbG("Nvram commit: %d\n", x);
		nvram_set_int("commit_cut", x);
		nvram_commit();
		if (nvram_get_int("commit_cut")!=x) {
			TRACE_PT("\n\n======== NVRAM Commit Failed at: %d =========\n\n", x);
			break;
		}
	}
}
			break;
		}

		chld_reap(0);		/* Periodically reap zombies. */
		check_services();

		do {
		ret = sigwait(&sigset, &state);
		} while (ret);
#ifdef RTCONFIG_BCMARM
		/* free pagecache */
		if (nvram_get_int("drop_caches"))
			f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
#endif
	}

	return 0;
}

int reboothalt_main(int argc, char *argv[])
{
	int reboot = (strstr(argv[0], "reboot") != NULL);
	int def_reset_wait = 20;

	_dprintf(reboot ? "Rebooting..." : "Shutting down...");
	kill(1, reboot ? SIGTERM : SIGQUIT);

#if defined(RTN14U) || defined(RTN65U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTCONFIG_QCA) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
	def_reset_wait = 50;
#endif

	/* In the case we're hung, we'll get stuck and never actually reboot.
	 * The only way out is to pull power.
	 * So after 'reset_wait' seconds (default: 20), forcibly crash & restart.
	 */
	if (fork() == 0) {
		int wait = nvram_get_int("reset_wait") ? : def_reset_wait;
		if ((wait < 10) || (wait > 120)) wait = 10;

		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(wait);
		_dprintf("Still running... Doing machine reset.\n");
#ifdef RTCONFIG_USB
		remove_usb_module();
#endif
		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(1);
		f_write("/proc/sysrq-trigger", "b", 1, 0 , 0); /* machine reset */
	}

	return 0;
}

