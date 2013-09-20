#include "event_queue.h"
#include <stdlib.h>
#include <stdint.h>

/* Simple queue implemented as a singly linked list with head 
   and tail pointers maintained
*/
int queue_empty (queue_t q)
{
    return q->head == NULL;
}

queue_t queue_create ()
{
    queue_t q;
    q = malloc (sizeof (struct queue_struct));
    if (q == NULL)
        exit (-1);

    q->head = q->tail = NULL;
    return q;
}

void queue_destroy (queue_t q)
{
    if (q != NULL)
    {
        while (q->head != NULL)
	{
            queue_entry_t next = q->head;
            q->head = next->next_ptr;
            next->next_ptr = NULL;
            free (next);
	}
        q->head = q->tail = NULL;
        free (q);
    }
}

void queue_enqueue (queue_entry_t d, queue_t q)
{
    d->next_ptr = NULL;
    if (q->tail)
    {
        q->tail->next_ptr = d;
        q->tail = d;
    }
    else
    {
        q->head = q->tail = d;
    }
}

queue_entry_t  queue_dequeue (queue_t q)
{
    queue_entry_t first = q->head;

    //if(first->inot_ev.mask == IN_MOVED_FROM)

    if (first)
    {
        q->head = first->next_ptr;
        if (q->head == NULL)
	{
            q->tail = NULL;
	}
        first->next_ptr = NULL;
    }
    return first;
}

//add by gauss
int action_empty (action_t q)
{
    return q->head == NULL;
}

action_t action_create ()
{
    action_t q;
    q = malloc (sizeof (struct action_struct));
    if (q == NULL)
        exit (-1);

    q->head = q->tail = NULL;
    return q;
}

void action_destroy (action_t q)
{
    if (q != NULL)
    {
        while (q->head != NULL)
        {
            action_entry_t next = q->head;
            q->head = next->next_ptr;
            next->next_ptr = NULL;
            free (next);
        }
        q->head = q->tail = NULL;
        free (q);
    }
}

void action_enqueue (action_entry_t d, action_t q)
{
    d->next_ptr = NULL;
    if (q->tail)
    {
        q->tail->next_ptr = d;
        q->tail = d;
    }
    else
    {
        q->head = q->tail = d;
    }
}

action_entry_t  action_dequeue (action_t q)
{
    action_entry_t first = q->head;

    if (first)
    {
        q->head = first->next_ptr;
        if (q->head == NULL)
        {
            q->tail = NULL;
        }
        first->next_ptr = NULL;
    }
    return first;
}

List *create_list_head(){

    List *head;

    head = (List *)malloc(sizeof(List));
    if(head == NULL)
    {
        printf("create memory error!\n");
        exit(-1);
    }
    memset(head,'\0',sizeof(List));
    head->next = NULL;

    return head;
}

int add_list(const char *path,List *head){

    //printf("add_list,path = %s\n",path);

    List *p1,*p2;

    p1 = head;

    p2 = (List *)malloc(sizeof(List));
    memset(p2,'\0',sizeof(List));
    p2->path = (char *)malloc(sizeof(char)*(strlen(path)+1));
    memset(p2->path,'\0',sizeof(p2->path));

    sprintf(p2->path,"%s",path);
    p2->open_num = 0;

    while(p1->next != NULL)
        p1 = p1->next;

    p1->next = p2;
    p2->next = NULL;

    //printf("add list OK!\n");

    return 0;
}

List *get_list(const char *path,List *head){

    List *p;
    p = head->next;

    while(p != NULL)
    {
        if(!strcmp(p->path,path))
        {
            return p;
        }
        p = p->next;
    }

    //printf("can not find action item\n");
    return NULL;
}

int del_list(const char *path,List *head){

    List *p1,*p2;
    p1 = head->next;
    p2 = head;

    while(p1 != NULL)
    {
        if(!strcmp(p1->path,path))
        {
            p2->next = p1->next;
            free(p1->path);
            free(p1);
            return 0;
        }
        p2 = p1;
        p1 = p1->next;
    }

    //printf("can not find action item\n");
    return 1;
}

Folder *del_Folder(int wd,Folder *head){

    Folder *p1,*p2;
    p1 = head->next;
    p2 = head;

    while(p1 != NULL)
    {
        if(p1->wd == wd)
        {
            p2->next = p1->next;
            free(p1->name);
            free(p1);
            //return 0;
            p1 = p2->next;
        }
        else
        {
            p2 = p1;
            p1 = p1->next;
        }
    }
    return p2;

    //printf("can not find action item\n");
    //return 0;
}

Folder *get_Folder(int wd,Folder *head){

    Folder *p;
    p = head->next;

    while(p != NULL)
    {
        //printf("p->wd = %d\n",p->wd);
        if(wd == p->wd)
        {
            return p;
        }
        p = p->next;
    }

    //printf("can not find action item\n");
    return NULL;
}
