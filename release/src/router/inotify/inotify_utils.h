#ifndef __INOTIFY_UTILS_H
#define __INOTIFY_UTILS_H

#include "event_queue.h"

#define ACT 1
//#define BASIC_PATH                  "../../asuswebstorage"
//#define INOTIFY_PATH                BASIC_PATH"/inotify_file/"
//#define MOVED_FROM                  INOTIFY_PATH"MOVED_FROM"
//#define MOVED_TO                    INOTIFY_PATH"MOVED_TO"
//#define MOVE_SELF                   INOTIFY_PATH"MOVE_SELF"
//#define MODIFY                      INOTIFY_PATH"MODIFY"
#define ASUSWEBSTORAGE_PORT         3567
#define BOOKSNET_PORT               3568
#define WEBDAV_PORT                 3569
#define SKYDRIVER_PORT              3570

#define MAXDATASIZE                 500
#define ASUSWEBSTORAGE_SYNCFOLDER   "MySyncFolder"



#define ASUSWEBSTORAGE      0
#define BOOKSNET            1
#define WEBDAV              2
#define SKYDRIVER           3

/* get all items struct*/

char mount_path[256];

typedef struct FOLDER
{
    int type;
    char name[512];
}Folder;

typedef struct FOLDERS
{
  int number;
  Folder folderlist[512];
}Folders;

typedef struct PATHS
{
  int number;
  Folder folderlist[8];
}Paths;

void handle_event (queue_entry_t event);
int read_event (int fd, struct inotify_event *event);
int event_check (int fd);
int process_inotify_events (queue_t q, int fd);
int watch_dir (int fd, const char *dirname, unsigned long mask);
int ignore_wd (int fd, int wd);
int close_inotify_fd (int fd);
int open_inotify_fd ();



//int inotifyStart(char *dirname); //add by gauss
int send_action(int type,char *content);



#endif
