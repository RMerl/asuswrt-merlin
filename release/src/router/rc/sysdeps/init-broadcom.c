/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"
#include "shared.h"
#include "interface.h"

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

struct txpower_s {
	uint16 min;
	uint16 max;
	uint8 maxp2ga0;
	uint8 maxp2ga1;
	uint8 cck2gpo;
	uint16 ofdm2gpo0;
	uint16 ofdm2gpo1;
	uint16 mcs2gpo0;
	uint16 mcs2gpo1;
	uint16 mcs2gpo2;
	uint16 mcs2gpo3;
	uint16 mcs2gpo4;
	uint16 mcs2gpo5;
	uint16 mcs2gpo6;
	uint16 mcs2gpo7;
	uint8 cdd2gpo;
	uint8 stbc2gpo;
	uint8 bw402gpo;
	uint8 bwdup2gpo;
};

static const struct txpower_s txpower_list_rtn12hp[] = {
#if !defined(RTCONFIG_RALINK)
	/* 1-20mW */
	{ 1, 20, 0x42, 0x42, 0x0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0, 0x0, 0x0, 0x0},
	/* 20-40mW */
	{ 21, 40, 0x4A, 0x4A, 0x0, 0x2000, 0x4422, 0x2200, 0x4444, 0x2200, 0x4444, 0x4422, 0x4444, 0x4422, 0x4444, 0x0, 0x0, 0x0, 0x0},
	/* 40-60mW */
	{ 41, 60, 0x52, 0x52, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 60-80mW */
	{ 61, 79, 0x5A, 0x5A, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 80mW */
	{ 80, 80, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 80-200mW */
	{ 81, 200, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 200-300mW */
	{ 201, 300, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 300-400mW */
	{ 301, 400, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 400-999mW */
	{ 401, 999, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
#endif	/* !RTCONFIG_RALINK */
	{ 0, 0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

int setpoweroffset_rtn12hp(uint8 level, char *prefix2)
{
	char tmp[100], tmp2[100];
	dbG("[rc] setpoweroffset_rtn12hp, level[%d]\n", level);

	sprintf(tmp2, "0x%02X", txpower_list_rtn12hp[level].maxp2ga0);
	nvram_set(strcat_r(prefix2, "maxp2ga0", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%02X", txpower_list_rtn12hp[level].maxp2ga1);
	nvram_set(strcat_r(prefix2, "maxp2ga1", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", txpower_list_rtn12hp[level].cck2gpo);
	nvram_set(strcat_r(prefix2, "cck2gpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X%04X", txpower_list_rtn12hp[level].ofdm2gpo1,
		txpower_list_rtn12hp[level].ofdm2gpo0);
	nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo0);
	nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo1);
	nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo2);
	nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo3);
	nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo4);
	nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo5);
	nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo6);
	nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", txpower_list_rtn12hp[level].mcs2gpo7);
	nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", txpower_list_rtn12hp[level].cdd2gpo);
	nvram_set(strcat_r(prefix2, "cddpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", txpower_list_rtn12hp[level].stbc2gpo);
	nvram_set(strcat_r(prefix2, "stbcpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", txpower_list_rtn12hp[level].bw402gpo);
	nvram_set(strcat_r(prefix2, "bw40po", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", txpower_list_rtn12hp[level].bwdup2gpo);
	nvram_set(strcat_r(prefix2, "bwduppo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	return 1;
}

void init_devs(void)
{
#if 0
#ifdef CONFIG_BCMWL5
	// ctf must be loaded prior to any other modules
	if (nvram_get_int("ctf_disable") == 0)
		modprobe("ctf");
#endif
#endif
}


void generate_switch_para(void)
{
	int model, cfg;
	char lan[SWCFG_BUFSIZE], wan[SWCFG_BUFSIZE];

	// generate nvram nvram according to system setting
	model = get_model();

	if (is_routing_enabled()) {
		cfg = nvram_get_int("switch_stb_x");
		if (cfg < SWCFG_DEFAULT || cfg > SWCFG_STB34)
			cfg = SWCFG_DEFAULT;
	} 
	else if (nvram_get_int("sw_mode") == SW_MODE_AP)
		cfg = SWCFG_BRIDGE;
	else	cfg = SWCFG_DEFAULT; // keep wan port, but get ip from bridge 

	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
		case MODEL_RTN53:
		case MODEL_RTN10D1:
		{				      /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			/* TODO: switch_wantag? */

			//if (!is_routing_enabled())
			//	nvram_set("lan_ifnames", "eth0 eth1"); //override
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			cfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN15U:
		{				      /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 8 };
			/* TODO: switch_wantag? */

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		case MODEL_RTN16:
		{				      /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");
			nvram_unset("vlan3ports");
			nvram_unset("vlan3hwname");

			// The first WAN port.
			if(get_wans_dualwan()&WANSCAP_WAN){
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan2ports", wan);
				if(get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)
					nvram_set("vlan2hwname", "et0");
			}

			// The second WAN port.
			if(get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4){
				wan1cfg += WAN1PORT1-1;
				if(wancfg != SWCFG_DEFAULT){
					gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
					nvram_set("vlan1ports", lan);
					gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
					nvram_set("lanports", lan);
				}
				else{
					switch_gen_config(lan, ports, wan1cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, wan1cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
				nvram_set("vlan3ports", wan);
				if(get_wans_dualwan()&WANSCAP_WAN)
					nvram_set("vlan3hwname", "et0");
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan1ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					switch_gen_config(wan, ports, wan1cfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else
					nvram_unset(nvram_ports);
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		{				      /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");
			nvram_unset("vlan3ports");
			nvram_unset("vlan3hwname");

			// The first WAN port.
			if(get_wans_dualwan()&WANSCAP_WAN){
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan2ports", wan);
				if(get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)
					nvram_set("vlan2hwname", "et0");
			}

			// The second WAN port.
			if(get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4){
				wan1cfg += WAN1PORT1-1;
				if(wancfg != SWCFG_DEFAULT){
					gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
					nvram_set("vlan1ports", lan);
					gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
					nvram_set("lanports", lan);
				}
				else{
					switch_gen_config(lan, ports, wan1cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, wan1cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
				nvram_set("vlan3ports", wan);
				if(get_wans_dualwan()&WANSCAP_WAN)
					nvram_set("vlan3hwname", "et0");
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan1ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
					switch_gen_config(wan, ports, wan1cfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else
					nvram_unset(nvram_ports);
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTN10U:
		{				      /* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 5 };
			/* TODO: switch_wantag? */

			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, cfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}
	}
}

void enable_jumbo_frame()
{
	int model;

	model = get_model();

	if(model!=MODEL_RTN66U && model!=MODEL_RTAC66U && model!=MODEL_RTN16 && model!=MODEL_RTN15U) return ;
	if(nvram_get_int("jumbo_frame_enable"))
		eval("et", "robowr", "0x40", "0x01", "0x1f");
	else eval("et", "robowr", "0x40", "0x01", "0x00");
}

void init_switch()
{
	generate_switch_para();

#ifdef CONFIG_BCMWL5
	// ctf should be disabled when some functions are enabled
	if(nvram_get_int("cstats_enable") || nvram_get_int("qos_enable") || nvram_get_int("url_enable_x") || nvram_get_int("keyword_enable_x") || nvram_get_int("ctf_disable_force")
#ifdef RTCONFIG_WIRELESSREPEATER
#ifndef RTCONFIG_PROXYSTA
	|| nvram_get_int("sw_mode") == SW_MODE_REPEATER
#endif
#endif
	) {
		nvram_set("ctf_disable", "1");
//		nvram_set("pktc_disable", "1");
	}
	else {
		nvram_set("ctf_disable", "0");
//		nvram_set("pktc_disable", "0");
	}
	// ctf must be loaded prior to any other modules 
	if (nvram_get_int("ctf_disable") == 0)
		modprobe("ctf");

/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
	if(nvram_get_int("sw_mode") == SW_MODE_ROUTER && nvram_get_int("qos_enable")) {
		// enable netfilter bridge only when phydev is used
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-filter-vlan-tagged", "1", 0, 0);
	}
*/
#endif

#ifdef RTCONFIG_SHP
	if(nvram_get_int("qos_enable") || nvram_get_int("macfilter_enable_x") || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_disable", "1");
	}
	else {
		nvram_set("lfp_disable", "0");
	}

	if(nvram_get_int("lfp_disable")==0) {
		restart_lfp();
	}
#endif
	modprobe("et");
	modprobe("bcm57xx");
	enable_jumbo_frame();
}

int
switch_exist(void)
{
	FILE *fp;
	char buf[128], *line;
	int ret = 1;

	fp = popen("et robord 0 0", "r");
	if (fp == NULL) {
		perror("popen");
		return 0;
	}

	line = fgets(buf, sizeof(buf), fp);
	if ((line == NULL) ||
	    (strstr(line, "not found") != NULL)) {
		_dprintf("No switch interface!!!: %s\n", line ? : "");
		ret = 0;
	}
	pclose(fp);

	return ret;
}

void config_switch(void)
{
	generate_switch_para();

	// setup switch
	// implement config switch as vlan here
	if(nvram_get_int("jumbo_frame_enable"))
		eval("et", "robowr", "0x40", "0x01", "0x1f");
	else eval("et", "robowr", "0x40", "0x01", "0x00");
}

extern struct nvram_tuple bcm4360ac_defaults[];

static void
set_bcm4360ac_vars(void)
{
	struct nvram_tuple *t;

	/* Restore defaults */
	for (t = bcm4360ac_defaults; t->name; t++) {
		if (!nvram_get(t->name))
			nvram_set(t->name, t->value);
	}
}

void init_wl(void)
{
#ifdef RTCONFIG_EMF
	modprobe("emf");
	modprobe("igs");
#endif
	set_bcm4360ac_vars();
	modprobe("wl");

#ifdef RTCONFIG_BRCM_USBAP
	/* We have to load USB modules after loading PCI wl driver so
	 * USB driver can decide its instance number based on PCI wl
	 * instance numbers (in hotplug_usb())
	 */
	modprobe("usbcore");

#ifdef LINUX26
	mount("usbfs", "/proc/bus/usb", "usbfs", MS_MGC_VAL, NULL);
#else
	mount("usbdevfs", "/proc/bus/usb", "usbdevfs", MS_MGC_VAL, NULL);
#endif /* LINUX26 */

	{
		char	insmod_arg[128];
		int	i = 0, maxwl_eth = 0, maxunit = -1;
		char	ifname[16] = {0};
		int	unit = -1;
		char arg1[20] = {0};
		char arg2[20] = {0};
		char arg3[20] = {0};
		char arg4[20] = {0};
		char arg5[20] = {0};
		char arg6[20] = {0};
		char arg7[20] = {0};
		const int wl_wait = 3;	/* max wait time for wl_high to up */

		/* Save QTD cache params in nvram */
		sprintf(arg1, "log2_irq_thresh=%d", nvram_get_int("ehciirqt"));
		sprintf(arg2, "qtdc_pid=%d", nvram_get_int("qtdc_pid"));
		sprintf(arg3, "qtdc_vid=%d", nvram_get_int("qtdc_vid"));
		sprintf(arg4, "qtdc0_ep=%d", nvram_get_int("qtdc0_ep"));
		sprintf(arg5, "qtdc0_sz=%d", nvram_get_int("qtdc0_sz"));
		sprintf(arg6, "qtdc1_ep=%d", nvram_get_int("qtdc1_ep"));
		sprintf(arg7, "qtdc1_sz=%d", nvram_get_int("qtdc1_sz"));

		modprobe("ehci-hcd", arg1, arg2, arg3, arg4, arg5, arg6, arg7);

		/* Search for existing PCI wl devices and the max unit number used.
		 * Note that PCI driver has to be loaded before USB hotplug event.
		 * This is enforced in rc.c
		 */
		#define DEV_NUMIFS 8
		for (i = 1; i <= DEV_NUMIFS; i++) {
			sprintf(ifname, "eth%d", i);
			if (!wl_probe(ifname)) {
				if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit,
					sizeof(unit))) {
					maxwl_eth = i;
					maxunit = (unit > maxunit) ? unit : maxunit;
				}
			}
		}

		/* Set instance base (starting unit number) for USB device */
		sprintf(insmod_arg, "instance_base=%d", maxunit + 1);
		modprobe("wl_high", insmod_arg);

		/* Hold until the USB/HSIC interface is up (up to wl_wait sec) */
		sprintf(ifname, "eth%d", maxwl_eth + 1);
		i = wl_wait;
		while (wl_probe(ifname) && i--) {
			sleep(1);
		}
		if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			cprintf("wl%d is up in %d sec\n", unit, wl_wait - i);
		else
			cprintf("wl%d not up in %d sec\n", unit, wl_wait);
	}
#endif /* __CONFIG_USBAP__ */
}

void init_syspara(void)
{
	char *ptr;
	int model;

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);
	nvram_set("buildno", rt_serialno);
	nvram_set("extendno", rt_extendno);
	nvram_set("buildinfo", rt_buildinfo);
	ptr = nvram_get("regulation_domain");

	model = get_model();
	switch(model) {
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12HP:
		case MODEL_RTN53:
		case MODEL_RTN15U:
		case MODEL_RTN10U:
		case MODEL_RTN10D1:
		case MODEL_RTN16:
			if (!nvram_get("et0macaddr")) //eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("0:macaddr")) //eth2(5G)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (nvram_get("regulation_domain_5G")) { // by ate command from asuswrt, prior than ui 2.0
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("0:ccode", nvram_get("regulation_domain_5G"));
			} else if (nvram_get("regulation_domain_5g")) { // by ate command from ui 2.0
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5g"));
				nvram_set("0:ccode", nvram_get("regulation_domain_5g"));
			}
			else {
				nvram_set("wl1_country_code", "US");
				nvram_set("0:ccode", "US");
			}
			nvram_set("sb/1/macaddr", nvram_safe_get("et0macaddr"));
			if (ptr && *ptr) {
				if ((strlen(ptr) == 6) && /* legacy format */
					!strncasecmp(ptr, "0x", 2))
				{
					nvram_set("wl0_country_code", ptr+4);
					nvram_set("sb/1/ccode", ptr+4);
				}
				else
				{
					nvram_set("wl0_country_code", ptr);
					nvram_set("sb/1/ccode", ptr);
				}
			}else {
				nvram_set("wl0_country_code", "US");
				nvram_set("sb/1/ccode", "US");
			}
			break;
		case MODEL_RTN66U:
		case MODEL_RTAC66U:
			if (!nvram_get("et0macaddr")) //eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("pci/2/1/macaddr")) //eth2(5G)
				nvram_set("pci/2/1/macaddr", "00:22:15:A5:03:04");
			if (nvram_get("regulation_domain_5G")) {
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("pci/2/1/ccode", nvram_get("regulation_domain_5G"));
			}else {
				nvram_set("wl1_country_code", "US");
				nvram_set("pci/2/1/ccode", "US");
			}
			nvram_set("pci/1/1/macaddr", nvram_safe_get("et0macaddr"));
			if (ptr) {
				nvram_set("wl0_country_code", ptr);
				nvram_set("pci/1/1/ccode", ptr);
			}else {
				nvram_set("wl0_country_code", "US");
				nvram_set("pci/1/1/ccode", "US");
			}
#ifdef RTCONFIG_ODMPID
			if (nvram_match("odmpid", "ASUS") ||
				!is_valid_hostname(nvram_safe_get("odmpid")))
				nvram_set("odmpid", "");
#endif
			break;
		default:
			if (!nvram_get("et0macaddr")) 
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			break;
	}

	if (nvram_get("secret_code"))
		nvram_set("wps_device_pin", nvram_get("secret_code"));
	else
		nvram_set("wps_device_pin", "12345670");
}

void chanspec_fix_5g(int unit)
{
	char tmp[100], prefix[]="wlXXXXXXX_";
	int channel;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	channel = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));

	if ((channel == 36) || (channel == 44) || (channel == 52) || (channel == 60) || (channel == 100) || (channel == 108) || (channel == 116) || (channel == 124) || (channel == 132) || (channel == 149) || (channel == 157))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "lower");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");
	}
	else if ((channel == 40) || (channel == 48) || (channel == 56) || (channel == 64) || (channel == 104) || (channel == 112) || (channel == 120) || (channel == 128) || (channel == 136) || (channel == 153) || (channel == 161))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "upper");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "upper");
	}
}

void chanspec_fix(int unit)
{
#if 0
	char *chanspec_5g_20m[] = {"36", "40", "44", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "120", "124", "128", "132", "136", "140", "149", "153", "157", "161", "165"};
#else
	char *chanspec_5g_20m[] = {"0", "36", "40", "44", "48", "56", "60", "64", "149", "153", "157", "161", "165"};
#endif
#if 0
	char *chanspec_5g_40m[] = {"40u", "48u", "56u", "64u", "104u", "112u", "120u", "128u", "136u", "153u", "161u", "36l", "44l", "52l", "60l", "100l", "108l", "116l", "124l", "132l", "149l", "157l"};
#else
	char *chanspec_5g_40m[] = {"0", "40u", "48u", "64u", "153u", "161u", "36l", "44l", "60l", "149l", "157l"};
#endif
#if 0
	char *chanspec_5g_80m[] = {"36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "149/80", "153/80", "157/80", "161/80"};
#else
	char *chanspec_5g_80m[] = {"0", "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80"};
#endif
	char *chanspec_20m[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"};
	char *chanspec_40m[] = {"0", "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u"};

	char tmp[100], prefix[]="wlXXXXXXX_";
	int i;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	dbG("unit: %d, bw_cap: %s, chanspec: %s\n", unit, nvram_safe_get(strcat_r(prefix, "bw_cap", tmp)), nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));

	if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
	{
		if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "3"))
			goto BAND_2G_BW_40M;
		else
			goto BAND_2G_BW_20M;
BAND_2G_BW_40M:
		for (i = 0; i < (sizeof(chanspec_40m)/sizeof(chanspec_40m[0])); i++)
		{
			if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_40m[i])) return;
		}
BAND_2G_BW_20M:
		for (i = 0; i < (sizeof(chanspec_20m)/sizeof(chanspec_20m[0])); i++)
		{
			if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_20m[i])) return;
		}
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "7"))
			goto BAND_5G_BW_80M;
		else if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "3"))
			goto BAND_5G_BW_40M;
		else
			goto BAND_5G_BW_20M;
BAND_5G_BW_80M:
		for (i = 0; i < (sizeof(chanspec_5g_80m)/sizeof(chanspec_5g_80m[0])); i++)
		{
			if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_80m[i])) return;
		}
BAND_5G_BW_40M:
		for (i = 0; i < (sizeof(chanspec_5g_40m)/sizeof(chanspec_5g_40m[0])); i++)
		{
			if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_40m[i])) return;
		}
BAND_5G_BW_20M:
		for (i = 0; i < (sizeof(chanspec_5g_20m)/sizeof(chanspec_5g_20m[0])); i++)
		{
			if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_20m[i])) return;
		}
	}

	dbG("reset %s for invalid setting\n", strcat_r(prefix, "chanspec", tmp));
	nvram_set(strcat_r(prefix, "chanspec", tmp), "0");
	nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
	nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
}

