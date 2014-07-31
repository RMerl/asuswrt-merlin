#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include "list.h"
#include "function.h"
#include "cJSON.h"
#include <sys/statvfs.h>
#include <utime.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

//#define NVRAM_ 1

#define CMD_SPLIT "\n"

#define __DEBUG__
#ifdef __DEBUG__
//#define wd_DEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"/n", __LINE__, ##__VA_ARGS__)
#define wd_DEBUG(format,...) printf(format, ##__VA_ARGS__)
#define CURL_DEBUG
//#define CURL_DEBUG curl_easy_setopt(curl,CURLOPT_VERBOSE,1)
#else
#define wd_DEBUG(format,...)
#define CURL_DEBUG
#endif

typedef cJSON *(*proc_pt)(char *filename);

#ifdef NVRAM_
    #ifndef USE_TCAPI
        #include <bcmnvram.h>
    #else
        #define WANDUCK "Wanduck_Common"
        #define AICLOUD "AiCloud_Entry"
        #include "libtcapi.h"
        #include "tcapi.h"
    #endif
#include <shutils.h>
#define SHELL_FILE  "/tmp/smartsync/script/write_nvram"
#else
    #define TMPCONFIG "/tmp/smartsync/dropbox/config/dropbox_tmpconfig"
    #define DropBox_Get_Nvram "/tmp/smartsync/dropbox/script/dropbox_get_nvram"
    #define TMP_NVRAM_VL "/tmp/smartsync/dropbox/temp/dropbox_tmp_nvram_vl"
    #define DropBox_Get_Nvram_Link "/tmp/smartsync/dropbox/script/dropbox_get_nvram_link"
#endif

#define HAVE_LOCAL_SOCKET               909
#define NOTIFY_PATH "/tmp/notify/usb"

//#define TEST 1
#define RENAME_F
#define TOKENFILE 1



pthread_t newthid1,newthid2,newthid3;
pthread_cond_t cond,cond_socket,cond_log;
pthread_mutex_t mutex,mutex_socket,mutex_receve_socket,mutex_log;
/*
 mutex=>server pthread
 mutex_socket=>socket save pthread
 mutex_receve_socket=>socket parse pthread
*/

/* log struct */
char log_path[MAX_LENGTH];
char general_log[MAX_CONTENT];
char trans_excep_file[MAX_LENGTH];

typedef struct LOG_STRUCT{
    uint8_t  status;
    char  error[512];
    float  progress;
    char path[512];
}Log_struc;


/*stat local file*/
typedef struct Local_Struct
{
    char *path;
    int index;
}Local_Str;

Local_Str LOCAL_FILE;

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
#define PARSE_XML_FAILED                907
#define COULD_NOT_READ_RESPONSE_BODY    908
#define HAVE_LOCAL_SOCKET               909
#define SERVER_ROOT_DELETED             910

//define temporary file
#define TMP_R "/tmp/smartsync/dropbox/temp/"
#define Con(TMP_R,b) TMP_R#b

/*#####server space######*/
long long int server_shared;
long long int server_quota;
long long int server_normal;
/*######Global var*/

int local_sync;
int server_sync;
int finished_initial;

typedef struct Action_Item
{
    char *action;
    char *path;
    struct Action_Item *next;
} action_item;

typedef struct Upload_chunk
{
    char *filename;
    char *upload_id;
    unsigned long chunk;
    unsigned long offset;
    time_t expires;
//    struct Upload_chunk *next;
} Upload_chunked;

Upload_chunked *upload_chunk;

/*#####socket list*/
struct queue_entry
{
    struct queue_entry * next_ptr;   /* Pointer to next entry */
    //int type;
    //char filename[256];
    char cmd_name[1024];
    char *re_cmd;
    int is_first;
    //int id;
    //long long int size;
};
typedef struct queue_entry * queue_entry_t;

/*struct queue*/
struct queue_struct
{
    struct queue_entry *head;
    struct queue_entry *tail;
};
typedef struct queue_struct *queue_t;

queue_entry_t SocketActionTmp;

#ifdef OAuth1
/*####auth*/
typedef struct Authentication
{
    char tmp_oauth_token_secret[32];
    char tmp_oauth_token[32];
    char oauth_token_secret[32];
    char oauth_token[32];
    int uid;
}Auth;

Auth *auth;
#else
typedef struct Authentication
{
    char *oauth_token;
}Auth;

Auth *auth;

#endif
typedef struct Account_info
{
    char usr[256];
    char pwd[256];
}Info;

Info *info;


#ifdef PC
#define CONFIG_PATH "/WorkSpace_pc/dropbox-test/dropbox.conf"
#else
#define CONFIG_PATH "/tmp/smartsync/dropbox/config/dropbox.conf"
#endif
/*muti dir read config*/
struct asus_rule
{
    int rule;
    char path[512];//fullname name
    char rooturl[512]; //sync folder
    char base_path[512];     //base_path is the mount path
    int base_path_len;
    int rooturl_len;
};

struct asus_config
{
    int type;
    char user[256];
    char pwd[256];
    int  enable;
    int ismuti;
    int dir_number;
    struct asus_rule **prule;
};

struct asus_config asus_cfg,asus_cfg_stop;

