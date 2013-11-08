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

#include <shared.h>
#include <stdio.h>
#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#include <disk_share.h>
#endif

#ifdef RTCONFIG_BCMWL6A
#include <etioctl.h>
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
#ifdef RTCONFIG_RALINK
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
	nvram_unset("wps_proc_status");
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

	strcpy(macstr, nvram_safe_get("et0macaddr"));
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
			nvram_unset(strcat_r(prefix, "bss_maxassoc", tmp));
			nvram_unset(strcat_r(prefix, "wme_bss_disable", tmp));
			nvram_unset(strcat_r(prefix, "ifname", tmp));
			nvram_unset(strcat_r(prefix, "unit", tmp));
			nvram_unset(strcat_r(prefix, "ap_isolate", tmp));
			nvram_unset(strcat_r(prefix, "macmode", tmp));
			nvram_unset(strcat_r(prefix, "maclist", tmp));
			nvram_unset(strcat_r(prefix, "maxassoc", tmp));
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
			nvram_unset(strcat_r(prefix, "sta_retry_time", tmp));
			nvram_unset(strcat_r(prefix, "wfi_enable", tmp));
			nvram_unset(strcat_r(prefix, "wfi_pinmode", tmp));
			nvram_unset(strcat_r(prefix, "wme", tmp));
			nvram_unset(strcat_r(prefix, "wmf_bss_enable", tmp));
			nvram_unset(strcat_r(prefix, "wps_mode", tmp));
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
			if (strncmp(t->name, "wl_", 3)!=0) continue;
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
			if(nvram_get_int("sw_mode") == SW_MODE_REPEATER) {
				if(nvram_get_int("wlc_band")==unit && subunit==1) {
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
				pssid = nvram_safe_get(strcat_r(pprefix, "ssid", tmp));
				if(strlen(pssid))
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
				nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
				nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
				nvram_set(strcat_r(prefix, "key", tmp), "1");
				nvram_set(strcat_r(prefix, "key1", tmp), "");
				nvram_set(strcat_r(prefix, "key2", tmp), "");
				nvram_set(strcat_r(prefix, "key3", tmp), "");
				nvram_set(strcat_r(prefix, "key4", tmp), "");
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), "");

				nvram_set(strcat_r(prefix, "ap_isolate", tmp), "0");
				nvram_set(strcat_r(prefix, "bridge", tmp), "");
				nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), "128");
				nvram_set(strcat_r(prefix, "closed", tmp), "0");
				nvram_set(strcat_r(prefix, "infra", tmp), "1");
				nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
				nvram_set(strcat_r(prefix, "maxassoc", tmp), "128");
				nvram_set(strcat_r(prefix, "mode", tmp), "ap");
				nvram_set(strcat_r(prefix, "net_reauth", tmp), "3600");
				nvram_set(strcat_r(prefix, "preauth", tmp), "");
				nvram_set(strcat_r(prefix, "radio", tmp), "1");
				nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), "");
				nvram_set(strcat_r(prefix, "radius_key", tmp), "");
				nvram_set(strcat_r(prefix, "radius_port", tmp), "1812");
				nvram_set(strcat_r(prefix, "sta_retry_time", tmp), "5");
				nvram_set(strcat_r(prefix, "wfi_enable", tmp), "0");
				nvram_set(strcat_r(prefix, "wfi_pinmode", tmp), "0");
				nvram_set(strcat_r(prefix, "wme", tmp), "on");
				nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), "0");
#ifdef RTCONFIG_EMF
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp),
					nvram_get(strcat_r(pprefix, "wmf_bss_enable", tmp)));
#else
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), "0");
				nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");

				nvram_set(strcat_r(prefix, "expire", tmp), "0");
				nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");
			}
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
			if (nvram_get_int("sw_mode") == SW_MODE_ROUTER)		// Only limite access of Guest network in Router mode
			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				if(strcmp(word, "rai0") == 0)
				{
					int vlan_id;
					if (nvram_match(strcat_r(prefix, "lanaccess", tmp), "off"))
						vlan_id = inic_vlan_id++;	// vlan id for no LAN access
					else
						vlan_id = 0;			// vlan id allow LAN access
					if(vlan_id)
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
		sprintf(lan_ifnames, "%s %s", nvram_safe_get("lan_ifnames"), wl_vifnames);
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
		if(strncmp(t->name, "wl_", 3) &&
			strncmp(t->name, prefix, 4))
			continue;
		if(!strcmp(t->name, "wl_ifname"))	// not to clean wl%d_ifname
			continue;

		if(!strncmp(t->name, "wl_", 3))
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
		else
			nvram_set(t->name, t->value);
	}
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
}