static int set_wltxpower_once = 0;

int wltxpower_rtn12hp(int txpower,
								char *tmp, char *prefix,
								char *tmp2, char *prefix2)
{
	int commit_needed = 0;
	int level;
	const struct txpower_s *p;

	if (txpower == 80)
	{
		if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
		{
			if (!nvram_match(strcat_r(prefix, "country_rev", tmp), "37"))
			{
				nvram_set(strcat_r(prefix2, "regrev", tmp2), "37");
				commit_needed++;
			}
		}
		else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
		{
			if (!nvram_match(strcat_r(prefix, "country_rev", tmp), "5"))
			{
				nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
				commit_needed++;
			}
		}
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
		{
			if (!nvram_match(strcat_r(prefix, "country_rev", tmp), "16"))
			{
				nvram_set(strcat_r(prefix2, "regrev", tmp2), "16");
				commit_needed++;
			}
		}
		else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
		{
			if (!nvram_match(strcat_r(prefix, "country_rev", tmp), "3"))
			{
				nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
				commit_needed++;
			}
		}
	}

#if 1	/* TMP, RT-N12HP */
	/* config power offset */
	level = 0;
	for (p = &txpower_list_rtn12hp[0]; p->min != 0; ++p) {
		if (txpower >= p->min && txpower <= p->max){
			dbG("[rc] txpoewr between: min:[%d] to max:[%d]\n",
									p->min, p->max);
			/* prefix2 is sb_1 */
			setpoweroffset_rtn12hp(level, prefix2);
			break;
		}
		level++;
	}

	if ( p->min == 0 )
		dbG("[rc] no correct power offset!\n");

	commit_needed = 1;
#endif

	return commit_needed;
}

