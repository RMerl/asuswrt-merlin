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

		if(nvram_match(strcat_r(prefix, "dnsenable_x", tmp), "0")) {
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

static int wlconf(char *ifname, int unit, int subunit)
{
	int r;
	char wl[24];
	int txpower;
	int model;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	if (unit < 0) return -1;

	if (subunit < 0)
	{
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
			model = get_model();

			switch (model) {
                                case MODEL_RTAC66U:
				case MODEL_RTN66U:
				case MODEL_RTN16:
					if ((unit == 0) &&
						nvram_match(strcat_r(prefix, "noisemitigation", tmp), "1"))
					{
						eval("wl", "-i", ifname, "interference_override", "4");
						eval("wl", "-i", ifname, "phyreg", "0x547", "0x4444");
						eval("wl", "-i", ifname, "phyreg", "0xc33", "0x280");
					}
				default:
					break;
			}

			txpower = nvram_get_int(wl_nvname("TxPower", unit, 0));

			dbG("unit: %d, txpower: %d\n", unit, txpower);

			switch (model) {
				case MODEL_RTN12HP:

					txpwr_rtn12hp(ifname, unit, subunit);

					break;

				default:

					eval("wl", "-i", ifname, "txpwr1", "-o", "-q", "-1");

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

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}
	strncpy(ifr->ifr_name, name, IFNAMSIZ);
	rv = ioctl(s, action, ifr);
	close(s);

	return rv;
}

/* Check NVRam to see if "name" is explicitly enabled */
static inline int
wl_vif_enabled(const char *name, char *tmp)
{
	return (atoi(nvram_safe_get(strcat_r(name, "_bss_enabled", tmp))));
}

/* Set the HW address for interface "name" if present in NVRam */
static void
wl_vif_hwaddr_set(const char *name)
{
	int rc;
	char *ea;
	char hwaddr[20];
	struct ifreq ifr;

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
void set_et_qos_mode(int sfd)
{
#ifdef CONFIG_BCMWL5
	int i, qos;
	caddr_t ifrdata;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	qos = (strcmp(nvram_safe_get("wl_wme"), "off") != 0);
	for (i = 1; i <= DEV_NUMIFS; i++) {
		ifr.ifr_ifindex = i;
		if (ioctl(sfd, SIOCGIFNAME, &ifr)) continue;
		if (ioctl(sfd, SIOCGIFHWADDR, &ifr)) continue;
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) continue;
		/* get flags */
		if (ioctl(sfd, SIOCGIFFLAGS, &ifr)) continue;
		/* if up (wan may not be up yet at this point) */
		if (ifr.ifr_flags & IFF_UP) {
			ifrdata = ifr.ifr_data;
			memset(&info, 0, sizeof(info));
			info.cmd = ETHTOOL_GDRVINFO;
			ifr.ifr_data = (caddr_t)&info;
			if (ioctl(sfd, SIOCETHTOOL, &ifr) >= 0) {
				/* Set QoS for et & bcm57xx devices */
				if (!strncmp(info.driver, "et", 2) ||
				    !strncmp(info.driver, "bcm57", 5)) {
					ifr.ifr_data = (caddr_t)&qos;
					ioctl(sfd, SIOCSETCQOS, &ifr);
				}
			}
			ifr.ifr_data = ifrdata;
		}
	}
#endif /* CONFIG_BCMWL5 */
}

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
#endif

#ifdef RTCONFIG_RALINK
char *wif_to_vif(char *wif)
{
	static char vif[32];
	int unit = 0, subunit = 0;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	vif[0] = '\0';

	for (unit = 0; unit < 2; unit++)
		for (subunit = 1; subunit < 4; subunit++)
		{
			snprintf(prefix, sizeof(prefix), "wl%d.%d", unit, subunit);

			if (nvram_match(strcat_r(prefix, "_ifname", tmp), wif))
			{
				sprintf(vif, "%s", prefix);
				goto RETURN_VIF;
			}
		}

RETURN_VIF:
	return vif;
}
#endif

void start_wl(void)
{
#ifdef CONFIG_BCMWL5
	char *lan_ifname, *lan_ifnames, *ifname, *p;
	int unit, subunit;
	int is_client = 0;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

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
#ifdef RTCONFIG_RALINK
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
#else
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", ifname);
#endif
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
					eval("wlconf", ifname, "down");
					eval("wl", "-i", ifname, "radio", "off");
				}
				else
#if 0
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
				if (!is_psta(1 - unit)) 
#endif
#endif
#endif
				eval("wlconf", ifname, "start"); /* start wl iface */
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
#ifdef RTAC66U
	if (nvram_match("wl1_radio", "1"))
	{
		nvram_set("led_5g", "1");
		led_control(LED_5G, LED_ON);
	}
	else
	{
		nvram_set("led_5g", "0");
		led_control(LED_5G, LED_OFF);
	}
#endif
#endif
#endif /* CONFIG_BCMWL5 */
}

void stop_wl(void)
{
}

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
			if (!strcmp(word, WIF_2G))
			{
				if (!strncmp(word, "rai", 3))	// iNIC
					gen_ralink_config(0, 1);
				else
					gen_ralink_config(0, 0);
			}
			else if (!strcmp(word, WIF_5G))
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

	if (!ctrl)
	{
		doSystem("iwpriv %s set RadioOn=0", wif);
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "radio", tmp), "1"))
			doSystem("iwpriv %s set RadioOn=1", wif);
	}
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
		int txpower = atoi(nvram_safe_get(strcat_r(prefix, "TxPower", tmp)));
		if ((txpower >= 0) && (txpower <= 100))
			doSystem("iwpriv %s set TxPower=%d",wif, txpower);
	}

	if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))
	{
		int channel = get_channel(band);

		if (channel)
			doSystem("iwpriv %s set HtBw=%d", wif, 1);
	}

	if (!nvram_get_int(strcat_r(prefix, "IgmpSnEnable", tmp)))
		doSystem("iwpriv %s set IgmpSnEnable=0", wif);
}

