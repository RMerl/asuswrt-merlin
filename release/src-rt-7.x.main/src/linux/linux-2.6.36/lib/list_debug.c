/*
 * Copyright 2006, Red Hat, Inc., Dave Jones
 * Released under the General Public License (GPL).
 *
 * This file contains the linked list implementations for
 * DEBUG_LIST.
 */

#include <linux/module.h>
#include <linux/list.h>

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */

void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	WARN(next->prev != prev,
		"list_add corruption. next->prev should be "
		"prev (%p), but was %p. (next=%p).\n",
		prev, next->prev, next);
	WARN(prev->next != next,
		"list_add corruption. prev->next should be "
		"next (%p), but was %p. (prev=%p).\n",
		next, prev->next, prev);
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}
EXPORT_SYMBOL(__list_add);

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
void list_del(struct list_head *entry)
{
	WARN(entry->next == LIST_POISON1,
		"list_del corruption, next is LIST_POISON1 (%p)\n",
		LIST_POISON1);
	WARN(entry->next != LIST_POISON1 && entry->prev == LIST_POISON2,
		"list_del corruption, prev is LIST_POISON2 (%p)\n",
		LIST_POISON2);
	WARN(entry->prev->next != entry,
		"list_del corruption. prev->next should be %p, "
		"but was %p\n", entry, entry->prev->next);
	WARN(entry->next->prev != entry,
		"list_del corruption. next->prev should be %p, "
		"but was %p\n", entry, entry->next->prev);
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}
EXPORT_SYMBOL(list_del);
