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

int wan_phyid = -1;

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

struct txpower_ac_s {
	uint16 min;
	uint16 max;
	uint8 maxp2ga0;
	uint8 maxp2ga1;
	uint8 maxp2ga2;
};

static const struct txpower_s txpower_list_rtn12hp[] = {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else
	/* 1-20mW */
	{ 1, 20, 0x42, 0x42, 0x0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0, 0x0, 0x0, 0x0},
	/* 20-40mW */
	{ 21, 40, 0x4A, 0x4A, 0x0, 0x2000, 0x4442, 0x2200, 0x4444, 0x2200, 0x4444, 0x4422, 0x4444, 0x4422, 0x4444, 0x0, 0x0, 0x0, 0x0},
	/* 40-60mW */
	{ 41, 60, 0x52, 0x52, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* 60-80mW */
	{ 61, 79, 0x5A, 0x5A, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
	/* > 80mW */
	{ 80, 999, 0x66, 0x66, 0x0, 0x2000, 0x6442, 0x2200, 0x6644, 0x2200, 0x6644, 0x4422, 0x8866, 0x4422, 0x8866, 0x0, 0x0, 0x0, 0x0},
#endif	/* !RTCONFIG_RALINK */
	{ 0, 0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

static const struct txpower_s txpower_list_rtn12hp_b1[] = {
#if !defined(RTCONFIG_RALINK)
	/* 1-19mW */
	{ 1, 19, 0x3C, 0x3C, 0x0, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x4422, 0x0000, 0x4422, 0x0, 0x0, 0x0, 0x0},
	/* 20-39mW */
	{ 20, 39, 0x42, 0x42, 0x0, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x4422, 0x0000, 0x4422, 0x0, 0x0, 0x0, 0x0},
	/* 40-59mW */
	{ 40, 59, 0x4A, 0x4A, 0x0, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x4422, 0x0000, 0x4422, 0x0, 0x0, 0x0, 0x0},
	/* 60-79mW */
	{ 60, 79, 0x52, 0x52, 0x0, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x4422, 0x0000, 0x4422, 0x0, 0x0, 0x0, 0x0},
	/* > 80mW */
	{ 80, 999, 0x5E, 0x5E, 0x0, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x2200, 0x0000, 0x4422, 0x0000, 0x4422, 0x0, 0x0, 0x0, 0x0},
#endif  /* !RTCONFIG_RALINK */
	{ 0, 0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

static const struct txpower_ac_s txpower_list_rtac87u[] = {
#if !defined(RTCONFIG_RALINK)
	/* 1 ~ 25% */
	{ 1, 25, 70, 70, 70},
	/* 26 ~ 50% */
	{ 26, 50, 82, 82, 82},
	/* 51 ~ 75% */
	{ 51, 75, 94, 94, 94},
	/* 76 ~ 100% */
	{ 76, 100, 106, 106, 106},
#endif	/* !RTCONFIG_RALINK */
	{ 0, 0, 0x0, 0x0, 0x0, 0x0}
};

int setpoweroffset_rtn12hp(uint8 level, char *prefix2)
{
	char tmp[100], tmp2[100];
	const struct txpower_s *p;
	int model;
	model = get_model();
	dbG("[rc] setpoweroffset_rtn12hp, level[%d]\n", level);

	if(model == MODEL_RTN12HP_B1)
		p = &txpower_list_rtn12hp_b1[level];
	else
		p = &txpower_list_rtn12hp[level];

	sprintf(tmp2, "0x%02X", p->maxp2ga0);
	nvram_set(strcat_r(prefix2, "maxp2ga0", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%02X", p->maxp2ga1);
	nvram_set(strcat_r(prefix2, "maxp2ga1", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", p->cck2gpo);
	nvram_set(strcat_r(prefix2, "cck2gpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X%04X", p->ofdm2gpo1,p->ofdm2gpo0);
	nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo0);
	nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo1);
	nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo2);
	nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo3);
	nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo4);
	nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo5);
	nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo6);
	nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%04X", p->mcs2gpo7);
	nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", p->cdd2gpo);
	nvram_set(strcat_r(prefix2, "cddpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", p->stbc2gpo);
	nvram_set(strcat_r(prefix2, "stbcpo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", p->bw402gpo);
	nvram_set(strcat_r(prefix2, "bw40po", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%01X", p->bwdup2gpo);
	nvram_set(strcat_r(prefix2, "bwduppo", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	return 1;
}

int setpoweroffset_rtac87u(uint8 level, char *prefix2)
{
	char tmp[100], tmp2[100];
	const struct txpower_ac_s *p;

	dbG("[%d][%s]\n", level, prefix2);

	p = &txpower_list_rtac87u[level];

	sprintf(tmp2, "0x%02X", p->maxp2ga0);
	nvram_set(strcat_r(prefix2, "maxp2ga0", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%02X", p->maxp2ga1);
	nvram_set(strcat_r(prefix2, "maxp2ga1", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	sprintf(tmp2, "0x%02X", p->maxp2ga2);
	nvram_set(strcat_r(prefix2, "maxp2ga2", tmp), tmp2);
	dbG("[rc] [%s]=[%s]\n", tmp,tmp2);

	return 1;
}

void init_devs(void)
{
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
	/* don't do this to save ports */
	//else if (nvram_get_int("sw_mode") == SW_MODE_REPEATER ||
	//        ((nvram_get_int("sw_mode") == SW_MODE_AP) && (nvram_get_int("wlc_psta") == 1)))
	//	cfg = SWCFG_PSTA;
	else if (nvram_get_int("sw_mode") == SW_MODE_AP)
		cfg = SWCFG_BRIDGE;
	else
		cfg = SWCFG_DEFAULT;	// keep wan port, but get ip from bridge

	switch(model) {
		/* BCM5325 series */
		case MODEL_APN12HP:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			/* TODO: switch_wantag? */

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
			//if (!is_routing_enabled())
			//	nvram_set("lan_ifnames", "eth0 eth1");	// override
			switch_gen_config(lan, ports, SWCFG_BRIDGE, 0, "*");
			switch_gen_config(wan, ports, SWCFG_BRIDGE, 1, "u");
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

		/* BCM5325 series */
		case MODEL_RTN14UHP:
		{
			/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 0, 1, 2, 3, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan1ports");
			nvram_unset("vlan1hwname");
			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
			// The first WAN port.
			if (get_wans_dualwan()&WANSCAP_WAN) {
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan1ports", wan);
				nvram_set("vlan1hwname", "et0");
			}

			// The second WAN port.
			if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
				wan1cfg += WAN1PORT1-1;
				if (wancfg != SWCFG_DEFAULT) {
					gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
					nvram_set("vlan0ports", lan);
					gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
					nvram_set("lanports", lan);
				}
				else{
					switch_gen_config(lan, ports, wan1cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, wan1cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
				if(get_wans_dualwan()&WANSCAP_WAN){
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}
				else{
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan0ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					switch_gen_config(wan, ports, wan1cfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else
					nvram_unset(nvram_ports);
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			wancfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		/* BCM5325 series */
		case MODEL_RTN53:
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan1ports");
				nvram_unset("vlan1hwname");
				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan0ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else{
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan0ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
					else{
						nvram_set("vlan1ports", wan);
						nvram_set("vlan1hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan0ports", lan);
				nvram_set("vlan1ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			cfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTN10U:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan1ports");
				nvram_unset("vlan1hwname");
				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan0ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else{
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan0ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
					else{
						nvram_set("vlan1ports", wan);
						nvram_set("vlan1hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan0ports", lan);
				nvram_set("vlan1ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		/* BCM53125 series */
		case MODEL_RTN15U:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");
			nvram_unset("vlan3ports");
			nvram_unset("vlan3hwname");

			// The first WAN port.
			if (get_wans_dualwan()&WANSCAP_WAN) {
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan2ports", wan);
				nvram_set("vlan2hwname", "et0");
			}

			// The second WAN port.
			if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
				wan1cfg += WAN1PORT1-1;
				if (wancfg != SWCFG_DEFAULT) {
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
				if(get_wans_dualwan()&WANSCAP_WAN){
					nvram_set("vlan3ports", wan);
					nvram_set("vlan3hwname", "et0");
				}
				else{
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan1ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
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

		case MODEL_RTN16:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else{
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
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

		case MODEL_RPAC68U:						/* 0  1  2  3  4 */
		case MODEL_RTAC68U:						/* 0  1  2  3  4 */
		case MODEL_RTAC3200:
		case MODEL_RTN18U:						/* 0  1  2  3  4 */
		case MODEL_RTAC53U:
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L1 L2 L3 L4 */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else{
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
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

		case MODEL_DSLAC68U:
		{
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN || get_wans_dualwan()&WANSCAP_DSL) {	//tmp
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"t");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "t");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTAC87U:						/* 0  1  2  3  4 */
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L1 L2 L3 L4 */
			const int ports[SWPORT_COUNT] = { 0, 5, 3, 2, 1, 7 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et1");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et1");
					}
					else{
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et1");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
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

		case MODEL_RTAC56S:						/* 0  1  2  3  4 */
		case MODEL_RTAC56U:
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: L1 L2 L3 L4 WAN  POW*/
			const int ports[SWPORT_COUNT] = { 4, 0, 1, 2, 3, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else{
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
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
		{				/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if(cfg != SWCFG_BRIDGE){
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
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
					if(get_wans_dualwan()&WANSCAP_WAN){
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else{
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				}
				else{
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
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
	}
}

void enable_jumbo_frame(void)
{
	int enable = nvram_get_int("jumbo_frame_enable");

	if (!nvram_contains_word("rc_support", "switchctrl"))
		return;

	switch (get_switch()) {
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
		eval("et", "robowr", "0x40", "0x01", enable ? "0x1f" : "0x00");
		break;
	case SWITCH_BCM5301x:
		eval("et", "robowr", "0x40", "0x01", enable ? "0x010001ff" : "0x00");
		break;
	}
}

void ether_led()
{
	int model;

	model = get_model();
	switch(model) {
//	case MODEL_RTAC68U:
	/* refer to 5301x datasheet page 2770 */
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
		eval("et", "robowr", "0x00", "0x10", "0x3000");
		break;
	case MODEL_RTN16:
		eval("et", "robowr", "0", "0x18", "0x01ff");
		eval("et", "robowr", "0", "0x1a", "0x01ff");
		break;
	}
}

void init_switch()
{
	generate_switch_para();

#ifdef CONFIG_BCMWL5
	// ctf should be disabled when some functions are enabled
	if (nvram_get_int("cstats_enable") || (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0) || nvram_get_int("ctf_disable_force")
	|| nvram_get_int("sw_mode") == SW_MODE_REPEATER
//#ifdef RTCONFIG_USB_MODEM
//	|| nvram_get_int("ctf_disable_modem")
//#endif
	) {
		nvram_set("ctf_disable", "1");
//		nvram_set("pktc_disable", "1");
	}
#ifdef RTCONFIG_BWDPI
	else if (check_bwdpi_nvram_setting() && nvram_get_int("sw_mode") == SW_MODE_ROUTER) {
		nvram_set("ctf_disable", "0");
	}
#endif
	else {
		nvram_set("ctf_disable", "0");
//		nvram_set("pktc_disable", "0");
	}
#ifdef RTCONFIG_BCMFA
	fa_nvram_adjust();
#endif
#ifdef RTCONFIG_GMAC3
	/* Ajuest GMAC3 NVRAM variables */
	gmac3_nvram_adjust();
#endif
	// ctf must be loaded prior to any other modules
	if (nvram_get_int("ctf_disable") == 0)
		modprobe("ctf");

/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
	if (nvram_get_int("sw_mode") == SW_MODE_ROUTER && nvram_get_int("qos_enable") == 1) {
		// enable netfilter bridge only when phydev is used
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-filter-vlan-tagged", "1", 0, 0);
	}
*/
#endif

#ifdef RTCONFIG_SHP
	if (nvram_get_int("qos_enable") == 1 || nvram_get_int("macfilter_enable_x") || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_disable", "1");
	}
	else {
		nvram_set("lfp_disable", "0");
	}

	if (nvram_get_int("lfp_disable")==0) {
		restart_lfp();
	}
#endif
	modprobe("et");
	modprobe("bcm57xx");
	enable_jumbo_frame();
	ether_led();

#ifdef RTCONFIG_DSL
	init_switch_dsl();
	config_switch_dsl();
#endif
}

int
switch_exist(void)
{
	int ret = 1;

	if (get_switch() == SWITCH_UNKNOWN) {
		_dprintf("No switch interface!!!\n");
		ret = 0;
	}

	return ret;
}

void config_switch(void)
{
	generate_switch_para();

}

#ifdef RTCONFIG_BCMWL6
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
#endif

static void
reset_mssid_hwaddr(int unit)
{
	char word[256], *next;
	char macaddr_str[18], macbuf[13];
	char *macaddr_strp;
	unsigned char mac_binary[6];
	unsigned long long macvalue;
	unsigned char *macp;
	int model = get_model();
	int unit_total = 0, idx, subunit;
	int max_mssid = num_of_mssid_support(unit);
	char tmp[100], prefix[]="wlXXXXXXX_";

	foreach(word, nvram_safe_get("wl_ifnames"), next)
		unit_total++;

	if (unit > (unit_total - 1))
		return;

	for (idx = 0; idx < unit_total ; idx++) {
		memset(mac_binary, 0x0, 6);
		memset(macbuf, 0x0, 13);

		switch(model) {
			case MODEL_RTN53:
			case MODEL_RTN16:
			case MODEL_RTN15U:
			case MODEL_RTN12:
			case MODEL_RTN12B1:
			case MODEL_RTN12C1:
			case MODEL_RTN12D1:
			case MODEL_RTN12VP:
			case MODEL_RTN12HP:
			case MODEL_RTN12HP_B1:
			case MODEL_APN12HP:
			case MODEL_RTN14UHP:
			case MODEL_RTN10U:
			case MODEL_RTN10P:
			case MODEL_RTN10D1:
			case MODEL_RTN10PV2:
			case MODEL_RTAC53U:
				if (unit == 0)	/* 2.4G */
					snprintf(macaddr_str, sizeof(macaddr_str), "sb/1/macaddr");
				else		/* 5G */
					snprintf(macaddr_str, sizeof(macaddr_str), "0:macaddr");
				break;
			case MODEL_RTN66U:
			case MODEL_RTAC66U:
				snprintf(macaddr_str, sizeof(macaddr_str), "pci/%d/1/macaddr", unit + 1);
				break;
			case MODEL_RTN18U:
			case MODEL_RPAC68U:
			case MODEL_RTAC68U:
			case MODEL_RTAC3200:
			case MODEL_DSLAC68U:
			case MODEL_RTAC87U:
			case MODEL_RTAC56S:
			case MODEL_RTAC56U:
				snprintf(macaddr_str, sizeof(macaddr_str), "%d:macaddr", unit);
				break;
			default:
#ifdef RTCONFIG_BCMARM
				snprintf(macaddr_str, sizeof(macaddr_str), "%d:macaddr", unit);
#else
				snprintf(macaddr_str, sizeof(macaddr_str), "pci/%d/1/macaddr", unit + 1);
#endif
				break;
		}

		macaddr_strp = nvram_get(macaddr_str);
		if (macaddr_strp)
		{
			if (!mssid_mac_validate(macaddr_strp))
				return;

			if (idx != unit)
				continue;

			ether_atoe(macaddr_strp, mac_binary);
			sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
					mac_binary[0],
					mac_binary[1],
					mac_binary[2],
					mac_binary[3],
					mac_binary[4],
					mac_binary[5]);
			macvalue = strtoll(macbuf, (char **) NULL, 16);

			/* including primary ssid */
			for (subunit = 1; subunit < max_mssid + 1 ; subunit++)
			{
				macvalue++;

				macp = (unsigned char*) &macvalue;
				memset(macaddr_str, 0, sizeof(macaddr_str));
				sprintf(macaddr_str, "%02X:%02X:%02X:%02X:%02X:%02X", *(macp+5), *(macp+4), *(macp+3), *(macp+2), *(macp+1), *(macp+0));
				snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
				nvram_set(strcat_r(prefix, "hwaddr", tmp), macaddr_str);
			}
		} else return;
	}
}

#ifdef RTCONFIG_BCM7
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

static int first_load = 1;

void load_wl_war()
{
	first_load = 0;

	eval("insmod", "wl");

	while (!net_dev_exist("eth1"))
		sleep(1);

	ifconfig("eth1", IFUP, NULL, NULL);
	ifconfig("eth2", IFUP, NULL, NULL);
	ifconfig("eth3", IFUP, NULL, NULL);

	eval("rmmod", "wl");
}

void load_wl()
{
	char module[80], *modules, *next;
#ifdef RTCONFIG_NOWL
	modules = "dhd";
#else
#ifdef RTCONFIG_DHDAP
	modules = "dpsta dhd wl";
#else
	modules = "dpsta wl";
#endif
#endif
	int i = 0, maxwl_eth = 0, maxunit = -1;
	int unit = -1;
	char ifname[16] = {0};
	char instance_base[128];
#ifndef RTCONFIG_NOWL
#ifndef RTCONFIG_DPSTA
	if (first_load) load_wl_war();
#endif
#endif
	foreach(module, modules, next) {
		if ((strcmp(module, "dhd") == 0) || (strcmp(module, "wl") == 0)) {
			/* Search for existing wl devices and the max unit number used */
			for (i = 1; i <= DEV_NUMIFS; i++) {
			snprintf(ifname, sizeof(ifname), "eth%d", i);
				if (!wl_probe(ifname)) {
					if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit,
					sizeof(unit))) {
						maxwl_eth = i;
						maxunit = (unit > maxunit) ? unit : maxunit;
					}
				}
			}
			snprintf(instance_base, sizeof(instance_base), "instance_base=%d", maxunit + 1);
#ifdef RTCONFIG_BCM7
			snprintf(instance_base, sizeof(instance_base), "%s dhd_msg_level=%d", instance_base, nvram_get_int("dhd_msg_level"));
#endif
			eval("insmod", module, instance_base);
		}
		else {
			eval("insmod", module);
#ifndef RTCONFIG_NOWL
#ifdef RTCONFIG_DPSTA
			if (first_load) load_wl_war();
#endif
#endif
		}
	}
}
#endif

void init_wl(void)
{
#ifdef RTCONFIG_EMF
	modprobe("emf");
	modprobe("igs");
#endif
#ifdef RTCONFIG_BCMWL6
	switch(get_model()) {
		case MODEL_RTAC3200:
		case MODEL_DSLAC68U:
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
		case MODEL_RTAC66U:
			set_bcm4360ac_vars();
			break;
	}
#endif
#ifdef RTCONFIG_BCM7
	load_wl();
#else
	modprobe("wl");
#endif
#ifndef RTCONFIG_BCMARM
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
	modprobe("proxyarp");
#endif
#endif

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

void fini_wl(void)
{
#ifndef RTCONFIG_BCMARM
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
	modprobe_r("proxyarp");
#endif
#endif

#ifndef RTCONFIG_BRCM_USBAP
	if ((get_model() == MODEL_RTAC3200) ||
		(get_model() == MODEL_RPAC68U) ||
		(get_model() == MODEL_RTAC68U) ||
		(get_model() == MODEL_DSLAC68U) ||
		(get_model() == MODEL_RTAC87U) ||
		(get_model() == MODEL_RTAC66U) ||
		(get_model() == MODEL_RTN66U) ||
		(get_model() == MODEL_RTN18U))
	eval("rmmod","wl");
#endif
#ifdef RTCONFIG_BCM7
#ifdef RTCONFIG_DHDAP
	eval("rmmod","dhd");
#endif
#endif
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
	nvram_set("swpjverno", rt_swpjverno);
	ptr = nvram_get("regulation_domain");

	model = get_model();
	switch(model) {
		case MODEL_RTN53:
		case MODEL_RTN16:
		case MODEL_RTN15U:
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		case MODEL_APN12HP:
		case MODEL_RTN14UHP:
		case MODEL_RTN10U:
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
			if (!nvram_get("et0macaddr"))	// eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("0:macaddr"))	// eth2(5G)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (nvram_get("regulation_domain_5G")) {// by ate command from asuswrt, prior than ui 2.0
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("0:ccode", nvram_get("regulation_domain_5G"));
			} else if (nvram_get("regulation_domain_5g")) {	// by ate command from ui 2.0
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
			if (!nvram_get("et0macaddr"))		// eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("pci/2/1/macaddr"))	// eth2(5G)
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
			break;

		case MODEL_RTN18U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			nvram_set("0:macaddr", nvram_safe_get("et0macaddr"));

			if (nvram_match("0:ccode", "0")) {
				nvram_set("0:ccode","US");
			}
			break;

		case MODEL_RTAC87U:
			if (!nvram_get("et1macaddr"))	//eth0, eth1
				nvram_set("et1macaddr", "00:22:15:A5:03:00");
			nvram_set("0:macaddr", nvram_safe_get("et1macaddr"));
			nvram_set("LANMACADDR", nvram_safe_get("et1macaddr"));
			break;

		case MODEL_DSLAC68U:
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("1:macaddr"))	//eth2(5G)
				nvram_set("1:macaddr", "00:22:15:A5:03:04");
			nvram_set("0:macaddr", nvram_safe_get("et0macaddr"));
			break;

		case MODEL_RTAC3200:
			if (!nvram_get("et0macaddr"))				// eth0 (ethernet)
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			nvram_set("1:macaddr", nvram_safe_get("et0macaddr"));	// eth2 (2.4GHz)
			if (!nvram_get("0:macaddr"))				// eth1(5GHz)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (!nvram_get("2:macaddr"))				// eth3(5GHz)
				nvram_set("2:macaddr", "00:22:15:A5:03:08");
			break;

		case MODEL_RTAC53U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("0:macaddr"))	//eth2(5G)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			nvram_set("sb/1/macaddr", nvram_safe_get("et0macaddr"));
			break;

		default:
#ifdef RTCONFIG_RGMII_BRCM5301X
			if (!nvram_get("lan_hwaddr"))
				nvram_set("lan_hwaddr", "00:22:15:A5:03:00");
#else
			if (!nvram_get("et0macaddr"))
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
#endif
			break;
	}

#ifdef RTCONFIG_ODMPID
	if (nvram_match("odmpid", "ASUS") ||
		!is_valid_hostname(nvram_safe_get("odmpid")))
		nvram_set("odmpid", "");
#endif

	if (nvram_get("secret_code"))
		nvram_set("wps_device_pin", nvram_get("secret_code"));
	else
		nvram_set("wps_device_pin", "12345670");
}
#ifdef RTCONFIG_BCMARM
#define ASUS_TWEAK
void init_others(void)
{
	int model = get_model();

	if (model == MODEL_RTAC56U || model == MODEL_RTAC56S || model == MODEL_RTAC3200 || model == MODEL_RTAC68U || model == MODEL_RPAC68U || model == MODEL_RTAC87U || model == MODEL_RTN18U) {
#ifdef SMP
		int fd;

		if ((fd = open("/proc/irq/163/smp_affinity", O_RDWR)) >= 0) {
			close(fd);
#ifdef RTCONFIG_GMAC3
			if (nvram_match("gmac3_enable", "1")) {
				char *fwd_cpumap;

				/* Place network interface vlan1/eth0 on CPU hosting 5G upper */
				fwd_cpumap = nvram_get("fwd_cpumap");

				if (fwd_cpumap == NULL) {
					/* BCM4709acdcrh: Network interface GMAC on Core#1(if !usb3)
					 * [5G+2G:163 on Core#0] and [5G:169 on Core#1].
					 * Bind et2:vlan1:eth0:181 to Core#1(if !usb3)
					 * USB3 xhci_hcd's irq#112 binds Core#1
					 * bind eth0:181 to Core#1 impacts USB3 performance
					 */
					if (nvram_get_int("usb_usb3") == 1)
						system("echo 1 > /proc/irq/181/smp_affinity");
					else
						system("echo 2 > /proc/irq/181/smp_affinity");

				} else {

					char cpumap[32], *next;

					foreach(cpumap, fwd_cpumap, next) {
						char mode, chan;
						int band, irq, cpu;

						/* Format: mode:chan:band#:irq#:cpu# */
						if (sscanf(cpumap, "%c:%c:%d:%d:%d",
							&mode, &chan, &band, &irq, &cpu) != 5) {
							break;
						}
						if (cpu > 1) {
							break;
						}
						/* Find the single 5G upper */
						if ((chan == 'u') || (chan == 'U')) {
							char command[128];
							snprintf(command, sizeof(command),
							"echo %d > /proc/irq/181/smp_affinity",
							1 << cpu);
							system(command);
							break;
						}
					}
				}
			} else
#endif
#ifdef ASUS_TWEAK
			if (nvram_match("enable_samba", "0")) {  // not set txworkq
#else
			if (!nvram_match("txworkq", "1")) {
#endif
				system("echo 2 > /proc/irq/163/smp_affinity");
				system("echo 2 > /proc/irq/169/smp_affinity");
			}
#ifdef ASUS_TWEAK
			system("echo 2 > /proc/irq/111/smp_affinity");
#endif
			system("echo 2 > /proc/irq/112/smp_affinity");
		}
#endif
#ifdef ASUS_TWEAK
		nvram_set("txworkq", "1");
#endif
	}
#ifdef RTAC68U
	update_cfe();
#endif
#ifdef RTAC3200
	if (nvram_match("bl_version", "1.0.1.3")) {
		system("nvram set asuscfe0:mcsbw205glpo=0x66644200");
		system("nvram set asuscfe0:mcsbw405glpo=0x66643200");
		system("nvram set asuscfebl_version=1.0.1.5");
		system("nvram set asuscfecommit=1");
		nvram_set("0:mcsbw205glpo", "0x66644200");
		nvram_set("0:mcsbw405glpo", "0x66643200");
		nvram_set("bl_version", "1.0.1.5");
		nvram_commit();
	}
#endif
}
#endif
void chanspec_fix_5g(int unit)
{
	char tmp[100], prefix[]="wlXXXXXXX_";
	int channel;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	channel = nvram_get_int(strcat_r(prefix, "channel", tmp));

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
//	char *chanspec_5g_20m[] = {"0", "36", "40", "44", "48", "56", "60", "64", "149", "153", "157", "161", "165"};
	char *chanspec_5g_20m_xx[] = {"0", "36", "40", "44", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "120", "124", "128", "132", "136", "140", "144", "149", "153", "157", "161", "165"};
//	char *chanspec_5g_40m[] = {"0", "40u", "48u", "64u", "153u", "161u", "36l", "44l", "60l", "149l", "157l"};
	char *chanspec_5g_40m_xx[] = {"0", "40u", "48u", "56u", "64u", "104u", "112u", "120u", "128u", "136u", "144u", "153u", "161u", "36l", "44l", "52l", "60l", "100l", "108l", "116l", "124l", "132l", "140l", "149l", "157l"};
//	char *chanspec_5g_80m[] = {"0", "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80"};
	char *chanspec_5g_80m_xx[] = {"0", "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116/80", "120/80", "124/80", "128/80", "132/80", "136/80", "140/80", "144/80", "149/80", "153/80", "157/80", "161/80"};
//	char *chanspec_20m[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"};
	char *chanspec_20m_xx[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};
	char *chanspec_40m[] = {"0", "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u"};

	char tmp[100], prefix[]="wlXXXXXXX_";
	int i;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	dbG("unit: %d, bw_cap: %s, chanspec: %s\n", unit, nvram_safe_get(strcat_r(prefix, "bw_cap", tmp)), nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));

//	if (nvram_match(strcat_r(prefix, "country_code", tmp), "XX"))	// worldwide
	{
		if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
		{
			if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "3"))
				goto BAND_2G_BW_40M_XX;
			else
				goto BAND_2G_BW_20M_XX;
BAND_2G_BW_40M_XX:
			for (i = 0; i < (sizeof(chanspec_40m)/sizeof(chanspec_40m[0])); i++)
			{
				if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_40m[i])) return;
			}
BAND_2G_BW_20M_XX:
			for (i = 0; i < (sizeof(chanspec_20m_xx)/sizeof(chanspec_20m_xx[0])); i++)
			{
				if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_20m_xx[i])) return;
			}
		}
		else
		{
			if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "7"))
				goto BAND_5G_BW_80M_XX;
			else if (nvram_match(strcat_r(prefix, "bw_cap", tmp), "3"))
				goto BAND_5G_BW_40M_XX;
			else
				goto BAND_5G_BW_20M_XX;
BAND_5G_BW_80M_XX:
			for (i = 0; i < (sizeof(chanspec_5g_80m_xx)/sizeof(chanspec_5g_80m_xx[0])); i++)
			{
				if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_80m_xx[i])) return;
			}
BAND_5G_BW_40M_XX:
			for (i = 0; i < (sizeof(chanspec_5g_40m_xx)/sizeof(chanspec_5g_40m_xx[0])); i++)
			{
				if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_40m_xx[i])) return;
			}
BAND_5G_BW_20M_XX:
			for (i = 0; i < (sizeof(chanspec_5g_20m_xx)/sizeof(chanspec_5g_20m_xx[0])); i++)
			{
				if (nvram_match(strcat_r(prefix, "chanspec", tmp), chanspec_5g_20m_xx[i])) return;
			}
		}
	}
#if 0
	else
	{
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
	}
#endif
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
	int model;

#if 0	/* move to init.c */
	if (nvram_match(strcat_r(prefix, "country_code", tmp), "US"))
	{
		if (nvram_match(strcat_r(prefix, "country_rev", tmp), "37"))
		{
			nvram_set(strcat_r(prefix2, "regrev", tmp2), "16");
			commit_needed++;
		}
	}
#endif

#if 1	/* TMP, RT-N12HP */
	/* config power offset */
	level = 0;
	model = get_model();
	if(model == MODEL_RTN12HP_B1)	p = &txpower_list_rtn12hp_b1[0];
	else				p = &txpower_list_rtn12hp[0];

	for (p; p->min != 0; ++p) {
		if (txpower >= p->min && txpower <= p->max) {
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

int wltxpower_rtac87u(int txpower, char *prefix2)
{
	int commit_needed = 0;
	int level;
	const struct txpower_ac_s *p;

	dbG("[%d][%s]\n", txpower, prefix2);
#if 1	/* TMP, RT-AC87U */
	/* config power offset */
	level = 0;

	p = &txpower_list_rtac87u[0];
	for (p; p->min != 0; ++p) {
		if (txpower >= p->min && txpower <= p->max) {
			dbG("txpoewr between: min:[%d] to max:[%d]\n",
									p->min, p->max);
			/* prefix2 is 0: */
			setpoweroffset_rtac87u(level, prefix2);
			break;
		}
		level++;
	}

	if ( p->min == 0 )
		dbG("no correct power offset!\n");

	commit_needed = 1;
#endif

	return commit_needed;
}

#define TXPWR_THRESHOLD_1	25
#define TXPWR_THRESHOLD_2	50
#define TXPWR_THRESHOLD_3	88
#define TXPWR_THRESHOLD_4	100

int set_wltxpower()
{
	char ifnames[256];
	char name[64], ifname[64], *next = NULL;
	int unit = -1, subunit = -1;
	int i;
	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="pci/x/1/";
	int txpower = 100;
	int commit_needed = 0;
	int model;

	// generate nvram nvram according to system setting
	model = get_model();

	if (!nvram_contains_word("rc_support", "pwrctrl")) {
		dbG("[rc] no Power Control on this model\n");
		return -1;
	}

	if ((model != MODEL_RTAC66U)
		&& (model != MODEL_RTN66U)
		&& (model != MODEL_RTN12HP)
		&& (model != MODEL_RTN12HP_B1)
		&& (model != MODEL_APN12HP)
		&& (model != MODEL_RTAC56S)
		&& (model != MODEL_RTAC56U)
		&& (model != MODEL_DSLAC68U)
		&& (model != MODEL_RTAC87U)
		&& (model != MODEL_RPAC68U)
		&& (model != MODEL_RTAC68U)
		&& (model != MODEL_RTAC3200)
		&& (model != MODEL_RTN18U))
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
			case MODEL_RTN53:
			case MODEL_RTN16:
			case MODEL_RTN15U:
			case MODEL_RTN12:
			case MODEL_RTN12B1:
			case MODEL_RTN12C1:
			case MODEL_RTN12D1:
			case MODEL_RTN12VP:
			case MODEL_RTN12HP:
			case MODEL_RTN12HP_B1:
			case MODEL_APN12HP:
			case MODEL_RTN14UHP:
			case MODEL_RTN10U:
			case MODEL_RTN10P:
			case MODEL_RTN10D1:
			case MODEL_RTN10PV2:
			case MODEL_RTAC53U:
				if (unit == 0)	/* 2.4G */
					snprintf(prefix2, sizeof(prefix2), "sb/1/");
				else		/* 5G */
					snprintf(prefix2, sizeof(prefix2), "0:");
				break;

			case MODEL_RTN66U:
			case MODEL_RTAC66U:
				snprintf(prefix2, sizeof(prefix2), "pci/%d/1/", unit + 1);
				break;

			case MODEL_RTN18U:
			case MODEL_RPAC68U:
			case MODEL_RTAC68U:
			case MODEL_DSLAC68U:
			case MODEL_RTAC87U:
			case MODEL_RTAC56S:
			case MODEL_RTAC56U:
				snprintf(prefix2, sizeof(prefix2), "%d:", unit);
				break;

			case MODEL_RTAC3200:
				if (unit < 2)
					snprintf(prefix2, sizeof(prefix2), "%d:", 1 - unit);
				else
					snprintf(prefix2, sizeof(prefix2), "%d:", unit);
				break;
		}

		if (strcmp(nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)),
			nvram_safe_get(strcat_r(prefix, "country_rev", tmp))))
		{
			nvram_set(strcat_r(prefix, "country_rev", tmp),
				nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)));
			commit_needed++;
		}

		txpower = nvram_get_int(wl_nvname("txpower", unit, 0));
		dbG("unit: %d, txpower: %d\n", unit, txpower);

		switch(model) {
			case MODEL_RTAC66U:
				if (set_wltxpower_once) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (txpower < TXPWR_THRESHOLD_1)
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
						else if (txpower < TXPWR_THRESHOLD_2)
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
						else if (txpower < TXPWR_THRESHOLD_3)
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
						else if (txpower < TXPWR_THRESHOLD_4)
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
						else	// txpower == 80 mw
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
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "52,52,52,52"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"52,52,52,52");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"52,52,52,52");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"52,52,52,52");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x33333333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x33333333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "64,64,64,64"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"64,64,64,64");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"64,64,64,64");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"64,64,64,64");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99975333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "76,76,76,76"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"76,76,76,76");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"76,76,76,76");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"76,76,76,76");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99975333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "88,88,88,88"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"88,88,88,88");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"88,88,88,88");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"88,88,88,88");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99975333");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "100,100,100,100"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"100,100,100,100");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"100,100,100,100");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"100,100,100,100");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99975333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99975333");
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

			case MODEL_RTN18U:

				if (set_wltxpower_once) {
					if (txpower < TXPWR_THRESHOLD_1)
					{
						if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "58"))
						{
							nvram_set(strcat_r(prefix2,"maxp2ga0", tmp2), "58");
							nvram_set(strcat_r(prefix2,"maxp2ga1", tmp2), "58");
							nvram_set(strcat_r(prefix2,"maxp2ga2", tmp2), "58");
							nvram_set(strcat_r(prefix2,"mcsbw202gpo", tmp2), "0x66642000");
							nvram_set(strcat_r(prefix2,"mcsbw402gpo", tmp2), "0x66642000");
							nvram_set(strcat_r(prefix2,"dot11agofdmhrbw202gpo", tmp2), "0x6533");
							commit_needed++;
						}
					}
					else if (txpower < TXPWR_THRESHOLD_2)
					{
						if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "70"))
						{
							nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), "70");
							nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), "70");
							nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), "70");
							nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2), "0x6533");
							commit_needed++;
						}
					}
					else if (txpower < TXPWR_THRESHOLD_3)
					{
						if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "82"))
						{
							nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), "82");
							nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), "82");
							nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), "82");
							nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2), "0x6533");
							commit_needed++;
						}
					}
					else if (txpower < TXPWR_THRESHOLD_4)
					{
						if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "94"))
						{
							nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), "94");
							nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), "94");
							nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), "94");
							nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2), "0x6533");
							commit_needed++;
						}
					}
					else	// txpower = 100%
					{
						if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "106"))
						{
							nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2), "106");
							nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2), "106");
							nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2), "106");
							nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2), "0xA8642000");
							nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2), "0x6533");
							commit_needed++;
						}
					}
				}
				break;

			case MODEL_DSLAC68U:
				if (set_wltxpower_once) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G, same with RT-AC68U
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x66653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "70"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "82"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "94"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "106"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "58,58,58,58"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "70,70,70,70"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "82,82,82,82"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,94,94,94"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "106,106,106,106"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
					}
				}
				break;

			case MODEL_RTAC3200:
				if (set_wltxpower_once) {
					if (unit == 0)	// 2.4G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x66420000");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x66420000");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x2000");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "70"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x2000");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "82"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x2000");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "94"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x2000");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "106"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x86420000");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x2000");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
					}
					else if (unit == 1)	// 5G low
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "58,58,58,58"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66664200");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "70,70,70,70"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66664200");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xA8643200");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "82,82,82,82"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66664200");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xA8643200");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,90,90,94"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,90,90,94");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,90,90,94");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,90,90,94");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66664200");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xA8643200");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,90,90,106"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,90,90,106");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,90,90,106");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,90,90,106");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66664200");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66643200");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xA8643200");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
					}
					else if (unit == 2)	// 5G high
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "58,58,58,58"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "70,70,70,70"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "82,82,82,82"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,94,94,94"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "90,90,90,106"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"90,90,90,106");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"90,90,90,106");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"90,90,90,106");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xfffda844");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xA6542100");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
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
					dbG("mcsbw202gpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw202gpo", tmp2)) ? : "NULL");
					dbG("mcsbw402gpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw402gpo", tmp2)) ? : "NULL");
					dbG("dot11agofdmhrbw202gpo: %s\n",nvram_get(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2)) ? : "NULL");
					dbG("ofdmlrbw202gpo: %s\n",	nvram_get(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2)) ? : "NULL");
					dbG("dot11agduphrpo: %s\n",	nvram_get(strcat_r(prefix2, "dot11agduphrpo", tmp2)) ? : "NULL");
					dbG("dot11agduplrpo: %s\n",	nvram_get(strcat_r(prefix2, "dot11agduplrpo", tmp2)) ? : "NULL");
				}
				else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
				{
					dbG("maxp5ga0: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga0", tmp2)) ? : "NULL");
					dbG("maxp5ga1: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga1", tmp2)) ? : "NULL");
					dbG("maxp5ga2: %s\n",		nvram_get(strcat_r(prefix2, "maxp5ga2", tmp2)) ? : "NULL");

					dbG("mcsbw205glpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw205glpo", tmp2)) ? : "NULL");
					dbG("mcsbw405glpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw405glpo", tmp2)) ? : "NULL");
					dbG("mcsbw805glpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw805glpo", tmp2)) ? : "NULL");
					dbG("mcsbw1605glpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw1605glpo", tmp2)) ? : "NULL");

					dbG("mcsbw205gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw205gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw405gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw405gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw805gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw805gmpo", tmp2)) ? : "NULL");
					dbG("mcsbw1605gmpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw1605gmpo", tmp2)) ? : "NULL");

					dbG("mcsbw205ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw205ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw405ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw405ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw805ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw805ghpo", tmp2)) ? : "NULL");
					dbG("mcsbw1605ghpo: %s\n",	nvram_get(strcat_r(prefix2, "mcsbw1605ghpo", tmp2)) ? : "NULL");
				}

				dbG("ccode: %s\n", nvram_safe_get(strcat_r(prefix2, "ccode", tmp2)));
				dbG("regrev: %s\n", nvram_safe_get(strcat_r(prefix2, "regrev", tmp2)));
				dbG("country_code: %s\n", nvram_safe_get(strcat_r(prefix, "country_code", tmp)));
				dbG("country_rev: %s\n", nvram_safe_get(strcat_r(prefix, "country_rev", tmp)));
