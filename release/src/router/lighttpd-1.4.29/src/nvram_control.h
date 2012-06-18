/*
   header of nvram_control
   */

//#include <bcmnvram.h>

int nvram_smbdav_pc_append(const char* ap_str );

char*  nvram_get_smbdav_str();

int nvram_set_smbdav_str(const char* pc_info);
char*  nvram_get_sharelink_str();

int nvram_set_sharelink_str(const char* sharelink_info);

int nvram_do_commit();

int nvram_is_ddns_enable();

char* nvram_get_ddns_server_name();
char* nvram_get_ddns_host_name();
int nvram_get_st_samba_mode();
char* nvram_get_http_username();
char* nvram_get_http_passwd();
char* nvram_get_computer_name();
char* nvram_get_webdavaidisk();
char* nvram_get_webdavproxy();

char* nvram_get_router_mac();
char* nvram_get_firmware_version();
char* nvram_get_build_no();



















