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
	NULL
};

extern struct nvram_tuple router_defaults[];
extern struct nvram_tuple router_state_defaults[];

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

#ifdef RTCONFIG_RALINK
	char word[256], *next;
	int unit = 0, subunit;
	char macaddr_str[18], macbuf[13];
	char *macaddr_strp;
	unsigned char mac_binary[6];
	unsigned long long macvalue;
	unsigned char *macp;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		memset(mac_binary, 0x0, 6);
		memset(macbuf, 0x0, 13);
		sprintf(macaddr_str, "pci/%d/1/macaddr", unit + 1);
		macaddr_strp = nvram_get(macaddr_str);

		if (macaddr_strp)
		{
			ether_atoe(macaddr_strp, mac_binary);
			sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
					mac_binary[0],
					mac_binary[1],
					mac_binary[2],
					mac_binary[3],
					mac_binary[4],
					mac_binary[5]);
			macvalue = strtoll(macbuf, (char **) NULL, 16);

			for (subunit = 1; subunit < 4; subunit++)
			{
				macvalue++;
				macp = &macvalue;
				sprintf(macbuf, "%02X:%02X:%02X:%02X:%02X:%02X\n", *(macp+5), *(macp+4), *(macp+3), *(macp+2), *(macp+1), *(macp+0));
				snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
				nvram_set(strcat_r(prefix, "hwaddr", tmp), macbuf);
			}
		}

		unit++;
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

	memset(wlx_vifnames, 0, sizeof(wlx_vifnames));
	memset(wl_vifnames, 0, sizeof(wl_vifnames));
	memset(lan_ifnames, 0, sizeof(lan_ifnames));

	if (!nvram_get("wl_country_code"))
		nvram_set("wl_country_code", "");

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		
		for (t = router_defaults; t->name; t++) {
			if(strncmp(t->name, "wl_", 3)!=0) continue;
			if (!nvram_get(strcat_r(prefix, &t->name[3], tmp))) {
				nvram_set(tmp, t->value);
			}
		}
		unit++;
	}

//	virtual_radio_restore_defaults();

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		memset(wlx_vifnames, 0x0, 32);
		snprintf(pprefix, sizeof(pprefix), "wl%d_", unit);
		for (subunit = 1; subunit < 4; subunit++)
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
				nvram_set(strcat_r(prefix, "net_reauth", tmp), "36000");
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
					nvram_get("emf_enable"));
#else
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), "0");
				nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");

				nvram_set(strcat_r(prefix, "expire", tmp), "0");
				nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");
			}
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

	if (strlen(wl_vifnames))
	{
		sprintf(lan_ifnames, "%s %s", nvram_safe_get("lan_ifnames"), wl_vifnames);
		nvram_set("lan_ifnames", lan_ifnames);
	}
}

/* for WPS Reset */
void
wl_defaults_wps(void)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXX_", tmp[100];
	char word[256], *next;
	int unit;

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		for (t = router_defaults; t->name; t++) {
			if(strncmp(t->name, "wl_", 3) &&
				strncmp(t->name, "wl0_", 4) &&
				strncmp(t->name, "wl1_", 4))
				continue;

			if(!strncmp(t->name, "wl_", 3))
				nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
			else
				nvram_set(t->name, t->value);
		}
		unit++;
	}
}
void
/* assign none-exist value or inconsist value */
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
		logmessage(LOGNAME, "Restoring defaults...");
	}

#ifdef CONFIG_BCMWL5
	restore_defaults_g = restore_defaults;
#endif
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
		FILE *fp;
		char memdata[256] = {0};
		uint memsize = 0;
		char pktq_thresh[8] = {0};

		if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
			/* get memory count in MemTotal = %d */
			while (fgets(memdata, 255, fp) != NULL) {
				if (strstr(memdata, "MemTotal") != NULL) {
					sscanf(memdata, "MemTotal:        %d kB", &memsize);
					break;
				}
			}
			fclose(fp);
		}

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
	}

	/* Commit values */
	if (restore_defaults) {
		nvram_commit();		
		fprintf(stderr, "done\n");
	}

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
		case MODEL_RTN10U:
                        nvram_set("reboot_time", "85"); // default is 60 sec
                        break;
