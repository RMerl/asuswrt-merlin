#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#include <shutils.h>
#include <shared.h>
#include <rtstate.h>

/* keyword for rc_support 	*/
/* ipv6 mssid update parental 	*/

void add_rc_support(char *feature)
{
	char *rcsupport = nvram_safe_get("rc_support");
	char *features;

	if (!(feature && *feature))
		return;

	if (*rcsupport) {
		features = malloc(strlen(rcsupport) + strlen(feature) + 2);
		if (features == NULL) {
			_dprintf("add_rc_support fail\n");
			return;
		}
		sprintf(features, "%s %s", rcsupport, feature);
		nvram_set("rc_support", features);
		free(features);
	} else
		nvram_set("rc_support", feature);
}

int get_wan_state(int unit)
{
	char tmp[100], prefix[]="wanXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	return nvram_get_int(strcat_r(prefix, "state_t", tmp));
}

// get wan_unit from device ifname or hw device ifname
#if 0
int get_wan_unit(char *ifname)
{
	char word[256], tmp[100], *next;
	char prefix[32]="wanXXXXXX_";
	int unit, found = 0;

	unit = WAN_UNIT_FIRST;

	foreach (word, nvram_safe_get("wan_ifnames"), next) {
		if(strncmp(ifname, "ppp", 3)==0) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if(strcmp(nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)), ifname)==0) {
				found = 1;
			}	
		}
		else if(strcmp(ifname, word)==0) {
			found = 1;
		}
		if(found) break;
		unit ++;
	}

	if(!found) unit = WAN_UNIT_FIRST;
	return unit;
}
#else
int get_wan_unit(char *ifname)
{
	char tmp[100], prefix[32]="wanXXXXXX_";
	int unit;

	if(ifname == NULL)
		return -1;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strncmp(ifname, "ppp", 3)){
			if(nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), ifname))
				return unit;
		}
		else if(nvram_match(strcat_r(prefix, "ifname", tmp), ifname))
			return unit;
	}

	return -1;
}
#endif

// Get physical wan ifname of working connection
char *get_wanx_ifname(int unit)
{
	char *wan_ifname;
	char tmp[100], prefix[]="wanXXXXXXXXXX_";
	
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_ifname=nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	return wan_ifname;
}

// Get wan ifname of working connection
char *get_wan_ifname(int unit)
{
	char *wan_proto, *wan_ifname;
	char tmp[100], prefix[]="wanXXXXXXXXXX_";
	
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	//_dprintf("wan_proto: %s\n", wan_proto);

#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
#else
	if(unit == WAN_UNIT_SECOND)
#endif
	{
		if(!strcmp(wan_proto, "dhcp"))
			wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		else
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	}
	else
#endif
	if(strcmp(wan_proto, "pppoe") == 0 ||
		strcmp(wan_proto, "pptp") == 0 ||
		strcmp(wan_proto, "l2tp") == 0)
		wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	else
		wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	//_dprintf("wan_ifname: %s\n", wan_ifname);

	return wan_ifname;
}

#ifdef RTCONFIG_IPV6

char *get_wan6_ifname(int unit)
{
	char *wan_proto, *wan_ifname;
	char tmp[100], prefix[]="wanXXXXXXXXXX_";
	
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));


	if(strcmp(nvram_safe_get("ipv6_ifdev"), "eth")==0) {
		wan_ifname=nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		dbG("wan6_ifname: %s\n", wan_ifname);
		return wan_ifname;
	}

#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
#else
	if(unit == WAN_UNIT_SECOND)
#endif
	{
		if(!strcmp(wan_proto, "dhcp"))
			wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		else
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	}
	else
#endif
	if(strcmp(wan_proto, "pppoe") == 0 ||
		strcmp(wan_proto, "pptp") == 0 ||
		strcmp(wan_proto, "l2tp") == 0)
		wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	else wan_ifname=nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	dbG("wan6_ifname: %s\n", wan_ifname);
	return wan_ifname;
}

#endif

// OR all lan port status
int get_lanports_status()
{
	return lanport_status();
}

// OR all wan port status
int get_wanports_status(int wan_unit)
{
// 1. PHY type, 2. factory owner, 3. model.
#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_DSL)
#endif
	{
		/* Paul modify 2012/10/17, shouldn't check ADSL sync status, check WAN0 state instead. */
		//if (nvram_match("dsltmp_adslsyncsts","up")) return 1;
		if (nvram_match("wan0_state_t","2")) return 1;
		return 0;
	}
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN)
	{
		return dsl_wanPort_phyStatus();		
	}
#endif
	// TO CHENI:
	// HOW TO HANDLE USB?	
#else // RJ-45
#ifdef RTCONFIG_RALINK
	return rtl8367m_wanPort_phyStatus();
#else
	return wanport_status(wan_unit);
#endif
#endif
}

int get_usb_modem_state(){
	if(!strcmp(nvram_safe_get("modem_running"), "1"))
		return 1;
	else
		return 0;
}

int set_usb_modem_state(const int flag){
	if(flag != 1 && flag != 0)
		return 0;

	if(flag){
		nvram_set("modem_running", "1");
		return 1;
	}
	else{
		nvram_set("modem_running", "0");
		return 0;
	}
}

int
set_wan_primary_ifunit(const int unit)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int i;

	if (unit < WAN_UNIT_FIRST || unit >= WAN_UNIT_MAX)
		return -1;

	nvram_set_int("wan_primary", unit);
	for (i = WAN_UNIT_FIRST; i < WAN_UNIT_MAX; i++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", i);
		nvram_set_int(strcat_r(prefix, "primary", tmp), (i == unit) ? 1 : 0);
	}

	return 0;
}

