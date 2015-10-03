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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <wlutils.h>
#include <bcmdevs.h>

#include <shared.h>

#ifdef RTCONFIG_QCA
#include <qca.h>
#include <flash_mtd.h>
#endif

#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
#error !!!!!!!!!!!QCA driver must use country code!!!!!!!!!!!
#endif
static struct load_wifi_kmod_seq_s {
	char *kmod_name;
	unsigned int load_sleep;
	unsigned int remove_sleep;
} load_wifi_kmod_seq[] = {
	{ "ath_hal", 0, 0 },
	{ "hst_tx99", 0, 0 },
	{ "ath_dev", 0, 0 },
	{ "umac", 0, 2 },
};

static struct load_sfe_kmod_seq_s {
	char *kmod_name;
	unsigned int load_sleep;
	unsigned int remove_sleep;
} load_sfe_kmod_seq[] = {
	{ "shortcut_fe", 0, 0 },
	{ "fast_classifier", 0, 0 },
};

static void __mknod(char *name, mode_t mode, dev_t dev)
{
	if (mknod(name, mode, dev)) {
		printf("## mknod %s mode 0%o fail! errno %d (%s)", name, mode, errno, strerror(errno));
	}
}

void init_devs(void)
{
	int status;

	__mknod("/dev/nvram", S_IFCHR | 0666, makedev(228, 0));
	__mknod("/dev/dk0", S_IFCHR | 0666, makedev(63, 0));
	__mknod("/dev/dk1", S_IFCHR | 0666, makedev(63, 1));
	__mknod("/dev/armem", S_IFCHR | 0660, makedev(1, 13));
	__mknod("/dev/sfe", S_IFCHR | 0660, makedev(252, 0)); // TBD
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	eval("ln", "-sf", "/dev/mtdblock2", "/dev/caldata");	/* mtdblock2 = SPI flash, Factory MTD partition */
#else
	eval("ln", "-sf", "/dev/mtdblock3", "/dev/caldata");	/* mtdblock3 = Factory MTD partition */
#endif

	if ((status = WEXITSTATUS(modprobe("nvram_linux"))))
		printf("## modprove(nvram_linux) fail status(%d)\n", status);
}

void generate_switch_para(void)
{
#if defined(RTCONFIG_DUALWAN)
	int model;
	int wans_cap = get_wans_dualwan() & WANSCAP_WAN;
	int wanslan_cap = get_wans_dualwan() & WANSCAP_LAN;

	// generate nvram nvram according to system setting
	model = get_model();

	switch (model) {
	case MODEL_RTAC55U:
	case MODEL_RTAC55UHP:
	case MODEL_RT4GAC55U:
		nvram_unset("vlan3hwname");
		if ((wans_cap && wanslan_cap)
		    )
			nvram_set("vlan3hwname", "et0");
		break;
	}
#endif
}

static void init_switch_qca(void)
{
	generate_switch_para();

	// TODO: replace to nvram controlled procedure later
#ifdef RTCONFIG_RALINK_RT3052
	if (is_routing_enabled())
		config_3052(nvram_get_int("switch_stb_x"));
#else
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	eval("ifconfig", MII_IFNAME, "hw", "ether", nvram_safe_get("et0macaddr"));
#endif
	if (strlen(nvram_safe_get("wan0_ifname"))) {
		eval("ifconfig", nvram_safe_get("wan0_ifname"), "hw",
		     "ether", nvram_safe_get("et0macaddr"));
	}
	config_switch();
#endif

#ifdef RTCONFIG_SHP
	if (nvram_get_int("qos_enable") || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_disable", "1");
	} else {
		nvram_set("lfp_disable", "0");
	}

	if (nvram_get_int("lfp_disable") == 0) {
		restart_lfp();
	}
#endif
}

void enable_jumbo_frame(void)
{
	int mtu = 1518;	/* default value */
	char mtu_str[] = "9000XXX";

	if (!nvram_contains_word("rc_support", "switchctrl"))
		return;

	if (nvram_get_int("jumbo_frame_enable"))
		mtu = 9000;

	sprintf(mtu_str, "%d", mtu);
	eval("swconfig", "dev", MII_IFNAME, "set", "max_frame_size", mtu_str);
}

void init_switch(void)
{
	init_switch_qca();
}

char *get_lan_hwaddr(void)
{
	/* TODO: handle exceptional model */
        return nvram_safe_get("et0macaddr");
}

/**
 * Setup a VLAN.
 * @vid:	VLAN ID
 * @prio:	VLAN PRIO
 * @mask:	bit31~16:	untag mask
 * 		bit15~0:	port member mask
 * @return:
 * 	0:	success
 *  otherwise:	fail
 *
 * bit definition of untag mask/port member mask
 * 0:	Port 0, LANx port which is closed to WAN port in visual.
 * 1:	Port 1
 * 2:	Port 2
 * 3:	Port 3
 * 4:	Port 4, WAN port
 * 9:	Port 9, RGMII/MII port that is used to connect CPU and WAN port.
 * 	a. If you only have one RGMII/MII port and it is shared by WAN/LAN ports,
 * 	   you have to define two VLAN interface for WAN/LAN ports respectively.
 * 	b. If your switch chip choose another port as same feature, convert bit9
 * 	   to your own port in low-level driver.
 */
