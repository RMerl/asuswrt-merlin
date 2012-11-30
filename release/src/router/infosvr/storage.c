#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>
#include <syslog.h>
#include "iboxcom.h"

int getStorageStatus(STORAGE_INFO_T *st)
{
	memset(st, sizeof(st), 0);

	st->AppHttpPort = nvram_get_int("dm_http_port");

	if(nvram_get_int("sw_mode")!=SW_MODE_ROUTER) {
		return 0;
	}

	st->MagicWord = EXTEND_MAGIC;

#ifdef RTCONFIG_WEBDAV
	st->ExtendCap = EXTEND_CAP_WEBDAV;
#else
	st->ExtendCap = nvram_get_int("webdav_extend_cap");
#endif

	if(nvram_get_int("enable_webdav")) 	
		st->u.wt.EnableWebDav = 1;
	else
		st->u.wt.EnableWebDav = 0;

	st->u.wt.HttpType = nvram_get_int("st_webdav_mode");
	st->u.wt.HttpPort = htons(nvram_get_int("webdav_http_port"));
	st->u.wt.HttpsPort = htons(nvram_get_int("webdav_https_port"));

	if(nvram_get_int("ddns_enable_x")) {
		st->u.wt.EnableDDNS = 1;
		snprintf(st->u.wt.HostName, sizeof(st->u.wt.HostName), nvram_safe_get("ddns_hostname_x"));
	}
	else {
		st->u.wt.EnableDDNS = 0;
	}

	// setup st->u.WANIPAddr
	st->u.wt.WANIPAddr = inet_network(get_wanip());

	st->u.wt.WANState = get_wanstate(); 
	st->u.wt.isNotDefault = nvram_get_int("x_Setting");
	return 0;
}


