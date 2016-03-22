#ifndef _NAT_NVRAM_H
#define _NAT_NVRAM_H
#define AAE_DEVID	"aae_deviceid"
#define AAE_ENABLE	"aae_enable"
#define AAE_STATUS	"aae_status"
#define AAE_USERNAME	"aae_username"
#define AAE_PWD		"aae_password"
#define AAE_SDK_LOG_LEVEL "aae_sdk_log_level"
#define ROUTER_MAC	"lan_hwaddr"
#define LINK_INTERNET 	"link_internet"
//#define AAE_SEM_NAME "AAE_ENABLE_SEM"
#define NV_DBG 1
int nvram_is_aae_enable();
int nvram_set_aae_enable(const char* aae_enable);
int nvram_set_aae_status(const char* aae_status );
int nvram_get_aae_pwd(char** aae_pwd);
int nvram_get_aae_username(char** aae_username);
int nvram_set_aae_info(const char* deviceid);
int nvram_get_aae_sdk_log_level();
int nvram_set_aae_sdk_log_level(const char* aae_sdk_log_level);
void WatchingNVram();
int nvram_get_mac_addr(char* mac_addr);
int nvram_get_link_internet();
#endif
