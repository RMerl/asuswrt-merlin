
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <getopt.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <net/if.h>
#include <bcmnvram.h>
#include <bcmparams.h>
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#include <rtstate.h>
#include <stdarg.h>
#include <time.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

#include <httpd.h>

// command
#define WRED		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/wred" : "wred"
#define AGENT		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/bwdpi-rule-agent" : "bwdpi-rule-agent"
#define QOSD		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/qosd" : "qosd"
#define DATACOLLD	nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/dc_monitor.sh" : "dc_monitor.sh"
#define WRED_SET	nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/wred_set_conf" : "wred_set_conf"

// conf / folder path
#define DATABASE		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/rule.trf" : "/tmp/bwdpi/rule.trf"
#define QOS_CONF		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/qosd.conf" : "/tmp/bwdpi/qosd.conf"
#define WRS_CONF		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/wred.conf" : "/tmp/bwdpi/wred.conf"
#define APP_SET_CONF		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/wrs_app.conf" : "/tmp/bwdpi/wrs_app.conf"
#define APP_CLEAN_CONF		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/wrs_app_clean.conf" : "/tmp/bwdpi/wrs_app_clean.conf"
#define TMP_BWDPI		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/" : "/tmp/bwdpi/"
#define TMP_BWDPI_DC		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/dc/" : "/tmp/bwdpi/dc/"
#define BWDPI_WAN		nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/dev_wan" : "/tmp/bwdpi/dev_wan"

// module
#define IDPKO	nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/IDP.ko" : "/usr/bwdpi/IDP.ko"
#define BWKO	nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/bw_forward.ko" : "/usr/bwdpi/bw_forward.ko"
#define CTKO	nvram_get_int("bwdpi_debug_path") ? "/jffs/TM/ct_notification.ko" : "/usr/bwdpi/ct_notification.ko"

// log / tmp file
#define TRAFFIC_PATH		"/tmp/traffic.log"
#define WRS_FULL_LOG		"/tmp/wrs_full.txt"
#define VP_FULL_LOG		"/tmp/vp_full.txt"
#define TMP_WRS_LOG		"/tmp/tmp_wrs.txt"
#define TMP_VP_LOG		"/tmp/tmp_vp.txt"
#define WRS_VP_LOG		"/jffs/wrs_vp.txt"
#define SIG_VER			"/proc/nk_policy"
#define DPI_VER			"/proc/ips_info"

typedef struct cat_id cid_s;
struct cat_id{
	int id;
	cid_s *next;
};

typedef struct wrs wrs_s;
struct wrs{
	int enabled;
	char mac[18];
	cid_s *ids;
	wrs_s *next;
};

typedef struct mac_list mac_s;
struct mac_list{
	char mac[18];
	mac_s *next;
};

typedef struct mac_group mac_g;
struct mac_group{
	char group_name[128];
	mac_s *macs;
};

typedef struct bwdpi_client bwdpi_device;
struct bwdpi_client{
	char hostname[32];
	char vendor_name[100];
	char type_name[100];
	char device_name[100];
};

typedef struct mail_info mail_s;
struct mail_info{
	int type;
	char rule[32];
	char mac[18];
	char hostname[100];
	char url[128];
	mail_s *next;
};

// define
#define MAC_OCTET_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_OCTET_EXPAND(o) \
	(uint8_t) o[0], (uint8_t) o[1], (uint8_t) o[2], (uint8_t) o[3], (uint8_t) o[4], (uint8_t) o[5]

#define IPV4_OCTET_FMT "%u.%u.%u.%u"
#define IPV4_OCTET_EXPAND(o) (uint8_t) o[0], (uint8_t) o[1], (uint8_t) o[2], (uint8_t) o[3]

#define IPV6_OCTET_FMT "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X"
#define IPV6_OCTET_EXPAND(o) \
	(uint8_t) o[0], (uint8_t) o[1], (uint8_t) o[2], (uint8_t) o[3], 	\
	(uint8_t) o[4], (uint8_t) o[5], (uint8_t) o[6], (uint8_t) o[7], 	\
	(uint8_t) o[8], (uint8_t) o[9], (uint8_t) o[10], (uint8_t) o[11], 	\
	(uint8_t) o[12], (uint8_t) o[13], (uint8_t) o[14], (uint8_t) o[15]

//iqos.c
extern char *dev_lan;
extern void check_qosd_wan_setting(char *dev_wan, int len);
extern void setup_qos_conf();
extern void stop_tm_qos();
extern void start_tm_qos();
extern int tm_qos_main(char *cmd);
extern void start_qosd();
extern void stop_qosd();
extern int qosd_main(char *cmd);
extern void run_signature_check();

//wrs.c
extern void setup_wrs_conf();
extern void stop_wrs();
extern void start_wrs();
extern int wrs_main(char *cmd);
extern int set_cc(char *cmd);
extern int set_vp(char *cmd);
extern void setup_vp_conf();
void free_id_list(cid_s **target_list);
cid_s *get_id_list(cid_s **target_list, char *target_string);
void print_id_list(cid_s *id_list);
void free_wrs_list(wrs_s **target_list);
extern wrs_s *get_all_wrs_list(wrs_s **wrs_list, char *setting_string);
void print_wrs_list(wrs_s *wrs_list);
wrs_s *match_enabled_wrs_list(wrs_s *wrs_list, wrs_s **target_list, int enabled);
void free_mac_list(mac_s **target_list);
mac_s *get_mac_list(mac_s **target_list, const char *target_string);
void print_mac_list(mac_s *mac_list);
void free_group_mac(mac_g **target);
mac_g *get_group_mac(mac_g **mac_group, const char *target);
void print_group_mac(mac_g *mac_group);

//stat.c
extern int stat_main(char *mode, char *name, char *dura, char *date);
extern void get_traffic_stat(char *mode, char *name, char *dura, char *date);
extern void get_traffic_hook(char *mode, char *name, char *dura, char *date, int *retval, webs_t wp);
extern void get_device_hook(char *MAC, int *retval, webs_t wp);
extern void get_device_stat(char *MAC);
extern int device_main(char *MAC);
extern int bwdpi_client_info(char *MAC, bwdpi_device *device);
extern int device_info_main(char *MAC);
extern int wrs_url_main();
extern void redirect_page_status(int cat_id, int *retval, webs_t wp);
extern int get_anomaly_main(char *cmd);
extern int get_app_patrol_main();

//dpi.c
extern void stop_dpi_engine_service(int forced);
extern void run_dpi_engine_service();
extern void start_dpi_engine_service();
extern void save_version_of_bwdpi();
extern void setup_dev_wan();

//web_history.c
extern int web_history_main(char *MAC, char *page);
extern void get_web_stat(char *MAC, char *page);
extern void get_web_hook(char *MAC, char *page, int *retval, webs_t wp);

//wrs_app.c
extern void setup_wrs_app_conf();
extern int wrs_app_main(char *cmd);
extern int wrs_app_service(int cmd);

//data_collect.c
extern void stop_dc();
extern void start_dc(char *path);
extern int data_collect_main(char *cmd, char *path);

//tools.c
extern int debug; // bwdpi_debug or wrs_debug
extern void check_filesize(char *path, long int size);
extern void StampToDate(unsigned long timestamp, char *date);
extern void rewrite_logfile(char *path1, char *path2, char *path3);
extern void get_hostname_from_NMP(char *mac, char *hostname);
extern void erase_symbol(char *old, char *sym);
extern void extract_data(char *path, FILE *fp);

//watchdog_check.c
extern void auto_sig_check();
extern void sqlite_db_check();
