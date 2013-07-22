/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: clist.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _CLIST_H_
#define _CLIST_H_

typedef struct clist_head
{
	struct clist_head *next, *prev;
} clist_head_t;

#define CLIST_DECL_INIT(head) clist_head_t head = { &(head), &(head) }

static inline void
clist_init_head(clist_head_t *head)
{
	head->next = head->prev = head;
}

static inline void
clist_add_head(clist_head_t *head, clist_head_t *item)
{
	head->next->prev = item;
	item->next = head->next;
	item->prev = head;
	head->next = item;

	return;
}

static inline void
clist_add_tail(clist_head_t *head, clist_head_t *item)
{
	item->next = head;
	item->prev = head->prev;
	head->prev->next = item;
	head->prev = item;

	return;
}

static inline void
clist_delete(clist_head_t *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;

	return;
}

static inline void
clist_walk(clist_head_t *head, void (*fn)(clist_head_t *, void *), void *arg)
{
	clist_head_t *ptr;

	ptr = head;

	while (ptr->next != head)
	{
		fn(ptr, arg);
		ptr = ptr->next;
	}

	return;
}

#define clist_empty(h) ((h)->next == (h))

#define clist_entry(p, type, member) \
	    ((type *)((char *)(p)-(unsigned long)(&((type *)0)->member)))

#endif /* _CLIST_H_ */