char *get_hwaddr(const char *ifname)
{
	int s;
	struct ifreq ifr;
	char eabuf[32];

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
}
#endif

#ifdef RTCONFIG_IPV6
void set_default_accept_ra(int flag)
{
	if (flag)
	{
		system("echo 2 > /proc/sys/net/ipv6/conf/all/accept_ra");
		system("echo 2 > /proc/sys/net/ipv6/conf/default/accept_ra");
	}
	else
	{
		system("echo 0 > /proc/sys/net/ipv6/conf/all/accept_ra");
		system("echo 0 > /proc/sys/net/ipv6/conf/default/accept_ra");
	}
}

void set_intf_ipv6_accept_ra(const char *ifname, int flag)
{
	char s[256];

	if (flag)
	{
		sprintf(s, "/proc/sys/net/ipv6/conf/%s/accept_ra", ifname);
		f_write_string(s, "2", 0, 0);

		sprintf(s, "/proc/sys/net/ipv6/conf/%s/forwarding", ifname);
#if 0
		if (!strncmp(ifname, "ppp", 3))
			f_write_string(s, "0", 0, 0);
		else
#endif
			f_write_string(s, "2", 0, 0);
	}
	else
	{
		sprintf(s, "/proc/sys/net/ipv6/conf/%s/accept_ra", ifname);
		f_write_string(s, "0", 0, 0);

		sprintf(s, "/proc/sys/net/ipv6/conf/%s/forwarding", ifname);
		f_write_string(s, "0", 0, 0);
	}
}

void set_intf_ipv6_dad(const char *ifname, int bridge, int flag)
{
	char s[256];

	if (flag)
	{
		sprintf(s, "/proc/sys/net/ipv6/conf/%s/accept_dad", ifname);
		f_write_string(s, "1", 0, 0);

		sprintf(s, "/proc/sys/net/ipv6/conf/%s/dad_transmits", ifname);
		if (bridge)
			f_write_string(s, "2", 0, 0);
		else
			f_write_string(s, "1", 0, 0);
	}
	else
	{
		sprintf(s, "/proc/sys/net/ipv6/conf/%s/accept_dad", ifname);
		f_write_string(s, "0", 0, 0);
	}
}