int set_wltxpower()
{
	char ifnames[256];
	char name[64], ifname[64], *next = NULL;
	int unit = -1, subunit = -1;
	int i;
	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="pci/x/1/";
	int txpower = 80;
	int commit_needed = 0;
	int model;
	int wlopmode = (nvram_get("wlopmode") == NULL) ? 1 : nvram_get_int("wlopmode");

	// generate nvram nvram according to system setting
	model = get_model();

	if ((model != MODEL_RTAC66U) && (model != MODEL_RTN66U) && (model != MODEL_RTN12HP))
	{
		dbG("\n\tDon't do this!\n\n");
		return -1;
	}

	snprintf(ifnames, sizeof(ifnames), "%s %s",
		 nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
	remove_dups(ifnames, sizeof(ifnames));

	i = 0;
	foreach(name, ifnames, next) {
		if (nvifname_to_osifname(name, ifname, sizeof(ifname)) != 0)
			continue;

		if (wl_probe(ifname) || wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			continue;

		/* Convert eth name to wl name */
		if (osifname_to_nvifname(name, ifname, sizeof(ifname)) != 0)
			continue;

		/* Slave intefaces have a '.' in the name */
		if (strchr(ifname, '.'))
			continue;

		if (get_ifname_unit(ifname, &unit, &subunit) < 0)
			continue;

		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		switch(model) {
			case MODEL_RTN12:
			case MODEL_RTN12B1:
			case MODEL_RTN12C1:
			case MODEL_RTN12D1:
			case MODEL_RTN12HP:
			case MODEL_RTN53:
			case MODEL_RTN15U:
			case MODEL_RTN10U:
			case MODEL_RTN10D1:
			case MODEL_RTN16:
			{
				if(unit == 0){	/* 2.4G */
					snprintf(prefix2, sizeof(prefix2), "sb/1/");
				}else{	/* 5G */
					snprintf(prefix2, sizeof(prefix2), "0:");
				}
				break;
			}

			case MODEL_RTN66U:
			case MODEL_RTAC66U:
			{
				snprintf(prefix2, sizeof(prefix2), "pci/%d/1/", unit + 1);
				break;
			}
		}

		if (strcmp(nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)),
			nvram_safe_get(strcat_r(prefix, "country_rev", tmp))))
		{
			nvram_set(strcat_r(prefix, "country_rev", tmp),
				nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)));
			commit_needed++;
		}

		txpower = nvram_get_int(wl_nvname("TxPower", unit, 0));
		dbG("unit: %d, txpower: %d\n", unit, txpower);

		switch(model) {
			case MODEL_RTAC66U:
				if (wlopmode == 0)
				{
					if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "12"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "12");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "12");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
					{
						nvram_set("regulation_domain_5G", "US");
						nvram_set(strcat_r(prefix, "country_code", tmp), "US");
						nvram_set(strcat_r(prefix2, "ccode", tmp2), "US");
						nvram_set(strcat_r(prefix, "country_rev", tmp), "12");
						nvram_set(strcat_r(prefix2, "regrev", tmp2), "12");
						commit_needed++;
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "31"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "31");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "31");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "9"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "9");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "9");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "11"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "11");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "11");
							commit_needed++;
						}
					}
				}
				else if (wlopmode == 7)
				{
					if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
					{
						nvram_set("regulation_domain_5G", "Q2");
						nvram_set(strcat_r(prefix, "country_code", tmp), "Q2");
						nvram_set(strcat_r(prefix2, "ccode", tmp2), "Q2");
						nvram_set(strcat_r(prefix, "country_rev", tmp), "12");
						nvram_set(strcat_r(prefix2, "regrev", tmp2), "12");
						commit_needed++;
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "12"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "12");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "12");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "15"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "15");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "15");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "4"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "4");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "4");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "5"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "5");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
							commit_needed++;
						}
					}
				}
				else
				{
					if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
					{
						nvram_set("regulation_domain", "US");
						nvram_set(strcat_r(prefix, "country_code", tmp), "US");
						nvram_set(strcat_r(prefix2, "ccode", tmp2), "US");
						nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
						nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
						commit_needed++;
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "13"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "13");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "13");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
							commit_needed++;
						}
					}
					else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
					{
						if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "1"))
						{
							nvram_set(strcat_r(prefix, "country_rev", tmp), "1");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "1");
							commit_needed++;
						}
					}
				}

				if (set_wltxpower_once) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (txpower < 20)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x34"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x34");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x34");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x34");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0x11111111");
								commit_needed++;
							}
						}
						else if (txpower < 40)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x40"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0x77741111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0x77741111");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0x77763333");
								commit_needed++;
							}
						}
						else if (txpower < 70)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x4C"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xDC963333");
								commit_needed++;
							}
						}
						else if (txpower < 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFC963333");
								commit_needed++;
							}
						}
						else if (txpower == 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x64"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x74111111");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xDA741111");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFC963333");
								commit_needed++;
							}
						}
						else
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x70"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x97555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x97555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xDA755555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xDA755555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFC965555");
								commit_needed++;
							}
						}
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < 20)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "52,52,52,52"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "52,52,52,52");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "52,52,52,52");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "52,52,52,52");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0x33333333");
								commit_needed++;
							}
						}
						else if (txpower < 40)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "64,64,64,64"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "64,64,64,64");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "64,64,64,64");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "64,64,64,64");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0x99975333");
								commit_needed++;
							}
						}
						else if (txpower < 70)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "76,76,76,76"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "76,76,76,76");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "76,76,76,76");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "76,76,76,76");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0x99975333");
								commit_needed++;
							}
						}
						else if (txpower < 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "88,88,88,88"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "88,88,88,88");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "88,88,88,88");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "88,88,88,88");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0x99975333");
								commit_needed++;
							}
						}
						else if (txpower == 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "100,100,100,100"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "100,100,100,100");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "100,100,100,100");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "100,100,100,100");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0x99975333");
								commit_needed++;
							}
						}
						else
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), ""))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2), "104,104,104,104");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2), "104,104,104,104");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2), "104,104,104,104");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2), "0xBB975311");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2), "0xBB975311");
								commit_needed++;
							}
						}
					}
				}
