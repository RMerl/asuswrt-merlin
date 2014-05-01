/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/
/*

	wificonf, OpenWRT
	Copyright (C) 2005 Felix Fietkau <nbd@vd-s.ath.cx>

*/
/*

	Modified for Tomato Firmware
	Portions, Copyright (C) 2006-2009 Jonathan Zarate

*/

#include <rc.h>

#ifndef UU_INT
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#endif

#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <dirent.h>

#include <wlutils.h>
#ifdef CONFIG_BCMWL5
#include <bcmparams.h>
#include <wlioctl.h>
#include <security_ipc.h>

#ifndef WL_BSS_INFO_VERSION
#error WL_BSS_INFO_VERSION
#endif
#if WL_BSS_INFO_VERSION >= 108
#include <etioctl.h>
#else
#include <etsockio.h>
#endif
#endif /* CONFIG_BCMWL5 */

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_USB_MODEM
#include <usb_info.h>
#endif

#define sin_addr(s) (((struct sockaddr_in *)(s))->sin_addr)

static time_t s_last_lan_port_stopped_ts = 0;

void update_lan_state(int state, int reason)
{
	char prefix[32];
	char tmp[100], tmp1[100], *ptr;

	snprintf(prefix, sizeof(prefix), "lan_");

	_dprintf("%s(%s, %d, %d)\n", __FUNCTION__, prefix, state, reason);

	nvram_set_int(strcat_r(prefix, "state_t", tmp), state);
	nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), 0);

	if(state==LAN_STATE_INITIALIZING)
	{
		if(nvram_match(strcat_r(prefix, "proto", tmp), "dhcp")) {
			// always keep in default ip before getting ip
			nvram_set(strcat_r(prefix, "dns", tmp), nvram_default_get("lan_ipaddr"));
		}

		if (!nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp))) {
			memset(tmp1, 0, sizeof(tmp1));

			ptr = nvram_get(strcat_r(prefix, "dns1_x", tmp));
			if(ptr && strlen(ptr))
				sprintf(tmp1, "%s", ptr);
			ptr = nvram_get(strcat_r(prefix, "dns2_x", tmp));

			if(ptr && strlen(ptr)) {
				if(strlen(tmp1))
					sprintf(tmp1, "%s %s", tmp1, ptr);
				else sprintf(tmp1, "%s", ptr);
			}

			nvram_set(strcat_r(prefix, "dns", tmp), tmp1);
		}
		else nvram_set(strcat_r(prefix, "dns", tmp), "");
	}
	else if(state==LAN_STATE_STOPPED) {
		// Save Stopped Reason
		// keep ip info if it is stopped from connected
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);
	}
}

#ifdef CONFIG_BCMWL5
void txpwr_rtn12hp(char *ifname, int unit, int subunit)
{
	int txpower;
	int txpowerq;
	char str_txpowerq[8];

	txpower = nvram_get_int(wl_nvname("TxPower", unit, 0));
	if (unit == 0)
	{
		if (txpower == 80)
			eval("wl", "-i", ifname, "txpwr1", "-o", "-q", "-1");
		else
		{
			if (txpower < 30)
				txpowerq = 56;
			else if (txpower < 50)
				txpowerq = 64;
			else if (txpower < 81)
				txpowerq = 72;
			else if (txpower < 151)
				txpowerq = 84;
			else if (txpower < 221)
				txpowerq = 88;
			else if (txpower < 291)
				txpowerq = 90;
			else if (txpower < 361)
				txpowerq = 92;
			else if (txpower < 431)
				txpowerq = 94;
			else
				txpowerq = 96;

			sprintf(str_txpowerq, "%d", txpowerq);
			eval("wl", "-i", ifname, "txpwr1", "-o", "-q", str_txpowerq);
		}
	}

	if (txpower != 80)
		dbG("txpowerq: %d\n", txpowerq);

}

#ifdef RTCONFIG_BCMWL6
/* workaround for BCMWL6 only */
static void set_mrate(const char* ifname, const char* prefix)
{
	float mrate = 0;
	char tmp[100];

	switch (nvram_get_int(strcat_r(prefix, "mrate_x", tmp))) {
	case 0: /* Auto */
		mrate = 0;
		break;
	case 1: /* Legacy CCK 1Mbps */
		mrate = 1;
		break;
	case 2: /* Legacy CCK 2Mbps */
		mrate = 2;
		break;
	case 3: /* Legacy CCK 5.5Mbps */
		mrate = 5.5;
		break;
	case 4: /* Legacy OFDM 6Mbps */
		mrate = 6;
		break;
	case 5: /* Legacy OFDM 9Mbps */
		mrate = 9;
		break;
	case 6: /* Legacy CCK 11Mbps */
		mrate = 11;
		break;
	case 7: /* Legacy OFDM 12Mbps */
		mrate = 12;
		break;
	case 8: /* Legacy OFDM 18Mbps */
		mrate = 18;
		break;
	case 9: /* Legacy OFDM 24Mbps */
		mrate = 24;
		break;
	case 10: /* Legacy OFDM 36Mbps */
		mrate = 36;
		break;
	case 11: /* Legacy OFDM 48Mbps */
		mrate = 48;
		break;
	case 12: /* Legacy OFDM 54Mbps */
		mrate = 54;
		break;
	default: /* Auto */
		mrate = 0;
		break;
	}

	sprintf(tmp, "wl -i %s mrate %.1f", ifname, mrate);
	system(tmp);
}
#endif

static int wlconf(char *ifname, int unit, int subunit)
{
	int r;
	char wl[24];
	int txpower;
	int model = get_model();
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
#ifdef RTCONFIG_QTN
	if (!strcmp(ifname, "wifi0"))
		unit = 1;
#endif
	if (unit < 0) return -1;

	if (subunit < 0)
	{
#ifdef RTCONFIG_QTN
		if (unit == 1)
			goto GEN_CONF;
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
		{
			eval("wlconf", ifname, "down");
			eval("wl", "-i", ifname, "radio", "off");
			return -1;
		}
#if 0
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		if (is_psta(1 - unit))
		{
			eval("wlconf", ifname, "down");
			eval("wl", "-i", ifname, "radio", "off");
			return -1;
		}
#endif
#endif
#endif
#ifdef RTCONFIG_QTN
GEN_CONF:
#endif
		generate_wl_para(unit, subunit);

		for (r = 1; r < MAX_NO_MSSID; r++)	// early convert for wlx.y
			generate_wl_para(unit, r);
	}

#if 0
	if (/* !wl_probe(ifname) && */ unit >= 0) {
		// validate nvram settings foa wireless i/f
		snprintf(wl, sizeof(wl), "--wl%d", unit);
		eval("nvram", "validate", wl);
	}
#endif

#ifdef RTCONFIG_QTN
	if (unit == 1)
		return -1;
#endif

	if (unit >= 0 && subunit < 0)
	{
#ifdef RTCONFIG_OPTIMIZE_XBOX
		if (nvram_match(strcat_r(prefix, "optimizexbox", tmp), "1"))
			eval("wl", "-i", ifname, "ldpc_cap", "0");
		else
			eval("wl", "-i", ifname, "ldpc_cap", "1");	// driver default setting
#endif
#ifdef RTCONFIG_BCMWL6
		if (nvram_match(strcat_r(prefix, "ack_ratio", tmp), "1"))
			eval("wl", "-i", ifname, "ack_ratio", "4");
		else
			eval("wl", "-i", ifname, "ack_ratio", "2");	// driver default setting

		if (nvram_match(strcat_r(prefix, "ampdu_mpdu", tmp), "1"))
			eval("wl", "-i", ifname, "ampdu_mpdu", "64");
		else
			eval("wl", "-i", ifname, "ampdu_mpdu", "-1");	// driver default setting
#ifdef RTCONFIG_BCMARM
		if (nvram_match(strcat_r(prefix, "ampdu_rts", tmp), "1"))
			eval("wl", "-i", ifname, "ampdu_rts", "1");	// driver default setting
		else
			eval("wl", "-i", ifname, "ampdu_rts", "0");

		if (nvram_match(strcat_r(prefix, "itxbf", tmp), "1"))
			eval("wl", "-i", ifname, "txbf_imp", "1");	// driver default setting
		else
			eval("wl", "-i", ifname, "txbf_imp", "0");
#endif
#else
// Disabled since we are still using 5.100
///		eval("wl", "-i", ifname, "ampdu_density", "6");		// resolve IOT with Intel STA for BRCM SDK 5.110.27.20012
#endif
	}

	r = eval("wlconf", ifname, "up");
	if (r == 0) {
		if (unit >= 0 && subunit < 0) {
#ifdef REMOVE
			// setup primary wl interface
			nvram_set("rrules_radio", "-1");
			eval("wl", "-i", ifname, "antdiv", nvram_safe_get(wl_nvname("antdiv", unit, 0)));
			eval("wl", "-i", ifname, "txant", nvram_safe_get(wl_nvname("txant", unit, 0)));
			eval("wl", "-i", ifname, "txpwr1", "-o", "-m", nvram_get_int(wl_nvname("txpwr", unit, 0)) ? nvram_safe_get(wl_nvname("txpwr", unit, 0)) : "-1");
			eval("wl", "-i", ifname, "interference", nvram_safe_get(wl_nvname("interfmode", unit, 0)));
#endif
#ifndef RTCONFIG_BCMWL6
			switch (model) {
				default:
					if ((unit == 0) &&
						nvram_match(strcat_r(prefix, "noisemitigation", tmp), "1"))
					{
						eval("wl", "-i", ifname, "interference_override", "4");
						eval("wl", "-i", ifname, "phyreg", "0x547", "0x4444");
						eval("wl", "-i", ifname, "phyreg", "0xc33", "0x280");
					}
					break;
			}
#else
//#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
			if (is_psta(1 - unit))
			{
				eval("wl", "-i", ifname, "closed", "1");
				eval("wl", "-i", ifname, "maxassoc", "0");
			}
#endif
//#endif
			set_mrate(ifname, prefix);
#ifdef RTCONFIG_BCMARM
			if (nvram_match(strcat_r(prefix, "ampdu_rts", tmp), "0") &&
				nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
				eval("wl", "-i", ifname, "rtsthresh", "65535");
#endif
			if (unit) {
#ifdef RTAC68U
				if (	nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
					nvram_match(strcat_r(prefix, "country_rev", tmp), "13"))
					eval("wl", "-i", ifname, "radarthrs",
					"0x6ac", "0x30", "0x6a8", "0x30", "0x6a8", "0x30", "0x6a4", "0x30", "0x6a4", "0x30", "0x6a0", "0x30");
				else if ((get_model() == MODEL_DSLAC68U) &&
					nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
					nvram_match(strcat_r(prefix, "country_rev", tmp), "13"))
					eval("wl", "-i", ifname, "radarthrs",
					"0x6ac", "0x30", "0x6a8", "0x30", "0x6a8", "0x30", "0x6a8", "0x30", "0x6a4", "0x30", "0x6a0", "0x30");
#elif defined(RTAC66U) || defined(RTN66U)
#if 0
				if (((get_model() == MODEL_RTAC66U) &&
					nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
					nvram_match(strcat_r(prefix, "country_rev", tmp), "13")) ||
					((get_model() == MODEL_RTN66U) &&
					nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
					nvram_match(strcat_r(prefix, "country_rev", tmp), "0"))
				)
					eval("wl", "-i", ifname, "radarthrs",
					"0x6ac", "0x30", "0x6a8", "0x30", "0x6a8", "0x30", "0x6a8", "0x30", "0x6a4", "0x30", "0x6a0", "0x30");
#endif
#endif
			}
#endif
			txpower = nvram_get_int(wl_nvname("TxPower", unit, 0));

			dbG("unit: %d, txpower: %d\n", unit, txpower);

			switch (model) {
				default:

					eval("wl", "-i", ifname, "txpwr1", "-1");

					break;
			}
		}

		if (wl_client(unit, subunit)) {
			if (nvram_match(wl_nvname("mode", unit, subunit), "wet")) {
				ifconfig(ifname, IFUP|IFF_ALLMULTI, NULL, NULL);
			}
			if (nvram_get_int(wl_nvname("radio", unit, 0))) {
				snprintf(wl, sizeof(wl), "%d", unit);
				xstart("radio", "join", wl);
			}
		}
	}
	return r;
}
#endif

// -----------------------------------------------------------------------------

#if defined(CONFIG_BCMWL5)
/*
 * Carry out a socket request including openning and closing the socket
 * Return -1 if failed to open socket (and perror); otherwise return
 * result of ioctl
 */
static int
soc_req(const char *name, int action, struct ifreq *ifr)
{
	int s;
	int rv = 0;

	if (name == NULL) return -1;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}
	strncpy(ifr->ifr_name, name, IFNAMSIZ);
	rv = ioctl(s, action, ifr);
	close(s);

	return rv;
}
#endif