typedef struct SYNC_LIST{

    char conflict_file[256];
    char temp_path[MAX_LENGTH];

    int sync_disk_exist;
    int have_local_socket;
    int no_local_root;
    int init_completed;
    int receve_socket;
    int first_sync;
    action_item *download_only_socket_head;
    action_item *server_action_list;    //Server变化导致的Socket
    action_item *copy_file_list;         //The copy files
    action_item *unfinished_list;
    action_item *up_space_not_enough_list;
    action_item *access_failed_list;
    queue_t SocketActionList;
    Server_TreeNode *ServerRootNode;
    Server_TreeNode *OldServerRootNode;
    Server_TreeNode *VeryOldServerRootNode;
}sync_list;
sync_list **g_pSyncList;



char *search_newpath(char *href,int index);
Browse *browseFolder(char *URL);
#ifdef OAuth1
char *makeAuthorize(int flag);
#else
char *makeAuthorize();
#endif
int get_request_token();
int parse(char *filename,int flag);
char *parse_login_page();
int login(void);
int cgi_init(char *query);
int api_download(char *fullname,char *filename,int index);
int sync_local_with_server_init(Server_TreeNode *treenode,Browse *perform_br,Local *perform_lo,int index);
int the_same_name_compare(LocalFile *localfiletmp,CloudFile *filetmp,int index,int is_init);
char *parse_name_from_path(const char *path);
int add_socket_item(char *buf);
char *get_socket_base_path(char *cmd);
void queue_enqueue (queue_entry_t d, queue_t q);
queue_t queue_create ();
void *Socket_Parser();
int cmd_parser(char *cmd,int index,char *re_cmd);
int is_server_exist(char *path,char *temp_name,int index);
int get_path_to_index(char *path);
int dragfolder(char *dir,int index);
int dragfolder_rename(char *dir,int index,time_t server_mtime);
queue_entry_t  queue_dequeue (queue_t q);
queue_entry_t  queue_dequeue_t (queue_t q);
int del_action_item(const char *action,const char *path,action_item *head);
action_item *get_action_item(const char *action,const char *path,action_item *head,int index);
int add_action_item(const char *action,const char *path,action_item *head);
void del_download_only_action_item(const char *action,const char *path,action_item *head);
void free_action_item(action_item *head);
action_item *create_action_item_head();
int download_only_add_socket_item(char *cmd,int index);
int add_all_download_only_socket_list(char *cmd,const char *dir,int index);
int wait_handle_socket(int index);
int get_create_threads_state();
void *SyncServer();
int compareServerList(int index);
int isServerChanged(Server_TreeNode *newNode,Server_TreeNode *oldNode);
int Server_sync(int index);
void del_all_items(char *dir,int index);
void cjson_to_space(cJSON *q);
int test_if_download_temp_file(char *filename);
int do_unfinished(int index);
int my_progress_func(char *clientfp,double t,double d,double ultotal,double ulnow);
size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
void clean_up();
void queue_destroy (queue_t q);
int write_log(int status, char *message, char *filename,int index);
char *change_local_same_file(char *oldname,int index);
void replace_char_in_str(char *str,char newchar,char oldchar);
action_item *get_action_item_access(const char *action,const char *path,action_item *head,int index);
action_item *check_action_item(const char *action,const char *oldpath,action_item *head,int index,const char *newpath);
void buffer_free(char *b);
int write_conflict_log(char *prename, char *conflict_name,int index);
int write_trans_excep_log(char *fullname,int type,char *msg);
int sync_initial_again(int index);

int api_metadata_test_dir(char *phref,proc_pt cmd_data);
int cJSON_printf_dir(cJSON *json);
char *get_server_exist(char *temp_name,int index);
void updata_socket_list(char *temp_name,char *new_name,int i);

#if TOKENFILE
int disk_change;
int sync_disk_removed;
int sighandler_finished;
#define TOKENFILE_RECORD "/opt/etc/.smartsync/db_tokenfile"
struct mounts_info_tag
{
    int num;
    char **paths;
};
struct tokenfile_info_tag
{
    char *folder;
    char *url;
    char *mountpath;
    struct tokenfile_info_tag *next;
};
struct tokenfile_info_tag *tokenfile_info,*tokenfile_info_start,*tokenfile_info_tmp;
int get_mounts_info(struct mounts_info_tag *info);
int write_notify_file(char *path,int signal_num);
int get_tokenfile_info();
struct tokenfile_info_tag *initial_tokenfile_info_data(struct tokenfile_info_tag **token);
int check_config_path(int is_read_config);
int write_to_tokenfile(char *mpath);
int write_to_wd_tokenfile(char *contents);
char *write_error_message(char *format,...);
#ifdef NVRAM_
int create_shell_file();
int convert_nvram_to_file_mutidir(char *file,struct asus_config *config);
int detect_process(char * process_name);
#else
int create_webdav_conf_file(struct asus_config *config);
int write_get_nvram_script();
int write_get_nvram_script_va(char *str);
#endif
int rewrite_tokenfile_and_nv();
int delete_tokenfile_info(char *url,char *folder);
char *delete_nvram_contents(char *url,char *folder);
int add_tokenfile_info(char *url,char *folder,char *mpath);
char *add_nvram_contents(char *url,char *folder);
int check_disk_change();
int check_sync_disk_removed();
int free_tokenfile_info(struct tokenfile_info_tag *head);
#endif
