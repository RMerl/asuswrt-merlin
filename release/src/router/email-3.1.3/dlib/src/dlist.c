#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dutil.h"
#include "dlist.h"

struct dlistnode {
	void *data;
	size_t size;
	struct dlistnode *next;
};

/**
 * Creates a linked list and allows you to specify a
 * destroy function for the data that is input into the list.
 *
 * Params
 * 	destroy - a function that the caller created for destroying data
 *
 * Return
 * 	A new dlist structure
 */
dlist
dlInit(dlistDestroyFunc destroy)
{
	dlist ptr = xmalloc(sizeof(struct _dlist));
	ptr->size = 0;
	ptr->list = NULL;
	ptr->save = NULL;
	ptr->destroy = destroy;
	return ptr;
}
	

/**
 * Create a new node and and link it up with the rest
 */
void
dlInsertTop(dlist ref, void *data)
{
	struct dlistnode *ret = xmalloc(sizeof(struct dlistnode));

	assert(ref != NULL);
	ret->next = NULL;
	if (data) {
		ret->data = data;
	} else {
		ret->data = NULL;
	}

	// The next pointer should point to what's currently
	// at the top of the list. Then, point the top of the list
	// to this node we just created.
	ret->next = ref->list;
	ref->list = ret;

	// Reset the "save" pointer to point to our new node.
	ref->save = ref->list;
	ref->size++;
}

void
dlInsertEnd(dlist ref, void *data)
{
	struct dlistnode *current;
	struct dlistnode *ret = xmalloc(sizeof(struct dlistnode));

	assert(ref != NULL);
	ret->next = NULL;
	if (data) {
		ret->data = data;
	} else {
		ret->data = NULL;
	}

	for (current = ref->list; current->next; current = current->next) {
		; /* Just get to the end of the list. */
	}
	current->next = ret;
	ref->size++;
}

/**
 * Copy 'from' list to 'to' list
 */
void
dlCopy(dlist to, dlist from)
{
	struct dlistnode *current = NULL;
	for (current = from->list; current; current = current->next) {
		dlInsertTop(to, current->data);
	}
}

/**
 * Resets the save pointer so that when calling GetNext, 
 * you won't be starting from mid list.
 */
void
dlReset(dlist ref)
{
	ref->save = ref->list;
}

/**
 * Returns the next element in a list.
 *
 * Params
 * 	ref - The dlist to get the next element from.
 *
 * Return
 * 	the next element in the dlist, or
 * 	null if at the end of the list.
 */
void *
dlGetNext(dlist ref)
{
	void *data = NULL;
	if (ref) {
		if (ref->save) {
			data = ref->save->data;
			ref->save = ref->save->next;
		} else {
			data = NULL;
			ref->save = ref->list;
		}
	}
	return data;
}

/**
 * Free the entire list 
 */
void
dlDestroy(dlist ref)
{
	struct dlistnode *tmp = NULL;
	struct dlistnode *first = ref->list;

	for (; first; first = tmp) {
		tmp = first->next;
		if (first->data && ref->destroy) {
			ref->destroy(first->data);
		}
		xfree(first);
	}
	xfree(ref);
}


/**
 * Removes the top element from the list
 */
void
dlPop(dlist ref)
{
	struct dlistnode *hold=NULL, *tmp=ref->list;

	hold = tmp->next;
	if (ref->destroy && tmp->data) {
		ref->destroy(tmp->data);
	}
	xfree(tmp);
	ref->list = hold;
	ref->save = ref->list;
	ref->size--;
}

void *
dlGetTop(dlist ref)
{
	return ref->list->data;
}