/* Check NVRam to see if "name" is explicitly enabled */
static inline int
wl_vif_enabled(const char *name, char *tmp)
{
	if (name == NULL) return 0;

	return (nvram_get_int(strcat_r(name, "_bss_enabled", tmp)));
}

#if defined(CONFIG_BCMWL5)
/* Set the HW address for interface "name" if present in NVRam */
static void
wl_vif_hwaddr_set(const char *name)
{
	int rc;
	char *ea;
	char hwaddr[20];
	struct ifreq ifr;

	if (name == NULL) return;

	snprintf(hwaddr, sizeof(hwaddr), "%s_hwaddr", name);
	ea = nvram_get(hwaddr);
	if (ea == NULL) {
		fprintf(stderr, "NET: No hw addr found for %s\n", name);
		return;
	}

	fprintf(stderr, "NET: Setting %s hw addr to %s\n", name, ea);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	ether_atoe(ea, (unsigned char *)ifr.ifr_hwaddr.sa_data);
	if ((rc = soc_req(name, SIOCSIFHWADDR, &ifr)) < 0) {
//		fprintf(stderr, "NET: Error setting hw for %s; returned %d\n", name, rc);
	}
}
#endif

#ifdef RTCONFIG_EMF
static void emf_mfdb_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *mgrp, *ifname;

	/* Add/Delete MFDB entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_entry"), next) {
		ifname = word;
		mgrp = strsep(&ifname, ":");

		if ((mgrp == NULL) || (ifname == NULL)) continue;

		/* Add/Delete MFDB entry using the group addr and interface */
		if (lan_port_ifname == NULL || strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"), "mfdb", lan_ifname, mgrp, ifname);
		}
	}
}

static void emf_uffp_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete UFFP entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_uffp_entry"), next) {
		ifname = word;

		if (ifname == NULL) continue;

		/* Add/Delete UFFP entry for the interface */
		if (lan_port_ifname == NULL || strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"), "uffp", lan_ifname, ifname);
		}
	}
}

static void emf_rtport_update(char *lan_ifname, char *lan_port_ifname, bool add)
{
	char word[256], *next;
	char *ifname;

	/* Add/Delete RTPORT entries corresponding to new interface */
	foreach (word, nvram_safe_get("emf_rtport_entry"), next) {
		ifname = word;

		if (ifname == NULL) continue;

		/* Add/Delete RTPORT entry for the interface */
		if (lan_port_ifname == NULL || strcmp(lan_port_ifname, ifname) == 0) {
			eval("emf", ((add) ? "add" : "del"), "rtport", lan_ifname, ifname);
		}
	}
}

static void start_emf(char *lan_ifname)
{
	/* Start EMF */
	eval("emf", "start", lan_ifname);

	/* Add the static MFDB entries */
	emf_mfdb_update(lan_ifname, NULL, 1);

	/* Add the UFFP entries */
	emf_uffp_update(lan_ifname, NULL, 1);

	/* Add the RTPORT entries */
	emf_rtport_update(lan_ifname, NULL, 1);
}

static void stop_emf(char *lan_ifname)
{
	eval("emf", "stop", lan_ifname);
	eval("igs", "del", "bridge", lan_ifname);
	eval("emf", "del", "bridge", lan_ifname);
}
#endif

// -----------------------------------------------------------------------------

/* Set initial QoS mode for all et interfaces that are up. */
#ifdef CONFIG_BCMWL5
void
set_et_qos_mode(void)
{
	int i, s, qos;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	qos = (strcmp(nvram_safe_get("wl_wme"), "off") != 0);

	for (i = 1; i <= DEV_NUMIFS; i ++) {
		ifr.ifr_ifindex = i;
		if (ioctl(s, SIOCGIFNAME, &ifr))
			continue;
		if (ioctl(s, SIOCGIFHWADDR, &ifr))
			continue;
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			continue;
		if (ioctl(s, SIOCGIFFLAGS, &ifr))
			continue;
		if (!(ifr.ifr_flags & IFF_UP))
			continue;
		/* Set QoS for et & bcm57xx devices */
		memset(&info, 0, sizeof(info));
		info.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t)&info;
		if (ioctl(s, SIOCETHTOOL, &ifr) < 0)
			continue;
		if ((strncmp(info.driver, "et", 2) != 0) &&
		    (strncmp(info.driver, "bcm57", 5) != 0))
			continue;
		ifr.ifr_data = (caddr_t)&qos;
		ioctl(s, SIOCSETCQOS, &ifr);
	}

	close(s);
}
#endif /* CONFIG_BCMWL5 */

#ifdef CONFIG_BCMWL5
static void check_afterburner(void)
{
	char *p;

	if (nvram_match("wl_afterburner", "off")) return;
	if ((p = nvram_get("boardflags")) == NULL) return;

	if (strcmp(p, "0x0118") == 0) {			// G 2.2, 3.0, 3.1
		p = "0x0318";
	}
	else if (strcmp(p, "0x0188") == 0) {	// G 2.0
		p = "0x0388";
	}
	else if (strcmp(p, "0x2558") == 0) {	// G 4.0, GL 1.0, 1.1
		p = "0x2758";
	}
	else {
		return;
	}

	nvram_set("boardflags", p);

	if (!nvram_match("debug_abrst", "0")) {
		modprobe_r("wl");
		modprobe("wl");
	}


/*	safe?

	unsigned long bf;
	char s[64];

	bf = strtoul(p, &p, 0);
	if ((*p == 0) && ((bf & BFL_AFTERBURNER) == 0)) {
		sprintf(s, "0x%04lX", bf | BFL_AFTERBURNER);
		nvram_set("boardflags", s);
	}
*/
}

void wlconf_pre()
{
	int unit = 0;
	int model = get_model();
	char word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_BCMARM
		if (unit == 0)
		{
			if (model == MODEL_RTN18U || model == MODEL_RTAC68U || model == MODEL_DSLAC68U || model == MODEL_RTAC87U) {
				if (nvram_match(strcat_r(prefix, "turbo_qam", tmp), "1"))
					eval("wl", "-i", word, "vht_features", "3");
				else
					eval("wl", "-i", word, "vht_features", "0");
			}
		}
#endif
		// early convertion for nmode setting
		generate_wl_para(unit, -1);
#ifdef RTCONFIG_QTN
		if (!strcmp(word, "wifi0")) break;
#endif
		if (nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
		{
			dbG("set vhtmode 1\n");
			eval("wl", "-i", word, "vhtmode", "1");
		}
		else
		{
			dbG("set vhtmode 0\n");
			eval("wl", "-i", word, "vhtmode", "0");
		}
#endif
		unit++;
	}
}

void wlconf_post(const char *ifname)
{
	int unit = -1;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	if (ifname == NULL) return;

	// get the instance number of the wl i/f
	if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		return;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#if 0
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_BCMARM
#ifdef RTAC68U
	if (unit == 1)
	{
		if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
		    nvram_match(strcat_r(prefix, "country_rev", tmp), "13"))
			eval("wl", "-i", ifname, "dfs_channel_forced", "36");
	}
#endif
#endif
#endif
#endif
}
#endif

#if defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)
void apcli_start(void)
{
//repeater mode :sitesurvey channel and apclienable=1
	int ch;
	char *aif;
	int ht_ext;

	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER)
	{
		int wlc_band = nvram_get_int("wlc_band");
		if (wlc_band == 0)
			aif=nvram_safe_get("wl0_ifname");
		else
			aif=nvram_safe_get("wl1_ifname");
		ch = site_survey_for_channel(0,aif, &ht_ext);
		if(ch!=-1)
		{
#if defined(RTCONFIG_RALINK_MT7620)
			if (wlc_band == 0)
#else
			if (wlc_band == 1)
#endif
			{
				doSystem("iwpriv apcli0 set Channel=%d", ch);
				doSystem("iwpriv apcli0 set ApCliEnable=1");
			}
			else
			{
				doSystem("iwpriv apclii0 set Channel=%d", ch);
				doSystem("iwpriv apclii0 set ApCliEnable=1");
			}
			fprintf(stderr,"##set channel=%d, enable apcli ..#\n",ch);
		}
		else
			fprintf(stderr,"## Can not find pap's ssid ##\n");
	}
}
#endif	/* RTCONFIG_RALINK && RTCONFIG_WIRELESSREPEATER */

void start_wl(void)
{
#ifdef CONFIG_BCMWL5
	char *lan_ifname, *lan_ifnames, *ifname, *p;
	int unit, subunit;
	int is_client = 0;
	char tmp[100], tmp2[100], prefix[] = "wlXXXXXXXXXXXXXX";

	lan_ifname = nvram_safe_get("lan_ifname");
	if (strncmp(lan_ifname, "br", 2) == 0) {
		if ((lan_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
			p = lan_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;

				unit = -1; subunit = -1;

				// ignore disabled wl vifs
				if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
					char nv[40];
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
					if (!nvram_get_int(nv))
						continue;
					if (get_ifname_unit(ifname, &unit, &subunit) < 0)
						continue;
				}
				// get the instance number of the wl i/f
				else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
					continue;

				is_client |= wl_client(unit, subunit) && nvram_get_int(wl_nvname("radio", unit, 0));

#ifdef CONFIG_BCMWL5
				snprintf(prefix, sizeof(prefix), "wl%d_", unit);
				if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
				{
					nvram_set_int(strcat_r(prefix, "timesched", tmp2), 0);	// disable wifi time-scheduler
#ifdef RTCONFIG_QTN
					if (strcmp(ifname, "wifi0"))
#endif
					{
						eval("wlconf", ifname, "down");
						eval("wl", "-i", ifname, "radio", "off");
					}
				}
				else
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
				if (!is_psta(1 - unit))
#endif
#endif
				{
#ifdef RTCONFIG_QTN
					if (strcmp(ifname, "wifi0"))
#endif
					eval("wlconf", ifname, "start"); /* start wl iface */
				}
				wlconf_post(ifname);
#endif	// CONFIG_BCMWL5
			}
			free(lan_ifnames);
		}
	}
#ifdef CONFIG_BCMWL5
	else if (strcmp(lan_ifname, "")) {
		/* specific non-bridged lan iface */
		eval("wlconf", lan_ifname, "start");
	}
#endif	// CONFIG_BCMWL5

#if 0
	killall("wldist", SIGTERM);
	eval("wldist");
#endif

	if (is_client)
		xstart("radio", "join");

#ifdef RTCONFIG_BCMWL6
#if defined(RTAC66U) || defined(BCM4352)
	if (nvram_match("wl1_radio", "1"))
	{
		nvram_set("led_5g", "1");
#ifdef RTCONFIG_LED_BTN
		if (nvram_get_int("AllLED"))
#endif
		led_control(LED_5G, LED_ON);
	}
	else
	{
		nvram_set("led_5g", "0");
		led_control(LED_5G, LED_OFF);
	}
#ifdef RTCONFIG_TURBO
	if ((nvram_match("wl0_radio", "1") || nvram_match("wl1_radio", "1"))
#ifdef RTCONFIG_LED_BTN
		&& nvram_get_int("AllLED")
#endif
	)
		led_control(LED_TURBO, LED_ON);
	else
		led_control(LED_TURBO, LED_OFF);
#endif
#endif
#endif
#endif /* CONFIG_BCMWL5 */
#ifdef RTCONFIG_USER_LOW_RSSI
	init_wllc();
#endif
}

void stop_wl(void)
{
}

#ifdef CONFIG_BCMWL5
static int set_wlmac(int idx, int unit, int subunit, void *param)
{
	char *ifname;

	ifname = nvram_safe_get(wl_nvname("ifname", unit, subunit));

	// skip disabled wl vifs
	if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.') &&
		!nvram_get_int(wl_nvname("bss_enabled", unit, subunit)))
		return 0;

	set_mac(ifname, wl_nvname("macaddr", unit, subunit),
		2 + unit + ((subunit > 0) ? ((unit + 1) * 0x10 + subunit) : 0));

	return 1;
}
#endif

static int
add_lan_routes(char *lan_ifname)
{
	return add_routes("lan_", "route", lan_ifname);
}

static int
del_lan_routes(char *lan_ifname)
{
	return del_routes("lan_", "route", lan_ifname);
}

#ifdef RTCONFIG_RALINK
static void
gen_ra_config(const char* wif)
{
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (!strcmp(word, wif))
		{
			if (!strcmp(word, nvram_safe_get("wl0_ifname"))) // 2.4G
			{
				if (!strncmp(word, "rai", 3))	// iNIC
					gen_ralink_config(0, 1);
				else
					gen_ralink_config(0, 0);
			}
			else if (!strcmp(word, nvram_safe_get("wl1_ifname"))) // 5G
			{
				if (!strncmp(word, "rai", 3))	// iNIC
					gen_ralink_config(1, 1);
				else
					gen_ralink_config(1, 0);
			}
		}
	}
}

