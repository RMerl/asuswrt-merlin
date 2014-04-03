/*
 TODO: merge 8367 control into one 
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

void check_upgrade_from_old_ui(void)
{
// 1=old ui
// dsl_web_ui_ver is reserved for new web ui
// when upgrade to new ui, it check this field and format NVRAM
	if (nvram_match("dsl_web_ui_ver", "1")) {
		dbg("Upgrade from old ui, erase NVRAM and reboot\n");	
		eval("mtd-erase","-d","nvram");
		/* FIXME: all stop-wan, umount logic will not be called
		 * prevous sys_exit (kill(1, SIGTERM) was ok
		 * since nvram isn't valid stop_wan should just kill possible daemons,
		 * nothing else, maybe with flag */
		sync();
		reboot(RB_AUTOBOOT);					
	}
	return;
}

void init_switch_dsl()
{
	// DSLTODO

	// Eth2 => SoC RGMII 0 => Because we use VLAN, we do not use this interface directly
	// Eth2.1 => WAN to modem , send packet to modem , used on modem driver
	// Eth2.2 => LAN PORT
	// Eth 2.1.50 => for stats, ethernet driver modify modem packets to this VLAN ID
	// Eth 2.1.51 => bridge for Ethernet and modem, ethernet driver modify modem packet to VLAN ID (51)
	// and then bridge eth2.2 and eth2.1.51. thus, PC could control modem from LAN port
	// Eth2.3 => IPTV port
	// Eth2.4 => WAN to Ethernet this is ethernet WAN ifname 
	int stbport;
	int enable_dsl_wan_dsl_if = 0;
	int enable_dsl_wan_eth_if = 0;
	int enable_dsl_wan_iptv_if = 0;	
	

	eval("ifconfig", "eth2", "up");
	// eth2.1 = WAN	
	eval("vconfig", "add", "eth2", "1");
	eval("ifconfig", "eth2.1", "up");	
	// eth2.2 = LAN
	eval("vconfig", "add", "eth2", "2");
	eval("ifconfig", "eth2.2", "up");	
	// eth2.3 = IPTV

#ifdef RTCONFIG_DUALWAN
	if (get_dualwan_secondary()==WANS_DUALWAN_IF_NONE)
	{
		if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL)
		{
			enable_dsl_wan_dsl_if = 1;
		}
		else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN)
		{
			enable_dsl_wan_eth_if = 1;
		}
	}
	else
	{
		if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL)
		{
			enable_dsl_wan_dsl_if = 1;
		}
		else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN)
		{
			enable_dsl_wan_eth_if = 1;
		}
	
		if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
		{
			enable_dsl_wan_dsl_if = 1;
		}
		else if (get_dualwan_secondary()==WANS_DUALWAN_IF_LAN)
		{
			enable_dsl_wan_eth_if = 1;
		}		
	}
#else
	enable_dsl_wan_dsl_if = 1;
#endif

	if (is_routing_enabled())
	{
		stbport = nvram_get_int("switch_stb_x");
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);

		if (stbport > 0)
		{
			enable_dsl_wan_iptv_if = 1;
		}
	}


	if (enable_dsl_wan_dsl_if)
	{
		// DSL WAN MODE
		eval("vconfig", "add", "eth2.1", "1");
		eval("ifconfig", "eth2.1.1", "up"); 
		eval("ifconfig", "eth2.1", "hw", "ether", nvram_safe_get("et0macaddr"));
	}
	
	if (enable_dsl_wan_eth_if)
	{
		// eth2.4 = ethenet WAN
		eval("vconfig", "add", "eth2", "4");
		eval("ifconfig", "eth2.4", "up");	
		// ethernet WAN , it needs to use another MAC address
		eval("ifconfig", "eth2.4", "hw", "ether", nvram_safe_get("et1macaddr"));			
	}

	if (enable_dsl_wan_iptv_if)
	{
		// eth2.3 = IPTV
		eval("vconfig", "add", "eth2", "3");
		eval("ifconfig", "eth2.3", "up");	
		// bypass , no need to have another MAC address
	}

	// for a dummy interface from trendchip
	// this is only for stats viewing , non-necessary
	// do not power up dummay interface. If so, it will send packet out	
	eval("vconfig", "add", "eth2.1", "50");
	eval("vconfig", "add", "eth2.1", "51");

}

