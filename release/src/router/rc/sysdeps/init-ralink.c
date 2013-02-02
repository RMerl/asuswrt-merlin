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
#endif

#ifdef RTCONFIG_RALINK_RT3052
#include <ra3052.h>
#endif

void init_devs(void)
{
	mknod("/dev/video0", S_IFCHR | 0x666, makedev(81, 0));
#ifdef RTCONFIG_DSL
	mknod("/dev/rtl8367r", S_IFCHR | 0x666, makedev(206, 0));
#else
	mknod("/dev/rtl8367m", S_IFCHR | 0x666, makedev(206, 0));
#endif
	mknod("/dev/spiS0", S_IFCHR | 0x666, makedev(217, 0));
	mknod("/dev/i2cM0", S_IFCHR | 0x666, makedev(218, 0));
	mknod("/dev/rdm0", S_IFCHR | 0x666, makedev(254, 0));
	mknod("/dev/flash0", S_IFCHR | 0x666, makedev(200, 0));
	mknod("/dev/swnat0", S_IFCHR | 0x666, makedev(210, 0));
	mknod("/dev/hwnat0", S_IFCHR | 0x666, makedev(220, 0));
	mknod("/dev/acl0", S_IFCHR | 0x666, makedev(230, 0));
	mknod("/dev/ac0", S_IFCHR | 0x666, makedev(240, 0));
	mknod("/dev/mtr0", S_IFCHR | 0x666, makedev(250, 0));
	mknod("/dev/gpio0", S_IFCHR | 0x666, makedev(252, 0));
	mknod("/dev/nvram", S_IFCHR | 0x666, makedev(228, 0));
	mknod("/dev/PCM", S_IFCHR | 0x666, makedev(233, 0));
	mknod("/dev/I2S", S_IFCHR | 0x666, makedev(234, 0));

	modprobe("nvram_linux");
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
	}
}

void init_switch_ralink()
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
	config_switch();
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
//	reinit_hwnat();
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


