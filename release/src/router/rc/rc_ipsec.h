#include <stdio.h>
#include <stdlib.h>

#define IPSEC_DEBUG 1

#define SZ_MAX    4096
#define SZ_TMP    512
#define SZ_BUF    256
#define SZ_128BUF 128
#define SZ_64BUF  64
#define SZ_4BUF   4
#define SZ_MIN    32

#define MAX_PROF_NUM 5

#define SZ_IP 4

#define CA_FILES_MAX_NUM 32

#define FILE_PATH_IPSEC_SH    "/tmp/etc/ipsec_exe.sh"
#define FILE_PATH_CA_CHECK_SH "/jffs/ca_files/awk.sh"
#define FILE_PATH_CA_GEN_SH   "/jffs/ca_files/pki_genkey.sh"
#define FILE_PATH_CA_ETC      "/jffs/ca_files/"
#define FILE_PATH_CA_PKI_TEMP "/jffs/ca_files/pki_tmp.txt"
#define FILE_PATH_CA_TEMP     "/jffs/ca_files/ca_idx_tmp.txt"

#define CA_MANUAL_GEN   1
#define CA_IMPORT       2
#define CA_AUTO_GEN     3
#define CERT_PRIVATEKEY 1
#define P12             2

typedef enum ipsec_prof_type_s{
    PROF_CLI = 0,/* Client*/
	PROF_SVR,	/* Server */
    PROF_ALL,
}ipsec_prof_type_t;


typedef enum ipsec_conn_status_s{
    IPSEC_SET,
    IPSEC_STOP,
    IPSEC_START,
    IPSEC_INIT, /*please do not remove it.*/
}ipsec_conn_status_t;

typedef enum ca_type_s{
    CA_TYPE_CACERT,
    CA_TYPE_CERT,
    CA_TYPE_PRIVATE_KEY,
    CA_TYPE_PKS12,
}ca_type_t;

typedef enum auth_method_s{
    AUTH_METHOD_RSA = 0,
    AUTH_METHOD_PSK,
}auth_method_t;

typedef enum vpn_type_s{
    VPN_TYPE_NET_NET_SVR = 1,
    VPN_TYPE_NET_NET_CLI,
    VPN_TYPE_NET_NET_PEER,
    VPN_TYPE_HOST_NET,
}vpn_type_t;

typedef enum flag_type_s{
    FLAG_IKE_ENCRYPT,
    FLAG_ESP_HASH,
    FLAG_KEY_CHAG_VER,
    FLAG_NONE,
}flag_type_t;

typedef enum ike_version_type_s{
    IKE_TYPE_AUTO,
    IKE_TYPE_V1,
    IKE_TYPE_V2,
    IKE_TYPE_MAX_NUM, /*do not remove it*/
}ike_version_type_t;

typedef enum encryption_type_s{
    ENCRYPTION_TYPE_DES,
    ENCRYPTION_TYPE_3DES,
    ENCRYPTION_TYPE_AES128,
    ENCRYPTION_TYPE_AES192,
    ENCRYPTION_TYPE_AES256,
    ENCRYPTION_TYPE_MAX_NUM, /*do not remove it*/
}encryption_type_t;   /*mapping to ike*/

typedef enum hash_type_s{
    HASH_TYPE_MD5,
    HASH_TYPE_SHA1,
    HASH_TYPE_SHA256,
    HASH_TYPE_SHA384,
    HASH_TYPE_SHA512,
    HASH_TYPE_MAX_NUM, /*don't remove it*/
}hash_type_t;        /*mapping to esp*/

typedef enum dh_group_type_s{
/*Regular Group*/
    DH_GROUP_1,
    DH_GROUP_2,
    DH_GROUP_5,
    DH_GROUP_14,
    DH_GROUP_15,
    DH_GROUP_16,
    DH_GROUP_17,
    DH_GROUP_18,
/*Modulo Prime Groups with Prime Order Subgroup*/
    DH_GROUP_22,
    DH_GROUP_23,
    DH_GROUP_24,
    DH_GROUP_MAX_NUM, /*don't remove it*/
}dh_group_type_t;

typedef enum ipsec_conn_en_s{
   IPSEC_CONN_EN_DOWN = 0,
   IPSEC_CONN_EN_UP,
   IPSEC_CONN_EN_DEFAULT,
}ipsec_conn_en_t;

typedef enum ipsec_auth2_type_s{
    IPSEC_AUTH2_TYP_DIS = 0,
    IPSEC_AUTH2_TYP_SVR,
    IPSEC_AUTH2_TYP_CLI,
}ipsec_auth2_type_t;

typedef struct ipsec_samba_s{
    int samba_en;
    char dns1[SZ_MIN];
    char dns2[SZ_MIN];
    char nbios1[SZ_MIN];
    char nbios2[SZ_MIN];
}ipsec_samba_t;

typedef struct ipsec_prof_s{
    uint8_t vpn_type;
    char profilename[SZ_MIN];
    char remote_gateway_method[SZ_MIN];
    char remote_gateway[SZ_MIN]; 
    char local_public_interface[SZ_MIN];
    char local_pub_ip[SZ_MIN];      /*local_public_ip*/
    uint8_t auth_method;
    char auth_method_key[SZ_MIN];           /*auth_menthod_value*/
    char local_subnet[SZ_MIN];
    uint16_t local_port; 
    char remote_subnet[SZ_MIN];
    uint16_t remote_port;
    char tun_type[SZ_MIN]; /*transport/tunnel type*/
    char virtual_ip_en[SZ_MIN]; /*virtual ip set or not*/
    char virtual_subnet[SZ_MIN];
    uint8_t accessible_networks;
    uint8_t ike; 
    uint8_t encryption_p1;
    uint8_t hash_p1;
    uint8_t exchange;
    char local_id[SZ_MIN];
    char remote_id[SZ_MIN];
    uint32_t keylife_p1;         /*IKE default:28800 seconds , 8hr*/
    uint8_t xauth;          /*0:disable,1:server,2:client*/
    char xauth_account[SZ_MIN];
    char xauth_password[SZ_MIN];
    char auth2meth[SZ_MIN];  /*auth2 methon -- xauth_server_type*/
    uint16_t traversal;
    uint16_t ike_isakmp_port;
    uint16_t ike_isakmp_nat_port;
    uint16_t ipsec_dpd;
    uint16_t dead_peer_detection;
    uint16_t encryption_p2;
    uint16_t hash_p2;
    uint32_t keylife_p2;           /*IPSEC default: 28800 seconds, 8hr*/
    uint16_t keyingtries;
    uint8_t ipsec_conn_en;  /*1: up ; 0:down*/
}ipsec_prof_t;


typedef struct pki_ca_s{
    char p12_pwd[SZ_MIN];
    char ca_txt[SZ_BUF];
    char ca_cert[SZ_BUF]; /*for server,client. e.g. CN=*/
    char san[SZ_MIN];
}pki_ca_t;

extern void rc_ipsec_config_init();
extern void rc_ipsec_topology_set();
extern void rc_ipsec_set(ipsec_conn_status_t conn_status, ipsec_prof_type_t prof_type);

extern void rc_ipsec_ca_import();
extern void rc_ipsec_cert_import(char *asus_cert, char *ipsec_cli_cert,
                                 char *ipsec_cli_key, char *pks12);
extern void rc_ipsec_ca_export(char *verify_pwd);
extern int rc_ipsec_ca_gen();
extern void rc_ipsec_pki_gen_exec(uint32_t idx);
extern int rc_ipsec_ca_txt_parse();