#if 0
				if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
				{
					dbG("maxp2ga0: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga0", tmp2)) ? : "NULL");
					dbG("maxp2ga1: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga1", tmp2)) ? : "NULL");
					dbG("maxp2ga2: %s\n", nvram_get(strcat_r(prefix2, "maxp2ga2", tmp2)) ? : "NULL");
					dbG("cckbw202gpo: %s\n", nvram_get(strcat_r(prefix2, "cckbw202gpo", tmp2)) ? : "NULL");
					dbG("cckbw20ul2gpo: %s\n", nvram_get(strcat_r(prefix2, "cckbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("legofdmbw202gpo: %s\n", nvram_get(strcat_r(prefix2, "legofdmbw202gpo", tmp2)) ? : "NULL");
					dbG("legofdmbw20ul2gpo: %s\n", nvram_get(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("mcsbw202gpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw202gpo", tmp2)) ? : "NULL");
					dbG("mcsbw20ul2gpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("mcsbw402gpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw402gpo", tmp2)) ? : "NULL");
				}
				else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
				{
					dbG("maxp5ga0: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)) ? : "NULL");
					dbG("maxp5ga1: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) ? : "NULL");
					dbG("maxp5ga2: %s\n", nvram_get(strcat_r(prefix2, "maxp5ga2", tmp2)) ? : "NULL");
					dbG("mcsbw205glpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw205glpo", tmp2)) ? : "NULL");
					dbG("mcsbw405glpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw405glpo", tmp2)) ? : "NULL");
					dbG("mcsbw805glpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw805glpo", tmp2)) ? : "NULL");
					dbG("mcsbw205gmpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw205gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw405gmpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw405gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw805gmpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw805gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw205ghpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw205ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw405ghpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw405ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw805ghpo: %s\n", nvram_get(strcat_r(prefix2, "mcsbw805ghpo", tmp2)) ? : "NULL");
				}
				dbG("ccode: %s\n", nvram_safe_get(strcat_r(prefix2, "ccode", tmp2)));
				dbG("regrev: %s\n", nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)));
				dbG("country_code: %s\n", nvram_safe_get(strcat_r(prefix, "country_code", tmp)));
				dbG("country_rev: %s\n", nvram_safe_get(strcat_r(prefix, "country_rev", tmp)));
