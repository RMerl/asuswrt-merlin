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

#if defined(RTN14U)
#ifdef HWNAT_FIX
#include <sys/shm.h>
#include <sys/ipc.h>
int shm_client_info_id;
void *shared_client_info=(void *) 0;
int *hwnat_rst_unlock=0;
#endif
#endif 

void init_devs(void)
{
#define MKNOD(name,mode,dev)	if(mknod(name,mode,dev)) perror("## mknod " name)

#if defined(LINUX30) && !defined(RTN14U)
	/* Below device node are used by proprietary driver.
	 * Thus, we cannot use GPL-only symbol to create/remove device node dynamically.
	 */
	MKNOD("/dev/swnat0", S_IFCHR | 0x666, makedev(210, 0));
	MKNOD("/dev/hwnat0", S_IFCHR | 0x666, makedev(220, 0));
	MKNOD("/dev/acl0", S_IFCHR | 0x666, makedev(230, 0));
	MKNOD("/dev/ac0", S_IFCHR | 0x666, makedev(240, 0));
	MKNOD("/dev/mtr0", S_IFCHR | 0x666, makedev(250, 0));
	MKNOD("/dev/rtkswitch", S_IFCHR | 0x666, makedev(206, 0));
	MKNOD("/dev/nvram", S_IFCHR | 0x666, makedev(228, 0));
#else
	MKNOD("/dev/video0", S_IFCHR | 0x666, makedev(81, 0));
#if !defined(RTN14U)
	MKNOD("/dev/rtkswitch", S_IFCHR | 0x666, makedev(206, 0));
#endif
	MKNOD("/dev/spiS0", S_IFCHR | 0x666, makedev(217, 0));
	MKNOD("/dev/i2cM0", S_IFCHR | 0x666, makedev(218, 0));
#if defined(RTN14U)
	MKNOD("/dev/rdm0", S_IFCHR | 0x666, makedev(253, 0));
#else
	MKNOD("/dev/rdm0", S_IFCHR | 0x666, makedev(254, 0));
#endif
	MKNOD("/dev/flash0", S_IFCHR | 0x666, makedev(200, 0));
	MKNOD("/dev/swnat0", S_IFCHR | 0x666, makedev(210, 0));
	MKNOD("/dev/hwnat0", S_IFCHR | 0x666, makedev(220, 0));
	MKNOD("/dev/acl0", S_IFCHR | 0x666, makedev(230, 0));
	MKNOD("/dev/ac0", S_IFCHR | 0x666, makedev(240, 0));
	MKNOD("/dev/mtr0", S_IFCHR | 0x666, makedev(250, 0));
	MKNOD("/dev/gpio0", S_IFCHR | 0x666, makedev(252, 0));
	MKNOD("/dev/nvram", S_IFCHR | 0x666, makedev(228, 0));
	MKNOD("/dev/PCM", S_IFCHR | 0x666, makedev(233, 0));
	MKNOD("/dev/I2S", S_IFCHR | 0x666, makedev(234, 0));
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

static void init_switch_ralink(void)
{
#if defined(RTN14U)
#ifdef HWNAT_FIX
	shm_client_info_id = shmget((key_t)1001, sizeof(int), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
 		 _dprintf("hwnat rst:shmget failed1\n");
	         goto err;
	}

        shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
	   	 _dprintf("hwnat rst:shmat failed2\n");
	         goto err;
	}	    
	hwnat_rst_unlock= (int *)shared_client_info;
	*hwnat_rst_unlock=0;
err:
#endif	
#endif	
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
#if defined(RTN14U)
	system("rtkswitch 8 0"); //Barton add
#endif
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
	int model;
	int stbport;
	int controlrate_unknown_unicast;
	int controlrate_unknown_multicast;
	int controlrate_multicast;
	int controlrate_broadcast;

	dbG("link down all ports\n");
	eval("rtkswitch", "17");	// link down all ports

	if (config_switch_for_first_time)
		config_switch_for_first_time = 0;
	else
	{
		dbG("software reset\n");
		eval("rtkswitch", "27");	// software reset
	}

	if (is_routing_enabled())
	{
		char parm_buf[] = "XXX";

		stbport = atoi(nvram_safe_get("switch_stb_x"));
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);
		/* stbport:	unifi_malaysia=1	otherwise
		 * ----------------------------------------------
		 *	0:	LLLLW			LLLLW
		 *	1:	LLLTW			LLLWW
		 *	2:	LLTLW			LLWLW
		 *	3:	LTLLW			LWLLW
		 *	4:	TLLLW			WLLLW
		 *	5:	TTLLW			WWLLW
		 *	6:	LLTTW			LLWWW
		 */
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
	
			sprintf(tmp, "rtkswitch 29 %d", voip_port);	
			system(tmp);	

			if(!strncmp(nvram_safe_get("switch_wantag"), "unifi", 5))/* Added for Unifi. Cherry Cho modified in 2011/6/28.*/
			{
				if(strstr(nvram_safe_get("switch_wantag"), "home"))
				{					
					system("rtkswitch 38 1");/* IPTV: P0 */
					/* Internet */
					system("rtkswitch 36 500");
					system("rtkswitch 37 0");
					system("rtkswitch 39 0x02000210");
					/* IPTV */
					system("rtkswitch 36 600");
					system("rtkswitch 37 0");
					system("rtkswitch 39 0x00010011");
				}
				else/* No IPTV. Business package */
				{
					/* Internet */
					system("rtkswitch 38 0");
					system("rtkswitch 36 500");
					system("rtkswitch 37 0");
					system("rtkswitch 39 0x02000210");
				}
			}
			else if(!strncmp(nvram_safe_get("switch_wantag"), "singtel", 7))/* Added for SingTel's exStream issues. Cherry Cho modified in 2011/7/19. */
			{		
				if(strstr(nvram_safe_get("switch_wantag"), "mio"))/* Connect Singtel MIO box to P3 */
				{
					system("rtkswitch 40 1");
					system("rtkswitch 38 3");/* IPTV: P0  VoIP: P1 */
					/* Internet */
					system("rtkswitch 36 10");
					system("rtkswitch 37 0");
					system("rtkswitch 39 0x02000210");
					/* VoIP */
					system("rtkswitch 36 30");
					system("rtkswitch 37 4");				
					system("rtkswitch 39 0x00000012");//VoIP Port: P1 tag
				}
				else//Connect user's own ATA to lan port and use VoIP by Singtel WAN side VoIP gateway at voip.singtel.com
				{
					system("rtkswitch 38 1");/* IPTV: P0 */
					/* Internet */
					system("rtkswitch 36 10");
					system("rtkswitch 37 0");
					system("rtkswitch 39 0x02000210");
				}

				/* IPTV */
				system("rtkswitch 36 20");
				system("rtkswitch 37 4");
				system("rtkswitch 39 0x00010011");
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "m1_fiber"))//VoIP: P1 tag. Cherry Cho added in 2012/1/13.
			{
				system("rtkswitch 40 1");
				system("rtkswitch 38 2");/* VoIP: P1  2 = 0x10 */
				/* Internet */
				system("rtkswitch 36 1103");
				system("rtkswitch 37 1");
				system("rtkswitch 39 0x02000210");
				/* VoIP */
				system("rtkswitch 36 1107");
				system("rtkswitch 37 1");				
				system("rtkswitch 39 0x00000012");//VoIP Port: P1 tag
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber"))//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
			{
				system("rtkswitch 40 1");
				system("rtkswitch 38 2");/* VoIP: P1  2 = 0x10 */
				/* Internet */
				system("rtkswitch 36 621");
				system("rtkswitch 37 0");
				system("rtkswitch 39 0x02000210");
				/* VoIP */
				system("rtkswitch 36 821");
				system("rtkswitch 37 0");				
				system("rtkswitch 39 0x00000012");//VoIP Port: P1 tag

				system("rtkswitch 36 822");
				system("rtkswitch 37 0");				
				system("rtkswitch 39 0x00000012");//VoIP Port: P1 tag
			}
			else if(!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber_sp"))//VoIP: P1 tag. Cherry Cho added in 2012/11/6.
			{
				system("rtkswitch 40 1");
				system("rtkswitch 38 2");/* VoIP: P1  2 = 0x10 */
				/* Internet */
				system("rtkswitch 36 11");
				system("rtkswitch 37 0");
				system("rtkswitch 39 0x02000210");
				/* VoIP */
				system("rtkswitch 36 14");
				system("rtkswitch 37 0");				
				system("rtkswitch 39 0x00000012");//VoIP Port: P1 tag
			}
			else/* Cherry Cho added in 2011/7/11. */
			{
				/* Initialize VLAN and set Port Isolation */
				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") && strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("rtkswitch 38 3");// 3 = 0x11 IPTV: P0  VoIP: P1
				else if(strcmp(nvram_safe_get("switch_wan1tagid"), ""))
					system("rtkswitch 38 1");// 1 = 0x01 IPTV: P0 
				else if(strcmp(nvram_safe_get("switch_wan2tagid"), ""))
					system("rtkswitch 38 2");// 2 = 0x10 VoIP: P1
				else
					system("rtkswitch 38 0");//No IPTV and VoIP ports

				/*++ Get and set Vlan Information */
				if(strcmp(nvram_safe_get("switch_wan0tagid"), "") != 0)	// Internet on WAN (port 4)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan0tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{											
						sprintf(tmp, "rtkswitch 36 %d", vlan_val);
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan0prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan0prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "rtkswitch 37 %d", vlan_val);
							system(tmp);
						}
						else
							system("rtkswitch 37 0");
					}

					system("rtkswitch 39 0x02000210");
				}

				if(strcmp(nvram_safe_get("switch_wan1tagid"), "") != 0)	// IPTV on LAN4 (port 0)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan1tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{								
						sprintf(tmp, "rtkswitch 36 %d", vlan_val);	
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan1prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan1prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "rtkswitch 37 %d", vlan_val);	
							system(tmp);
						}
						else
							system("rtkswitch 37 0");
					}	

					if(!strcmp(nvram_safe_get("switch_wan1tagid"), nvram_safe_get("switch_wan2tagid")))
						system("rtkswitch 39 0x00030013"); //IPTV=VOIP
					else
						system("rtkswitch 39 0x00010011");//IPTV Port: P0 untag 65553 = 0x10 011
				}	

				if(strcmp(nvram_safe_get("switch_wan2tagid"), "") != 0)	// VoIP on LAN3 (port 1)
				{
					vlan_val = atoi(nvram_safe_get("switch_wan2tagid"));
					if((vlan_val >= 2) && (vlan_val <= 4094))
					{					
						sprintf(tmp, "rtkswitch 36 %d", vlan_val);	
						system(tmp);
					}

					if(strcmp(nvram_safe_get("switch_wan2prio"), "") != 0)
					{
						vlan_val = atoi(nvram_safe_get("switch_wan2prio"));
						if((vlan_val >= 0) && (vlan_val <= 7))
						{
							sprintf(tmp, "rtkswitch 37 %d", vlan_val);	
							system(tmp);		
						}
						else
							system("rtkswitch 37 0");
					}
	
                                        if(!strcmp(nvram_safe_get("switch_wan1tagid"), nvram_safe_get("switch_wan2tagid")))
                                                system("rtkswitch 39 0x00030013"); //IPTV=VOIP
                                        else
						system("rtkswitch 39 0x00020012");//VoIP Port: P1 untag
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
		if (controlrate_unknown_multicast)
		{
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
		if (controlrate_broadcast)
		{
			sprintf(parm_buf, "%d", controlrate_broadcast);
			eval("rtkswitch", "25", parm_buf);
		}
	}
	else if (is_apmode_enabled())
	{
		model = get_model();
		if (model == MODEL_RTN65U || model == MODEL_RTN36U3)
			eval("rtkswitch", "8", "100");
	}

#ifdef RTCONFIG_DSL
	dbG("link up all ports\n");
	eval("rtkswitch", "16");	// link up all ports
#else
	dbG("link up wan port(s)\n");
	eval("rtkswitch", "114");	// link up wan port(s)
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
	if (!is_module_loaded("rt2860v2_ap"))
		modprobe("rt2860v2_ap");
#if defined (RTCONFIG_WLMODULE_RT3090_AP)
	if (!is_module_loaded("RTPCI_ap"))
	{
		modprobe("RTPCI_ap");
	}
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if (!is_module_loaded("iNIC_mii"))
		modprobe("iNIC_mii", "mode=ap", "bridge=1", "miimaster=eth2", "syncmiimac=0");	// set iNIC mac address from eeprom need insmod with "syncmiimac=0"
#endif

	sleep(1);
}