#ifdef RTCONFIG_RALINK
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
			nvram_set("reboot_time", "75"); // default is 60 sec
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
#ifdef RTAC66U
	nvram_set("led_5g", "0");
#endif

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
	
	if(button_pressed(BTN_SWMODE_SW_REPEATER))
		nvram_set_int("sw_mode", SW_MODE_REPEATER);
	else if(button_pressed(BTN_SWMODE_SW_AP))
		nvram_set_int("sw_mode", SW_MODE_AP);
	else nvram_set_int("sw_mode", SW_MODE_ROUTER);
}
#endif

#define GPIO_ACTIVE_LOW 0x1000

// intialized in this area
//  lan_ifnames
//  wan_ifnames
//  wl_ifnames
//  btn_xxxx_gpio
//  led_xxxx_gpio

int init_nvram(void)
{
	int model = get_model();
	char wan_if[10];
#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_RALINK)
	int unit = 0;
#endif

	TRACE_PT("init_nvram for %d\n", model);

	nvram_set("rc_support", "");
	nvram_set_int("led_usb_gpio", 0xff);
	nvram_set_int("led_lan_gpio", 0xff);
	nvram_set_int("led_wan_gpio", 0xff);
	nvram_set_int("led_2g_gpio", 0xff);
	nvram_set_int("led_5g_gpio", 0xff);
#ifdef RTCONFIG_SWMODE_SWITCH
	nvram_set_int("btn_swmode1_gpio", 0xff);
	nvram_set_int("btn_swmode2_gpio", 0xff);
	nvram_set_int("btn_swmode3_gpio", 0xff);
	nvram_set_int("swmode_switch", 0);
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	nvram_set_int("btn_wifi_gpio", 0xff);
#endif
	/* In the order of physical placement */
	nvram_set("ehci_ports", "");
	nvram_set("ohci_ports", "");

	/* treat no phy, no internet as disconnect */
	nvram_set("web_redirect", "3");

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
		nvram_set_int("btn_rst_gpio", 25);
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
		eval("8367m", "11");		// for SR3 LAN LED
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
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;

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
		// Support dsl lan dsl/usb lan/usb dsl/lan only, so fart
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
			nvram_set("ct_max", "30000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("dsl");
#ifdef RTCONFIG_DUALWAN
		add_rc_support("dualwan");		
#endif		
#ifdef RTCONFIG_WIRELESS_SWITCH		
		add_rc_support("wifi_hw_sw");
#endif

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
		nvram_set_int("led_usb_gpio", 0xff);
		break;
#endif // RTCONFIG_RALINK

#ifdef CONFIG_BCMWL5
	case MODEL_RTN12HP:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);	/* does HP have it */
		nvram_set_int("sb/1/ledbh5", 2);			/* is active_high? set 7 then */
		add_rc_support("pwrctrl");
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12D1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh5", 7);
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12C1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("mssid");
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
		add_rc_support("mssid");
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
		add_rc_support("2.4G mssid usbX1");
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
		nvram_set_int("led_usb_gpio", 0xff);
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
		nvram_set_int("btn_radio_gpio", 0xff);
		nvram_set_int("led_pwr_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 0xff);
		nvram_set_int("led_lan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set("sb/1/ledbh0", "2");
		/* change lan interface to vlan0 */
		nvram_set("vlan0hwname", "et0");
		nvram_set("landevs", "vlan0 wl0 wl1");
		nvram_unset("vlan2ports");
		nvram_unset("vlan2hwname");
		/* end */
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");

		add_rc_support("2.4G 5G update mssid no5gmssid");
		break;

	case MODEL_RTN66U:
		nvram_set_int("btn_radio_gpio", 13|GPIO_ACTIVE_LOW);	// ? should not used by Louis
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
		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set_int("led_usb_gpio", 8);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G mssid usbX1 update");
		break;

	case MODEL_RTN10D1:
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");
		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		if(!nvram_get("ct_max")) 
			nvram_set("ct_max", "1024");
		add_rc_support("2.4G mssid");
		break;
#endif // CONFIG_BCMWL5
	}

#if defined(CONFIG_BCMWL5) && !defined(RTCONFIG_DUALWAN)
	if(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")){
		sprintf(wan_if, "vlan%s", nvram_safe_get("switch_wan0tagid"));
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

#ifdef RTCONFIG_APP_PREINSTALLED
	add_rc_support("appbase");

#ifdef RTCONFIG_MEDIA_SERVER
	add_rc_support("media");
#endif
#endif // RTCONFIG_APP_PREINSTALLED

#ifdef RTCONFIG_APP_NETINSTALLED
	if(model==MODEL_RTN10U)
		add_rc_support("appnone");
	else add_rc_support("appnet");

// DSL-N55U will follow N56U to have DM
//#ifdef RTCONFIG_RALINK
//#ifdef RTCONFIG_DSL
//	if(model==MODEL_DSLN55U) 
//		add_rc_support("nodm");
//#endif
//#endif
#endif // RTCONFIG_APP_NETINSTALLED

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
	//add_rc_support("ruisp");

#ifdef RTCONFIG_NFS
	add_rc_support("nfsd");
#endif

	return 0;
}

void force_free_caches()
{
#ifdef RTCONFIG_RALINK
	free_caches(FREE_MEM_PAGE, 2, 0);
#else
	int model;

	model = get_model();

	if(model==MODEL_RTN53||model==MODEL_RTN10D1) {
		free_caches(FREE_MEM_PAGE, 2, 0);
	}
#endif
}

static void sysinit(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i;
	DIR *d;
	struct dirent *de;
	char s[256];
	char t[256];
	int model;

	mount("proc", "/proc", "proc", 0, NULL);
	mount("tmpfs", "/tmp", "tmpfs", 0, NULL);

#ifdef LINUX26
	mount("devfs", "/dev", "tmpfs", MS_MGC_VAL | MS_NOATIME, NULL);
	mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
	mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
	mount("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
	mkdir("/dev/shm", 0777);
	mkdir("/dev/pts", 0777);
	mount("devpts", "/dev/pts", "devpts", MS_MGC_VAL, NULL);
#endif

	init_devs(); // for system dependent part
	
	if (console_init()) noconsole = 1;

	stime(&tm);

	static const char *mkd[] = {
		"/tmp/etc", "/tmp/var", "/tmp/home", 
#ifdef RTCONFIG_USB
POOL_MOUNT_ROOT,
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
	eval("hotplug2", "--coldplug");
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
	init_spinlock();

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
	if (get_model() == MODEL_RTN56U)
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
		model==MODEL_RTN12D1 || model==MODEL_RTN12HP ||
		model==MODEL_RTN15U){

		f_write_string("/proc/sys/vm/panic_on_oom", "1", 0, 0);
		f_write_string("/proc/sys/vm/overcommit_ratio", "100", 0, 0);
	}
#endif

#ifdef RTCONFIG_IPV6
	// disable IPv6 by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/disable_ipv6", "1", 0, 0);
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
}

int init_main(int argc, char *argv[])
{
	int state, i;
	sigset_t sigset;
	int rc_check, dev_check, boot_check; //Power on/off test
	int boot_fail, dev_fail, dev_fail_count, total_fail_check;
	char reboot_log[128], dev_log[128];
	int ret;

	sysinit();

	sigemptyset(&sigset);
	for (i = 0; i < sizeof(initsigs) / sizeof(initsigs[0]); i++) {
		sigaddset(&sigset, initsigs[i]);
	}
	sigprocmask(SIG_BLOCK, &sigset, NULL);


#ifdef RTCONFIG_JFFS2
	start_jffs2();
#endif

	run_custom_script("init-start", NULL);
	use_custom_config("fstab", "/etc/fstab");

	state = SIGUSR2;	/* START */

	for (;;) {
		TRACE_PT("main loop signal/state=%d\n", state);

		switch (state) {
		case SIGUSR1:		/* USER1: service handler */
			handle_notifications();
#ifdef RTCONFIG_16M_RAM_SFP
			force_free_caches();
#endif
			break;

		case SIGHUP:		/* RESTART */
		case SIGINT:		/* STOP */
		case SIGQUIT:		/* HALT */
		case SIGTERM:		/* REBOOT */
			stop_services();
			stop_wan();
			stop_lan();
			stop_vlan();
			stop_logger();

			if ((state == SIGTERM /* REBOOT */) ||
				(state == SIGQUIT /* HALT */)) {
#ifdef RTCONFIG_USB
				remove_storage_main(1);
				stop_usb();
				stop_usbled();
#endif
				shutdn(state == SIGTERM /* REBOOT */);
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
			enable_ipv6((ipv6_enabled() && is_routing_enabled())/*, 0*/);
#endif
			start_vlan();
#ifdef RTCONFIG_DSL
			start_dsl();
#endif
			start_lan();
			start_wan();
			start_services();
			start_wl();
#ifdef CONFIG_BCMWL5
			if (restore_defaults_g)
			{
				restore_defaults_g = 0;
				if ((get_model() == MODEL_RTAC66U) ||
					(get_model() == MODEL_RTN12HP) ||
					(get_model() == MODEL_RTN66U))
					set_wltxpower();
				restart_wireless();
			}
#endif
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
					nvram_commit();
					dbG("System boot up %d times, reboot...\n", boot_check);
					sleep(1);
					reboot(RB_AUTOBOOT);
				}
				else {
					dbG("System boot up success %d times\n", boot_check);
					setAllLedOn();
					ate_commit_bootlog("2");
				}
			}

#ifdef RTCONFIG_BCMWL6
#ifdef ACS_ONCE
			if (nvram_match("acsd_restart_wl", "1"))
			{
				nvram_set("acsd_restart_wl", "0");

				restart_wireless_acsd();
				nvram_set("wl0_chanspec", nvram_safe_get("wly0_chanspec"));
				nvram_set("wl1_chanspec", nvram_safe_get("wly1_chanspec"));
				nvram_set("wly0_chanspec", "0");
				nvram_set("wly1_chanspec", "0");
			}
#endif
#endif
			if (nvram_get_int("led_disable")==1)
				setup_leds();

			nvram_set("success_start_service", "1");

			force_free_caches();		
			break;
		}

		chld_reap(0);		/* Periodically reap zombies. */
		check_services();
		do {
		ret = sigwait(&sigset, &state);
		} while (ret);
	}

	return 0;
}

int reboothalt_main(int argc, char *argv[])
{
	int reboot = (strstr(argv[0], "reboot") != NULL);
	puts(reboot ? "Rebooting..." : "Shutting down...");
	fflush(stdout);
	sleep(1);
	kill(1, reboot ? SIGTERM : SIGQUIT);
	
	/* In the case we're hung, we'll get stuck and never actually reboot.
	 * The only way out is to pull power.
	 * So after 'reset_wait' seconds (default: 20), forcibly crash & restart.
	 */
	if (fork() == 0) {
		int wait = nvram_get_int("reset_wait") ? : 20;
		if ((wait < 10) || (wait > 120)) wait = 10;

		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(wait);
		puts("Still running... Doing machine reset.");
		fflush(stdout);
		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(1);
		f_write("/proc/sysrq-trigger", "b", 1, 0 , 0); /* machine reset */
	}

	return 0;
}