/* assign none-exist value or inconsist value */
void
lan_defaults(void)
{
	if(nvram_get_int("sw_mode")==SW_MODE_ROUTER && nvram_invmatch("lan_proto", "static"))
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

	unit = WAN_UNIT_FIRST;
	foreach (word, nvram_safe_get("wan_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		nvram_set(strcat_r(prefix, "ifname", tmp), word);

#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DUALWAN
		if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
#else
		if(unit == WAN_UNIT_SECOND)
#endif
		{
			nvram_set(strcat_r(prefix, "ifname", tmp), "");
			//nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), "");
		}
#endif

		for (t = router_defaults; t->name; t++) {
			if(strncmp(t->name, "wan_", 4)!=0) continue;

			if(!nvram_get(strcat_r(prefix, &t->name[4], tmp))){
				//_dprintf("_set %s = %s\n", tmp, t->value);
				nvram_set(tmp, t->value);
			}
		}

		++unit;
	}
}

void
restore_defaults_module(char *prefix)
{
	struct nvram_tuple *t;

	if(strcmp(prefix, "wl_")==0) wl_defaults();
	else if(strcmp(prefix, "wan_")==0) wan_defaults();
	else {
		for (t = router_defaults; t->name; t++) {
			if(strncmp(t->name, prefix, sizeof(prefix))!=0) continue;
			nvram_set(t->name, t->value);
		}
	}
}

#ifdef CONFIG_BCMWL5
int restore_defaults_g = 0;

// increase this number each time need to reset power value
#define WLPWRVER_RTN66U 1

void wlpwr_default()
{
	char tmp[100], prefix[]="wlXXXXXX_";
	int unit;
	int wlpwrver;

	if(get_model()==MODEL_RTN66U) {
		wlpwrver = nvram_get_int("wlpwrver");
		if(wlpwrver!=WLPWRVER_RTN66U) {
			nvram_set_int("wlpwrver", WLPWRVER_RTN66U);
			nvram_unset("wl_TxPower");
			for (unit = 0; unit < MAX_NVPARSE; unit++) {
				sprintf(prefix, "wl%d_", unit);
				nvram_unset(strcat_r(prefix, "TxPower", tmp));
			}
		}
	}
}
#endif

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
	if(usbctrlver != USBCTRLVER || nvram_get("acc_username0") != NULL){
		nvram_set_int("usbctrlver", USBCTRLVER);
		acc_num = nvram_get_int("acc_num");
		acc_num_ret = 0;

		char ascii_passwd[64];

		memset(ascii_passwd, 0, 64);
		char_to_ascii_safe(ascii_passwd, nvram_safe_get("http_passwd"), 64);
		buf_len = snprintf(tmp, sizeof(tmp), "%s>%s", nvram_safe_get("http_username"), ascii_passwd); // default account.
		ptr = tmp+buf_len;
		acc_num_ret = 1;

		for(i = 0; i < acc_num; i++){
			sprintf(acc_nvram_username, "acc_username%d", i);
			if((acc_user = nvram_get(acc_nvram_username)) == NULL) continue;

			sprintf(acc_nvram_password, "acc_password%d", i);
			if((acc_password = nvram_get(acc_nvram_password)) == NULL) continue;
			memset(ascii_passwd, 0, 64);
			char_to_ascii_safe(ascii_passwd, acc_password, 64);

			if(strcmp(acc_user, "admin")){
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
	if(acc_num >= 0 && acc_num != nvram_get_int("acc_num"))
		nvram_set_int("acc_num", acc_num);
	free_2_dimension_list(&acc_num, &account_list);
}
#endif

/* ASUS use erase nvram to reset default only */
static void
restore_defaults(void)
{
	struct nvram_tuple *t;
	int restore_defaults;
	char tmp[100], prefix[]="wanXXXXXX_";
	int unit;
	int model;

	// Restore defaults if nvram version mismatch
	restore_defaults = !nvram_match("nvramver", RTCONFIG_NVRAM_VER);

	/* Restore defaults if told to or OS has changed */
	if(!restore_defaults)
	{
		restore_defaults = !nvram_match("restore_defaults", "0");
#ifdef RTCONFIG_RALINK
		/* upgrade from firmware 1.x.x.x */
		if ((get_model() == MODEL_RTN56U) &&
			(nvram_get("HT_AutoBA") != NULL))
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

#ifdef CONFIG_BCMWL5
	restore_defaults_g = restore_defaults;
#endif
	nvram_set_int("wlready", 0);
	if (restore_defaults) {
#ifdef RTCONFIG_WPS
		wps_restore_defaults();
#endif
		virtual_radio_restore_defaults();
	}


#ifdef CONFIG_BCMWL5
	wlpwr_default();
#endif

#ifdef RTCONFIG_USB
	usbctrl_default();
#endif

	/* Restore defaults */
	for (t = router_defaults; t->name; t++) {
		if (restore_defaults || !nvram_get(t->name)) {	
			// add special default value handle here		
			if(strcmp(t->name, "computer_name")==0) 
				nvram_set(t->name, get_productid());
			else if(strcmp(t->name, "ct_max")==0) {
				// handled in init_nvram already
			}
			else nvram_set(t->name, t->value);			
		}
	}

	wl_defaults();
	lan_defaults();
	wan_defaults();
#ifdef RTCONFIG_DSL
	dsl_defaults();
#endif

	if (restore_defaults) {
		uint memsize = 0;
		char pktq_thresh[8] = {0};

		memsize = get_meminfo_item("MemTotal");

		if (memsize <= 32768) {
			/* Set to 512 as long as onboard memory <= 32M */
			sprintf(pktq_thresh, "512");
		}
		else {
			/* We still have to set the thresh to prevent oom killer */
			sprintf(pktq_thresh, "1024");
		}

		nvram_set("wl_txq_thresh", pktq_thresh);
		nvram_set("et_txq_thresh", pktq_thresh);
#if defined(__CONFIG_USBAP__)
		nvram_set("wl_rpcq_rxthresh", pktq_thresh);
#endif
#if 0
#ifdef RTCONFIG_BCMWL6
		if (nvram_match("wl1_bw", "0"))
		{
			nvram_set("wl1_bw", "3");

			if (nvram_match("wl1_country_code", "EU"))
				nvram_set("wl1_chanspec", "36/80");
			else
				nvram_set("wl1_chanspec", "149/80");
		}
#endif
#endif
#ifdef RTCONFIG_USB_XHCI
		if ((nvram_get_int("wlopmode") == 7) || nvram_match("ATEMODE", "1"))
			nvram_set("usb_usb3", "1");
#endif
		if (nvram_get_int("wlopmode") == 7){
			switch (get_model()) {
				case MODEL_RTAC56U:
					nvram_set("clkfreq", "800,666");
					break;
				case MODEL_RTAC68U:
					if (!nvram_match("bl_version", "1.0.1.1"))
					nvram_set("clkfreq", "800,666");
                        		break;
			}
		}
	}

	/* Commit values */
	if (restore_defaults) {
		nvram_commit();		
		fprintf(stderr, "done\n");
	}
#ifdef RTCONFIG_BCMARM
	if (!nvram_match("extendno_org", nvram_safe_get("extendno")))
	{
		dbg("Reset TxBF settings...\n");
		nvram_set("extendno_org", nvram_safe_get("extendno"));
		nvram_set("wl_txbf", "1");
		nvram_set("wl0_txbf", "1");
		nvram_set("wl1_txbf", "1");
		nvram_set("wl_itxbf", "1");
		nvram_set("wl0_itxbf", "1");
		nvram_set("wl1_itxbf", "1");
		nvram_commit();
	}
#endif
	/* default for state control variables */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		
		for (t = router_state_defaults; t->name; t++) {
			if(!strncmp(t->name, "wan_", 4))
				nvram_set(strcat_r(prefix, &t->name[4], tmp), t->value);
			else if(unit == WAN_UNIT_FIRST) // let non-wan nvram to be set one time.
				nvram_set(t->name, t->value);
		}
	}

	/* some default state values is model deps, so handled here*/
	model = get_model();

	switch(model) {
		case MODEL_RTN14UHP:
			nvram_set("reboot_time", "85"); // default is 60 sec
			break;
		case MODEL_RTN10U:
			nvram_set("reboot_time", "85"); // default is 60 sec
			break;
#ifdef RTCONFIG_RALINK
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
			nvram_set("reboot_time", "75"); // default is 60 sec
			break;
#else
		case MODEL_RTAC52U:
			nvram_set("reboot_time", "80"); // default is 60 sec
			break;
		case MODEL_RTN65U:
			nvram_set("reboot_time", "90"); // default is 60 sec
			break;
#endif
#endif
		default:
			break;
	}

#ifdef RTCONFIG_WEBDAV
	webdav_account_default();
#endif

	nvram_set("success_start_service", "0");
#if defined(RTAC66U) || defined(BCM4352)
	nvram_set("led_5g", "0");
#endif

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

/*
	if (restore_defaults)
	{
		nvram_set("jffs2_clean_fs", "1");
	}
*/
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
		if (((act = check_action()) == ACT_IDLE) || (act == ACT_REBOOT)) break;
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
	if(!nvram_get_int("swmode_switch")) return;
	
	if(button_pressed(BTN_SWMODE_SW_REPEATER)){
		nvram_set_int("sw_mode", SW_MODE_REPEATER);
		dbg("%s: swmode: repeater", LOGNAME);
		if(nvram_match("x_Setting", "0")){
			nvram_set("lan_proto", "dhcp");
		}
	}else if(button_pressed(BTN_SWMODE_SW_AP)){
		nvram_set_int("sw_mode", SW_MODE_AP);
		dbg("%s: swmode: ap", LOGNAME);
		if(nvram_match("x_Setting", "0")){
			nvram_set("lan_proto", "dhcp");
		}
	}else if(button_pressed(BTN_SWMODE_SW_ROUTER)){
		dbg("%s: swmode: router", LOGNAME);
		nvram_set_int("sw_mode", SW_MODE_ROUTER);
		nvram_set("lan_proto", "static");
	}else{
		dbg("%s: swmode: unknow swmode", LOGNAME);
	}
}
#endif

#define GPIO_ACTIVE_LOW 0x1000

void conf_swmode_support(int model)
{
	switch (model) {
		case MODEL_RTAC66U:
		case MODEL_RTAC53U:
			nvram_set("swmode_support", "router repeater ap psta");
			dbg("%s: swmode: router repeater ap psta", LOGNAME);
			break;
		case MODEL_APN12HP:
			nvram_set("swmode_support", "repeater ap");
			dbg("%s: swmode: repeater ap", LOGNAME);
			break;
		default:
			nvram_set("swmode_support", "router repeater ap");
			dbg("%s: swmode: router repeater ap", LOGNAME);
			break;
	}
}

// intialized in this area
//  lan_ifnames
//  wan_ifnames
//  wl_ifnames
//  btn_xxxx_gpio
//  led_xxxx_gpio

int init_nvram(void)
{
	int model = get_model();
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_DUALWAN)
	int unit = 0;
#endif
#if defined(RTCONFIG_DUALWAN) || defined(CONFIG_BCMWL5)
	char wan_if[10];
#endif

	TRACE_PT("init_nvram for %d\n", model);

	/* set default value */
	nvram_unset(ASUS_STOP_COMMIT);
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
	/* In the order of physical placement */
	nvram_set("ehci_ports", "");
	nvram_set("ohci_ports", "");

	/* treat no phy, no internet as disconnect */
	nvram_set("web_redirect", "3");

	conf_swmode_support(model);

#ifdef RTCONFIG_OPENVPN
	nvram_set("vpn_server1_state", "0");
	nvram_set("vpn_server2_state", "0");
	nvram_set("vpn_client1_state", "0");
	nvram_set("vpn_client2_state", "0");
	nvram_set("vpn_server1_errno", "0");
	nvram_set("vpn_server2_errno", "0");
	nvram_set("vpn_client1_errno", "0");
	nvram_set("vpn_client2_errno", "0");
	nvram_set("vpn_upload_state", "");
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
		//if(!nvram_get("ct_max")) 
		//	nvram_set("ct_max", "30000");
		if(nvram_get("sw_mode")==NULL||nvram_get_int("sw_mode")==SW_MODE_ROUTER)
			nvram_set_int("sw_mode", SW_MODE_REPEATER);
		add_rc_support("mssid 2.4G 5G update");
		break;

	case MODEL_RTN65U:
		nvram_set("boardflags", "0x100");	// although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "vlan1 rai0 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "vlan1 rai0 ra0");
			nvram_set("wan_ifnames", "vlan2");
		}
		nvram_set("wl_ifnames","rai0 ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
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
		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
	case MODEL_RTN67U:
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "eth2 eth3 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "eth2 ra0");
			nvram_set("wan_ifnames", "eth3");
		}
		nvram_set("wl_ifnames","ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
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
		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
	case MODEL_RTN36U3:
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "eth2 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "eth2 ra0");
			nvram_set("wan_ifnames", "eth3");
		}
		nvram_set("wl_ifnames","ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;

	case MODEL_RTN56U:
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
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
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#if defined(RTN14U)
	case MODEL_RTN14U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "vlan1 vlan2 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "vlan1 ra0");
			nvram_set("wan_ifnames", "vlan2");
		}
		nvram_set("wl_ifnames","ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "apcli0 wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 41|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 40|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
//		if(!nvram_get("ct_max")) 
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

#if defined(RTAC52U)
	case MODEL_RTAC52U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
			nvram_set("lan_ifnames", "vlan1 vlan2 ra0 rai0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "vlan1 ra0 rai0");
			nvram_set("wan_ifnames", "vlan2");
		}
		nvram_set("wl_ifnames","ra0 rai0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

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
		/* FIXME: RADIO ON/OFF BTN */

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "30000");

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
#endif

#ifdef RTCONFIG_DSL
	case MODEL_DSLN55U:
		nvram_set("lan_ifname", "br0");

		if(nvram_get_int("sw_mode")==SW_MODE_AP) {
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
		if(get_dualwan_secondary()==WANS_DUALWAN_IF_NONE) {
			// dsl, lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1");
			}
			else if(get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4");
			}
		}
		else if(get_dualwan_secondary()==WANS_DUALWAN_IF_LAN) {
			//dsl/lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1 eth2.4");
			}
#ifdef RTCONFIG_DSL
			/* Paul add 2013/1/24 */
			//usb/lan
			else {
				nvram_set("wan_ifnames", "eth2.1 eth2.4");
			}
#endif
		}
		else {
			//dsl/usb, lan/usb
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1");
			}
			else if(get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4");
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

		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("dsl");
#if defined(RTCONFIG_WIRELESS_SWITCH) || defined(RTCONFIG_WIFI_TOG_BTN)
		add_rc_support("wifi_hw_sw");
#endif
		add_rc_support("pwrctrl");

		//Ren: set "spectrum_supported" in /router/dsl_drv_tool/tp_init/tp_init_main.c
		if(nvram_match("spectrum_supported", "1"))
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
#endif // RTCONFIG_RALINK

#ifdef CONFIG_BCMWL5
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
		if(nvram_match("AUTO_CHANNEL", "1")){
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
		nvram_set_int("sb/1/ledbh5", 2);			/* is active_high? set 7 then */
		add_rc_support("pwrctrl");
#ifdef RTCONFIG_WL_AUTO_CHANNEL
		if(nvram_match("AUTO_CHANNEL", "1")){
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
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wandevs", "et0");
		nvram_set("wl_ifnames", "eth1");

		break;

	case MODEL_RTN15U:
		nvram_set("lan_ifname", "br0");

#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
					add_wan_phy("vlan3");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "15000");
		add_rc_support("2.4G update mssid usbX1 nomedia");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;

	case MODEL_RTN16:
		nvram_set("lan_ifname", "br0");

#ifdef RTCONFIG_DUALWAN
		if(get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					if(get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "");
		nvram_set_int("btn_rst_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-2 1-1");
		nvram_set("ohci_ports", "2-2 2-1");
		nvram_set("boardflags", "0x310");
		nvram_set("sb/1/boardflags", "0x310");
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "300000");
		add_rc_support("2.4G update usbX2 mssid");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;

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
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");

		add_rc_support("2.4G 5G update mssid no5gmssid");
#ifdef RTCONFIG_WLAN_LED
		add_rc_support("led_2g");
		nvram_set("led_5g", "1");
#endif
		break;

	case MODEL_RTN18U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0");
		if(nvram_match("usb_usb3_disabled_force", "0"))
			nvram_set("usb_usb3", "1");
		else
			nvram_set("usb_usb3", "0");
#ifdef RTCONFIG_DUALWAN
		if(get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					if(get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		nvram_set_int("led_usb_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 10|GPIO_ACTIVE_LOW);

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if(nvram_get_int("usb_usb3") == 1){
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

		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
#ifdef RTCONFIG_WLAN_LED
					add_rc_support("led_2g");
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
		break;

	case MODEL_RTAC68U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "vlan1 wl0 wl1");
#ifdef RTCONFIG_DUALWAN
		if(get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");
		if(!(get_wans_dualwan()&WANSCAP_5G))
			add_lan_phy("eth2");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					if(get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
					add_wan_phy("eth2");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif

		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
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
		if(nvram_get_int("usb_usb3") == 1){
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

		if(!nvram_get("ct_max"))
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

		break;

	case MODEL_RTAC56U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("lan_ifname", "br0");
		nvram_set("0:ledbh3", "0x87");	  /* since 163.42 */
		nvram_set("1:ledbh10", "0x87");
		nvram_set("landevs", "vlan1 wl0 wl1");
#ifdef RTCONFIG_DUALWAN
		if(get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");
		if(!(get_wans_dualwan()&WANSCAP_5G))
			add_lan_phy("eth2");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					if(get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
					add_wan_phy("eth2");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif

		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("pwr_usb_gpio2", 10|GPIO_ACTIVE_LOW);	// Use at the first shipment of RT-AC56U.
#else
		nvram_set_int("pwr_usb_gpio", 9);
		nvram_set_int("pwr_usb_gpio2", 10);	// Use at the first shipment of RT-AC56U.
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
		if(nvram_get_int("usb_usb3") == 1){
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

		if(!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		break;

	case MODEL_RTN66U:
		nvram_set_int("btn_radio_gpio", 13|GPIO_ACTIVE_LOW);	// ? should not used by Louis
		if(nvram_match("pci/1/1/ccode", "JP") && nvram_match("pci/1/1/regrev", "42") && !nvram_match("watchdog", "20000")){
			_dprintf("force set watchdog as 20s\n");
			nvram_set("watchdog", "20000");
			nvram_commit();
			reboot(0);
		}

	case MODEL_RTAC66U:
		nvram_set("lan_ifname", "br0");

#ifdef RTCONFIG_DUALWAN
		if(get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
		else
			nvram_set("wandevs", "et0");

		set_lan_phy("vlan1");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");
		if(!(get_wans_dualwan()&WANSCAP_5G))
			add_lan_phy("eth2");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					if(get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
					add_wan_phy("eth2");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif

		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 13|GPIO_ACTIVE_LOW);	// tmp use
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("fan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("have_fan_gpio", 6|GPIO_ACTIVE_LOW);
		if(nvram_get("regulation_domain")) {
			nvram_set("ehci_ports", "1-1.2 1-1.1 1-1.4");
			nvram_set("ohci_ports", "2-1.2 2-1.1 2-1.4");
		}
		else {
			nvram_set("ehci_ports", "1-1.2 1-1.1");
			nvram_set("ohci_ports", "2-1.2 2-1.1");
		}
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
#ifdef RTCONFIG_OPTIMIZE_XBOX
		add_rc_support("optimize_xbox");
#endif
		add_rc_support("WIFI_LOGO");
		break;

	case MODEL_RTN14UHP:
		nvram_set("lan_ifname", "br0");
#ifdef RTCONFIG_LAN4WAN_LED
		if (nvram_match("odmpid", "TW")) {
			add_rc_support("lanwan_led2");
		}
#endif

#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan0");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
					add_wan_phy("vlan3");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		if(nvram_match("AUTO_CHANNEL", "1")){
			nvram_set("wl_channel", "6");
			nvram_set("wl0_channel", "6");
		}
#endif
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G mssid media usbX1 update");
		break;

	case MODEL_RTN10U:
		nvram_set("lan_ifname", "br0");

#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan0");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
					add_wan_phy("vlan2");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan1");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set_int("led_usb_gpio", 8);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G mssid usbX1 update nomedia");
		break;

	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
		nvram_set("lan_ifname", "br0");
#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan0");

		if(!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if(nvram_get("wans_dualwan")){
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
					add_wan_phy("vlan2");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")){
						sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
						add_wan_phy(wan_if);
					}
					else if(get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan1");
					else
						add_wan_phy("eth0");
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
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
		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set("sb/1/maxp2ga0", "0x52");
		nvram_set("sb/1/maxp2ga1", "0x52");
		if(!nvram_get("ct_max")) {
			if (model == MODEL_RTN10D1)
				nvram_set_int("ct_max", 1024);
		}
		add_rc_support("2.4G mssid");
#ifdef RTCONFIG_KYIVSTAR
		add_rc_support("kyivstar");
#endif
		break;
	case MODEL_RTAC53U:
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio",  3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio",22|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1.4");
		nvram_set("ohci_ports", "2-1.4");
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G 5G mssid usbX1");
#ifdef RTCONFIG_WLAN_LED
		add_rc_support("led_2g");
#endif
		break;
#endif // CONFIG_BCMWL5
	}

#if defined(CONFIG_BCMWL5) && !defined(RTCONFIG_DUALWAN)
	if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")){
		if(!nvram_match("switch_wan0tagid", "") && !nvram_match("switch_wan0tagid", "0"))
			sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
		else
			sprintf(wan_if, "eth0");

		nvram_set("wan_ifnames", wan_if);
	}
#endif

#ifdef RTCONFIG_RALINK
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
	if( button_pressed(BTN_HAVE_FAN) )
		add_rc_support("fanctrl");
#endif

#ifdef RTCONFIG_OLD_PARENTALCTRL
	add_rc_support("PARENTAL");
#endif

#ifdef RTCONFIG_PARENTALCTRL
	add_rc_support("PARENTAL2");
#endif

#ifdef RTCONFIG_YANDEXDNS
	add_rc_support("yadns");
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
	if(strcmp(nvram_safe_get("firmver"), "3.0.0.1")!=0)
		add_rc_support("modem");

#ifdef RTCONFIG_USB_BECEEM	
	add_rc_support("wimax");
#endif
#endif

#ifdef RTCONFIG_OPENVPN
	add_rc_support("openvpnd");
	nvram_set("vpnc_proto", "disable");
#endif

#ifdef RTCONFIG_HTTPS
	add_rc_support("HTTPS");
#endif

#ifdef RTCONFIG_WEBDAV
	add_rc_support("webdav");
	nvram_set("start_aicloud", "1");
#endif

#ifdef RTCONFIG_CLOUDSYNC
	add_rc_support("cloudsync");
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
//	if(model==MODEL_DSLN55U) 
//		add_rc_support("nodm");
//#endif
//#endif
#endif // RTCONFIG_APP_NETINSTALLED

#ifdef RTCONFIG_VPNC
	add_rc_support("vpnc");
	nvram_set("vpnc_proto", "disable");
#endif

#if RTCONFIG_TIMEMACHINE
	add_rc_support("timemachine");
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
#ifdef RTCONFIG_BCMARM
	nvram_set("apps_install_folder", "asusware.arm");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/arm/stable");
#else
	if(!strcmp(get_productid(), "VSL-N66U")){
		nvram_set("apps_install_folder", "asusware.mipsbig");
		nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsbig/stable");
	}
	else{ // mipsel
		nvram_set("apps_install_folder", "asusware");
		if(nvram_match("apps_ipkg_old", "1"))
			nvram_set("apps_ipkg_server", "http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT");
		else
			nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsel/stable");
	}
#endif
#endif

#ifdef RTCONFIG_DISK_MONITOR
	add_rc_support("diskutility");
#endif

#endif // RTCONFIG_USB

#ifdef RTCONFIG_WIRELESSREPEATER
	add_rc_support("repeater");
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	add_rc_support("psta");
#endif
#endif
#ifdef RTCONFIG_BCMWL6
	add_rc_support("wl6");
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

#ifdef RTCONFIG_WPSMULTIBAND
	add_rc_support("wps_multiband");
#endif

#ifdef RTCONFIG_USER_LOW_RSSI
	add_rc_support("user_low_rssi");
#endif

#if defined(RTCONFIG_NTFS) && !defined(RTCONFIG_NTFS3G)
	add_rc_support("ufsd");
#endif
	return 0;
}

void force_free_caches()
{
	int model = get_model();
#ifdef RTCONFIG_RALINK
	if (model != MODEL_RTN65U)
		free_caches(FREE_MEM_PAGE, 2, 0);
#else
	if(model==MODEL_RTN53||model==MODEL_RTN10D1) {
		free_caches(FREE_MEM_PAGE, 2, 0);
	}
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
#if defined(RTCONFIG_NTFS3G)
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

#ifdef RTCONFIG_BCMWL6A
#define ARPHRD_ETHER                    1 /* ARP Header */
#define CTF_FA_DISABLED         0
#define CTF_FA_BYPASS           1
#define CTF_FA_NORMAL           2
#define CTF_FA_SW_ACC           3
#define FA_ON(mode)             (mode == CTF_FA_BYPASS || mode == CTF_FA_NORMAL)

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
                cprintf("Memory allocate failed!\n");
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

static void
fa_nvram_adjust()
{
        FILE *fp;
        int fa_mode;
        bool reboot = FALSE;

        if (restore_defaults_g)
                return;

        if ((fp = fopen("/proc/fa", "r"))) {
                /* FA is capable */
                fclose(fp);

                fa_mode = atoi(nvram_safe_get("ctf_fa_mode"));
                switch (fa_mode) {
                        case CTF_FA_BYPASS:
                        case CTF_FA_NORMAL:
                                break;
                        default:
                                fa_mode = CTF_FA_DISABLED;
                                break;
                }

                if (FA_ON(fa_mode)) {
                        char wan_ifnames[128];
                        char wan_ifname[32], *next;
                        int len, ret;

                        cprintf("\nFA on.\n");

                        /* Set et2macaddr, et2phyaddr as same as et0macaddr, et0phyaddr */
                        if (!nvram_get("vlan2ports") || !nvram_get("wandevs"))  {
                                cprintf("Insufficient envram, cannot do FA override\n");
                                return;
                        }

                        /* adjusted */
                        if (!strcmp(nvram_get("wandevs"), "vlan2") &&
                            !strchr(nvram_get("vlan2ports"), 'u'))
                            return;

                        /* The vlan2ports will be change to "0 8u" dynamically by
                         * robo_fa_imp_port_upd. Keep nvram vlan2ports unchange.
                         */
                        /* Override wandevs to "vlan2" */
                        nvram_set("wandevs", "vlan2");
                        /* build wan i/f list based on def nvram variable "wandevs" */
                        len = sizeof(wan_ifnames);
                        ret = build_ifnames("vlan2", wan_ifnames, &len);
                        if (!ret && len) {
                                /* automatically configure wan0_ too */
                                nvram_set("wan_ifnames", wan_ifnames);
                                nvram_set("wan0_ifnames", wan_ifnames);
                                foreach(wan_ifname, wan_ifnames, next) {
                                        nvram_set("wan_ifname", wan_ifname);
                                        nvram_set("wan0_ifname", wan_ifname);
                                        break;
                                }
                        }
                        cprintf("Override FA nvram...\n");
                        reboot = TRUE;
                }
                else {
                        cprintf("\nFA off.\n");
                }
        }
        else {
                /* FA is not capable */
                if (FA_ON(fa_mode)) {
                        nvram_unset("ctf_fa_mode");
                        cprintf("FA not supported...\n");
                        reboot = TRUE;
                }
        }

        if (reboot) {
                nvram_commit();
                cprintf("FA nvram overridden, rebooting...\n");
                kill(1, SIGTERM);
        }
}
#endif /* LINUX_2_6_36 */


static void sysinit(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i, r, retry = 0;
	DIR *d;
	struct dirent *de;
	char s[256];
	char t[256];
#if defined(CONFIG_BCMWL5)
	int model;
#endif

#define MKNOD(name,mode,dev)		if(mknod(name,mode,dev))		perror("## mknod " name)
#define MOUNT(src,dest,type,flag,data)	if(mount(src,dest,type,flag,data))	perror("## mount " src)
#define MKDIR(pathname,mode)		if(mkdir(pathname,mode))		perror("## mkdir " pathname)

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

	limit_page_cache_ratio(30);

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

	set_action(ACT_IDLE);

	for (i = 0; defenv[i]; ++i) {
		putenv(defenv[i]);
	}

	if (!noconsole) {
		printf("\n\nHit ENTER for console...\n\n");
		run_shell(1, 0);
	}

#ifdef RTCONFIG_RALINK
	// avoid the process like fsck to devour the memory.
	// ex: when DUT ran fscking, restarting wireless would let DUT crash.
	if ((get_model() == MODEL_RTN56U) || (get_model() == MODEL_DSLN55U) || (get_model() == MODEL_RTAC52U))
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
	else
	f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
#else
	// At the Broadcom platform, restarting wireless won't use too much memory.
	if(get_model()==MODEL_RTN53) {
		f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
	}
#endif
#ifdef RTCONFIG_16M_RAM_SFP
	f_write_string("/proc/sys/vm/min_free_kbytes", "512", 0, 0);
#endif
#ifdef RTCONFIG_BCMARM
	f_write_string("/proc/sys/vm/min_free_kbytes", "14336", 0, 0);
#endif
	force_free_caches();

	// autoreboot after kernel panic
	f_write_string("/proc/sys/kernel/panic", "3", 0, 0);
	f_write_string("/proc/sys/kernel/panic_on_oops", "3", 0, 0);

	// be precise about vm commit
#ifdef CONFIG_BCMWL5
	f_write_string("/proc/sys/vm/overcommit_memory", "2", 0, 0);

	model = get_model();
	if(model==MODEL_RTN53 ||
		model==MODEL_RTN10U ||
		model==MODEL_RTN12B1 || model==MODEL_RTN12C1 ||
		model==MODEL_RTN12D1 || model==MODEL_RTN12VP || model==MODEL_RTN12HP || model==MODEL_RTN12HP_B1 ||
		model==MODEL_RTN15U){

		f_write_string("/proc/sys/vm/panic_on_oom", "1", 0, 0);
		f_write_string("/proc/sys/vm/overcommit_ratio", "100", 0, 0);
	}
#endif

#ifdef RTCONFIG_IPV6
	// disable IPv6 by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/disable_ipv6", "1", 0, 0);
#endif

#if defined(RTCONFIG_PSISTLOG) || defined(RTCONFIG_JFFS2LOG)
	f_write_string("/proc/sys/net/unix/max_dgram_qlen", "150", 0, 0);
#endif

	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++) {
		signal(fatalsigs[i], handle_fatalsigs);
	}
	signal(SIGCHLD, handle_reap);
	
#ifdef RTCONFIG_DSL
	check_upgrade_from_old_ui();
#endif	
	init_syspara();// for system dependent part
	init_nvram();  // for system indepent part after getting model	
	restore_defaults(); // restore default if necessary 
#ifdef RTCONFIG_BCMWL6A
	/* Ajuest FA NVRAM variables */
	fa_nvram_adjust();
#endif
#if defined(RTN14U)
	nvram_unset("wl1_ssid");
#endif
	init_gpio();   // for system dependent part
#ifdef RTCONFIG_SWMODE_SWITCH
	init_swmode(); // need to check after gpio initized
#endif
	init_switch(); // for system dependent part
	init_wl();     // for system dependent part

//	config_loopback();

	klogctl(8, NULL, nvram_get_int("console_loglevel"));

	setup_conntrack();
	setup_timezone();
	if (!noconsole) xstart("console");

#ifdef RTCONFIG_USB_BECEEM
	eval("cp", "-rf", "/rom/Beceem_firmware", "/tmp");
#endif
#ifdef RTCONFIG_BCMARM
	if(get_model() == MODEL_RTAC56U || get_model() == MODEL_RTAC68U || get_model() == MODEL_RTN18U){
#ifdef SMP
		int fd;

		if ((fd = open("/proc/irq/163/smp_affinity", O_RDWR)) >= 0) {
			close(fd);
			if (nvram_match("enable_samba", "0")){  // not set txworkq 
				system("echo 2 > /proc/irq/163/smp_affinity");
				system("echo 2 > /proc/irq/169/smp_affinity");
			}
			system("echo 2 > /proc/irq/111/smp_affinity");
			system("echo 2 > /proc/irq/112/smp_affinity");
		}
#endif
		nvram_set("txworkq", "1");
	}
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

#ifdef RTN65U
		extern void asm1042_upgrade(int);
		asm1042_upgrade(1);	// check whether upgrade firmware of ASM1042
#endif

		run_custom_script("init-start", NULL);
		use_custom_config("fstab", "/etc/fstab");

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
					stop_usb();
					stop_usbled();
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
			if(nvram_match("Ate_power_on_off_enable", "1")) {
				rc_check = nvram_get_int("Ate_rc_check");
				boot_check = nvram_get_int("Ate_boot_check");
				boot_fail = nvram_get_int("Ate_boot_fail");
				total_fail_check = nvram_get_int("Ate_total_fail_check");
dbG("rc/boot/total chk= %d/%d/%d, boot/total fail= %d/%d\n", rc_check,boot_check,total_fail_check, boot_fail,total_fail_check);
				if( nvram_get_int("Ate_FW_err") >= nvram_get_int("Ate_fw_fail")) {
					ate_commit_bootlog("6");
					dbG("*** Bootloader read FW fail: %d ***\n", nvram_get_int("Ate_FW_err"));
					Alarm_Led();
				}

				if (rc_check != boot_check) {
					if(nvram_get("Ate_reboot_log")==NULL)
						sprintf(reboot_log, "%d,", rc_check);
					else
						sprintf(reboot_log, "%s%d,", nvram_get("Ate_reboot_log"), rc_check);
					nvram_set("Ate_reboot_log", reboot_log);
					nvram_set_int("Ate_boot_fail", ++boot_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
dbg("boot/continue fail= %d/%d\n", nvram_get_int("Ate_boot_fail"),nvram_get_int("Ate_continue_fail"));
					if(boot_fail >= nvram_get_int("Ate_continue_fail")) { //Show boot fail!!
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

				if(total_fail_check == nvram_get_int("Ate_total_fail")) {
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
			int fd = -1;
			fd = file_lock("usb");	// hold off automount processing
			start_usb();
			start_usbled();
#ifdef LINUX26
			start_sd_idle();
#endif
			file_unlock(fd);	// allow to process usb hotplug events
			/*
			 * On RESTART some partitions can stay mounted if they are busy at the moment.
			 * In that case USB drivers won't unload, and hotplug won't kick off again to
			 * remount those drives that actually got unmounted. Make sure to remount ALL
			 * partitions here by simulating hotplug event.
			 */
			if (state == SIGHUP /* RESTART */)
				add_remove_usbhost("-1", 1);
#endif
			create_passwd();
#ifdef RTCONFIG_IPV6
			if ( !(ipv6_enabled() && is_routing_enabled()) )
				f_write_string("/proc/sys/net/ipv6/conf/all/disable_ipv6", "1", 0, 0);
#endif
			start_vlan();
#ifdef RTCONFIG_DSL
			start_dsl();
#endif
			start_lan();
			start_services();
			start_wan();
			start_wl();
#ifdef CONFIG_BCMWL5
			if (restore_defaults_g)
			{
				restore_defaults_g = 0;

				if ((get_model() == MODEL_RTAC66U) ||
					(get_model() == MODEL_RTAC56U) ||
					(get_model() == MODEL_RTAC68U) ||
					(get_model() == MODEL_RTN12HP) ||
					(get_model() == MODEL_RTN12HP_B1) ||
					(get_model() == MODEL_RTN66U)){
					set_wltxpower();
				}else if (nvram_contains_word("rc_support", "pwrctrl")) {
					set_wltxpower();
				}
				restart_wireless();
			}
			else
#endif
			nvram_set_int("wlready", 1);
			lanaccess_wl();
#ifdef RTCONFIG_USB_PRINTER
			start_usblpsrv();
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

			if(nvram_match("Ate_power_on_off_enable", "3")|| //Show alert light
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

			ate_dev_status();
/*
			//For 66U normal boot & check device
			if((get_model()==MODEL_RTN66U) && nvram_match("Ate_power_on_off_enable", "0")) {
			    if(nvram_get_int("dev_fail_reboot")!=0) {
				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					dev_fail_count = nvram_get_int("dev_fail_count");
					dev_fail_count++;
					if(dev_fail_count < nvram_get_int("dev_fail_reboot")) {
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
					if(!nvram_match("dev_fail_count", "0")) {
						nvram_set("dev_fail_count", "0");
						nvram_commit();
					}
				}
			    }
			}
*/
			if(nvram_match("Ate_power_on_off_enable", "1")) {
				dev_check = nvram_get_int("Ate_dev_check");
				dev_fail = nvram_get_int("Ate_dev_fail");

				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					nvram_set_int("Ate_dev_fail", ++dev_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
					if(nvram_get("Ate_dev_log")==NULL)
						sprintf(dev_log, "%d,", dev_check);
					else
						sprintf(dev_log, "%s%d,", nvram_get("Ate_dev_log"), dev_check);
					nvram_set("Ate_dev_log", dev_log);
					dbG("dev Fail at %d, status= %s\n", dev_check, nvram_get("Ate_dev_status"));
					dbG("dev/total fail= %d/%d\n", dev_fail, total_fail_check);
					if(dev_fail== nvram_get_int("Ate_continue_fail")) {
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

				if(boot_check < nvram_get_int("Ate_reboot_count")) {
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
			if(is_usb_modem_ready() != 1 && nvram_match("ctf_disable_modem", "1")){
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
#if defined (RTCONFIG_USB_XHCI) || defined (RTCONFIG_USB_2XHCI2)
			if(nvram_get_int("usb_usb3") == 0){
#ifndef RTCONFIG_XHCIMODE
				_dprintf("USB2: reset USB power to call up the device on the USB 3.0 port.\n");
				set_pwr_usb(0);
				sleep(2);
				set_pwr_usb(1);
#endif
			}
#endif
#endif

			nvram_set("success_start_service", "1");

			force_free_caches();

if(nvram_match("commit_test", "1")) {
	int x=0, y=0;
	while(1) {
		x++;
		dbG("Nvram commit: %d\n", x);
		nvram_set_int("commit_cut", x);
		nvram_commit();
		if(nvram_get_int("commit_cut")!=x) {
			TRACE_PT("\n\n======== NVRAM Commit Failed at: %d =========\n\n", x);
			break;
		}			
	}	
}
			break;
		}

		chld_reap(0);		/* Periodically reap zombies. */
		check_services();
#ifdef RTCONFIG_BCMARM
		/* free pagecache */
		system("echo 1 > /proc/sys/vm/drop_caches");
#endif
		do {
		ret = sigwait(&sigset, &state);
		} while (ret);
	}

	return 0;
}

int reboothalt_main(int argc, char *argv[])
{
	int reboot = (strstr(argv[0], "reboot") != NULL);
	int def_reset_wait = 20;

	cprintf(reboot ? "Rebooting..." : "Shutting down...");
	kill(1, reboot ? SIGTERM : SIGQUIT);

#if defined(RTN14U) || defined(RTN65U) || defined(RTAC52U)
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
		cprintf("Still running... Doing machine reset.\n");
#ifdef RTCONFIG_USB
		remove_usb_module();
#endif
		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(1);
		f_write("/proc/sysrq-trigger", "b", 1, 0 , 0); /* machine reset */
	}

	return 0;
}