void fini_wl(void)
{
	if (is_module_loaded("hw_nat"))
		modprobe_r("hw_nat");

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if (is_module_loaded("iNIC_mii"))
		modprobe_r("iNIC_mii");
#endif

#if defined (RTCONFIG_WLMODULE_RT3090_AP)
	if (is_module_loaded("RTPCI_ap"))
	{
		modprobe_r("RTPCI_ap");
	}
#endif

	if (is_module_loaded("rt2860v2_ap"))
		modprobe_r("rt2860v2_ap");
}

void init_syspara(void)
{
	unsigned char buffer[16];
	unsigned int *src;
	char *dst;
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

#if !defined(RTN14U) // single band
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

#if defined(RTN14U) // single band
	if (!ralink_mssid_mac_validate(macaddr))
#else
	if (!ralink_mssid_mac_validate(macaddr) || !ralink_mssid_mac_validate(macaddr2))
#endif
		nvram_set("wl_mssid", "0");
	else
		nvram_set("wl_mssid", "1");

#if defined(RTN14U) // single band
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
	dst = country_code;
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
		   //for specific power
		        if (country_code[0] ==0x5a  && country_code[1] == 0x31)
			{
			   country_code[0]='U';
			   country_code[1]='S';
			}   
			else if (country_code[0] ==0x5a  && country_code[1] == 0x32)
			{
			   country_code[0]='G';
			   country_code[1]='B';
			}   
			else if (country_code[0] ==0x5a  && country_code[1] == 0x33)
			{
			   country_code[0]='T';
			   country_code[1]='W';
			}   
			else if (country_code[0] ==0x5a  && country_code[1] == 0x34)
			{
			   country_code[0]='C';
			   country_code[1]='N';
			}   
			   
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

	src = (unsigned int*) 0x50020;	/* /dev/mtd/3, firmware, starts from 0x50000 */
	dst = buffer;
	bytes = 16;
	if (FRead(dst, (int)src, bytes)<0)
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

	{
#ifdef RTCONFIG_ODMPID
		char modelname[16];
		FRead(modelname, OFFSET_ODMPID, sizeof(modelname));
		modelname[sizeof(modelname)-1] = '\0';
		if(modelname[0] != 0 && (unsigned char)(modelname[0]) != 0xff && is_valid_hostname(modelname) && strcmp(modelname, "ASUS"))
		{
			nvram_set("odmpid", modelname);
		}
		else
#endif
			nvram_unset("odmpid");
	}

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



#if defined(RTN14U)
#ifdef HWNAT_FIX
void hwnat_workaround(void)
{   
	int t;

	//_dprintf("--fix--\n");
	if(t=is_module_loaded("hw_nat")) modprobe_r("hw_nat");
	 
	usleep(100000);
	system("reg s 0xb0000000");
	system("reg w 0x34 0x80000000");
	usleep(200000);
	system("reg w 0x34 0x0");
	usleep(200000);
	
	if(t)  modprobe("hw_nat");
}
#endif
#endif	


// only ralink solution can reload it dynamically
void reinit_hwnat()
{
	// only happened when hwnat=1
	// only loaded when unloaded, and unloaded when loaded
	// in restart_firewall for fw_pt_l2tp/fw_pt_ipsec
	// in restart_qos for qos_enable
	// in restart_wireless for wlx_mrate_x
	
	if (nvram_get_int("hwnat")) {
#if defined(RTN14U)	
#ifdef HWNAT_FIX
		   if(*hwnat_rst_unlock)
			hwnat_workaround();
#endif 
#endif		   
		if (is_nat_enabled() && !nvram_get_int("qos_enable") /*&&*/
			/* TODO: consider RTCONFIG_DUALWAN case */
//			!nvram_match("wan0_proto", "l2tp") &&
//			!nvram_match("wan0_proto", "pptp") &&
			/*(nvram_match("wl0_radio", "0") || !nvram_get_int("wl0_mrate_x")) &&
			(nvram_match("wl1_radio", "0") || !nvram_get_int("wl1_mrate_x"))*/) {
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
		sprintf(buf, "%s", "");
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