static int __setup_vlan(int vid, int prio, unsigned int mask)
{
	char vlan_str[] = "4096XXX";
	char prio_str[] = "7XXX";
	char mask_str[] = "0x00000000XXX";
	char *set_vlan_argv[] = { "rtkswitch", "36", vlan_str, NULL };
	char *set_prio_argv[] = { "rtkswitch", "37", prio_str, NULL };
	char *set_mask_argv[] = { "rtkswitch", "39", mask_str, NULL };

	if (vid > 4096) {
		_dprintf("%s: invalid vid %d\n", __func__, vid);
		return -1;
	}

	if (prio > 7)
		prio = 0;

	_dprintf("%s: vid %d prio %d mask 0x%08x\n", __func__, vid, prio, mask);

	if (vid >= 0) {
		sprintf(vlan_str, "%d", vid);
		_eval(set_vlan_argv, NULL, 0, NULL);
	}

	if (prio >= 0) {
		sprintf(prio_str, "%d", prio);
		_eval(set_prio_argv, NULL, 0, NULL);
	}

	sprintf(mask_str, "0x%08x", mask);
	_eval(set_mask_argv, NULL, 0, NULL);

	return 0;
}

int config_switch_for_first_time = 1;
void config_switch(void)
{
	int model = get_model();
	int stbport;
	int controlrate_unknown_unicast;
	int controlrate_unknown_multicast;
	int controlrate_multicast;
	int controlrate_broadcast;
	int merge_wan_port_into_lan_ports;

	dbG("link down all ports\n");
	eval("rtkswitch", "17");	// link down all ports

	switch (model) {
	case MODEL_RTAC55U:	/* fall through */
	case MODEL_RTAC55UHP:	/* fall through */
	case MODEL_RT4GAC55U:	/* fall through */
		merge_wan_port_into_lan_ports = 1;
		break;
	default:
		merge_wan_port_into_lan_ports = 0;
	}

	if (config_switch_for_first_time)
		config_switch_for_first_time = 0;
	else {
		dbG("software reset\n");
		eval("rtkswitch", "27");	// software reset
	}

#ifdef RTCONFIG_DEFAULT_AP_MODE
	if (nvram_get_int("sw_mode") != SW_MODE_ROUTER)
		system("rtkswitch 8 7"); // LLLLL
	else
#endif
	system("rtkswitch 8 0"); // init, rtkswitch 114,115,14,15 need it
	if (is_routing_enabled()) {
		char parm_buf[] = "XXX";

		stbport = atoi(nvram_safe_get("switch_stb_x"));
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("ISP Profile/STB: %s/%d\n", nvram_safe_get("switch_wantag"), stbport);
		/* stbport:	Model-independent	unifi_malaysia=1	otherwise
		 * 		IPTV STB port		(RT-N56U)		(RT-N56U)
		 * -----------------------------------------------------------------------
		 *	0:	N/A			LLLLW
		 *	1:	LAN1			LLLTW			LLLWW
		 *	2:	LAN2			LLTLW			LLWLW
		 *	3:	LAN3			LTLLW			LWLLW
		 *	4:	LAN4			TLLLW			WLLLW
		 *	5:	LAN1 + LAN2		LLTTW			LLWWW
		 *	6:	LAN3 + LAN4		TTLLW			WWLLW
		 */

		if (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) {
			//2012.03 Yau modify
			char tmp[128];
			char *p;
			int voip_port = 0;
			int t, vlan_val = -1, prio_val = -1;
			unsigned int mask = 0;

//			voip_port = atoi(nvram_safe_get("voip_port"));
			voip_port = 3;
			if (voip_port < 0 || voip_port > 4)
				voip_port = 0;		

			/* Fixed Ports Now*/
			stbport = 4;	
			voip_port = 3;
	
			sprintf(tmp, "rtkswitch 29 %d", voip_port);	
			system(tmp);	

			if (!strncmp(nvram_safe_get("switch_wantag"), "unifi", 5)) {
				/* Added for Unifi. Cherry Cho modified in 2011/6/28.*/
				if(strstr(nvram_safe_get("switch_wantag"), "home")) {
					system("rtkswitch 38 1");		/* IPTV: P0 */
					/* Internet:	untag: P9;   port: P4, P9 */
					__setup_vlan(500, 0, 0x02000210);
					/* IPTV:	untag: P0;   port: P0, P4 */
					__setup_vlan(600, 0, 0x00010011);
				}
				else {
					/* No IPTV. Business package */
					/* Internet:	untag: P9;   port: P4, P9 */
					system("rtkswitch 38 0");
					__setup_vlan(500, 0, 0x02000210);
				}
			}
			else if (!strncmp(nvram_safe_get("switch_wantag"), "singtel", 7)) {
				/* Added for SingTel's exStream issues. Cherry Cho modified in 2011/7/19. */
				if(strstr(nvram_safe_get("switch_wantag"), "mio")) {
					/* Connect Singtel MIO box to P3 */
					system("rtkswitch 40 1");		/* admin all frames on all ports */
					system("rtkswitch 38 3");		/* IPTV: P0  VoIP: P1 */
					/* Internet:	untag: P9;   port: P4, P9 */
					__setup_vlan(10, 0, 0x02000210);
					/* VoIP:	untag: N/A;  port: P1, P4 */
					//VoIP Port: P1 tag
					__setup_vlan(30, 4, 0x00000012);
				}
				else {
					//Connect user's own ATA to lan port and use VoIP by Singtel WAN side VoIP gateway at voip.singtel.com
					system("rtkswitch 38 1");		/* IPTV: P0 */
					/* Internet:	untag: P9;   port: P4, P9 */
					__setup_vlan(10, 0, 0x02000210);
				}

				/* IPTV */
				__setup_vlan(20, 4, 0x00010011);		/* untag: P0;   port: P0, P4 */
			}
			else if (!strcmp(nvram_safe_get("switch_wantag"), "m1_fiber")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/1/13.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(1103, 1, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				//VoIP Port: P1 tag
				__setup_vlan(1107, 1, 0x00000012);
			}
			else if (!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(621, 0, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				__setup_vlan(821, 0, 0x00000012);

				__setup_vlan(822, 0, 0x00000012);		/* untag: N/A;  port: P1, P4 */ //VoIP Port: P1 tag
			}
			else if (!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber_sp")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(11, 0, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				//VoIP Port: P1 tag
				__setup_vlan(14, 0, 0x00000012);
			}
			else {
				/* Cherry Cho added in 2011/7/11. */
				/* Initialize VLAN and set Port Isolation */
				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") && strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("rtkswitch 38 3");		// 3 = 0x11 IPTV: P0  VoIP: P1
				else if(strcmp(nvram_safe_get("switch_wan1tagid"), ""))
					system("rtkswitch 38 1");		// 1 = 0x01 IPTV: P0
				else if(strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("rtkswitch 38 2");		// 2 = 0x10 VoIP: P1
				else
					system("rtkswitch 38 0");		//No IPTV and VoIP ports

				/*++ Get and set Vlan Information */
				if(strcmp(nvram_safe_get("switch_wan0tagid"), "") != 0) {
					// Internet on WAN (port 4)
					if ((p = nvram_get("switch_wan0tagid")) != NULL) {
						t = atoi(p);
						if((t >= 2) && (t <= 4094))
							vlan_val = t;
					}

					if((p = nvram_get("switch_wan0prio")) != NULL && *p != '\0')
						prio_val = atoi(p);

					__setup_vlan(vlan_val, prio_val, 0x02000210);
				}

				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") != 0) {
					// IPTV on LAN4 (port 0)
					if ((p = nvram_get("switch_wan1tagid")) != NULL) {
						t = atoi(p);
						if((t >= 2) && (t <= 4094))
							vlan_val = t;
					}

					if((p = nvram_get("switch_wan1prio")) != NULL && *p != '\0')
						prio_val = atoi(p);

					if(!strcmp(nvram_safe_get("switch_wan1tagid"), nvram_safe_get("switch_wan2tagid")))
						mask = 0x00030013;	//IPTV=VOIP
					else
						mask = 0x00010011;	//IPTV Port: P0 untag 65553 = 0x10 011

					__setup_vlan(vlan_val, prio_val, mask);
				}	

				if(strcmp(nvram_safe_get("switch_wan2tagid"), "") != 0) {
					// VoIP on LAN3 (port 1)
					if ((p = nvram_get("switch_wan2tagid")) != NULL) {
						t = atoi(p);
						if((t >= 2) && (t <= 4094))
							vlan_val = t;
					}

					if((p = nvram_get("switch_wan2prio")) != NULL && *p != '\0')
						prio_val = atoi(p);

					if(!strcmp(nvram_safe_get("switch_wan1tagid"), nvram_safe_get("switch_wan2tagid")))
						mask = 0x00030013;	//IPTV=VOIP
					else
						mask = 0x00020012;	//VoIP Port: P1 untag

					__setup_vlan(vlan_val, prio_val, mask);
				}

			}
		}
		else
		{
			sprintf(parm_buf, "%d", stbport);
			if (stbport)
				eval("rtkswitch", "8", parm_buf);
		}

		/* unknown unicast storm control */
		if (!nvram_get("switch_ctrlrate_unknown_unicast"))
			controlrate_unknown_unicast = 0;
		else
			controlrate_unknown_unicast = atoi(nvram_get("switch_ctrlrate_unknown_unicast"));
		if (controlrate_unknown_unicast < 0 || controlrate_unknown_unicast > 1024)
			controlrate_unknown_unicast = 0;
		if (controlrate_unknown_unicast)
		{
			sprintf(parm_buf, "%d", controlrate_unknown_unicast);
			eval("rtkswitch", "22", parm_buf);
		}
	
		/* unknown multicast storm control */
		if (!nvram_get("switch_ctrlrate_unknown_multicast"))
			controlrate_unknown_multicast = 0;
		else
			controlrate_unknown_multicast = atoi(nvram_get("switch_ctrlrate_unknown_multicast"));
		if (controlrate_unknown_multicast < 0 || controlrate_unknown_multicast > 1024)
			controlrate_unknown_multicast = 0;
		if (controlrate_unknown_multicast) {
			sprintf(parm_buf, "%d", controlrate_unknown_multicast);
			eval("rtkswitch", "23", parm_buf);
		}
	
		/* multicast storm control */
		if (!nvram_get("switch_ctrlrate_multicast"))
			controlrate_multicast = 0;
		else
			controlrate_multicast = atoi(nvram_get("switch_ctrlrate_multicast"));
		if (controlrate_multicast < 0 || controlrate_multicast > 1024)
			controlrate_multicast = 0;
		if (controlrate_multicast)
		{
			sprintf(parm_buf, "%d", controlrate_multicast);
			eval("rtkswitch", "24", parm_buf);
		}
	
		/* broadcast storm control */
		if (!nvram_get("switch_ctrlrate_broadcast"))
			controlrate_broadcast = 0;
		else
			controlrate_broadcast = atoi(nvram_get("switch_ctrlrate_broadcast"));
		if (controlrate_broadcast < 0 || controlrate_broadcast > 1024)
			controlrate_broadcast = 0;
		if (controlrate_broadcast) {
			sprintf(parm_buf, "%d", controlrate_broadcast);
			eval("rtkswitch", "25", parm_buf);
		}
	}
	else if (is_apmode_enabled())
	{
		if (merge_wan_port_into_lan_ports)
			eval("rtkswitch", "8", "100");
	}
#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
	else if (mediabridge_mode())
	{
	}
#endif

	if (is_routing_enabled() || is_apmode_enabled()) {
		dbG("link up wan port(s)\n");
		eval("rtkswitch", "114");	// link up wan port(s)
	}

	enable_jumbo_frame();

#if defined(RTCONFIG_BLINK_LED)
	if (is_swports_bled("led_lan_gpio")) {
		update_swports_bled("led_lan_gpio", nvram_get_int("lanports_mask"));
	}
	if (is_swports_bled("led_wan_gpio")) {
		update_swports_bled("led_wan_gpio", nvram_get_int("wanports_mask"));
	}
#endif
}

int switch_exist(void)
{
	FILE *fp;
	char cmd[64], buf[512];
	int rlen;

	sprintf(cmd, "swconfig dev %s port 0 get link", MII_IFNAME);
	if ((fp = popen(cmd, "r")) == NULL) {
		return 0;
	}
	rlen = fread(buf, 1, sizeof(buf), fp);
	pclose(fp);
	if (rlen <= 1)
		return 0;

	buf[rlen-1] = '\0';
	if (strstr(buf, "link:up speed:1000"))
		return 1;
	return 0;
}

static const char * country_to_code(char *ctry, int band)
{
	if (strcmp(ctry, "US") == 0)
		return "841";
	else if (strcmp(ctry, "CA") == 0)
		return "124";
	else if (strcmp(ctry, "TW") == 0)
		return "158";
	else if (strcmp(ctry, "CN") == 0)
		return "156";
	else if (strcmp(ctry, "GB") == 0)
		return "826";
	else if (strcmp(ctry, "DE") == 0)
		return "276";
	else if (strcmp(ctry, "SG") == 0)
		return "702";
	else if (strcmp(ctry, "HU") == 0)
		return "348";
	else if (strcmp(ctry, "AU") == 0)
		return "37";
	else { // "DB"
		if (band == 2)
			return "392"; // ch1-ch14
		else
			return "100"; // 5G_ALL
	}
}

void load_wifi_driver(void)
{
	char country[FACTORY_COUNTRY_CODE_LEN+1];
	const char *code;
	int i;
	struct load_wifi_kmod_seq_s *p = &load_wifi_kmod_seq[0];

	for (i = 0, p = &load_wifi_kmod_seq[i]; i < ARRAY_SIZE(load_wifi_kmod_seq); ++i, ++p) {
		if (module_loaded(p->kmod_name))
			continue;

		modprobe(p->kmod_name);
		if (p->load_sleep)
			sleep(p->load_sleep);
	}

	//sleep(2);
	eval("iwpriv", "wifi0", "disablestats", "0");
	eval("iwpriv", "wifi1", "enable_ol_stats", "0");
	///////////
	strncpy(country, nvram_safe_get("wl0_country_code"), FACTORY_COUNTRY_CODE_LEN);
	country[FACTORY_COUNTRY_CODE_LEN] = '\0';
	code=country_to_code(country, 2);
	eval("iwpriv", "wifi0", "setCountryID", (char*)code);
	///////////
	strncpy(country, nvram_safe_get("wl1_country_code"), FACTORY_COUNTRY_CODE_LEN);
	country[FACTORY_COUNTRY_CODE_LEN] = '\0';
	code=country_to_code(country, 5);
	eval("iwpriv", "wifi1", "setCountryID", (char*)code);
}

void set_uuid(void)
{
	int len;
	char *p, uuid[60];
	FILE *fp;

	fp = popen("cat /proc/sys/kernel/random/uuid", "r");
	 if (fp) {
	    memset(uuid, 0, sizeof(uuid));
	    fread(uuid, 1, sizeof(uuid), fp);
	    for (len = strlen(uuid), p = uuid; len > 0; len--, p++) {
		    if (isxdigit(*p) || *p == '-')
			    continue;
		    *p = '\0';
		    break;
	    }
	    nvram_set("uuid",uuid);
	    pclose(fp);
	 }   
}

static int create_node=0;
void init_wl(void)
{
   	int unit;
	char *p, *ifname;
	char *wl_ifnames;
	int wlc_band;
	if(!create_node)
	{ 
#if defined(QCA_WIFI_INS_RM)
	   	if(nvram_get_int("sw_mode")==2)
		{   
	  		load_wifi_driver();
			sleep(2);
		}	
#endif	
		dbG("init_wl:create wi node\n");
		if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) 
		{
			p = wl_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (*ifname == 0) break;

				//create ath00x & ath10x 
				 if(strncmp(ifname,WIF_2G,strlen(WIF_2G))==0)
					unit=0;	
				 else if(strncmp(ifname,WIF_5G,strlen(WIF_5G))==0)
					unit=1;
			 	 else
			   		unit=-99;	
			         if(unit>=0)
			   	 {	    
					dbG("\ncreate a wifi node %s from wifi%d\n",ifname,unit);
					doSystem("wlanconfig %s create wlandev wifi%d wlanmode ap",ifname,unit);
   					sleep(1);
				 }	
			}
			free(wl_ifnames);
		}
		create_node=1;
#ifdef RTCONFIG_WIRELESSREPEATER
		if(nvram_get_int("sw_mode")==SW_MODE_REPEATER)
		{ 
		  	wlc_band=nvram_get_int("wlc_band");
			doSystem("wlanconfig sta%d create wlandev wifi%d wlanmode sta nosbeacon",wlc_band,wlc_band);
		}      
#endif		
	}
}