void enable_ipv6(int enable/*, int forceup*/)
{
	DIR *dir;
	struct dirent *dirent;
	char s[256];
	int service;
#if 0
	struct ifreq ifr;
	int sfd;

	if ((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) return;
#endif
	if ((dir = opendir("/proc/sys/net/ipv6/conf")) != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
#if 0
			strcpy(ifr.ifr_name, dirent->d_name);
			if (!ioctl(sfd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP))
				ifconfig(dirent->d_name, 0, NULL, NULL);
#endif
			sprintf(s, "/proc/sys/net/ipv6/conf/%s/disable_ipv6", dirent->d_name);
			f_write_string(s, enable ? "0" : "1", 0, 0);
#if 0
#ifdef CONFIG_BCMWL5
			if ((forceup || !strncmp(dirent->d_name, "eth", 3)) && !ioctl(sfd, SIOCGIFFLAGS, &ifr) && !(ifr.ifr_flags & IFF_UP))
#else
			if (forceup && !ioctl(sfd, SIOCGIFFLAGS, &ifr) && !(ifr.ifr_flags & IFF_UP))
#endif
				ifconfig(dirent->d_name, IFUP, NULL, NULL);
#endif
		}
		closedir(dir);
	}
#if 0
	close(sfd);
#endif
	service = get_ipv6_service();

	if (is_routing_enabled())
	{
		switch (service) {
		case IPV6_NATIVE:
		case IPV6_NATIVE_DHCP:
			if ((nvram_get_int("ipv6_accept_ra") & 1) != 0)
			{
				set_default_accept_ra(1);
				break;
			}
#if 0
		case IPV6_6IN4:
		case IPV6_6TO4:
		case IPV6_6RD:
		case IPV6_MANUAL:
		default:
			set_default_accept_ra(0);
			break;
#endif
		}
	}
	else set_default_accept_ra(0);
}
#endif

void start_lan(void)
{
	_dprintf("%s %d\n", __FUNCTION__, __LINE__);

	char *lan_ifname;
	struct ifreq ifr;
	char *lan_ifnames, *ifname, *p;
	int sfd;
	uint32 ip;
	int unit, subunit, sta;
	int hwaddrset;
	char eabuf[32];
	char word[256], *next;
	int match;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	int i;

	update_lan_state(LAN_STATE_INITIALIZING, 0);

	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER){
		nvram_set("wlc_mode", "0");
		nvram_set("btn_ez_radiotoggle", "0"); // reset to default
	}

	convert_routes();

#ifdef CONFIG_BCMWL5
#ifndef ACS_ONCE
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U))
	modprobe("wl");
#endif
#endif

#ifdef RTCONFIG_RALINK
	init_wl();
#endif

#ifdef CONFIG_BCMWL5
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN12HP) ||
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
		set_intf_ipv6_accept_ra(nvram_safe_get("lan_ifname"), 0);
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
#ifdef RTCONFIG_IPV6
				match = 0;
				foreach (word, nvram_safe_get("wl_ifnames"), next) {
					if (!strcmp(ifname, word))
					{
						match = 1;
						break;
					}
				}

				if (!match && !next)
				{
					set_intf_ipv6_accept_ra(ifname, 0);
					set_intf_ipv6_dad(ifname, 0, 1);
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
#ifdef RTCONFIG_RALINK
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
#else
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", ifname);
#endif
#ifdef RTCONFIG_RALINK
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire", wif_to_vif(ifname));
#else
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire", ifname);
#endif
					if (nvram_get_int(nv2))
					{
						nvram_set(nv, "0");
#ifdef RTCONFIG_RALINK
						snprintf(nv3, sizeof(nv3) - 1, "%s_lanaccess", wif_to_vif(ifname));
#else
						snprintf(nv3, sizeof(nv3) - 1, "%s_lanaccess", ifname);
#endif
						nvram_set(nv3, "off");
					}
#ifdef RTCONFIG_RALINK
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire", wif_to_vif(ifname));
#else
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire", ifname);
#endif
					nvram_set(nv2, "0");
#ifdef RTCONFIG_RALINK
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire_tmp", wif_to_vif(ifname));
#else
					snprintf(nv2, sizeof(nv2) - 1, "%s_expire_tmp", ifname);
#endif
					nvram_set(nv2, "0");

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
				// bring up interface
				if (ifconfig(ifname, IFUP, NULL, NULL) != 0) continue;

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

	// Set initial QoS mode for LAN ports
	set_et_qos_mode(sfd);

	close(sfd);

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
		if(is_routing_enabled())
		{
			update_lan_state(LAN_STATE_CONNECTED, 0);
			add_lan_routes(lan_ifname);
			start_default_filter(0);
		}
		else lan_up(lan_ifname);
	}

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

	ifconfig(lan_ifname, 0, NULL, NULL);

#ifdef RTCONFIG_IPV6
	stop_ipv6();
