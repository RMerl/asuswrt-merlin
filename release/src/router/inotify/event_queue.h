#ifndef __EVENT_QUEUE_H
#define __EVENT_QUEUE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>

struct queue_entry;

struct queue_entry
{
  struct queue_entry * next_ptr;   /* Pointer to next entry */
  struct inotify_event inot_ev;
};

typedef struct queue_entry * queue_entry_t;

/*struct queue_struct; */
struct queue_struct
{
  struct queue_entry * head;
  struct queue_entry * tail;
};
typedef struct queue_struct *queue_t;

int queue_empty (queue_t q);
queue_t queue_create ();
void queue_destroy (queue_t q);
void queue_enqueue (queue_entry_t d, queue_t q);
queue_entry_t queue_dequeue (queue_t q);

//add by gauss
struct action_entry;

struct action_entry
{
    int type;
    char name[64];
    char path[256];
    struct action_entry *next_ptr;
};

typedef struct action_entry * action_entry_t;

struct action_struct
{
  struct action_entry * head;
  struct action_entry * tail;
};
typedef struct action_struct *action_t;

typedef struct LIST
{
    char *path;
    int open_num;
    struct LIST *next;
}List;
List *create_file_list;

int action_empty (action_t q);
action_t action_create ();
void action_destroy (action_t q);
void action_enqueue (action_entry_t d, action_t q);
action_entry_t action_dequeue (action_t q);
List *create_list_head();
int add_list(const char *path,List *head);
List *get_list(const char *path,List *head);
int del_list(const char *path,List *head);

typedef struct FOLDER
{
    int type;
    int wd;
    char *name;
    struct FOLDER *next;
}Folder;

Folder *allfolderlist;
Folder *allfolderlist_tail;
Folder *allfolderlist_tmp;

Folder *pathlist;
Folder *pathlist_tail;
Folder *pathlist_tmp;

Folder *del_Folder(int wd,Folder *head);
Folder *get_Folder(int wd,Folder *head);

#endif
