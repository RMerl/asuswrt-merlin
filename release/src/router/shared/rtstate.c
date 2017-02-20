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

void del_rc_support(char *features)
{
	char *tmp = nvram_safe_get("rc_support");
	char *rcsupport = NULL;

	if (!(features && *features))
		return;

	rcsupport = malloc(strlen(tmp) + 1);

	if (rcsupport == NULL) {
		_dprintf("del_rc_support fail\n");
		return;
	}
	memset(rcsupport, 0, strlen(tmp) + 1);
	strncpy(rcsupport, tmp, strlen(tmp));

	if (*rcsupport) {
		char word[256];
		char *next;

		foreach(word,features,next) {
			remove_from_list(word, rcsupport, strlen(tmp) + 1);
		}

		nvram_set("rc_support", rcsupport);
		free(rcsupport);
	}
	else{
		_dprintf("del_rc_support fail\n");
	}
}

int get_wan_state(int unit){
	char tmp[100], prefix[16];

	snprintf(prefix, 16, "wan%d_", unit);

	return nvram_get_int(strcat_r(prefix, "state_t", tmp));
}

int get_wan_sbstate(int unit){
	char tmp[100], prefix[16];

	snprintf(prefix, 16, "wan%d_", unit);

	return nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
}

int get_wan_auxstate(int unit){
	char tmp[100], prefix[16];

	snprintf(prefix, 16, "wan%d_", unit);

	return nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));
}

int is_wan_connect(int unit){
	int wan_state, wan_sbstate, wan_auxstate;

	wan_state = get_wan_state(unit);
	wan_sbstate = get_wan_sbstate(unit);
	wan_auxstate = get_wan_auxstate(unit);

	if(wan_state == 2 && wan_sbstate == 0 &&
			(wan_auxstate == 0 || wan_auxstate == 2)
			)
		return 1;
	else
		return 0;
}

int is_phy_connect(int unit){
	int wan_auxstate;

	wan_auxstate = get_wan_auxstate(unit);

	if(wan_auxstate == 0 || wan_auxstate == 2)
		return 1;
	else
		return 0;
}

int is_ip_conflict(int unit){
	int wan_state, wan_sbstate;

	wan_state = get_wan_state(unit);
	wan_sbstate = get_wan_sbstate(unit);

	if(wan_state == 4 && wan_sbstate == 4)
		return 1;
	else
		return 0;
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
	int unit = 0;
	int model = get_model();

	if(ifname == NULL)
		return -1;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(!strncmp(ifname, "ppp", 3) ){

			if(nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), ifname)) {
				if (model ==  MODEL_RTN65U) {
					if(!nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") || nvram_match(strcat_r(prefix, "is_usb_modem_ready", tmp), "1"))						
						return unit;
				}	
				else if (nvram_match(strcat_r(prefix, "state_t", tmp), "2") && nvram_match(strcat_r(prefix, "auxstate_t", tmp), "0") && nvram_match(strcat_r(prefix, "gw_ifname", tmp), ifname)) 
					return unit;				
			}

				
		}
		else if(nvram_match(strcat_r(prefix, "ifname", tmp), ifname)) {
			
			if (model == MODEL_RTN65U && !nvram_match(strcat_r(prefix, "proto", tmp), "l2tp") && !nvram_match(strcat_r(prefix, "proto", tmp), "pptp"))
					return unit;
			
			if (!nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") && !nvram_match(strcat_r(prefix, "proto", tmp), "l2tp") && !nvram_match(strcat_r(prefix, "proto", tmp), "pptp") && nvram_match(strcat_r(prefix, "gw_ifname", tmp), ifname))
					return unit;						
		}   
	}

	return -1;
}
#endif

// Get physical wan ifname of working connection
char *get_wanx_ifname(int unit)
{
	char *wan_ifname;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	return wan_ifname;
}

// Get wan ifname of working connection
char *get_wan_ifname(int unit)
{
	char *wan_proto, *wan_ifname;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(unit)) {
		if (strcmp(wan_proto, "dhcp") == 0)
			wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		else
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	} else
#endif
	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
	} else
		wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	return wan_ifname;
}

// Get wan ipv6 ifname of working connection
#ifdef RTCONFIG_IPV6
char *get_wan6_ifname(int unit)
{
	char *wan_proto, *wan_ifname;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];

	switch (get_ipv6_service_by_unit(unit)) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

#ifdef RTCONFIG_USB_MODEM
		if (dualwan_unit__usbif(unit)) {
			if (strcmp(wan_proto, "dhcp") == 0)
				wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
			else
				wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
		} else
#endif
		if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
		    nvram_match(ipv6_nvname_by_unit("ipv6_ifdev", unit), "ppp")) {
			wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp));
		} else
			wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		break;
	case IPV6_6TO4:
	case IPV6_6IN4:
	case IPV6_6RD:
		/* no ipv6 multiwan tunnel support so far */
		wan_ifname = "v6tun0";
		break;
	default:
		return "";
	}

	return wan_ifname;
}
#endif

// OR all lan port status
int get_lanports_status(void)
{
	return lanport_status();
}

extern int wanport_status(int wan_unit);

// OR all wan port status
int get_wanports_status(int wan_unit)
{
// 1. PHY type, 2. factory owner, 3. model.
#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_DSL)
#endif
	{
		if (nvram_match("dsltmp_adslsyncsts","up")) return 1;
		return 0;
	}
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN)
	{
	#ifdef RTCONFIG_RALINK
		return rtkswitch_wanPort_phyStatus(wan_unit); //Paul modify 2012/12/4
	#else
		return wanport_status(wan_unit);
	#endif
	}