#endif

	eval("ebtables", "-F");
	eval("ebtables", "-t", "broute", "-F");

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
				eval("wlconf", ifname, "down");
				eval("wl", "-i", ifname, "radio", "off");
#elif defined RTCONFIG_RALINK
				if (!strncmp(ifname, "ra", 2))
					stop_wds_ra(lan_ifname, ifname);
#endif
				eval("brctl", "delif", lan_ifname, ifname);
				ifconfig(ifname, 0, NULL, NULL);
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
#ifdef RTAC66U
	nvram_set("led_5g", "0");
	led_control(LED_5G, LED_OFF);
#endif
#endif

	// inform watchdog to stop WPS LED
	kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);

#ifdef CONFIG_BCMWL5
#ifndef ACS_ONCE
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U))
	modprobe_r("wl");
#endif
#endif

#ifdef RTCONFIG_RALINK
	fini_wl();
#endif
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
	char device_path[128], usb_path[PATH_MAX], usb_port[8];
	int port_num;
	char nvram_name[32];
	char word[PATH_MAX], *next;
	int got_modem;
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

		if (strncmp(interface, "wdsi", 4))
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
	if (!strncmp(interface, "ppp", 3)){
		char pid_file[256], *value;
		int ppp_pid, unit_pid, retry;

		if(!strcmp(action, "add")){
			memset(pid_file, 0, 256);
			snprintf(pid_file, 256, "/var/run/%s.pid", interface);

			retry = 5;
			while((value = file2str(pid_file)) == NULL && retry-- > 0){
				_dprintf("%s %s: wait the pid file of %s at %d seconds...\n", __FUNCTION__, action, interface, retry);
				sleep(1);
			}

			if(value == NULL){
				_dprintf("%s %s: Could not get the pid file of %s!\n", __FUNCTION__, action, interface);
				return;
			}

			ppp_pid = atoi(value);
			free(value);
			if(ppp_pid <= 1){
				_dprintf("%s %s: Could not get the pid of %s!\n", __FUNCTION__, action, interface);
				return;
			}

			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				memset(pid_file, 0, 256);
				snprintf(pid_file, 256, "/var/run/ppp-wan%d.pid", unit);

				if((value = file2str(pid_file)) == NULL)
					continue;

				unit_pid = atoi(value);
				free(value);
				if(unit_pid <= 1)
					continue;

				if(unit_pid != ppp_pid)
					continue;

				snprintf(prefix, sizeof(prefix), "wan%d_", unit);

				// fix the wrong filter table which would let the dialing on demand be failed.
				if(nvram_match(strcat_r(prefix, "pppoe_demand", tmp), "1"))
					nvram_set(strcat_r(prefix, "pppoe_ifname", tmp), interface);

				break;
			}
		}
		else{
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				if(nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), interface)){
					nvram_set(tmp, "");
				}
			}
		}
	}