#endif
				break;

			case MODEL_RPAC68U:
			case MODEL_RTAC68U:
				if (set_wltxpower_once) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"58");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x66653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "70"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"70");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "82"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"82");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "94"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"94");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "106"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "maxp2ga2", tmp2),			"106");
								nvram_set(strcat_r(prefix2, "cckbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "cckbw20ul2gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "mcsbw202gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw402gpo", tmp2),		"0x88653320");
								nvram_set(strcat_r(prefix2, "dot11agofdmhrbw202gpo", tmp2),	"0x6533");
								nvram_set(strcat_r(prefix2, "ofdmlrbw202gpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40hrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "sb20in40lrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduphrpo", tmp2),		"0");
								nvram_set(strcat_r(prefix2, "dot11agduplrpo", tmp2),		"0");
								commit_needed++;
							}
						}
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (!(nvram_match(strcat_r(prefix, "country_code", tmp), "Q2") &&
						     nvram_match(strcat_r(prefix, "country_rev", tmp), "40"))) {

						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "58,58,58,58"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x66653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "70,70,70,70"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "82,82,82,82"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,94,94,94"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "106,106,106,106"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x88653320");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}

						} else {

						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!(nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "58,58,58,58") &&
							      nvram_match(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x33333311")))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"58,58,58,58");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x33333311");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!(nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "70,70,70,70") &&
							      nvram_match(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0x99986422")))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"70,70,70,70");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99986422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!(nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "82,82,82,82") &&
							      nvram_match(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0xCAA86422")))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"82,82,82,82");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!(nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "94,94,94,94") &&
							      nvram_match(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0xCAA86422")))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"94,94,94,94");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!(nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "106,106,106,106") &&
							      nvram_match(strcat_r(prefix2, "mcsbw205glpo", tmp2), "0xCAA86422")))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "maxp5ga2", tmp2),		"106,106,106,106");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605glpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605gmpo", tmp2),	"0");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0xCAA86422");
								nvram_set(strcat_r(prefix2, "mcsbw1605ghpo", tmp2),	"0");
								commit_needed++;
							}
						}

						}
					}
				}
				break;

			case MODEL_RTAC56S:
			case MODEL_RTAC56U:
				if (set_wltxpower_once) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x40"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),	"0x40");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),	"0x40");
								nvram_set(strcat_r(prefix2, "cck2gpo",  tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp2),	"0x32000000");
								nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp2),	"0x3332");
								nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp2),	"0x3332");
								nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp2),	"0x3333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x48"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),	"0x48");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),	"0x48");
								nvram_set(strcat_r(prefix2, "cck2gpo",  tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp2),	"0x32000000");
								nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp2),	"0x5332");
								nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp2),	"0x5332");
								nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp2),	"0x7333");
								nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp2),	"0x7333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x50"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),	"0x50");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),	"0x50");
								nvram_set(strcat_r(prefix2, "cck2gpo",  tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp2),	"0x32000000");
								nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp2),	"0x7332");
								nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp2),	"0x7332");
								nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp2),	"0x9333");
								nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp2),	"0x9333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x58"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),	"0x58");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),	"0x58");
								nvram_set(strcat_r(prefix2, "cck2gpo",  tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp2),	"0x32000000");
								nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp2),	"0x9532");
								nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp2),	"0x2222");
								nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp2),	"0x9532");
								nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp2),	"0xB533");
								nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp2),	"0xB533");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp2ga0", tmp2), "0x64"))
							{
								nvram_set(strcat_r(prefix2, "maxp2ga0", tmp2),	"0x64");
								nvram_set(strcat_r(prefix2, "maxp2ga1", tmp2),	"0x64");
								nvram_set(strcat_r(prefix2, "cck2gpo",  tmp2),	"0x1111");
								nvram_set(strcat_r(prefix2, "ofdm2gpo", tmp2),	"0x54222222");
								nvram_set(strcat_r(prefix2, "mcs2gpo0", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo1", tmp2),	"0xD954");
								nvram_set(strcat_r(prefix2, "mcs2gpo2", tmp2),	"0x3333");
								nvram_set(strcat_r(prefix2, "mcs2gpo3", tmp2),	"0xD954");
								nvram_set(strcat_r(prefix2, "mcs2gpo4", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "mcs2gpo5", tmp2),	"0xF955");
								nvram_set(strcat_r(prefix2, "mcs2gpo6", tmp2),	"0x5555");
								nvram_set(strcat_r(prefix2, "mcs2gpo7", tmp2),	"0xF955");
								commit_needed++;
							}
						}
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < TXPWR_THRESHOLD_1)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "68,68,68,68"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"68,68,68,68");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"68,68,68,68");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99753333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_2)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "76,76,76,76"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"76,76,76,76");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"76,76,76,76");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99753333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_3)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "84,84,84,84"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"84,84,84,84");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"84,84,84,84");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99753333");
								commit_needed++;
							}
						}
						else if (txpower < TXPWR_THRESHOLD_4)
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "92,92,92,92"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"92,92,92,92");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"92,92,92,92");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99753333");
								commit_needed++;
							}
						}
						else	// txpower == 80 mw
						{
							if (!nvram_match(strcat_r(prefix2, "maxp5ga0", tmp2), "100,100,100,100"))
							{
								nvram_set(strcat_r(prefix2, "maxp5ga0", tmp2),		"100,100,100,100");
								nvram_set(strcat_r(prefix2, "maxp5ga1", tmp2),		"100,100,100,100");
								nvram_set(strcat_r(prefix2, "mcsbw205glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805glpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805gmpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw205ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw405ghpo", tmp2),	"0x99753333");
								nvram_set(strcat_r(prefix2, "mcsbw805ghpo", tmp2),	"0x99753333");
								commit_needed++;
							}
						}
					}
				}
				break;

			case MODEL_RTN66U:
				if (set_wltxpower_once || nvram_match("bl_version", "1.0.0.9")) {
					if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
					{
						if (txpower < TXPWR_THRESHOLD_1)
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
						else if (txpower < TXPWR_THRESHOLD_2)
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
						else if (txpower < TXPWR_THRESHOLD_3)
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
						else if (txpower < TXPWR_THRESHOLD_4)
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
						else	// txpower == 80 mw
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
					}
					else if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))	// 5G
					{
						if (txpower < TXPWR_THRESHOLD_1)
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
						else if (txpower < TXPWR_THRESHOLD_2)
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
						else if (txpower < TXPWR_THRESHOLD_3)
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
						else if (txpower < TXPWR_THRESHOLD_4)
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
						else	// txpower == 80 mw
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

			case MODEL_RTAC87U:
				commit_needed = wltxpower_rtac87u(txpower, prefix2);
				break;

			case MODEL_RTN12HP:
			case MODEL_RTN12HP_B1:
			case MODEL_APN12HP:

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
	if (nvram_get_int("sw_mode")==SW_MODE_REPEATER) {
		if (nvram_get_int("wlc_band")==unit) return 1;
	}
	return 0;
}
#endif

