#include <rc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#include "rc_ipsec.h"

#ifdef IPSEC_DEBUG
#define DBG(args) _dprintf args
#endif

/*ike version : v1, v2, auto:ike*/
char ikev[IKE_TYPE_MAX_NUM][SZ_MIN] = {
    {"ike"},
    {"ikev1"},
    {"ikev2"},
};
/*ike default: 3des-sha1-modp1536, aes128-sha1-modp2048*/
char encryp[ENCRYPTION_TYPE_MAX_NUM][SZ_MIN] = {
    {"des"},   
    {"3des"},
    {"aes128"},
    {"aes192"},
    {"aes256"},
};

/*hash default is sha1*/
char hash[HASH_TYPE_MAX_NUM][SZ_MIN] = {
    {"md5"},
    {"sha1"},
    {"sha256"},
    {"sha384"},
    {"sha512"},
};

/*DH: default group is modp1536,modp2048*/
char dh_group[DH_GROUP_MAX_NUM][SZ_MIN] = {
/*Regular Groups*/
    {"modp768"},      /*DH Group 1  : 768bits*/
    {"modp1024"},     /*DH Group 2  : 1024bits*/
    {"modp1536"},     /*DH Group 5  : 1536bits*/
    {"modp2048"},     /*DH Group 14 : 2048bits*/
    {"modp3072"},     /*DH Group 15 : 3072bits*/
    {"modp4096"},     /*DH Group 16 : 4096bits*/
    {"modp6144"},     /*DH Group 17 : 6144bits*/
    {"modp8192"},     /*DH Group 18 : 8192bits*/
/*Modulo Rrime Groups with Prime Order Sbugroup*/
    {"modp1024s160"}, /*DH Group 22 : 1024bits*/
    {"modp2048s224"}, /*DH Group 23 : 2048bits*/
    {"modp2048s256"}, /*DH Group 24 : 2048bits*/
};

static ipsec_samba_t samba_prof;
static ipsec_prof_t prof[2][MAX_PROF_NUM];
static pki_ca_t ca_tab[CA_FILES_MAX_NUM];

/*param 1: char *p_end , IN : the src of string buf */
/*param 2: char *p_tmp , OUT: the dest of string buf*/
/*param 3: int *nsize_shifft , OUT: the size of shifft*/
void ipsec_profile_str_parse(char *p_end, char *p_tmp, int *nsize_shifft)
{
    int i = 1;
    while(*p_end != '>'){
        *p_tmp = *p_end;
        p_tmp++;
        p_end++;
        i++;
    }
    *p_tmp = '\0';
    *p_end = '\0';
    *nsize_shifft = i;
    return;
}

static int ipsec_ike_type_idx_get(char *data);
static int ipsec_esp_type_idx_get(char *data);
/*param 1: char *p_end , IN : the src of string buf */
/*param 2: int *nsize_shifft , OUT: nsize_shifft: OUTPUT*/
int ipsec_profile_int_parse(int ike_esp_type, char *p_end, int *nsize_shifft)
{
    int i = 1, i_value = -1;
    char *p_head = p_end;
    if('n' == *p_head){ i_value = 0; }
    while(*p_end++ != '>'){ i++; }
    *(p_end - 1) = '\0';
    *nsize_shifft = i;
    if(0 != i_value){
        if(FLAG_IKE_ENCRYPT == ike_esp_type){
            i_value = ipsec_ike_type_idx_get(strndup(p_head, i));
        } else if(FLAG_ESP_HASH == ike_esp_type){
            i_value = ipsec_esp_type_idx_get(strndup(p_head, i));
        } else if(FLAG_KEY_CHAG_VER == ike_esp_type){
            if(('a' == *p_head) && ('u' == *(p_head + 1))){
               i_value = IKE_TYPE_AUTO; 
            } else {
               i_value = atoi(strndup(p_head, (size_t)i));
            }
        } else if(FLAG_NONE == ike_esp_type){
            i_value = atoi(strndup(p_head, (size_t)i));
        }
    }
    return i_value;
}

/* function compares the two strings s1 and s2.*/
/* if s1 is fouund to be less than, to match, or be greater than s2.*/
/* the last character of all type of ike and esp is different. */
/* that's why just compare last character */
int reverse_str_cmp(char *s1, char *s2)
{
    int n = 0;
    int sz_s1 = strlen(s1), sz_s2 = strlen(s2);
    if(sz_s1 != sz_s2) {
        return 0;
    }
    n = sz_s1;
    while(0 != n){
        if(s1[n] != s2[n]){
            return 0;
        }else{
            return n;
        }
        n--;
    }
    return sz_s2;
}

int ipsec_ike_type_idx_get(char *data)
{
    int i = 0;
    encryption_type_t ike_type_idx = ENCRYPTION_TYPE_MAX_NUM; /*default : auto*/
    if(0 == strcmp(data, "auto")){
        return ENCRYPTION_TYPE_MAX_NUM;
    }
    if(strlen(data) == strlen("des")){
        return ENCRYPTION_TYPE_DES;
    }
    for(i = ENCRYPTION_TYPE_3DES; i < ENCRYPTION_TYPE_MAX_NUM; i++){
        if(0 == reverse_str_cmp(data, encryp[i])){
            continue;
        }else{
            return i;
        }
    }
    return ike_type_idx;
}

int ipsec_esp_type_idx_get(char *data)
{
    int i = 0;
    encryption_type_t ike_type_idx = HASH_TYPE_MAX_NUM; /*default : auto*/
    if(0 == strcmp(data, "auto")){
        return HASH_TYPE_MAX_NUM;
    }
    if(strlen(data) == strlen("md5")){
        return HASH_TYPE_MD5;
    }
    for(i = HASH_TYPE_SHA1; i < HASH_TYPE_MAX_NUM; i++){
        if(0 == reverse_str_cmp(data, hash[i])){
            continue;
        }else{
            return i;
        }
    }
    return ike_type_idx;
}