void fini_wl(void)
{
	int wlc_band;
	char wif[256];
	int unit = -1, sunit = 0;
	unsigned int m;	/* bit0~3: 2G, bit4~7: 5G */
	char pid_path[] = "/var/run/hostapd_athXXX.pidYYYYYY";
	char path[] = "/sys/class/net/ath001XXXXXX";
#if defined(QCA_WIFI_INS_RM)
	int i;
	struct load_wifi_kmod_seq_s *wp;
#endif

	dbG("fini_wl:destroy wi node\n");
	for (i = 0, unit = 0, sunit = 0, m = 0xFF; m > 0; ++i, ++sunit, m >>= 1) {
		if (i == 4) {
			unit = 1;
			sunit -= 4;
		}
		__get_wlifname(unit, sunit, wif);
		sprintf(pid_path, "/var/run/hostapd_%s.pid", wif);
		if (f_exists(pid_path))
			kill_pidfile_tk(pid_path);
		sprintf(path, "/sys/class/net/%s", wif);
		if (d_exists(path)) {
			eval("ifconfig", wif, "down");
			eval("wlanconfig", wif, "destroy");
		}
	}

#ifdef RTCONFIG_WIRELESSREPEATER
		if(nvram_get_int("sw_mode")==SW_MODE_REPEATER)
		{ 
		  	wlc_band=nvram_get_int("wlc_band");
			doSystem("wlanconfig sta%d destroy",wlc_band);
		}      
#endif
	create_node=0;

#if defined(QCA_WIFI_INS_RM)
	if(nvram_get_int("sw_mode")==2)
	{   
		for (i = ARRAY_SIZE(load_wifi_kmod_seq)-1, wp = &load_wifi_kmod_seq[i]; i >= 0; --i, --wp) {
			if (!module_loaded(wp->kmod_name))
				continue;

			modprobe_r(wp->kmod_name);
			if (wp->remove_sleep)
				sleep(wp->remove_sleep);
		}
	}	
#endif
}

