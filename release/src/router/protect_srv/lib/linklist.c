/* Generic linked list routine.
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
 * modded for ecmh by Jeroen Massar
 */

#include "linklist.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

/* Allocate new list. */
struct list *list_new()
{
	struct list *new;
	
	new = malloc(sizeof(struct list));
	if (!new) return NULL;
	memset(new, 0, sizeof(struct list));
	return new;
}

/* Free list. */
void list_free(struct list *l)
{
	if (l) free(l);
}

/* Allocate new listnode.  Internal use only. */
static struct listnode *listnode_new(void)
{
	struct listnode *node;
	
	node = malloc(sizeof(struct listnode));
	if (!node) return NULL;
	memset(node, 0, sizeof(struct listnode));
	return node;
}

/* Free listnode. */
static void listnode_free(struct listnode *node)
{
	free(node);
}

/* Add new data to the list. */
void listnode_add(struct list *list, void *val)
{
	struct listnode *node;
	
	node = listnode_new();
	if (!node) return;
	
	node->prev = list->tail;
	node->data = val;
	
	if (list->head == NULL) list->head = node;
	else list->tail->next = node;
	list->tail = node;
	
	if (list->count < 0) list->count = 0;
	list->count++;
}

/* Delete specific date pointer from the list. */
void listnode_delete(struct list *list, void *val)
{
	struct listnode *node;
	
	for (node = list->head; node; node = node->next)
	{
		if (node->data != val) continue;
		
		if (node->prev) node->prev->next = node->next;
		else list->head = node->next;
		if (node->next) node->next->prev = node->prev;
		else list->tail = node->prev;
		list->count--;
		if(node->data)
			free(node->data);
		listnode_free(node);
		return;
	}
}

/* Delete all listnode from the list. */
void list_delete_all_node(struct list *list)
{
	struct listnode *node;
	struct listnode *next;
	
	for (node = list->head; node; node = next)
	{
		next = node->next;
		//if (list->del) (*list->del)(node->data);
		if(node->data)
			free(node->data);
		listnode_free(node);
	}
	list->head = list->tail = NULL;
	list->count = 0;
}

/* Delete all listnode then free list itself. */
void list_delete(struct list *list)
{
	struct listnode *node;
	struct listnode *next;
	
	for (node = list->head; node; node = next)
	{
		next = node->next;
		//if (list->del) (*list->del)(node->data);
		if(node->data)
			free(node->data);
		listnode_free(node);
	}
	list_free (list);
}

/* Delete the node from list.  For ospfd and ospf6d. */
void list_delete_node(struct list *list, struct listnode *node)
{
	if (node->prev) node->prev->next = node->next;
	else list->head = node->next;
	if (node->next) node->next->prev = node->prev;
	else list->tail = node->prev;
	list->count--;
	listnode_free(node);
}

/* Move the node to the front - for int_find() - jeroen */
void list_movefront_node(struct list *list, struct listnode *node)
{
	/* Don't do a thing when it is already there */
	if (list->head == node) return;
	
	/* Delete it from the list's current position */
	if (node->prev) node->prev->next = node->next;
	else list->head = node->next;
	if (node->next) node->next->prev = node->prev;
	else list->tail = node->prev;
	
	/* Insert it at the front */
	if (list->head)
	{
		node->prev = list->head->prev;
		list->head->prev = node;
	}
	node->next = list->head;
	list->head = node;
}