int wl_max_no_vifs(int unit)
{
	char nv_interface[NVRAM_MAX_PARAM_LEN];
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_SMLEN];
	char *name = NULL;
	char *next = NULL;
	int max_no_vifs = 0;
#ifdef RTCONFIG_QTN
	if (unit == 1)
		return 4;
#endif
	sprintf(nv_interface, "wl%d_ifname", unit);
	name = nvram_safe_get(nv_interface);
	if (!strlen(name))
	{
		sprintf(nv_interface, "eth%d", unit + 1);
		name = nv_interface;
	}

	if (!wl_iovar_get(name, "cap", (void *)caps, WLC_IOCTL_SMLEN)) {
		foreach(cap, caps, next) {
			if (!strcmp(cap, "mbss16"))
				max_no_vifs = 16;
			else if (!strcmp(cap, "mbss4"))
				max_no_vifs = 4;
		}
	}

	return max_no_vifs;
}

void generate_wl_para(int unit, int subunit)
{
	dbG("unit %d subunit %d\n", unit, subunit);

	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="wlXXXXXXX_";
	char *list;
	char *nv, *nvp, *b, *c;
#ifndef RTCONFIG_BCMWL6
	char word[256], *next;
	int match;
#endif
	int max_no_vifs = wl_max_no_vifs(unit), i, mcast_rate;
	char interface_list[NVRAM_MAX_VALUE_LEN];
	int interface_list_size = sizeof(interface_list);
	char nv_interface[NVRAM_MAX_PARAM_LEN];
#ifdef RTCONFIG_PROXYSTA
	char os_interface[NVRAM_MAX_PARAM_LEN];
	char lan_ifnames[NVRAM_MAX_PARAM_LEN] = "lan_ifnames";
	bool psta=0, psr=0, db_rpt=0;
#endif

	if (subunit == -1)
	{
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (nvram_match("wps_enable", "1") &&
			((unit == nvram_get_int("wps_band") || nvram_match("w_Setting", "0"))))
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "enabled");
		else
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");

#ifdef BCM_BSD
		if (((unit == 0) && nvram_get_int("smart_connect_x") == 1) ||
		    ((unit == 1) && nvram_get_int("smart_connect_x") == 2)) {
			for (i = unit + 1; i < 3; i++) {
				snprintf(prefix2, sizeof(prefix2), "wl%d_", i);
				nvram_set(strcat_r(prefix2, "ssid", tmp2), nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
				nvram_set(strcat_r(prefix2, "auth_mode_x", tmp2), nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));
				nvram_set(strcat_r(prefix2, "wep_x", tmp2), nvram_safe_get(strcat_r(prefix, "wep_x", tmp)));
				nvram_set(strcat_r(prefix2, "key", tmp2), nvram_safe_get(strcat_r(prefix, "key", tmp)));
				nvram_set(strcat_r(prefix2, "key1", tmp2), nvram_safe_get(strcat_r(prefix, "key1", tmp)));
				nvram_set(strcat_r(prefix2, "key2", tmp2), nvram_safe_get(strcat_r(prefix, "key2", tmp)));
				nvram_set(strcat_r(prefix2, "key3", tmp2), nvram_safe_get(strcat_r(prefix, "key3", tmp)));
				nvram_set(strcat_r(prefix2, "key4", tmp2), nvram_safe_get(strcat_r(prefix, "key4", tmp)));
				nvram_set(strcat_r(prefix2, "phrase_x", tmp2), nvram_safe_get(strcat_r(prefix, "phrase_x", tmp)));
				nvram_set(strcat_r(prefix2, "crypto", tmp2), nvram_safe_get(strcat_r(prefix, "crypto", tmp)));
				nvram_set(strcat_r(prefix2, "wpa_psk", tmp2), nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
				nvram_set(strcat_r(prefix2, "radius_ipaddr", tmp2), nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)));
				nvram_set(strcat_r(prefix2, "radius_key", tmp2), nvram_safe_get(strcat_r(prefix, "radius_key", tmp)));
				nvram_set(strcat_r(prefix2, "radius_port", tmp2), nvram_safe_get(strcat_r(prefix, "radius_port", tmp)));
			}
		}

		int acs = 1;