static int
radio_ra(const char *wif, int band, int ctrl)
{
	char tmp[100], prefix[]="wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", band);

	if (wif == NULL) return -1;

	if (!ctrl)
	{
		doSystem("iwpriv %s set RadioOn=0", wif);
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "radio", tmp), "1"))
			doSystem("iwpriv %s set RadioOn=1", wif);
	}

	return 0;
}

static void
set_wlpara_ra(const char* wif, int band)
{
	char tmp[100], prefix[]="wlXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", band);

	if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
		radio_ra(wif, band, 0);
	else
	{
		int txpower = nvram_get_int(strcat_r(prefix, "TxPower", tmp));
		if ((txpower >= 0) && (txpower <= 100))
			doSystem("iwpriv %s set TxPower=%d",wif, txpower);
	}
#if 0
	if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))
	{
		int channel = get_channel(band);

		if (channel)
			eval("iwpriv", (char *)wif, "set", "HtBw=1");
	}
#endif
#if 0 //defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)	/* set RT3352 iNIC in kernel driver to avoid loss setting after reset */
	if(strcmp(wif, "rai0") == 0)
	{
		int i;
		char buf[32];
		eval("iwpriv", (char *)wif, "set", "asiccheck=1");
		for(i = 0; i < 3; i++)
		{
			sprintf(buf, "setVlanId=%d,%d", i + INIC_VLAN_IDX_START, i + INIC_VLAN_ID_START);
			eval("iwpriv", (char *)wif, "switch", buf);	//set vlan id can pass through the switch insided the RT3352.
								//set this before wl connection linu up. Or the traffic would be blocked by siwtch and need to reconnect.
		}
		for(i = 0; i < 5; i++)
		{
			sprintf(buf, "setPortPowerDown=%d,%d", i, 1);
			eval("iwpriv", (char *)wif, "switch", buf);	//power down the Ethernet PHY of RT3352 internal switch.
		}
	}
#endif // RTCONFIG_WLMODULE_RT3352_INIC_MII
	eval("iwpriv", (char *)wif, "set", "IgmpAdd=01:00:5e:7f:ff:fa");
	eval("iwpriv", (char *)wif, "set", "IgmpAdd=01:00:5e:00:00:09");
	eval("iwpriv", (char *)wif, "set", "IgmpAdd=01:00:5e:00:00:fb");
}

char *get_hwaddr(const char *ifname)
{
	int s;
	struct ifreq ifr;
	char eabuf[32];

	if (ifname == NULL) return NULL;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) return NULL;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(s, SIOCGIFHWADDR, &ifr)) return NULL;

	return strdup(ether_etoa(ifr.ifr_hwaddr.sa_data, eabuf));
}

static int
wlconf_ra(const char* wif)
{
	int unit = 0;
	char word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *p;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		if (!strcmp(word, wif))
		{
			p = get_hwaddr(wif);
			if (p)
			{
				nvram_set(strcat_r(prefix, "hwaddr", tmp), p);
				free(p);
			}

			if (!strcmp(word, WIF_2G))
				set_wlpara_ra(wif, 0);
			else if (!strcmp(word, WIF_5G))
				set_wlpara_ra(wif, 1);
		}

		unit++;
	}
	return 0;
}
#endif

#ifdef RTCONFIG_IPV6
void ipv6_sysconf(const char *ifname, const char *name, int value)
{
	char path[PATH_MAX], sval[16];

	if (ifname == NULL || name == NULL) return;

	snprintf(path, sizeof(path), "/proc/sys/net/ipv6/conf/%s/%s", ifname, name);
	snprintf(sval, sizeof(sval), "%d", value);
	f_write_string(path, sval, 0, 0);
}

void set_default_accept_ra(int flag)
{
	ipv6_sysconf("all", "accept_ra", flag ? 1 : 0);
	ipv6_sysconf("default", "accept_ra", flag ? 1 : 0);
}

void set_default_forwarding(int flag)
{
	ipv6_sysconf("all", "forwarding", flag ? 1 : 0);
	ipv6_sysconf("default", "forwarding", flag ? 1 : 0);
}

void set_intf_ipv6_dad(const char *ifname, int addbr, int flag)
{
	if (ifname == NULL) return;

	ipv6_sysconf(ifname, "accept_dad", flag ? 2 : 0);
	if (flag)
		ipv6_sysconf(ifname, "dad_transmits", addbr ? 2 : 1);
}

void enable_ipv6(const char *ifname)
{
	if (ifname == NULL) return;

	ipv6_sysconf(ifname, "disable_ipv6", 0);
}

void disable_ipv6(const char *ifname)
{
	if (ifname == NULL) return;

	ipv6_sysconf(ifname, "disable_ipv6", 1);
}

void config_ipv6(int enable, int incl_wan)
{
	DIR *dir;
	struct dirent *dirent;
	int service;
	int match;
	char word[256], *next;

	if ((dir = opendir("/proc/sys/net/ipv6/conf")) != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
				continue;
			if (incl_wan)
				goto ALL;
			match = 0;
			foreach (word, nvram_safe_get("wan_ifnames"), next) {
				if (!strcmp(dirent->d_name, word))
				{
					match = 1;
					break;
				}
			}
			if (match) continue;
ALL:
			if (enable)
				enable_ipv6(dirent->d_name);
			else
				disable_ipv6(dirent->d_name);

			if (enable && strcmp(dirent->d_name, "all") &&
				strcmp(dirent->d_name, "default") &&
				!with_ipv6_linklocal_addr(dirent->d_name))
				reset_ipv6_linklocal_addr(dirent->d_name, 0);
		}
		closedir(dir);
	}

	if (is_routing_enabled())
	{
		service = get_ipv6_service();
		switch (service) {
		case IPV6_NATIVE:
		case IPV6_NATIVE_DHCP:
			if ((nvram_get_int("ipv6_accept_ra") & 1) != 0)
			{
				set_default_accept_ra(1);
				break;
			}
		case IPV6_6IN4:
		case IPV6_6TO4:
		case IPV6_6RD:
		case IPV6_MANUAL:
		default:
			set_default_accept_ra(0);
			break;
		}

		set_default_forwarding(1);
	}
	else set_default_accept_ra(0);
}
#endif

void start_lan(void)
{
	char *lan_ifname;
	struct ifreq ifr;
	char *lan_ifnames, *ifname, *p;
	char *hostname;
	int sfd;
	uint32 ip;
	int unit, subunit, sta;
	int hwaddrset;
	char eabuf[32];
	char word[256], *next;
	int match;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	int i;
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif

	_dprintf("%s %d\n", __func__, __LINE__);

	update_lan_state(LAN_STATE_INITIALIZING, 0);

	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER){
		nvram_set("wlc_mode", "0");
		nvram_set("btn_ez_radiotoggle", "0"); // reset to default
	}

	set_device_hostname();

	convert_routes();

#ifdef CONFIG_BCMWL5
#ifndef RTCONFIG_BRCM_USBAP
	if ((get_model() == MODEL_RTAC68U) ||
		(get_model() == MODEL_DSLAC68U) ||
		(get_model() == MODEL_RTAC87U) ||
		(get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTAC53U) ||
		(get_model() == MODEL_RTAC53U) ||
		(get_model() == MODEL_RTN66U)) {
		modprobe("wl");
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
		modprobe("proxyarp");
#endif
	}
#endif
	wlconf_pre();
#endif

#ifdef RTCONFIG_RALINK
	init_wl();
#endif

#ifdef RTCONFIG_LED_ALL
	led_control(LED_ALL, LED_ON);
#endif

#ifdef CONFIG_BCMWL5
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTAC56S) ||
		(get_model() == MODEL_RTAC56U) ||
		(get_model() == MODEL_RTAC68U) ||
		(get_model() == MODEL_DSLAC68U) ||
		(get_model() == MODEL_RTAC87U) ||
		(get_model() == MODEL_RTN12HP) ||
		(get_model() == MODEL_APN12HP) ||
		(get_model() == MODEL_RTN66U))
	set_wltxpower();
#endif

#ifdef CONFIG_BCMWL5
	if (0)
	{
		foreach_wif(1, NULL, set_wlmac);
		check_afterburner();
	}
#endif

	if (no_need_to_start_wps())
		nvram_set("wps_enable", "0");

	if ((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) return;

/*#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_match("lan_proto", "dhcp"))
		nvram_set("lan_ipaddr", nvram_default_get("lan_ipaddr"));
#endif//*/

	lan_ifname = strdup(nvram_safe_get("lan_ifname"));
	if (strncmp(lan_ifname, "br", 2) == 0) {
		_dprintf("%s: setting up the bridge %s\n", __FUNCTION__, lan_ifname);

		eval("brctl", "addbr", lan_ifname);
		eval("brctl", "setfd", lan_ifname, "0");
		if (is_routing_enabled())
			eval("brctl", "stp", lan_ifname, nvram_safe_get("lan_stp"));
		else
			eval("brctl", "stp", lan_ifname, "0");
#ifdef RTCONFIG_IPV6
		if ((get_ipv6_service() != IPV6_DISABLED) &&
			(!((nvram_get_int("ipv6_accept_ra") & 2) != 0 && !nvram_get_int("ipv6_radvd"))))
		{
			ipv6_sysconf(lan_ifname, "accept_ra", 0);
			ipv6_sysconf(lan_ifname, "forwarding", 0);
		}
		set_intf_ipv6_dad(lan_ifname, 1, 1);
#endif
#ifdef RTCONFIG_EMF
		if (nvram_get_int("emf_enable")) {
			eval("emf", "add", "bridge", lan_ifname);
			eval("igs", "add", "bridge", lan_ifname);
		}
#endif

		inet_aton(nvram_safe_get("lan_ipaddr"), (struct in_addr *)&ip);

		hwaddrset = 0;
		sta = 0;
		if ((lan_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
			p = lan_ifnames;

			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
				if (strncmp(ifname, "wl", 2) == 0) {
					if (!wl_vif_enabled(ifname, tmp)) {
						continue; /* Ignore disabled WL VIF */
					}
					wl_vif_hwaddr_set(ifname);
				}
#endif
				unit = -1; subunit = -1;

				// ignore disabled wl vifs
#ifdef CONFIG_BCMWL5
				if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.'))
#elif defined RTCONFIG_RALINK
				if (strncmp(ifname, "ra", 2) == 0 && !strchr(ifname, '0'))
#endif
				{
					char nv[40];
					char nv2[40];
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire", wif_to_vif(ifname));
					if(nvram_get_int("sw_mode")==SW_MODE_REPEATER)
					{
						nvram_set(nv,"1");	/*wl0.x_bss_enable=1*/
						nvram_unset(nv2);	/*remove wl0.x_expire*/
					}
					if (nvram_get_int(nv2) && nvram_get_int("success_start_service")==0)
					{
						nvram_set(nv, "0");
					}

					if (!nvram_get_int(nv))
						continue;
#ifdef CONFIG_BCMWL5
					if (get_ifname_unit(ifname, &unit, &subunit) < 0)
						continue;
#endif
				}
#ifdef CONFIG_BCMWL5
				else
					wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit));
#endif

#ifdef RTCONFIG_RALINK
				gen_ra_config(ifname);
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
				{ // add interface for iNIC packets
					char *nic_if, *nic_ifs, *nic_lan_ifnames;
					if((nic_lan_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))))
					{
						nic_ifs = nic_lan_ifnames;
						while ((nic_if = strsep(&nic_ifs, " ")) != NULL) {
							while (*nic_if == ' ')
								nic_if++;
							if (*nic_if == 0)
								break;
							if(strcmp(ifname, nic_if) == 0)
							{
								int vlan_id;
								if((vlan_id = atoi(ifname + 4)) > 0)
								{
									char id[8];
									sprintf(id, "%d", vlan_id);
									eval("vconfig", "add", "eth2", id);
								}
								break;
							}
						}
						free(nic_lan_ifnames);
					}
				}
#endif
				// bring up interface
				if (ifconfig(ifname, IFUP, NULL, NULL) != 0){
#if defined(RTAC56U) || defined(RTAC56S)
					if(strncmp(ifname, "eth2", 4)==0)
						nvram_set("5g_fail", "1");	// gpio led
#endif
#ifdef RTCONFIG_QTN
					if (strcmp(ifname, "wifi0"))
#endif
					continue;
				}
#if defined(RTAC56U) || defined(RTAC56S)
				else if(strncmp(ifname, "eth2", 4)==0)
					nvram_unset("5g_fail");			// gpio led
