/*
 * Copyright (C) 1995, 1997-1999 Jeffrey A. Uphoff
 * Modified by Olaf Kirch, 1996.
 * Modified by H.J. Lu, 1998.
 * Modified by Lon Hohberger, Oct. 2000.
 *   - Fixed memory leaks, run-off-end problems, etc.
 *
 * NSM for Linux.
 */

/*
 * Simple list management for notify list
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "misc.h"
#include "statd.h"
#include "notlist.h"


#ifdef DEBUG
/* 
 * LH - The linked list code had some bugs.  Used this to help debug
 * new code. 
 */
static void 
plist(notify_list *head, int en)
{
	/* case where we ran off the end */
	if (!head) return;

	printf("Entry %d: %s\n",en, NL_MON_NAME(head));
	plist(head->next, ++en);
}

static void 
nlist_print(notify_list **head)
{
	printf("--- Begin notify list dump ---\n");
	plist(*head,1);
	printf("--- End notify list dump ---\n");
}
#endif /* DEBUG */

/* 
 * Allocate memory and set up a new notify list entry.
 */
notify_list * 
nlist_new(char *my_name, char *mon_name, int state)
{
	notify_list	*new;

	new = (notify_list *) xmalloc(sizeof(notify_list));
	memset(new, 0, sizeof(*new));

	NL_TIMES(new) = MAX_TRIES;
	NL_STATE(new) = state;
	NL_MY_NAME(new) = xstrdup(my_name);
	NL_MON_NAME(new) = xstrdup(mon_name);

	return new;
}

/*
 * Insert *entry into a notify list at the point specified by
 * **head.  This can be in the middle.  However, we do not handle
 * list _append_ in this function; rather, the only place we should
 * have to worry about this case is in nlist_insert_timer below.
 * - entry must not be NULL.
 */
void 
nlist_insert(notify_list **head, notify_list *entry)
{
	if (*head) {
		/* 
		 * Cases where we're prepending a non-empty list
		 * or inserting possibly in the middle somewhere (eg,
		 * nlist_insert_timer...)
		 */
		entry->next = (*head);		/* Forward pointer */
		entry->prev = (*head)->prev;	/* Back pointer */
		(*head)->prev = entry;		/* head's new back pointer */
	}

	/* Common to all cases, including new list creation */
	*head = entry;			/* New head */

#ifdef DEBUG
	nlist_print(head);
#endif
}

/* 
 * (re)insert *entry into notify_list **head.  This requires that
 * NL_WHEN(entry) has been set (usually, this is time() + 5 seconds).
 * - entry must not be NULL
 *
 * LH - This used to cause (a) a memory leak and (b) dropped notify-list
 * entries.  The pointer ran off the end of the list, and changed the 
 * head-end to point to the new, one-entry list.  All other entries became garbage.
 *
 * FIXME: Optimize this function. (I'll work on it - LH)
 */
void 
nlist_insert_timer(notify_list **head, notify_list *entry)
{
	notify_list 	*spot = *head,	 	/* Insertion location */
						/* ...Start at head */
			*back = NULL;		/* Back pointer */


	/* Find first entry with higher timeout value or end of list */
	while (spot && NL_WHEN(spot) <= NL_WHEN(entry)) {
		/* 
		 * Keep the back pointer in case we 
		 * run off the end...  (see below)
		 */
		back = spot;
		spot = spot->next;
	}

	if (spot == (*head)) {
		/* 
		 * case where we're prepending an empty or non-empty
		 * list or inserting in the middle somewhere.  Pass 
		 * the real head of the list, since we'll be changing 
		 * during the insert... 
		 */
		nlist_insert(head, entry);
	} else {
		/* all other cases - don't move the real head pointer */
		nlist_insert(&spot, entry);

		/* 
		 * If spot == entry, then spot was NULL when we called
		 * nlist_insert.  This happened because we had run off 
		 * the end of the list.  Append entry to original list.
		 */
		if (spot == entry) {
			back->next = entry;
			entry->prev = back;
		}
	}
}

/* 
 * Remove *entry from the list pointed to by **head.
 * Do not destroy *entry.  This is normally done before
 * a re-insertion with a timer, but can be done anywhere.
 * - entry must not be NULL.
 */
void 
nlist_remove(notify_list **head, notify_list *entry)
{
	notify_list	*prev = entry->prev,
			*next = entry->next;

	if (next) {
		next->prev = prev;
	}

	if (prev) {
		/* Case(s) where entry isn't at the front */
		prev->next = next; 
	} else {
		/* cases where entry is at the front */
		*head = next;
	}

	entry->next = entry->prev = NULL;
#ifdef DEBUG
	nlist_print(head);
#endif
}

/* 
 * Clone an entry in the notify list -
 * - entry must not be NULL
 */
notify_list *
nlist_clone(notify_list *entry)
{
	notify_list	*new;

	new = nlist_new(NL_MY_NAME(entry), NL_MON_NAME(entry), NL_STATE(entry));
	NL_MY_PROG(new) = NL_MY_PROG(entry);
	NL_MY_VERS(new) = NL_MY_VERS(entry);
	NL_MY_PROC(new) = NL_MY_PROC(entry);
	NL_ADDR(new)    = NL_ADDR(entry);
	memcpy(NL_PRIV(new), NL_PRIV(entry), SM_PRIV_SIZE);

	return new;
}

/* 
 * Destroy an entry in a notify list and free the memory.
 * If *head is NULL, just free the entry.  This would be
 * done only when we know entry isn't in any list.
 * - entry must not be NULL.
 */
void 
nlist_free(notify_list **head, notify_list *entry)
{
	if (head && (*head))
		nlist_remove(head, entry);
	if (NL_MY_NAME(entry))
		free(NL_MY_NAME(entry));
	if (NL_MON_NAME(entry))
		free(NL_MON_NAME(entry));
	free(entry->dns_name);
	free(entry);
}

/* 
 * Destroy an entire notify list 
 */
void 
nlist_kill(notify_list **head)
{
	while (*head)
		nlist_free(head, *head);
}

/*
 * Walk a list looking for a matching name in the NL_MON_NAME field.
 */
notify_list *
nlist_gethost(notify_list *list, char *host, int myname)
{
	notify_list	*lp;

	for (lp = list; lp; lp = lp->next) {
		if (matchhostname(host, myname? NL_MY_NAME(lp) : NL_MON_NAME(lp)))
			return lp;
	}

	return (notify_list *) NULL;
}