#ifdef RTCONFIG_USB_MODEM
	// RNDIS interface.
	else if(!strncmp(interface, "usb", 3)){
		if(nvram_get_int("sw_mode") != SW_MODE_ROUTER)
			return;

#ifdef RTCONFIG_DUALWAN
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
				break;
		if(unit == WAN_UNIT_MAX)
			return;
#else
		unit = WAN_UNIT_SECOND;
#endif

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			memset(device_path, 0, 128);
			sprintf(device_path, "%s/%s/device", SYS_NET, interface);

			memset(usb_path, 0, PATH_MAX);
			if(realpath(device_path, usb_path) == NULL)
				return;

			if(get_usb_port_by_string(usb_path, usb_port, sizeof(usb_port)) == NULL)
				return;

			port_num = get_usb_port_number(usb_port);
			if(!port_num)
				return;

			memset(nvram_name, 0, 32);
			sprintf(nvram_name, "usb_path%d_act", port_num);

			if(strcmp(nvram_safe_get(nvram_name), "") != 0)
				return;

			nvram_set(nvram_name, interface);

			nvram_set(strcat_r(prefix, "ifname", tmp), interface);

			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

#ifdef RTCONFIG_DUALWAN
			if(!strcmp(nvram_safe_get("success_start_service"), "1"))
#endif
				start_wan_if(unit);
		}
		else{
			if(!strcmp(interface, nvram_safe_get("usb_path1_act")))
				nvram_set("usb_path1_act", "");
			else if(!strcmp(interface, nvram_safe_get("usb_path2_act")))
				nvram_set("usb_path2_act", "");
			else if(!strcmp(interface, nvram_safe_get("usb_path3_act")))
				nvram_set("usb_path3_act", "");
			else
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

#ifdef RTCONFIG_DUALWAN
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
				break;
		if(unit == WAN_UNIT_MAX)
			return;
#else
		unit = WAN_UNIT_SECOND;
#endif

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			nvram_set(strcat_r(prefix, "ifname", tmp), interface);

			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

			if(!strcmp(nvram_safe_get("success_start_service"), "1")){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
		}
		else{
			nvram_set(strcat_r(prefix, "ifname", tmp), "");

			stop_wan_if(unit);

			system("asus_usbbcm usbbcm remove");
		}

		// Notify wanduck to switch the wan line to WAN port.
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
#ifdef RTCONFIG_USB_BECEEM
	else if(!strncmp(interface, "wimax", 5)){
		if(nvram_get_int("sw_mode") != SW_MODE_ROUTER)
			return;

#ifdef RTCONFIG_DUALWAN
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
				break;
		if(unit == WAN_UNIT_MAX)
			return;
#else
		unit = WAN_UNIT_SECOND;
#endif

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strcmp(action, "add")){
			nvram_set(strcat_r(prefix, "ifname", tmp), interface);

			_dprintf("hotplug net INTERFACE=%s ACTION=%s: wait 2 seconds...\n", interface, action);
			sleep(2);

			got_modem = 0;
			port_num = 1;
			foreach(word, nvram_safe_get("ehci_ports"), next){
				memset(nvram_name, 0, 32);
				sprintf(nvram_name, "usb_path%d", port_num);

				if(!strcmp(nvram_safe_get(nvram_name), "modem")){
					got_modem = 1;
					memset(nvram_name, 0, 32);
					sprintf(nvram_name, "usb_path%d_act", port_num);
					nvram_set(nvram_name, interface);
					break;
				}

				++port_num;
			}

			if(!strcmp(nvram_safe_get("success_start_service"), "1")){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
		}
		else{
			nvram_set(strcat_r(prefix, "ifname", tmp), "");

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

static int radio_switch(int subunit)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	char *p;
	int i;		// unit
	int sw = 0;	// record get_radio status
	int MAX = 0;	// if MAX = 1: single band,  2: dual band

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
		sw |= get_radio(i, subunit);
	}

	sw = !sw;

	for(i = 0; i < MAX; i++){
		// set wlx_radio
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		nvram_set_int(strcat_r(prefix, "radio", tmp), sw);
		//dbG("[radio switch] %s=%d, MAX=%d\n", tmp, sw, MAX); // radio switch
		set_radio(sw, i, subunit);
	}

	// commit to flash once */
	nvram_commit();

	// make sure all interfaces work well
	notify_rc("restart_wireless");

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
	int lock;

	lock = file_lock("resolv");

	if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
		file_unlock(lock);
		perror("/tmp/resolv.conf");
		return errno;
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") != WLC_STATE_CONNECTED)
		fprintf(fp, "nameserver %s\n", nvram_default_get("lan_ipaddr"));
	else
#endif
	{
		if(!nvram_match("lan_gateway", ""))
			fprintf(fp, "nameserver %s\n", nvram_safe_get("lan_gateway"));

		foreach(word, nvram_safe_get("lan_dns"), next){
			fprintf(fp, "nameserver %s\n", word);
		}
	}

	fclose(fp);

	unlink("/etc/resolv.conf");
	symlink("/tmp/resolv.conf", "/etc/resolv.conf");
	file_unlock(lock);
	return 0;
}


void
lan_up(char *lan_ifname)
{
	_dprintf("%s(%s)\n", __FUNCTION__, lan_ifname);

	restart_dnsmasq();

	update_lan_resolvconf();

	/* Set default route to gateway if specified */
	if(nvram_get_int("sw_mode") != SW_MODE_REPEATER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| (nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_state") == WLC_STATE_CONNECTED)
#endif
			){
		route_add(lan_ifname, 0, "0.0.0.0", nvram_safe_get("lan_gateway"), "0.0.0.0");

		/* Sync time */
		stop_ntpc();
		start_ntpc();
	}

	/* Scan new subnetwork */
	stop_networkmap();
	start_networkmap(); 
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

	/* remove resolv.conf */
	unlink("/tmp/resolv.conf");
	unlink("/etc/resolv.conf");

	update_lan_state(LAN_STATE_STOPPED, 0);
}

void stop_lan_wl(void)
{
	char *p, *ifname;
	char *wl_ifnames;
	char *lan_ifname;
	int unit, subunit;

	eval("ebtables", "-F");
	eval("ebtables", "-t", "broute", "-F");

	lan_ifname = nvram_safe_get("lan_ifname");
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
			else if (wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				continue;

			eval("wlconf", ifname, "down");
			eval("wl", "-i", ifname, "radio", "off");
#elif defined RTCONFIG_RALINK
			if (!strncmp(ifname, "ra", 2))
				stop_wds_ra(lan_ifname, ifname);
#endif
			eval("brctl", "delif", lan_ifname, ifname);
			ifconfig(ifname, 0, NULL, NULL);
		}

		free(wl_ifnames);
	}

#ifdef RTCONFIG_BCMWL6
#ifdef RTAC66U
	nvram_set("led_5g", "0");
	led_control(LED_5G, LED_OFF);
#endif
#endif

	// inform watchdog to stop WPS LED
        kill_pidfile_s("/var/run/watchdog.pid", SIGUSR2);

#ifdef CONFIG_BCMWL5
#ifndef ACS_ONCE
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U))
	modprobe_r("wl");