void ipsec_samba_prof_fill(char *p_data)
{
    int i = 1;
    char *p_end = NULL, *p_tmp = NULL;
    p_end = p_data;
    /*DNS1*/
    p_tmp = &(samba_prof.dns1[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
DBG(("dns1:%s\n", samba_prof.dns1));
    p_end += i;
    /*DNS2*/
    p_tmp = &(samba_prof.dns2[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
DBG(("dns2:%s\n", samba_prof.dns2));
    p_end += i ;
    /*NBIOS1*/
    p_tmp = &(samba_prof.nbios1[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
DBG(("nbios1:%s\n", samba_prof.nbios1));
    p_end += i;
    /*NBIOS2*/
    p_tmp = &(samba_prof.nbios2[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
DBG(("nbios2:%s\n", samba_prof.nbios2));
    p_end += i;
    return;
}

void ipsec_prof_fill(int prof_idx, char *p_data, ipsec_prof_type_t prof_type)
{
    int i = 1;
    char *p_head = NULL, *p_end = NULL, *p_tmp = NULL;
    p_head = p_end = p_data;
    /*vpn_type*/
    prof[prof_type][prof_idx].vpn_type = (uint8_t)ipsec_profile_int_parse(FLAG_NONE,
                                                               p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*profilename*/    
    p_tmp = &(prof[prof_type][prof_idx].profilename[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*remote_gateway_method*/
    p_tmp = &(prof[prof_type][prof_idx].remote_gateway_method[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*remote gateway*/
    p_tmp = &(prof[prof_type][prof_idx].remote_gateway[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*local public interface*/
    p_tmp = &(prof[prof_type][prof_idx].local_public_interface[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*local public ip*/
    p_tmp = &(prof[prof_type][prof_idx].local_pub_ip[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*auth_method*/
    prof[prof_type][prof_idx].auth_method = (uint8_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                  p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*auth method value -- psk password or private key of rsa*/
    p_tmp = &(prof[prof_type][prof_idx].auth_method_key[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*to parse local subnet e.g.192.168.2.1/24> [local_port] 0>*/
    p_tmp = &(prof[prof_type][prof_idx].local_subnet[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*local port*/
    prof[prof_type][prof_idx].local_port = (uint16_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                  p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*remote subnet*/
    /*to parse remote subnet e.g. <192.168.3.1/16>3600>*/
    /*p_end+1 for skipping '<'*/
    p_end += 1;
    p_tmp = &(prof[prof_type][prof_idx].remote_subnet[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i ;
    /*remote port*/
    prof[prof_type][prof_idx].remote_port = (uint16_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                     p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*tunnel type: transport or tunnel*/
    p_tmp = &(prof[prof_type][prof_idx].tun_type[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*virtual ip en */
    p_tmp = &(prof[prof_type][prof_idx].virtual_ip_en[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*virtual ip subnet*/
    p_tmp = &(prof[prof_type][prof_idx].virtual_subnet[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*accessible_networks*/
    prof[prof_type][prof_idx].accessible_networks = (uint8_t)ipsec_profile_int_parse(
                                                          FLAG_NONE, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*ike*/
    prof[prof_type][prof_idx].ike = (uint8_t)ipsec_profile_int_parse(FLAG_KEY_CHAG_VER,
                                                          p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*encryption_p1*/
    prof[prof_type][prof_idx].encryption_p1 = (uint8_t)ipsec_profile_int_parse(
                                                 FLAG_IKE_ENCRYPT, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*hash_p1*/
    prof[prof_type][prof_idx].hash_p1 = (uint8_t)ipsec_profile_int_parse(
                                           FLAG_ESP_HASH, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*exchange*/
    prof[prof_type][prof_idx].exchange = (uint8_t)ipsec_profile_int_parse(FLAG_NONE,
                                                               p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*local id*/
    p_tmp = &(prof[prof_type][prof_idx].local_id[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*remote_id*/
    p_tmp = &(prof[prof_type][prof_idx].remote_id[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*keylife_p1*/
    prof[prof_type][prof_idx].keylife_p1 = (uint32_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                  p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*xauth*/
    prof[prof_type][prof_idx].xauth = (uint8_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                 p_end, &i);
    p_end += i;
    /*xauth_account*/
    p_tmp = &(prof[prof_type][prof_idx].xauth_account[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*xauth_password*/
    p_tmp = &(prof[prof_type][prof_idx].xauth_password[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*xauth_server_type,USER auth: auth2meth for IKEv2*/
    p_tmp = &(prof[prof_type][prof_idx].auth2meth[0]);
    ipsec_profile_str_parse(p_end, p_tmp, &i);
    p_end += i;
    /*traversal*/
    prof[prof_type][prof_idx].traversal = (uint16_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                 p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*ike_isakmp*/
    prof[prof_type][prof_idx].ike_isakmp_port = (uint16_t)ipsec_profile_int_parse(
                                                    FLAG_NONE, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*ike_isakmp_nat*/
    prof[prof_type][prof_idx].ike_isakmp_nat_port = (uint16_t)ipsec_profile_int_parse(
                                                        FLAG_NONE, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*ipsec_dpd*/
    prof[prof_type][prof_idx].ipsec_dpd = (uint16_t)ipsec_profile_int_parse(
                                              FLAG_NONE, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*dead_peer_detection*/
    prof[prof_type][prof_idx].dead_peer_detection = (uint16_t)ipsec_profile_int_parse(
                                                        FLAG_NONE, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*encryption_p2*/
    prof[prof_type][prof_idx].encryption_p2 = (uint16_t)ipsec_profile_int_parse(
                                                  FLAG_IKE_ENCRYPT, p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*hash_p2*/
    prof[prof_type][prof_idx].hash_p2 = (uint16_t)ipsec_profile_int_parse(FLAG_ESP_HASH,
                                                               p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*keylife_p2: IPSEC phase 2*/
    prof[prof_type][prof_idx].keylife_p2 = (uint32_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                  p_end, &i);
    p_end += i; /*to shifft next '>'*/
    /*keyingtries*/
    prof[prof_type][prof_idx].keyingtries = (uint16_t)ipsec_profile_int_parse(FLAG_NONE,
                                                                   p_end, &i);
    p_end += i; /*to shifft next '>'*/

    /*ipsec_conn_en*/
	/*the last one doesn't need to parse ">".*/
	prof[prof_type][prof_idx].ipsec_conn_en = atoi(p_end);
    /*the end of profile*/
    return;
}

int pre_ipsec_prof_set()
{
    char buf[SZ_MIN];
    char *p_tmp = NULL, buf1[SZ_BUF];
    int i, rc = 0, prof_count = 0;

    p_tmp = &buf1[0];
    memset(p_tmp, 0, sizeof(char) * SZ_MIN);    
	for(prof_count = PROF_CLI; prof_count < PROF_ALL; prof_count++){
	    for(i = 1; i <= MAX_PROF_NUM; i++){
			if(PROF_SVR == prof_count)
	        	sprintf(&buf[0], "ipsec_profile_%d", i);
			else if(PROF_CLI == prof_count)
				sprintf(&buf[0], "ipsec_profile_client_%d", i);
				
	        if(NULL != nvram_safe_get(&buf[0])){
	            strcpy(p_tmp, nvram_safe_get(&buf[0]));
	            /*to avoid nvram that it has not been inited ready*/
	            if(0 != *p_tmp){
		                ipsec_prof_fill(i-1, p_tmp,prof_count);
	                rc = 1;
	            }
	        }
	    }
	}

    return rc;
}

int pre_ipsec_samba_prof_set()
{
    char buf[SZ_BUF];
    char *p_tmp = NULL;
    int rc = 0;
    memset((ipsec_samba_t *)&samba_prof, 0, sizeof(ipsec_samba_t));
    p_tmp = &buf[0];
    memset(p_tmp, '\0', sizeof(char) * SZ_BUF);
    strcpy(p_tmp, nvram_safe_get("ipsec_samba"));
    DBG(("ipsec_samba#%s[p_tmp]\n", p_tmp));
    samba_prof.samba_en = atoi(p_tmp);
    DBG(("ipsec_samba#%d[samba_prof.samba_en]\n", samba_prof.samba_en));
    memset(p_tmp, '\0', sizeof(char) * SZ_BUF);
    DBG(("ipsec_samba#%d[samba_prof.samba_en]\n", samba_prof.samba_en));
    if(NULL != nvram_safe_get("ipsec_samba_list")){
        strcpy(p_tmp, nvram_safe_get("ipsec_samba_list"));
        DBG(("pre_ipsec_samba_prof_set#%s\n", p_tmp));
        /*to avoid nvram that it has not been inited ready*/
        if('\0' != *p_tmp){
            ipsec_samba_prof_fill(p_tmp);
            rc = 1;
        }
    }
DBG(("samba_en[%d] dns1:%s\n, dns2:%s\n, wins1=%s\n, wins2=%s\n",
     samba_prof.samba_en,
     samba_prof.dns1, samba_prof.dns2, samba_prof.nbios1, samba_prof.nbios2));
    return rc;
}


void rc_ipsec_conf_set()
{
    int rc = 0;

    rc = pre_ipsec_prof_set();
    if(0 < rc) {
        rc_ipsec_topology_set();
    }
    return;
}

void rc_ipsec_secrets_init()
{
    FILE *fp = NULL;

    fp = fopen("/tmp/etc/ipsec.secrets", "w");

    fprintf(fp,"#/etc/ipsec.secrets set\n"
                " : PSK 1234567\n\n"
                "test_secret : XAUTH 1234\n"
                "tingya_secret : EAP 1234\n");
    if(NULL != fp){
        fclose(fp);
    }
    return;
}

void rc_strongswan_conf_set()
{
    int rc = 0;
    FILE *fp = NULL;

    rc = pre_ipsec_samba_prof_set();
DBG(("ipsec_samba#%d[samba_prof.samba_en]\n", samba_prof.samba_en));
    fp = fopen("/tmp/etc/strongswan.conf", "w");
    fprintf(fp, "# strongswan.conf - strongSwan configuration file\n#\n"
                "# Refer to the strongswan.conf(5) manpage for details\n#\n"
                "# Configuration changes should be made in the included files"
                "\ncharon {\n\n\n"
                "  send_vendor_id = yes\n"
                "  duplicheck.enable = no\n"
                "  starter { load_warning = no }\n\n"
                "  load_modular = yes\n\n"
                "  i_dont_care_about_security_and_use_aggressive_mode_psk = yes\n\n"
                "  plugins {\n    include strongswan.d/charon/*.conf\n  }\n"
                "  filelog {\n      /var/log/strongswan.charon.log {\n"
                "        time_format = %%b %%e %%T\n        default = 1\n"
                "        append = no\n        flush_line = yes\n"
                "     }\n  }\n");
    if((0 != rc) && (1 == samba_prof.samba_en)){
        if(('n' != samba_prof.dns1[0]) && ('\0' != samba_prof.dns1[0])){
            fprintf(fp,"\n  dns1=%s\n", samba_prof.dns1);
        }
        if(('n' != samba_prof.dns2[0]) && ('\0' != samba_prof.dns2[0])){
            fprintf(fp,"  dns2=%s\n", samba_prof.dns2);
        }
        if(('n' != samba_prof.nbios1[0]) && ('\0' != samba_prof.nbios1[0])){
            fprintf(fp,"\n\n  wins1=%s\n", samba_prof.nbios1);
        }
        if(('n' != samba_prof.nbios2[0]) && ('\0' != samba_prof.nbios2[0])){
            fprintf(fp,"  wins2=%s\n", samba_prof.nbios2);
        }
    }
    fprintf(fp, "\n}#the end of the Charon {\n\n");
    if(NULL != fp){
        fclose(fp);
    }
DBG(("[%d]strongswan:samba_en[%d]dns1:%s\n, dns2:%s\n, wins1=%s\n, wins2=%s\n",
      rc, samba_prof.samba_en, 
      samba_prof.dns1, samba_prof.dns2, samba_prof.nbios1, samba_prof.nbios2));
    return;
}

void rc_ipsec_ca2ipsecd_cp(FILE *fp, uint32_t idx)
{
    fprintf(fp, "cp -r %s%d_asusCert.pem /tmp/etc/ipsec.d/cacerts/\n"
                "cp -r %s%d_svrCert.pem /tmp/etc/ipsec.d/certs/\n"
                "cp -r %s%d_svrKey.pem /tmp/etc/ipsec.d/private/\n"
                "cp -r %s%d_cliCert.pem /tmp/etc/ipsec.d/certs/\n"
                "cp -r %s%d_cliKey.pem /tmp/etc/ipsec.d/private/\n"
                "echo %s > %s%d_p12.pwd\n",
                FILE_PATH_CA_ETC, idx, FILE_PATH_CA_ETC, idx,
                FILE_PATH_CA_ETC, idx, FILE_PATH_CA_ETC, idx,
                FILE_PATH_CA_ETC, idx, ca_tab[idx].p12_pwd,
                FILE_PATH_CA_ETC, idx);
    return;
}


void rc_ipsec_pki_gen_exec(uint32_t idx)
{
    FILE *fp = NULL;
	char *argv[3];
	argv[0] = "/bin/sh";
	argv[1] = FILE_PATH_CA_GEN_SH"&";
	argv[2] = NULL;

    fp = fopen(FILE_PATH_CA_GEN_SH, "a+w");

    if(NULL != fp){
        rc_ipsec_ca2ipsecd_cp(fp, idx);
        fclose(fp);
        DBG(("to run %s in the background!\n", FILE_PATH_CA_GEN_SH));
        //system("."FILE_PATH_CA_GEN_SH"&");
        _eval(argv, NULL, 0, NULL);
    }
    return;
}

void rc_ipsec_start(FILE *fp)
{
    if(NULL != fp){
        fprintf(fp,"ipsec start > /dev/null 2>&1 \n");
    }
    return;
}

void rc_ipsec_up(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    if((NULL != fp) && ('\0' != prof[prof_type][prof_idx].profilename[0])){
        fprintf(fp, "sleep 1 > /dev/null 2>&1\n"
                 "ipsec up %s > /dev/null 2>&1 \n", prof[prof_type][prof_idx].profilename);
    }
    return;
}

void rc_ipsec_down(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    if((NULL != fp) && ('\0' != prof[prof_type][prof_idx].profilename[0])){
        fprintf(fp, "sleep 1 > /dev/null 2>&1\n"
                "ipsec down %s > /dev/null 2>&1\n", prof[prof_type][prof_idx].profilename);
    }
    return;
}

void rc_ipsec_restart(FILE *fp)
{
    if(NULL != fp){
        /*to do this command after ipsec reload command has been exec*/
        fprintf(fp, "ipsec restart > /dev/null 2>&1\n");
    }
    return;
}

void rc_ipsec_reload(FILE *fp)
{
    if(NULL != fp){
        /* to do this command after ipsec config has been modified */
        /* and ipsec pthread has already run                       */
        fprintf(fp, "ipsec reload > /dev/null 2>&1\n");
    }
    return;
}

void rc_ipsec_rereadall(FILE *fp)
{
    if(NULL != fp){
        fprintf(fp, "ipsec rereadall > /dev/null 2>&1\n");
    }
    return;
}

void rc_ipsec_stop(FILE *fp)
{
    if(NULL != fp){
        /*Disabled ipsec*/
        fprintf(fp, "ipsec stop > /dev/null 2>&1\n");
    }
    return;
}

int rc_ipsec_ca_fileidx_check()
{
    char awk_cmd[SZ_BUF], str_buf[SZ_MIN];
    FILE *fp = NULL;
    uint32_t file_idx_32bits = 0, i_tmp = 0;
	char *argv[3];
	
	argv[0] = "/bin/sh";
	argv[1] = FILE_PATH_CA_CHECK_SH;
	argv[2] = NULL;
    
    fp = fopen(FILE_PATH_CA_CHECK_SH, "w");
    
    sprintf(awk_cmd, "awk '{for ( x = 0; x < %d; x++ )"
                     "{ if($1 == x\"_cliKey.pem\")"
                     "{ print x}}}' %s > %s",CA_FILES_MAX_NUM,
                      FILE_PATH_CA_PKI_TEMP, FILE_PATH_CA_TEMP);
    fprintf(fp, "ls %s > %s\n%s\n", FILE_PATH_CA_ETC, 
                                    FILE_PATH_CA_PKI_TEMP, awk_cmd);
    if(NULL != fp){
        fclose(fp);
    }
	chmod(FILE_PATH_CA_CHECK_SH, 0777);
    //system("."FILE_PATH_CA_CHECK_SH);
    _eval(argv, NULL, 0, NULL);
	
    fp = fopen(FILE_PATH_CA_TEMP, "r");
    if(NULL == fp){
        return file_idx_32bits;
    }
    memset(str_buf, 0, SZ_MIN);
    while(EOF != fscanf(fp, "%s", str_buf)){
        i_tmp = atoi(str_buf);
        file_idx_32bits |= (uint32_t)(1 << i_tmp);
        memset(str_buf, 0, SZ_MIN);
    }
    if(NULL != fp){
        fclose(fp);
    }
    return file_idx_32bits;
}

int rc_ipsec_ca_fileidx_available_get()
{
    unsigned int idx = 0;
    uint32_t file_idx_32bits = 0;

    file_idx_32bits = rc_ipsec_ca_fileidx_check();
    DBG(("__rc_ipsec_ca_fileidx_available_get:0x%x\n", file_idx_32bits));
    for(idx = 0; idx < CA_FILES_MAX_NUM; idx++){
        if(0 == (file_idx_32bits & (uint32_t)(1 << idx))){
            file_idx_32bits |= (uint32_t)(1 << idx);
            DBG(("file_idx_32bits:0x%x\n", file_idx_32bits));
            return idx;
        }
    }
    return CA_FILES_MAX_NUM;
}

/*return value : ca_file's index*/
int rc_ipsec_ca_txt_parse()
{
    char buf[SZ_BUF];
    char *p_tmp = NULL, *p_buf = NULL; 
    int file_idx = 0;
    /*ca index star from 0*/
    sprintf(buf, "%s", nvram_safe_get("ca_manage_profile"));
    p_buf = &buf[0];
   
    file_idx = rc_ipsec_ca_fileidx_available_get();

    if(CA_FILES_MAX_NUM == file_idx){
        DBG(("ca files reach the maxmum numbers(%d)", file_idx));
        return file_idx;
    }
 
    p_tmp = (char *)&(ca_tab[file_idx].p12_pwd[0]);
    while('>' != *p_buf){
        *p_tmp = *p_buf;
        p_tmp++;
        p_buf++;
    }
    *p_tmp = '\0';
    
    p_tmp = (char *)&(ca_tab[file_idx].ca_txt[0]);
    while('>' != *(++p_buf)){
        *p_tmp = *p_buf;
        p_tmp++;
    }
    *p_tmp = '\0';

    p_tmp = (char *)&(ca_tab[file_idx].san[0]);
    while('\0' != *(++p_buf)){
        *p_tmp = *p_buf;
        p_tmp++;
    }
    *p_tmp = '\0';
    /*to re-parsing root CA cert for server,client cert*/
    p_tmp = (char *)&(ca_tab[file_idx].ca_cert[0]);
    p_buf = (char *)&(ca_tab[file_idx].ca_txt[0]);

    memset(p_tmp, 0, SZ_BUF);
    while('\0' != *p_buf){
        if(('C' == *p_buf) && ('N' == *(p_buf + 1)) && ('=' == *(p_buf + 2))){
            break;
        }
        *p_tmp++ = *p_buf++;
    }

    *p_tmp = '\0';
    return file_idx;
}

int rc_ipsec_ca_gen()
{
    FILE *fp = NULL;
    int file_idx = 0;

    fp = fopen(FILE_PATH_CA_GEN_SH,"w");
    file_idx = rc_ipsec_ca_txt_parse();
    fprintf(fp,"#%s\npki --gen --outform pem > "FILE_PATH_CA_ETC"%d_ca.pem \n"
               "pki --self --in "FILE_PATH_CA_ETC"%d_ca.pem --dn \"%s\""
               " --ca --outform pem > "FILE_PATH_CA_ETC"%d_asusCert.pem\n"
               "pki --gen --outform pem > "FILE_PATH_CA_ETC"%d_svrKey.pem\n"
               "pki --pub --in "FILE_PATH_CA_ETC"%d_svrKey.pem | "
               "pki --issue --cacert "FILE_PATH_CA_ETC"%d_asusCert.pem"
               " --cakey "FILE_PATH_CA_ETC"%d_ca.pem --dn \"%s CN=%s\""
               " --san=%s --outform pem > "FILE_PATH_CA_ETC"%d_svrCert.pem\n"
               "pki --gen --outform pem > "FILE_PATH_CA_ETC"%d_cliKey.pem\n"
               "pki --pub --in "FILE_PATH_CA_ETC"%d_cliKey.pem | "
               "pki --issue --cacert "FILE_PATH_CA_ETC"%d_asusCert.pem"
               " --cakey "FILE_PATH_CA_ETC"%d_ca.pem --dn \"%s CN=@client\" "
               "--san=@client --outform pem > %s%d_cliCert.pem\n"
               "export RANDFILE="FILE_PATH_CA_ETC".rnd\n"
               "openssl pkcs12 -export -inkey %s%d_cliKey.pem "
               " -in %s%d_cliCert.pem -name \"client Cert\" "
               " -certfile %s%d_asusCert.pem -caname \"ASUS Root CA\""
               " -out %s%d_cliCert.p12 -password pass:%s\n",
               FILE_PATH_CA_GEN_SH, file_idx, 
               file_idx, ca_tab[file_idx].ca_txt,
               file_idx,file_idx, file_idx, file_idx,
               file_idx, ca_tab[file_idx].ca_cert, ca_tab[file_idx].san,
               ca_tab[file_idx].san, file_idx, file_idx, file_idx, file_idx,
               file_idx, ca_tab[file_idx].ca_cert, FILE_PATH_CA_ETC, file_idx,
               FILE_PATH_CA_ETC, file_idx, FILE_PATH_CA_ETC, file_idx,
               FILE_PATH_CA_ETC, file_idx, FILE_PATH_CA_ETC, file_idx,
               ca_tab[file_idx].p12_pwd);
    if(NULL != fp){
        fclose(fp);
    }
	chmod(FILE_PATH_CA_GEN_SH, 0777);
    return file_idx;
}

void rc_ipsec_ca_import(uint32_t ca_type, FILE *fp)
{
#if 0
    for(i = 1; i <= MAX_PROF_NUM; i++){
        memset(ca_prof_name, 0, sizeof(char) * SZ_MIN);
        sprintf(ca_prof_name, "ipsec_ca_%d", i);
        if(NULL != nvram_safe_get(ca_prof_name)){
            memset(cert_name, 0, sizeof(char) * SZ_MIN);
            sprintf(cert_name, "/tmp/etc/ipsec.d/certs/ipsec_cert_%d.pem", i);
            fp = fopen(cert_name, "w");

            fprintf(fp, "%s", nvram_safe_get(ca_prof_name));
            if(NULL != fp){
                fclose(fp);
            }
        }
    }
#endif
    switch(ca_type){
    case CA_TYPE_CACERT:
    break;
    case CA_TYPE_CERT:
    break;
    case CA_TYPE_PRIVATE_KEY:
    break;
    case CA_TYPE_PKS12:
        /*to get verify code(password)*/
    break;
    }

    return; 
}

void rc_ipsec_cert_import(char *asus_cert, char *ipsec_cli_cert,
                          char *ipsec_cli_key, char *pks12)
{
    char *p_file = NULL;
    char cmd[SZ_BUF], tmp[SZ_MIN];
    if(NULL != asus_cert){
        memset(cmd, 0, sizeof(char) * SZ_BUF);
        sprintf(&cmd[0], "cp -r %s /tmp/etc/ipsec.d/cacerts/", asus_cert);
        system(cmd);
    }
    if(NULL != ipsec_cli_cert){
        memset(cmd, 0, sizeof(char) * SZ_BUF);
        sprintf(&cmd[0], "cp -r %s /tmp/etc/ipsec.d/certs/", ipsec_cli_cert);
        system(cmd);
    }
    if(NULL != ipsec_cli_key){
        memset(cmd, 0, sizeof(char) * SZ_BUF);
        sprintf(&cmd[0], "cp -r %s /tmp/etc/ipsec.d/private/", ipsec_cli_key);
        system(cmd);
    }
    if(NULL != pks12){
        if(NULL != nvram_safe_get("ca_manage_profile")){
            DBG(("pks12:%s", pks12));
            p_file = &tmp[0];
            memset(p_file, '\0', sizeof(char) * SZ_MIN);
            strncpy(p_file, pks12, strlen(pks12) - 4); /*4 : strlen(p12)*/
            sprintf(cmd, "echo %s > "FILE_PATH_CA_ETC"%s.pwd", 
                    nvram_safe_get("ca_manage_profile"), p_file);
            system(cmd);
            memset(cmd, 0, sizeof(char) * SZ_BUF);
            sprintf(&cmd[0], "openssl pkcs12 -in %s%s -clcerts -out "
                             "/tmp/etc/ipsec.d/certs/%s.pem -password pass:%s", 
                             FILE_PATH_CA_ETC, pks12, p_file, 
                             nvram_safe_get("ca_manage_profile"));
            DBG((cmd));
            system(cmd);
            memset(cmd, 0, sizeof(char) * SZ_BUF);
            sprintf(&cmd[0], "openssl pkcs12 -in %s%s -cacerts -out "
                             "/tmp/etc/ipsec.d/cacerts/%s.pem"
                             " -password pass:%s",
                             FILE_PATH_CA_ETC, pks12, p_file,
                             nvram_safe_get("ca_manage_profile"));
            DBG((cmd));
            system(cmd);
            memset(cmd, 0, sizeof(char) * SZ_BUF);
            sprintf(&cmd[0], "openssl pkcs12 -in %s%s -out "
                    "/tmp/etc/ipsec.d/certs/%s.pem -nodes -password pass:%s",
                     FILE_PATH_CA_ETC, pks12, p_file,
                     nvram_safe_get("ca_manage_profile"));
            DBG((cmd));
            system(cmd);
        }
    }
    return;
}

void rc_ipsec_ca_export(char *verify_pwd)
{
    char cmd[SZ_BUF];
    if(NULL != verify_pwd){
        sprintf(&cmd[0], "openssl pkcs12 -export -inkey ipsec_cliKey.pem "
                " -in ipsec_cliCert.pem -name \"IPSEC client\" "
                " -certfile asusCert.pem -caname \"ASUS Root CA\""
                " -out ipsec_cliCert.p12");
        system(cmd);
        system(verify_pwd);
    }
    return;
}

void rc_ipsec_ca_init( )
{
    FILE *fp = NULL;
	char *argv[3];

	argv[0] = "/bin/sh";
	argv[1] = FILE_PATH_CA_ETC"ca_init.sh&";
	argv[2] = NULL;

    fp = fopen(FILE_PATH_CA_ETC"ca_init.sh", "w");
    if(NULL != fp){
        fprintf(fp, "cp -r %s*asusCert.pem /tmp/etc/ipsec.d/cacerts/\n"
                    "cp -r %s*Cert.pem /tmp/etc/ipsec.d/certs/\n"
                    "cp -r %s*Key.pem /tmp/etc/ipsec.d/private/\n",
                    FILE_PATH_CA_ETC, FILE_PATH_CA_ETC, FILE_PATH_CA_ETC);
        fclose(fp);
    }
    chmod(FILE_PATH_CA_ETC"ca_init.sh", 0777);
    DBG(("to run "FILE_PATH_CA_ETC"ca_init.sh in the background!\n"));
    //system("."FILE_PATH_CA_ETC"ca_init.sh&");
	_eval(argv, NULL, 0, NULL);
    return;
}

void rc_ipsec_conf_default_init()
{
    FILE *fp = NULL;
    fp = fopen("/tmp/etc/ipsec.conf", "w");
    fprintf(fp, "# /etc/ipsec.conf\n"
                "config setup\n\n\n"
                "conn %%default\n"
                "  ikelifetime=60m\n"
                "  keylife=20m\n"
                "  rekeymargin=3m\n"
                "  keyingtries=1\n"
                "  keyexchange=ike\n\n");
    if(NULL != fp){
        fclose(fp);
    }
    return;
}

void rc_ipsec_psk_xauth_rw_init()
{
    FILE *fp = NULL;
    fp = fopen("/tmp/etc/ipsec.conf", "a+w");
    fprintf(fp,"#also supports iOS PSK and Shrew on Windows\n"
                "conn android_xauth_psk\n"
                "  keyexchange=ikev1\n"
                "  left=%%defaultroute\n"
                "  leftauth=psk\n"
                "  leftsubnet=0.0.0.0/0\n"
                "  right=%%any\n"
                "  rightauth=psk\n"
                "  rightauth2=xauth\n"
                "  rightsourceip=10.2.1.0/24\n"
                "  auto=add\n\n");
    if(NULL != fp){
        fclose(fp);
    }
    return;
}

void rc_ipsec_secrets_set()
{
    char ipsec_client_list_name[SZ_MIN], buf[SZ_BUF], s_tmp[SZ_TMP];
    char auth2meth[SZ_MIN];
    char *p_str = NULL, *p_str1 = NULL;
    int i,prof_count = 0, unit;
	char tmpStr[5];
    FILE *fp = NULL;
    fp = fopen("/tmp/etc/ipsec.secrets", "w");
    fprintf(fp,"#/etc/ipsec.secrets\n\n");
	
	for(prof_count = PROF_CLI; prof_count < PROF_ALL; prof_count++){
    	for(i = 0; i < MAX_PROF_NUM; i++){
	        if((0 != strcmp(prof[prof_count][i].auth_method_key, "null")) &&
	           ('\0' != prof[prof_count][i].auth_method_key[0]) &&
	           ((1 == prof[prof_count][i].auth_method) || (0 == prof[prof_count][i].auth_method))){

			   if(strcmp(prof[prof_count][i].local_public_interface,"wan") == NULL){
				   for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
						sprintf(tmpStr, "wan%d_ipaddr", unit);
						strcpy(prof[prof_count][i].local_pub_ip,nvram_safe_get(tmpStr));
						//DBG(("=====(%s:%s->%d):prof[%d][%d].local_pub_ip(%s)=====\n",__FILE__,__FUNCTION__,__LINE__,prof_count,i,prof[prof_count][i].local_pub_ip));
						fprintf(fp,"\n %s %s : %s %s\n\n"
	               , ((0 == strcmp(prof[prof_count][i].local_id, "null") ||
	                  ('\0' == prof[prof_count][i].local_id[0])) ?
	                  ((('\0' == prof[prof_count][i].local_pub_ip[0]) ||
	                    ('n' == prof[prof_count][i].local_pub_ip[0])) ? "\%any" :
	                 nvram_safe_get(tmpStr) ) : prof[prof_count][i].local_id)
	               , ((0 == strcmp(prof[prof_count][i].remote_id, "null") ||
	                  ('\0' == prof[prof_count][i].remote_id[0])) ?
	                  ((('\0' == prof[prof_count][i].remote_gateway[0]) ||
	                    ('n' == prof[prof_count][i].remote_gateway[0])) ? "\%any" : 
	                    prof[prof_count][i].remote_gateway) : prof[prof_count][i].remote_id)
	               , ((0 == prof[prof_count][i].auth_method) ? "RSA" : "PSK")
	               , prof[prof_count][i].auth_method_key);
				   }
				   
			   }
			   else{
			   	   fprintf(fp,"\n %s %s : %s %s\n\n"
	               , ((0 == strcmp(prof[prof_count][i].local_id, "null") ||
	                  ('\0' == prof[prof_count][i].local_id[0])) ?
	                  ((('\0' == prof[prof_count][i].local_pub_ip[0]) ||
	                    ('n' == prof[prof_count][i].local_pub_ip[0])) ? "\%any" :
	                 prof[prof_count][i].local_pub_ip ) : prof[prof_count][i].local_id)
	               , ((0 == strcmp(prof[prof_count][i].remote_id, "null") ||
	                  ('\0' == prof[prof_count][i].remote_id[0])) ?
	                  ((('\0' == prof[prof_count][i].remote_gateway[0]) ||
	                    ('n' == prof[prof_count][i].remote_gateway[0])) ? "\%any" : 
	                    prof[prof_count][i].remote_gateway) : prof[prof_count][i].remote_id)
	               , ((0 == prof[prof_count][i].auth_method) ? "RSA" : "PSK")
	               , prof[prof_count][i].auth_method_key);
			   	}
				
           
        }
        /*second-factor auth*/
	        if((IKE_TYPE_V1 == prof[prof_count][i].ike) && 
	           (IPSEC_AUTH2_TYP_CLI == prof[prof_count][i].xauth)){
	            fprintf(fp, "#cli[%d]\n %s : XAUTH %s\n", i, prof[prof_count][i].xauth_account
	                      , prof[prof_count][i].xauth_password);
	        }else if((IKE_TYPE_V2 == prof[prof_count][i].ike) &&
	           (IPSEC_AUTH2_TYP_CLI == prof[prof_count][i].xauth)){
	            fprintf(fp, "#cli[%d]\n %s : EAP %s\n", i, prof[prof_count][i].xauth_account
	                      , prof[prof_count][i].xauth_password);
        }
        memset(ipsec_client_list_name, 0, sizeof(char) * SZ_MIN);
        sprintf(ipsec_client_list_name, "ipsec_client_list_%d", i);
        if(NULL != nvram_safe_get(ipsec_client_list_name)){
            p_str = &buf[0];
            p_str1 = &s_tmp[0];
            memset(buf, 0, sizeof(char) * SZ_BUF);
            memset(s_tmp, 0, sizeof(char) * SZ_BUF);
            strcpy(buf, nvram_safe_get(ipsec_client_list_name));
            while('\0' != *p_str){
                if('<' == *p_str){
                    *p_str = '\n';
                }
                if('>' == *p_str){
                    *p_str1 = *p_str++ = ' ';
                    memset(auth2meth, 0, sizeof(char) * SZ_MIN);
                    sprintf(auth2meth, ": %s ", 
	                            (IKE_TYPE_V2 == prof[prof_count][i].ike) ? "EAP" : "XAUTH");
                    sprintf(p_str1 + 1, "%s", auth2meth);
                    p_str1 += strlen(auth2meth) + 1;
                }
                *p_str1++ = *p_str++;
            }
            p_str1 = '\0';
            fprintf(fp, "\n#ipsec_client_list_%d\n\n%s\n", i, s_tmp);
        }
    	}
	}
    if(NULL != fp){
        fclose(fp);
    }
    return;
}

void ipsec_conf_local_set(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    fprintf(fp, "  left=%%defaultroute\n  #receive web value#left=%s\n"
                "  leftsubnet=%s\n"
                "  leftfirewall=%s\n  #interface=%s\n"
                , prof[prof_type][prof_idx].local_pub_ip
                , (VPN_TYPE_HOST_NET == prof[prof_type][prof_idx].vpn_type) ?
                  "0.0.0.0/0" : prof[prof_type][prof_idx].local_subnet
                , ((VPN_TYPE_NET_NET_CLI == prof[prof_type][prof_idx].vpn_type) || 
                   (prof[prof_type][prof_idx].traversal == 1)) ? "yes" : "no"
                , prof[prof_type][prof_idx].local_public_interface
           );
    if((0 != prof[prof_type][prof_idx].local_port) && (500 != prof[prof_type][prof_idx].local_port) &&
       (4500 != prof[prof_type][prof_idx].local_port)){
        fprintf(fp, "  leftprotoport=17/%d\n", prof[prof_type][prof_idx].local_port);
    }
    if(IKE_TYPE_AUTO != prof[prof_type][prof_idx].ike){
        fprintf(fp, "  leftauth=%s\n"
                , (prof[prof_type][prof_idx].auth_method == 1) ? "psk" :
                   ((prof[prof_type][prof_idx].ike == IKE_TYPE_V2) ?
                     prof[prof_type][prof_idx].auth2meth : "pubkey")
               );
    }
    if((VPN_TYPE_NET_NET_CLI == prof[prof_type][prof_idx].vpn_type) &&
       (prof[prof_type][prof_idx].ike == IKE_TYPE_V1) && 
       (IPSEC_AUTH2_TYP_DIS != prof[prof_type][prof_idx].xauth)){
        fprintf(fp, "  rightauth2=xauth\n");
    }else if((VPN_TYPE_NET_NET_CLI == prof[prof_type][prof_idx].vpn_type) &&
       (prof[prof_type][prof_idx].ike == IKE_TYPE_V2) && 
       (IPSEC_AUTH2_TYP_DIS != prof[prof_type][prof_idx].xauth)){
        fprintf(fp, "  rightauth2=%s\n", prof[prof_type][prof_idx].auth2meth);
    }
    if((0 != strcmp(prof[prof_type][prof_idx].local_id, "null")) && 
       ('\0' != prof[prof_type][prof_idx].local_id[0])){
        fprintf(fp, "  leftid=%s\n", prof[prof_type][prof_idx].local_id);
    }
    return;
}

void ipsec_conf_remote_set(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    if((VPN_TYPE_NET_NET_SVR == prof[prof_type][prof_idx].vpn_type) || 
       (VPN_TYPE_HOST_NET == prof[prof_type][prof_idx].vpn_type)){
        fprintf(fp, "  right=%%any\n");
    }else{
        fprintf(fp, "  right=%s\n", prof[prof_type][prof_idx].remote_gateway);
    }

    if((0 != prof[prof_type][prof_idx].remote_port) && (500 != prof[prof_type][prof_idx].remote_port) 
        && (4500 != prof[prof_type][prof_idx].remote_port)){
        fprintf(fp, "  rightprotoport=17/%d\n", prof[prof_type][prof_idx].remote_port);
    }

    if(VPN_TYPE_HOST_NET != prof[prof_type][prof_idx].vpn_type){
        fprintf(fp, "  rightsubnet=%s\n",prof[prof_type][prof_idx].remote_subnet);
    }
    if(IKE_TYPE_AUTO != prof[prof_type][prof_idx].ike){
        fprintf(fp, "  rightauth=%s\n"
                  , (prof[prof_type][prof_idx].auth_method == 1) ? "psk" :
                     ((prof[prof_type][prof_idx].ike == IKE_TYPE_V2) ?
                     prof[prof_type][prof_idx].auth2meth : "pubkey")
               );
    }
    if(((VPN_TYPE_NET_NET_SVR == prof[prof_type][prof_idx].vpn_type) ||
        (VPN_TYPE_HOST_NET == prof[prof_type][prof_idx].vpn_type)) &&
       (prof[prof_type][prof_idx].ike == IKE_TYPE_V1) && 
       (IPSEC_AUTH2_TYP_DIS != prof[prof_type][prof_idx].xauth)){
        fprintf(fp, "  rightauth2=xauth\n");
    }else if(((VPN_TYPE_NET_NET_SVR == prof[prof_type][prof_idx].vpn_type) ||
        (VPN_TYPE_HOST_NET == prof[prof_type][prof_idx].vpn_type)) &&
       (prof[prof_type][prof_idx].ike == IKE_TYPE_V2) &&
       (IPSEC_AUTH2_TYP_DIS != prof[prof_type][prof_idx].xauth)){
        fprintf(fp, "  rightauth2=%s\n", prof[prof_type][prof_idx].auth2meth);
    }
    if(VPN_TYPE_HOST_NET == prof[prof_type][prof_idx].vpn_type){
        if((0 != strcmp(prof[prof_type][prof_idx].virtual_subnet, "null")) &&
           ('\0' != prof[prof_type][prof_idx].virtual_subnet[0])){
            fprintf(fp, "#sourceip_en=%s\n  rightsourceip=%s\n"
                 , prof[prof_type][prof_idx].virtual_ip_en, prof[prof_type][prof_idx].virtual_subnet);
        }
    }
    if((0 != strcmp(prof[prof_type][prof_idx].remote_id, "null")) &&
       ('\0' != prof[prof_type][prof_idx].remote_id[0])){
        fprintf(fp, "  rightid=%s\n", prof[prof_type][prof_idx].remote_id);
    }
    return;
}

void ipsec_conf_phase1_set(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    fprintf(fp, "  ikelifetime=%d\n", prof[prof_type][prof_idx].keylife_p1);
    if((ENCRYPTION_TYPE_MAX_NUM != prof[prof_type][prof_idx].encryption_p1) &&
       (HASH_TYPE_MAX_NUM != prof[prof_type][prof_idx].hash_p1)){
        fprintf(fp,"  ike=%s-%s-%s\n", encryp[prof[prof_type][prof_idx].encryption_p1]
                  , hash[prof[prof_type][prof_idx].hash_p1], dh_group[DH_GROUP_14]);
    } else if((ENCRYPTION_TYPE_MAX_NUM == prof[prof_type][prof_idx].encryption_p1) && 
              (HASH_TYPE_MAX_NUM != prof[prof_type][prof_idx].hash_p1)){
        fprintf(fp,"  ike=%s-%s-%s\n", encryp[ENCRYPTION_TYPE_AES128]
                  , hash[prof[prof_type][prof_idx].hash_p1], dh_group[DH_GROUP_14]);
    } else if((ENCRYPTION_TYPE_MAX_NUM != prof[prof_type][prof_idx].encryption_p1) && 
              (HASH_TYPE_MAX_NUM == prof[prof_type][prof_idx].hash_p1)){
        fprintf(fp,"  ike=%s-%s-%s\n", encryp[prof[prof_type][prof_idx].encryption_p1]
                  , hash[HASH_TYPE_SHA1], dh_group[DH_GROUP_14]);
    }
    return;
}

void ipsec_conf_phase2_set(FILE *fp, int prof_idx, ipsec_prof_type_t prof_type)
{
    fprintf(fp, "  keylife=%d\n", prof[prof_type][prof_idx].keylife_p2);
    if((ENCRYPTION_TYPE_MAX_NUM != prof[prof_type][prof_idx].encryption_p2) && 
       (HASH_TYPE_MAX_NUM != prof[prof_type][prof_idx].hash_p2)){
        fprintf(fp,"  esp=%s-%s\n", encryp[prof[prof_type][prof_idx].encryption_p2]
                  , hash[prof[prof_type][prof_idx].hash_p2]);
    } else if((ENCRYPTION_TYPE_MAX_NUM == prof[prof_type][prof_idx].encryption_p2) &&
              (HASH_TYPE_MAX_NUM != prof[prof_type][prof_idx].hash_p2)){
        fprintf(fp,"  esp=%s-%s\n", encryp[ENCRYPTION_TYPE_AES128]
                  , hash[prof[prof_type][prof_idx].hash_p2]);
    } else if((ENCRYPTION_TYPE_MAX_NUM != prof[prof_type][prof_idx].encryption_p2) &&
              (HASH_TYPE_MAX_NUM == prof[prof_type][prof_idx].hash_p2)){
        fprintf(fp,"  esp=%s-%s\n", encryp[prof[prof_type][prof_idx].encryption_p2]
                  , hash[HASH_TYPE_SHA1]);
    }
    return;
}

void rc_ipsec_topology_set()
{
    int i,prof_count = 0;
    FILE *fp = NULL;
    char *p_tmp = NULL, buf[SZ_MIN];
    p_tmp = &buf[0];

    char *s_tmp = NULL, buf1[SZ_BUF];
    s_tmp = &buf1[0];

    memset(p_tmp, 0, sizeof(char) * SZ_MIN);
    fp = fopen("/tmp/etc/ipsec.conf", "w");
    fprintf(fp,"conn %%default\n  keyexchange=ikev1\n  authby=secret\n");

	for(prof_count = PROF_CLI; prof_count < PROF_ALL; prof_count++){
	    for(i = 0; i < MAX_PROF_NUM; i++){
			if(PROF_SVR == prof_count)
	        	sprintf(&buf[0], "ipsec_profile_%d", i+1);
			else if(PROF_CLI == prof_count)
				sprintf(&buf[0], "ipsec_profile_client_%d", i+1);
	        memset(s_tmp, '\0', SZ_BUF);
	        if(NULL != nvram_safe_get(&buf[0])){
	            strcpy(s_tmp, nvram_safe_get(&buf[0]));
	        }
	        if('\0' == *s_tmp){
	            memset((ipsec_prof_t *)&prof[prof_count][i], 0, sizeof(ipsec_prof_t));
	            prof[prof_count][i].ipsec_conn_en = IPSEC_CONN_EN_DEFAULT;
	            continue;
	        }
		        if(VPN_TYPE_NET_NET_SVR == prof[prof_count][i].vpn_type){
	            fprintf(fp,"#Net-to-Net VPN SVR[prof#%d]:%s\n\n", i, s_tmp);
		        }else if(VPN_TYPE_NET_NET_CLI == prof[prof_count][i].vpn_type){
	            fprintf(fp,"#Net-to-Net VPN CLI[prof#%d]:%s\n\n", i, s_tmp);
		        }else if(VPN_TYPE_NET_NET_PEER == prof[prof_count][i].vpn_type){
	            fprintf(fp,"#Net-to-Net PEER[prof#%d]:%s\n\n", i, s_tmp);
		        }else if(VPN_TYPE_HOST_NET == prof[prof_count][i].vpn_type){
	            fprintf(fp,"#Host-to-NET[prof#%d]:%s\n\n", i, s_tmp);
	        } else {
	            continue;
	        }
		        fprintf(fp,"\nconn %s\n", prof[prof_count][i].profilename);
		        if(VPN_TYPE_HOST_NET != prof[prof_count][i].vpn_type){
	            fprintf(fp,"##enforced UDP encapsulation (forceencaps=yes)\n"
		                       "  keyingtries=%d\n  type=%s\n", prof[prof_count][i].keyingtries
		                      , prof[prof_count][i].tun_type);
	        }
		        if(IKE_TYPE_AUTO == prof[prof_count][i].ike){
	            fprintf(fp,"  keyexchange=ikev1\n");
	        }else{
		            fprintf(fp,"  keyexchange=%s\n", ikev[prof[prof_count][i].ike]);
	        }
		        if(1 == prof[prof_count][i].exchange){
	            fprintf(fp,"  aggressive=yes\n");
	        }
		        ipsec_conf_local_set(fp, i, prof_count);
		        ipsec_conf_remote_set(fp, i, prof_count);
		        if(VPN_TYPE_HOST_NET != prof[prof_count][i].vpn_type){
		            ipsec_conf_phase1_set(fp, i, prof_count);
		            ipsec_conf_phase2_set(fp, i, prof_count);
	        }
	        fprintf(fp,"  auto=add\n");
	    }
	}
    if(NULL != fp){
        fclose(fp);
    }
    return;
}

void rc_ipsec_config_init()
{
    memset((ipsec_samba_t *)&samba_prof, 0, sizeof(ipsec_samba_t));
    memset((ipsec_prof_t *)&prof[0][0], 0, sizeof(ipsec_prof_t) * MAX_PROF_NUM);
    memset((pki_ca_t *)&ca_tab[0], 0, sizeof(pki_ca_t) * CA_FILES_MAX_NUM);
    system("cp -rf /usr/etc/* /tmp/etc/");
    mkdir("/jffs/ca_files", 0777);
    /*ipsec.conf init*/    
    rc_ipsec_conf_default_init();
    rc_ipsec_psk_xauth_rw_init();
    /*ipsec.secrets init*/
    rc_ipsec_set(IPSEC_INIT,PROF_ALL);
    //rc_ipsec_secrets_set();
    //rc_ipsec_conf_set();
    /*ipsec pki shell script default generate*/
    //rc_ipsec_ca_default_gen();
    //rc_ipsec_pki_gen_exec();
    rc_ipsec_ca_init();
    /*ca import*/
    //rc_ipsec_ca_import();
    return;
}

static int cur_bitmap_en_scan()
{
    uint32_t cur_bitmap_en = 0, i = 0,prof_count = 0;
	for(prof_count = PROF_CLI; prof_count < PROF_ALL; prof_count++){
	    for(i = 0; i < MAX_PROF_NUM; i++){
			if(IPSEC_CONN_EN_UP == prof[prof_count][i].ipsec_conn_en){
	        	cur_bitmap_en |= (uint32_t)(1 << i);
	        }
	    }
	}
    return cur_bitmap_en;
}

void rc_ipsec_set(ipsec_conn_status_t conn_status, ipsec_prof_type_t prof_type)
{
    static bool ipsec_start_en = FALSE;
    static int32_t pre_bitmap_en[2] = {0,0};
    uint32_t i = 0, cur_bitmap_en= 0;
	int prof_count = 0;
    FILE *fp = NULL;
    char interface[4];
	int ike_isakmp_port,ike_isakmp_nat_port;
	char *argv[3];

	char lan_class[32];
	ip2class(nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"), lan_class);

	argv[0] = "/bin/sh";
	argv[1] = FILE_PATH_IPSEC_SH;
	argv[2] = NULL;
	
    rc_ipsec_conf_set();
    rc_ipsec_secrets_set();
    rc_strongswan_conf_set();

    fp = fopen(FILE_PATH_IPSEC_SH, "w");
#if 0
    if(FALSE == ipsec_start_en){
        rc_ipsec_start(fp);
        ipsec_start_en = TRUE;
        if(IPSEC_INIT == conn_status){
            fprintf(fp, "\nsleep 7 > /dev/null 2>&1 \n");
        }
    }
#endif
    cur_bitmap_en = cur_bitmap_en_scan();
	if(FALSE == ipsec_start_en){
		rc_ipsec_restart(fp);
		ipsec_start_en = TRUE;
	}
	if(IPSEC_SET == conn_status){
		rc_ipsec_rereadall(fp);
		rc_ipsec_reload(fp);
	}
	
	for(prof_count = PROF_CLI; prof_count < PROF_ALL; prof_count++){
		//DBG(("rc_ipsec_down_stat>>>> 0x%x, 0x%x\n", pre_bitmap_en[0],pre_bitmap_en[1]));		
    	for(i = 0; i < MAX_PROF_NUM; i++){
			if(strcmp(prof[prof_count][i].local_public_interface,"wan") == NULL)
				strcpy(interface,get_wan_ifname(wan_primary_ifunit()));
			else if(strcmp(prof[prof_count][i].local_public_interface,"lan") == NULL) 
				strcpy(interface,"br0");

			ike_isakmp_port = prof[prof_count][i].ike_isakmp_port;
			ike_isakmp_nat_port = prof[prof_count][i].ike_isakmp_nat_port;
		
		if(((IPSEC_START == conn_status) || (IPSEC_INIT == conn_status)) &&  
	           (IPSEC_CONN_EN_UP == prof[prof_count][i].ipsec_conn_en)){

	            if(((uint32_t)(1 << i)) != (pre_bitmap_en[prof_count] & ((uint32_t)(1 << i)))){
                cur_bitmap_en = cur_bitmap_en_scan();
				
				fprintf(fp, "iptables -I INPUT -i %s -p udp --dport %d -j ACCEPT\n",interface,ike_isakmp_port);
				fprintf(fp, "iptables -I INPUT -i %s -p udp --dport %d -j ACCEPT\n",interface,ike_isakmp_nat_port);
				fprintf(fp, "iptables -I OUTPUT -o %s -p udp --sport %d -j ACCEPT\n",interface,ike_isakmp_port);
				fprintf(fp, "iptables -I OUTPUT -o %s -p udp --sport %d -j ACCEPT\n",interface,ike_isakmp_nat_port);
				fprintf(fp, "iptables -t nat -I POSTROUTING -s %s -m policy --dir out --pol ipsec -j ACCEPT\n", lan_class);

                rc_ipsec_up(fp, i, prof_count);
            }
        }else if((IPSEC_STOP == conn_status) &&
		         (IPSEC_CONN_EN_DOWN == prof[prof_count][i].ipsec_conn_en)){
				fprintf(fp, "iptables -D INPUT -i %s -p udp --dport %d -j ACCEPT\n",interface,ike_isakmp_port);
				fprintf(fp, "iptables -D INPUT -i %s -p udp --dport %d -j ACCEPT\n",interface,ike_isakmp_nat_port);
				fprintf(fp, "iptables -D OUTPUT -o %s -p udp --sport %d -j ACCEPT\n",interface,ike_isakmp_port);
				fprintf(fp, "iptables -D OUTPUT -o %s -p udp --sport %d -j ACCEPT\n",interface,ike_isakmp_nat_port);
				fprintf(fp, "iptables -t nat -D POSTROUTING -s %s -m policy --dir out --pol ipsec -j ACCEPT\n", lan_class);

				rc_ipsec_down(fp, i, prof_count);
			}
        }
	}

	if((IPSEC_STOP == conn_status) && (0 == cur_bitmap_en) &&
       (TRUE == ipsec_start_en)){
        ipsec_start_en = FALSE;
        rc_ipsec_stop(fp);
    }
    if(NULL != fp){
        fclose(fp);
        //DBG(("rc_ipsec_down_stat<<<<: 0x%x\n", cur_bitmap_en));
		chmod(FILE_PATH_IPSEC_SH, 0777);
		_eval(argv, NULL, 0, NULL);
       //DBG(("rc_ipsec_down_stat<<<<: 0x%x\n", cur_bitmap_en));
    }
    return;
}
