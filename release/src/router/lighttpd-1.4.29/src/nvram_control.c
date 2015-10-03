/*
	nvram control 
 */

#if EMBEDDED_EANBLE
#include "nvram_control.h"
#include "log.h"

#if defined APP_IPKG
#include<stdlib.h> //for system cmd by zero added
#elif defined USE_TCAPI
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#include "libtcapi.h"
#include "tcapi.h"
#else
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#endif

#ifdef USE_TCAPI
#define WEBDAV	"AiCloud_Entry"
#define APPS	"Apps_Entry"
#define DDNS	"Ddns_Entry"
#define SAMBA	"Samba_Entry"
#define ACCOUNT "Account_Entry0"
#define SYSINFO "SysInfo_Entry"
#define INFOETH	"Info_Ether"
#define DEVICEINFO "DeviceInfo"
#define TIMEZONE "Timezone_Entry"
#define FIREWALL "Firewall_Entry"
#define DDNS_ENANBLE_X	"Active"	// #define DDNS_ENANBLE_X	"ddns_enable_x"
#define DDNS_SERVER_X	"SERVERNAME"	// #define DDNS_SERVER_X	"ddns_server_x"
#define DDNS_HOST_NAME_X	"MYHOST"	// #define DDNS_HOST_NAME_X	"ddns_hostname_x"
#define WEBDAV_SMB_PC	"webdav_smb_pc"
#define PRODUCT_ID "productid"
#define COMPUTER_NAME "NetBiosName"
#define ACC_LIST "acc_list"
#define ACC_WEBDAVPROXY "acc_webdavproxy"
#define ST_SAMBA_MODE "st_samba_mode"
#define ST_SAMBA_FORCE_MODE "st_samba_force_mode"
#define HTTP_USERNAME "username"	// #define HTTP_USERNAME "http_username"
#define HTTP_PASSWD "web_passwd"	// #define HTTP_PASSWD "http_passwd"
#define WEBDAVAIDISK "webdav_aidisk"
#define WEBDAVPROXY "webdav_proxy"
#define SHARELINK "share_link"
#define ETHMACADDR "mac"	// #define ETHMACADDR "et0macaddr"
#define FIRMVER "FwVer"	// #define FIRMVER "firmver"
#define BUILDNO "buildno"
#define ST_WEBDAV_MODE "st_webdav_mode"
#define WEBDAV_HTTP_PORT "webdav_http_port"
#define WEBDAV_HTTPS_PORT "webdav_https_port"
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
#define TIME_ZONE_X "TZ"
#define SWPJVERNO "swpjverno"
#define EXTENDNO "extendno"
#define DMS_ENABLE "dms_enable"
#define MS_DLNA "ms_dlna"
#define MS_PATH "ms_path"
#define DMS_DBCWD "dms_dbcwd"
#define DMS_DIR "dms_dir"
#define HTTPS_CRT_CN "https_crt_cn"
#define ODMPID "odmpid"
#else
#define DDNS_ENANBLE_X	"ddns_enable_x"
#define DDNS_SERVER_X	"ddns_server_x"
#define DDNS_HOST_NAME_X	"ddns_hostname_x"
#define WEBDAV_SMB_PC	"webdav_smb_pc"
#define PRODUCT_ID "productid"
#define COMPUTER_NAME "computer_name"
#define ACC_LIST "acc_list"
#define ACC_WEBDAVPROXY "acc_webdavproxy"
#define ST_SAMBA_MODE "st_samba_mode"
#define ST_SAMBA_FORCE_MODE "st_samba_force_mode"
#define HTTP_USERNAME "http_username"
#define HTTP_PASSWD "http_passwd"
#define WEBDAVAIDISK "webdav_aidisk"
#define WEBDAVPROXY "webdav_proxy"
#define SHARELINK "share_link"
#define ETHMACADDR "lan_hwaddr"
#define FIRMVER "firmver"
#define BUILDNO "buildno"
#define ST_WEBDAV_MODE "st_webdav_mode"
#define WEBDAV_HTTP_PORT "webdav_http_port"
#define WEBDAV_HTTPS_PORT "webdav_https_port"
#define HTTP_ENABLE "http_enable"
#define LAN_HTTPS_PORT "https_lanport"
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
#define SWPJVERNO "swpjverno"
#define EXTENDNO "extendno"
#define DMS_ENABLE "dms_enable"
#define MS_DLNA "ms_dlna"
#define DMS_DBCWD "dms_dbcwd"
#define DMS_DIR "dms_dir"
#define HTTPS_CRT_CN "https_crt_cn"
#define HTTPS_CRT_SAVE "https_crt_save"
#define HTTPS_CRT_FILE "https_crt_file"
#define ODMPID "odmpid"
#endif

#define DBE 0

#if defined APP_IPKG

