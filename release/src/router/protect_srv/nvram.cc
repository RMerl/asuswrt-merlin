#include <stdio.h>
#include <stdlib.h>
#include <ptcsrv_nvram.h>

int IsLoadBalanceMode(void)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	if (nvram_match("wans_mode", "lb"))
		return 1;
#else /* DSL_ASUSWRT SDK */
 #ifdef RTCONFIG_DUALWAN
	if (tcapi_match("Dualwan_Entry", "wans_mode", "lb"))
		return 1;
 #endif
#endif
	return 0;
}

char *GetWanIpaddr(char *ret, size_t len)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *wan_ipaddr;
	char prefix[16], tmp[100];
	
	snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_ifunit());
	wan_ipaddr = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	
	if (!strcmp(wan_ipaddr, "0.0.0.0")) {
		return NULL;
	} else 
		strncpy(ret, wan_ipaddr, len-1);
	
#else /* DSL_ASUSWRT SDK */
	int retval;
	char value[32], buf[128];
	
	snprintf(value, sizeof(value), "wan%d_ipaddr", tcapi_get_int("Wanduck_Common", "wan_primary"));
	retval = tcapi_get("Wanduck_Common", value, buf);
	
	if (retval < 0)
		return NULL;
	
	strncpy(ret, buf, len-1);
	
#endif
	return ret;
}

char *GetWanNetmask(char *ret, size_t len)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *wan_netmask;
	char prefix[16], tmp[100];
	
	snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_ifunit());
	wan_netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
	
	if (!strcmp(wan_netmask, "0.0.0.0")) {
		return NULL;
	} else 
		strncpy(ret, wan_netmask, len-1);
	
#else /* DSL_ASUSWRT SDK */
	int retval;
	char value[32], buf[128];
	
	memset(buf, 0, sizeof(buf));
	snprintf(value, sizeof(value), "wan%d_netmask", tcapi_get_int("Wanduck_Common", "wan_primary"));
	retval = tcapi_get("Wanduck_Common", value, buf);
	
	if (retval < 0)
		return NULL;
	
	strncpy(ret, buf, len-1);
	
#endif
	return ret;
}

char *GetSecWanIpaddr(char *ret, size_t len)
{
	if (!IsLoadBalanceMode()) {
		return NULL;
	}
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *wan_ipaddr;
	char prefix[16], tmp[100];
	
 #if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	snprintf(prefix, sizeof(prefix), "wan%d_", WAN_UNIT_SECOND);
	wan_ipaddr = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
	
	if (!strcmp(wan_ipaddr, "0.0.0.0")) {
		return NULL;
	} else {
		strncpy(ret, wan_ipaddr, len-1);
		return ret;
	}
	
 #endif

#else /* DSL_ASUSWRT SDK */
 #ifdef RTCONFIG_DUALWAN
	int retval;
	int wan_secondary;
	char value[32], buf[128];
	
	wan_secondary = tcapi_get_int("Wanduck_Common", "wan_secondary");
	if (wan_secondary != -1) {
		memset(buf, 0, sizeof(buf));
		snprintf(value, sizeof(value), "wan%d_ipaddr", wan_secondary);
		retval = tcapi_get("Wanduck_Common", value, buf);
		
		if (retval < 0)
			return NULL;
		
		strncpy(ret, buf, len-1);
		return ret;
	}
 #endif
#endif
	return NULL;
}

char *GetSecWanNetmask(char *ret, size_t len)
{
	if (!IsLoadBalanceMode()) {
		return NULL;
	}
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *wan_netmask;
	char prefix[16], tmp[100];
	
 #if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	snprintf(prefix, sizeof(prefix), "wan%d_", WAN_UNIT_SECOND);
	wan_netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
	
	if (!strcmp(wan_netmask, "0.0.0.0")) {
		return NULL;
	} else {
		strncpy(ret, wan_netmask, len-1);
		return ret;
	}
 #endif
	
#else /* DSL_ASUSWRT SDK */
	int retval;
	int wan_secondary;
	char value[32], buf[128];
	
	wan_secondary = tcapi_get_int("Wanduck_Common", "wan_secondary");
	if (wan_secondary != -1) {
		memset(buf, 0, sizeof(buf));
		snprintf(value, sizeof(value), "wan%d_netmask", wan_secondary);
		retval = tcapi_get("Wanduck_Common", value, buf);
		
		if (retval < 0)
			return NULL;
		
		strncpy(ret, buf, len-1);
		return ret;
	}
	
#endif
	return NULL;
}

char *GetLanIpaddr(char *ret, size_t len)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *lan_ipaddr;
	
	lan_ipaddr = nvram_safe_get("lan_ipaddr");
	
	if (!strcmp(lan_ipaddr, "0.0.0.0")) {
		return NULL;
	} else 
		strncpy(ret, lan_ipaddr, len-1);
	
#else /* DSL_ASUSWRT SDK */
	int retval;
	char buf[128];
	
	memset(buf, 0, sizeof(buf));
	retval = tcapi_get("Lan_Entry0", "IP", buf);
	
	if (retval < 0)
		return NULL;
	
	strncpy(ret, buf, len-1);
	
#endif
	return ret;
}

char *GetLanNetmask(char *ret, size_t len)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	char *lan_netmask;
	
	lan_netmask = nvram_safe_get("lan_netmask");
	
	if (!strcmp(lan_netmask, "0.0.0.0")) {
		return NULL;
	} else 
		strncpy(ret, lan_netmask, len-1);
	
#else /* DSL_ASUSWRT SDK */
	int retval;
	char buf[128];
	
	memset(buf, 0, sizeof(buf));
	retval = tcapi_get("Lan_Entry0", "netmask", buf);
	
	if (retval < 0)
		return NULL;
	
	strncpy(ret, buf, len-1);
	
#endif
	return ret;
}

int GetSSHport(void)
{
#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
	return nvram_get_int("sshd_port") ? : 22;
#else /* DSL_ASUSWRT SDK */
	return tcapi_get_int("SSH_Entry", "sshport");
#endif
}