#endif
				break;

			case MODEL_RTN66U:
				if (wlopmode == 0)
				{
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "63"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "63");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "63");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "9"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "9");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "9");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "14"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "14");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "14");
								commit_needed++;
							}
						}
					}
					else								// 5G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							nvram_set("regulation_domain_5G", "Q2");
							nvram_set(strcat_r(prefix, "country_code", tmp), "Q2");
							nvram_set(strcat_r(prefix2, "ccode", tmp2), "Q2");
							nvram_set(strcat_r(prefix, "country_rev", tmp), "3");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
							commit_needed++;
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "3"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "3");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "9"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "9");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "9");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "5"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "5");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "5"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "5");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "14"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "14");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "14");
								commit_needed++;
							}
						}
					}
				}
				else if (wlopmode == 7)
				{
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "2"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "2");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "2");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "5"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "5");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "5");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "13"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "13");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "13");
								commit_needed++;
							}
						}
					}
					else								// 5G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							nvram_set("regulation_domain_5G", "Q2");
							nvram_set(strcat_r(prefix, "country_code", tmp), "Q2");
							nvram_set(strcat_r(prefix2, "ccode", tmp2), "Q2");
							nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
							commit_needed++;
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "3"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "3");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "13"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "13");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "13");
								commit_needed++;
							}
						}
					}
				}
				else
				{
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "39"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "39");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "39");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "3"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "3");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "3");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "13"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "13");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "13");
								commit_needed++;
							}
						}
					}
					else								// 5G
					{
						if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
						{
							nvram_set("regulation_domain_5G", "Q2");
							nvram_set(strcat_r(prefix, "country_code", tmp), "Q2");
							nvram_set(strcat_r(prefix2, "ccode", tmp2), "Q2");
							nvram_set(strcat_r(prefix, "country_rev", tmp), "2");
							nvram_set(strcat_r(prefix2, "regrev", tmp2), "2");
							commit_needed++;
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "Q2"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "2"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "2");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "2");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "EU"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "TW"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "CN"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "0"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "0");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "0");
								commit_needed++;
							}
						}
						else if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
						{
							if (!nvram_match(strcat_r(prefix2, "regrev", tmp2), "13"))
							{
								nvram_set(strcat_r(prefix, "country_rev", tmp), "13");
								nvram_set(strcat_r(prefix2, "regrev", tmp2), "13");
								commit_needed++;
							}
						}
					}
				}

				if (set_wltxpower_once || nvram_match("bl_version", "1.0.0.9")) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (txpower < 20)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x38"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x38");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x38");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x38");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x3333");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x3333");
								commit_needed++;
							}
						}
						else if (txpower < 40)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x40"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x40");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0x77755555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0x77755555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x7777");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x2222");
								commit_needed++;
							}
						}
						else if (txpower < 70)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x4C"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x4C");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xDC955555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xDC955555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xDDDD9999");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x9999");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x4444");
								commit_needed++;
							}
						}
						else if (txpower < 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x58");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFFFF9999");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x9999");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x4444");
								commit_needed++;
							}
						}
						else if (txpower == 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x64"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x64");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x55555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFFFF9999");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x9999");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x4444");
								commit_needed++;
							}
						}
						else
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x70"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),		"0x70");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "legofdmbw202gpo", tmp2),	"0x97555555");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2),	"0x97555555");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2),	"0xFC955555");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),	"0xFFFF9999");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x9999");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x4444");
								commit_needed++;
							}
						}
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < 20)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x30"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x30");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x11111111");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x1111");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x0000");
								commit_needed++;
							}
						}
						else if (txpower < 40)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x3A"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x3A");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x65311111");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x2222");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x2222");
								commit_needed++;
							}
						}
						else if (txpower < 70)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x46"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x46");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x2222");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x2222");
								commit_needed++;
							}
						}
						else if (txpower < 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x52"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x52");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x2222");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x2222");
								commit_needed++;
							}
						}
						else if (txpower == 80)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x5E"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x5E");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x75311111");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x2222");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x2222");
								commit_needed++;
							}
						}
						else
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "0x6A"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "legofdmbw205gmpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2),"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "maxp5gha0", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "maxp5gha1", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "maxp5gha2", tmp2),		"0x6A");
								nvram_set(strcat_r(prefix2, "legofdmbw205ghpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2),"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x77777777");
								nvram_set(strcat_r(prefix2, "mcs32po", tmp2),		"0x7777");
								nvram_set(strcat_r(prefix2, "legofdm40duppo", tmp2),	"0x0000");
								commit_needed++;
							}
						}
					}
				}
#if 0
				if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
				{
					dbG("maxp2ga0: %s\n",		nvram_get(strcat_r(prefix2, "maxp2ga0", tmp2)) ? : "NULL");
					dbG("maxp2ga1: %s\n",		nvram_get(strcat_r(prefix2, "maxp2ga1", tmp2)) ? : "NULL");
					dbG("maxp2ga2: %s\n",		nvram_get(strcat_r(prefix2, "maxp2ga2", tmp2)) ? : "NULL");
					dbG("cckbw202gpo: %s\n",	nvram_get(strcat_r(prefix2, "cckbw202gpo", tmp2)) ? : "NULL");
					dbG("cckbw20ul2gpo: %s\n",	nvram_get(strcat_r(prefix2, "cckbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("legofdmbw202gpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw202gpo", tmp2)) ? : "NULL");
					dbG("legofdmbw20ul2gpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("mcsbw202gpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw202gpo", tmp2)) ? : "NULL");
					dbG("mcsbw20ul2gpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw20ul2gpo", tmp2)) ? : "NULL");
					dbG("mcsbw402gpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw402gpo", tmp2)) ? : "NULL");
					dbG("mcs32po: %s\n",		nvram_get(strcat_r(prefix2, "mcs32po", tmp2)) ? : "NULL");
					dbG("legofdm40duppo: %s\n",	nvram_get(strcat_r(prefix2, "legofdm40duppo", tmp2)) ? : "NULL");
				}
				else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
				{
					dbG("maxp5ga0: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)) ? : "NULL");
					dbG("maxp5ga1: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) ? : "NULL");
					dbG("maxp5ga2: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga2", tmp2)) ? : "NULL");
					dbG("legofdmbw205gmpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw205gmpo", tmp2)) ? : "NULL");
					dbG("legofdmbw20ul5gmpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw20ul5gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw205gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw205gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw20ul5gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw20ul5gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw405gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw405gmpo", tmp2)) ? : "NULL");
					dbG("maxp5gha0: %s\n",		nvram_get(strcat_r(prefix2, "maxp5gha0", tmp2)) ? : "NULL");
					dbG("maxp5gha1: %s\n",		nvram_get(strcat_r(prefix2, "maxp5gha1", tmp2)) ? : "NULL");
					dbG("maxp5gha2: %s\n",		nvram_get(strcat_r(prefix2, "maxp5gha2", tmp2)) ? : "NULL");
					dbG("legofdmbw205ghpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw205ghpo", tmp2)) ? : "NULL");
					dbG("legofdmbw20ul5ghpo: %s\n",	nvram_get(strcat_r(prefix2, "legofdmbw20ul5ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw205ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw205ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw20ul5ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw20ul5ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw405ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw405ghpo", tmp2)) ? : "NULL");
					dbG("mcs32po: %s\n",		nvram_get(strcat_r(prefix2, "mcs32po", tmp2)) ? : "NULL");
					dbG("legofdm40duppo: %s\n",	nvram_get(strcat_r(prefix2, "legofdm40duppo", tmp2)) ? : "NULL");
				}
				dbG("ccode: %s\n", nvram_safe_get(strcat_r(prefix2, "ccode", tmp2)));
				dbG("regrev: %s\n", nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)));
				dbG("country_code: %s\n", nvram_safe_get(strcat_r(prefix, "country_code", tmp)));
				dbG("country_rev: %s\n", nvram_safe_get(strcat_r(prefix, "country_rev", tmp)));
#endif
				break;

			case MODEL_RTN12HP:

				commit_needed = wltxpower_rtn12hp(txpower, tmp, prefix,
															tmp2, prefix2);
				break;

			default:

				break;
		}

		i++;
	}

	if (!set_wltxpower_once)
		set_wltxpower_once = 1;

	if (commit_needed)
		nvram_commit();

	return 0;
}