static void chk_valid_country_code(char *country_code)
{
	if ((unsigned char)country_code[0]!=0xff)
	{
		//
	}
	else
	{
		strcpy(country_code, "DB");
	}
}

void init_syspara(void)
{
	unsigned char buffer[16];
	unsigned char *dst;
	unsigned int bytes;
	char macaddr[] = "00:11:22:33:44:55";
	char macaddr2[] = "00:11:22:33:44:58";
	char country_code[FACTORY_COUNTRY_CODE_LEN+1];
	char pin[9];
	char productid[13];
	char fwver[8];
	char blver[20];
#ifdef RTCONFIG_ODMPID
	char modelname[16];
#endif

	nvram_set("buildno", rt_serialno);
	nvram_set("extendno", rt_extendno);
	nvram_set("buildinfo", rt_buildinfo);
	nvram_set("swpjverno", rt_swpjverno);

	/* /dev/mtd/2, RF parameters, starts from 0x40000 */
	dst = buffer;
	bytes = 6;
	memset(buffer, 0, sizeof(buffer));
	memset(country_code, 0, sizeof(country_code));
	memset(pin, 0, sizeof(pin));
	memset(productid, 0, sizeof(productid));
	memset(fwver, 0, sizeof(fwver));

	if (FRead(dst, OFFSET_MAC_ADDR_2G, bytes) < 0) {  // ET0/WAN is same as 2.4G
		_dprintf("READ MAC address 2G: Out of scope\n");
	} else {
		if (buffer[0] != 0xff)
			ether_etoa(buffer, macaddr);
	}

	if (FRead(dst, OFFSET_MAC_ADDR, bytes) < 0) { // ET1/LAN is same as 5G
		_dprintf("READ MAC address : Out of scope\n");
	} else {
		if (buffer[0] != 0xff)
			ether_etoa(buffer, macaddr2);
	}

	if (!mssid_mac_validate(macaddr) || !mssid_mac_validate(macaddr2))
		nvram_set("wl_mssid", "0");
	else
		nvram_set("wl_mssid", "1");

#if 0	// single band
	nvram_set("et0macaddr", macaddr);
	nvram_set("et1macaddr", macaddr);
#else
	//TODO: separate for different chipset solution
	nvram_set("et0macaddr", macaddr);
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	nvram_set("et1macaddr", macaddr);
#else
	nvram_set("et1macaddr", macaddr2);
#endif
#endif

	dst = (unsigned char*) country_code;
	bytes = FACTORY_COUNTRY_CODE_LEN;
	if (FRead(dst, OFFSET_COUNTRY_CODE, bytes)<0)
	{
		_dprintf("READ ASUS country code: Out of scope\n");
		nvram_set("wl_country_code", "DB");
	}
	else
	{
		dst[FACTORY_COUNTRY_CODE_LEN]='\0';
		chk_valid_country_code(country_code);
		nvram_set("wl_country_code", country_code);
		nvram_set("wl0_country_code", country_code);
		nvram_set("wl1_country_code", country_code);
	}

	/* reserved for Ralink. used as ASUS pin code. */
	dst = (char *)pin;
	bytes = 8;
	if (FRead(dst, OFFSET_PIN_CODE, bytes) < 0) {
		_dprintf("READ ASUS pin code: Out of scope\n");
		nvram_set("wl_pin_code", "");
	} else {
		if ((unsigned char)pin[0] != 0xff)
			nvram_set("secret_code", pin);
		else
			nvram_set("secret_code", "12345670");
	}

	dst = buffer;
	bytes = 16;
	if (linuxRead(dst, 0x20, bytes) < 0) {	/* The "linux" MTD partition, offset 0x20. */
		fprintf(stderr, "READ firmware header: Out of scope\n");
		nvram_set("productid", "unknown");
		nvram_set("firmver", "unknown");
	} else {
		strncpy(productid, buffer + 4, 12);
		productid[12] = 0;
		sprintf(fwver, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2],
			buffer[3]);
		nvram_set("productid", trim_r(productid));
		nvram_set("firmver", trim_r(fwver));
	}