#endif
#ifdef RTCONFIG_RALINK
				wlconf_ra(ifname);
#endif

				// set the logical bridge address to that of the first interface
				strlcpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
				if ((!hwaddrset) ||
				    (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0 &&
				    memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN) == 0)) {
					strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
					if (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0) {
						strlcpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
						ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
						_dprintf("%s: setting MAC of %s bridge to %s\n", __FUNCTION__,
							ifr.ifr_name, ether_etoa(ifr.ifr_hwaddr.sa_data, eabuf));
						ioctl(sfd, SIOCSIFHWADDR, &ifr);
						hwaddrset = 1;
					}
				}
#ifdef CONFIG_BCMWL5
				if (wlconf(ifname, unit, subunit) == 0) {
					const char *mode = nvram_safe_get(wl_nvname("mode", unit, subunit));

					if (strcmp(mode, "wet") == 0) {
						// Enable host DHCP relay
						if (nvram_get_int("dhcp_relay")) {
							wl_iovar_set(ifname, "wet_host_mac", ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
							wl_iovar_setint(ifname, "wet_host_ipv4", ip);
						}
					}

					sta |= (strcmp(mode, "sta") == 0);
					if ((strcmp(mode, "ap") != 0) && (strcmp(mode, "wet") != 0)
						&& (strcmp(mode, "psta") != 0))
						continue;
				}
#endif
#ifdef RTCONFIG_QTN
				if (!strcmp(ifname, "wifi0"))
					continue;
#endif
				/* Don't attach the main wl i/f in wds mode */
				match = 0, i = 0;
				foreach (word, nvram_safe_get("wl_ifnames"), next) {
					if (!strcmp(ifname, word))
					{
						snprintf(prefix, sizeof(prefix), "wl%d_", i);
						if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1"))
							match = 1;

						break;
					}

					i++;
				}
				if (!match)
				eval("brctl", "addif", lan_ifname, ifname);
#ifdef RTCONFIG_EMF
				if (nvram_get_int("emf_enable"))
					eval("emf", "add", "iface", lan_ifname, ifname);
#endif
			}

			free(lan_ifnames);
		}
	}
	// --- this shouldn't happen ---
	else if (*lan_ifname) {
		ifconfig(lan_ifname, IFUP, NULL, NULL);
#ifdef CONFIG_BCMWL5
		wlconf(lan_ifname, -1, -1);
#endif
	}
	else {
		close(sfd);
		free(lan_ifname);
		return;
	}

	// Get current LAN hardware address
	strlcpy(ifr.ifr_name, lan_ifname, IFNAMSIZ);
	if (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0) nvram_set("lan_hwaddr", ether_etoa(ifr.ifr_hwaddr.sa_data, eabuf));

	close(sfd);
	/* Set initial QoS mode for LAN ports. */
#ifdef CONFIG_BCMWL5
	set_et_qos_mode();
#endif

	// bring up and configure LAN interface
/*#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED)
		ifconfig(lan_ifname, IFUP, nvram_default_get("lan_ipaddr"), nvram_default_get("lan_netmask"));
	else
#endif
		ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));//*/
	if(nvram_match("lan_proto", "static"))
		ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	else
		ifconfig(lan_ifname, IFUP, nvram_default_get("lan_ipaddr"), nvram_default_get("lan_netmask"));

	config_loopback();

#ifdef RTCONFIG_IPV6
	set_intf_ipv6_dad(lan_ifname, 0, 1);
	config_ipv6(ipv6_enabled() && is_routing_enabled(), 0);
	start_ipv6();
#endif

#ifdef RTCONFIG_EMF
	if (nvram_get_int("emf_enable")) start_emf(lan_ifname);
#endif

	if(nvram_match("lan_proto", "dhcp"))
	{
		// only none routing mode need lan_proto=dhcp
		if (pids("udhcpc"))
		{
			killall("udhcpc", SIGUSR2);
			killall("udhcpc", SIGTERM);
			unlink("/tmp/udhcpc_lan");
		}

		hostname = nvram_safe_get("computer_name");

		char *dhcp_argv[] = { "udhcpc",
					"-i", "br0",
					"-p", "/var/run/udhcpc_lan.pid",
					"-s", "/tmp/udhcpc_lan",
					(*hostname != 0 ? "-H" : NULL), (*hostname != 0 ? hostname : NULL),
					NULL };
		pid_t pid;

		symlink("/sbin/rc", "/tmp/udhcpc_lan");
		_eval(dhcp_argv, NULL, 0, &pid);

		update_lan_state(LAN_STATE_CONNECTING, 0);
	}
	else {
		if(is_routing_enabled())
		{
			update_lan_state(LAN_STATE_CONNECTED, 0);
			add_lan_routes(lan_ifname);
			start_default_filter(0);
		}
		else lan_up(lan_ifname);
	}

#ifdef RTCONFIG_SHP
	if(nvram_get_int("lfp_disable")==0) {
		restart_lfp();
	}
#endif

#ifdef WEB_REDIRECT
#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER){
		// Drop the DHCP server from PAP.
		repeater_pap_disable();

		// When CONNECTED, need to redirect 10.0.0.1(from the browser's cache) to DUT's home page.
		repeater_nat_setting();
	}
#endif

	if(nvram_get_int("sw_mode") != SW_MODE_AP) {
		redirect_setting();
		stop_nat_rules();
	}

	start_wanduck();
#endif

#ifdef RTCONFIG_BCMWL6
	set_acs_ifnames();
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode")==SW_MODE_REPEATER) {
		start_wlcconnect();
	}
#endif

	free(lan_ifname);

#ifdef RTCONFIG_WIRELESSREPEATER
	if(get_model() == MODEL_APN12HP &&
		nvram_get_int("sw_mode") == SW_MODE_AP){
		// When CONNECTED, need to redirect 10.0.0.1
		// (from the browser's cache) to DUT's home page.
		repeater_nat_setting();
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		eval("ebtables", "-t", "broute", "-I", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
		sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
		f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);
		start_nat_rules();
	}
#endif

#ifdef RTCONFIG_QTN
	eval("ifconfig", "br0:0", "1.1.1.1", "netmask", "255.255.255.0");
#endif
	nvram_set("reload_svc_radio", "1");

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

#ifdef RTCONFIG_RALINK
static void
stop_wds_ra(const char* lan_ifname, const char* wif)
{
	char prefix[32];
	char wdsif[32];
	int i;

	if (strcmp(wif, WIF_2G) && strcmp(wif, WIF_5G))
		return;

	if (!strncmp(wif, "rai", 3))
		snprintf(prefix, sizeof(prefix), "wdsi");
	else
		snprintf(prefix, sizeof(prefix), "wds");

	for (i = 0; i < 4; i++)
	{
		sprintf(wdsif, "%s%d", prefix, i);
		doSystem("brctl delif %s %s 1>/dev/null 2>&1", lan_ifname, wdsif);
		ifconfig(wdsif, 0, NULL, NULL);
	}
}
#endif

void stop_lan(void)
{
	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	char *lan_ifname;
	char *lan_ifnames, *p, *ifname;

	lan_ifname = nvram_safe_get("lan_ifname");

	if(is_routing_enabled())
	{
		stop_wanduck();
		del_lan_routes(lan_ifname);
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else if(nvram_get_int("sw_mode") == SW_MODE_REPEATER){
		stop_wlcconnect();

		stop_wanduck();
	}
#endif
#ifdef WEB_REDIRECT
	else if (is_apmode_enabled())
		stop_wanduck();
#endif

#ifdef RTCONFIG_IPV6
	stop_ipv6();
	set_intf_ipv6_dad(lan_ifname, 0, 0);
	config_ipv6(ipv6_enabled() && is_routing_enabled(), 1);
#endif

	ifconfig("lo", 0, NULL, NULL);

	ifconfig(lan_ifname, 0, NULL, NULL);

	if (module_loaded("ebtables")) {
		eval("ebtables", "-F");
		eval("ebtables", "-t", "broute", "-F");
	}

	if (strncmp(lan_ifname, "br", 2) == 0) {
#ifdef RTCONFIG_EMF
		stop_emf(lan_ifname);
#endif
		if ((lan_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
			p = lan_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_QTN
				if (strcmp(ifname, "wifi0"))
#endif
				{
					eval("wlconf", ifname, "down");
					eval("wl", "-i", ifname, "radio", "off");
				}
#elif defined RTCONFIG_RALINK
				if (!strncmp(ifname, "ra", 2))
					stop_wds_ra(lan_ifname, ifname);
#endif
				eval("brctl", "delif", lan_ifname, ifname);
				ifconfig(ifname, 0, NULL, NULL);
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
				{ // remove interface for iNIC packets
					char *nic_if, *nic_ifs, *nic_lan_ifnames;
					if((nic_lan_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))))
					{
						nic_ifs = nic_lan_ifnames;
						while ((nic_if = strsep(&nic_ifs, " ")) != NULL) {
							while (*nic_if == ' ')
								nic_if++;
							if (*nic_if == 0)
								break;
							if(strcmp(ifname, nic_if) == 0)
							{
								eval("vconfig", "rem", ifname);
								break;
							}
						}
						free(nic_lan_ifnames);
					}
				}
#endif
			}
			free(lan_ifnames);
		}
		eval("brctl", "delbr", lan_ifname);
	}
	else if (*lan_ifname) {
#ifdef CONFIG_BCMWL5
		eval("wlconf", lan_ifname, "down");
		eval("wl", "-i", lan_ifname, "radio", "off");
#endif
	}

#ifdef RTCONFIG_BCMWL6
#if defined(RTAC66U) || defined(BCM4352)
	nvram_set("led_5g", "0");
	led_control(LED_5G, LED_OFF);
#ifdef RTCONFIG_TURBO
	led_control(LED_TURBO, LED_OFF);
#endif
#endif
#endif

	// inform watchdog to stop WPS LED
	kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);

	fini_wl();

	init_nvram();	// init nvram lan_ifnames
	wl_defaults();	// init nvram wlx_ifnames & lan_ifnames

	update_lan_state(LAN_STATE_STOPPED, 0);

	killall("udhcpc", SIGUSR2);
	killall("udhcpc", SIGTERM);
	unlink("/tmp/udhcpc_lan");

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void do_static_routes(int add)
{
	char *buf;
	char *p, *q;
	char *dest, *mask, *gateway, *metric, *ifname;
	int r;

	if ((buf = strdup(nvram_safe_get(add ? "routes_static" : "routes_static_saved"))) == NULL) return;
	if (add) nvram_set("routes_static_saved", buf);
		else nvram_unset("routes_static_saved");
	p = buf;
	while ((q = strsep(&p, ">")) != NULL) {
		if (vstrsep(q, "<", &dest, &gateway, &mask, &metric, &ifname) != 5) continue;
		ifname = nvram_safe_get((*ifname == 'L') ? "lan_ifname" :
					((*ifname == 'W') ? "wan_iface" : "wan_ifname"));
		if (add) {
			for (r = 3; r >= 0; --r) {
				if (route_add(ifname, atoi(metric) + 1, dest, gateway, mask) == 0) break;
				sleep(1);
			}
		}
		else {
			route_del(ifname, atoi(metric) + 1, dest, gateway, mask);
		}
	}
	free(buf);
}

#ifdef CONFIG_BCMWL5

/*
 * EAP module
 */

static int
wl_send_dif_event(const char *ifname, uint32 event)
{
	static int s = -1;
	int len, n;
	struct sockaddr_in to;
	char data[IFNAMSIZ + sizeof(uint32)];

	if (ifname == NULL) return -1;

	/* create a socket to receive dynamic i/f events */
	if (s < 0) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			perror("socket");
			return -1;
		}
	}

	/* Init the message contents to send to eapd. Specify the interface
	 * and the event that occured on the interface.
	 */
	strncpy(data, ifname, IFNAMSIZ);
	*(uint32 *)(data + IFNAMSIZ) = event;
	len = IFNAMSIZ + sizeof(uint32);

	/* send to eapd */
	to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(EAPD_WKSP_DIF_UDP_PORT);

	n = sendto(s, data, len, 0, (struct sockaddr *)&to,
		sizeof(struct sockaddr_in));

	if (n != len) {
		perror("udp send failed\n");
		return -1;
	}

	_dprintf("hotplug_net(): sent event %d\n", event);

	return n;
}
#endif

