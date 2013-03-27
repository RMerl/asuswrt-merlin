/*
	nvram control 
 */

#if EMBEDDED_EANBLE
#include "nvram_control.h"
#include "log.h"

#ifndef APP_IPKG
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#else
#include<stdlib.h> //for system cmd by zero added
#endif

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
#define HTTP_ENABLE "http_enable"
#define MISC_HTTP_X "misc_http_x"
#define MISC_HTTP_PORT "misc_httpport_x"
#define MISC_HTTPS_PORT "misc_httpsport_x"
#define ENABLE_WEBDAV_CAPTCHA "enable_webdav_captcha"
#define ENABLE_WEBDAV_LOCK "enable_webdav_lock"
#define WEBDAV_ACC_LOCK "webdav_acc_lock"
#define WEBDAV_LOCK_INTERVAL "webdav_lock_interval"
#define WEBDAV_LOCK_TIMES "webdav_lock_times"
#define WEBDAV_LAST_LOGININFO "webdav_last_login_info"
#define WEBS_STATE_INFO "webs_state_info"
#define WEBS_STATE_ERROR "webs_state_error"
#define SHARE_LINK_PARAM "share_link_param"
#define SHARE_LINK_RESULT "share_link_result"
#define TIME_ZONE_X "time_zone_x"

#define DBE 0

#ifdef APP_IPKG
static inline char * strcat_r(const char *s1, const char *s2, char *buf)
{
        strcpy(buf, s1);
        strcat(buf, s2);
        return buf;
}

char *nvram_get(char *name);

char *get_productid(void)
{
        char *productid = nvram_get("productid");
#ifdef RTCONFIG_ODMPID
        char *odmpid = nvram_get("odmpid");
        if (*odmpid)
                productid = odmpid;
#endif
        return productid;
}

char *nvram_get(char *name)
{
#if 1
    if(strcmp(name,"webdav_aidisk")==0 ||strcmp(name,"webdav_proxy")==0||strcmp(name,"webdav_smb_pc")==0
       ||strcmp(name,"share_link")==0||strcmp(name,"webdav_acc_lock")==0
       ||strcmp(name,"webdav_last_login_info")==0)
    {
        char tmp_name[256]="/opt/etc/asus_script/aicloud_nvram_check.sh";
        //char tmp_name[256]="/tmp/aicloud_nvram_check.sh";
        char *cmd_name;
        cmd_name=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
        memset(cmd_name,0,sizeof(cmd_name));
        sprintf(cmd_name,"%s %s",tmp_name,name);
        system(cmd_name);
        free(cmd_name);

        while(-1!=access("/tmp/aicloud_check.control",F_OK))
            usleep(50);
    }
#endif
    FILE *fp;
    if((fp=fopen("/tmp/webDAV.conf","r+"))==NULL)
    {
        return NULL;
    }
    char *value;
    //value=(char *)malloc(256);
    //memset(value,'\0',sizeof(value));
    char tmp[256]={0};
    while(!feof(fp)){
        memset(tmp,0,sizeof(tmp));
        fgets(tmp,sizeof(tmp),fp);
        if(strncmp(tmp,name,strlen(name))==0)
        {
            char *p=NULL;
            p=strchr(tmp,'=');
            p++;
            value=(char *)malloc(strlen(p)+1);
            memset(value,'\0',sizeof(value));
            strcpy(value,p);
            if(value[strlen(value)-1]=='\n')
                value[strlen(value)-1]='\0';
        }
    }
    fclose(fp);
    return value;
}
char *nvram_safe_get(char *name)
{
#if 1
    if(strcmp(name,"webdav_aidisk")==0 ||strcmp(name,"webdav_proxy")==0||strcmp(name,"webdav_smb_pc")==0
       ||strcmp(name,"share_link")==0||strcmp(name,"webdav_acc_lock")==0
       ||strcmp(name,"webdav_last_login_info")==0)
    {
        char tmp_name[256]="/opt/etc/asus_script/aicloud_nvram_check.sh";
        //char tmp_name[256]="/tmp/aicloud_nvram_check.sh";
        char *cmd_name;
        cmd_name=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
        memset(cmd_name,0,sizeof(cmd_name));
        sprintf(cmd_name,"%s %s",tmp_name,name);
        system(cmd_name);
        free(cmd_name);

        while(-1!=access("/tmp/aicloud_check.control",F_OK))
            usleep(50);
    }
#endif

    FILE *fp;
    if((fp=fopen("/tmp/webDAV.conf","r+"))==NULL)
    {
        return NULL;
    }
    char *value;
    //value=(char *)malloc(256);
    //memset(value,'\0',sizeof(value));
    char tmp[256]={0};
    while(!feof(fp)){
        memset(tmp,0,sizeof(tmp));
        fgets(tmp,sizeof(tmp),fp);
        if(strncmp(tmp,name,strlen(name))==0)
        {
            char *p=NULL;
            p=strchr(tmp,'=');
            p++;
            value=(char *)malloc(strlen(p)+1);
            memset(value,'\0',sizeof(value));
            strcpy(value,p);
            if(value[strlen(value)-1]=='\n')
                value[strlen(value)-1]='\0';
        }
    }
    fclose(fp);
    return value;
}
int nvram_set(const char *name, const char *value)
{
    char *cmd;
    cmd=(char *)malloc(sizeof(char)*(32+strlen(name)+strlen(value)));
    memset(cmd,0,sizeof(cmd));
    sprintf(cmd,"nvram set \"%s=%s\"",name,value);
    system(cmd);
    free(cmd);

    size_t count = strlen(name) + 1;
    char tmp[100];
    char *buf = tmp;
    int ret;
    FILE *fp;
    char tmp_name[100]={0};
    /* Unset if value is NULL */
    if (value) count += strlen(value) + 1;

    if (count > sizeof(tmp)) {
        if ((buf = malloc(count)) == NULL) return -1;
    }

    if (value) {
        sprintf(buf, "%s=%s", name, value);
    }
    else {
        strcpy(buf, name);
    }
    if((fp=fopen("/tmp/webDAV.conf","r+"))==NULL)
    {
        return -1;
    }
    while(!feof(fp)){
        fgets(tmp_name,sizeof(tmp_name),fp);
        if(strncmp(tmp_name,name,strlen(name))==0)
        {
            char *cmd_name;
            cmd_name=(char *)malloc(sizeof(char)*(64+2*strlen(name)+strlen(value)));
            memset(cmd_name,0,sizeof(cmd_name));
            if(strcmp(name,"webdav_last_login_info")==0)
            {

                char tmp_name[256]="/opt/etc/asus_script/aicloud_nvram_check.sh";
                char *cmd_name_1;
                cmd_name_1=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
                memset(cmd_name_1,0,sizeof(cmd_name));
                sprintf(cmd_name_1,"%s %s",tmp_name,name);
                system(cmd_name_1);
                //fprintf(stderr,"cmd_name_1=%s\n",cmd_name_1);
                free(cmd_name_1);


#if 0
                char value_tmp[256]={0};
                FILE *fp_1=fopen("/tmp/aicloud_nvram.txt","r+");
                fgets(value_tmp,sizeof(value_tmp),fp_1);
                fclose(fp_1);
                sprintf(cmd_name,"sed -i 's/^%s.*/%s=%s/g' /tmp/webDAV.conf",name,name,value_tmp);
                system(cmd_name);
#endif
            }
            else
            {
                sprintf(cmd_name,"sed -i 's/^%s.*/%s=%s/g' /tmp/webDAV.conf",name,name,value);
                system(cmd_name);
            }
            //printf("tmp_name = %s\n",tmp_name);
            //ret = fwrite(buf,sizeof(buf),1,fp);
            //ret = fprintf(fp,"%s\n",buf);
            free(cmd_name);
            if (buf != tmp) free(buf);
            fclose(fp);
            return 0;
            //return (ret == count) ? 0 : ret;
        }
    }
    ret = fprintf(fp,"%s\n",buf);
    //ret = fwrite(buf,sizeof(buf),1,fp);

    if (buf != tmp) free(buf);
    fclose(fp);
    return (ret == count) ? 0 : ret;
}