int config_switch_for_first_time = 1;
void config_switch()
{
	int stbport;
	int controlrate_unknown_unicast;
	int controlrate_unknown_multicast;
	int controlrate_multicast;
	int controlrate_broadcast;

	dbG("link down all ports\n");
	eval("8367m", "17");	// link down all ports

	if (config_switch_for_first_time)
		config_switch_for_first_time = 0;
	else
	{
		dbG("software reset\n");
		eval("8367m", "27");	// software reset
	}

	if (is_routing_enabled())
	{
		stbport = atoi(nvram_safe_get("switch_stb_x"));
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);

		if(!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", ""))//2012.03 Yau modify
		{
			char tmp[128];
			int voip_port = 0;
			int vlan_val;/* VID and PRIO */

//			voip_port = atoi(nvram_safe_get("voip_port"));
			voip_port = 3;
			if (voip_port < 0 || voip_port > 4)
				voip_port = 0;		

			/* Fixed Ports Now*/
			stbport = 4;	
			voip_port = 3;
	
			sprintf(tmp, "8367m 29 %d", voip_port);	
			system(tmp);	

			if(!strncmp(nvram_safe_get("switch_wantag"), "unifi", 5))/* Added for Unifi. Cherry Cho modified in 2011/6/28.*/
			{
				if(strstr(nvram_safe_get("switch_wantag"), "home"))
				{					
					system("8367m 38 1");/* IPTV: P0 */
					/* Internet */
					system("8367m 36 500");
					system("8367m 37 0");
					system("8367m 39 51249950");
					/* IPTV */
					system("8367m 36 600");
					system("8367m 37 0");
					system("8367m 39 65553");			
				}
				else/* No IPTV. Business package */
				{
					/* Internet */
					system("8367m 38 0");
					system("8367m 36 500");
					system("8367m 37 0");
					system("8367m 39 51315487");
				}
			}
			else if(!strncmp(nvram_safe_get("switch_wantag"), "singtel", 7))/* Added for SingTel's exStream issues. Cherry Cho modified in 2011/7/19. */
			{		
				if(strstr(nvram_safe_get("switch_wantag"), "mio"))/* Connect Singtel MIO box to P3 */
				{
					system("8367m 40 1");
					system("8367m 38 3");/* IPTV: P0  VoIP: P1 */
					/* Internet */
					system("8367m 36 10");
					system("8367m 37 0");
					system("8367m 39 51118876");
					/* VoIP */
					system("8367m 36 30");
					system("8367m 37 4");				
					system("8367m 39 18");//VoIP Port: P1 tag
				}
				else//Connect user's own ATA to lan port and use VoIP by Singtel WAN side VoIP gateway at voip.singtel.com
				{
					system("8367m 38 1");/* IPTV: P0 */
					/* Internet */
					system("8367m 36 10");
					system("8367m 37 0");
					system("8367m 39 51249950");			
				}

				/* IPTV */
				system("8367m 36 20");
				system("8367m 37 4");
				system("8367m 39 65553");	
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "m1_fiber"))//VoIP: P1 tag. Cherry Cho added in 2012/1/13.
			{
				system("8367m 40 1");
				system("8367m 38 2");/* VoIP: P1  2 = 0x10 */
				/* Internet */
				system("8367m 36 1103");
				system("8367m 37 1");
				system("8367m 39 51184413");
				/* VoIP */
				system("8367m 36 1107");
				system("8367m 37 1");				
				system("8367m 39 18");//VoIP Port: P1 tag
			}
			else/* Cherry Cho added in 2011/7/11. */
			{
				/* Initialize VLAN and set Port Isolation */
				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") && strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("8367m 38 3");// 3 = 0x11 IPTV: P0  VoIP: P1
				else if(strcmp(nvram_safe_get("switch_wan1tagid"), ""))
					system("8367m 38 1");// 1 = 0x01 IPTV: P0 
				else if(strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("8367m 38 2");// 2 = 0x10 VoIP: P1
				else
					system("8367m 38 0");//No IPTV and VoIP ports

				/*++ Get and set Vlan Information */
				if(strcmp(nvram_safe_get("switch_wan0tagid"), "") != 0)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan0tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{											
						sprintf(tmp, "8367m 36 %d", vlan_val);
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan0prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan0prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "8367m 37 %d", vlan_val);
							system(tmp);
						}
						else
							system("8367m 37 0");
					}

					if(strcmp(nvram_safe_get("switch_wan1tagid"), "") && strcmp(nvram_safe_get("switch_wan2tagid"), ""))
						system("8367m 39 51118876");//5118876 = 0x30C 031C Port: P2 P3 P4 P8 P9 Untag: P2 P3 P8 P9
					else if(strcmp(nvram_safe_get("switch_wan1tagid"), ""))
						system("8367m 39 51249950");//51249950 = 0x30E 031E Port: P1 P2 P3 P4 P8 P9 Untag: P1 P2 P3 P8 P9
					else if(strcmp(nvram_safe_get("switch_wan2tagid"), ""))
						system("8367m 39 51184413");//51184413 = 0x30D 031D Port: P0 P2 P3 P4 P8 P9 Untag: P0 P2 P3 P8 P9
					else
						system("8367m 39 51315487");//51315487 = 0x30F 031F
				}

				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") != 0)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan1tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{								
						sprintf(tmp, "8367m 36 %d", vlan_val);	
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan1prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan1prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "8367m 37 %d", vlan_val);	
							system(tmp);
						}
						else
							system("8367m 37 0");
					}	

					system("8367m 39 65553");//IPTV Port: P0 untag 65553 = 0x10 011
				}	


				if(strcmp(nvram_safe_get("switch_wan2tagid"), "") != 0)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan2tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{					
						sprintf(tmp, "8367m 36 %d", vlan_val);	
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan2prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan2prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "8367m 37 %d", vlan_val);	
							system(tmp);		
						}
						else
							system("8367m 37 0");
					}
	
					system("8367m 39 131090");//VoIP Port: P1 untag
				}

			}
		}
		else
		{
			switch(stbport)
			{
				case 1:	// LLLWW
					system("8367m 8 1");
					break;
				case 2:	// LLWLW
					system("8367m 8 2");
					break;
				case 3:	// LWLLW
					system("8367m 8 3");
					break;
				case 4:	// WLLLW
					system("8367m 8 4");
					break;
				case 5:	// WWLLW
					system("8367m 8 5");
					break;
				case 6: // LLWWW
					system("8367m 8 6");
					break;
				default:// LLLLW
	//				system("8367m 8 0");
					break;
			}
		}

		/* unknown unicast storm control */
		if (!nvram_get("switch_ctrlrate_unknown_unicast"))
			controlrate_unknown_unicast = 0;
		else
			controlrate_unknown_unicast = atoi(nvram_get("switch_ctrlrate_unknown_unicast"));
		if (controlrate_unknown_unicast < 0 || controlrate_unknown_unicast > 1024)
			controlrate_unknown_unicast = 0;
		if (controlrate_unknown_unicast)
			eval("8367m", "22", controlrate_unknown_unicast);
	
		/* unknown multicast storm control */
		if (!nvram_get("switch_ctrlrate_unknown_multicast"))
			controlrate_unknown_multicast = 0;
		else
			controlrate_unknown_multicast = atoi(nvram_get("switch_ctrlrate_unknown_multicast"));
		if (controlrate_unknown_multicast < 0 || controlrate_unknown_multicast > 1024)
			controlrate_unknown_multicast = 0;
		if (controlrate_unknown_multicast)
			eval("8367m", "23", controlrate_unknown_multicast);
	
		/* multicast storm control */
		if (!nvram_get("switch_ctrlrate_multicast"))
			controlrate_multicast = 0;
		else
			controlrate_multicast = atoi(nvram_get("switch_ctrlrate_multicast"));
		if (controlrate_multicast < 0 || controlrate_multicast > 1024)
			controlrate_multicast = 0;
		if (controlrate_multicast)
			eval("8367m", "24", controlrate_multicast);
	
		/* broadcast storm control */
		if (!nvram_get("switch_ctrlrate_broadcast"))
			controlrate_broadcast = 0;
		else
			controlrate_broadcast = atoi(nvram_get("switch_ctrlrate_broadcast"));
		if (controlrate_broadcast < 0 || controlrate_broadcast > 1024)
			controlrate_broadcast = 0;
		if (controlrate_broadcast)
			eval("8367m", "25", controlrate_broadcast);
	}

	dbG("link up all ports\n");
	eval("8367m", "16");	// link up all ports
}