void config_switch_dsl_set_dsl()
{
	// DSL WAN , no IPTV , USB also goes here
	dbG("wan port = dsl\n");
	eval("rtkswitch", "8", "0");					
}

void config_switch_dsl_set_iptv(int stbport)
{
	char param_buf[32];	
	sprintf(param_buf, "%d", stbport);
	eval("rtkswitch", "8", param_buf);
}

void config_switch_dsl_set_lan_iptv(int stbport)
{
	// ethernet wan port = high-byte
	// iptv port = low-byte
	char param_buf[32];	
	// ethernet WAN , has IPTV
	if (nvram_match("wans_lanport","1"))
	{
		// 0x100
		dbG("wan port = lan1\n");				
		sprintf(param_buf, "%d", 0x100 + stbport);
		eval("rtkswitch", "8", param_buf);
	}
	else if (nvram_match("wans_lanport","2"))
	{
		// 0x200
		dbG("wan port = lan2\n");				
		sprintf(param_buf, "%d", 0x200 + stbport);
		eval("rtkswitch", "8", param_buf);
	}
	else if (nvram_match("wans_lanport","3"))
	{
		// 0x300
		dbG("wan port = lan3\n");								
		sprintf(param_buf, "%d", 0x300 + stbport);
		eval("rtkswitch", "8", param_buf);
	}
	else
	{
		// 0x400
		dbG("wan port = lan4\n");												
		sprintf(param_buf, "%d", 0x400 + stbport);
		eval("rtkswitch", "8", param_buf);
	}
}

void config_switch_dsl_set_lan()
{
	// ethernet wan port = high-byte
	// iptv port = low-byte
	// DSLWAN , ethernet WAN , no IPTV
	if (nvram_match("wans_lanport","1"))
	{
		// 0x100
		dbG("wan port = lan1\n");				
		eval("rtkswitch", "8", "256");
	}
	else if (nvram_match("wans_lanport","2"))
	{
		// 0x200
		dbG("wan port = lan2\n");				
		eval("rtkswitch", "8", "512");		
	}
	else if (nvram_match("wans_lanport","3"))
	{
		// 0x300
		dbG("wan port = lan3\n");								
		eval("rtkswitch", "8", "768");		
	}
	else
	{
		// 0x400
		dbG("wan port = lan4\n");												
		eval("rtkswitch", "8", "1024"); 	
	}
}

void config_switch_dsl()
{
	int stbport;
	dbG("config ethernet switch IC\n");
	
	if (is_routing_enabled())
	{
		stbport = nvram_get_int("switch_stb_x");
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);

#ifdef RTCONFIG_DUALWAN
		if (stbport == 0)
		{
			if (get_dualwan_secondary()==WANS_DUALWAN_IF_LAN)
			{
				config_switch_dsl_set_lan();
			}
			else
			{
				if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN)
				{
					config_switch_dsl_set_lan();				
				}
				else
				{
					config_switch_dsl_set_dsl();
				}
			}
		}
		else
		{
			if (get_dualwan_secondary()==WANS_DUALWAN_IF_LAN)
			{
				config_switch_dsl_set_lan_iptv(stbport);
			}
			else
			{
				if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN)
				{
					config_switch_dsl_set_lan_iptv(stbport);				
				}
				else
				{
					config_switch_dsl_set_iptv(stbport);
				}
			}
		}
#else
		if (stbport == 0)
		{
			config_switch_dsl_set_dsl();
		}
		else
		{
			config_switch_dsl_set_iptv(stbport);						
		}
#endif
	}
}

