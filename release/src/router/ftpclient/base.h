#ifndef __BASE_H
#define __BASE_H
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <string>
#include <dirent.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <utime.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <iconv.h>
#include <cstring>
#include <cstdlib>
#include <stdarg.h>
#include "list.h"


using namespace std;

#ifdef NVRAM_
extern "C"
{
    #ifdef USE_TCAPI
        #define WANDUCK "Wanduck_Command"
        #define AICLOUD "AiCloud_Entry"
        #include "libtcapi.h"
        #include "tcapi.h"
    #else
        //#include <bcmnvram.h>
        extern char * nvram_get(const char *name);
        #define nvram_safe_get(name) (nvram_get(name) ? : "")
    #endif
        #include <shutils.h>
}
#endif

#define __DEBUG__
#ifdef __DEBUG__
#define DEBUG(info, ...) printf(info,##__VA_ARGS__)
#else
#define DEBUG(info, ...)
#endif

#define __DEBUG1__
#ifdef __DEBUG1__
#define DEBUG1(info, ...) printf(info,##__VA_ARGS__)
#else
#define DEBUG1(info, ...)
#endif

#define __FTPDEBUG__

#ifdef __FTPDEBUG__
#define _show(info, ...) printf(info,##__VA_ARGS__)
#else
#define _show(info, ...)
#endif

/* log struct */
typedef struct LOG_STRUCT
{
    uint8_t  status;
    char  error[512];
    float  progress;
    char path[512];
}Log_struc;

typedef struct Mult_Rule
{
    int rules;
    char base_path[256];
    int base_path_len;
    char base_path_parentref[256];
    int base_path_parentref_len;
    char rooturl[256];
    int rooturl_len;
    char fullrooturl[256];
    char mount_path[256];
    char base_folder[256];
    int encoding;
    char f_usr[32];
    char f_pwd[32];
    char user_pwd[64];
    char server_ip[256];
    int server_ip_len;
    char hardware[256];
}MultRule;
typedef struct _Config
{
    int id;
    int dir_num;
    MultRule **multrule;
}Config;

typedef struct _Node
{
    char *cmdName;
    //char *re_cmd;
    struct _Node *next;
}Node;
//Node *newNode;

typedef struct Action_Item
{
    char *action;
    char *path;
    struct Action_Item *next;
} action_item;

typedef struct node{
    int isfile;
    char * href;
    //char auth[MIN_LENGTH];
    long long int size;
    //char month[MIN_LENGTH];
    //char day[MIN_LENGTH];
    //char lastmodifytime[MINSIZE];
    char filename[MAX_CONTENT];
    //char mtime[MIN_LENGTH];
    unsigned long int modtime;
    struct node *next;
    int ismodify;
}CloudFile;

typedef struct BROWSE
{
    int foldernumber;
    int filenumber;
    CloudFile *folderlist;
    CloudFile *filelist;
}Browse;

typedef struct ServerTreeNode
{
    int level;
    char *parenthref;
    Browse *browse;
    struct ServerTreeNode *Child;
    struct ServerTreeNode *NextBrother;
}Server_TreeNode;
//Server_TreeNode *ServerNode_del,*ServerNode_List;

typedef struct SYNC_LIST
{
    char conflict_item[256];
    char up_item_file[256];
    char temp_path[MAX_LENGTH];
    char down_item_file[256];
    int index;
    int sync_up;
    int sync_down;
    int download_only;
    int upload_only;
    int first_sync;
    int receve_socket;
    int have_local_socket;
    int no_local_root;
    int init_completed;
    int sync_disk_exist;
    int upload_break;
    int IsNetWorkUnlink;    //2014.11.03 by sherry (ip是否正确，网络通不通)
    int IsPermission;       //2014.11.04 by sherry (server权限)
    action_item *copy_file_list;         //The copy files
    action_item *server_action_list;    //Server变化导致的Socket
    action_item *dragfolder_action_list;   //dragfolder时递归动作造成的Socket重复
    action_item *unfinished_list;
    action_item *download_only_socket_head;
    action_item *up_space_not_enough_list;
    Node *SocketActionList;
    Node *SocketActionList_Rename;
    Node *SocketActionList_Refresh;
    Server_TreeNode *ServerRootNode;
    Server_TreeNode *OldServerRootNode;
    Server_TreeNode *OrigServerRootNode;
}sync_list;
//sync_list **g_pSyncList;