#if defined(RTCONFIG_TCODE)
	/* Territory code */
	memset(buffer, 0, sizeof(buffer));
	if (FRead(buffer, OFFSET_TERRITORY_CODE, 5) < 0) {
		_dprintf("READ ASUS territory code: Out of scope\n");
		nvram_unset("territory_code");
	} else {
		/* [A-Z][A-Z]/[0-9][0-9] */
		if (buffer[2] != '/' ||
		    !isupper(buffer[0]) || !isupper(buffer[1]) ||
		    !isdigit(buffer[3]) || !isdigit(buffer[4]))
		{
			nvram_unset("territory_code");
		} else {
			nvram_set("territory_code", buffer);
		}
	}

	/* PSK */
	memset(buffer, 0, sizeof(buffer));
	if (FRead(buffer, OFFSET_PSK, 14) < 0) {
		_dprintf("READ ASUS PSK: Out of scope\n");
		nvram_set("wifi_psk", "");
	} else {
		if (buffer[0] == 0xff)
			nvram_set("wifi_psk", "");
		else
			nvram_set("wifi_psk", buffer);
	}
#endif

	memset(buffer, 0, sizeof(buffer));
	FRead(buffer, OFFSET_BOOT_VER, 4);
	sprintf(blver, "%s-0%c-0%c-0%c-0%c", trim_r(productid), buffer[0],
		buffer[1], buffer[2], buffer[3]);
	nvram_set("blver", trim_r(blver));

	_dprintf("bootloader version: %s\n", nvram_safe_get("blver"));
	_dprintf("firmware version: %s\n", nvram_safe_get("firmver"));

	nvram_set("wl1_txbf_en", "0");