#if 0
		if ((unit == 0) && (nvram_get_int("smart_connect_x") == 1))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else if ((unit == 1) && nvram_get_int("smart_connect_x"))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else if ((unit == 2) && nvram_get_int("smart_connect_x"))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else {
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 0);
			acs = 0 ;
		}
#else
		if (!(((unit == 0) && (nvram_get_int("smart_connect_x") == 1)) ||
		      ((unit == 1) && nvram_get_int("smart_connect_x")) ||
		      ((unit == 2) && nvram_get_int("smart_connect_x"))))
			acs = 0;
#endif

		if (acs) {
			nvram_set(strcat_r(prefix, "nmode_x", tmp), "0");
			nvram_set(strcat_r(prefix, "bw", tmp), "0");
			nvram_set(strcat_r(prefix, "chanspec", tmp), "0");
		}
#endif

#ifdef RTCONFIG_PROXYSTA
		/* See if other interface also has psta or psr enabled */
		char words[256], *if_next;
		int if_unit=0;
		foreach (words, nvram_safe_get("wl_ifnames"), if_next) {
			psta = is_psta(if_unit);
			psr = is_psr(if_unit);
			db_rpt |= ((psta_exist_except(if_unit) || psr_exist_except(if_unit)) &&
			  	(psta || psr));
			if_unit++;
		}

		/* Disable all VIFS wlX.2 onwards */
		if (is_psta(unit) || is_psr(unit))
		{
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");

			strncpy(interface_list, nvram_safe_get(lan_ifnames), interface_list_size);

			/* While enabling proxy sta or repeater modes on second wl interface
			 * (dual band repeater) set nvram variable dpsta_ifnames to upstream
			 * interfaces.
			 */
#ifdef PXYSTA_DUALBAND
			if (db_rpt) {
				char if_list[NVRAM_MAX_VALUE_LEN];
				int if_list_size = sizeof(if_list);

				memset(if_list, 0, sizeof(if_list));
				strcpy(tmp, "dpsta_ifnames");
				sprintf(nv_interface, "wl%s", nvram_safe_get("wlc_band"));
				nvifname_to_osifname(nv_interface, os_interface,
					sizeof(os_interface));
				add_to_list(os_interface, if_list, if_list_size);
				sprintf(nv_interface, "wl%s", nvram_safe_get("wlc_band_ex"));
				nvifname_to_osifname(nv_interface, os_interface,
					sizeof(os_interface));
				add_to_list(os_interface, if_list, if_list_size);
				nvram_set(tmp, if_list);
			} else if(!db_rpt)
#endif
				nvram_set("dpsta_ifnames", "");

			sprintf(nv_interface, "wl%d.1", unit);

			/* Set the wl mode for the virtual interface */
			sprintf(tmp, "wl%d.1_mode", unit);
			if (is_psta(unit)) {
				/* For Proxy we need to remove our ap interface */
				remove_from_list(nv_interface, interface_list, interface_list_size);
				nvram_set(lan_ifnames, interface_list);

				/* Clear the ap mode */
				nvram_set(tmp, "");
			} else {
				/* For Repeater we need to add our ap interface to the bridged lan */
				add_to_list(nv_interface, interface_list, interface_list_size);
				nvram_set(lan_ifnames, interface_list);

				/* Set the ap mode */
				nvram_set(tmp, "ap");
			}

			/* Turn off wlX_ure when proxy repeater modes are enabled */
			nvram_set("wl_ure", "0");

			for (i = 2; i < max_no_vifs; i++) {
				sprintf(tmp, "wl%d.%d_bss_enabled", unit, i);
				nvram_set(tmp, "0");
			}
		}
		else
