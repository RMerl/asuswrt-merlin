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

#include <shared.h>

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#include <flash_mtd.h>
#endif

#ifdef RTCONFIG_RALINK_RT3052
#include <ra3052.h>
#endif

void init_devs(void)
{
#define MKNOD(name,mode,dev)	if(mknod(name,mode,dev)) perror("## mknod " name)

#if defined(LINUX30) && !defined(RTN14U) && !defined(RTAC52U) && !defined(RTAC51U) && !defined(RTN11P) && !defined(RTN300) && !defined(RTN54U) && !defined(RTAC1200HP) && !defined(RTN56UB1) && !defined(RTAC54U)
	/* Below device node are used by proprietary driver.
	 * Thus, we cannot use GPL-only symbol to create/remove device node dynamically.
	 */
	MKNOD("/dev/swnat0", S_IFCHR | 0666, makedev(210, 0));
	MKNOD("/dev/hwnat0", S_IFCHR | 0666, makedev(220, 0));
	MKNOD("/dev/acl0", S_IFCHR | 0666, makedev(230, 0));
	MKNOD("/dev/ac0", S_IFCHR | 0666, makedev(240, 0));
	MKNOD("/dev/mtr0", S_IFCHR | 0666, makedev(250, 0));
	MKNOD("/dev/rtkswitch", S_IFCHR | 0666, makedev(206, 0));
	MKNOD("/dev/nvram", S_IFCHR | 0666, makedev(228, 0));
#else
	MKNOD("/dev/video0", S_IFCHR | 0666, makedev(81, 0));
#if !defined(RTN14U) && !defined(RTAC52U) && !defined(RTAC51U) && !defined(RTN11P) && !defined(RTN300) && !defined(RTN54U) && !defined(RTAC1200HP) && !defined(RTN56UB1) && !defined(RTAC54U)
	MKNOD("/dev/rtkswitch", S_IFCHR | 0666, makedev(206, 0));
#endif
	MKNOD("/dev/spiS0", S_IFCHR | 0666, makedev(217, 0));
	MKNOD("/dev/i2cM0", S_IFCHR | 0666, makedev(218, 0));
#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
#else
	MKNOD("/dev/rdm0", S_IFCHR | 0666, makedev(254, 0));
#endif
	MKNOD("/dev/flash0", S_IFCHR | 0666, makedev(200, 0));
	MKNOD("/dev/swnat0", S_IFCHR | 0666, makedev(210, 0));
	MKNOD("/dev/hwnat0", S_IFCHR | 0666, makedev(220, 0));
	MKNOD("/dev/acl0", S_IFCHR | 0666, makedev(230, 0));
	MKNOD("/dev/ac0", S_IFCHR | 0666, makedev(240, 0));
	MKNOD("/dev/mtr0", S_IFCHR | 0666, makedev(250, 0));
	MKNOD("/dev/gpio0", S_IFCHR | 0666, makedev(252, 0));
	MKNOD("/dev/nvram", S_IFCHR | 0666, makedev(228, 0));
	MKNOD("/dev/PCM", S_IFCHR | 0666, makedev(233, 0));
	MKNOD("/dev/I2S", S_IFCHR | 0666, makedev(234, 0));
#endif
	{
		int status;
		if((status = WEXITSTATUS(modprobe("nvram_linux"))))	printf("## modprove(nvram_linux) fail status(%d)\n", status);
	}
}

//void init_gpio(void)
//{
//	ralink_gpio_init(0, GPIO_DIR_OUT); // Power
//	ralink_gpio_init(13, GPIO_DIR_IN); // RESET
//	ralink_gpio_init(26, GPIO_DIR_IN); // WPS
//}

