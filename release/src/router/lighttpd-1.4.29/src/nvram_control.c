/*
	nvram control 
 */

#if EMBEDDED_EANBLE
#include "nvram_control.h"
#include "log.h"

#include <utils.h>
#include <shutils.h>
#include <shared.h>

#define DDNS_ENANBLE_X	"ddns_enable_x"
#define DDNS_SERVER_X	"ddns_server_x"
#define DDNS_HOST_NAME_X	"ddns_hostname_x"
#define WEBDAV_SMB_PC	"webdav_smb_pc"
#define PRODUCT_ID "productid"
#define COMPUTER_NAME "computer_name"
#define ACC_LIST "acc_list"
#define ACC_WEBDAVPROXY "acc_webdavproxy"
#define ST_SAMBA_MODE "st_samba_mode"
#define HTTP_USERNAME "http_username"
#define HTTP_PASSWD "http_passwd"
#define WEBDAVAIDISK "webdav_aidisk"
#define WEBDAVPROXY "webdav_proxy"
#define SHARELINK "share_link"
#define ETHMACADDR "et0macaddr"
#define FIRMVER "firmver"
#define BUILDNO "buildno"
#define ST_WEBDAV_MODE "st_webdav_mode"
#define WEBDAV_HTTP_PORT "webdav_http_port"
#define WEBDAV_HTTPS_PORT "webdav_https_port"
#define MISC_HTTP_X "misc_http_x"
#define MISC_HTTP_PORT "misc_httpport_x"
#define ENABLE_WEBDAV_CAPTCHA "enable_webdav_captcha"
#define ENABLE_WEBDAV_LOCK "enable_webdav_lock"
#define WEBDAV_ACC_LOCK "webdav_acc_lock"
#define WEBDAV_LOCK_INTERVAL "webdav_lock_interval"
#define WEBDAV_LOCK_TIMES "webdav_lock_times"
#define WEBDAV_LAST_LOGININFO "webdav_last_login_info"
#define WEBS_STATE_INFO "webs_state_info"
#define WEBS_STATE_ERROR "webs_state_error"

#define DBE 0

int nvram_smbdav_pc_append(const char* ap_str )
{
	char* nv_var_val=NULL;
	Cdbg(1,"call nvram_get");
	if( !( nv_var_val = nvram_get(WEBDAV_SMB_PC)) ){
	   Cdbg(1,"nv_var get null");
	   return -1;
	}
	char tmp_nv_var_val[120]={0};
	strcpy(tmp_nv_var_val, nv_var_val);
	strcat(tmp_nv_var_val, ap_str);
	nvram_set(WEBDAV_SMB_PC, tmp_nv_var_val);
//	char * test_str = nvram_get(nv_var);
	return 0;
}

char*  nvram_get_smbdav_str()
{	
   return  nvram_get(WEBDAV_SMB_PC);
}

int nvram_set_smbdav_str(const char* pc_info)
{
	char* set_str = nvram_set(WEBDAV_SMB_PC, pc_info);
	return set_str;
}

char*  nvram_get_sharelink_str()
{	
   return  nvram_get(SHARELINK);
}

int nvram_set_sharelink_str(const char* share_info)
{
	char* set_str = nvram_set(SHARELINK, share_info);
	return set_str;
}

int nvram_do_commit(){
	nvram_commit();
	return 1;
}

int nvram_is_ddns_enable()
{
	char*	ddns_e=NULL;
	int		ddns_enable_x=0;
	if( (ddns_e = nvram_get(DDNS_ENANBLE_X))!=NULL ){
		ddns_enable_x = atoi(ddns_e);
		Cdbg(DBE," ddns_e = %s", ddns_e);
	}
	if(ddns_enable_x)	return 1;
	else				return 0;
}

char* nvram_get_ddns_server_name()
{
	return nvram_get(DDNS_SERVER_X);
}