#endif
		{
#ifdef RTCONFIG_PROXYSTA
			if(!db_rpt)
				nvram_set("dpsta_ifnames", "");
#endif
			memset(interface_list, 0, interface_list_size);
			for (i = 1; i < max_no_vifs; i++) {
				sprintf(nv_interface, "wl%d.%d", unit, i);
				add_to_list(nv_interface, interface_list, interface_list_size);
				nvram_set(strcat_r(nv_interface, "_hwaddr", tmp), "");
			}

			reset_mssid_hwaddr(unit);
		}
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		snprintf(prefix2, sizeof(prefix2), "wl%d_", unit);
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		if (is_psta(unit))
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
		else if (is_psr(unit))
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
#endif
#endif
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	// convert wlc_xxx to wlX_ according to wlc_band == unit
	if (is_ure(unit)) {
		if (subunit==-1) {
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
		else if (subunit==1) {
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
	// TODO: recover nvram from repeater
	else
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	if (is_psta(unit) || is_psr(unit))
	{
		if (subunit == -1)
		{
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
			if (nvram_match(strcat_r(prefix, "phytype", tmp), "v"))	// 802.11AC
				nvram_set(strcat_r(prefix, "bw", tmp), "3");
			else
				nvram_set(strcat_r(prefix, "bw", tmp), "2");
		}
	}
#endif
#endif

	memset(tmp, 0, sizeof(tmp));
	memset(tmp2, 0, sizeof(tmp2));

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared"))
		nvram_set(strcat_r(prefix, "auth", tmp), "1");
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
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa2");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa wpa2");
	}
	else nvram_set(strcat_r(prefix, "akm", tmp), "");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
	{
		nvram_set(strcat_r(prefix, "auth_mode", tmp), "radius");
		nvram_set(strcat_r(prefix, "key", tmp), "2");
	}
	else nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else if (nvram_invmatch(strcat_r(prefix, "akm", tmp), ""))
		nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
	else if (nvram_get_int(strcat_r(prefix, "wep_x", tmp)) != 0)
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else nvram_set(strcat_r(prefix, "wep", tmp), "disabled");

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
	    strstr(nvram_safe_get(strcat_r(prefix, "akm", tmp2)), "wpa"))
		nvram_set(strcat_r(prefix, "preauth", tmp), "1");
	else
		nvram_set(strcat_r(prefix, "preauth", tmp), "");

	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {

		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		list = (char*) malloc(sizeof(char) * (strlen(nv)+1));
		list[0] = 0;

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
		free(list);
	}
	else
		nvram_set(strcat_r(prefix, "maclist", tmp), "");

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
		}
		else if (is_psr(unit))
		{
			nvram_set(strcat_r(prefix, "mode", tmp), "psr");
		}
		else