void hotplug_net(void)
{
	char *lan_ifname = nvram_safe_get("lan_ifname");
	char *interface, *action;
	bool psta_if, dyn_if, add_event, remove_event;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
#ifdef RTCONFIG_USB_MODEM
	char device_path[128], usb_path[PATH_MAX], usb_node[32], port_path[8];
	char nvram_name[32];
	char word[PATH_MAX], *next;
#ifdef RTCONFIG_USB_BECEEM
	int i, j;
#endif
#endif

	if (!(interface = getenv("INTERFACE")) ||
	    !(action = getenv("ACTION")))
		return;

	_dprintf("hotplug net INTERFACE=%s ACTION=%s\n", interface, action);

#ifdef LINUX26
	add_event = !strcmp(action, "add");
#else
	add_event = !strcmp(action, "register");
#endif

#ifdef LINUX26
	remove_event = !strcmp(action, "remove");
#else
	remove_event = !strcmp(action, "unregister");
#endif

#ifdef RTCONFIG_BCMWL6
	psta_if = wl_wlif_is_psta(interface);
#else
	psta_if = 0;
#endif

	dyn_if = !strncmp(interface, "wds", 3) || psta_if;

	if (!dyn_if && !remove_event)
		goto NEITHER_WDS_OR_PSTA;

	if (dyn_if && add_event) {
#ifdef RTCONFIG_RALINK
		if (nvram_match("sw_mode", "2"))
			return;

		if (strncmp(interface, WDSIF_5G, strlen(WDSIF_5G)) == 0 && isdigit(interface[strlen(WDSIF_5G)]))
		{
			if (nvram_match("wl1_mode_x", "0")) return;
		}
		else
		{
			if (nvram_match("wl0_mode_x", "0")) return;
		}
#endif

		/* Bring up the interface and add to the bridge */
		ifconfig(interface, IFUP, NULL, NULL);

#ifdef RTCONFIG_EMF
		if (nvram_get_int("emf_enable")) {
			eval("emf", "add", "iface", lan_ifname, interface);
			emf_mfdb_update(lan_ifname, interface, 1);
			emf_uffp_update(lan_ifname, interface, 1);
			emf_rtport_update(lan_ifname, interface, 1);
		}
#endif
#ifdef CONFIG_BCMWL5
		/* Indicate interface create event to eapd */
		if (psta_if) {
			_dprintf("hotplug_net(): send dif event to %s\n", interface);
			wl_send_dif_event(interface, 0);
			return;
		}
#endif
		if (!strncmp(lan_ifname, "br", 2)) {
			eval("brctl", "addif", lan_ifname, interface);
#ifdef CONFIG_BCMWL5
			/* Inform driver to send up new WDS link event */
			if (wl_iovar_setint(interface, "wds_enable", 1)) {
				_dprintf("%s set wds_enable failed\n", interface);
				return;
			}
#endif
		}

		return;
	}

#ifdef CONFIG_BCMWL5
	if (remove_event) {
		/* Indicate interface delete event to eapd */
		wl_send_dif_event(interface, 1);

#ifdef RTCONFIG_EMF
		if (nvram_get_int("emf_enable"))
			eval("emf", "del", "iface", lan_ifname, interface);
#endif /* RTCONFIG_EMF */
	}
#endif

NEITHER_WDS_OR_PSTA:
	/* PPP interface removed */
	if (strncmp(interface, "ppp", 3) == 0 && remove_event) {
		while ((unit = ppp_ifunit(interface)) >= 0) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), "");
		}
	}
