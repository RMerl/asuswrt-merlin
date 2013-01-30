/* vi: set sw=4 ts=4: */

#include "list.h"

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
void __list_add(struct list_head * add,
	struct list_head * prev,
	struct list_head * next)
{
	next->prev = add;
	add->next = next;
	add->prev = prev;
	prev->next = add;
}

/*
 * list_add - add a new entry
 * @add:	new entry to be added
 * @head:	list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
void list_add(struct list_head *add, struct list_head *head)
{
	__list_add(add, head, head->next);
}

/*
 * list_add_tail - add a new entry
 * @add:	new entry to be added
 * @head:	list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
void list_add_tail(struct list_head *add, struct list_head *head)
{
	__list_add(add, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/*
 * list_del - deletes entry from list.
 * @entry:	the element to delete from the list.
 *
 * list_empty() on @entry does not return true after this, @entry is
 * in an undefined state.
 */
void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/*
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry:	the element to delete from the list.
 */
void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/*
 * list_empty - tests whether a list is empty
 * @head:	the list to test.
 */
int list_empty(struct list_head *head)
{
	return head->next == head;
}

/*
 * list_splice - join two lists
 * @list:	the new list to add.
 * @head:	the place to add it in the first list.
 */
void list_splice(struct list_head *list, struct list_head *head)
{
	struct list_head *first = list->next;

	if (first != list) {
		struct list_head *last = list->prev;
		struct list_head *at = head->next;

		first->prev = head;
		head->next = first;

		last->next = at;
		at->prev = last;
	}
}
