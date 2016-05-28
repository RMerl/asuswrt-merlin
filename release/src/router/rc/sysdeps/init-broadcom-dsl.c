#include "rc.h"
#include "shared.h"

int cur_ewan_vid = 0;

static int  check_ewan_dot1q_enable(const int enable, const int vid)
{
	if(enable)
	{
		if(vid >= 4 && !(vid >= DSL_WAN_VID && vid <= DSL_WAN_VID + 7))
			return 1;
	}
	return 0;
}


void init_switch_dsl()
{
	// vlan1 => LAN PORT
	// vlan2 => WAN to modem , send packet to modem , used on modem driver
	// vlan3 => IPTV port
	// vlan4 => WAN to Ethernet this is ethernet WAN ifname
	int stbport;
	int enable_dsl_wan_dsl_if = 0;
	int enable_dsl_wan_eth_if = 0;
	int enable_dsl_wan_iptv_if = 0;
	char buf[64];

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
		stbport = atoi(nvram_safe_get("switch_stb_x"));
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);

		if (stbport > 0)
		{
			enable_dsl_wan_iptv_if = 1;
		}
	}

	eval("ifconfig", "eth0", "up");
	// vlan2 = WAN
	eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
	eval("vconfig", "add", "eth0", "2");
	eval("ifconfig", "vlan2", "up");

	if (enable_dsl_wan_dsl_if)
	{
		// DSL WAN MODE
		eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
		snprintf(buf, sizeof(buf), "%d", DSL_WAN_VID);
		eval("vconfig", "add", "eth0", buf);
		eval("ifconfig", DSL_WAN_VIF, "up");
		eval("ifconfig", DSL_WAN_VIF, "hw", "ether", nvram_safe_get("et0macaddr"));
	}

	if (enable_dsl_wan_eth_if)
	{
		int ewan_dot1q = nvram_get_int("ewan_dot1q");	//Andy Chiu, 2015/09/08
		int ewan_vid = nvram_get_int("ewan_vid");		//Andy Chiu, 2015/09/08
		int ewan_dot1p = nvram_get_int("ewan_dot1p");	//Andy Chiu, 2015/09/08
		
		// vlan4 = ethenet WAN
		eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
		if(check_ewan_dot1q_enable(ewan_dot1q, ewan_vid))
		{
			snprintf(buf, sizeof(buf), "%d", ewan_vid);
			eval("vconfig", "add", "eth0", buf);
			snprintf(buf, sizeof(buf), "vlan%d", ewan_vid);
			eval("ifconfig", buf, "up");
			// ethernet WAN , it needs to use another MAC address
			eval("ifconfig", buf, "hw", "ether", nvram_safe_get("et1macaddr"));
			cur_ewan_vid = ewan_vid;
		}
		else
		{
			eval("vconfig", "add", "eth0", "4");
			eval("ifconfig", "vlan4", "up");
			// ethernet WAN , it needs to use another MAC address
			eval("ifconfig", "vlan4", "hw", "ether", nvram_safe_get("et1macaddr"));
			cur_ewan_vid = 4;
		}
	}

	if (enable_dsl_wan_iptv_if)
	{
		// vlan3 = IPTV
		eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
		eval("vconfig", "add", "eth0", "3");
		eval("ifconfig", "vlan3", "up");
		// bypass , no need to have another MAC address
	}

}

void config_switch_dsl_set_dsl()
{	
	char buf[8];
	snprintf(buf, sizeof(buf), "0x%04x", DSL_WAN_VID);	
	eval("et", "robowr", "0x05", "0x83", "0x0021");
	eval("et", "robowr", "0x05", "0x81", buf);	//Andy Chiu, 2016/04/11. modify 
	eval("et", "robowr", "0x05", "0x80", "0x0000");
	eval("et", "robowr", "0x05", "0x80", "0x0080");
}

