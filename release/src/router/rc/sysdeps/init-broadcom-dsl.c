#include "rc.h"
#include "shared.h"

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
		eval("vconfig", "add", "eth0", "100");
		eval("ifconfig", "vlan100", "up");
		eval("ifconfig", "vlan100", "hw", "ether", nvram_safe_get("et0macaddr"));
	}

	if (enable_dsl_wan_eth_if)
	{
		// vlan4 = ethenet WAN
		eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
		eval("vconfig", "add", "eth0", "4");
		eval("ifconfig", "vlan4", "up");
		// ethernet WAN , it needs to use another MAC address
		eval("ifconfig", "vlan4", "hw", "ether", nvram_safe_get("et1macaddr"));
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
	eval("et", "robowr", "0x05", "0x83", "0x0021");
	eval("et", "robowr", "0x05", "0x81", "0x0064");
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

	if(wans_lanport < 1 || wans_lanport > 4)
		return;

	switch(wans_lanport) {
	case 1:
		dbG("wan port = lan1\n");
		eval("et", "robowr", "0x34", "0x12", "0x04");
		eval("et", "robowr", "0x05", "0x83", "0x0422");
		break;
	case 2:
		dbG("wan port = lan2\n");
		eval("et", "robowr", "0x34", "0x14", "0x04");
		eval("et", "robowr", "0x05", "0x83", "0x0824");
		break;
	case 3:
		dbG("wan port = lan3\n");
		eval("et", "robowr", "0x34", "0x16", "0x04");
		eval("et", "robowr", "0x05", "0x83", "0x1028");
		break;
	case 4:
		dbG("wan port = lan4\n");
		eval("et", "robowr", "0x34", "0x18", "0x04");
		eval("et", "robowr", "0x05", "0x83", "0x2030");
		break;
	}
	eval("et", "robowr", "0x05", "0x81", "0x0004");
	eval("et", "robowr", "0x05", "0x80", "0x0000");
	eval("et", "robowr", "0x05", "0x80", "0x0080");
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

