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
#include <linux/mii.h>
#include <wlutils.h>
#include <bcmdevs.h>

#include <shared.h>

#ifdef RTCONFIG_QCA
#include <qca.h>
#include <flash_mtd.h>
#if defined(RTCONFIG_SOC_IPQ40XX)
int bg=0;
#endif
#endif

#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
#error !!!!!!!!!!!QCA driver must use country code!!!!!!!!!!!
#endif
static struct load_wifi_kmod_seq_s {
	char *kmod_name;
	int stick;
	unsigned int load_sleep;
	unsigned int remove_sleep;
} load_wifi_kmod_seq[] = {
#if defined(RTCONFIG_SOC_IPQ40XX)
	{ "mem_manager", 1, 0, 0 },	/* If QCA WiFi configuration file has WIFI_MEM_MANAGER_SUPPORT=1 */
	{ "asf", 0, 0, 0 },
	{ "qdf", 0, 0, 0 },
	{ "ath_dfs", 0, 0, 0 },
	{ "ath_spectral", 0, 0, 0 },
	{ "umac", 0, 0, 2 },
	{ "ath_hal", 0, 0, 0 },
	{ "ath_rate_atheros", 0, 0, 0 },
	{ "hst_tx99", 0, 0, 0 },
	{ "ath_dev", 0, 0, 0 },
	/* { "qca_da", 0, 0, 0 }, */
	{ "qca_ol", 0, 0, 0 },
#else
	{ "asf", 0, 0, 0 },
	{ "adf", 0, 0, 0 },
	{ "ath_hal", 0, 0, 0 },
	{ "ath_rate_atheros", 0, 0, 0 },
	{ "ath_dfs", 0, 0, 0 },
	{ "ath_spectral", 0, 0, 0 },
	{ "hst_tx99", 0, 0, 0 },
	{ "ath_dev", 0, 0, 0 },
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	{ "umac", 0, 0, 2 },
#else
	{ "umac", 0, 0, 2 },
#endif
#endif
	/* { "ath_pktlog", 0, 0, 0 }, */
	/* { "smart_antenna", 0, 0, 0 }, */
};

static struct load_nat_accel_kmod_seq_s {
	char *kmod_name;
	unsigned int load_sleep;
	unsigned int remove_sleep;
} load_nat_accel_kmod_seq[] = {
#if defined(RTCONFIG_SOC_IPQ8064)
#if defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	{ "shortcut_fe_drv", 0, 0 },
#endif
	{ "ecm", 0, 0 },
#else
#if defined(RTCONFIG_SOC_IPQ40XX)
        { "shortcut_fe_cm", 0, 0 },
#else
	{ "shortcut_fe", 0, 0 },
	{ "fast_classifier", 0, 0 },
#endif /* IPQ40XX */
#endif
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
#if !defined(RTCONFIG_SOC_IPQ40XX)
	__mknod("/dev/sfe", S_IFCHR | 0660, makedev(252, 0)); // TBD
#endif
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	__mknod("/dev/rtkswitch", S_IFCHR | 0666, makedev(206, 0));
#endif
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	eval("ln", "-sf", "/dev/mtdblock2", "/dev/caldata");	/* mtdblock2 = SPI flash, Factory MTD partition */
#elif (defined(RTAC58U) || defined(RTAC82U))
	eval("ln", "-sf", "/dev/mtdblock3", "/dev/caldata");	/* mtdblock3 = cal in NAND flash, Factory MTD partition */
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
	case MODEL_RTAC88N:
		nvram_unset("vlan3hwname");
		if ((wans_cap && wanslan_cap)
		    )
			nvram_set("vlan3hwname", "et0");
		break;
	}
#endif
}

#if defined(RTCONFIG_SOC_IPQ8064)
void tweak_wifi_ps(const char *wif)
{
	if (!strncmp(wif, WIF_2G, strlen(WIF_2G)) || !strcmp(wif, VPHY_2G))
		set_iface_ps(wif, 1);	/* 2G: CPU0 only */
	else
		set_iface_ps(wif, 3);	/* 5G: CPU0 and CPU1 */
}
#endif

static void tweak_lan_wan_ps(void)
{
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2)
	/* Reference to qca-nss-drv.init */
	set_irq_smp_affinity(245, 2);	/* 1st nss irq, eth0, LAN */
	set_irq_smp_affinity(246, 2);	/* 2nd nss irq, eth1, LAN */
	set_irq_smp_affinity(264, 1);	/* 3rd nss irq, eth2, WAN0 */
	set_irq_smp_affinity(265, 1);	/* 4th nss irq, eth3, WAN1 */

	set_iface_ps("eth0", 2);
	set_iface_ps("eth1", 2);
	set_iface_ps("eth2", 1);
	set_iface_ps("eth3", 1);