#ifdef RTCONFIG_WIRELESSREPEATER
// this function is used to jutisfy which band(unit) to be forced connected.
int is_ure(int unit)
{
	// forced to connect to which band
	// is it suitable 
	if(nvram_get_int("sw_mode")==SW_MODE_REPEATER) {
		if(nvram_get_int("wlc_band")==unit) return 1;
	}
	return 0;
}
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
int is_psta(int unit)
{
	if (unit < 0) return 0;

	if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
		(nvram_get_int("wlc_psta") == 1) &&
		(nvram_get_int("wlc_band") == unit))
		return 1;

	return 0;
}
#endif
#endif
void generate_wl_para(int unit, int subunit)
{
	dbG("unit %d subunit %d\n", unit, subunit);

	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="wlXXXXXXX_";
	char list[640];
	char *nv, *nvp, *b, *c;
#ifndef RTCONFIG_BCMWL6
	char word[256], *next;
	int match;
#endif
	if (subunit == -1)
	{
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (nvram_match("wps_enable", "1") /*&& (unit == nvram_get_int("wps_band"))*/
			&& !no_need_to_start_wps())
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "enabled");
		else
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		snprintf(prefix2, sizeof(prefix2), "wl%d_", unit);
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		if (is_psta(unit) /*|| is_psta(1 - unit)*/)
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
#endif
#endif
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	// convert wlc_xxx to wlX_ according to wlc_band == unit
	if (is_ure(unit)) {
		if(subunit==-1) {
			nvram_set("ure_disable", "0");

			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));

			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));

			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));

			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bw", tmp), "0");
#else
			nvram_set(strcat_r(prefix, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));
#endif
		}
		else if(subunit==1) {
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
			//nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("et0macaddr"));
/*
			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ure_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			nvram_set(strcat_r(prefix, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));
*/
		}
	}
	//TODO: recover nvram from repeater
	else
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	if (is_psta(unit))
	{
		if (subunit == -1)
		{
			nvram_set("ure_disable", "1");

			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			if (nvram_match(strcat_r(prefix, "phytype", tmp), "v")) // 802.11AC
				nvram_set(strcat_r(prefix, "bw", tmp), "3");
			else
				nvram_set(strcat_r(prefix, "bw", tmp), "2");
		}
		else
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
	}
	else
#endif
#endif
	nvram_set("ure_disable", "1");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared"))
	{
		nvram_set("wps_enable", "0");
		nvram_set(strcat_r(prefix, "auth", tmp), "1");
	}
	else nvram_set(strcat_r(prefix, "auth", tmp), "0");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk2");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk psk2");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa");
		nvram_set("wps_enable", "0");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa2");
		nvram_set("wps_enable", "0");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa wpa2");
		nvram_set("wps_enable", "0");
	}
	else nvram_set(strcat_r(prefix, "akm", tmp), "");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius")) 
	{
		nvram_set(strcat_r(prefix, "auth_mode", tmp), "radius");
		nvram_set(strcat_r(prefix, "key", tmp), "2");
		nvram_set("wps_enable", "0");
	}
	else nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else if (!nvram_match(strcat_r(prefix, "akm", tmp), ""))
		nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
	else if (!nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else nvram_set(strcat_r(prefix, "wep", tmp), "disabled");

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
		strstr(strcat_r(prefix, "akm", tmp), "wpa"))
		nvram_set(strcat_r(prefix, "preauth", tmp), "1");
	else
		nvram_set(strcat_r(prefix, "preauth", tmp), "");

	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {
		list[0] = 0;
		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b)==0) continue;
				if((vstrsep(b, ">", &c)!=1)) continue;
				if (list[0]==0)
					sprintf(list, "%s", b);
				else
					sprintf(list, "%s %s", list, b);
			}
			free(nv);
		}
		nvram_set(strcat_r(prefix, "maclist", tmp), list);
	}
	else nvram_set(strcat_r(prefix, "maclist", tmp), "");

	// reset nmode to "auto"
	nvram_set(strcat_r(prefix, "nmode", tmp), "-1");

	if (subunit==-1)
	{
		// wds mode control
#ifdef RTCONFIG_WIRELESSREPEATER
		if (is_ure(unit)) nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		else
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		if (is_psta(unit))
		{
			nvram_set(strcat_r(prefix, "mode", tmp), "psta");
			nvram_set(strcat_r(prefix, "ure", tmp), "0");
		}
		else
#endif
#endif
		if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1")) // wds only
			nvram_set(strcat_r(prefix, "mode", tmp), "wds");

		else if (nvram_match(strcat_r(prefix, "mode_x", tmp), "3")) // special defined for client
			nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		else nvram_set(strcat_r(prefix, "mode", tmp), "ap");

		// TODO use lazwds directly
		//if(!nvram_match(strcat_r(prefix, "wdsmode_ex", tmp), "2"))
		//	nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
		//else nvram_set(strcat_r(prefix, "lazywds", tmp), "0");

		// TODO need sw_mode ?
		// handle wireless wds list
		if (!nvram_match(strcat_r(prefix, "mode_x", tmp), "0")) {
			if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "1")) {
				list[0] = 0;
				nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "wdslist", tmp)));
				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						if (strlen(b)==0) continue;
						if (list[0]==0) 
							sprintf(list, "%s", b);
						else
							sprintf(list, "%s %s", list, b);
					}
					free(nv);
				}
				nvram_set(strcat_r(prefix, "wds", tmp), list);
				nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
			}
			else {
				nvram_set(strcat_r(prefix, "wds", tmp), "");
				nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
			}
		} else {
			nvram_set(strcat_r(prefix, "wds", tmp), "");
			nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
		}

		if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "0"))		// disable, but bcm says "auto"
			nvram_set(strcat_r(prefix, "mrate", tmp), "0");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "1"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "1000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "2"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "2000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "3"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "5500000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "4"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "6000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "5"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "9000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "6"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "11000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "7"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "12000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "8"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "18000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "9"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "24000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "10"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "36000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "11"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "48000000");
		else if (nvram_match(strcat_r(prefix, "mrate_x", tmp), "12"))
			nvram_set(strcat_r(prefix, "mrate", tmp), "54000000");
		else
			nvram_set(strcat_r(prefix, "mrate", tmp), "0");

		if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "0"))		// auto => b/g/n mixed or a/n mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "1"))	// n only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
		}
#if 0
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "2"))	// b/g mixed
#else
		else								// b/g mixed
#endif
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");

			nvram_set(strcat_r(prefix, "bw", tmp), "1");		// reset to default setting
		}

#ifdef RTCONFIG_BCMWL6
		if (nvram_match(strcat_r(prefix, "bw", tmp), "0"))		// Auto
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");// 40M
			else
			{
				if (get_model() == MODEL_RTAC66U)
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "7");// 80M
				else
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			}

			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "1"))		// 20M
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))		// 40M
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "3"))		// 80M
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			else
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "7");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}

		chanspec_fix(unit);
#else
		if (nvram_match(strcat_r(prefix, "bw", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "0");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}

		if (unit) chanspec_fix_5g(unit);

		match = 0;
		foreach (word, "lower upper", next)
		{
			if (nvram_match(strcat_r(prefix, "nctrlsb", tmp), word))
			{
				match = 1;
				break;
			}
		}
/*
		if ((nvram_match(strcat_r(prefix, "channel", tmp), "0")))
			nvram_unset(strcat_r(prefix, "nctrlsb", tmp));
		else
*/
		if (!match)
			nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");
#endif

#ifdef RTCONFIG_EMF
		/* overwrite if emf is enabled */
		if (nvram_get_int("emf_enable") ||
		    nvram_get_int(strcat_r(prefix, "mrate", tmp))) {
			//nvram_set(strcat_r(prefix, "mrate", tmp), "54000000");
			nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), "1");
		}