#endif
	// TO CHENI:
	// HOW TO HANDLE USB?	
#else // RJ-45
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	return rtkswitch_wanPort_phyStatus(wan_unit);
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

int
wan_primary_ifunit_ipv6(void)
{
#ifdef RTCONFIG_DUALWAN
#if 0
	int unit = wan_primary_ifunit();

	if (!strstr(nvram_safe_get("wans_dualwan"), "none")
	    && !strcmp(nvram_safe_get("wans_mode"), "lb")
#ifdef RTCONFIG_IPV6
	    && get_ipv6_service_by_unit(unit) == IPV6_DISABLED
#endif
	)
		return (1 - unit);

	return unit;
#else
	return 0;
#endif
#else
	return wan_primary_ifunit();
#endif
}

#ifdef RTCONFIG_MEDIA_SERVER
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
#endif	/* RTCONFIG_MEDIA_SERVER */

#ifdef RTCONFIG_USB

char xhci_string[32];
char ehci_string[32];
char ohci_string[32];

char *get_usb_xhci_port(int port)
{
        char word[100], *next;
        int i=0;

        strcpy(xhci_string, "xxxxxxxx");

        foreach(word, nvram_safe_get("xhci_ports"), next) {
                if(i==port) {
                        strcpy(xhci_string, word);
                        break;
                }
                i++;
        }
        return xhci_string;
}

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

int get_usb_port_number(const char *usb_port)
{
	char word[100], *next;
	int port_num, i;

	port_num = 0;
	i = 0;
	foreach(word, nvram_safe_get("xhci_ports"), next){
		++i;
		if(!strcmp(usb_port, word)){
			port_num = i;
			break;
		}
	}

	i = 0;
	if(port_num == 0){
		foreach(word, nvram_safe_get("ehci_ports"), next){
			++i;
			if(!strcmp(usb_port, word)){
				port_num = i;
				break;
			}
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

int get_usb_port_host(const char *usb_port)
{
	char word[100], *next;
	int i;

	i = 0;
	foreach(word, nvram_safe_get("xhci_ports"), next){
		++i;
		if(!strcmp(usb_port, word)){
			return USB_HOST_XHCI;
		}
	}

	i = 0;
	foreach(word, nvram_safe_get("ehci_ports"), next){
		++i;
		if(!strcmp(usb_port, word)){
			return USB_HOST_EHCI;
		}
	}

	i = 0;
	foreach(word, nvram_safe_get("ohci_ports"), next){
		++i;
		if(!strcmp(usb_port, word)){
			return USB_HOST_OHCI;
		}
	}

	return USB_HOST_NONE;
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
	char word[80], *next;
	char *wancaps = nvram_get("wans_dualwan");

	if(wancaps == NULL)
	{
#ifdef RTCONFIG_DSL
		caps =  WANSCAP_DSL;
#else
		caps = WANSCAP_WAN;
#endif
		wancaps = DEF_SECOND_WANIF;
	}

	foreach(word, wancaps, next) {
		if (!strcmp(word,"lan")) caps |= WANSCAP_LAN;
		if (!strcmp(word,"2g")) caps |= WANSCAP_2G;
		if (!strcmp(word,"5g")) caps |= WANSCAP_5G;
		if (!strcmp(word,"usb")) caps |= WANSCAP_USB;
		if (!strcmp(word,"dsl")) caps |= WANSCAP_DSL;
		if (!strcmp(word,"wan")) caps |= WANSCAP_WAN;
		if (!strcmp(word,"wan2")) caps |= WANSCAP_WAN2;
	}

	return caps;
}

int get_dualwan_by_unit(int unit) 
{
	int i;
	char word[80], *next;
	char *wans_dualwan = nvram_get("wans_dualwan");

	if(wans_dualwan == NULL)	//default value
	{
		wans_dualwan = nvram_default_get("wans_dualwan");
	}

#ifdef RTCONFIG_MULTICAST_IPTV
	if(unit == WAN_UNIT_IPTV)
		return WAN_UNIT_IPTV;
        if(unit == WAN_UNIT_VOIP)
                return WAN_UNIT_VOIP;
#endif

	i = 0;
	foreach(word, wans_dualwan, next) {
		if(i==unit) {
			if (!strcmp(word,"lan")) return WANS_DUALWAN_IF_LAN;
			if (!strcmp(word,"2g")) return WANS_DUALWAN_IF_2G;
			if (!strcmp(word,"5g")) return WANS_DUALWAN_IF_5G;
			if (!strcmp(word,"usb")) return WANS_DUALWAN_IF_USB;	
			if (!strcmp(word,"dsl")) return WANS_DUALWAN_IF_DSL;
			if (!strcmp(word,"wan")) return WANS_DUALWAN_IF_WAN;
			if (!strcmp(word,"wan2")) return WANS_DUALWAN_IF_WAN2;
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

/**
 * Return total number of WAN unit.
 * @return:
 */
int get_nr_wan_unit(void)
{
	int i, c = 0;

	for (i = WAN_UNIT_FIRST; i < WAN_UNIT_MAX; ++i) {
		if (get_dualwan_by_unit(i) != WANS_DUALWAN_IF_NONE)
			c++;
	}

	return c;
}
#endif	/* RTCONFIG_DUALWAN */

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

