#include "options.h"
#include "dbutil.h"
#include "list.h"

void list_append(m_list *list, void *item) {
	m_list_elem *elem;
	
	elem = m_malloc(sizeof(*elem));
	elem->item = item;
	elem->list = list;
	elem->next = NULL;
	if (!list->first) {
		list->first = elem;
		elem->prev = NULL;
	} else {
		elem->prev = list->last;
		list->last->next = elem;
	}
	list->last = elem;
}

m_list * list_new() {
	m_list *ret = m_malloc(sizeof(m_list));
	ret->first = ret->last = NULL;
	return ret;
}

void * list_remove(m_list_elem *elem) {
	void *item = elem->item;
	m_list *list = elem->list;
	if (list->first == elem)
	{
		list->first = elem->next;
	}
	if (list->last == elem)
	{
		list->last = elem->prev;
	}
	if (elem->prev)
	{
		elem->prev->next = elem->next;
	}
	if (elem->next)
	{
		elem->next->prev = elem->prev;
	}
	m_free(elem);
	return item;
}