#endif

		dbG("bw: %s\n", nvram_safe_get(strcat_r(prefix, "bw", tmp)));
#ifdef RTCONFIG_BCMWL6
		dbG("chanspec: %s\n", nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));
		dbG("bw_cap: %s\n", nvram_safe_get(strcat_r(prefix, "bw_cap", tmp)));
#else
		dbG("channel: %s\n", nvram_safe_get(strcat_r(prefix, "channel", tmp)));
		dbG("nbw_cap: %s\n", nvram_safe_get(strcat_r(prefix, "nbw_cap", tmp)));
		dbG("nctrlsb: %s\n", nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp)));
#endif
		dbG("obss_coex: %s\n", nvram_safe_get(strcat_r(prefix, "obss_coex", tmp)));
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), nvram_safe_get(strcat_r(prefix2, "bss_maxassoc", tmp2)));
			nvram_set(strcat_r(prefix, "ap_isolate", tmp), nvram_safe_get(strcat_r(prefix2, "ap_isolate", tmp2)));
			nvram_set(strcat_r(prefix, "net_reauth", tmp), nvram_safe_get(strcat_r(prefix2, "net_reauth", tmp2)));
			nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), nvram_safe_get(strcat_r(prefix2, "radius_ipaddr", tmp2)));
			nvram_set(strcat_r(prefix, "radius_key", tmp), nvram_safe_get(strcat_r(prefix2, "radius_key", tmp2)));
			nvram_set(strcat_r(prefix, "radius_port", tmp), nvram_safe_get(strcat_r(prefix2, "radius_port", tmp2)));
			nvram_set(strcat_r(prefix, "wme", tmp), nvram_safe_get(strcat_r(prefix2, "wme", tmp2)));
			nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), nvram_safe_get(strcat_r(prefix2, "wme_bss_disable", tmp2)));
			nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), nvram_safe_get(strcat_r(prefix2, "wpa_gtk_rekey", tmp2)));
			nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), nvram_safe_get(strcat_r(prefix2, "wmf_bss_enable", tmp2)));
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled") ||
		nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
	{
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#if 0
		if (subunit != -1)
		{
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
		}
#endif

		strcpy(tmp2, nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));
		if ((nvp = strchr(tmp2, '/')) || (nvp = strchr(tmp2, 'l'))
			|| (nvp = strchr(tmp2, 'u')))
		{
			*nvp = '\0';
			nvram_set(strcat_r(prefix, "chanspec", tmp), tmp2);
		}
	}
	else
	{
		//nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#if 0
		if (subunit != -1)
		{
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
		}
#endif
	}
}