#elif defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	/* Reference to qca-nss-drv.init */
	set_irq_smp_affinity(245, 1);	/* 1st nss irq, eth0, WAN0 */
	set_irq_smp_affinity(246, 2);	/* 2nd nss irq, eth1, LAN */
	set_irq_smp_affinity(264, 2);	/* 3rd nss irq, eth2, LAN */
	set_irq_smp_affinity(265, 1);	/* 4th nss irq, eth3, WAN1 */

	set_iface_ps("eth0", 1);
	set_iface_ps("eth1", 2);
	set_iface_ps("eth2", 2);
	set_iface_ps("eth3", 1);
#endif
}

static void init_switch_qca(void)
{
	char *qca_module_list[] = {
#if defined(RTCONFIG_SOC_IPQ8064)
#if defined(RTCONFIG_SWITCH_QCA8337N)
		"qca-ssdk",
#endif
		"qca-nss-gmac", "qca-nss-drv",
		"qca-nss-qdisc",
#elif defined(RTCONFIG_SOC_IPQ40XX)
		"qrfs",
		"shortcut-fe", "shortcut-fe-ipv6", "shortcut-fe-cm",
		"qca-ssdk",
		"essedma",
/*		"qca-nss-gmac", "qca-nss-drv",	*/
#if defined(RTCONFIG_HIVE_BLUEZ)
		"hyfi_qdisc", "hyfi-bridging",
#endif
#endif
#if defined(RTCONFIG_STRONGSWAN)
		"qca-nss-capwapmgr", "qca-nss-cfi-cryptoapi",
		"qca-nss-crypto-tool", "qca-nss-crypto",
		"qca-nss-profile-drv", "qca-nss-tun6rd",
		"qca-nss-tunipip6", "qca-nss-ipsec",
		"qca-nss-ipsecmgr", "qca-nss-cfi-ocf", 
#endif
		NULL
	}, **qmod;

	for (qmod = &qca_module_list[0]; *qmod != NULL; ++qmod) {
		if (module_loaded(*qmod))
			continue;

#if defined(RTCONFIG_SOC_IPQ40XX)
		if (!strcmp(*qmod, "shortcut-fe-cm")) {
			if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1)
				continue;
		}
		else if (!strcmp(*qmod, "essedma")) {
			if (nvram_get_int("jumbo_frame_enable")) {
				modprobe(*qmod, "overwrite_mode=1", "page_mode=1");
				continue;
			}
		}
#endif
		modprobe(*qmod);
	}

	char *wan0_ifname = nvram_safe_get("wan0_ifname");
	char *lan_ifname, *lan_ifnames, *ifname, *p;

#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	wan0_ifname = MII_IFNAME;
#endif

	tweak_lan_wan_ps();
	generate_switch_para();

	/* Set LAN MAC address to all LAN ethernet interface. */
	lan_ifname = nvram_safe_get("lan_ifname");
	if (!strncmp(lan_ifname, "br", 2) &&
	    !strstr(nvram_safe_get("lan_ifnames"), "bond"))
	{
		if ((lan_ifnames = strdup(nvram_safe_get("lan_ifnames"))) != NULL) {
			p = lan_ifnames;
			while ((ifname = strsep(&p, " ")) != NULL) {
				while (*ifname == ' ') ++ifname;
				if (!strcmp(ifname, lan_ifname))
					continue;
				if (!strncmp(ifname, WIF_2G, strlen(WIF_2G)) ||
				    !strncmp(ifname, WIF_5G, strlen(WIF_5G)))
					continue;
				if (*ifname == 0)
					break;
				eval("ifconfig", ifname, "hw", "ether", get_lan_hwaddr());
			}
			free(lan_ifnames);
		}
	}

	// TODO: replace to nvram controlled procedure later
	if (strlen(wan0_ifname)) {
		eval("ifconfig", wan0_ifname, "hw", "ether", get_wan_hwaddr());
	}
	config_switch();

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

#if defined(RTCONFIG_SOC_IPQ40XX)
	/* qrfs.init:
	 * enable Qualcomm Receiving Flow Steering (QRFS)
	 */
	f_write_string("/proc/qrfs/enable", "0", 0, 0); //for throughput, disable it
#endif
}

