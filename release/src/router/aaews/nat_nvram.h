#ifndef _NAT_NVRAM_H
#define _NAT_NVRAM_H
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