#endif
#endif
		if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1"))		// wds only
			nvram_set(strcat_r(prefix, "mode", tmp), "wds");

		else if (nvram_match(strcat_r(prefix, "mode_x", tmp), "3"))	// special defined for client
			nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		else nvram_set(strcat_r(prefix, "mode", tmp), "ap");

		// TODO use lazwds directly
		//if (!nvram_match(strcat_r(prefix, "wdsmode_ex", tmp), "2"))
		//	nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
		//else nvram_set(strcat_r(prefix, "lazywds", tmp), "0");

		// TODO need sw_mode ?
		// handle wireless wds list
		if (!nvram_match(strcat_r(prefix, "mode_x", tmp), "0")) {
			if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "1")) {
				nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "wdslist", tmp)));
				list = (char*) malloc(sizeof(char) * (strlen(nv)+1));
				list[0] = 0;

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
				free(list);
			}
			else {
				nvram_set(strcat_r(prefix, "wds", tmp), "");
				nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
			}
		} else {
			nvram_set(strcat_r(prefix, "wds", tmp), "");
			nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
		}

		switch (nvram_get_int(strcat_r(prefix, "mrate_x", tmp))) {
		case 0: /* Auto */
			mcast_rate = 0;
			break;
		case 1: /* Legacy CCK 1Mbps */
			mcast_rate = 1000000;
			break;
		case 2: /* Legacy CCK 2Mbps */
			mcast_rate = 2000000;
			break;
		case 3: /* Legacy CCK 5.5Mbps */
			mcast_rate = 5500000;
			break;
		case 4: /* Legacy OFDM 6Mbps */
			mcast_rate = 6000000;
			break;
		case 5: /* Legacy OFDM 9Mbps */
			mcast_rate = 9000000;
			break;
		case 6: /* Legacy CCK 11Mbps */
			mcast_rate = 11000000;
			break;
		case 7: /* Legacy OFDM 12Mbps */
			mcast_rate = 12000000;
			break;
		case 8: /* Legacy OFDM 18Mbps */
			mcast_rate = 18000000;
			break;
		case 9: /* Legacy OFDM 24Mbps */
			mcast_rate = 24000000;
			break;
		case 10: /* Legacy OFDM 36Mbps */
			mcast_rate = 36000000;
			break;
		case 11: /* Legacy OFDM 48Mbps */
			mcast_rate = 48000000;
			break;
		case 12: /* Legacy OFDM 54Mbps */
			mcast_rate = 54000000;
			break;
		default: /* Auto */
			mcast_rate = 0;
			break;
		}
		nvram_set_int(strcat_r(prefix, "mrate", tmp), mcast_rate);

		if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "0"))		// auto => b/g/n mixed or a/n(/ac) mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");	// 1: 54g Auto, 4: 4g Performance, 5: 54g LRS, 0: 802.11b Only
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "1"))	// n only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "1");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "2");	// devices must advertise HT (11n) capabilities to be allowed to associate
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "4"))	// g/n mixed or a/n mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "1");	// devices must advertise ERP (11g) capabilities to be allowed to associate
			else
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "5"))	// g only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "2");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "1");	// devices must advertise ERP (11g) capabilities to be allowed to associate
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "6"))	// b only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "0");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "7"))	// a only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
#ifdef RTCONFIG_BCMWL6
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "2"))	// b/g mixed
#else
		else								// b/g mixed