#ifdef RTCONFIG_USB_MODEM
	// Android phone, RNDIS interface.
	else if(!strncmp(interface, "usb", 3)){
		if(nvram_get_int("sw_mode") != SW_MODE_ROUTER)
			return;

		if ((unit = get_usbif_dualwan_unit()) < 0){
			usb_dbg("(%s): in the current dual wan mode, didn't support the USB modem.\n", interface);
			return;
		}

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			unsigned int vid, pid;

			logmessage("hotplug", "add net %s.", interface);

			memset(device_path, 0, 128);
			sprintf(device_path, "%s/%s/device", SYS_NET, interface);

			memset(usb_path, 0, PATH_MAX);
			if(realpath(device_path, usb_path) == NULL)
				return;

			if(get_usb_node_by_string(usb_path, usb_node, 32) == NULL)
				return;

			if(get_path_by_node(usb_node, port_path, 8) == NULL)
				return;

			vid = get_usb_vid(usb_node);
			pid = get_usb_pid(usb_node);

			memset(nvram_name, 0, 32);
			sprintf(nvram_name, "usb_path%s_act", port_path);
			snprintf(word, PATH_MAX, "%s", nvram_safe_get(nvram_name));

			if(vid == 0x19d2 && pid == 0x0167 && !strcmp(word, "usb1")){
				// ZTE MF821D use QMI:usb1 to connect.
				logmessage("hotplug", "skip to set net %s.", interface);
			}
			else{
				logmessage("hotplug", "set net %s.", interface);
				nvram_set(nvram_name, interface);
				nvram_set("usb_modem_act_path", usb_node);

				nvram_set(strcat_r(prefix, "ifname", tmp), interface);
			}

			// wait for Andorid phones.
			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

#if 0
			eval("find_modem_type.sh");

#ifdef RTCONFIG_DUALWAN
			if(!strcmp(nvram_safe_get("success_start_service"), "1"))
#endif
				if(strcmp(nvram_safe_get("usb_modem_act_type"), "qmi")){
					_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
					start_wan_if(unit);
				}
#endif
		}
		else{
			logmessage("hotplug", "remove net %s.", interface);

			snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
			if(strlen(usb_node) <= 0)
				return;

			if(get_path_by_node(usb_node, port_path, 8) == NULL)
				return;

			snprintf(nvram_name, 32, "usb_path%s_act", port_path);

			if(!strcmp(nvram_safe_get(nvram_name), interface)){
				nvram_unset(nvram_name);
				nvram_unset("usb_modem_act_path");
			}

			if(strlen(port_path) <= 0)
				return;

			nvram_set(strcat_r(prefix, "ifname", tmp), "");

			char dhcp_pid_file[1024];

			memset(dhcp_pid_file, 0, 1024);
			sprintf(dhcp_pid_file, "/var/run/udhcpc%d.pid", unit);

			kill_pidfile_s(dhcp_pid_file, SIGUSR2);
			kill_pidfile_s(dhcp_pid_file, SIGTERM);
		}

		// Notify wanduck to switch the wan line to WAN port.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
	// Beceem dongle, ASIX USB to RJ45 converter, Huawei E353.
	else if(!strncmp(interface, "eth", 3)){
		if(nvram_get_int("sw_mode") != SW_MODE_ROUTER)
			return;

		// Not wired ethernet.
		foreach(word, nvram_safe_get("wan_ifnames"), next)
			if(!strcmp(interface, word))
				return;

		// Not wired ethernet.
		foreach(word, nvram_safe_get("lan_ifnames"), next)
			if(!strcmp(interface, word))
				return;

		// Not wireless ethernet.
		foreach(word, nvram_safe_get("wl_ifnames"), next)
			if(!strcmp(interface, word))
				return;

#ifdef RTCONFIG_RALINK
		// In the Ralink platform eth2 isn't defined at wan_ifnames or other nvrams,
		// so it need to deny additionally.
		if(!strcmp(interface, "eth2"))
			return;
#endif

		if ((unit = get_usbif_dualwan_unit()) < 0){
			usb_dbg("(%s): in the current dual wan mode, didn't support the USB modem.\n", interface);
			return;
		}

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			nvram_set(strcat_r(prefix, "ifname", tmp), interface);

			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

			memset(device_path, 0, 128);
			sprintf(device_path, "%s/%s/device", SYS_NET, interface);

			// Huawei E353.
			if(check_if_dir_exist(device_path)){
				memset(usb_path, 0, PATH_MAX);
				if(realpath(device_path, usb_path) == NULL)
					return;

				if(get_usb_node_by_string(usb_path, usb_node, 32) == NULL)
					return;

				if(get_path_by_node(usb_node, port_path, 8) == NULL)
					return;

				memset(nvram_name, 0, 32);
				sprintf(nvram_name, "usb_path%s_act", port_path);

				if(!strcmp(nvram_safe_get(nvram_name), "")){
					nvram_set(nvram_name, interface);
					nvram_set("usb_modem_act_path", usb_node);
				}
			}
			// Beceem dongle.
			else{
				// do nothing.
			}

#if 0
			if(!strcmp(nvram_safe_get("success_start_service"), "1")){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
#endif
		}
		else{
			nvram_set(strcat_r(prefix, "ifname", tmp), "");

			stop_wan_if(unit);

			snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
			if(strlen(usb_node) <= 0)
				return;

			if(get_path_by_node(usb_node, port_path, 8) == NULL)
				return;

			snprintf(nvram_name, 32, "usb_path%s_act", port_path);

			if(!strcmp(nvram_safe_get(nvram_name), interface)){
				nvram_unset(nvram_name);
				nvram_unset("usb_modem_act_path");
			}

#ifdef RTCONFIG_USB_BECEEM
			if(strlen(port_path) <= 0)
				system("asus_usbbcm usbbcm remove");
#endif
		}

		// Notify wanduck to switch the wan line to WAN port.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
#ifdef RTCONFIG_USB_BECEEM
	else if(!strncmp(interface, "wimax", 5)){
		if(nvram_get_int("sw_mode") != SW_MODE_ROUTER)
			return;

		if ((unit = get_usbif_dualwan_unit()) < 0){
			usb_dbg("(%s): in the current dual wan mode, didn't support the USB modem.\n", interface);
			return;
		}

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			nvram_set(strcat_r(prefix, "ifname", tmp), interface);

			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

			memset(port_path, 0, 8);
			for(i = 1; i <= MAX_USB_PORT; ++i){ // MAX USB port number is 3.
				snprintf(nvram_name, 32, "usb_path%d", i);
				if(!strcmp(nvram_safe_get(nvram_name), "modem")){
					snprintf(port_path, 8, "%d", i);
					break;
				}

				for(j = 1; j <= MAX_USB_HUB_PORT; ++j){
					snprintf(nvram_name, 32, "usb_path%d.%d", i, j);
					if(!strcmp(nvram_safe_get(nvram_name), "modem")){
						snprintf(port_path, 8, "%d.%d", i, j);
						break;
					}
				}

				if(strlen(port_path) > 0)
					break;
			}

			if(strlen(port_path) > 0){
				snprintf(nvram_name, 32, "usb_path%s_act", port_path);
				nvram_set(nvram_name, interface);

				snprintf(nvram_name, 32, "usb_path%s_node", port_path);
				snprintf(usb_node, 32, "%s", nvram_safe_get(nvram_name));

				nvram_set("usb_modem_act_path", usb_node);
			}

#if 0
			if(!strcmp(nvram_safe_get("success_start_service"), "1")){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
#endif
		}
		else{
			nvram_set(strcat_r(prefix, "ifname", tmp), "");

			snprintf(usb_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
			if(strlen(usb_node) <= 0)
				return;

			if(get_path_by_node(usb_node, port_path, 8) == NULL)
				return;

			snprintf(nvram_name, 32, "usb_path%s_act", port_path);

			if(!strcmp(nvram_safe_get(nvram_name), interface)){
				nvram_unset(nvram_name);
				nvram_unset("usb_modem_act_path");
			}

			int i = 0;
			while(i < 3){
				if(pids("madwimax")){
					killall_tk("madwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}

			i = 0;
			while(i < 3){
				if(pids("gctwimax")){
					killall_tk("gctwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}

			modprobe_r("tun");
		}

		// Notify wanduck to switch the wan line to WAN port.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
#endif
#endif
}

#ifdef CONFIG_BCMWL5
static int is_same_addr(struct ether_addr *addr1, struct ether_addr *addr2)
{
	int i;
	for (i = 0; i < 6; i++) {
		if (addr1->octet[i] != addr2->octet[i])
			return 0;
	}
	return 1;
}

#define WL_MAX_ASSOC	128
static int check_wl_client(char *ifname, int unit, int subunit)
{
	struct ether_addr bssid;
	wl_bss_info_t *bi;
	char buf[WLC_IOCTL_MAXLEN];
	struct maclist *mlist;
	int mlsize, i;
	int associated, authorized;

	*(uint32 *)buf = WLC_IOCTL_MAXLEN;
	if (wl_ioctl(ifname, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN) < 0 ||
	    wl_ioctl(ifname, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN) < 0)
		return 0;

	bi = (wl_bss_info_t *)(buf + 4);
	if ((bi->SSID_len == 0) ||
	    (bi->BSSID.octet[0] + bi->BSSID.octet[1] + bi->BSSID.octet[2] +
	     bi->BSSID.octet[3] + bi->BSSID.octet[4] + bi->BSSID.octet[5] == 0))
		return 0;

	associated = 0;
	authorized = strstr(nvram_safe_get(wl_nvname("akm", unit, subunit)), "psk") == 0;

	mlsize = sizeof(struct maclist) + (WL_MAX_ASSOC * sizeof(struct ether_addr));
	if ((mlist = malloc(mlsize)) != NULL) {
		mlist->count = WL_MAX_ASSOC;
		if (wl_ioctl(ifname, WLC_GET_ASSOCLIST, mlist, mlsize) == 0) {
			for (i = 0; i < mlist->count; ++i) {
				if (is_same_addr(&mlist->ea[i], &bi->BSSID)) {
					associated = 1;
					break;
				}
			}
		}

		if (associated && !authorized) {
			memset(mlist, 0, mlsize);
			mlist->count = WL_MAX_ASSOC;
			strcpy((char*)mlist, "autho_sta_list");
			if (wl_ioctl(ifname, WLC_GET_VAR, mlist, mlsize) == 0) {
				for (i = 0; i < mlist->count; ++i) {
					if (is_same_addr(&mlist->ea[i], &bi->BSSID)) {
						authorized = 1;
						break;
					}
				}
			}
		}
		free(mlist);
	}
	return (associated && authorized);
}
#else /* ! CONFIG_BCMWL5 */
static int check_wl_client(char *ifname, int unit, int subunit)
{
// add ralink client check code here
	return 0;
}
#endif /* CONFIG_BCMWL5 */

#define STACHECK_CONNECT	30
#define STACHECK_DISCONNECT	5

static int radio_join(int idx, int unit, int subunit, void *param)
{
	int i;
	char s[32], f[64];
	char *ifname;

	int *unit_filter = param;
	if (*unit_filter >= 0 && *unit_filter != unit) return 0;

	if (!nvram_get_int(wl_nvname("radio", unit, 0)) || !wl_client(unit, subunit)) return 0;

	ifname = nvram_safe_get(wl_nvname("ifname", unit, subunit));

	// skip disabled wl vifs
	if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.') &&
		!nvram_get_int(wl_nvname("bss_enabled", unit, subunit)))
		return 0;

	sprintf(f, "/var/run/radio.%d.%d.pid", unit, subunit < 0 ? 0 : subunit);
	if (f_read_string(f, s, sizeof(s)) > 0) {
		if ((i = atoi(s)) > 1) {
			kill(i, SIGTERM);
			sleep(1);
		}
	}

	if (fork() == 0) {
		sprintf(s, "%d", getpid());
		f_write(f, s, sizeof(s), 0, 0644);

		int stacheck_connect = nvram_get_int("sta_chkint");
		if (stacheck_connect <= 0)
			stacheck_connect = STACHECK_CONNECT;
		int stacheck;

		while (get_radio(unit, 0) && wl_client(unit, subunit)) {

			if (check_wl_client(ifname, unit, subunit)) {
				stacheck = stacheck_connect;
			}
			else {
#ifdef RTCONFIG_RALINK
// add ralink client mode code here.
#else
				eval("wl", "-i", ifname, "disassoc");
#ifdef CONFIG_BCMWL5
				char *amode, *sec = nvram_safe_get(wl_nvname("akm", unit, subunit));

				if (strstr(sec, "psk2")) amode = "wpa2psk";
				else if (strstr(sec, "psk")) amode = "wpapsk";
				else if (strstr(sec, "wpa2")) amode = "wpa2";
				else if (strstr(sec, "wpa")) amode = "wpa";
				else if (nvram_get_int(wl_nvname("auth", unit, subunit))) amode = "shared";
				else amode = "open";

				eval("wl", "-i", ifname, "join", nvram_safe_get(wl_nvname("ssid", unit, subunit)),
					"imode", "bss", "amode", amode);
#else
				eval("wl", "-i", ifname, "join", nvram_safe_get(wl_nvname("ssid", unit, subunit)));
#endif
#endif
				stacheck = STACHECK_DISCONNECT;
			}
			sleep(stacheck);
		}
		unlink(f);
	}

	return 1;
}

enum {
	RADIO_OFF = 0,
	RADIO_ON = 1,
	RADIO_TOGGLE = 2,
	RADIO_SWITCH = 3
};

static int radio_toggle(int idx, int unit, int subunit, void *param)
{
	if (!nvram_get_int(wl_nvname("radio", unit, 0))) return 0;

	int *op = param;

	if (*op == RADIO_TOGGLE) {
		*op = get_radio(unit, subunit) ? RADIO_OFF : RADIO_ON;
	}

	set_radio(*op, unit, subunit);
	return *op;
}

#ifdef RTCONFIG_BCMWL6
static void led_bh(int sw)
{
	switch (get_model()) {
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
			if(sw)
			{
				eval("wl", "ledbh", "3", "7");
				led_control(LED_5G, LED_ON);
			}
			else
			{
				eval("wl", "ledbh", "3", "0");
				led_control(LED_5G, LED_OFF);
			}
			break;
		case MODEL_DSLAC68U:
		case MODEL_RTAC68U:
		case MODEL_RTAC87U:
			if(sw)
			{
				eval("wl", "ledbh", "10", "7");
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
				led_control(LED_5G, LED_ON);
#ifdef RTCONFIG_TURBO
				led_control(LED_TURBO, LED_ON);
#endif
			}
			else
			{
				eval("wl", "ledbh", "10", "0");
				eval("wl", "-i", "eth2", "ledbh", "10", "0");
				led_control(LED_5G, LED_OFF);
#ifdef RTCONFIG_TURBO
				led_control(LED_TURBO, LED_OFF);
#endif
			}
			break;
		default:
			break;
	}
}

static void led_bh_prep(int post)
{
	switch (get_model()) {
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
			if(post)
			{
				eval("wl", "ledbh", "3", "7");
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
			}
			else
			{
				eval("wl", "ledbh", "3", "1");
				eval("wl", "-i", "eth2", "ledbh", "10", "1");
				led_control(LED_5G, LED_ON);
				eval("wlconf", "eth1", "up");
				eval("wl", "maxassoc", "0");
				eval("wlconf", "eth2", "up");
				eval("wl", "-i", "eth2", "maxassoc", "0");
			}
			break;
		case MODEL_DSLAC68U:
		case MODEL_RTAC68U:
		case MODEL_RTAC87U:
			if(post)
			{
				eval("wl", "ledbh", "10", "7");
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
			}
			else
			{
				eval("wl", "ledbh", "10", "1");
				eval("wl", "-i", "eth2", "ledbh", "10", "1");
				led_control(LED_5G, LED_ON);
#ifdef RTCONFIG_TURBO
				led_control(LED_TURBO, LED_ON);
#endif
				eval("wlconf", "eth1", "up");
				eval("wl", "maxassoc", "0");
				eval("wlconf", "eth2", "up");
				eval("wl", "-i", "eth2", "maxassoc", "0");
			}
			break;
		case MODEL_RTAC53U:
			if(post)
			{
				eval("wl", "-i", "eth1", "ledbh", "3", "7");
				eval("wl", "-i", "eth2", "ledbh", "9", "7");
			}
			else
			{
				eval("wl", "-i", "eth1", "ledbh", "3", "1");
				eval("wl", "-i", "eth2", "ledbh", "9", "1");
				eval("wlconf", "eth1", "up");
				eval("wl", "maxassoc", "0");
				eval("wlconf", "eth2", "up");
				eval("wl", "-i", "eth2", "maxassoc", "0");
			}
			break;
		default:
			break;
	}
}
#endif

static int radio_switch(int subunit)
{
	char tmp[100], tmp2[100], prefix[] = "wlXXXXXXXXXXXXXX";
	char *p;
	int i;		// unit
	int sw = 1;	// record get_radio status
	int MAX = 0;	// if MAX = 1: single band, 2: dual band

#ifdef RTCONFIG_WIRELESSREPEATER
	// repeater mode not support HW radio
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER){
		dbG("[radio switch] repeater mode not support HW radio\n");
		return -1;
	}
#endif

	foreach(tmp, nvram_safe_get("wl_ifnames"), p)
		MAX++;

	for(i = 0; i < MAX; i++){
		sw &= get_radio(i, subunit);
	}

	sw = !sw;

	for(i = 0; i < MAX; i++){
		// set wlx_radio
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		nvram_set_int(strcat_r(prefix, "radio", tmp), sw);
		//dbG("[radio switch] %s=%d, MAX=%d\n", tmp, sw, MAX); // radio switch
		set_radio(sw, i, subunit); 	// set wifi radio

		// trigger wifi via WPS/HW toggle, no matter disable or enable wifi, must disable wifi time-scheduler
		// prio : HW switch > WPS/HW toggle > time-scheduler
		nvram_set_int(strcat_r(prefix, "timesched", tmp2), 0);	// disable wifi time-scheduler
		//dbG("[radio switch] %s=%s\n", tmp2, nvram_safe_get(tmp2));
	}

	// commit to flash once */
	nvram_commit();
#ifdef RTCONFIG_BCMWL6
	if (sw) led_bh_prep(0);	// early turn wl led on
#endif
	// make sure all interfaces work well
	notify_rc("restart_wireless");
#ifdef RTCONFIG_BCMWL6
	if (sw) led_bh_prep(1); // restore ledbh if needed
#endif
	return 0;
}

int radio_main(int argc, char *argv[])
{
	int op = RADIO_OFF;
	int unit;
	int subunit;

	if (argc < 2) {
HELP:
		usage_exit(argv[0], "on|off|toggle|switch|join [N]\n");
	}
	unit = (argc >= 3) ? atoi(argv[2]) : -1;
	subunit = (argc >= 4) ? atoi(argv[3]) : 0;

	if (strcmp(argv[1], "toggle") == 0)
		op = RADIO_TOGGLE;
	else if (strcmp(argv[1], "off") == 0)
		op = RADIO_OFF;
	else if (strcmp(argv[1], "on") == 0)
		op = RADIO_ON;
	else if (strcmp(argv[1], "switch") == 0){
		op = RADIO_SWITCH;
		goto SWITCH;
	}
	else if (strcmp(argv[1], "join") == 0)
		goto JOIN;
	else
		goto HELP;

	if (unit >= 0)
		op = radio_toggle(0, unit, subunit, &op);
	else
		op = foreach_wif(0, &op, radio_toggle);

	if (!op) {
		//led(LED_DIAG, 0);
		return 0;
	}
JOIN:
	foreach_wif(1, &unit, radio_join);
SWITCH:
	if(op == RADIO_SWITCH){
		radio_switch(subunit);
	}

	return 0;
}

#if 0
/*
int wdist_main(int argc, char *argv[])
{
	int n;
	rw_reg_t r;
	int v;

	if (argc != 2) {
		r.byteoff = 0x684;
		r.size = 2;
		if (wl_ioctl(nvram_safe_get("wl_ifname"), 101, &r, sizeof(r)) == 0) {
			v = r.val - 510;
			if (v <= 9) v = 0;
				else v = (v - (9 + 1)) * 150;
			printf("Current: %d-%dm (0x%02x)\n\n", v + (v ? 1 : 0), v + 150, r.val);
		}
		usage_exit(argv[0], "<meters>");
	}
	if ((n = atoi(argv[1])) <= 0) setup_wldistance();
		else set_wldistance(n);
	return 0;
}
*/
static int get_wldist(int idx, int unit, int subunit, void *param)
{
	int n;

	char *p = nvram_safe_get(wl_nvname("distance", unit, 0));
	if ((*p == 0) || ((n = atoi(p)) < 0)) return 0;

	return (9 + (n / 150) + ((n % 150) ? 1 : 0));
}

static int wldist(int idx, int unit, int subunit, void *param)
{
	rw_reg_t r;
	uint32 s;
	char *p;
	int n;

	n = get_wldist(idx, unit, subunit, param);
	if (n > 0) {
		s = 0x10 | (n << 16);
		p = nvram_safe_get(wl_nvname("ifname", unit, 0));
		wl_ioctl(p, 197, &s, sizeof(s));

		r.byteoff = 0x684;
		r.val = n + 510;
		r.size = 2;
		wl_ioctl(p, 102, &r, sizeof(r));
	}
	return 0;
}

// ref: wificonf.c
int wldist_main(int argc, char *argv[])
{
	if (fork() == 0) {
		if (foreach_wif(0, NULL, get_wldist) == 0) return 0;

		while (1) {
			foreach_wif(0, NULL, wldist);
			sleep(2);
		}
	}

	return 0;
}
#endif
int
update_lan_resolvconf(void)
{
	FILE *fp;
	char word[256], *next;
	int lock, dup_dns = 0;
	char *lan_dns, *lan_gateway;

	lock = file_lock("resolv");

	if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
		perror("/tmp/resolv.conf");
		file_unlock(lock);
		return errno;
	}

	lan_dns = nvram_safe_get("lan_dns");
	lan_gateway = nvram_safe_get("lan_gateway");
#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED)
		fprintf(fp, "nameserver %s\n", nvram_default_get("lan_ipaddr"));
	else
#endif
	{
		foreach(word, lan_dns, next) {
			if (!strcmp(word, lan_gateway))
				dup_dns = 1;
			fprintf(fp, "nameserver %s\n", word);
		}
	}

	if (*lan_gateway != '\0' && !dup_dns)
		fprintf(fp, "nameserver %s\n", lan_gateway);

	fclose(fp);

	file_unlock(lock);
	return 0;
}


void
lan_up(char *lan_ifname)
{
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif

	_dprintf("%s(%s)\n", __FUNCTION__, lan_ifname);

	restart_dnsmasq(0);

	update_lan_resolvconf();

	/* Set default route to gateway if specified */
	if(nvram_get_int("sw_mode") != SW_MODE_REPEATER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") == WLC_STATE_CONNECTED)
#endif
			){
		route_add(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

		/* Sync time */
		if (!pids("ntp"))
		{
			stop_ntpc();
			start_ntpc();
		}
		else
			kill_pidfile_s("/var/run/ntp.pid", SIGALRM);
	}

	/* Scan new subnetwork */
	stop_networkmap();
	start_networkmap(0);
	update_lan_state(LAN_STATE_CONNECTED, 0);

#ifdef RTCONFIG_WIRELESSREPEATER
	// when wlc_mode = 0 & wlc_state = WLC_STATE_CONNECTED, don't notify wanduck yet.
	// when wlc_mode = 1 & wlc_state = WLC_STATE_CONNECTED, need to notify wanduck.
	// When wlc_mode = 1 & lan_up, need to set wlc_state be WLC_STATE_CONNECTED always.
	// wlcconnect often set the wlc_state too late.
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_mode") == 1){
		repeater_nat_setting();
		nvram_set_int("wlc_state", WLC_STATE_CONNECTED);

		logmessage("notify wanduck", "wlc_state change!");
		_dprintf("%s: notify wanduck: wlc_state=%d.\n", __FUNCTION__, nvram_get_int("wlc_state"));
		// notify the change to wanduck.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
	}
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if(get_model() == MODEL_APN12HP &&
		nvram_get_int("sw_mode") == SW_MODE_AP){
		repeater_nat_setting();
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		eval("ebtables", "-t", "broute", "-I", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
		sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
		f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);
		start_nat_rules();
	}
#endif

#ifdef RTCONFIG_USB
#ifdef RTCONFIG_MEDIA_SERVER
	if(get_invoke_later()&INVOKELATER_DMS)
		notify_rc("restart_dms");
#endif
#endif
}

void
lan_down(char *lan_ifname)
{
	_dprintf("%s(%s)\n", __FUNCTION__, lan_ifname);

	/* Remove default route to gateway if specified */
	route_del(lan_ifname, 0, "0.0.0.0",
			nvram_safe_get("lan_gateway"),
			"0.0.0.0");

	/* Clear resolv.conf */
	f_write("/tmp/resolv.conf", NULL, 0, 0, 0);

	update_lan_state(LAN_STATE_STOPPED, 0);
}

void stop_lan_wl(void)
{
	char *p, *ifname;
	char *wl_ifnames;
	char *lan_ifname;
#ifdef CONFIG_BCMWL5
	int unit, subunit;
#endif

	if (module_loaded("ebtables")) {
		eval("ebtables", "-F");
		eval("ebtables", "-t", "broute", "-F");
	}

	lan_ifname = nvram_safe_get("lan_ifname");
	if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_QTN
			if (!strcmp(ifname, "wifi0")) continue;
#endif
			if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
				if (get_ifname_unit(ifname, &unit, &subunit) < 0)
					continue;
			}
			else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				continue;

			eval("wlconf", ifname, "down");
			eval("wl", "-i", ifname, "radio", "off");
#elif defined RTCONFIG_RALINK
			if (!strncmp(ifname, "ra", 2))
				stop_wds_ra(lan_ifname, ifname);
#endif
#ifdef RTCONFIG_EMF
			eval("emf", "del", "iface", lan_ifname, ifname);
#endif
			eval("brctl", "delif", lan_ifname, ifname);
			ifconfig(ifname, 0, NULL, NULL);

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
			{ // remove interface for iNIC packets
				char *nic_if, *nic_ifs, *nic_lan_ifnames;
				if((nic_lan_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))))
				{
					nic_ifs = nic_lan_ifnames;
					while ((nic_if = strsep(&nic_ifs, " ")) != NULL) {
						while (*nic_if == ' ')
							nic_if++;
						if (*nic_if == 0)
							break;
						if(strcmp(ifname, nic_if) == 0)
						{
							eval("vconfig", "rem", ifname);
							break;
						}
					}
					free(nic_lan_ifnames);
				}
			}
#endif
		}

		free(wl_ifnames);
	}

#ifdef RTCONFIG_BCMWL6
#if defined(RTAC66U) || defined(BCM4352)
	nvram_set("led_5g", "0");
	led_control(LED_5G, LED_OFF);
#ifdef RTCONFIG_TURBO
	led_control(LED_TURBO, LED_OFF);
#endif
#endif
#endif

	// inform watchdog to stop WPS LED
	kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);

	fini_wl();
}