int nvram_commit(void)
{
#if 0
        FILE *fp;
        fp = fopen("/var/log/commit_ret", "w");
         fprintf(fp,"commit: OK\n");
        fclose(fp);
#endif
        char cmd[]="nvram commit";
        system(cmd);
        return 1;
}

#endif
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
#ifdef APP_IPKG
	free(nv_var_val);
#endif
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
		#ifdef APP_IPKG
		free(ddns_e);
		#endif
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
	rc rc_service restart_ddns or notify_rc(?œrestart_ddns?? through libshared
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

char* nvram_get_ddns_host_name2()
{
	char* ddns_host_name_x=NULL;
	ddns_host_name_x= nvram_get(DDNS_HOST_NAME_X);
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

int nvram_set_webdavaidisk(const char* enable)
{
	nvram_set(WEBDAVAIDISK, enable);
	return 1;
}

char* nvram_get_webdavproxy()
{
	return nvram_get(WEBDAVPROXY);
}

int nvram_set_webdavproxy(const char* enable)
{
	nvram_set(WEBDAVPROXY, enable);
	return 1;
}

char* nvram_get_acc_webdavproxy()
{
	return nvram_get(ACC_WEBDAVPROXY);
}

int nvram_get_st_samba_mode()
{
	char* res = nvram_get(ST_SAMBA_MODE);
	int a = atoi(res);
#ifdef APP_IPKG
	free(res);
#endif
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

char* nvram_get_http_enable()
{
	// 0 --> http
    // 1 --> https
    // 2 --> both
	return nvram_get(HTTP_ENABLE);
}

char* nvram_get_misc_http_x()
{
	return nvram_get(MISC_HTTP_X);
}

char* nvram_get_misc_http_port()
{
	return nvram_get(MISC_HTTP_PORT);
}

char* nvram_get_misc_https_port()
{
	return nvram_get(MISC_HTTPS_PORT);
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
#ifdef APP_IPKG
	free(res);
#endif
	return a;
}

char* nvram_get_share_link_param()
{
	return nvram_get(SHARE_LINK_PARAM);
}

char* nvram_get_time_zone()
{
	return nvram_get(TIME_ZONE_X);
}


int nvram_set_share_link_result(const char* result)
{
	nvram_set(SHARE_LINK_RESULT, result);
	return 1;
}

int nvram_wan_primary_ifunit()
{	
	int unit;	
	for (unit = 0; unit < 10; unit ++) {		
		char tmp[100], prefix[] = "wanXXXXXXXXXX_";		
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		char* res = strcat_r(prefix, "primary", tmp);		
		if (strncmp(res, "1", 1)==0)			
			return unit;	
	}	
	return 0;
}

char* nvram_get_wan_ip(){
	char *wan_ip;
	char tmp[32], prefix[] = "wanXXXXXXXXXX_";
	int unit = nvram_wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_ip = nvram_get(strcat_r(prefix, "ipaddr", tmp));
	return wan_ip;
}
#endif