void enable_jumbo_frame(void)
{
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
#else
	int mtu = 1518;	/* default value */
	char mtu_str[] = "9000XXX";

	if (!nvram_contains_word("rc_support", "switchctrl"))
		return;

	if (nvram_get_int("jumbo_frame_enable"))
		mtu = 9000;

	sprintf(mtu_str, "%d", mtu);
#if defined(RTCONFIG_SOC_IPQ40XX)
	eval("ifconfig", "eth0", "mtu", mtu_str);
	eval("ifconfig", "eth1", "mtu", mtu_str);
	eval("ssdk_sh", "misc", "frameMaxSize", "set", mtu_str);
#else
	eval("swconfig", "dev", MII_IFNAME, "set", "max_frame_size", mtu_str);
#endif
#endif
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
	case MODEL_RTAC55UHP:	/* fall through */
	case MODEL_RT4GAC55U:	/* fall through */
	case MODEL_RTAC58U:	/* fall through */
	case MODEL_RTAC82U:	/* fall through */
	case MODEL_RTAC88N:	/* fall through */
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

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	if (is_routing_enabled()) {
		int wanscap_wanlan = get_wans_dualwan() & (WANSCAP_WAN | WANSCAP_LAN);
		int wans_lanport = nvram_get_int("wans_lanport");
		int wan_mask, unit;
		char cmd[64];
		char prefix[8], nvram_ports[20];

		if ((wanscap_wanlan & WANSCAP_LAN) && (wans_lanport < 0 || wans_lanport > 8)) {
			_dprintf("%s: invalid wans_lanport %d!\n", __func__, wans_lanport);
			wanscap_wanlan &= ~WANSCAP_LAN;
		}

		wan_mask = 0;
		if(wanscap_wanlan & WANSCAP_WAN)
			wan_mask |= 0x1 << 0;
		if(wanscap_wanlan & WANSCAP_LAN)
			wan_mask |= 0x1 << wans_lanport;
		sprintf(cmd, "rtkswitch 44 0x%08x", wan_mask);
		system(cmd);

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			sprintf(prefix, "%d", unit);
			sprintf(nvram_ports, "wan%sports_mask", (unit == WAN_UNIT_FIRST)? "" : prefix);
			nvram_unset(nvram_ports);
			if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
				/* BRT-AC828 LAN1 = P0, LAN2 = P1, etc */
				nvram_set_int(nvram_ports, (1 << (wans_lanport - 1)));
			}
		}
	}
#endif

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

		/* portmask in rtkswitch
		 * 	P9	P8	P7	P6	P5	P4	P3	P2	P1	P0
		 * 	MII-W	MII-L	-	-	-	WAN	LAN1	LAN2	LAN3	LAN4
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
			else if (!strcmp(nvram_safe_get("switch_wantag"), "movistar")) {
				system("rtkswitch 38 3");			/* IPTV/VoIP: P1/P0 */
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(6, 0, 0x02000210);
				/* IPTV:	untag: P1;   port: P1, P4 */
				__setup_vlan(3, 0, 0x00020012);
				/* VoIP:	untag: P0;   port: P0, P4 */
				__setup_vlan(2, 0, 0x00010011);
			}
			else if (!strcmp(nvram_safe_get("switch_wantag"), "meo")) {
				system("rtkswitch 40 1");			/* admin all frames on all ports */
				system("rtkswitch 38 1");			/* VoIP: P0 */
				/* Internet/VoIP:	untag: P9;   port: P0, P4, P9 */
				__setup_vlan(12, 0, 0x02000211);
			}
#if defined(RTAC58U)
			else if (!strcmp(nvram_safe_get("switch_wantag"), "stuff_fibre")) {
				system("rtkswitch 38 0");			//No IPTV and VoIP ports
				/* Internet:	untag: P9;   port: P4, P9 */
				__setup_vlan(10, 0, 0x02000210);
			}
#endif
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
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
			{
				char *str;

				str = nvram_get("lan_trunk_0");
				if(str != NULL && str[0] != '\0')
				{
					eval("rtkswitch", "45", "0");
					eval("rtkswitch", "46", str);
				}

				str = nvram_get("lan_trunk_1");
				if(str != NULL && str[0] != '\0')
				{
					eval("rtkswitch", "45", "1");
					eval("rtkswitch", "46", str);
				}
			}
#endif
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
	else if (access_point_mode())
	{
		if (merge_wan_port_into_lan_ports)
			eval("rtkswitch", "8", "100");
	}