void
set_wan_tag(char *interface) {
	int model, wan_vid, iptv_vid, voip_vid, wan_prio, iptv_prio, voip_prio;
	char wan_dev[10], port_id[7], tag_register[7], vlan_entry[7];

	model = get_model();
	wan_vid = nvram_get_int("switch_wan0tagid");
	iptv_vid = nvram_get_int("switch_wan1tagid");
	voip_vid = nvram_get_int("switch_wan2tagid");
	wan_prio = nvram_get_int("switch_wan0prio");
	iptv_prio = nvram_get_int("switch_wan1prio");
	voip_prio = nvram_get_int("switch_wan2prio");

	sprintf(wan_dev, "vlan%d", wan_vid);

	switch(model) {
	case MODEL_RTN12:
	case MODEL_RTN12B1:
	case MODEL_RTN12C1:
	case MODEL_RTN12D1:
	case MODEL_RTN12HP:
	case MODEL_RTN53:
		/* Reset vlan 1 */
		eval("vconfig", "rem", "vlan1");
		eval("et", "robowr", "0x34", "0x8", "0x01001000");
		eval("et", "robowr", "0x34", "0x6", "0x3001");
		/* Add wan interface */
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		/* Set Wan prio*/
		if(!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");

		if(nvram_match("switch_wantag", "unifi_home")) {
			/* vlan0ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010003ae");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258051");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x10", "0x0258");
		}
		else if(nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		}
		else if(nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan0ports= 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100032c");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014051");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e012"); /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x10", "0x8014");
		}
		else if(nvram_match("switch_wantag", "singtel_others")) {
			/* vlan0ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010003ae");
			/* vlan0ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010003ae");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014051");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x10", "0x8014");
		}
		if(nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 0 2 3 5 */			   /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d"); /*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f030"); /*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01453012"); /*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");

		}
		break;

        case MODEL_RTN10U:
                /* Reset vlan 1 */
                eval("vconfig", "rem", "vlan1");
                eval("et", "robowr", "0x34", "0x8", "0x01001000");
                eval("et", "robowr", "0x34", "0x6", "0x3001");
                /* Add wan interface */
                sprintf(port_id, "%d", wan_vid);
                eval("vconfig", "add", interface, port_id);
                /* Set Wan prio*/
                if(!nvram_match("switch_wan0prio", "0"))
                        eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
                /* Enable high bits check */
                eval("et", "robowr", "0x34", "0x3", "0x0080");

                if(nvram_match("switch_wantag", "unifi_home")) {
                        /* vlan0ports= 2 3 4 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x0100073c");
                        eval("et", "robowr", "0x34", "0x6", "0x3000");
                        /* vlan500ports= 0 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x011f4021");
                        eval("et", "robowr", "0x34", "0x6", "0x31f4");
                        /* vlan600ports= 0 1 */
                        eval("et", "robowr", "0x34", "0x8", "0x01258083");
                        eval("et", "robowr", "0x34", "0x6", "0x3258");
                        /* LAN4 vlan tag */
                        eval("et", "robowr", "0x34", "0x12", "0x0258");
                }
                else if(nvram_match("switch_wantag", "unifi_biz")) {
                        /* Modify vlan500ports= 0 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x011f4021");
                        eval("et", "robowr", "0x34", "0x6", "0x31f4");
                }
                else if(nvram_match("switch_wantag", "singtel_mio")) {
                        /* vlan0ports= 3 4 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x01000638");
                        eval("et", "robowr", "0x34", "0x6", "0x3000");
                        /* vlan10ports= 0 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x0100a021");
                        eval("et", "robowr", "0x34", "0x6", "0x300a");
                        /* vlan20ports= 1 0 */
                        eval("et", "robowr", "0x34", "0x8", "0x01014083");
                        eval("et", "robowr", "0x34", "0x6", "0x3014");
                        /* vlan30ports= 2 0 */
                        eval("et", "robowr", "0x34", "0x8", "0x0101e005"); /*Just forward without untag*/
                        eval("et", "robowr", "0x34", "0x6", "0x301e");
                        /* LAN4 vlan tag & prio */
                        eval("et", "robowr", "0x34", "0x12", "0x8014");
                }
                else if(nvram_match("switch_wantag", "singtel_others")) {
                        /* vlan0ports= 2 3 4 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x0100073c");
                        eval("et", "robowr", "0x34", "0x6", "0x3000");
                        /* vlan10ports= 0 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x0100a021");
                        eval("et", "robowr", "0x34", "0x6", "0x300a");
                        /* vlan20ports= 0 1 */
                        eval("et", "robowr", "0x34", "0x8", "0x01014083");
                        eval("et", "robowr", "0x34", "0x6", "0x3014");
                        /* LAN4 vlan tag & prio */
                        eval("et", "robowr", "0x34", "0x12", "0x8014");
                }
                if(nvram_match("switch_wantag", "m1_fiber")) {
                        /* vlan0ports= 1 3 4 5 */                          /*5432 1054 3210*/
                        eval("et", "robowr", "0x34", "0x8", "0x010006ba"); /*0110|1011|1010*/
                        eval("et", "robowr", "0x34", "0x6", "0x3000");
                        /* vlan1103ports= 0 5 */
                        eval("et", "robowr", "0x34", "0x8", "0x0144f021"); /*0000|0010|0001*/
                        eval("et", "robowr", "0x34", "0x6", "0x344f");
                        /* vlan1107ports= 2 0 */
                        eval("et", "robowr", "0x34", "0x8", "0x01453005"); /*0000|0000|0101*/ /*Just forward without untag*/
                        eval("et", "robowr", "0x34", "0x6", "0x3453");
                }
                break;

	case MODEL_RTN16:
		eval("vconfig", "rem", "vlan2");
		//config wan port
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		sprintf(vlan_entry, "0x%x", wan_vid);
		eval("et", "robowr", "0x05", "0x83", "0x0101");
		eval("et", "robowr", "0x05", "0x81", vlan_entry);
		eval("et", "robowr", "0x05", "0x80", "0x0000");
		eval("et", "robowr", "0x05", "0x80", "0x0080");

		//Set Wan port PRIO
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if(nvram_match("switch_stb_x", "3")) {
			//config LAN 3 = VoIP
			if(nvram_match("switch_wantag", "m1_fiber")) {
				//Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else { //Nomo case, untag it.
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				//Set vlan table entry register
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		else if(nvram_match("switch_stb_x", "4")) {
			//config LAN 4 = IPTV
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x12", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			//Set vlan table entry register
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0403");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		else if(nvram_match("switch_stb_x", "6")) {
			//config LAN 3 = VoIP
			if(nvram_match("switch_wantag", "singtel_mio")) {
				//Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else { //Nomo case, untag it.
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				//Set vlan table entry register
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			//config LAN 4 = IPTV
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x12", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			//Set vlan table entry register
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0403");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		break;

	case MODEL_RTN66U:
	case MODEL_RTAC66U:
		eval("vconfig", "rem", "vlan2");
		/* config wan port */
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		sprintf(vlan_entry, "0x%x", wan_vid);
		eval("et", "robowr", "0x05", "0x83", "0x0101");
		eval("et", "robowr", "0x05", "0x81", vlan_entry);
		eval("et", "robowr", "0x05", "0x80", "0x0000");
		eval("et", "robowr", "0x05", "0x80", "0x0080");
		/* Set Wan port PRIO */
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if(nvram_match("switch_stb_x", "3")) {
			if(nvram_match("switch_wantag", "m1_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		else if(nvram_match("switch_stb_x", "4")) {
			/* config LAN 4 = IPTV */
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x18", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			/* Set vlan table entry register */
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x2011");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		else if(nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */
			if(nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			/* config LAN 4 = IPTV */
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x18", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			/* Set vlan table entry register */
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x2011");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		break;

        case MODEL_RTN15U:
                eval("vconfig", "rem", "vlan2");
                /* config wan port */
                sprintf(port_id, "%d", wan_vid);
                eval("vconfig", "add", interface, port_id);
                sprintf(vlan_entry, "0x%x", wan_vid);
                eval("et", "robowr", "0x05", "0x83", "0x0110");
                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                eval("et", "robowr", "0x05", "0x80", "0x0000");
                eval("et", "robowr", "0x05", "0x80", "0x0080");
                /* Set Wan port PRIO */
                if(nvram_invmatch("switch_wan0prio", "0"))
                        eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

                if(nvram_match("switch_stb_x", "3")) {
                        if(nvram_match("switch_wantag", "m1_fiber")) {
                                /* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0012");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                        else {  /* Nomo case, untag it. */
                                voip_prio = voip_prio << 13;
                                sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
                                eval("et", "robowr", "0x34", "0x12", tag_register);
                                _dprintf("lan 3 tag register: %s\n", tag_register);
                                /* Set vlan table entry register */
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0412");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                }
                else if(nvram_match("switch_stb_x", "4")) {
                        /* config LAN 4 = IPTV */
                        iptv_prio = iptv_prio << 13;
                        sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
                        eval("et", "robowr", "0x34", "0x10", tag_register);
                        _dprintf("lan 4 tag register: %s\n", tag_register);
                        /* Set vlan table entry register */
                        sprintf(vlan_entry, "0x%x", iptv_vid);
                        _dprintf("vlan entry: %s\n", vlan_entry);
                        eval("et", "robowr", "0x05", "0x83", "0x0211");
                        eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }
                else if(nvram_match("switch_stb_x", "6")) {
                        /* config LAN 3 = VoIP */
                        if(nvram_match("switch_wantag", "singtel_mio")) {
                                /* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x0012");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                        else {  /* Nomo case, untag it. */
                                voip_prio = voip_prio << 13;
                                sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
                                eval("et", "robowr", "0x34", "0x12", tag_register);
                                _dprintf("lan 3 tag register: %s\n", tag_register);
                                /* Set vlan table entry register */
                                sprintf(vlan_entry, "0x%x", voip_vid);
                                _dprintf("vlan entry: %s\n", vlan_entry);
                                eval("et", "robowr", "0x05", "0x83", "0x412");
                                eval("et", "robowr", "0x05", "0x81", vlan_entry);
                                eval("et", "robowr", "0x05", "0x80", "0x0000");
                                eval("et", "robowr", "0x05", "0x80", "0x0080");
                        }
                        /* config LAN 4 = IPTV */
                        iptv_prio = iptv_prio << 13;
                        sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
                        eval("et", "robowr", "0x34", "0x10", tag_register);
                        _dprintf("lan 4 tag register: %s\n", tag_register);
                        /* Set vlan table entry register */
                        sprintf(vlan_entry, "0x%x", iptv_vid);
                        _dprintf("vlan entry: %s\n", vlan_entry);
                        eval("et", "robowr", "0x05", "0x83", "0x0211");
                        eval("et", "robowr", "0x05", "0x81", vlan_entry);
                        eval("et", "robowr", "0x05", "0x80", "0x0000");
                        eval("et", "robowr", "0x05", "0x80", "0x0080");
                }
		break;
	}
	return;
}

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
	sprintf(buf, "wl%d.%d", unit, subunit);

	return buf;
}

int
wl_exist(char *ifname, int band)
{
	FILE *fp;
	char buf[128], *line;
	int ret = 1;

	sprintf(buf, "wl -i %s bands", ifname);
	fp = popen(buf, "r");
	if (fp == NULL) {
		perror("popen");
		return 0;
	}

	line = fgets(buf, sizeof(buf), fp);
	if ((line == NULL) ||
	    (strstr(line, "not found") != NULL) ||
	    (band == 1 && !strstr(line, "b ")) ||
	    (band == 2 && !strstr(line, "a "))) {
		_dprintf("No wireless %s interface!!!: %s\n", ifname, line ? : "");
		ret = 0;
	}
	pclose(fp);

	return ret;
}

void ate_commit_bootloader(char *err_code) {
	nvram_set("Ate_power_on_off_enable", err_code);
	nvram_commit();
	nvram_set("asuscfeAte_power_on_off_ret", err_code);
	nvram_set("asuscfeAte_fail_reboot_log", nvram_get("Ate_reboot_log"));
	nvram_set("asuscfeAte_fail_dev_log", nvram_get("Ate_dev_log"));
	nvram_set("asuscfecommit=", "1");
}

