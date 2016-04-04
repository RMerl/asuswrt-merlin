#ifndef __SMB_H
#define __SMB_H
#include "Data.h"

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

#ifndef NVRAM_
#define TOKENFILE_RECORD "/opt/etc/.smartsync/usb_tokenfile"
#endif
#define NOTIFY_PATH "/tmp/notify/usb"

#define SHELL_FILE    "/tmp/smartsync/usbclient/script/write_nvram"
#define GET_NVRAM     "/tmp/smartsync/usbclient/script/usbclient_get_nvram"
#define TMP_CONFIG    "/tmp/smartsync/usbclient/config/usb_tmpconfig"
#define CONFIG_PATH   "/tmp/smartsync/usbclient/config/usbclient.conf"
#define GET_INTERNET  "/tmp/smartsync/usbclient/script/usbclient_get_internet"
#define VAL_INTERNET  "/tmp/smartsync/usbclient/config/usb_val_internet"
#define LOG_DIR       "/tmp/smartsync/.logs/usbclient"
#define CONFLICT_DIR  "/tmp/smartsync/.logs/usbclient_errlog"

char *my_malloc(size_t len);
action_item *create_action_item_head();
action_item *get_action_item(const char *action, const char *path, action_item *head, int index);
Server_TreeNode *create_server_treeroot();
int test_if_dir_empty(char *dir);
char *serverpath_to_localpath(char *serverpath, int index);
char *localpath_to_serverpath(char *localpath, int index);
void free_server_tree(Server_TreeNode *node);
void free_CloudFile_item(CloudFile *head);
static void auth_fn(const char *server, const char *share, char *workgroup, int wgmaxlen,
                    char *username, int unmaxlen, char *password, int pwmaxlen);
int SMB_init(int index);
int is_local_space_enough(CloudFile *do_file, int index);
int add_action_item(const char *action, const char *path,action_item *head);
int SMB_download(char *serverpath, int index);
int USB_download(char *serverpath, int index);
int SMB_upload(char *localpath, int index);
int USB_upload(char *localpath, int index);
int SMB_remo(char *localoldpath, char *localnewpath, int index);
int USB_remo(char *localoldpath, char *localnewpath, int index);
int SMB_rm(char *localpath, int index);
int USB_rm(char *localpath, int index);
int SMB_mkdir(char *localpath, int index);
int USB_mkdir(char *localpath, int index);
int test_if_download_temp_file(char *filename);
Local *Find_Floor_Dir(const char *path);
int test_if_dir(const char *dir);
void free_LocalFolder_item(LocalFolder *head);
void free_LocalFile_item(LocalFile *head);
void free_localfloor_node(Local *local);
int is_server_have_localpath(char *path, Server_TreeNode *treenode, int index);
int del_action_item(const char *action, const char *path, action_item *head);
void local_mkdir(char *path);
void free_action_item(action_item *head);
time_t Getmodtime(char *url, int index);
int ChangeFile_modtime(char *localfilepath, time_t modtime, int index);
char *change_same_name(char *localpath, int index, int flag);
Node *queue_dequeue (Node *q);
int get_path_to_index(char *path);
void del_download_only_action_item(const char *action, const char *path, action_item *head);
int add_all_download_only_dragfolder_socket_list(const char *dir, int index);
void del_all_items(char *dir, int index);
CloudFile *get_CloudFile_node(Server_TreeNode* treeRoot, const char *dofile_href, int a,int index);
char *write_error_message(char *format,...);
int write_conflict_log(char *fullname, int type, char *msg);
char *change_same_name(char *localpath, int index, int flag);
char *get_socket_base_path(char *cmd);
char *my_nstrchr(const char chr, char *str, int n);
int detect_process(char *process_name);
int create_shell_file();
void clear_global_var();
int write_notify_file(char *path, int signal_num);
void stop_process_clean_up();
int getCloudInfo(char *URL, int index);
int socket_check(char *dir,char *name,int index);
int moveFolder(char *old_dir, char *new_dir, int index);
int is_server_exist(char *localpath, int index);
void replace_char_in_str(char *str,char newchar,char oldchar);
long long stat_file(const char *filename);
void my_mkdir_r(char *path,int index);
int write_log(int status, char *message, char *filename, int index);
char *reduce_memory(char *URL, int index, int flag);

#ifdef NVRAM_
int write_to_nvram(char *contents, char *nv_name);
#else
int write_get_nvram_script(char *name, char *shell_dir, char *val_dir);
int write_to_smb_tokenfile(char *contents);
#endif

int rewrite_tokenfile_and_nv();

struct tokenfile_info_tag *initial_tokenfile_info_data(struct tokenfile_info_tag **token);
int get_tokenfile_info();
int check_config_path(int is_read_config);

#endif //SMB_H
