#ifdef RTCONFIG_ODMPID
	FRead(modelname, OFFSET_ODMPID, sizeof(modelname));
	modelname[sizeof(modelname) - 1] = '\0';
	if (modelname[0] != 0 && (unsigned char)(modelname[0]) != 0xff
	    && is_valid_hostname(modelname)
	    && strcmp(modelname, "ASUS")) {
		nvram_set("odmpid", modelname);
	} else
#endif
		nvram_unset("odmpid");

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);

#if !defined(RTCONFIG_TCODE) // move the verification later bcz TCODE/LOC
	verify_ctl_table();
#endif

#ifdef RTCONFIG_QCA_PLC_UTILS
	getPLC_MAC(macaddr);
	nvram_set("plc_macaddr", macaddr);
#endif

#ifdef RTCONFIG_DEFAULT_AP_MODE
	char dhcp = '0';

	if (FRead(&dhcp, OFFSET_FORCE_DISABLE_DHCP, 1) < 0) {
		_dprintf("READ Disable DHCP: Out of scope\n");
	} else {
		if (dhcp == '1')
			nvram_set("ate_flag", "1");
		else
			nvram_set("ate_flag", "0");
	}
#endif
}

#ifdef RTCONFIG_ATEUSB3_FORCE
void post_syspara(void)
{
	unsigned char buffer[16];
	buffer[0]='0';
	if (FRead(&buffer[0], OFFSET_FORCE_USB3, 1) < 0) {
		fprintf(stderr, "READ FORCE_USB3 address: Out of scope\n");
	}
	if (buffer[0]=='1')
		nvram_set("usb_usb3", "1");
}
#endif