#ifdef RTCONFIG_RALINK
pid_t pid_from_file(char *pidfile)
{
	FILE *fp;
	char buf[256];
	pid_t pid = 0;

	if ((fp = fopen(pidfile, "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp))
			pid = strtoul(buf, NULL, 0);

		fclose(fp);
	}

	return pid;
}
#endif

void start_lan_wl(void)
{
	char *lan_ifname;
	char *wl_ifnames, *ifname, *p;
	uint32 ip;
	int unit, subunit, sta;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	char word[256], *next;
	int match;
	int i;
#ifdef CONFIG_BCMWL5
	struct ifreq ifr;
#endif

#ifdef CONFIG_BCMWL5
#ifndef RTCONFIG_BRCM_USBAP
	if ((get_model() == MODEL_RTAC68U) ||
		(get_model() == MODEL_DSLAC68U) ||
		(get_model() == MODEL_RTAC87U) ||
		(get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U)) {
		modprobe("wl");
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
		modprobe("proxyarp");
#endif
	}
#endif
	wlconf_pre();
#endif

#ifdef RTCONFIG_RALINK
	init_wl();
#endif

#ifdef CONFIG_BCMWL5
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTAC56S) ||
		(get_model() == MODEL_RTAC56U) ||
		(get_model() == MODEL_RTAC68U) ||
		(get_model() == MODEL_DSLAC68U) ||
		(get_model() == MODEL_RTAC87U) ||
		(get_model() == MODEL_RTN12HP) ||
		(get_model() == MODEL_APN12HP) ||
		(get_model() == MODEL_RTN66U))
	set_wltxpower();
#endif

#ifdef CONFIG_BCMWL5
	if (0)
	{
		foreach_wif(1, NULL, set_wlmac);
		check_afterburner();
	}
#endif

	if (no_need_to_start_wps())
		nvram_set("wps_enable", "0");

	lan_ifname = strdup(nvram_safe_get("lan_ifname"));
	if (strncmp(lan_ifname, "br", 2) == 0) {
		inet_aton(nvram_safe_get("lan_ipaddr"), (struct in_addr *)&ip);

		sta = 0;
		if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
			p = wl_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
				if (strncmp(ifname, "wl", 2) == 0) {
					if (!wl_vif_enabled(ifname, tmp)) {
						continue; /* Ignore disabled WL VIF */
					}
					wl_vif_hwaddr_set(ifname);
				}
#endif
				unit = -1; subunit = -1;

				// ignore disabled wl vifs
#ifdef CONFIG_BCMWL5
				if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.'))
#elif defined RTCONFIG_RALINK
				if (strncmp(ifname, "ra", 2) == 0 && !strchr(ifname, '0'))
#endif
				{
					char nv[40];
					char nv2[40];
					char nv3[40];
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
					if (!nvram_get_int(nv))
					{
						continue;
					}
					else
					{
						if (!nvram_get(strcat_r(prefix, "lanaccess", tmp)))
							nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");

						if ((nvram_get_int("wl_unit") >= 0) && (nvram_get_int("wl_subunit") > 0))
						{
							snprintf(nv, sizeof(nv) - 1, "%s_expire", wif_to_vif(ifname));
							snprintf(nv2, sizeof(nv2) - 1, "%s_expire_tmp", wif_to_vif(ifname));
							snprintf(nv3, sizeof(nv3) - 1, "wl%d.%d", nvram_get_int("wl_unit"), nvram_get_int("wl_subunit"));
							if (!strcmp(nv3, wif_to_vif(ifname)))
							{
								nvram_set(nv2, nvram_safe_get(nv));
								nvram_set("wl_unit", "-1");
								nvram_set("wl_subunit", "-1");
							}
						}
					}
#ifdef CONFIG_BCMWL5
					if (get_ifname_unit(ifname, &unit, &subunit) < 0)
						continue;
#endif
				}
#ifdef CONFIG_BCMWL5
				else
					wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit));
#endif

#ifdef RTCONFIG_RALINK
				gen_ra_config(ifname);
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
				{ // add interface for iNIC packets
					char *nic_if, *nic_ifs, *nic_lan_ifnames;
					if((nic_lan_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))))
					{
						nic_ifs = nic_lan_ifnames;
						while ((nic_if = strsep(&nic_ifs, " ")) != NULL) {
							while (*nic_if == ' ')
								nic_if++;
							if (*nic_if == 0)
								break;
							if(strcmp(ifname, nic_if) == 0)
							{
								int vlan_id;
								if((vlan_id = atoi(ifname + 4)) > 0)
								{
									char id[8];
									sprintf(id, "%d", vlan_id);
									eval("vconfig", "add", "eth2", id);
								}
								break;
							}
						}
						free(nic_lan_ifnames);
					}
				}
#endif
				// bring up interface
				if (ifconfig(ifname, IFUP, NULL, NULL) != 0) {
#ifdef RTCONFIG_QTN
					if (strcmp(ifname, "wifi0"))
#endif
						continue;
				}

#ifdef RTCONFIG_RALINK
				wlconf_ra(ifname);
#elif defined CONFIG_BCMWL5
				if (wlconf(ifname, unit, subunit) == 0) {
					const char *mode = nvram_safe_get(wl_nvname("mode", unit, subunit));

					if (strcmp(mode, "wet") == 0) {
						// Enable host DHCP relay
						if (nvram_get_int("dhcp_relay")) {
							wl_iovar_set(ifname, "wet_host_mac", ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
							wl_iovar_setint(ifname, "wet_host_ipv4", ip);
						}
					}

					sta |= (strcmp(mode, "sta") == 0);
					if ((strcmp(mode, "ap") != 0) && (strcmp(mode, "wet") != 0)
						&& (strcmp(mode, "psta") != 0))
						continue;
				}
#endif
				/* Don't attach the main wl i/f in wds mode */
				match = 0, i = 0;
				foreach (word, nvram_safe_get("wl_ifnames"), next) {
					if (!strcmp(ifname, word))
					{
						snprintf(prefix, sizeof(prefix), "wl%d_", i);
						if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1"))
							match = 1;

						break;
					}

					i++;
				}
				if (!match)
				{
					eval("brctl", "addif", lan_ifname, ifname);
#ifdef RTCONFIG_EMF
					if (nvram_get_int("emf_enable"))
						eval("emf", "add", "iface", lan_ifname, ifname);
#endif
				}
			}
			free(wl_ifnames);
		}
	}

#ifdef RTCONFIG_BCMWL6
	set_acs_ifnames();
#endif

	free(lan_ifname);

	nvram_set("reload_svc_radio", "1");
}

void restart_wl(void)
{
#ifdef CONFIG_BCMWL5
	char *wl_ifnames, *ifname, *p;
	int unit, subunit;
	int is_client = 0;
	char tmp[100], tmp2[100], prefix[] = "wlXXXXXXXXXXXXXX";

	if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;

			unit = -1; subunit = -1;

			// ignore disabled wl vifs
			if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
				char nv[40];
				snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
				if (!nvram_get_int(nv))
					continue;
				if (get_ifname_unit(ifname, &unit, &subunit) < 0)
					continue;
			}
			// get the instance number of the wl i/f
			else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				continue;

			is_client |= wl_client(unit, subunit) && nvram_get_int(wl_nvname("radio", unit, 0));

#ifdef CONFIG_BCMWL5
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
			{
				nvram_set_int(strcat_r(prefix, "timesched", tmp2), 0);	// disable wifi time-scheduler
#ifdef RTCONFIG_QTN
				if (strcmp(ifname, "wifi0"))
#endif
				{
					eval("wlconf", ifname, "down");
					eval("wl", "-i", ifname, "radio", "off");
				}
			}
			else
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
			if (!is_psta(1 - unit))