#endif
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
#ifdef RTCONFIG_BCMWL6
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "3") && 	// ac only
			 nvram_match(strcat_r(prefix, "nband", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "3");	// devices must advertise VHT (11ac) capabilities to be allowed to associate
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "8"))	// n/ac mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "2");	// devices must advertise HT (11n) capabilities to be allowed to associate
		}
#endif

#ifdef RTCONFIG_BCMWL6
		if (nvram_match(strcat_r(prefix, "bw", tmp), "0"))			// Auto
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
			{
				if (nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");// 40M
				else
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");// 20M
			}
			else
			{
				if (   (get_model() == MODEL_RTAC66U ||
					get_model() == MODEL_RTAC53U ||
					get_model() == MODEL_RTAC56U ||
					get_model() == MODEL_RTAC56S ||
					get_model() == MODEL_RTAC3200 ||
					get_model() == MODEL_RPAC68U ||
					get_model() == MODEL_RTAC68U ||
					get_model() == MODEL_DSLAC68U ||
					get_model() == MODEL_RTAC87U) &&
					nvram_match(strcat_r(prefix, "vreqd", tmp), "1"))
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "7");// 80M
				else if (nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");// 40M
				else
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");// 20M
			}

			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "1") ||	// 20M
			 nvram_match(strcat_r(prefix, "nmcsidx", tmp), "-2"))
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2") &&	// 40M
			 nvram_match(strcat_r(prefix, "nmode", tmp2), "-1"))
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
#ifdef RTCONFIG_PROXYSTA
			if (is_psta(unit) || is_psr(unit))
				nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
			else
#endif
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "3") &&	// 80M
			 nvram_match(strcat_r(prefix, "vreqd", tmp2), "1"))
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			else
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "7");
#ifdef RTCONFIG_PROXYSTA
			if (is_psta(unit) || is_psr(unit))
				nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
			else
#endif
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}

		if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))		// 5G
		chanspec_fix(unit);

//		if (unit)
		{
			if (nvram_match(strcat_r(prefix, "txbf", tmp), "1"))
			{
				nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "1");
				nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "1");
			}
			else
			{
				nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "0");
				nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "0");
			}
		}
#ifdef RTCONFIG_BCM7
		nvram_set(strcat_r(prefix, "txbf_imp", tmp), nvram_safe_get(strcat_r(prefix, "itxbf", tmp2)));
#endif
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
		/* Wireless IGMP Snooping */
		i = nvram_get_int(strcat_r(prefix, "igs", tmp));
		nvram_set_int(strcat_r(prefix, "wmf_bss_enable", tmp), i ? 1 : 0);
#ifdef RTCONFIG_BCMWL6
		nvram_set_int(strcat_r(prefix, "wmf_ucigmp_query", tmp), /*i ? 1 :*/ 0);
		nvram_set_int(strcat_r(prefix, "wmf_mdata_sendup", tmp), /*i ? 1 :*/ 0);
#ifdef RTCONFIG_BCMARM
		nvram_set_int(strcat_r(prefix, "wmf_ucast_upnp", tmp), /*i ? 1 :*/ 0);
#ifndef RTCONFIG_BCM7
		nvram_set_int(strcat_r(prefix, "wmf_igmpq_filter", tmp), /*i ? 1 :*/ 0);
#endif
#endif
		nvram_set_int(strcat_r(prefix, "acs_fcs_mode", tmp), i && nvram_match(strcat_r(prefix, "nband", tmp2), "1") ? 1 : 0);
		nvram_set_int(strcat_r(prefix, "dcs_csa_unicast", tmp), i ? 1 : 0);
#endif
#else
		nvram_set_int(strcat_r(prefix, "wmf_bss_enable", tmp), 0);
#ifdef RTCONFIG_BCMWL6
		nvram_set_int(strcat_r(prefix, "wmf_ucigmp_query", tmp), 0);
		nvram_set_int(strcat_r(prefix, "wmf_mdata_sendup", tmp), 0);
#ifdef RTCONFIG_BCMARM
		nvram_set_int(strcat_r(prefix, "wmf_ucast_upnp", tmp), 0);
#ifndef RTCONFIG_BCM7
		nvram_set_int(strcat_r(prefix, "wmf_igmpq_filter", tmp), 0);
#endif
#endif
		nvram_set_int(strcat_r(prefix, "acs_fcs_mode", tmp), 0);
		nvram_set_int(strcat_r(prefix, "dcs_csa_unicast", tmp), 0);