int
switch_exist(void)
{
	int ret;
#ifdef RTCONFIG_DSL
	// 0 means switch exist
	ret = 0;
#else
	ret = eval("8367m", "41");
	_dprintf("eval(8367m, 41) ret(%d)\n", ret);
#endif
	return (ret == 0);
}

void init_wl(void)
{
	if (!is_module_loaded("rt2860v2_ap"))
		modprobe("rt2860v2_ap");

	if (!is_module_loaded("RT3090_ap_util"))
	{
		modprobe("RT3090_ap_util");
		modprobe("RT3090_ap");
		modprobe("RT3090_ap_net");
	}

	sleep(1);
}

void fini_wl(void)
{
	if (is_module_loaded("hw_nat"))
		modprobe_r("hw_nat");

	if (is_module_loaded("RT3090_ap_util"))
	{
		modprobe_r("RT3090_ap_net");
		modprobe_r("RT3090_ap");
		modprobe_r("RT3090_ap_util");
	}

	if (is_module_loaded("rt2860v2_ap"))
		modprobe_r("rt2860v2_ap");
}

void init_syspara(void)
{
	unsigned char buffer[16];
	unsigned int *src;
	unsigned int *dst;
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

	nvram_set("buildno", rt_serialno);
	nvram_set("extendno", rt_extendno);
	nvram_set("buildinfo", rt_buildinfo);

	/* /dev/mtd/2, RF parameters, starts from 0x40000 */
	dst = (unsigned int *)buffer;
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

	if (FRead(dst, OFFSET_MAC_ADDR_2G, bytes)<0)
	{
		_dprintf("READ MAC address 2G: Out of scope\n");
	}
	else
	{
		if (buffer[0]!=0xff)
			ether_etoa(buffer, macaddr2);
	}

	if (!ralink_mssid_mac_validate(macaddr) || !ralink_mssid_mac_validate(macaddr2))
		nvram_set("wl_mssid", "0");
	else
		nvram_set("wl_mssid", "1");

	//TODO: separate for different chipset solution
	nvram_set("et0macaddr", macaddr);
	nvram_set("et1macaddr", macaddr2);
	

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
	dst = (unsigned int *)country_code;
	bytes = 2;
	if (FRead(dst, OFFSET_COUNTRY_CODE, bytes)<0)
	{
		_dprintf("READ ASUS country code: Out of scope\n");
		nvram_set("wl_country_code", "");
	}
	else
	{
		if ((unsigned char)country_code[0]!=0xff)
		{
			nvram_set("wl_country_code", country_code);
			nvram_set("wl0_country_code", country_code);
			nvram_set("wl1_country_code", country_code);
		}
		else
		{
			nvram_set("wl_country_code", "DB");
			nvram_set("wl0_country_code", "DB");
			nvram_set("wl1_country_code", "DB");
		}

		if (!strcasecmp(nvram_safe_get("wl_country_code"), "BR"))
		{
			nvram_set("wl_country_code", "UZ");
			nvram_set("wl0_country_code", "UZ");
			nvram_set("wl1_country_code", "UZ");
		}

		if (nvram_match("wl_country_code", "HK") && nvram_match("preferred_lang", ""))
			nvram_set("preferred_lang", "TW");
	}

	/* reserved for Ralink. used as ASUS pin code. */
	dst = (unsigned int *)pin;
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

	src = 0x50020;	/* /dev/mtd/3, firmware, starts from 0x50000 */
	dst = (unsigned int *)buffer;
	bytes = 16;
	if (FRead(dst, src, bytes)<0)
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

	memset(buffer, 0, sizeof(buffer));
	FRead(buffer, OFFSET_BOOT_VER, 4);
//	sprintf(blver, "%c.%c.%c.%c", buffer[0], buffer[1], buffer[2], buffer[3]);
	sprintf(blver, "%s-0%c-0%c-0%c-0%c", trim_r(productid), buffer[0], buffer[1], buffer[2], buffer[3]);
	nvram_set("blver", trim_r(blver));

	_dprintf("bootloader version: %s\n", nvram_safe_get("blver"));
	_dprintf("firmware version: %s\n", nvram_safe_get("firmver"));

	dst = (unsigned int *)txbf_para;
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

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);
}