#endif
#endif
			{
#ifdef RTCONFIG_QTN
				if (strcmp(ifname, "wifi0"))
#endif
				eval("wlconf", ifname, "start"); /* start wl iface */
			}
			wlconf_post(ifname);
#endif	// CONFIG_BCMWL5
		}
		free(wl_ifnames);
	}

#if 0
	killall("wldist", SIGTERM);
	eval("wldist");
#endif

	if (is_client)
		xstart("radio", "join");

#ifdef RTCONFIG_BCMWL6
#if defined(RTAC66U) || defined(BCM4352)
	if (nvram_match("wl1_radio", "1"))
	{
		nvram_set("led_5g", "1");
nvram_set("rmtest","1");
#ifdef RTCONFIG_LED_BTN
		if (nvram_get_int("AllLED"))
#endif
		led_control(LED_5G, LED_ON);
	}
	else
	{
		nvram_set("led_5g", "0");
nvram_set("rmtest", "0");
		led_control(LED_5G, LED_OFF);
	}
#ifdef RTCONFIG_TURBO
	if ((nvram_match("wl0_radio", "1") || nvram_match("wl1_radio", "1"))
#ifdef RTCONFIG_LED_BTN
		&& nvram_get_int("AllLED")
#endif
	)
		led_control(LED_TURBO, LED_ON);
	else
		led_control(LED_TURBO, LED_OFF);
#endif
#endif
#endif
#endif /* CONFIG_BCMWL5 */
#ifdef RTCONFIG_USER_LOW_RSSI
	init_wllc();
#endif
}

void lanaccess_mssid_ban(const char *limited_ifname)
{
	char lan_subnet[32];

	if (limited_ifname == NULL) return;

	if (nvram_get_int("sw_mode") != SW_MODE_ROUTER) return;

	eval("ebtables", "-A", "FORWARD", "-i", (char*)limited_ifname, "-j", "DROP"); //ebtables FORWARD: "for frames being forwarded by the bridge"
	eval("ebtables", "-A", "FORWARD", "-o", (char*)limited_ifname, "-j", "DROP"); // so that traffic via host and nat is passed

	snprintf(lan_subnet, sizeof(lan_subnet), "%s/%s", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", (char*)limited_ifname, "--ip-dst", lan_subnet, "--ip-proto", "tcp", "-j", "DROP");
}

void lanaccess_wl(void)
{
	char *p, *ifname;
	char *wl_ifnames;
#ifdef CONFIG_BCMWL5
	int unit, subunit;
#endif

	if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;
#ifdef CONFIG_BCMWL5
			if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
				if (get_ifname_unit(ifname, &unit, &subunit) < 0)
					continue;
			}
			else continue;

#ifdef RTCONFIG_WIRELESSREPEATER
			if(nvram_get_int("sw_mode")==SW_MODE_REPEATER && unit==nvram_get_int("wlc_band") && subunit==1) continue;
#endif
#elif defined RTCONFIG_RALINK
			if (!strncmp(ifname, "ra", 2) && !strchr(ifname, '0'))
				;
			else
				continue;
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
			if (strncmp(ifname, "rai", 3) == 0)
				continue;
#endif

			char nv[40];
			snprintf(nv, sizeof(nv) - 1, "%s_lanaccess", wif_to_vif(ifname));
			if (!strcmp(nvram_safe_get(nv), "off"))
				lanaccess_mssid_ban(ifname);
		}
		free(wl_ifnames);
	}
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if ((wl_ifnames = strdup(nvram_safe_get("nic_lan_ifnames"))) != NULL) {
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;

			lanaccess_mssid_ban(ifname);
		}
		free(wl_ifnames);
	}
#endif
	setup_leds();   // Refresh LED state if in Stealth Mode
}

void restart_wireless(void)
{
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif
	int lock = file_lock("wireless");

	nvram_set_int("wlready", 0);

	stop_wps();
#ifdef CONFIG_BCMWL5
	stop_nas();
	stop_eapd();
#elif defined RTCONFIG_RALINK
	stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
	stop_acsd();
#endif
	stop_lan_wl();
	init_nvram();	// init nvram lan_ifnames
	wl_defaults();	// init nvram wlx_ifnames & lan_ifnames
	sleep(2);	// delay to avoid start interface on stoping.
	start_lan_wl();

	reinit_hwnat(-1);

#ifdef CONFIG_BCMWL5
	start_eapd();
	start_nas();
#elif defined RTCONFIG_RALINK
	start_8021x();
#endif
	start_wps();
#ifdef RTCONFIG_BCMWL6
	start_acsd();
#endif
#ifdef RTCONFIG_WL_AUTO_CHANNEL
	if(nvram_match("AUTO_CHANNEL", "1")){
		nvram_set("wl_channel", "6");
		nvram_set("wl0_channel", "6");
	}
#endif
	restart_wl();
	lanaccess_wl();

#ifdef CONFIG_BCMWL5
	/* for MultiSSID */
	if(!strcmp(nvram_safe_get("qos_enable"), "1")){
		del_EbtablesRules(); // flush ebtables nat table
		add_EbtablesRules();
	}
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	// when wlc_mode = 0 & wlc_state = WLC_STATE_CONNECTED, don't notify wanduck yet.
	// when wlc_mode = 1 & wlc_state = WLC_STATE_CONNECTED, need to notify wanduck.
	// When wlc_mode = 1 & lan_up, need to set wlc_state be WLC_STATE_CONNECTED always.
	// wlcconnect often set the wlc_state too late.
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_mode") == 1){
		repeater_nat_setting();
		nvram_set_int("wlc_state", WLC_STATE_CONNECTED);

		logmessage("notify wanduck", "wlc_state change!");
		_dprintf("%s: notify wanduck: wlc_state=%d.\n", __FUNCTION__, nvram_get_int("wlc_state"));
		// notify the change to wanduck.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
	}

	if(get_model() == MODEL_APN12HP &&
		nvram_get_int("sw_mode") == SW_MODE_AP){
		repeater_nat_setting();
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		eval("ebtables", "-t", "broute", "-I", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
		sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
		f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);
		start_nat_rules();
	}
#endif

	nvram_set_int("wlready", 1);

	file_unlock(lock);
}

/* for WPS Reset */
void restart_wireless_wps(void)
{
	int lock = file_lock("wireless");

	nvram_set_int("wlready", 0);

	stop_wps();
#ifdef CONFIG_BCMWL5
	stop_nas();
	stop_eapd();
#elif defined RTCONFIG_RALINK
	stop_8021x();
#endif
#ifdef RTCONFIG_BCMWL6
	stop_acsd();
#endif
	stop_lan_wl();
	wl_defaults_wps();
	sleep(2);	// delay to avoid start interface on stoping.
	start_lan_wl();

	reinit_hwnat(-1);

#ifdef CONFIG_BCMWL5
	start_eapd();
	start_nas();
#elif defined RTCONFIG_RALINK
	start_8021x();
#endif
	start_wps();
#ifdef RTCONFIG_BCMWL6
	start_acsd();
#endif
#ifdef RTCONFIG_WL_AUTO_CHANNEL
	if(nvram_match("AUTO_CHANNEL", "1")){
		nvram_set("wl_channel", "6");
		nvram_set("wl0_channel", "6");
	}
#endif
	restart_wl();
	lanaccess_wl();

#ifdef CONFIG_BCMWL5
	/* for MultiSSID */
	if(!strcmp(nvram_safe_get("qos_enable"), "1")){
		del_EbtablesRules(); // flush ebtables nat table
		add_EbtablesRules();
	}
#endif

	nvram_set_int("wlready", 1);

	file_unlock(lock);
}

//FIXME: add sysdep wrapper

void start_wan_port(void)
{
	wanport_ctrl(1);
}

void stop_wan_port(void)
{
	wanport_ctrl(0);
}

void restart_lan_port(int dt)
{
	stop_lan_port();
	start_lan_port(dt);
}

void start_lan_port(int dt)
{
	int now = 0;

	if (dt <= 0)
		now = 1;

	_dprintf("%s(%d) %d\n", __func__, dt, now);
	if (!s_last_lan_port_stopped_ts) {
		s_last_lan_port_stopped_ts = uptime();
		now = 1;
	}

	while (!now && (uptime() - s_last_lan_port_stopped_ts < dt)) {
		_dprintf("sleep 1\n");
		sleep(1);
	}

	lanport_ctrl(1);
}

void stop_lan_port(void)
{
	lanport_ctrl(0);
	s_last_lan_port_stopped_ts = uptime();
	_dprintf("%s() stop lan port. ts %ld\n", __func__, s_last_lan_port_stopped_ts);
}

void start_lan_wlport(void)
{
	char word[256], *next;
	int unit, subunit;

	unit=0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if(unit!=nvram_get_int("wlc_band")) set_radio(1, unit, 0);
		for (subunit = 1; subunit < 4; subunit++)
		{
			set_radio(1, unit, subunit);
		}
		unit++;
	}
}

void stop_lan_wlport(void)
{
	char word[256], *next;
	int unit, subunit;

	unit=0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if(unit!=nvram_get_int("wlc_band")) set_radio(0, unit, 0);
		for (subunit = 1; subunit < 4; subunit++)
		{
			set_radio(0, unit, subunit);
		}
		unit++;
	}
}
#if 0
static int
net_dev_exist(const char *ifname)
{
	DIR *dir_to_open = NULL;
	char tmpstr[128];

	if (ifname == NULL) return 0;

	sprintf(tmpstr, "/sys/class/net/%s", ifname);
	dir_to_open = opendir(tmpstr);
	if (dir_to_open)
	{
		closedir(dir_to_open);
		return 1;
	}
		return 0;
}

int
wl_dev_exist(void)
{
	char word[256], *next;
	int val = 1;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (!net_dev_exist(word))
		{
			val = 0;
			break;
		}
	}

	return val;
}
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
void start_lan_wlc(void)
{
	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	char *lan_ifname;

	lan_ifname = nvram_safe_get("lan_ifname");
	update_lan_state(LAN_STATE_INITIALIZING, 0);

	// bring up and configure LAN interface
	/*if(nvram_get_int("wlc_state") != WLC_STATE_CONNECTED)
		ifconfig(lan_ifname, IFUP, nvram_default_get("lan_ipaddr"), nvram_default_get("lan_netmask"));
	else
		ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));//*/
	if(nvram_match("lan_proto", "static"))
		ifconfig(lan_ifname, IFUP, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	else
		ifconfig(lan_ifname, IFUP, nvram_default_get("lan_ipaddr"), nvram_default_get("lan_netmask"));

	if(nvram_match("lan_proto", "dhcp"))
	{
		// only none routing mode need lan_proto=dhcp
		if (pids("udhcpc"))
		{
			killall("udhcpc", SIGUSR2);
			killall("udhcpc", SIGTERM);
			unlink("/tmp/udhcpc_lan");
		}

		char *dhcp_argv[] = { "udhcpc",
					"-i", "br0",
					"-p", "/var/run/udhcpc_lan.pid",
					"-s", "/tmp/udhcpc_lan",
					NULL };
		pid_t pid;

		symlink("/sbin/rc", "/tmp/udhcpc_lan");
		_eval(dhcp_argv, NULL, 0, &pid);

		update_lan_state(LAN_STATE_CONNECTING, 0);
	}
	else {
		lan_up(lan_ifname);
	}

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}

void stop_lan_wlc(void)
{
	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	char *lan_ifname;

	lan_ifname = nvram_safe_get("lan_ifname");

	ifconfig(lan_ifname, 0, NULL, NULL);

	update_lan_state(LAN_STATE_STOPPED, 0);

	killall("udhcpc", SIGUSR2);
	killall("udhcpc", SIGTERM);
	unlink("/tmp/udhcpc_lan");

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}
#endif //RTCONFIG_WIRELESSREPEATER


void set_device_hostname(void)
{
	FILE *fp;
	char hostname[32];

	strncpy(hostname, nvram_safe_get("computer_name"), 31);

	if (*hostname == 0) {
		if (get_productid()) {
			strncpy(hostname, get_productid(), 31);
		}
	}

	if ((fp=fopen("/proc/sys/kernel/hostname", "w+"))) {
		fputs(hostname, fp);
		fclose(fp);
	}

}


#ifdef RTCONFIG_QTN
int reset_qtn(vod)
{
	system("cp /rom/qtn/* /tmp/");
	eval("ifconfig", "br0:0", "1.1.1.1", "netmask", "255.255.255.0");
	eval("tftpd");
	led_control(BTN_QTN_RESET, LED_ON);
	led_control(BTN_QTN_RESET, LED_OFF);
}

int start_qtn(void)
{
	reset_qtn();
	sleep(8);
	// start_wireless_qtn();
	return 1;
}

#endif