#endif
#endif

		sprintf(tmp2, "%d", nvram_get_int(strcat_r(prefix, "pmk_cache", tmp)) * 60);
		nvram_set(strcat_r(prefix, "net_reauth", tmp), tmp2);

		if (nvram_match(strcat_r(prefix, "nband", tmp), "1")) {
			if (	((get_model() == MODEL_RTAC68U || get_model() == MODEL_RPAC68U || get_model() == MODEL_DSLAC68U) &&
				nvram_match(strcat_r(prefix, "reg_mode", tmp), "off") &&
                                nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
                                nvram_match(strcat_r(prefix, "country_rev", tmp), "13")) ||
                                ((get_model() == MODEL_RTAC66U) &&
				nvram_match(strcat_r(prefix, "reg_mode", tmp), "off") &&
                                nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
                                nvram_match(strcat_r(prefix, "country_rev", tmp), "31") &&
                                nvram_match(strcat_r(prefix, "dfs", tmp), "1")) ||
                                ((get_model() == MODEL_RTN66U) &&
				nvram_match(strcat_r(prefix, "reg_mode", tmp), "off") &&
                                nvram_match(strcat_r(prefix, "country_code", tmp), "EU") &&
                                nvram_match(strcat_r(prefix, "country_rev", tmp), "0"))
                        )
			{
				nvram_set(strcat_r(prefix, "reg_mode", tmp), "h");
			}
		}

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
			/*
			if (!nvram_match(strcat_r(prefix2, "macmode", tmp2), "disabled"))
			{
				nvram_set(strcat_r(prefix, "macmode", tmp), nvram_safe_get(strcat_r(prefix2, "macmode", tmp2)));
				nvram_set(strcat_r(prefix, "maclist", tmp), nvram_safe_get(strcat_r(prefix2, "maclist", tmp2)));
			}
			else
			{
				nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
				nvram_set(strcat_r(prefix, "maclist", tmp), "");
			}
			*/
                        if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled"))
			{
			/* Allow guest macmode independent of base macmode, initialize maclist with base macmode */
				if (!nvram_match("guest_maclist_ovr", "1"))
				{
					nvram_set(strcat_r(prefix, "maclist", tmp), nvram_safe_get(strcat_r(prefix2, "maclist", tmp2)));
					nvram_set(strcat_r(prefix, "maclist_x", tmp), nvram_safe_get(strcat_r(prefix2, "maclist_x", tmp2)));
				}
			}
			else
			/* Clear maclist when disabled from gui */
			{
				nvram_set(strcat_r(prefix, "maclist", tmp), "");
				nvram_set(strcat_r(prefix, "maclist_x", tmp), "");
			}
		}
		else
		{
			nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
			/* Do not clear guest maclist on guest disable network
			nvram_set(strcat_r(prefix, "maclist", tmp), "");
			nvram_set(strcat_r(prefix, "maclist_x", tmp), ""); */
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled") ||
		(nvram_invmatch(strcat_r(prefix, "akm", tmp), "") &&
		 nvram_match(strcat_r(prefix, "crypto", tmp), "tkip")))
	{
#ifdef RTCONFIG_BCMWL6
		if (subunit == -1)
		{
			strcpy(tmp2, nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));
			if ((nvp = strchr(tmp2, '/')) || (nvp = strchr(tmp2, 'l'))
				|| (nvp = strchr(tmp2, 'u')))
			{
				*nvp = '\0';
				nvram_set(strcat_r(prefix, "chanspec", tmp), tmp2);
			}
		}
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
		nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
		if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
		nvram_set(strcat_r(prefix, "gmode", tmp), "1");
		nvram_set(strcat_r(prefix, "bw", tmp), "1");		// reset to default setting
#ifdef RTCONFIG_BCMWL6
		nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
	}
#ifdef RTCONFIG_QTN
	if (nvram_get_int("qtn_ready") == 1){
		runtime_config_qtn(unit, subunit);
	}
#endif
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
	case MODEL_RTN53:
	case MODEL_RTN12:
	case MODEL_RTN12B1:
	case MODEL_RTN12C1:
	case MODEL_RTN12D1:
	case MODEL_RTN12VP:
	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
	case MODEL_APN12HP:
	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
		/* Reset vlan 1 */
		eval("vconfig", "rem", "vlan1");
		eval("et", "robowr", "0x34", "0x8", "0x01001000");
		eval("et", "robowr", "0x34", "0x6", "0x3001");
		/* Add wan interface */
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");

		if (nvram_match("switch_wantag", "unifi_home")) {
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
		else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		}
		else if (nvram_match("switch_wantag", "singtel_mio")) {
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
			eval("et", "robowr", "0x34", "0x8", "0x0101e012");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x10", "0x8014");
		}
		else if (nvram_match("switch_wantag", "singtel_others")) {
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
		else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01453012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01335012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		}
		break;

	case MODEL_RTN14UHP:
		/* WAN(P4), L1(P0), L2(P1), L3(P2), L4(P3) */

		/* Reset vlan 1 */
		eval("vconfig", "rem", "vlan1");
		eval("et", "robowr", "0x34", "0x8", "0x01001000");
		eval("et", "robowr", "0x34", "0x6", "0x3001");
		/* Add wan interface */
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");
		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan0ports= 0 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010001e7");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258218");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x16", "0x0258");
		}
		else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		}
		else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan0ports= 0 1 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010000E3");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014218");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e014");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x16", "0x8014");
		}
		else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan0ports= 0 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010001e7");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014218");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x16", "0x8014");
		}
		else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01453014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01335014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e014");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
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
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");

		if (nvram_match("switch_wantag", "unifi_home")) {
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
		else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		}
		else if (nvram_match("switch_wantag", "singtel_mio")) {
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
			eval("et", "robowr", "0x34", "0x8", "0x0101e005");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x12", "0x8014");
		}
		else if (nvram_match("switch_wantag", "singtel_others")) {
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
		else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01453005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01335005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		}

		break;

	case MODEL_RTN16:
		// config wan port
		if (wan_vid) {
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0101");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		// Set Wan port PRIO
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			// config LAN 3 = VoIP
			if (nvram_match("switch_wantag", "m1_fiber")) {
				// Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	// Nomo case, untag it.
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				// Set vlan table entry register
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		else if (nvram_match("switch_stb_x", "4")) {
			// config LAN 4 = IPTV
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x12", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			// Set vlan table entry register
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0403");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		else if (nvram_match("switch_stb_x", "6")) {
			// config LAN 3 = VoIP
			if (nvram_match("switch_wantag", "singtel_mio")) {
				// Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	// Nomo case, untag it.
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				// Set vlan table entry register
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			// config LAN 4 = IPTV
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				// Set vlan table entry register
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;
				/* P0  P1 P2 P3 P4 P5 */
	case MODEL_RTAC68U:	/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTAC3200:
	case MODEL_RTN18U:	/* WAN L1 L2 L3 L4 CPU */
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0021");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {	// P3
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
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
		else if (nvram_match("switch_stb_x", "4")) { // P4
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
		else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */ // P3
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			/* config LAN 4 = IPTV */ // P4
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;
				/* P0  P1 P2 P3 P5 P7 */
	case MODEL_RTAC87U:	/* WAN L4 L3 L2 L1 CPU */
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0081");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {  // P3
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		else if (nvram_match("switch_stb_x", "4")) { // P4
			/* config LAN 4 = IPTV */
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x12", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			/* Set vlan table entry register */
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0403");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */ // P3
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			/* config LAN 4 = IPTV */ // P4
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;
				/* P0 P1 P2 P3 P4 P5 */
	case MODEL_RTAC56S:	/* L1 L2 L3 L4 WAN CPU */
	case MODEL_RTAC56U:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0030");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {	// P2
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 4 & 2, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 4 & 2, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0814");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		else if (nvram_match("switch_stb_x", "4")) { // P3
			/* config LAN 4 = IPTV */
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x16", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			/* Set vlan table entry register */
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x1018");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */	// P2
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 2 & 4, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x081C");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0814");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			/* config LAN 4 = IPTV */ //P3
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x081C");
				else
					eval("et", "robowr", "0x05", "0x83", "0x1018");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;

	case MODEL_RTN66U:
	case MODEL_RTAC66U:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0101");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber_iptv")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* Just forward packets between port 0 & 4 & untag */
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0337");	/* vlan id=823 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0338");	/* vlan id=824 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber_sp_iptv")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0006");	/* vlan id= 6  */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x000E");	/* vlan id= 14 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* Just forward packets between port 0 & 4 & untag */
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x000F");	/* vlan id= 15 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0011");	/* vlan id= 17 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
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
		else if (nvram_match("switch_stb_x", "4")) {
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
		else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			if (iptv_vid) { /* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;

	case MODEL_RTN15U:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0110");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			if (nvram_match("switch_wantag", "m1_fiber")) {
				/* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
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
		else if (nvram_match("switch_stb_x", "4")) {
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
		else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
			    if (voip_vid) {
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0613");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0412");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			    }
			}
			if (iptv_vid) { /* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x10", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0613");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0211");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;

	case MODEL_RTAC53U:
		/* WAN L1 L2 L3 L4 CPU */
		/*  0   1  2  3  4  5  */
		/*
			vlan1 : LAN
			vlan2 : WAN
		*/
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x80", "0x1");
		/* Reset vlan 2 */
		eval("vconfig", "rem", "vlan2");
		eval("et", "robowr", "0x34", "0x8", "0x01002000", "0x4");
		eval("et", "robowr", "0x34", "0x6", "0x3002");
		/* Add wan interface */
		sprintf(port_id, "%d", wan_vid);
		eval("vconfig", "add", interface, port_id);
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan1ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010013ae", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x18", "0x0258");
		}
		else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		}
		else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan1ports= 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010011a6", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 4 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01014411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e009", "0x4");/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x18", "0x8014");
		}
		else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan1ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010013ae", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x18", "0x8014");
		}
		else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan1103ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01453009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan621 ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01335009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		}
		else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan11ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
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
#ifdef RTCONFIG_QTN
	if (!strcmp(ifname, "wifi0")){
		ret = rpc_qcsapi_init();
		if (ret == 0) {
			return 1;
		}else{
			return -1;
		}
	}
#endif
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

void
ate_commit_bootloader(char *err_code)
{
	nvram_set("Ate_power_on_off_enable", err_code);
	nvram_commit();
	nvram_set("asuscfeAte_power_on_off_ret", err_code);
	nvram_set("asuscfecommit=", "1");
}

