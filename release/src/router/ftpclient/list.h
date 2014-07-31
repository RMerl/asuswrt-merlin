#ifndef __LIST_H
#define __LIST_H

#define MAX_LENGTH 128
#define MIN_LENGTH 32
#define MAX_CONTENT 256
#define MINSIZE 64

#define MYPORT 3568
#define INOTIFY_PORT 3678
#define MAXDATASIZE 1024
#define BACKLOG 100 /* max listen num*/
#define OUTLEN 256

#define PROGRESSBAR 1
#define FIX 0
#define TELLENC 1

#define S_INITIAL		70
#define S_SYNC			71
#define S_DOWNUP		72
#define S_UPLOAD		73
#define S_DOWNLOAD		74
#define S_STOP			75
#define S_ERROR			76
#define LOG_SIZE                sizeof(struct LOG_STRUCT)

#define LOCAL_SPACE_NOT_ENOUGH          900
#define SERVER_SPACE_NOT_ENOUGH         901
#define LOCAL_FILE_LOST                 902
#define SERVER_FILE_DELETED             903
#define COULD_NOT_CONNECNT_TO_SERVER    904
#define CONNECNTION_TIMED_OUT           905
#define INVALID_ARGUMENT                906
#define COULD_NOT_READ_RESPONSE_BODY    908
#define HAVE_LOCAL_SOCKET               909
#define SERVER_ROOT_DELETED             910
#define PERMISSION_DENIED               911
#define UNSUPPORT_ENCODING              912

//#define SHELL_FILE  "/tmp/write_nvram"
//#define CONFIG_PATH "/tmp/Cloud/ftp.conf"

//#ifndef NVRAM_
//#define TOKENFILE_RECORD "/opt/etc/ftp_tokenfile"
//#endif

//#define LIST_DIR "/tmp/ftpclient/list"
//#define LIST_ONE_DIR "/tmp/ftpclient/list_one.txt"
//#define LIST_SIZE_DIR "/tmp/ftpclient/list_size.txt"
//#define TIME_DIR "/tmp/ftpclient/time.txt"
//#define TIME_ONE_DIR "/tmp/ftpclient/time_one.txt"
//#define LOG_DIR "/tmp/ftpclient/log/log.txt"

#ifndef NVRAM_
#define TOKENFILE_RECORD "/opt/etc/.smartsync/ftp_tokenfile"
#endif
#define NOTIFY_PATH "/tmp/notify/usb"

#define SHELL_FILE    "/tmp/smartsync/ftpclient/script/write_nvram"
#define CONFIG_PATH   "/tmp/smartsync/ftpclient/config/ftp.conf"
#define TMP_CONFIG    "/tmp/smartsync/ftpclient/config/ftp_tmpconfig"
#define GET_NVRAM     "/tmp/smartsync/ftpclient/script/ftpclient_get_nvram"
#define GET_INTERNET  "/tmp/smartsync/ftpclient/script/ftpclient_get_internet"
#define VAL_INTERNET  "/tmp/smartsync/ftpclient/config/ftp_val_internet"
#define LIST_DIR      "/tmp/smartsync/ftpclient/temp/list"
#define LIST_ONE_DIR  "/tmp/smartsync/ftpclient/temp/list_one.txt"
#define LIST_SIZE_DIR "/tmp/smartsync/ftpclient/temp/list_size.txt"
#define TIME_DIR      "/tmp/smartsync/ftpclient/temp/time.txt"
#define TIME_ONE_DIR  "/tmp/smartsync/ftpclient/temp/time_one.txt"
#define CURL_HEAD     "/tmp/smartsync/ftpclient/temp/headdata"
#define LOG_DIR       "/tmp/smartsync/.logs/ftpclient"
#define CONFLICT_DIR  "/tmp/smartsync/.logs/ftpclient_errlog"

//#define SERVERLIST_XML "/tmp/smartsync/ftpclient/temp/serverlist.xml"
//#define SERVERLIST_TD  "/tmp/smartsync/ftpclient/temp/serverlist.td"
//#define SERVERLIST_LOG "/tmp/smartsync/ftpclient/temp/serverlist.log"
//#define SERVERLIST_0   "/tmp/smartsync/ftpclient/temp/serverlist_0"
//#define SERVERLIST_1   "/tmp/smartsync/ftpclient/temp/serverlist_1"
//#define SERVERLIST_2   "/tmp/smartsync/ftpclient/temp/serverlist_2"

#define SPACE " "
#define ENTER "\n"

#define PLAN_B 0

#endif // LIST_H