#endif
#endif

#ifdef RTCONFIG_RALINK
	fini_wl();
#endif
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
	struct ifreq ifr;
	char *wl_ifnames, *ifname, *p;
	uint32 ip;
	int unit, subunit, sta;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";
	char word[256], *next;
	int match;
	int i;

#ifdef CONFIG_BCMWL5
#ifndef ACS_ONCE
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U))
	modprobe("wl");
#endif
#endif

#ifdef RTCONFIG_RALINK
	init_wl();
#endif

#ifdef CONFIG_BCMWL5
	if ((get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN12HP) ||
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
				if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
#elif defined RTCONFIG_RALINK
				if (strncmp(ifname, "ra", 2) == 0 && !strchr(ifname, '0')) {
#endif
					char nv[40];
					char nv2[40];
					char nv3[40];
#ifdef RTCONFIG_RALINK
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
#else
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", ifname);
#endif
					if (!nvram_get_int(nv))
					{
#ifdef RTCONFIG_RALINK
						snprintf(nv2, sizeof(nv2) - 1, "%s_lanaccess", wif_to_vif(ifname));
#else
						snprintf(nv2, sizeof(nv2) - 1, "%s_lanaccess", ifname);
#endif
						nvram_set(nv2, "off");
#ifdef RTCONFIG_RALINK
						snprintf(nv2, sizeof(nv2) - 1, "%s_expire", wif_to_vif(ifname));
#else
						snprintf(nv2, sizeof(nv2) - 1, "%s_expire", ifname);
#endif
						nvram_set(nv2, "0");
						continue;
					}
					else
					{
						if (!nvram_get(strcat_r(prefix, "lanaccess", tmp)))
							nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");

						if ((nvram_get_int("wl_unit") >= 0) && (nvram_get_int("wl_subunit") > 0))
						{
#ifdef RTCONFIG_RALINK
							snprintf(nv, sizeof(nv) - 1, "%s_expire", wif_to_vif(ifname));
							snprintf(nv2, sizeof(nv2) - 1, "%s_expire_tmp", wif_to_vif(ifname));
#else
							snprintf(nv, sizeof(nv) - 1, "%s_expire", ifname);
							snprintf(nv2, sizeof(nv2) - 1, "%s_expire_tmp", ifname);
#endif
							snprintf(nv3, sizeof(nv3) - 1, "wl%d.%d", nvram_get_int("wl_unit"), nvram_get_int("wl_subunit"));
#ifdef RTCONFIG_RALINK
							if (!strcmp(nv3, wif_to_vif(ifname)))
#else
							if (!strcmp(nv3, ifname))
#endif
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

				// bring up interface
				if (ifconfig(ifname, IFUP, NULL, NULL) != 0) continue;

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
				eval("brctl", "addif", lan_ifname, ifname);

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
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;

			unit = -1; subunit = -1;

			// ignore disabled wl vifs
			if (strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.')) {
				char nv[40];
#ifdef RTCONFIG_RALINK
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(ifname));
#else
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", ifname);
#endif
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
				eval("wlconf", ifname, "down");
				eval("wl", "-i", ifname, "radio", "off");
			}
			else
#if 0
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
			if (!is_psta(1 - unit))
#endif
#endif
#endif
			eval("wlconf", ifname, "start"); /* start wl iface */
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
#ifdef RTAC66U
	if (nvram_match("wl1_radio", "1"))
	{
		nvram_set("led_5g", "1");
		led_control(LED_5G, LED_ON);
	}
	else
	{
		nvram_set("led_5g", "0");
		led_control(LED_5G, LED_OFF);
	}
#endif
#endif
#endif /* CONFIG_BCMWL5 */
}

void lanaccess_mssid_ban(const char *ifname_in)
{
	char lan_subnet[32];

	if (nvram_get_int("sw_mode") != SW_MODE_ROUTER) return;

	eval("ebtables", "-A", "FORWARD", "-i", ifname_in, "-o", "!", get_wan_ifname(0), "-j", "DROP");
	eval("ebtables", "-A", "FORWARD", "-i", "!", get_wan_ifname(0), "-o", ifname_in, "-j", "DROP");

	snprintf(lan_subnet, sizeof(lan_subnet), "%s/%s", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", ifname_in, "-p", "ipv4", "--ip-dst", lan_subnet, "--ip-proto", "tcp", "-j", "DROP");
}

void lanaccess_wl()
{
	char *p, *ifname;
	char *wl_ifnames;
	int unit, subunit;

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

			char nv[40];
#ifdef RTCONFIG_RALINK
			snprintf(nv, sizeof(nv) - 1, "%s_lanaccess", wif_to_vif(ifname));
#else
			snprintf(nv, sizeof(nv) - 1, "%s_lanaccess", ifname);
#endif
			if (!strcmp(nvram_safe_get(nv), "off"))
				lanaccess_mssid_ban(ifname);
		}
		free(wl_ifnames);
	}
}

void restart_wireless()
{
	nvram_set("wlready", "0");

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
	start_lan_wl();

#ifdef RTCONFIG_RALINK
	reinit_hwnat();
#endif

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

	restart_wl();
	lanaccess_wl();

#ifdef CONFIG_BCMWL5
	/* for MultiSSID */
	if(!strcmp(nvram_safe_get("qos_enable"), "1")){
		del_EbtablesRules(); // flush ebtables nat table
		add_EbtablesRules();
	}
#endif

	nvram_set("wlready", "1");
}
#ifdef ACS_ONCE
void restart_wireless_acsd()
{
	nvram_set("wlready", "0");

	stop_wps();
#ifdef CONFIG_BCMWL5
	stop_nas();
	stop_eapd();
#elif defined RTCONFIG_RALINK
	stop_8021x();
#endif
	stop_lan_wl();
	init_nvram();	// init nvram lan_ifnames
	wl_defaults();	// init nvram wlx_ifnames & lan_ifnames
	start_lan_wl();

#ifdef RTCONFIG_RALINK
	reinit_hwnat();
#endif

#ifdef CONFIG_BCMWL5
	start_eapd();
	start_nas();
#elif defined RTCONFIG_RALINK
	start_8021x();
#endif
	start_wps();

	restart_wl();
	lanaccess_wl();

	nvram_set("wlready", "1");
}
#endif
/* for WPS Reset */
void restart_wireless_wps()
{
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
	start_lan_wl();

#ifdef RTCONFIG_RALINK
	reinit_hwnat();
#endif

#ifdef CONFIG_BCMWL5
	start_eapd();
	start_nas();
#elif defined RTCONFIG_RALINK
	start_8021x();
#endif
	start_wps();

	restart_wl();
	lanaccess_wl();
}

//FIXME: add sysdep wrapper

void start_lan_port() 
{
	lanport_ctrl(1);
}

void stop_lan_port() {
	lanport_ctrl(0);
}

void start_lan_wlport()
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

void stop_lan_wlport()
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

static int
net_dev_exist(const char *ifname)
{
	DIR *dir_to_open = NULL;
	char tmpstr[128];

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
	char *lan_ifnames, *p, *ifname;

	lan_ifname = nvram_safe_get("lan_ifname");

	ifconfig(lan_ifname, 0, NULL, NULL);

	update_lan_state(LAN_STATE_STOPPED, 0);

	killall("udhcpc", SIGUSR2);
	killall("udhcpc", SIGTERM);
	unlink("/tmp/udhcpc_lan");

	_dprintf("%s %d\n", __FUNCTION__, __LINE__);
}
#endif //RTCONFIG_WIRELESSREPEATER