#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
	else if (mediabridge_mode())
	{
		if (merge_wan_port_into_lan_ports)
			eval("rtkswitch", "8", "100");
	}
#endif

	dbG("link up wan port(s)\n");
	eval("rtkswitch", "114");	// link up wan port(s)

	enable_jumbo_frame();

#if defined(RTCONFIG_BLINK_LED)
	if (is_swports_bled("led_lan_gpio")) {
		update_swports_bled("led_lan_gpio", nvram_get_int("lanports_mask"));
	}
	if (is_swports_bled("led_wan_gpio")) {
		update_swports_bled("led_wan_gpio", nvram_get_int("wanports_mask"));
	}
#if defined(RTCONFIG_WANPORT2)
	if (is_swports_bled("led_wan2_gpio")) {
		update_swports_bled("led_wan2_gpio", nvram_get_int("wan1ports_mask"));
	}
#endif
#endif
}

int switch_exist(void)
{
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	int ret;
	unsigned int id[2];
	char *wan_ifname[2] = {
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2)
		"eth2", "eth3"	/* BRT-AC828 SR1 ~ SR3 wan0, wan1 interface. */
#elif defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
		"eth0", "eth3"	/* BRT-AC828 SR4 or above wan0, wan1 interface. */
#endif
	};

	ret = eval("rtkswitch", "41");
	_dprintf("eval(rtkswitch, 41) ret(%d)\n", ret);
	id[0] = (mdio_read(wan_ifname[0], MII_PHYSID1) << 16) | mdio_read(wan_ifname[0], MII_PHYSID2);
	id[1] = (mdio_read(wan_ifname[1], MII_PHYSID1) << 16) | mdio_read(wan_ifname[1], MII_PHYSID2);
	_dprintf("phy0/1 id %08x/%08x\n", id[0], id[1]);

	return (!ret && id[0] == 0x004dd074 && id[1] == 0x004dd074);
#elif defined(RTCONFIG_SOC_IPQ40XX)
//TBD
#else
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
#endif
}

/**
 * Low level function to load QCA WiFi driver.
 * @testmode:	if true, load WiFi driver as test mode which is required in ATE mode.
 */
