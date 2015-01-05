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

static void __mknod(char *name, mode_t mode, dev_t dev)
{
	if (mknod(name, mode, dev)) {
		printf("## mknod %s mode 0%o fail! errno %d (%s)", name, mode, errno, strerror(errno));
	}
}

void init_devs(void)
{
	int status;

	__mknod("/dev/nvram", S_IFCHR | 0x666, makedev(228, 0));
	__mknod("/dev/dk0", S_IFCHR | 0666, makedev(63, 0));
	__mknod("/dev/dk1", S_IFCHR | 0666, makedev(63, 1));
	__mknod("/dev/armem", S_IFCHR | 0660, makedev(1, 13));
	__mknod("/dev/sfe", S_IFCHR | 0660, makedev(252, 0)); // TBD
	eval("ln", "-sf", "/dev/mtdblock3", "/dev/caldata");	/* mtdblock3 = Factory MTD partition */

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
	if (strlen(nvram_safe_get("wan0_ifname"))) {
		eval("ifconfig", nvram_safe_get("wan0_ifname"), "hw",
		     "ether", nvram_safe_get("et0macaddr"));
	}
	config_switch();
#endif

#ifdef RTCONFIG_SHP
	if (nvram_get_int("qos_enable") || nvram_get_int("macfilter_enable_x")
	    || nvram_get_int("lfp_disable_force")) {
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
	eval("swconfig", "dev", "eth0", "set", "max_frame_size", mtu_str);
}

void init_switch(void)
{
	init_switch_qca();
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
	else if (is_mediabridge_mode())
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
	char buf[512];
	int rlen;

	if ((fp = popen("swconfig dev eth0 port 0 get link", "r")) == NULL) {
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

static char * country_to_code(char *ctry, int band)
{
	if (strcmp(ctry, "US") == 0)
		return "841";
	else if (strcmp(ctry, "CA") == 0)
		return "5001";
	else if (strcmp(ctry, "TW") == 0)
		return "158";
	else if (strcmp(ctry, "CN") == 0)
		return "156";
	else if (strcmp(ctry, "GB") == 0)
		return "826";
	else if (strcmp(ctry, "SG") == 0)
		return "702";
	else if (strcmp(ctry, "HU") == 0)
		return "348";
	else { // "DB"
		if (band == 2)
			return "392"; // ch1-ch14
		else
			return "100"; // 5G_ALL
	}
}

void load_wifi_driver(void)
{
	char country[FACTORY_COUNTRY_CODE_LEN+1], *code;
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
	code=country_to_code(country, 2);
	eval("iwpriv", "wifi0", "setCountryID", code);
	///////////
	strncpy(country, nvram_safe_get("wl1_country_code"), FACTORY_COUNTRY_CODE_LEN);
	code=country_to_code(country, 5);
	eval("iwpriv", "wifi1", "setCountryID", code);
}

#if 0
void set_uuid(void)
{
	char uuid[60],buf[80];
	FILE *fp;
	fp = popen("cat /proc/sys/kernel/random/uuid", "r");
	 if (fp) {
	    memset(uuid, 0, sizeof(uuid));
	    fread(uuid, 1, sizeof(uuid), fp);
	    nvram_set("uuid",uuid);
	    pclose(fp);
	 }   
}
#endif

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
	char *p, *ifname;
	char *wl_ifnames;
	int wlc_band;
	dbG("fini_wl:destroy wi node\n");
	char wl[10];
#if defined(QCA_WIFI_INS_RM)
	int i;
	struct load_wifi_kmod_seq_s *wp;
#endif

	memset(wl,0,sizeof(wl));
	strncpy(wl,WIF_2G,strlen(WIF_2G)-1);
	if ((wl_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) 
	{
		p = wl_ifnames;
		while ((ifname = strsep(&p, " ")) != NULL) {
			while (*ifname == ' ') ++ifname;
			if (*ifname == 0) break;

			if (strncmp(ifname,wl,strlen(wl))==0)
			{
				dbG("\n destroy a wifi node: %s \n",ifname);
				//ifconfig(ifname, 0, NULL, NULL); no use for "beacon buffer av_wbuf is NULL - Ignoring SWBA event"
				doSystem("wlanconfig %s destroy",ifname);
   				sleep(1);
			}
			else
				continue;

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
	nvram_set("et1macaddr", macaddr2);
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

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
#if 1 //eric++
	char wifbuf[32];
	char prefix[] = "wlXXXXXX_", tmp[100];
#if defined(RTCONFIG_WIRELESSREPEATER)
	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER
	    && nvram_get_int("wlc_band") == unit && subunit == 1) {
		if (unit == 0)
			sprintf(buf, "%s", "sta0");
		else
			sprintf(buf, "%s", "sta1");
	} else
#endif /* RTCONFIG_WIRELESSREPEATER */
	{
		memset(wifbuf, 0, sizeof(wifbuf));

		if (unit == 0)
			strncpy(wifbuf, WIF_2G, strlen(WIF_2G) - 1);
		else
			strncpy(wifbuf, WIF_5G, strlen(WIF_5G) - 1);

		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			sprintf(buf, "%s%d0%d", wifbuf, unit,subunit_x);
		else
			sprintf(buf, "%s", "");
	}
	return buf;
#else
	/* FIXME */
	sprintf(buf, "ath%d", unit? 1:0);
	return buf;
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
