/*
 * safe_list - linked list protected against recursive iteration with deletes
 *
 * Copyright (C) 2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "safe_list.h"

struct safe_list_iterator {
	struct safe_list_iterator **head;
	struct safe_list_iterator *next_i;
	struct safe_list *next;
};

static void
__safe_list_set_iterator(struct safe_list *list,
			 struct safe_list_iterator *i)
{
	struct safe_list_iterator *next_i;
	struct safe_list *next;

	next = list_entry(list->list.next, struct safe_list, list);
	next_i = next->i;

	next->i = i;
	i->next = next;
	i->head = &next->i;

	i->next_i = next_i;
	if (next_i)
		next_i->head = &i->next_i;
}

static void
__safe_list_del_iterator(struct safe_list_iterator *i)
{
	*i->head = i->next_i;
	if (i->next_i)
		i->next_i->head = i->head;
}

static void
__safe_list_move_iterator(struct safe_list *list,
			  struct safe_list_iterator *i)
{
	__safe_list_del_iterator(i);
	__safe_list_set_iterator(list, i);
}

int safe_list_for_each(struct safe_list *head,
		       int (*cb)(void *ctx, struct safe_list *list),
		       void *ctx)
{
	struct safe_list_iterator i;
	struct safe_list *cur;
	int ret = 0;

	for (cur = list_entry(head->list.next, struct safe_list, list),
	     __safe_list_set_iterator(cur, &i);
	     cur != head;
	     cur = i.next, __safe_list_move_iterator(cur, &i)) {
		ret = cb(ctx, cur);
		if (ret)
			break;
	}

	__safe_list_del_iterator(&i);
	return ret;
}

void safe_list_add(struct safe_list *list, struct safe_list *head)
{
	list->i = NULL;
	list_add_tail(&list->list, &head->list);
}

void safe_list_del(struct safe_list *list)
{
	struct safe_list_iterator *i, *next_i, **tail;
	struct safe_list *next;

	next = list_entry(list->list.next, struct safe_list, list);
	list_del(&list->list);

	if (!list->i)
		return;

	next_i = next->i;
	tail = &next->i;

	for (i = list->i; i; i = i->next_i) {
		tail = &i->next_i;
		i->next = next;
	}

	next->i = list->i;
	list->i->head = &next->i;
	*tail = next_i;
	if (next_i)
		next_i->head = tail;

	list->i = NULL;
}