char* nvram_get_ddns_host_name()
{
	/*
	nvram get/set ddns_enable_x
	nvram get/set ddns_server_x (WWW.ASUS.COM)
	nvram get/set ddns_hostname_x (RT-N66U-00E012112233.asuscomm.com)
	rc rc_service restart_ddns or notify_rc(“restart_ddns”) through libshared
	 */
	char* ddns_host_name_x=NULL;
	if(!nvram_is_ddns_enable())
	   goto nvram_get_ddns_host_name_EXIT;
	if(!nvram_get_ddns_server_name) 
	   goto nvram_get_ddns_host_name_EXIT;
	 ddns_host_name_x= nvram_get (DDNS_HOST_NAME_X);  
	
nvram_get_ddns_host_name_EXIT:
	Cdbg(DBE,"ddns_hostname_x = %s", ddns_host_name_x);
	return ddns_host_name_x;
}

char* nvram_get_productid()
{
	return get_productid();
}

char* nvram_get_acc_list()
{
	return nvram_get(ACC_LIST);
}

char* nvram_get_webdavaidisk()
{
	return nvram_get(WEBDAVAIDISK);
}

char* nvram_get_webdavproxy()
{
	return nvram_get(WEBDAVPROXY);
}

char* nvram_get_acc_webdavproxy()
{
	return nvram_get(ACC_WEBDAVPROXY);
}

int nvram_get_st_samba_mode()
{
	char* res = nvram_get(ST_SAMBA_MODE);
	int a = atoi(res);
	return a;
}

char* nvram_get_http_username()
{
	return nvram_get(HTTP_USERNAME);
}

char* nvram_get_http_passwd()
{
	return nvram_get(HTTP_PASSWD);
}

char* nvram_get_computer_name()
{
	return nvram_get(COMPUTER_NAME);
}

char* nvram_get_router_mac()
{
	return nvram_get(ETHMACADDR);
}

char* nvram_get_firmware_version()
{
	return nvram_get(FIRMVER);
}

char* nvram_get_build_no()
{
	return nvram_get(BUILDNO);
}

char* nvram_get_st_webdav_mode()
{
	return nvram_get(ST_WEBDAV_MODE);
}

char* nvram_get_webdav_http_port()
{
	return nvram_get(WEBDAV_HTTP_PORT);
}

char* nvram_get_webdav_https_port()
{
	return nvram_get(WEBDAV_HTTPS_PORT);
}

char* nvram_get_misc_http_x()
{
	return nvram_get(MISC_HTTP_X);
}

char* nvram_get_msie_http_port()
{
	return nvram_get(MISC_HTTP_PORT);
}

char* nvram_get_enable_webdav_captcha()
{
	return nvram_get(ENABLE_WEBDAV_CAPTCHA);
}

char* nvram_get_enable_webdav_lock()
{
	return nvram_get(ENABLE_WEBDAV_LOCK);
}

char* nvram_get_webdav_acc_lock()
{
	return nvram_get(WEBDAV_ACC_LOCK);
}

int nvram_set_webdav_acc_lock(const char* acc_lock)
{
	nvram_set(WEBDAV_ACC_LOCK, acc_lock);
	return 1;
}

char* nvram_get_webdav_lock_interval()
{
	return nvram_get(WEBDAV_LOCK_INTERVAL);
}

char* nvram_get_webdav_lock_times()
{
	return nvram_get(WEBDAV_LOCK_TIMES);
}

char* nvram_get_webdav_last_login_info()
{
	return nvram_get(WEBDAV_LAST_LOGININFO);
}

int nvram_set_webdav_last_login_info(const char* last_login_info)
{
	char* set_str = nvram_set(WEBDAV_LAST_LOGININFO, last_login_info);
	return set_str;
}

char* nvram_get_latest_version()
{
	return nvram_get(WEBS_STATE_INFO);
}

int nvram_get_webs_state_error()
{
	char* res = nvram_get(WEBS_STATE_ERROR);
	int a = atoi(res);
	return a;
}

#endif
