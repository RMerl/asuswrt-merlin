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
char* nvram_get_ddns_host_name2();
int nvram_get_st_samba_mode();
char* nvram_get_http_username();
char* nvram_get_http_passwd();
char* nvram_get_computer_name();
char* nvram_get_webdavaidisk();
int nvram_set_webdavaidisk(const char* enable);
char* nvram_get_webdavproxy();
int nvram_set_webdavproxy(const char* enable);

char* nvram_get_router_mac();
char* nvram_get_firmware_version();
char* nvram_get_build_no();

char* nvram_get_st_webdav_mode();
char* nvram_get_webdav_http_port();
char* nvram_get_webdav_https_port();

char* nvram_get_http_enable();
char* nvram_get_misc_http_x();
char* nvram_get_misc_http_port();
char* nvram_get_misc_https_port();


char* nvram_get_enable_webdav_captcha();

char* nvram_get_enable_webdav_lock();

char* nvram_get_webdav_acc_lock();
int nvram_set_webdav_acc_lock(const char* acc_block);

char* nvram_get_webdav_lock_interval();

char* nvram_get_webdav_lock_times();

char* nvram_get_webdav_last_login_info();
int nvram_set_webdav_last_login_info(const char* last_login_info);
char* nvram_get_latest_version();
int nvram_get_webs_state_error();

char* nvram_get_share_link_param();
char* nvram_get_time_zone();
int nvram_set_share_link_result(const char* result);
int nvram_wan_primary_ifunit();
char* nvram_get_wan_ip();
char* nvram_get_swpjverno();
char* nvram_get_extendno();
char* nvram_get_dms_enable();
char* nvram_get_dms_dbcwd();
char* nvram_get_dms_dir();
char* nvram_get_ms_enable();
int nvram_set_https_crt_save(const char* enable);
int nvram_set_https_crt_cn(const char* cn);
char* nvram_get_https_crt_cn();
int nvram_setfile_https_crt_file(const char* file, int size);
int nvram_getfile_https_crt_file(const char* file, int size);
char* nvram_get_https_crt_file();
char* nvram_get_odmpid();

int nvram_set_value(const char* key, const char* value);
char* nvram_get_value(const char* key);

int check_aicloud_db(const char* username, const char* password);

/* for hostspot module */
char* nvram_get_uamsecret(const char *str);





























