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

#include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN
#include <linux/byteorder/big_endian.h>
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#include <linux/byteorder/little_endian.h>
#else
#error Unknown endian
#endif

int getStorageStatus(STORAGE_INFO_T *st)
{
	memset(st, sizeof(st), 0);

	st->AppHttpPort = __cpu_to_le16(nvram_get_int("dm_http_port"));

	/*
	if(nvram_get_int("sw_mode")!=SW_MODE_ROUTER) {
		return 0;
	}
	*/

	st->MagicWord = __cpu_to_le16(EXTEND_MAGIC);
	st->AppAPILevel = EXTEND_API_LEVEL;
	st->ExtendCap = 0;

#ifdef RTCONFIG_WEBDAV
	st->ExtendCap |= __cpu_to_le16(EXTEND_CAP_WEBDAV);
#else
	st->ExtendCap = 0;
	if(check_if_file_exist("/opt/etc/init.d/S50aicloud")) 
		st->ExtendCap |= __cpu_to_le16(EXTEND_CAP_WEBDAV);
#endif


#ifdef RTCONFIG_TUNNEL
	st->ExtendCap |= __cpu_to_le16(EXTEND_CAP_AAE_BASIC);
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

	st->u.dev.sw.AiHOMEAPILevel = EXTEND_AIHOME_API_LEVEL;

	// setup st->u.WANIPAddr
	st->u.wt.WANIPAddr = __cpu_to_le32(inet_network(get_wanip()));

	st->u.wt.isNotDefault = nvram_get_int("x_Setting");


#ifdef RTCONFIG_TUNNEL
	if(nvram_get_int("aae_enable"))
	{
		st->EnableAAE = 1;
		const char* tnl_devid = nvram_get("aae_deviceid");
		if(tnl_devid) {
			strcpy(st->AAEDeviceID, tnl_devid);
			printf("AAE DeviceID =%s <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n", st->AAEDeviceID);
		}
	}
#endif


	return 0;
}