void generate_switch_para(void)
{
	int model;
	int wans_cap = get_wans_dualwan() & WANSCAP_WAN;
	int wanslan_cap = get_wans_dualwan() & WANSCAP_LAN;

	// generate nvram nvram according to system setting
	model = get_model();

	switch(model) {
		case MODEL_RTN13U:
			if(!is_routing_enabled()) {
				// override boardflags with no VLAN flag
				nvram_set_int("boardflags", nvram_get_int("boardflags")&(~BFL_ENETVLAN));
				nvram_set("lan_ifnames", "eth2 ra0");
			}
			else if(nvram_match("switch_stb_x", "1")) {
				nvram_set("vlan0ports", "0 1 2 5*");
				nvram_set("vlan1ports", "3 4 5u");
			}
			else if(nvram_match("swtich_stb_x", "2")) {
				nvram_set("vlan0ports", "0 1 3 5*");
				nvram_set("vlan1ports", "2 4 5u");
			}
			else if(nvram_match("switch_stb_x", "3")) {
				nvram_set("vlan0ports", "0 2 3 5*");
				nvram_set("vlan1ports", "1 4 5u");
			}
			else if(nvram_match("switch_stb_x", "4")) {
				nvram_set("vlan0ports", "1 2 3 5*");
				nvram_set("vlan1ports", "0 4 5u");
			}
			else if(nvram_match("switch_stb_x", "5")) {
				nvram_set("vlan0ports", "2 3 5*");
				nvram_set("vlan1ports", "0 1 4 5u");
			}
			else {	// default for 0
				nvram_set("vlan0ports", "0 1 2 3 5*");
				nvram_set("vlan1ports", "4 5u");
			}
			break;
		case MODEL_RTN11P:	/* fall through */
		case MODEL_RTN300:	/* fall through */
		case MODEL_RTN14U:	/* fall through */
		case MODEL_RTN54U:      /* fall through */
		case MODEL_RTAC54U:      /* fall through */
		case MODEL_RTN56UB1:      /* fall through */
		case MODEL_RTAC1200HP:  /* fall through */
		case MODEL_RTAC51U:	/* fall through */
		case MODEL_RTAC52U:
			nvram_unset("vlan3hwname");
			if ((wans_cap && wanslan_cap) ||
			    (wanslan_cap && (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")))
			   )
				nvram_set("vlan3hwname", "et0");
			break;
	}
}

static void init_switch_ralink(void)
{
	generate_switch_para();

	// TODO: replace to nvram controlled procedure later
	eval("ifconfig", "eth2", "hw", "ether", nvram_safe_get("et0macaddr"));
#ifdef RTCONFIG_RALINK_RT3052
	if(is_routing_enabled()) config_3052(nvram_get_int("switch_stb_x"));
#else
	if(strlen(nvram_safe_get("wan0_ifname"))) {
		if (!nvram_match("et1macaddr", ""))
			eval("ifconfig", nvram_safe_get("wan0_ifname"), "hw", "ether", nvram_safe_get("et1macaddr"));
		else
			eval("ifconfig", nvram_safe_get("wan0_ifname"), "hw", "ether", nvram_safe_get("et0macaddr"));
	}
#if defined(RTN56UB1)	//workaround, let network device initialize before config_switch()
	eval("ifconfig", "eth2", "up");
	sleep(1);
#endif	
	config_switch();
#endif

#ifdef RTCONFIG_SHP
	if (nvram_get_int("qos_enable") == 1 || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_disable", "1");
	} else {
		nvram_set("lfp_disable", "0");
	}

	if(nvram_get_int("lfp_disable")==0) {
		restart_lfp();
	}
#endif
//	reinit_hwnat(-1);

}

void init_switch()
{
#ifdef RTCONFIG_DSL
	init_switch_dsl();
	config_switch_dsl();	
#else
	init_switch_ralink();
#endif	
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
	char *set_vlan_argv[] = { "rtkswitch", "36", vlan_str , NULL };
	char *set_prio_argv[] = { "rtkswitch", "37", prio_str , NULL };
	char *set_mask_argv[] = { "rtkswitch", "39", mask_str , NULL };

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
void config_switch()
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
	case MODEL_RTN11P:	/* fall through */
	case MODEL_RTN300:	/* fall through */
	case MODEL_RTN14U:	/* fall through */
	case MODEL_RTN36U3:	/* fall through */
	case MODEL_RTN65U:	/* fall through */
	case MODEL_RTN54U:
	case MODEL_RTAC54U:   
	case MODEL_RTAC1200HP:   
	case MODEL_RTAC51U:	/* fall through */
	case MODEL_RTAC52U:	/* fall through */
	case MODEL_RTN56UB1:	/* fall through */
		merge_wan_port_into_lan_ports = 1;
		break;
	default:
		merge_wan_port_into_lan_ports = 0;
	}

	if (config_switch_for_first_time)
		config_switch_for_first_time = 0;
	else
	{
		dbG("software reset\n");
		eval("rtkswitch", "27");	// software reset
	}
#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
	system("rtkswitch 8 0"); //Barton add
#endif

	if (is_routing_enabled())
	{
		char parm_buf[] = "XXX";

		stbport = nvram_get_int("switch_stb_x");
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

		if(!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", ""))//2012.03 Yau modify
		{
			char tmp[128];
			char *p;
			int voip_port = 0;
			int t, vlan_val = -1, prio_val = -1;
			unsigned int mask = 0;

//			voip_port = nvram_get_int("voip_port");
			voip_port = 3;
			if (voip_port < 0 || voip_port > 4)
				voip_port = 0;		

			/* Fixed Ports Now*/
			stbport = 4;	
			voip_port = 3;
	
			sprintf(tmp, "rtkswitch 29 %d", voip_port);	
			system(tmp);	

			if(!strncmp(nvram_safe_get("switch_wantag"), "unifi", 5)) {
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
			else if(!strncmp(nvram_safe_get("switch_wantag"), "singtel", 7)) {
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
			else if(!strcmp(nvram_safe_get("switch_wantag"), "m1_fiber")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/1/13.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(1103, 1, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				//VoIP Port: P1 tag
				__setup_vlan(1107, 1, 0x00000012);
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(621, 0, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				__setup_vlan(821, 0, 0x00000012);

				__setup_vlan(822, 0, 0x00000012);		/* untag: N/A;  port: P1, P4 */ //VoIP Port: P1 tag
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber_sp")) {
				//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 2");			/* VoIP: P1  2 = 0x10 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(11, 0, 0x02000210);
				/* VoIP:	untag: N/A;  port: P1, P4 */
				//VoIP Port: P1 tag
				__setup_vlan(14, 0, 0x00000012);
			}
			else if (!strcmp(nvram_safe_get("switch_wantag"), "meo")) {
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 1");			/* VoIP: P0 */
				/* Internet/VoIP:	untag: P9;   port: P0, P4, P9 */
				__setup_vlan(12, 0, 0x02000211);
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
			controlrate_unknown_unicast = nvram_get_int("switch_ctrlrate_unknown_unicast");
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
			controlrate_unknown_multicast = nvram_get_int("switch_ctrlrate_unknown_multicast");
		if (controlrate_unknown_multicast < 0 || controlrate_unknown_multicast > 1024)
			controlrate_unknown_multicast = 0;
		if (controlrate_unknown_multicast)
		{
			sprintf(parm_buf, "%d", controlrate_unknown_multicast);
			eval("rtkswitch", "23", parm_buf);
		}
	
		/* multicast storm control */
		if (!nvram_get("switch_ctrlrate_multicast"))
			controlrate_multicast = 0;
		else
			controlrate_multicast = nvram_get_int("switch_ctrlrate_multicast");
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
			controlrate_broadcast = nvram_get_int("switch_ctrlrate_broadcast");
		if (controlrate_broadcast < 0 || controlrate_broadcast > 1024)
			controlrate_broadcast = 0;
		if (controlrate_broadcast)
		{
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
#ifdef RTCONFIG_DSL
		dbG("link up all ports\n");
		eval("rtkswitch", "16");	// link up all ports
#else
		dbG("link up wan port(s)\n");
		eval("rtkswitch", "114");	// link up wan port(s)
#endif
	}

#if defined(RTCONFIG_BLINK_LED)
	if (is_swports_bled("led_lan_gpio")) {
		update_swports_bled("led_lan_gpio", nvram_get_int("lanports_mask"));
	}
	if (is_swports_bled("led_wan_gpio")) {
		update_swports_bled("led_wan_gpio", nvram_get_int("wanports_mask"));
	}
#endif
}

int
switch_exist(void)
{
	int ret;
#ifdef RTCONFIG_DSL
	// 0 means switch exist
	ret = 0;
#else
	ret = eval("rtkswitch", "41");
	_dprintf("eval(rtkswitch, 41) ret(%d)\n", ret);
#endif
	return (ret == 0);
}

void init_wl(void)
{
	if (!module_loaded("rt2860v2_ap"))
		modprobe("rt2860v2_ap");
#if defined (RTCONFIG_WLMODULE_RT3090_AP)
	if (!module_loaded("RTPCI_ap"))
	{
		modprobe("RTPCI_ap");
	}
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if (!module_loaded("iNIC_mii"))
		modprobe("iNIC_mii", "mode=ap", "bridge=1", "miimaster=eth2", "syncmiimac=0");	// set iNIC mac address from eeprom need insmod with "syncmiimac=0"
#endif
#if defined (RTCONFIG_WLMODULE_MT7610_AP)
	if (!module_loaded("MT7610_ap"))
		modprobe("MT7610_ap");
#endif
#if defined (RTCONFIG_WLMODULE_RLT_WIFI)
	if (!module_loaded("rlt_wifi"))
	{   
		modprobe("rlt_wifi");
	}
#endif
#if defined (RTCONFIG_WLMODULE_MT7603E_AP)
	if (!module_loaded("rlt_wifi_7603e"))
		modprobe("rlt_wifi_7603e");
#endif
	sleep(1);
}

void fini_wl(void)
{
	if (module_loaded("hw_nat"))
		modprobe_r("hw_nat");

#if defined (RTCONFIG_WLMODULE_MT7610_AP)
	if (module_loaded("MT7610_ap"))
		modprobe_r("MT7610_ap");
#endif
#if defined (RTCONFIG_WLMODULE_RLT_WIFI)
	if (module_loaded("rlt_wifi"))
	{   
		modprobe_r("rlt_wifi");
#if defined(RTAC1200HP)
		//remove wifi driver, 5G wifi gpio led turn off 
		sleep(1);	
		led_onoff(1); 
#endif
	}
#endif
#if defined (RTCONFIG_WLMODULE_MT7603E_AP)
	if (module_loaded("rlt_wifi_7603e"))
		modprobe_r("rlt_wifi_7603e");
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if (module_loaded("iNIC_mii"))
		modprobe_r("iNIC_mii");
#endif

#if defined (RTCONFIG_WLMODULE_RT3090_AP)
	if (module_loaded("RTPCI_ap"))
	{
		modprobe_r("RTPCI_ap");
	}
#endif

	if (module_loaded("rt2860v2_ap"))
		modprobe_r("rt2860v2_ap");
}


#if ! defined(RTCONFIG_NEW_REGULATION_DOMAIN)
static void chk_valid_country_code(char *country_code)
{
	if ((unsigned char)country_code[0]!=0xff)
	{
		//for specific power
		if     (memcmp(country_code, "Z1", 2) == 0)
			strcpy(country_code, "US");
		else if(memcmp(country_code, "Z2", 2) == 0)
			strcpy(country_code, "GB");
		else if(memcmp(country_code, "Z3", 2) == 0)
			strcpy(country_code, "TW");
		else if(memcmp(country_code, "Z4", 2) == 0)
			strcpy(country_code, "CN");
		//for normal
		if(memcmp(country_code, "BR", 2) == 0)
			strcpy(country_code, "UZ");
	}
	else
	{
		strcpy(country_code, "DB");
	}
}
#endif

#ifdef RA_SINGLE_SKU
static void create_SingleSKU(const char *path, const char *pBand, const char *reg_spec, const char *pFollow)
{
	char src[128];
	char dest[128];

	sprintf(src , "/ra_SKU/SingleSKU%s_%s%s.dat", pBand, reg_spec, pFollow);
	sprintf(dest, "%s/SingleSKU%s.dat", path, pBand);

	eval("mkdir", "-p", (char*)path);
	unlink(dest);
	eval("ln", "-s", src, dest);
}

void gen_ra_sku(const char *reg_spec)
{
#ifdef RTAC52U	// [0x40002] == 0x00 0x02
	unsigned char dst[16];
	if (!(FRead(dst, OFFSET_EEPROM_VER, 2) < 0) && dst[0] == 0x00 && dst[1] == 0x02)
	{
		create_SingleSKU("/etc/Wireless/RT2860", "", reg_spec, "_0002");
	}
	else
#endif
	create_SingleSKU("/etc/Wireless/RT2860", "", reg_spec, "");

#ifdef RTCONFIG_HAS_5G
#ifdef RTAC52U	// [0x40002] == 0x00 0x02
	if (!(FRead(dst, OFFSET_EEPROM_VER, 2) < 0) && dst[0] == 0x00 && dst[1] == 0x02)
	{
		create_SingleSKU("/etc/Wireless/iNIC", "_5G", reg_spec, "_0002");
	}
	else
#endif
	create_SingleSKU("/etc/Wireless/iNIC", "_5G", reg_spec, "");
#endif	/* RTCONFIG_HAS_5G */
}
#endif	/* RA_SINGLE_SKU */

void init_syspara(void)
{
	unsigned char buffer[16];
	unsigned char *dst;
	unsigned int bytes;
	int i;
	char macaddr[]="00:11:22:33:44:55";
	char macaddr2[]="00:11:22:33:44:58";
	char country_code[3];
	char pin[9];
	char productid[13];
	char fwver[8];
	char blver[20];
	unsigned char txbf_para[33];
	char ea[ETHER_ADDR_LEN];
	const char *reg_spec_def;

#if defined(RTAC1200HP) || defined(RTN56UB1)
	char fixch;
	char value_str[MAX_REGSPEC_LEN+1];
	memset(value_str, 0, sizeof(value_str));
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
	memset(txbf_para, 0, sizeof(txbf_para));

	if (FRead(dst, OFFSET_MAC_ADDR, bytes)<0)
	{
		_dprintf("READ MAC address: Out of scope\n");
	}
	else
	{
		if (buffer[0]!=0xff)
			ether_etoa(buffer, macaddr);
	}

#if !defined(RTN14U) && !defined(RTN11P) && !defined(RTN300) // single band
	if (FRead(dst, OFFSET_MAC_ADDR_2G, bytes)<0)
	{
		_dprintf("READ MAC address 2G: Out of scope\n");
	}
	else
	{
		if (buffer[0]!=0xff)
			ether_etoa(buffer, macaddr2);
	}
#endif

#if defined(RTAC1200HP) || defined(RTN56UB1)
	fixch='0';
	FRead(&fixch, OFFSET_FIX_CHANNEL, 1);
	if(fixch=='1')
	{
		_dprintf("Fix Channel for RF Cal. and disable br0's STP\n");
		nvram_set("wl0_channel","1");
		nvram_set("wl1_channel","36");
		nvram_set("lan_stp","0");
	} 

	FRead(value_str, REGSPEC_ADDR, MAX_REGSPEC_LEN);
	for(i = 0; i < MAX_REGSPEC_LEN && value_str[i] != '\0'; i++) {
		if ((unsigned char)value_str[i] == 0xff)
		{
			value_str[i] = '\0';
			break;
		}
	}
	if(!strcmp(value_str,"JP"))
	   nvram_set("JP_CS","1");
	else
	   nvram_set("JP_CS","0");
#endif
#if defined(RTN14U) || defined(RTN11P) || defined(RTN300) // single band
	if (!mssid_mac_validate(macaddr))
#else
	if (!mssid_mac_validate(macaddr) || !mssid_mac_validate(macaddr2))
#endif
		nvram_set("wl_mssid", "0");
	else
		nvram_set("wl_mssid", "1");

#if defined(RTN14U) || defined(RTN11P) || defined(RTN300) // single band
	nvram_set("et0macaddr", macaddr);
	nvram_set("et1macaddr", macaddr);
#else
	//TODO: separate for different chipset solution
	nvram_set("et0macaddr", macaddr);
	nvram_set("et1macaddr", macaddr2);
#endif

	if (FRead(dst, OFFSET_MAC_GMAC0, bytes)<0)
		dbg("READ MAC address GMAC0: Out of scope\n");
	else
	{
		if (buffer[0]==0xff)
		{
			if (ether_atoe(macaddr, ea))
				FWrite(ea, OFFSET_MAC_GMAC0, 6);
		}
	}

	if (FRead(dst, OFFSET_MAC_GMAC2, bytes)<0)
		dbg("READ MAC address GMAC2: Out of scope\n");
	else
	{
		if (buffer[0]==0xff)
		{
			if (ether_atoe(macaddr2, ea))
				FWrite(ea, OFFSET_MAC_GMAC2, 6);
		}
	}

	/* reserved for Ralink. used as ASUS country code. */
#if !defined(RTCONFIG_NEW_REGULATION_DOMAIN)
	dst = (unsigned char*) country_code;
	bytes = 2;
	if (FRead(dst, OFFSET_COUNTRY_CODE, bytes)<0)
	{
		_dprintf("READ ASUS country code: Out of scope\n");
		nvram_set("wl_country_code", "");
	}
	else
	{
		chk_valid_country_code(country_code);
		nvram_set("wl_country_code", country_code);
		nvram_set("wl0_country_code", country_code);
#ifdef RTCONFIG_HAS_5G
		nvram_set("wl1_country_code", country_code);
#endif
	}
#if defined(RTN14U) // for CE Adaptivity
	if ((strcmp(country_code, "DE") == 0) || (strcmp(country_code, "EU") == 0))
		nvram_set("reg_spec", "CE");
	else
		nvram_set("reg_spec", "NDF");
#endif
#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
	dst = buffer;

#if defined(RTAC51U) || defined(RTN11P) 
	reg_spec_def = "CE";
#else
	reg_spec_def = "FCC";
#endif
	bytes = MAX_REGSPEC_LEN;
	memset(dst, 0, MAX_REGSPEC_LEN+1);
	if(FRead(dst, REGSPEC_ADDR, bytes) < 0)
		nvram_set("reg_spec", reg_spec_def); // DEFAULT
	else
	{
		for (i=(MAX_REGSPEC_LEN-1);i>=0;i--) {
			if ((dst[i]==0xff) || (dst[i]=='\0'))
				dst[i]='\0';
		}
		if (dst[0]!=0x00)
			nvram_set("reg_spec", dst);
		else
			nvram_set("reg_spec", reg_spec_def); // DEFAULT
	}

	if (FRead(dst, REG2G_EEPROM_ADDR, MAX_REGDOMAIN_LEN)<0 || memcmp(dst,"2G_CH", 5) != 0)
	{
		_dprintf("Read REG2G_EEPROM_ADDR fail or invalid value\n");
		nvram_set("wl_country_code", "");
		nvram_set("wl0_country_code", "DB");
		nvram_set("wl_reg_2g", "2G_CH14");
	}
	else
	{
		for(i = 0; i < MAX_REGDOMAIN_LEN; i++)
			if(dst[i] == 0xff || dst[i] == 0)
				break;

		dst[i] = 0;
		nvram_set("wl_reg_2g", dst);
		if      (strcmp(dst, "2G_CH11") == 0)
			nvram_set("wl0_country_code", "US");
		else if (strcmp(dst, "2G_CH13") == 0)
			nvram_set("wl0_country_code", "GB");
		else if (strcmp(dst, "2G_CH14") == 0)
			nvram_set("wl0_country_code", "DB");
		else
			nvram_set("wl0_country_code", "DB");
	}

#ifdef RTCONFIG_HAS_5G
	if (FRead(dst, REG5G_EEPROM_ADDR, MAX_REGDOMAIN_LEN)<0 || memcmp(dst,"5G_", 3) != 0)
	{
		_dprintf("Read REG5G_EEPROM_ADDR fail or invalid value\n");
		nvram_set("wl_country_code", "");
		nvram_set("wl1_country_code", "DB");
		nvram_set("wl_reg_5g", "5G_ALL");
	}
	else
	{
		for(i = 0; i < MAX_REGDOMAIN_LEN; i++)
			if(dst[i] == 0xff || dst[i] == 0)
				break;

		dst[i] = 0;
		nvram_set("wl_reg_5g", dst);
		if      (strcmp(dst, "5G_BAND1") == 0)
			nvram_set("wl1_country_code", "GB");
		else if (strcmp(dst, "5G_BAND123") == 0)
			nvram_set("wl1_country_code", "GB");
		else if (strcmp(dst, "5G_BAND14") == 0)
			nvram_set("wl1_country_code", "US");
		else if (strcmp(dst, "5G_BAND24") == 0)
			nvram_set("wl1_country_code", "TW");
		else if (strcmp(dst, "5G_BAND4") == 0)
			nvram_set("wl1_country_code", "CN");
		else if (strcmp(dst, "5G_BAND124") == 0)
			nvram_set("wl1_country_code", "IN");
		else
			nvram_set("wl1_country_code", "DB");
	}
#endif	/* RTCONFIG_HAS_5G */
#endif	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */
#if defined(RTN56U) || defined(RTCONFIG_DSL)
		if (nvram_match("wl_country_code", "BR"))
		{
			nvram_set("wl_country_code", "UZ");
			nvram_set("wl0_country_code", "UZ");
#ifdef RTCONFIG_HAS_5G
			nvram_set("wl1_country_code", "UZ");
#endif	/* RTCONFIG_HAS_5G */
		}
#endif
		if (nvram_match("wl_country_code", "HK") && nvram_match("preferred_lang", ""))
			nvram_set("preferred_lang", "TW");

	/* reserved for Ralink. used as ASUS pin code. */
	dst = (char*)pin;
	bytes = 8;
	if (FRead(dst, OFFSET_PIN_CODE, bytes)<0)
	{
		_dprintf("READ ASUS pin code: Out of scope\n");
		nvram_set("wl_pin_code", "");
	}
	else
	{
		if ((unsigned char)pin[0]!=0xff)
			nvram_set("secret_code", pin);
		else
			nvram_set("secret_code", "12345670");
	}

	dst = buffer;
	bytes = 16;
	if (linuxRead(dst, 0x20, bytes)<0)	/* The "linux" MTD partition, offset 0x20. */
	{
		fprintf(stderr, "READ firmware header: Out of scope\n");
		nvram_set("productid", "unknown");
		nvram_set("firmver", "unknown");
	}
	else
	{
		strncpy(productid, buffer + 4, 12);
		productid[12] = 0;
		sprintf(fwver, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2], buffer[3]);
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
//	sprintf(blver, "%c.%c.%c.%c", buffer[0], buffer[1], buffer[2], buffer[3]);
	sprintf(blver, "%s-0%c-0%c-0%c-0%c", trim_r(productid), buffer[0], buffer[1], buffer[2], buffer[3]);
	nvram_set("blver", trim_r(blver));

	_dprintf("mtd productid: %s\n", nvram_safe_get("productid"));
	_dprintf("bootloader version: %s\n", nvram_safe_get("blver"));
	_dprintf("firmware version: %s\n", nvram_safe_get("firmver"));

	dst = txbf_para;
	int count_0xff = 0;
	if (FRead(dst, OFFSET_TXBF_PARA, 33) < 0)
	{
		fprintf(stderr, "READ TXBF PARA address: Out of scope\n");
	}
	else
	{
		for (i = 0; i < 33; i++)
		{
			if (txbf_para[i] == 0xff)
				count_0xff++;
/*
			if ((i % 16) == 0) fprintf(stderr, "\n");
			fprintf(stderr, "%02x ", (unsigned char) txbf_para[i]);
*/
		}
/*
		fprintf(stderr, "\n");

		fprintf(stderr, "TxBF parameter 0xFF count: %d\n", count_0xff);
*/
	}

	if (count_0xff == 33)
		nvram_set("wl1_txbf_en", "0");
	else
		nvram_set("wl1_txbf_en", "1");

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
#define EEPROM_INIC_SIZE (512)
#define EEPROM_INIT_ADDR 0x48000
#define EEPROM_INIT_FILE "/etc/Wireless/iNIC/iNIC_e2p.bin"
	{
		char eeprom[EEPROM_INIC_SIZE];
		if(FRead(eeprom, EEPROM_INIT_ADDR, sizeof(eeprom)) < 0)
		{
			fprintf(stderr, "FRead(eeprom, 0x%08x, 0x%x) failed\n", EEPROM_INIT_ADDR, sizeof(eeprom));
		}
		else
		{
			FILE *fp;
			char *filepath = EEPROM_INIT_FILE;

			system("mkdir -p /etc/Wireless/iNIC/");
			if((fp = fopen(filepath, "w")) == NULL)
			{
				fprintf(stderr, "fopen(%s) failed!!\n", filepath);
			}
			else
			{
				if(fwrite(eeprom, sizeof(eeprom), 1, fp) < 1)
				{
					perror("fwrite(eeprom)");
				}
				fclose(fp);
			}
		}
	}
#endif

#ifdef RA_SINGLE_SKU
#if defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
	gen_ra_sku(nvram_safe_get("reg_spec"));
#endif	/* RTAC52U && RTAC51U && RTN54U && RTAC54U && RTAC1200HP && RTN56UB1 */
#endif	/* RA_SINGLE_SKU */

	{
#ifdef RTCONFIG_ODMPID
		char modelname[16];
		FRead(modelname, OFFSET_ODMPID, sizeof(modelname));
		modelname[sizeof(modelname)-1] = '\0';
		if(modelname[0] != 0 && (unsigned char)(modelname[0]) != 0xff && is_valid_hostname(modelname) && strcmp(modelname, "ASUS"))
		{
#if defined(RTN11P) || defined(RTN300)
			if(strcmp(modelname, "RT-N12E_B")==0)
				nvram_set("odmpid", "RT-N12E_B1");
			else
#endif	/* RTN11P */
			nvram_set("odmpid", modelname);
		}
		else
#endif
			nvram_unset("odmpid");
	}
#if defined(RTCONFIG_TCODE) && defined(RTN11P)
	if (nvram_match("odmpid", "RT-N12+") && nvram_match("reg_spec", "CN"))
	{
		char *str;
		str = nvram_get("territory_code");
		if(str == NULL || str[0] == '\0') {
			nvram_set("territory_code", "CN/01");
		}
	}
#endif	/* RTCONFIG_TCODE && RTN11P */

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);

	_dprintf("odmpid: %s\n", nvram_safe_get("odmpid"));
	_dprintf("current FW productid: %s\n", nvram_safe_get("productid"));
	_dprintf("current FW firmver: %s\n", nvram_safe_get("firmver"));
}

void generate_wl_para(int unit, int subunit)
{
}

#if defined(RTAC52U) || defined(RTAC51U) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
#define HW_NAT_WIFI_OFFLOADING		(0xFF00)
#define HW_NAT_DEVNAME			"hwnat0"
static void adjust_hwnat_wifi_offloading(void)
{
	int enable_hwnat_wifi = 1, fd;

	if (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")) {
		nvram_unset("isp_profile_hwnat_not_safe");
		eval("rtkswitch", "50");
		if (nvram_get_int("isp_profile_hwnat_not_safe") == 1)
			enable_hwnat_wifi = 0;
	}

	if ((fd = open("/dev/" HW_NAT_DEVNAME, O_RDONLY)) < 0) {
		_dprintf("Open /dev/%s fail. errno %d (%s)\n", HW_NAT_DEVNAME, errno, strerror(errno));
		return;
	}

	_dprintf("hwnat_wifi = %d\n", enable_hwnat_wifi);
	if (ioctl(fd, HW_NAT_WIFI_OFFLOADING, &enable_hwnat_wifi) < 0)
		_dprintf("ioctl error. errno %d (%s)\n", errno, strerror(errno));

	close(fd);
}
#else
static inline void adjust_hwnat_wifi_offloading(void) { }
#endif

// only ralink solution can reload it dynamically
// only happened when hwnat=1
// only loaded when unloaded, and unloaded when loaded
// in restart_firewall for fw_pt_l2tp/fw_pt_ipsec
// in restart_qos for qos_enable
// in restart_wireless for wlx_mrate_x, etc
void reinit_hwnat(int unit)
{
	int prim_unit = wan_primary_ifunit();
	int act = 1;	/* -1/0/otherwise: ignore/remove hwnat/load hwnat */
#if defined(RTCONFIG_DUALWAN)
	int nat_x = -1, i, l, t, link_wan = 1, link_wans_lan = 1;
	int wans_cap = get_wans_dualwan() & WANSCAP_WAN;
	int wanslan_cap = get_wans_dualwan() & WANSCAP_LAN;
	char nat_x_str[] = "wanX_nat_xXXXXXX";
#endif
	if (!nvram_get_int("hwnat"))
		return;

	/* If QoS is enabled, disable hwnat. */
	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1)
		act = 0;

#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
	if (act > 0 && !nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))
		act = 0;
#endif

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

#if defined(RTN65U) || defined(RTN56U) || defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
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
#endif

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

	switch (act) {
	case 0:		/* remove hwnat */
		if (module_loaded("hw_nat")) {
			modprobe_r("hw_nat");
			sleep(1);
		}
		break;
	default:	/* load hwnat */
		if (!module_loaded("hw_nat")) {
			modprobe("hw_nat");
			sleep(1);
		}
		adjust_hwnat_wifi_offloading();
	}
}

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
	char wifbuf[32];
	char prefix[]="wlXXXXXX_", tmp[100];
#if defined(RTCONFIG_WIRELESSREPEATER)
	if (nvram_get_int("sw_mode") == SW_MODE_REPEATER  && nvram_get_int("wlc_band") == unit && subunit==1)
	{   
		if(unit == 1)
			sprintf(buf, "%s", APCLI_5G);
		else
			sprintf(buf, "%s", APCLI_2G);
	}	
	else
#endif /* RTCONFIG_WIRELESSREPEATER */
	{
		memset(wifbuf, 0, sizeof(wifbuf));

		if(unit==0) strncpy(wifbuf, WIF_2G, strlen(WIF_2G)-1);
#if defined(RTCONFIG_HAS_5G)
		else strncpy(wifbuf, WIF_5G, strlen(WIF_5G)-1);
#endif	/* RTCONFIG_HAS_5G */

		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			sprintf(buf, "%s%d", wifbuf, subunit_x);
		else
			sprintf(buf, "%s", "");
	}	
	return buf;
}

int
wl_exist(char *ifname, int band)
{
	int ret = 0;
	ret = eval("iwpriv", ifname, "stat");
	_dprintf("eval(iwpriv, %s, stat) ret(%d)\n", ifname, ret);
	return !ret;
}

void
set_wan_tag(char *interface) {
	int model, wan_vid; //, iptv_vid, voip_vid, wan_prio, iptv_prio, voip_prio;
	char wan_dev[10], port_id[7];

	model = get_model();
	wan_vid = nvram_get_int("switch_wan0tagid");

	sprintf(wan_dev, "vlan%d", wan_vid);

	switch(model) {
	case MODEL_RTAC1200HP:
	case MODEL_RTAC51U:
	case MODEL_RTAC52U:
	case MODEL_RTAC54U:
	case MODEL_RTN11P:
	case MODEL_RTN14U:
	case MODEL_RTN54U:
	case MODEL_RTN56UB1:
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

#ifdef RA_SINGLE_SKU
void reset_ra_sku(const char *location, const char *country, const char *reg_spec)
{
	const char *try_list[] = { reg_spec, location, country, "CE", "FCC"};
	int i;
	for (i = 0; i < ARRAY_SIZE(try_list); i++) {
		if(try_list[i] != NULL && setRegSpec(try_list[i], 0) == 0)
			break;
	}

	if(i >= ARRAY_SIZE(try_list)) {
		cprintf("## NO SKU suit for %s\n", location);
		return;
	}

	cprintf("using %s SKU for %s\n", try_list[i], location);
	gen_ra_sku(try_list[i]);
}
#endif	/* RA_SINGLE_SKU */