void generate_wl_para(int unit, int subunit)
{
}

char *get_staifname(int band)
{
	return (char*) ((!band)? STA_2G:STA_5G);
}

char *get_vphyifname(int band)
{
	return (char*) ((!band)? VPHY_2G:VPHY_5G);
}

char *__get_wlifname(int band, int subunit, char *buf)
{
	if (!buf)
		return buf;

	if (!subunit)
		strcpy(buf, (!band)? WIF_2G:WIF_5G);
	else
		sprintf(buf, "%s0%d", (!band)? WIF_2G:WIF_5G, subunit);

	return buf;
}

// only qca solution can reload it dynamically
// only happened when qca_sfe=1
// only loaded when unloaded, and unloaded when loaded
// in restart_firewall for fw_pt_l2tp/fw_pt_ipsec
// in restart_qos for qos_enable
// in restart_wireless for wlx_mrate_x, etc
void reinit_sfe(int unit)
{
	int prim_unit = wan_primary_ifunit();
	int act = 1,i;	/* -1/0/otherwise: ignore/remove sfe/load sfe */
	struct load_sfe_kmod_seq_s *p = &load_sfe_kmod_seq[0];
#if defined(RTCONFIG_DUALWAN)
	int nat_x = -1, l, t, link_wan = 1, link_wans_lan = 1;
	int wans_cap = get_wans_dualwan() & WANSCAP_WAN;
	int wanslan_cap = get_wans_dualwan() & WANSCAP_LAN;
	char nat_x_str[] = "wanX_nat_xXXXXXX";
#endif
	if (!nvram_get_int("qca_sfe"))
		return;

	/* If QoS is enabled, disable sfe. */
	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1)
		act = 0;

	if (act > 0 && !nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))
		act = 0;

	if (act > 0) {
#if defined(RTCONFIG_DUALWAN)
		if (unit < 0 || unit > WAN_UNIT_SECOND) {
			if ((wans_cap && wanslan_cap) ||
			    (wanslan_cap && (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")))
			   )
				act = 0;
		} else {
			sprintf(nat_x_str, "wan%d_nat_x", unit);
			nat_x = nvram_get_int(nat_x_str);
			if (unit == prim_unit && !nat_x)
				act = 0;
			else if ((wans_cap && wanslan_cap) ||
				 (wanslan_cap && (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")))
				)
				act = 0;
			else if (unit != prim_unit)
				act = -1;
		}
#else
		if (!is_nat_enabled())
			act = 0;
#endif
	}

	if (act > 0) {
#if defined(RTCONFIG_DUALWAN)
		if (unit < 0 || unit > WAN_UNIT_SECOND || nvram_match("wans_mode", "lb")) {
			if (get_wans_dualwan() & WANSCAP_USB)
				act = 0;
		} else {
			if (unit == prim_unit && get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
				act = 0;
		}
#else
		if (dualwan_unit__usbif(prim_unit))
			act = 0;
#endif
	}

#if defined(RTCONFIG_DUALWAN)
	if (act != 0 &&
	    ((wans_cap && wanslan_cap) || (wanslan_cap && (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))))
	   )
	{
		/* If WANS_LAN and WAN is enabled, WANS_LAN is link-up and WAN is not link-up, hw_nat MUST be removed.
		 * If hw_nat exists in such scenario, LAN PC can't connect to Internet through WANS_LAN correctly.
		 *
		 * FIXME:
		 * If generic IPTV feature is enabled, STB port and VoIP port are recognized as WAN port(s).
		 * In such case, we don't know whether real WAN port is link-up/down.
		 * Thus, if WAN is link-up and primary unit is not WAN, assume WAN is link-down.
		 */
		for (i = WAN_UNIT_FIRST; i < WAN_UNIT_MAX; ++i) {
			if ((t = get_dualwan_by_unit(i)) == WANS_DUALWAN_IF_USB)
				continue;

			l = wanport_status(i);
			switch (t) {
			case WANS_DUALWAN_IF_WAN:
				link_wan = l && (i == prim_unit);
				break;
			case WANS_DUALWAN_IF_DSL:
				link_wan = l;
				break;
			case WANS_DUALWAN_IF_LAN:
				link_wans_lan = l;
				break;
			default:
				_dprintf("%s: Unknown WAN type %d\n", __func__, t);
			}
		}

		if (!link_wan && link_wans_lan)
			act = 0;
	}

	_dprintf("%s:DUALWAN: unit %d,%d type %d iptv [%s] nat_x %d qos %d wans_mode %s link %d,%d: action %d.\n",
		__func__, unit, prim_unit, get_dualwan_by_unit(unit), nvram_safe_get("switch_wantag"), nat_x,
		nvram_get_int("qos_enable"), nvram_safe_get("wans_mode"),
		link_wan, link_wans_lan, act);
#else
	_dprintf("%s:WAN: unit %d,%d type %d nat_x %d qos %d: action %d.\n",
		__func__, unit, prim_unit, get_dualwan_by_unit(unit),
		nvram_get_int("wan0_nat_x"), nvram_get_int("qos_enable"), act);
#endif

	if (act < 0)
		return;

	for (i = 0, p = &load_sfe_kmod_seq[i]; i < ARRAY_SIZE(load_sfe_kmod_seq); ++i, ++p) {
		if (!act) {
			/* remove sfe */
			if (!module_loaded(p->kmod_name))
				continue;

			modprobe_r(p->kmod_name);
			if (p->remove_sleep)
				sleep(p->load_sleep);

		} else {
			/* load sfe */
			if (module_loaded(p->kmod_name))
				continue;

			modprobe(p->kmod_name);
			if (p->load_sleep)
				sleep(p->load_sleep);
		}			
	}
}

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
#if 1 //eric++
	char wifbuf[32];
	char prefix[] = "wlXXXXXX_", tmp[100];
#if defined(RTCONFIG_WIRELESSREPEATER)
	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER
	    && nvram_get_int("wlc_band") == unit && subunit == 1) {
		strcpy(buf, get_staifname(unit));
	} else
#endif /* RTCONFIG_WIRELESSREPEATER */
	{
		__get_wlifname(unit, 0, wifbuf);
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			sprintf(buf, "%s0%d", wifbuf, subunit_x);
		else
			sprintf(buf, "%s", "");
	}
	return buf;
#else
	return __get_wlifname(unit, subunit, buf);
#endif //eric++
}

int wl_exist(char *ifname, int band)
{
	int ret = 0;
	ret = eval("iwpriv", ifname, "get_driver_caps");
	_dprintf("eval(iwpriv, %s, get_driver_caps) ret(%d)\n", ifname, ret);
	return (ret==0);
}

void
set_wan_tag(char *interface) {
	int model, wan_vid; //, iptv_vid, voip_vid, wan_prio, iptv_prio, voip_prio;
	char wan_dev[10], port_id[7];

	model = get_model();
	wan_vid = nvram_get_int("switch_wan0tagid");
//	iptv_vid = nvram_get_int("switch_wan1tagid");
//	voip_vid = nvram_get_int("switch_wan2tagid");
//	wan_prio = nvram_get_int("switch_wan0prio");
//	iptv_prio = nvram_get_int("switch_wan1prio");
//	voip_prio = nvram_get_int("switch_wan2prio");

	sprintf(wan_dev, "vlan%d", wan_vid);

	switch(model) {
	case MODEL_RTAC55U:
	case MODEL_RTAC55UHP:
	case MODEL_RT4GAC55U:
		ifconfig(interface, IFUP, 0, 0);
		if(wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan port PRIO */
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		break;
	}
}