void config_switch_dsl_set_iptv(int stbport)
{
	if(stbport < 1 || stbport > 6)
		return;

	switch(stbport) {
	case 1:
		eval("et", "robowr", "0x34", "0x12", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x0422");
		break;
	case 2:
		eval("et", "robowr", "0x34", "0x14", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x0824");
		break;
	case 3:
		eval("et", "robowr", "0x34", "0x16", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x1028");
		break;
	case 4:
		eval("et", "robowr", "0x34", "0x18", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x2030");
		break;
	case 5: // 1 & 2
		eval("et", "robowr", "0x34", "0x12", "0x0003");
		eval("et", "robowr", "0x34", "0x14", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x0C26");
		break;
	case 6:	// 3 & 4
		eval("et", "robowr", "0x34", "0x16", "0x0003");
		eval("et", "robowr", "0x34", "0x18", "0x0003");
		eval("et", "robowr", "0x05", "0x83", "0x3038");
		break;
	}
	eval("et", "robowr", "0x05", "0x81", "0x0003");
	eval("et", "robowr", "0x05", "0x80", "0x0000");
	eval("et", "robowr", "0x05", "0x80", "0x0080");

}

void config_switch_dsl_set_lan()
{
	int wans_lanport = nvram_get_int("wans_lanport");

	char vid[32], buf[32];	//Andy Chiu, 2015/09/09
	int ewan_dot1q = nvram_get_int("ewan_dot1q");	//Andy Chiu, 2015/09/08
	int ewan_vid = nvram_get_int("ewan_vid");	//Andy Chiu, 2015/09/08
	int ewan_dot1p = nvram_get_int("ewan_dot1p");	//Andy Chiu, 2015/09/08

	if(wans_lanport < 1 || wans_lanport > 4)
		return;

	dbG("ewan_dot1q=%d, ewan_vid =%d, ewan_dot1p=%d\n", ewan_dot1q, ewan_vid, ewan_dot1p);
	//Andy Chiu, 2015/09/09
	if(check_ewan_dot1q_enable(ewan_dot1q, ewan_vid))
		sprintf(vid, "0x%04x", ewan_vid);
	else
		strcpy(vid, "0x0004");

	dbG("vid=%s\n", vid);
	
	switch(wans_lanport) {
	case 1:
		dbG("wan port = lan1\n");
		eval("et", "robowr", "0x34", "0x12", vid);
		eval("et", "robowr", "0x05", "0x83", check_ewan_dot1q_enable(ewan_dot1q, ewan_vid)? "0x0022": "0x0422");
		break;
	case 2:
		dbG("wan port = lan2\n");
		eval("et", "robowr", "0x34", "0x14", vid);
		eval("et", "robowr", "0x05", "0x83", check_ewan_dot1q_enable(ewan_dot1q, ewan_vid)? "0x0024": "0x0824");
		break;
	case 3:
		dbG("wan port = lan3\n");
		eval("et", "robowr", "0x34", "0x16", vid);
		eval("et", "robowr", "0x05", "0x83", check_ewan_dot1q_enable(ewan_dot1q, ewan_vid)? "0x0028": "0x1028");
		break;
	case 4:
		dbG("wan port = lan4\n");
		eval("et", "robowr", "0x34", "0x18", vid);
		eval("et", "robowr", "0x05", "0x83", check_ewan_dot1q_enable(ewan_dot1q, ewan_vid)? "0x0030": "0x2030");
		break;
	}
	eval("et", "robowr", "0x05", "0x81", vid);
	eval("et", "robowr", "0x05", "0x80", "0x0000");
	eval("et", "robowr", "0x05", "0x80", "0x0080");

	//Set proi, NOT work, need checking
	dbG("checking<%d, %d>\n", check_ewan_dot1q_enable(ewan_dot1q, ewan_vid), ewan_dot1p);
	if(check_ewan_dot1q_enable(ewan_dot1q, ewan_vid) && ewan_dot1p > 0)
	{
		sprintf(buf, "vlan%d", ewan_vid);
		dbG("<%s, %s>\n", buf, nvram_get("ewan_dot1p"));
		eval("vconfig", "set_egress_map", buf, "0", nvram_get("ewan_dot1p"));
	}
}

void config_switch_dsl()
{
	int stbport;
	dbG("config ethernet switch IC\n");

	if (is_routing_enabled())
	{
		stbport = atoi(nvram_safe_get("switch_stb_x"));
		if (stbport < 0 || stbport > 6) stbport = 0;
		dbG("STB port: %d\n", stbport);

#ifdef RTCONFIG_DUALWAN
		if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL)
			config_switch_dsl_set_dsl();
		if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN || get_dualwan_secondary()==WANS_DUALWAN_IF_LAN)
			config_switch_dsl_set_lan();
		config_switch_dsl_set_iptv(stbport);
#else
		config_switch_dsl_set_dsl();
		config_switch_dsl_set_iptv(stbport);
#endif
	}
}

