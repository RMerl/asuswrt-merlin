/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 1999-2008 by Theodore Ts'o
 *
 * (based on list.h from e2fsprogs)
 * Merge sort based on kernel's implementation.
 */

#ifndef UTIL_LINUX_LIST_H
#define UTIL_LINUX_LIST_H

/* TODO: use AC_C_INLINE */
#ifdef __GNUC__
#define _INLINE_ static __inline__
#else                         /* For Watcom C */
#define _INLINE_ static inline
#endif

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
_INLINE_ void __list_add(struct list_head * add,
	struct list_head * prev,
	struct list_head * next)
{
	next->prev = add;
	add->next = next;
	add->prev = prev;
	prev->next = add;
}

/**
 * list_add - add a new entry
 * @add:	new entry to be added
 * @head:	list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
_INLINE_ void list_add(struct list_head *add, struct list_head *head)
{
	__list_add(add, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @add:	new entry to be added
 * @head:	list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
_INLINE_ void list_add_tail(struct list_head *add, struct list_head *head)
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
_INLINE_ void __list_del(struct list_head * prev,
				  struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry:	the element to delete from the list.
 *
 * list_empty() on @entry does not return true after this, @entry is
 * in an undefined state.
 */
_INLINE_ void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry:	the element to delete from the list.
 */
_INLINE_ void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/**
 * list_empty - tests whether a list is empty
 * @head:	the list to test.
 */
_INLINE_ int list_empty(struct list_head *head)
{
	return head->next == head;
}

/**
 * list_last_entry - tests whether is entry last in the list
 * @entry:	the entry to test.
 * @head:	the list to test.
 */
_INLINE_ int list_last_entry(struct list_head *entry, struct list_head *head)
{
	return head->prev == entry;
}

/**
 * list_splice - join two lists
 * @list:	the new list to add.
 * @head:	the place to add it in the first list.
 */
_INLINE_ void list_splice(struct list_head *list, struct list_head *head)
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

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * list_for_each - iterate over elements in a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_backwardly - iterate over elements in a list in reverse
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_backwardly(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe - iterate over elements in a list, but don't dereference
 *                      pos after the body is done (in case it is freed)
 * @pos:	the &struct list_head to use as a loop counter.
 * @pnext:	the &struct list_head to use as a pointer to the next item.
 * @head:	the head for your list (not included in iteration).
 */
#define list_for_each_safe(pos, pnext, head) \
	for (pos = (head)->next, pnext = pos->next; pos != (head); \
	     pos = pnext, pnext = pos->next)

#define MAX_LIST_LENGTH_BITS 20

/*
 * Returns a list organized in an intermediate format suited
 * to chaining of merge() calls: null-terminated, no reserved or
 * sentinel head node, "prev" links not maintained.
 */
_INLINE_ struct list_head *merge(int (*cmp)(struct list_head *a,
					  struct list_head *b),
			       struct list_head *a, struct list_head *b)
{
	struct list_head head, *tail = &head;

	while (a && b) {
		/* if equal, take 'a' -- important for sort stability */
		if ((*cmp)(a, b) <= 0) {
			tail->next = a;
			a = a->next;
		} else {
			tail->next = b;
			b = b->next;
		}
		tail = tail->next;
	}
	tail->next = a?:b;
	return head.next;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
_INLINE_ void merge_and_restore_back_links(int (*cmp)(struct list_head *a,
						    struct list_head *b),
					 struct list_head *head,
					 struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;

	while (a && b) {
		/* if equal, take 'a' -- important for sort stability */
		if ((*cmp)(a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			a = a->next;
		} else {
			tail->next = b;
			b->prev = tail;
			b = b->next;
		}
		tail = tail->next;
	}
	tail->next = a ? : b;

	do {
		/*
		 * In worst cases this loop may run many iterations.
		 * Continue callbacks to the client even though no
		 * element comparison is needed, so the client's cmp()
		 * routine can invoke cond_resched() periodically.
		 */
		(*cmp)(tail->next, tail->next);

		tail->next->prev = tail;
		tail = tail->next;
	} while (tail->next);

	tail->next = head;
	head->prev = tail;
}


/**
 * list_sort - sort a list
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * This function implements "merge sort", which has O(nlog(n))
 * complexity.
 *
 * The comparison function @cmp must return a negative value if @a
 * should sort before @b, and a positive value if @a should sort after
 * @b. If @a and @b are equivalent, and their original relative
 * ordering is to be preserved, @cmp must return 0.
 */
_INLINE_ void list_sort(struct list_head *head,
			int (*cmp)(struct list_head *a,
				   struct list_head *b))
{
	struct list_head *part[MAX_LIST_LENGTH_BITS+1]; /* sorted partial lists
							   -- last slot is a sentinel */
	size_t lev;  /* index into part[] */
	size_t max_lev = 0;
	struct list_head *list;

	if (list_empty(head))
		return;

	memset(part, 0, sizeof(part));

	head->prev->next = NULL;
	list = head->next;

	while (list) {
		struct list_head *cur = list;
		list = list->next;
		cur->next = NULL;

		for (lev = 0; part[lev]; lev++) {
			cur = merge(cmp, part[lev], cur);
			part[lev] = NULL;
		}
		if (lev > max_lev) {
			/* list passed to list_sort() too long for efficiency */
			if (lev >= ARRAY_SIZE(part) - 1)
				lev--;
			max_lev = lev;
		}
		part[lev] = cur;
	}

	for (lev = 0; lev < max_lev; lev++)
		if (part[lev])
			list = merge(cmp, part[lev], list);

	merge_and_restore_back_links(cmp, head, part[max_lev], list);
}

#undef _INLINE_

#endif /* UTIL_LINUX_LIST_H */
