/* Name:SambaClient
   Nvram Port : 4
   Client Port : 3571
   Inotify Port : 3678
   Nvram Struct:
        4>WORKGROUP>smb://192.168.1.9>zxcvbnmmmmmm>mars>123456>0>/tmp/mnt/ANDI/syncFolder
        |       |             |               |      |_____|   |            |
        |       |             |               |         |      |            |
        ID   workgroup        IP         shareFolder   auth   rule     localFolder

*/

#ifndef __DATA_H
#define __DATA_H
#include <stdlib.h>

#ifdef NVRAM_
#ifdef USE_TCAPI
        #define WANDUCK "Wanduck_Command"
        #define AICLOUD "AiCloud_Entry"
        #include "libtcapi.h"
        #include "tcapi.h"
#else
        #include <bcmnvram.h>
#endif
        #include <shutils.h>
#endif

#define MAX_BUFF_SIZE   255

#define FILE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define DIR_MODE  S_IRWXU | S_IRWXG | S_IRWXO

//#define __DEBUG__
#ifdef __DEBUG__
#define DEBUG(info, ...) printf(info,##__VA_ARGS__)
#else
#define DEBUG(info, ...)
#endif

#define S_INITIAL		70
#define S_SYNC			71
#define S_DOWNUP		72
#define S_UPLOAD		73
#define S_DOWNLOAD		74
#define S_STOP			75
#define S_ERROR			76

#define NV_PORT 5
#define MYPORT 3572
#define INOTIFY_PORT 3678

typedef struct Mult_Rule
{
        char workgroup[64];
        char serverIP[32];
        char shareFolder[255];
        char server_root_path[255];
        char acount[64];
        char password[64];
        int  rule;
        char client_root_path[255];
        char mount_path[255];
}MultRule;

typedef struct _Config
{
        int id;
        int dir_num;
        MultRule **multrule;
}Config;

typedef struct node{
    char *href;
    char *filename;
    int isfile;
    long long int size;
    time_t modtime;
    int ismodify;
    struct node *next;
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
    char *parenthref;
    Browse *browse;
    struct ServerTreeNode *Child;
    struct ServerTreeNode *NextBrother;
}Server_TreeNode;

typedef struct _Node
{
    char *cmdName;
    struct _Node *next;
}Node;

typedef struct Action_Item
{
    char *action;
    char *path;
    struct Action_Item *next;
} action_item;

typedef struct SYNC_LIST
{
    char up_item_file[256];
    char temp_path[256];
    int index;
    int first_sync;
    int receve_socket;
    int have_local_socket;
    int no_local_root;
    int init_completed;
    int sync_disk_exist;
    int IsNetWorkUnlink; //2014.11.20 by sherry (ip是否正确，网络通不通)
    action_item *copy_file_list;
    action_item *server_action_list;
    action_item *dragfolder_action_list;
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

typedef struct LOCALFOLDER{
    char *path;
    char *name;
    struct LOCALFOLDER *next;
}LocalFolder;

typedef struct LOCALFILE{
    char *path;
    char *name;
    time_t modtime;
    long long int size;
    struct LOCALFILE *next;
}LocalFile;

typedef struct LOCAL
{
    int foldernumber;
    int filenumber;
    LocalFolder *folderlist;
    LocalFile *filelist;
}Local;

struct tokenfile_info_tag
{
    char *folder;
    char *url;
    char *mountpath;
    struct tokenfile_info_tag *next;
};

struct mounts_info_tag
{
    int num;
    char **paths;
};

#endif // DATA_H