static const char base64_xlat[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


int base64_encode(const unsigned char *in, char *out, int inlen)
{
        char *o;

        o = out;
        while (inlen >= 3) {
                *out++ = base64_xlat[*in >> 2];
                *out++ = base64_xlat[((*in << 4) & 0x3F) | (*(in + 1) >> 4)];
                ++in;	// note: gcc complains if *(++in)
                *out++ = base64_xlat[((*in << 2) & 0x3F) | (*(in + 1) >> 6)];
                ++in;
                *out++ = base64_xlat[*in++ & 0x3F];
                inlen -= 3;
        }
        if (inlen > 0) {
                *out++ = base64_xlat[*in >> 2];
                if (inlen == 2) {
                        *out++ = base64_xlat[((*in << 4) & 0x3F) | (*(in + 1) >> 4)];
                        ++in;
                        *out++ = base64_xlat[((*in << 2) & 0x3F)];
                }
                else {
                        *out++ = base64_xlat[(*in << 4) & 0x3F];
                        *out++ = '=';
                }
                *out++ = '=';
        }
        return out - o;
}


int base64_decode_t(const char *in, unsigned char *out, int inlen)
{
        char *p;
        int n;
        unsigned long x;
        unsigned char *o;
        char c;

        o = out;
        n = 0;
        x = 0;
        while (inlen-- > 0) {
                if (*in == '=') break;
                if ((p = strchr(base64_xlat, c = *in++)) == NULL) {
//			printf("ignored - %x %c\n", c, c);
                        continue;	// note: invalid characters are just ignored
                }
                x = (x << 6) | (p - base64_xlat);
                if (++n == 4) {
                        *out++ = x >> 16;
                        *out++ = (x >> 8) & 0xFF;
                        *out++ = x & 0xFF;
                        n = 0;
                        x = 0;
                }
        }
        if (n == 3) {
                *out++ = x >> 10;
                *out++ = (x >> 2) & 0xFF;
        }
        else if (n == 2) {
                *out++ = x >> 4;
        }
        return out - o;
}

int base64_encoded_len(int len)
{
        return ((len + 2) / 3) * 4;
}

int base64_decoded_len(int len)
{
        return ((len + 3) / 4) * 3;
}

static inline char * strcat_r(const char *s1, const char *s2, char *buf)
{
        strcpy(buf, s1);
        strcat(buf, s2);
        return buf;
}

char *nvram_get(char *name);
#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

unsigned long f_size(const char *path)	// 4GB-1	-1 = error
{
        struct stat st;
        if (stat(path, &st) == 0) return st.st_size;
        return (unsigned long)-1;
}

int f_write(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode)
{
        static const char nl = '\n';
        int f;
        int r = -1;
        mode_t m;

        m = umask(0);
        if (cmode == 0) cmode = 0666;
        if ((f = open(path, (flags & FW_APPEND) ? (O_WRONLY|O_CREAT|O_APPEND) : (O_WRONLY|O_CREAT|O_TRUNC), cmode)) >= 0) {
                if ((buffer == NULL) || ((r = write(f, buffer, len)) == len)) {
                        if (flags & FW_NEWLINE) {
                                if (write(f, &nl, 1) == 1) ++r;
                        }
                }
                close(f);
        }
        umask(m);
        return r;
}

int f_read(const char *path, void *buffer, int max)
{
        int f;
        int n;

        if ((f = open(path, O_RDONLY)) < 0) return -1;
        n = read(f, buffer, max);
        close(f);
        return n;
}

static int _f_read_alloc(const char *path, char **buffer, int max, int z)
{
        unsigned long n;

        *buffer = NULL;
        if (max >= 0) {
                if ((n = f_size(path)) != (unsigned long)-1) {
                        if (n < max) max = n;
                        if ((!z) && (max == 0)) return 0;
                        if ((*buffer = malloc(max + z)) != NULL) {
                                if ((max = f_read(path, *buffer, max)) >= 0) {
                                        if (z) *(*buffer + max) = 0;
                                        return max;
                                }
                                free(buffer);
                        }
                }
        }
        return -1;
}

int f_read_alloc(const char *path, char **buffer, int max)
{
        return _f_read_alloc(path, buffer, max, 0);
}

char *get_productid(void)
{
        char *productid = nvram_get("productid");
        char *odmpid = nvram_get("odmpid");
        if(odmpid != NULL)
        {
        	if (*odmpid)
                productid = odmpid;
        }
        return productid;
}

//test{

#include <fcntl.h>
#include <sys/mman.h>
//#include <utils.h>
#include <errno.h>

#define NVRAM_SPACE		0x8000
/* For CFE builds this gets passed in thru the makefile */
#define MAX_NVRAM_SPACE		NVRAM_SPACE

#define PATH_DEV_NVRAM "/dev/nvram"

/* Globals */
static int nvram_fd = -1;
static char *nvram_buf = NULL;

int
nvram_init(void *unused)
{
        if (nvram_fd >= 0)
                return 0;

        if ((nvram_fd = open(PATH_DEV_NVRAM, O_RDWR)) < 0)
                goto err;

        /* Map kernel string buffer into user space */
        nvram_buf = mmap(NULL, MAX_NVRAM_SPACE, PROT_READ, MAP_SHARED, nvram_fd, 0);
        if (nvram_buf == MAP_FAILED) {
                close(nvram_fd);
                nvram_fd = -1;
                goto err;
        }

        fcntl(nvram_fd, F_SETFD, FD_CLOEXEC);

        return 0;

err:
        perror(PATH_DEV_NVRAM);
        return errno;
}
char *nvram_get_original(char *name);
char *nvram_get(char *name)
{
    //fprintf(stderr,"name = %s\n",name);
    //if(!strcmp(name,"computer_name"))
        //return nvram_get_original(name);

        char tmp[100];
        char *value;
        char *out;
        size_t count = strlen(name) + 1;
        unsigned long *off = (unsigned long *)tmp;

        if (nvram_fd < 0) {
                if (nvram_init(NULL) != 0) //return NULL;
                {
                    return nvram_get_original(name);
                }
        }

        if (count > sizeof(tmp)) {
                if ((off = malloc(count)) == NULL) return NULL;
        }

        /* Get offset into mmap() space */
        strcpy((char *) off, name);
        count = read(nvram_fd, off, count);

        if (count == sizeof(*off)) {
                value = &nvram_buf[*off];
                out = malloc(strlen(value)+1);
                memset(out,0,strlen(value)+1);
                sprintf(out,"%s",value);
        }
        else {
                value = NULL;
                out = NULL;
                if (count < 0) perror(PATH_DEV_NVRAM);
        }

        if (off != (unsigned long *)tmp) free(off);
        //fprintf(stderr,"value = %s\n",out);
         return out;
}
//end test}

char *nvram_get_original(char *name)
//char *nvram_get(char *name)
{
    fprintf(stderr,"name = %s\n",name);
#if 0

    if(strcmp(name,"webdav_aidisk")==0 ||strcmp(name,"webdav_proxy")==0||strcmp(name,"webdav_smb_pc")==0
       ||strcmp(name,"share_link")==0||strcmp(name,"webdav_acc_lock")==0
       ||strcmp(name,"webdav_last_login_info")==0||strcmp(name,"enable_webdav_lock")==0
       ||strcmp(name,"http_passwd")==0||strcmp(name,"webdav_lock_times")==0||strcmp(name,"webdav_lock_interval")==0
       ||strcmp(name,"ddns_hostname_x")==0||strcmp(name,"ddns_enable_x")==0||strcmp(name,"ddns_server_x")==0
		 ||strcmp(name,"share_link_param")==0||strcmp(name,"share_link_result")==0
       ||strcmp(name,"swpjverno")==0||strcmp(name,"extendno")==0)
    {
#endif
        char tmp_name[256]="/opt/etc/apps_asus_script/aicloud_nvram_check.sh";
        //char tmp_name[256]="/tmp/aicloud_nvram_check.sh";
        char *cmd_name;
        cmd_name=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
        memset(cmd_name,0,sizeof(cmd_name));
        sprintf(cmd_name,"%s %s",tmp_name,name);
        system(cmd_name);
        free(cmd_name);

        while(-1!=access("/tmp/aicloud_check.control",F_OK))
            usleep(50);
 //   }
//#endif

    FILE *fp;
    if((fp=fopen("/tmp/webDAV.conf","r+"))==NULL)
    {
        return NULL;
    }
    char *value = NULL;
    //value=(char *)malloc(256);
    //memset(value,'\0',sizeof(value));
    int file_size = f_size("/tmp/webDAV.conf");
    char *tmp=(char *)malloc(sizeof(char)*(file_size+1));
    while(!feof(fp)){
        memset(tmp,'\0',sizeof(tmp));
        fgets(tmp,file_size+1,fp);
        if(strncmp(tmp,name,strlen(name))==0)
        {
            if(tmp[strlen(tmp)-1] == 10)
            {
                tmp[strlen(tmp)-1]='\0';
            }
            char *p=NULL;
            p=strchr(tmp,'=');
            p++;
            if(p == NULL || strlen(p) == 0)
            {
                fclose(fp);
                free(tmp);
                return NULL;
            }
            else
            {
            value=(char *)malloc(strlen(p)+1);
            memset(value,'\0',sizeof(value));
            strcpy(value,p);
            if(value[strlen(value)-1]=='\n')
                value[strlen(value)-1]='\0';
        }

    }
    fclose(fp);
    free(tmp);
    return value;
}
}
char *nvram_safe_get(char *name)
{
    char *p = nvram_get(name);
	return p ? p : "";
}
int nvram_set(const char *name, const char *value)
{
    char *cmd;

    if(value == NULL)
        cmd=(char *)malloc(sizeof(char)*(64+strlen(name)));
    else
        cmd=(char *)malloc(sizeof(char)*(64+strlen(name)+strlen(value)));

    memset(cmd,0,sizeof(cmd));

    sprintf(cmd,"nvram set \"%s=%s\"",name,value);

    system(cmd);

    free(cmd);
    return 0;
# if 0
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
#if 0
            char *cmd_name;
            if(value == NULL)
                cmd_name=(char *)malloc(sizeof(char)*(64+2*strlen(name)));
            else
            cmd_name=(char *)malloc(sizeof(char)*(64+2*strlen(name)+strlen(value)));
            memset(cmd_name,0,sizeof(cmd_name));
#endif
            /*
            if(strcmp(name,"webdav_last_login_info")==0)
            {
 				*/
                char tmp_name[256]="/opt/etc/apps_asus_script/aicloud_nvram_check.sh";
                char *cmd_name_1;
                cmd_name_1=(char *)malloc(sizeof(char)*(strlen(tmp_name)+strlen(name)+2));
                memset(cmd_name_1,0,sizeof(cmd_name_1));
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

            }
            else
            {

                sprintf(cmd_name,"sed -i 's/^%s=.*/%s=%s/g' /tmp/webDAV.conf",name,name,value);

                system(cmd_name);
            }
#endif
            //free(cmd_name);
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
#endif
}

int nvram_commit(void)
{
#if 0
        FILE *fp;
        fp = fopen("/var/log/commit_ret", "w");
        fclose(fp);
#endif
        char cmd[]="nvram commit";
        system(cmd);
        return 1;
}
int nvram_get_file(const char *key, const char *fname, int max)
{
        int n;
        char *p;
        char *b;
        int r;

        r = 0;
        p = nvram_safe_get(key);
        n = strlen(p);
        if (n <= max) {
                if ((b = malloc(base64_decoded_len(n) + 128)) != NULL) {
                        n = base64_decode_t(p, b, n);
                        if (n > 0) r = (f_write(fname, b, n, 0, 0644) == n);
                        free(b);
                }
        }
        return r;
}

int nvram_set_file(const char *key, const char *fname, int max)
{
        char *in;
        char *out;
        long len;
        int n;
        int r;

        if ((len = f_size(fname)) > max) return 0;
        max = (int)len;
        r = 0;
        if (f_read_alloc(fname, &in, max) == max) {
                if ((out = malloc(base64_encoded_len(max) + 128)) != NULL) {
                        n = base64_encode(in, out, max);
                        out[n] = 0;
                        nvram_set(key, out);
                        free(out);
                        r = 1;
                }
                free(in);
        }
        return r;
}
void start_ssl()
{
    fprintf(stderr,"\nstart ssl\n");
    int ok;
    int save;
    int retry;
    unsigned long long sn;
    char t[32];

    retry = 1;
    while (retry) {
        char *https_crt_save = nvram_get("https_crt_save");
        if(https_crt_save != NULL)
        {
            if(atoi(https_crt_save) == 1)
                save = 1; //pem nvram is exit
            else
                save = 0;
        }
        else
            save = 0;
        free(https_crt_save);
        //save = nvram_match("https_crt_save", "1");

        if (1) {
            ok = 0;
            if (save) {
                fprintf(stderr, "Save SSL certificate...\n"); // tmp test
                if (nvram_get_file("https_crt_file", "/tmp/cert.tgz", 8192)) {
                        system("tar -xzf /tmp/cert.tgz -C / etc/cert.pem etc/key.pem");
                        usleep(1000*100);
                        system("cat /etc/key.pem /etc/cert.pem > /etc/server.pem");
                        ok = 1;
                    unlink("/tmp/cert.tgz");
                }
            }
            if (!ok) {
                fprintf(stderr, "Generating SSL certificate...\n"); // tmp test
                // browsers seems to like this when the ip address moves...	-- zzz
                f_read("/dev/urandom", &sn, sizeof(sn));

                sprintf(t, "%llu", sn & 0x7FFFFFFFFFFFFFFFULL);
                char *cmd_app=NULL;
                cmd_app=(char *)malloc(sizeof(char)*(strlen(t)+64));
                memset(cmd_app,'\0',sizeof(cmd_app));
                sprintf(cmd_app,"%s %s","/opt/etc/apps_asus_script/gencert.sh",t);
                system(cmd_app);
                while(-1==access("/etc/cert.pem",F_OK)||-1==access("/etc/key.pem",F_OK))
                {
                    usleep(1000*100);
                }
                free(cmd_app);

                system("tar -C / -czf /tmp/cert.tgz etc/cert.pem etc/key.pem");
                while(-1==access("/tmp/cert.tgz",F_OK))
                {
                    usleep(1000*100);
                }

                save = 1;
            }
        }
        fprintf(stderr,"\n nvram get https_crt_file\n");
        if ((save) && (*nvram_safe_get("https_crt_file")) == 0) {

            if (nvram_setfile_https_crt_file("/tmp/cert.tgz", 8192)) {
                Cdbg(DBE, "complete nvram_setfile_https_crt_file");
                nvram_set_https_crt_save("1");
                nvram_do_commit();
                Cdbg(DBE, "end nvram_setfile_https_crt_file");
            }

            unlink("/tmp/cert.tgz");
        }
        fprintf(stderr,"\n over ssl \n");
        retry = 0;
    }
}
#elif defined USE_TCAPI

#else
extern char *nvram_get(const char *name);
extern int nvram_set(const char *name, const char *value);
extern int nvram_commit(void);
#endif

int nvram_smbdav_pc_append(const char* ap_str)
{
#ifdef USE_TCAPI
	char nv_var_val[MAXLEN_TCAPI_MSG] = {0};
	if( tcapi_get(WEBDAV, WEBDAV_SMB_PC, nv_var_val) ){
		Cdbg(1,"nv_var get null");
		return -1;
	}
	char tmp_nv_var_val[120]={0};
	strcpy(tmp_nv_var_val, nv_var_val);
	strcat(tmp_nv_var_val, ap_str);
	tcapi_set(WEBDAV, WEBDAV_SMB_PC, tmp_nv_var_val);
#else
	char* nv_var_val=NULL;
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
#endif

	return 0;
}

char* nvram_get_smbdav_str(void)
{	
#ifdef USE_TCAPI
	static char smbdav_str[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, WEBDAV_SMB_PC, smbdav_str);
	return smbdav_str;
#else
	return nvram_get(WEBDAV_SMB_PC);
#endif
}

int nvram_set_smbdav_str(const char* pc_info)
{
#ifdef USE_TCAPI
	return tcapi_set(WEBDAV, WEBDAV_SMB_PC, pc_info);
#else
	return nvram_set(WEBDAV_SMB_PC, pc_info);
#endif
}

char* nvram_get_sharelink_str(void)
{
#ifdef USE_TCAPI
	static char sharelink_str[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, SHARELINK, sharelink_str);
	return sharelink_str;
#else
	return nvram_get(SHARELINK);
#endif
}

int nvram_set_sharelink_str(const char* share_info)
{
#ifdef USE_TCAPI
	if(strlen(share_info)>576)
		return 0;
	return tcapi_set(WEBDAV, SHARELINK, share_info);
#else
	return nvram_set(SHARELINK, share_info);
#endif
}

int nvram_do_commit(void){
#ifdef USE_TCAPI
	tcapi_commit(WEBDAV);
#else
	nvram_commit();
#endif
	return 1;
}

int nvram_is_ddns_enable(void)
{
#ifdef USE_TCAPI
	char	ddns_e[4] = {0};
	int	ddns_enable_x=0;
	if( tcapi_get(DDNS, DDNS_ENANBLE_X, ddns_e) ) {
		ddns_enable_x = atoi(ddns_e);
		Cdbg(DBE," ddns_e = %s", ddns_e);
	}
	if(ddns_enable_x)
		return 1;
	else	
		return 0;
#else
	char*	ddns_e = NULL;
	int		ddns_enable_x = 0;
	if( (ddns_e = nvram_get(DDNS_ENANBLE_X))!=NULL ){
		ddns_enable_x = atoi(ddns_e);
		Cdbg(DBE," ddns_e = %s", ddns_e);
		#ifdef APP_IPKG
		free(ddns_e);
		#endif
	}
	if(ddns_enable_x)	return 1;
	else				return 0;
#endif
}

char* nvram_get_ddns_server_name(void)
{
#ifdef USE_TCAPI
	static char ddns_server_name[64] = {0};
	tcapi_get(DDNS, DDNS_SERVER_X, ddns_server_name);
	return ddns_server_name;
#else
	return nvram_get(DDNS_SERVER_X);
#endif
}

char* nvram_get_ddns_host_name(void)
{
#ifdef USE_TCAPI
	static char ddns_host_name_x[MAXLEN_TCAPI_MSG] = {0};
	if(tcapi_get(DDNS, DDNS_HOST_NAME_X, ddns_host_name_x)) {
		Cdbg(DBE,"ddns_hostname_x = %s", ddns_host_name_x);
		return NULL;
	}
	else
		return ddns_host_name_x;
#else

	/*
	nvram get/set ddns_enable_x
	nvram get/set ddns_server_x (WWW.ASUS.COM)
	nvram get/set ddns_hostname_x (RT-N66U-00E012112233.asuscomm.com)
	rc rc_service restart_ddns or notify_rc(?œrestart_ddns?? through libshared
	 */
	char* ddns_host_name_x=NULL;
	if(!nvram_is_ddns_enable())
	   goto nvram_get_ddns_host_name_EXIT;
	if(!nvram_get_ddns_server_name())
	   goto nvram_get_ddns_host_name_EXIT;
	 ddns_host_name_x= nvram_get (DDNS_HOST_NAME_X);  
	
nvram_get_ddns_host_name_EXIT:
	Cdbg(DBE,"ddns_hostname_x = %s", ddns_host_name_x);
	return ddns_host_name_x;
#endif
}

char* nvram_get_ddns_host_name2(void)
{
#ifdef USE_TCAPI
	static char ddns_host_name_x[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(DDNS, DDNS_HOST_NAME_X, ddns_host_name_x);
	return ddns_host_name_x;
#else
	char* ddns_host_name_x = NULL;
	ddns_host_name_x = nvram_get(DDNS_HOST_NAME_X);
	return ddns_host_name_x;
#endif
}

char* nvram_get_productid(void)
{
	return get_productid();
}

char* nvram_get_acc_list(void)
{
#ifdef USE_TCAPI
	static char acc_list[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(SAMBA, ACC_LIST, acc_list);
	return acc_list;
#else
	return nvram_get(ACC_LIST);
#endif
}

char* nvram_get_webdavaidisk(void)
{
#ifdef USE_TCAPI
	static char webdavaidisk[4] = {0};
	tcapi_get(WEBDAV, WEBDAVAIDISK, webdavaidisk);
	return webdavaidisk;
#else
	return nvram_get(WEBDAVAIDISK);
#endif
}

int nvram_set_webdavaidisk(const char* enable)
{
#ifdef USE_TCAPI
	tcapi_set(WEBDAV, WEBDAVAIDISK, enable);
#else
	nvram_set(WEBDAVAIDISK, enable);
#endif
	return 1;
}

char* nvram_get_webdavproxy(void)
{
#ifdef USE_TCAPI
	static char webdavproxy[4] = {0};
	tcapi_get(WEBDAV, WEBDAVPROXY, webdavproxy);
	return webdavproxy;
#else
	return nvram_get(WEBDAVPROXY);
#endif
}

int nvram_set_webdavproxy(const char* enable)
{
#ifdef USE_TCAPI
	tcapi_set(WEBDAV, WEBDAVPROXY, enable);
#else
	nvram_set(WEBDAVPROXY, enable);
#endif
	return 1;
}

char* nvram_get_acc_webdavproxy(void)
{
#ifdef USE_TCAPI
	static char acc_webdavproxy[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, ACC_WEBDAVPROXY, acc_webdavproxy);
	return acc_webdavproxy;
#else
	return nvram_get(ACC_WEBDAVPROXY);
#endif
}

int nvram_get_st_samba_mode(void)
{
#ifdef USE_TCAPI
	char st_samba_mode[4] ={0};
	int a = 0;
	tcapi_get(SAMBA, ST_SAMBA_FORCE_MODE, st_samba_mode);

	if(strcmp(st_samba_mode,"")==0)
		tcapi_get(SAMBA, ST_SAMBA_MODE, st_samba_mode);
	
	a = atoi(st_samba_mode);
	return a;
#else
	char* res = nvram_get(ST_SAMBA_FORCE_MODE);

	if(res==NULL)
		res = nvram_get(ST_SAMBA_MODE);
	
	int a = atoi(res);
#ifdef APP_IPKG
	free(res);
#endif
	return a;
#endif
}

char* nvram_get_http_username(void)
{
#ifdef USE_TCAPI
	static char http_username[32] = {0};
	tcapi_get(ACCOUNT, HTTP_USERNAME, http_username);
	return http_username;
#else
	return nvram_get(HTTP_USERNAME);
#endif
}

char* nvram_get_http_passwd(void)
{
#ifdef USE_TCAPI
	static char http_passwd[32] = {0};
	tcapi_get(ACCOUNT, HTTP_PASSWD, http_passwd);
	return http_passwd;
#else
	return nvram_get(HTTP_PASSWD);
#endif
}

char* nvram_get_computer_name(void)
{
#ifdef USE_TCAPI
	static char computer_name[16] = {0};
	tcapi_get(SAMBA, COMPUTER_NAME, computer_name);
	return computer_name;
#else
	return nvram_get(COMPUTER_NAME);
#endif
}

char* nvram_get_router_mac(void)
{
#ifdef USE_TCAPI
	static char router_mac[32] = {0};
	tcapi_get(INFOETH, ETHMACADDR, router_mac);
	return router_mac;
#else
	char* mac = nvram_get(ETHMACADDR);
	return mac;
#endif
}

char* nvram_get_firmware_version(void)
{
#ifdef USE_TCAPI
	static char firmware_version[16] = {0};
	tcapi_get(DEVICEINFO, FIRMVER, firmware_version);
	return firmware_version;
#else
	return nvram_get(FIRMVER);
#endif
}

char* nvram_get_build_no(void)
{
#ifdef USE_TCAPI
	return 0;
#else
	return nvram_get(BUILDNO);
#endif
}

char* nvram_get_st_webdav_mode(void)
{
#ifdef USE_TCAPI
	static char st_webdav_mode[4] = {0};
	tcapi_get(WEBDAV, ST_WEBDAV_MODE, st_webdav_mode);
	return st_webdav_mode;
#else
	return nvram_get(ST_WEBDAV_MODE);
#endif
}

char* nvram_get_webdav_http_port(void)
{
#ifdef USE_TCAPI
	static char webdav_http_port[8] = {0};
	tcapi_get(WEBDAV, WEBDAV_HTTP_PORT, webdav_http_port);
	return webdav_http_port;
#else
	return nvram_get(WEBDAV_HTTP_PORT);
#endif
}

char* nvram_get_webdav_https_port(void)
{
#ifdef USE_TCAPI
	static char webdav_https_port[8] = {0};
	tcapi_get(WEBDAV, WEBDAV_HTTPS_PORT, webdav_https_port);
	return webdav_https_port;
#else
	return nvram_get(WEBDAV_HTTPS_PORT);
#endif
}

char* nvram_get_http_enable(void)
{
	// 0 --> http
    // 1 --> https
    // 2 --> both
#ifdef USE_TCAPI
	return 0;
#else
	return nvram_get(HTTP_ENABLE);
#endif
}

char* nvram_get_lan_https_port(void){
#ifdef USE_TCAPI
	return 0;
#else
	return nvram_get(LAN_HTTPS_PORT);
#endif
}

char* nvram_get_misc_http_x(void)
{
#ifdef USE_TCAPI
	static char misc_http_x[4] = {0};
	tcapi_get(FIREWALL, MISC_HTTP_X, misc_http_x);
	return misc_http_x;
#else
	return nvram_get(MISC_HTTP_X);
#endif
}

char* nvram_get_misc_http_port(void)
{
#ifdef USE_TCAPI
	static char misc_http_port[8] = {0};
	tcapi_get(FIREWALL, MISC_HTTP_PORT, misc_http_port);
	return misc_http_port;
#else
	return nvram_get(MISC_HTTP_PORT);
#endif
}

char* nvram_get_misc_https_port(void)
{
#ifdef USE_TCAPI
	static char misc_https_port[8] = {0};
	tcapi_get(FIREWALL, MISC_HTTPS_PORT, misc_https_port);
	return misc_https_port;
#else
	return nvram_get(MISC_HTTPS_PORT);
#endif
}

char* nvram_get_enable_webdav_captcha(void)
{
#ifdef USE_TCAPI
	static char enable_webdav_captcha[4] = {0};
	tcapi_get(WEBDAV, ENABLE_WEBDAV_CAPTCHA, enable_webdav_captcha);
	return enable_webdav_captcha;
#else
	return nvram_get(ENABLE_WEBDAV_CAPTCHA);
#endif
}

char* nvram_get_enable_webdav_lock(void)
{
#ifdef USE_TCAPI
	static char enable_webdav_lock[4] = {0};
	tcapi_get(WEBDAV, ENABLE_WEBDAV_LOCK, enable_webdav_lock);
	return enable_webdav_lock;
#else
	return nvram_get(ENABLE_WEBDAV_LOCK);
#endif
}

char* nvram_get_webdav_acc_lock(void)
{
#ifdef USE_TCAPI
	static char webdav_acc_lock[4] = {0};
	tcapi_get(WEBDAV, WEBDAV_ACC_LOCK, webdav_acc_lock);
	return webdav_acc_lock;
#else
	return nvram_get(WEBDAV_ACC_LOCK);
#endif
}

int nvram_set_webdav_acc_lock(const char* acc_lock)
{
#ifdef USE_TCAPI
	tcapi_set(WEBDAV, WEBDAV_ACC_LOCK, acc_lock);
#else
	nvram_set(WEBDAV_ACC_LOCK, acc_lock);
#endif
	return 1;
}

char* nvram_get_webdav_lock_interval(void)
{
#ifdef USE_TCAPI
	static char webdav_lock_interval[4] = {0};
	tcapi_get(WEBDAV, WEBDAV_LOCK_INTERVAL, webdav_lock_interval);
	return webdav_lock_interval;
#else
	return nvram_get(WEBDAV_LOCK_INTERVAL);
#endif
}

char* nvram_get_webdav_lock_times(void)
{
#ifdef USE_TCAPI
	static char webdav_lock_times[4] = {0};
	tcapi_get(WEBDAV, WEBDAV_LOCK_TIMES, webdav_lock_times);
	return webdav_lock_times;
#else
	return nvram_get(WEBDAV_LOCK_TIMES);
#endif
}

char* nvram_get_webdav_last_login_info(void)
{
#ifdef USE_TCAPI
	static char webdav_last_login_info[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, WEBDAV_LAST_LOGININFO, webdav_last_login_info);
	return webdav_last_login_info;
#else
	return nvram_get(WEBDAV_LAST_LOGININFO);
#endif
}

int nvram_set_webdav_last_login_info(const char* last_login_info)
{
#ifdef USE_TCAPI
	return tcapi_set(WEBDAV, WEBDAV_LAST_LOGININFO, last_login_info);
#else
	return nvram_set(WEBDAV_LAST_LOGININFO, last_login_info);
#endif
}

char* nvram_get_latest_version(void)
{
#ifdef USE_TCAPI
	static char latest_version[16] = {0};
	tcapi_get(WEBDAV, WEBS_STATE_INFO, latest_version);
	return latest_version;
#else
	return nvram_get(WEBS_STATE_INFO);
#endif
}

int nvram_get_webs_state_error(void)
{
#ifdef USE_TCAPI
	char webs_state_error[4] = {0};
	int a = 0;
	tcapi_get(WEBDAV, WEBS_STATE_ERROR, webs_state_error);
	a = atoi(webs_state_error);
	return a;
#else
	char* res = nvram_get(WEBS_STATE_ERROR);
	int a = atoi(res);
#ifdef APP_IPKG
	free(res);
#endif
	return a;
#endif
}

char* nvram_get_share_link_param(void)
{
#ifdef USE_TCAPI
	static char share_link_param[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, SHARE_LINK_PARAM, share_link_param);
	return share_link_param;
#else
	return nvram_get(SHARE_LINK_PARAM);
#endif
}

char* nvram_get_time_zone(void)
{
#ifdef USE_TCAPI
	static char time_zone[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(TIMEZONE, TIME_ZONE_X, time_zone);
	return time_zone;
#else
	return nvram_get(TIME_ZONE_X);
#endif
}


int nvram_set_share_link_result(const char* result)
{
#ifdef USE_TCAPI
	tcapi_set(WEBDAV, SHARE_LINK_RESULT, result);
#else
	nvram_set(SHARE_LINK_RESULT, result);
#endif
	return 1;
}

int nvram_wan_primary_ifunit(void)
{	
#ifdef USE_TCAPI
	char tmp[4], prefix[16] = {0};
	int unit;	
	for (unit = 0; unit < 12; unit ++) {		
		if( unit > 0 && unit < 8 )	//ignore nas1~7 which should be bridge mode for ADSL
			continue;	
		snprintf(prefix, sizeof(prefix), "Wan_PVC%d", unit);
		tcapi_get(prefix, "Active", tmp);
		if (!strcmp(tmp, "Yes"))
			return unit;
	}
#else
	int unit;	
	for (unit = 0; unit < 10; unit ++) {		
		char tmp[100], prefix[] = "wanXXXXXXXXXX_";		
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		char* res = strcat_r(prefix, "primary", tmp);		
		if (strncmp(res, "1", 1)==0)			
			return unit;	
	}
#endif
	return 0;
}

char* nvram_get_wan_ip(void)
{
#ifdef USE_TCAPI
	static char wan_ip[16]= {0};
	char prefix[32] = {0};
	int unit = nvram_wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "%s_PVC%d", DEVICEINFO, unit);
	tcapi_get(prefix, "WanIP", wan_ip);
	return wan_ip;
#else
	char *wan_ip;
	char tmp[32], prefix[] = "wanXXXXXXXXXX_";
	int unit = nvram_wan_primary_ifunit();
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_ip = nvram_get(strcat_r(prefix, "ipaddr", tmp));
	return wan_ip;
#endif
}

char* nvram_get_swpjverno(void)
{
#ifdef USE_TCAPI
	static char swpjverno[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, SWPJVERNO, swpjverno);
	return swpjverno;
#else
	return nvram_get(SWPJVERNO);
#endif
}

char* nvram_get_extendno(void)
{
#ifdef USE_TCAPI
	static char extendno[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, EXTENDNO, extendno);
	return extendno;
#else	
	return nvram_get(EXTENDNO);
#endif
}

char* nvram_get_dms_enable(void)
{
	// 0 --> off
    // 1 --> on
#ifdef USE_TCAPI
	static char dms_enable[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, DMS_ENABLE, dms_enable);
	return dms_enable;
#else
	return nvram_get(DMS_ENABLE);
#endif
}

char* nvram_get_dms_dbcwd(void)
{
#ifdef USE_TCAPI
	/*
	static char dms_dbcwd[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(APPS, MS_PATH, dms_dbcwd);
	strcat(dms_dbcwd, "/minidlna");
	return dms_dbcwd;
	*/
	return NULL;
#else
	return nvram_get(DMS_DBCWD);
#endif
}

char* nvram_get_dms_dir(void)
{
#ifdef USE_TCAPI
	/*
	static char dms_dir[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(APPS, MS_PATH, dms_dir);
	return dms_dir;
	*/
	return NULL;
#else
	return nvram_get(DMS_DIR);
#endif
}

char* nvram_get_ms_enable(void)
{
#if EMBEDDED_EANBLE

#ifdef USE_TCAPI
	static char ms_enable[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(APPS, MS_DLNA, ms_enable);
	return ms_enable;
#else
	return nvram_get(MS_DLNA);
#endif

#else
	return "1";
#endif
}

int nvram_set_https_crt_cn(const char* cn)
{
#ifdef USE_TCAPI
	tcapi_set(WEBDAV, HTTPS_CRT_CN, cn);
#else
	nvram_set(HTTPS_CRT_CN, cn);
#endif
	return 1;
}

char* nvram_get_https_crt_cn(){
#ifdef USE_TCAPI		
	static char https_crt_cn[MAXLEN_TCAPI_MSG] = {0};
	tcapi_get(WEBDAV, HTTPS_CRT_CN, https_crt_cn);
	return https_crt_cn;
#else
	return nvram_get(HTTPS_CRT_CN);
#endif
}

int nvram_setfile_https_crt_file(const char* file, int size)
{
#ifdef USE_TCAPI
	
#else
	nvram_set_file(HTTPS_CRT_FILE, file, size);
#endif
	return 1;
}

int nvram_getfile_https_crt_file(const char* file, int size)
{
#ifdef USE_TCAPI
	
#else
	nvram_get_file(HTTPS_CRT_FILE, file, size);
#endif
	return 1;
}

char* nvram_get_https_crt_file()
{
#ifdef USE_TCAPI
	
#else
	return nvram_get("https_crt_file");
#endif
}

char* nvram_get_odmpid()
{
#ifdef USE_TCAPI
	return NULL;
#else
	return nvram_get(ODMPID);
#endif
}

int nvram_set_https_crt_save(const char* enable)
{
#ifdef USE_TCAPI
	
#else
	nvram_set(HTTPS_CRT_SAVE, enable);
#endif
	return 1;
}

int nvram_set_value(const char* key, const char* value){
#ifdef USE_TCAPI
		
#else
	nvram_set(key, value);
#endif
	return 1;
}

char* nvram_get_value(const char* key){
#ifdef USE_TCAPI
	return NULL;
#else
	return nvram_get(key);
#endif
}

#endif