int
wan_primary_ifunit(void)
{
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int unit;

	/* TODO: Why not just nvram_get_int("wan_primary")? */
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "primary", tmp), "1"))
			return unit;
	}

	return 0;
}

void
set_invoke_later(int flag)
{
	nvram_set_int("invoke_later", nvram_get_int("invoke_later")|flag);
}

int
get_invoke_later()
{
	return(nvram_get_int("invoke_later"));
}

#ifdef RTCONFIG_USB

char ehci_string[32];
char ohci_string[32];

char *get_usb_ehci_port(int port)
{
	char word[100], *next;
	int i=0;

	strcpy(ehci_string, "xxxxxxxx");

	foreach(word, nvram_safe_get("ehci_ports"), next) {
		if(i==port) {
			strcpy(ehci_string, word);
			break;
		}		
		i++;
	}
	return ehci_string;
}

char *get_usb_ohci_port(int port)
{
	char word[100], *next;
	int i=0;

	strcpy(ohci_string, "xxxxxxxx");

	foreach(word, nvram_safe_get("ohci_ports"), next) {
		if(i==port) {
			strcpy(ohci_string, word);
			break;
		}		
		i++;
	}
	return ohci_string;
}

int get_usb_port_number(const char *usb_port){
	char word[100], *next;
	int port_num, i;

	port_num = 0;
	i = 0;
	foreach(word, nvram_safe_get("ehci_ports"), next){
		++i;
		if(!strcmp(usb_port, word)){
			port_num = i;
			break;
		}
	}

	i = 0;
	if(port_num == 0){
		foreach(word, nvram_safe_get("ohci_ports"), next){
			++i;
			if(!strcmp(usb_port, word)){
				port_num = i;
				break;
			}
		}
	}

	return port_num;
}
#endif

#ifdef RTCONFIG_DUALWAN
void set_wanscap_support(char *feature)
{
	nvram_set("wans_cap", feature);
}

void add_wanscap_support(char *feature)
{
	char features[128];

	strcpy(features, nvram_safe_get("wans_cap"));

	if(strlen(features)==0) nvram_set("wans_cap", feature);
	else {
		sprintf(features, "%s %s", features, feature);
		nvram_set("wans_cap", features);
	}
}

int get_wans_dualwan(void) 
{
	int caps=0;
	char *wancaps = nvram_safe_get("wans_dualwan");

	if(strstr(wancaps, "lan")) caps |= WANSCAP_LAN;
	if(strstr(wancaps, "2g")) caps |= WANSCAP_2G;
	if(strstr(wancaps, "5g")) caps |= WANSCAP_5G;
	if(strstr(wancaps, "usb")) caps |= WANSCAP_USB;
	if(strstr(wancaps, "dsl")) caps |= WANSCAP_DSL;
	if(strstr(wancaps, "wan")) caps |= WANSCAP_WAN;

	return caps;
}

int get_dualwan_by_unit(int unit) 
{
	int i;
	char word[80], *next;

	i = 0;
	foreach(word, nvram_safe_get("wans_dualwan"), next) {
		if(i==unit) {
			if (!strcmp(word,"lan")) return WANS_DUALWAN_IF_LAN;
			if (!strcmp(word,"2g")) return WANS_DUALWAN_IF_2G;
			if (!strcmp(word,"5g")) return WANS_DUALWAN_IF_5G;
			if (!strcmp(word,"usb")) return WANS_DUALWAN_IF_USB;	
			if (!strcmp(word,"dsl")) return WANS_DUALWAN_IF_DSL;
			if (!strcmp(word,"wan")) return WANS_DUALWAN_IF_WAN;
			return WANS_DUALWAN_IF_NONE;
		}
		i++;
	}

	return WANS_DUALWAN_IF_NONE;
}

// imply: unit 0: primary, unit 1: secondary
int get_dualwan_primary(void)
{
	return get_dualwan_by_unit(0);
}

int get_dualwan_secondary(void) 
{
	return get_dualwan_by_unit(1);
}
#endif

// no more to use
/*
void set_dualwan_type(char *type)
{
	nvram_set("wans_dualwan", type);
}

void add_dualwan_type(char *type)
{
	char types[128];

	strcpy(types, nvram_safe_get("wans_dualwan"));

	if(strlen(types)==0) nvram_set("wans_dualwan", type);
	else {
		sprintf(types, "%s %s", types, type);
		nvram_set("wans_dualwan", types);
	}
}
*/

void set_lan_phy(char *phy)
{
	nvram_set("lan_ifnames", phy);
}

void add_lan_phy(char *phy)
{
	char phys[128];

	strcpy(phys, nvram_safe_get("lan_ifnames"));

	if(strlen(phys)==0) nvram_set("lan_ifnames", phy);
	else {
		sprintf(phys, "%s %s", phys, phy);
		nvram_set("lan_ifnames", phys);
	}
}

void set_wan_phy(char *phy)
{
	nvram_set("wan_ifnames", phy);
}

void add_wan_phy(char *phy)
{
	char phys[128];

	strcpy(phys, nvram_safe_get("wan_ifnames"));

	if(strlen(phys)==0) nvram_set("wan_ifnames", phy);
	else {
		sprintf(phys, "%s %s", phys, phy);
		nvram_set("wan_ifnames", phys);
	}
}