typedef struct LOCALFOLDER{
    char *path;
    char name[MAX_CONTENT];
    struct LOCALFOLDER *next;
}LocalFolder;

typedef struct LOCALFILE{
    char *path;
    char name[MAX_CONTENT];
    //char creationtime[MINSIZE];
    //char lastaccesstime[MINSIZE];
    //char lastwritetime[MINSIZE];
    unsigned long modtime;
    //long long int size;
    struct LOCALFILE *next;
}LocalFile;

typedef struct LOCAL
{
    int foldernumber;
    int filenumber;
    LocalFolder *folderlist;
    LocalFile *filelist;
}Local;

typedef struct MODTIME
{
    char mtime[MINSIZE];
    unsigned long modtime;
}mod_time;

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

typedef struct file_info
{
    char *path;
    int index;
}_info;

struct FtpFile {
  const char *filename;
  FILE *stream;
};

int code_convert(char *from_charset,char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen);
char *to_utf8(char *buf,int index);
char *utf8_to(char *buf,int index);
size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
int my_progress_func(char *progress_data,double t, double d,double ultotal,double ulnow);
int download_(char *serverpath,int index);
int upload(char *localpath1,int index);
int my_delete_DELE(char *localpath,int index);
int my_delete_RMD(char *localpath,int index);
int my_rename_(char *localpath,char *newername,int index);
int my_mkdir_(char *localpath,int index);
int moveFolder(char *old_dir,char *new_dir,int index);
int createFolder(char *dir,int index);
void SearchServerTree_(Server_TreeNode* treeRoot,int index);
int Delete(char *localpath,int index);
int write_log(int status, char *message, char *filename,int index);
void clear_global_var();
int wait_handle_socket(int index);
mod_time *get_mtime_1(FILE *fp);
mod_time *Getmodtime(char *href,int index);
int ChangeFile_modtime(char *filepath,time_t servermodtime,int index);
int parseCloudInfo_one(char *parentherf,int index);
int getCloudInfo_one(char *URL,int (* cmd_data)(char *,int),int index);
int is_server_exist(char *parentpath,char *filepath,int index);
void parseCloudInfo_forsize(char *parentherf);
int getCloudInfo_forsize(char *URL,void (* cmd_data)(char *),int index);
long long int get_file_size(char *serverpath,int index);
int is_ftp_file_copying(char *serverhref,int index);
void *Save_Socket(void *argc);
int getCloudInfo(char *URL,void (* cmd_data)(char *,int),int index);
int ftp_zone();
time_t change_time_to_sec(char *time);
mod_time *get_mtime(FILE *fp);
mod_time *ftp_MDTM(char *href,int index);
void parserHW(Config config);
int parseCloudInfo_tree(char *parentherf,int index);
Browse *browseFolder(char *URL,int index);
int browse_to_tree(char *parenthref,Server_TreeNode *node,int index);
Local *Find_Floor_Dir(const char *path);
int the_same_name_compare(LocalFile *localfiletmp,CloudFile *filetmp,int index);
int sync_server_to_local_perform(Browse *perform_br,Local *perform_lo,int index);
int sync_server_to_local(Server_TreeNode *treenode,int (*sync_fuc)(Browse*,Local*,int),int index);
int compareLocalList(int index);
int isServerChanged(Server_TreeNode *newNode,Server_TreeNode *oldNode);
int compareServerList(int index);
void *SyncServer(void *argc);
char *change_server_same_name(char *fullname,int index);
char *change_local_same_name(char *fullname);
int cmd_parser(char *cmd,int index);
int download_only_add_socket_item(char *cmd,int index);
void *Socket_Parser(void *argc);
int send_action(int type, char *content,int port);
void send_to_inotify();
void read_config();
void init_global_var();
int initMyLocalFolder(Server_TreeNode *servertreenode,int index);
int sync_local_to_server_init(Local *perform_lo,int index);
int sync_local_to_server(char *path,int index);
int sync_server_to_local_init(Browse *perform_br,Local *perform_lo,int index);
int initialization();
void run();
void my_local_mkdir(char *path);
void* sigmgr_thread(void *argc);
int verify(Config config);
char *my_str_malloc(size_t len);
char *my_nstrchr(const char chr,char *str,int n);
long long int get_local_freespace(int index);
char *get_socket_base_path(char *cmd);
char *get_prepath(char *temp,int len);
action_item *get_action_item(const char *action,const char *path,action_item *head,int index);
int get_path_to_index(char *path);
action_item *create_action_item_head();
Server_TreeNode *create_server_treeroot();
int add_action_item(const char *action,const char *path,action_item *head);
int add_all_download_only_dragfolder_socket_list(const char *dir,int index);
int add_socket_item(char *buf,int i);
int add_all_download_only_socket_list(char *cmd,const char *dir,int index);
int del_action_item(const char *action,const char *path,action_item *head);
void del_download_only_action_item(const char *action,const char *path,action_item *head);
void del_all_items(char *dir,int index);
void show(Node* head);
void show_item(action_item* head);
int test_if_download_temp_file(char *filename);
int test_if_dir(const char *dir);
int test_if_dir_empty(char *dir);
Node *queue_dequeue (Node *q);
void replace_char_in_str(char *str,char newchar,char oldchar);
void SearchServerTree(Server_TreeNode* treeRoot);
char *localpath_to_serverpath(char *from_localpath,int index);
char *serverpath_to_localpath(char *from_serverpath,int index);
int is_file(char *p);
int is_folder(char *p);
int is_local_space_enough(CloudFile *do_file,int index);
int is_server_space_enough(char *localfilepath,int index);//2014.11.14 by sherry add：server是否足够
//int get_server_info(char *URL,int (* cmd_data)(char *,int),int index);//2014.11.14 by sherry add：server是否足够
int get_server_info(char *URL,int index);
int is_server_have_localpath(char *path,Server_TreeNode *treenode,int index);
void free_action_item(action_item *head);
void free_CloudFile_item(CloudFile *head);
void free_server_tree(Server_TreeNode *node);
void free_LocalFolder_item(LocalFolder *head);
void free_LocalFile_item(LocalFile *head);
void free_localfloor_node(Local *local);
void init_utf8_char_table();
const char* tellenc(const char* const buffer, const size_t len);
int get_mounts_info(struct mounts_info_tag *info);
int get_tokenfile_info();
int check_config_path(int is_read_config);
struct tokenfile_info_tag *initial_tokenfile_info_data(struct tokenfile_info_tag **token);
int write_to_nvram(char *contents,char *nv_name);
int write_to_ftp_tokenfile(char *contents);
int delete_tokenfile_info(char *url,char *folder);
char *delete_nvram_contents(char *url,char *folder);
int write_to_tokenfile(char *mpath);
int add_tokenfile_info(char *url,char *folder,char *mpath);
char *add_nvram_contents(char *url,char *folder);
int rewrite_tokenfile_and_nv();
void sig_handler (int signum);
int write_notify_file(char *path,int signal_num);
int detect_process(char * process_name);
int create_shell_file();
int write_get_nvram_script(char *name,char *shell_dir,char *val_dir);
int create_ftp_conf_file(Config *config);
int free_tokenfile_info(struct tokenfile_info_tag *head);
int check_disk_change();
int check_sync_disk_removed();
CloudFile *get_CloudFile_node(Server_TreeNode* treeRoot,const char *dofile_href,int a);
int do_unfinished(int index);
int deal_dragfolder_to_socketlist(char *dir,int index);
int check_serverlist_modify(int index,Server_TreeNode *newnode);
int parse_config(const char *path,Config *config);
Server_TreeNode * getoldnode(Server_TreeNode *tempoldnode,char *href);
int get_prefix(char *temp,int len);
int is_server_modify(Server_TreeNode *newNode,Server_TreeNode *oldNode);
char *search_newpath(char *href,int i);
char *oauth_url_unescape(const char *string, size_t *olen);
char *oauth_url_escape(const char *string);
static void *xmalloc_fatal(size_t size);
void *xmalloc (size_t size);
void *xrealloc (void *ptr, size_t size);
char *xstrdup (const char *s);
int convert_nvram_to_file_mutidir(char *file,Config *config);
int convert_nvram_to_file_usbinfo(char *file);
int webdav_config_usbinfo_exist();
int convert_nvram_to_file(char *file);
int debugFun(CURL *curl,curl_infotype type,char *str,size_t len,void *stream);
void my_mkdir_r(char *path,int index);
int create_xml(int status);
int socket_check(char *dir,char *name,int index);
int usr_auth(char *ip,char *user_pwd);
//void create_start_file();
//int detect_process_file();
#endif  //BASE_H