static void __load_wifi_driver(int testmode)
{
	char country[FACTORY_COUNTRY_CODE_LEN + 1], code_str[6];
	const char *umac_params[] = {
		"vow_config", "OL_ACBKMinfree", "OL_ACBEMinfree", "OL_ACVIMinfree",
		"OL_ACVOMinfree", "ar900b_emu", "frac", "intval",
		"fw_dump_options", "enableuartprint", "ar900b_20_targ_clk",
		"max_descs", "qwrap_enable", "otp_mod_param", "max_active_peers",
		"enable_smart_antenna", "max_vaps", "enable_smart_antenna_da",
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
		"qca9888_20_targ_clk", "lteu_support",
		"atf_msdu_desc", "atf_peers", "atf_max_vdevs",
#endif
#if defined(RTCONFIG_SOC_IPQ40XX)
		"atf_msdu_desc", "atf_peers", "atf_max_vdevs",
#endif
		NULL
	}, **up;
	int i, code;
	char param[512], *s = &param[0], umac_nv[64], *val;
	char *argv[30] = {
		"modprobe", "-s", NULL
	}, **v;
	struct load_wifi_kmod_seq_s *p = &load_wifi_kmod_seq[0];
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	int olcfg = 0;
	char buf[16];
	const char *extra_pbuf_core0 = "0", *n2h_high_water_core0 = "8704", *n2h_wifi_pool_buf = "8576";
	int r, r0, r1, r2, l0, l1, l2;
#endif

#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	/* Always wait NSS ready whether NSS WiFi offloading is enabled or not. */
	for (i = 0, *buf = '\0'; i < 10; ++i) {
		r0 = f_read_string("/proc/sys/dev/nss/n2hcfg/n2h_high_water_core0", buf, sizeof(buf));
		if (r0 > 0 && strlen(buf) > 0)
			break;
		else {
			dbg(".");
			sleep(1);
		}
	}

	olcfg = (!!nvram_get_int("wl0_hwol") << 0) | (!!nvram_get_int("wl1_hwol") << 1);
	/* Always use maximum extra_pbuf_core0.
	 * Because it can't be changed if it is allocated, write non-zero value.
	 */
	extra_pbuf_core0 = "5939200";
	if (olcfg == 1 || olcfg == 2) {
		n2h_high_water_core0 = "43008";
		n2h_wifi_pool_buf = "20224";
	} else if (olcfg) {
		n2h_high_water_core0 = "59392";
		n2h_wifi_pool_buf = "36608";
	}
	l0 = strlen(extra_pbuf_core0);
	l1 = strlen(n2h_high_water_core0);
	l2 = strlen(n2h_wifi_pool_buf);
	for (i = 0; olcfg && i < 10; ++i) {
		f_read_string("/proc/sys/dev/nss/n2hcfg/n2h_high_water_core0", buf, sizeof(buf));

		*buf = '\0';
		r0 = l0;
		r = f_read_string("/proc/sys/dev/nss/general/extra_pbuf_core0", buf, sizeof(buf));
		if (r > 0 && atol(buf)) {
			dbg("%s: extra_pbuf_core0 is allocated!!! [%s]\n", __func__, buf);
		}
		if (r <= 0 || (r > 0 && atol(buf) != atol(extra_pbuf_core0)))
			r0 = f_write_string("/proc/sys/dev/nss/general/extra_pbuf_core0", extra_pbuf_core0, 0, 0);

		r1 = f_write_string("/proc/sys/dev/nss/n2hcfg/n2h_high_water_core0", n2h_high_water_core0, 0, 0);
		r2 = f_write_string("/proc/sys/dev/nss/n2hcfg/n2h_wifi_pool_buf", n2h_wifi_pool_buf, 0, 0);
		if (r0 < l0 || r1 < l1 || r2 < l2) {
			dbg(".");
			sleep(1);
			continue;
		}
		break;
	}
#endif

	for (i = 0, p = &load_wifi_kmod_seq[i]; i < ARRAY_SIZE(load_wifi_kmod_seq); ++i, ++p) {
		if (module_loaded(p->kmod_name))
			continue;

		v = &argv[2];
		*v++ = p->kmod_name;
		*param = '\0';
		s = &param[0];
#if defined(RTCONFIG_WIFI_QCA9557_QCA9882) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X)
		if (!strcmp(p->kmod_name, "ath_hal")) {
			int ce_level = nvram_get_int("ce_level");
			if (ce_level <= 0)
				ce_level = 0xce;

			*v++ = s;
			s += sprintf(s, "ce_level=%d", ce_level);
			s++;
		}
#endif
		if (!strcmp(p->kmod_name, "umac")) {
			if (!testmode) {
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
				*v++ = "msienable=0";	/* FIXME: Enable MSI interrupt in future. */
				for (up = &umac_params[0]; *up != NULL; up++) {
					snprintf(umac_nv, sizeof(umac_nv), "qca_%s", *up);
					if (!(val = nvram_get(umac_nv)))
						continue;
					*v++ = s;
					s += sprintf(s, "%s=%s", *up, val);
					s++;
				}
#endif

#ifdef RTCONFIG_AIR_TIME_FAIRNESS
				if (nvram_match("wl0_atf", "1") || nvram_match("wl1_atf", "1")) {
					*v++ = s;
					s += sprintf(s, "atf_mode=1");
					s++;
				}
#endif

#if !defined(RTCONFIG_SOC_IPQ40XX)
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
				*v++ = s;
				s += sprintf(s, "nss_wifi_olcfg=%d", olcfg);
				s++;
#endif
#endif

#if defined(RTCONFIG_SOC_IPQ40XX)
				if (get_meminfo_item("MemTotal") <= 131072) {
					f_write_string("/proc/net/skb_recycler/flush", "1", 0, 0);
					f_write_string("/proc/net/skb_recycler/max_skbs", "10", 0, 0);
					f_write_string("/proc/net/skb_recycler/max_spare_skbs", "10", 0, 0);
					/* *v++ = "low_mem_system=1"; obsoleted in new driver */
				}
#endif
			}
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
			else {
				*v++ = "testmode=1";
				*v++ = "ahbskip=1";
			}
#endif
		}

#if defined(RTCONFIG_SOC_IPQ40XX)
		if (!strcmp(p->kmod_name, "qca_ol")) {
			if (!testmode) {
				for (up = &umac_params[0]; *up != NULL; up++) {
					snprintf(umac_nv, sizeof(umac_nv), "qca_%s", *up);
					if (!(val = nvram_get(umac_nv)))
						continue;
					*v++ = s;
					s += sprintf(s, "%s=%s", *up, val);
					s++;
				}
			}
			else {
				*v++ = "testmode=1";
			}
		}
#endif

#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
		if (!strcmp(p->kmod_name, "adf")) {
			if (nvram_get("qca_prealloc_disabled") != NULL) {
				*v++ = s;
				s += sprintf(s, "prealloc_disabled=%d", nvram_get_int("qca_prealloc_disabled"));
				s++;
			}
		}
#endif

		*v++ = NULL;
		_eval(argv, NULL, 0, NULL);

		if (p->load_sleep)
			sleep(p->load_sleep);
	}

	if (!testmode) {
		//sleep(2);
#if defined(RTCONFIG_WIFI_QCA9557_QCA9882) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X)
		eval("iwpriv", (char*) VPHY_2G, "disablestats", "0");
		eval("iwpriv", (char*) VPHY_5G, "enable_ol_stats", "0");
#elif defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
		eval("iwpriv", (char*) VPHY_2G, "enable_ol_stats", "0");
		eval("iwpriv", (char*) VPHY_5G, "enable_ol_stats", "0");
#endif

		strncpy(country, nvram_safe_get("wl0_country_code"), FACTORY_COUNTRY_CODE_LEN);
		country[FACTORY_COUNTRY_CODE_LEN] = '\0';
		code = country_to_code(country, 2);
		if (code < 0)
			code = country_to_code("DB", 2);
		sprintf(code_str, "%d", code);
		eval("iwpriv", (char*) VPHY_2G, "setCountryID", code_str);
		strncpy(code_str, nvram_safe_get("wl0_txpower"), sizeof(code_str)-1);
		eval("iwpriv", (char*) VPHY_2G, "txpwrpc", code_str);

		strncpy(country, nvram_safe_get("wl1_country_code"), FACTORY_COUNTRY_CODE_LEN);
		country[FACTORY_COUNTRY_CODE_LEN] = '\0';
		code = country_to_code(country, 5);
		if (code < 0)
			code = country_to_code("DB", 5);
		sprintf(code_str, "%d", code);
		eval("iwpriv", (char*) VPHY_5G, "setCountryID", code_str);
		strncpy(code_str, nvram_safe_get("wl1_txpower"), sizeof(code_str)-1);
		eval("iwpriv", (char*) VPHY_5G, "txpwrpc", code_str);

#if defined(BRTAC828)
		set_irq_smp_affinity(68, 1);	/* wifi0 = 2G ==> CPU0 */
		set_irq_smp_affinity(90, 2);	/* wifi1 = 5G ==> CPU1 */
#elif defined(RTCONFIG_SOC_IPQ40XX)
		set_irq_smp_affinity(200, 4);	/* wifi0 = 2G ==> core 3 */
#if defined(RTAC82U)
		set_irq_smp_affinity(174, 2);	/* wifi1 = 5G ==> core 2 */
#else
		set_irq_smp_affinity(201, 8);	/* wifi1 = 5G ==> core 4 */
#endif
		f_write_string("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "performance", 0, 0); // for throughput
#if 0 // before 1.1CSU3
		f_write_string("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "710000", 0, 0); // for throughput
#else // new QSDK 1.1CSU3
		f_write_string("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "716000", 0, 0); // for throughput
#endif
#endif
#if defined(RTCONFIG_SOC_IPQ8064)
		tweak_wifi_ps(VPHY_2G);
		tweak_wifi_ps(VPHY_5G);
#endif
	}
}

void load_wifi_driver(void)
{
	__load_wifi_driver(0);
}

void load_testmode_wifi_driver(void)
{
	__load_wifi_driver(1);
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
		load_wifi_driver();
		sleep(2);

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
					dbG("\ncreate a wifi node %s from %s\n", ifname, get_vphyifname(unit));
					doSystem("wlanconfig %s create wlandev %s wlanmode ap", ifname, get_vphyifname(unit));
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
			doSystem("wlanconfig %s create wlandev %s wlanmode sta nosbeacon",
				get_staifname(wlc_band), get_vphyifname(wlc_band));
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
	int i;
	struct load_wifi_kmod_seq_s *wp;
#if defined(RTCONFIG_QCA) && defined(RTCONFIG_SOC_IPQ40XX)
        if(nvram_get_int("restwifi_qis")==0) //reduce the finish time of QIS 
        {
		nvram_set("restwifi_qis","1");
		nvram_commit();
		bg=1;
                return ;
        }
	else
		bg=0;
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
			doSystem("wlanconfig %s destroy", get_staifname(wlc_band));
		}      
#endif
	create_node=0;

	eval("ifconfig", (char*) VPHY_2G, "down");
	eval("ifconfig", (char*) VPHY_5G, "down");
	for (i = ARRAY_SIZE(load_wifi_kmod_seq)-1, wp = &load_wifi_kmod_seq[i]; i >= 0; --i, --wp) {
		if (!module_loaded(wp->kmod_name))
			continue;

		if (!wp->stick) {
			modprobe_r(wp->kmod_name);
			if (wp->remove_sleep)
				sleep(wp->remove_sleep);
		}
	}
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
#ifdef RTCONFIG_32BYTES_ODMPID
	char modelname[32];
#else
	char modelname[16];
#endif
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

#if defined(RTCONFIG_SOC_IPQ8064)
	/* Hack et0macaddr/et1macaddr after MAC address checking of wl_mssid. */
	ether_atoe(macaddr, buffer);
	buffer[5] += 1;
	ether_etoa(buffer, macaddr);
	ether_atoe(macaddr2, buffer);
	buffer[5] += 1;
	ether_etoa(buffer, macaddr2);
#endif

#if defined(PLN12) || defined(PLAC56)
	/* set et1macaddr the same as et0macaddr (for cpu connect to switch only use single RGMII) */
	strcpy(macaddr2, macaddr);
#endif

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

#if defined(RTCONFIG_FITFDT)
	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);
	strncpy(productid, rt_buildname, 12);
#else
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
#endif

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
		if ((buffer[0] == 0xff)|| !strcmp(buffer,"NONE"))
			nvram_set("wifi_psk", "");
		else
			nvram_set("wifi_psk", buffer);
	}
#if defined(RTAC58U)
	if (!strncmp(nvram_safe_get("territory_code"), "CX", 2))
		nvram_set("wifi_psk", nvram_safe_get("secret_code"));
#endif
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

/**
 * Generate interface name based on @band and @subunit. (@subunit is NOT y in wlX.Y)
 * @band:
 * @subunit:
 * @buf:
 * @return:
 */
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

/**
 * Input @band and @ifname and return Y of wlX.Y.
 * Last digit of VAP interface name of guest is NOT always equal to Y of wlX.Y,
 * if guest network is not enabled continuously.
 * @band:
 * @ifname:	ath0, ath1, ath001, ath002, ath103, etc
 * @return:	index of guest network configuration. (wlX.Y: X = @band, Y = @return)
 * 		If both main 2G/5G, 1st/3rd 2G guest network, and 2-nd 5G guest network are enabled,
 * 		return value should as below:
 * 		ath0:	0
 * 		ath001:	1
 * 		ath002: 3
 * 		ath1:	0
 * 		ath101: 2
 */
int get_wlsubnet(int band, const char *ifname)
{
	int subnet, sidx;
	char buf[32];

	for (subnet = 0, sidx = 0; subnet < MAX_NO_MSSID; subnet++)
	{
		if(!nvram_match(wl_nvname("bss_enabled", band, subnet), "1")) {
			if (!subnet)
				sidx++;
			continue;
		}

		if(strcmp(ifname, __get_wlifname(band, sidx, buf)) == 0)
			return subnet;

		sidx++;
	}
	return -1;
}

#if defined(RTCONFIG_SOC_QCA9557) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X) || defined(RTCONFIG_SOC_IPQ40XX)
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
	struct load_nat_accel_kmod_seq_s *p = &load_nat_accel_kmod_seq[0];
#if defined(RTCONFIG_DUALWAN)
	int nat_x = -1, l, t, link_wan = 1, link_wans_lan = 1;
	int wans_cap = get_wans_dualwan() & WANSCAP_WAN;
	int wanslan_cap = get_wans_dualwan() & WANSCAP_LAN;
	char nat_x_str[] = "wanX_nat_xXXXXXX";
#endif

#if  defined(RTCONFIG_SOC_IPQ40XX) 
	act = nvram_get_int("qca_sfe");	
#else
	if (!nvram_get_int("qca_sfe"))
		return;
#endif

#if  defined(RTCONFIG_SOC_IPQ40XX) 
	if(nvram_get_int("url_enable_x")==1 && strlen(nvram_get("url_rulelist"))!=0)
		act = 0;

	if(nvram_get_int("keyword_enable_x")==1 && strlen(nvram_get("keyword_rulelist"))!=0)
		act = 0;
#endif

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


	for (i = 0, p = &load_nat_accel_kmod_seq[i]; i < ARRAY_SIZE(load_nat_accel_kmod_seq); ++i, ++p) {
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
			{

#if defined(RTCONFIG_SOC_IPQ40XX)
				if(nvram_get_int("MULTIFILTER_ENABLE")==1 &&
				   !strcmp(p->kmod_name,"shortcut_fe_cm"))
					modprobe_r("shortcut_fe_cm");
				else				
#endif
					continue;
			}
			
			modprobe(p->kmod_name);
			if (p->load_sleep)
				sleep(p->load_sleep);
		}			
	}
}
#endif	/* RTCONFIG_SOC_QCA9557 || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X) || defined(RTCONFIG_SOC_IPQ40XX) */