void generate_wl_para(int unit, int subunit)
{
}

int is_module_loaded(const char *module)
{
	DIR *dir_to_open = NULL;
	char sys_path[128];

	sprintf(sys_path, "/sys/module/%s", module);
	dir_to_open = opendir(sys_path);
	if(dir_to_open)
	{
		closedir(dir_to_open);
		return 1;
	}

	return 0;
}

// only ralink solution can reload it dynamically
void reinit_hwnat()
{
	// only happened when hwnat=1
	// only loaded when unloaded, and unloaded when loaded
	// in restart_firewall for fw_pt_l2tp/fw_pt_ipsec
	// in restart_qos for qos_enable
	// in restart_wireless for wlx_mrate_x
	
	if (nvram_get_int("hwnat")) {
		if (is_nat_enabled() && !nvram_get_int("qos_enable") &&
			/* TODO: consider RTCONFIG_DUALWAN case */
//			!nvram_match("wan0_proto", "l2tp") &&
//			!nvram_match("wan0_proto", "pptp") &&
			(nvram_match("wl0_radio", "0") || !nvram_get_int("wl0_mrate_x")) &&
			(nvram_match("wl1_radio", "0") || !nvram_get_int("wl1_mrate_x"))) {
			if (!is_module_loaded("hw_nat")) {
#if 0
				system("echo 2 > /proc/sys/net/ipv4/conf/default/force_igmp_version");
				system("echo 2 > /proc/sys/net/ipv4/conf/all/force_igmp_version");
#endif
				modprobe("hw_nat");
				sleep(1);
			}
		}	
		else if (is_module_loaded("hw_nat")) {
			modprobe_r("hw_nat");
			sleep(1);
#if 0
			system("echo 0 > /proc/sys/net/ipv4/conf/default/force_igmp_version");
			system("echo 0 > /proc/sys/net/ipv4/conf/all/force_igmp_version");
#endif
		}
	}
}

char *get_wlifname(int unit, int subunit, int subunit_x, char *buf)
{
	char wifbuf[32];
	char prefix[]="wlXXXXXX_", tmp[100];

	memset(wifbuf, 0, sizeof(wifbuf));

	if(unit==0) strncpy(wifbuf, WIF_2G, strlen(WIF_2G)-1);
	else strncpy(wifbuf, WIF_5G, strlen(WIF_5G)-1);

	snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		sprintf(buf, "%s%d", wifbuf, subunit_x);
	else
		sprintf(buf, "");
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