#if defined(RTCONFIG_SOC_IPQ8064)
#define IPV46_CONN	4096
// ecm kernel module must be loaded before bonding interface creation!
// only qca solution can reload it dynamically
// only happened when qca_sfe=1
// only loaded when unloaded, and unloaded when loaded
// in restart_firewall for fw_pt_l2tp/fw_pt_ipsec
// in restart_qos for qos_enable
// in restart_wireless for wlx_mrate_x, etc
void reinit_ecm(int unit)
{
	int act = nvram_get_int("qca_sfe"), i, r;	/* -1/0/otherwise: ignore/remove ecm/load ecm */
	struct load_nat_accel_kmod_seq_s *p = &load_nat_accel_kmod_seq[0];
	const char *val = "1";
	char tmp[16], ipv46_conn[16];

	/* If QoS is enabled, disable ecm. */
	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1)
		act = 0;

	if (act > 0) {
		/* FIXME: IPTV, ISP profile, USB modem, etc. */
	}

	_dprintf("%s: nat_x %d qos %d: action %d.\n", __func__,
		nvram_get_int("wan0_nat_x"), nvram_get_int("qos_enable"), act);

	if (act < 0)
		return;

	if (!act) {
		/* remove ecm */
		for (i = ARRAY_SIZE(load_nat_accel_kmod_seq) - 1, p = &load_nat_accel_kmod_seq[i]; i >= 0; --i, --p) {
			if (!module_loaded(p->kmod_name))
				continue;

			modprobe_r(p->kmod_name);
			if (p->remove_sleep)
				sleep(p->load_sleep);
		}
	} else {
		/* load ecm */
		for (i = 0, p = &load_nat_accel_kmod_seq[i]; i < ARRAY_SIZE(load_nat_accel_kmod_seq); ++i, ++p) {
			if (module_loaded(p->kmod_name))
				continue;

			modprobe(p->kmod_name);
			if (p->load_sleep)
				sleep(p->load_sleep);
		}
	}

	if (!act)
		val = "0";

	/* Enable NSS RPS */
	f_write_string("/proc/sys/dev/nss/general/rps", "1", 0, 0);

	/* qca-nss-drv.sysctl: Default Number of connection configuration */
	snprintf(ipv46_conn, sizeof(ipv46_conn), "%d", IPV46_CONN);
	*tmp = '\0';
	r = f_read_string("/proc/sys/dev/nss/ipv4cfg/ipv4_conn", tmp, sizeof(tmp));
	if (r > 0 && atoi(tmp) != IPV46_CONN) {
		r = f_write_string("/proc/sys/dev/nss/ipv4cfg/ipv4_conn", ipv46_conn, 0, 0);
	}

	*tmp = '\0';
	r = f_read_string("/proc/sys/dev/nss/ipv6cfg/ipv6_conn", tmp, sizeof(tmp));
	if (r > 0 && atoi(tmp) != IPV46_CONN) {
		r = f_write_string("/proc/sys/dev/nss/ipv6cfg/ipv6_conn", ipv46_conn, 0, 0);
	}

	/* qca-nss-ecm.init: */
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", val, 0, 0);
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", val, 0, 0);

	/* qca-nss-ecm.sysctl:
	 * enable bridge nf options so the nss conntrack can detect these.
	 */
	f_write_string("/proc/sys/dev/nss/general/redirect", val, 0, 0);
}
#endif	/* RTCONFIG_SOC_IPQ8064 */

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
#if 1
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
#endif
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
	case MODEL_RTAC58U:
	case MODEL_RTAC82U:
	case MODEL_RTAC88N:
		ifconfig(interface, IFUP, 0, 0);
		if(wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan port PRIO */
		if(nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
#if defined(RTAC58U)
		if (nvram_match("switch_wantag", "stuff_fibre"))
			eval("vconfig", "set_egress_map", wan_dev, "3", "5");
#endif
		break;
	}
}

int start_thermald(void)
{
	char *thermald_argv[] = {"thermald", "-c", "/etc/thermal/ipq-thermald-8064.conf", NULL};
	pid_t pid;

	return _eval(thermald_argv, NULL, 0, &pid);
}
