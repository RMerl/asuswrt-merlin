#ifndef _DROPBEAR_LIST_H
#define _DROPBEAR_LIST_H

struct _m_list;

struct _m_list_elem {
    void *item;
	struct _m_list_elem *next;
	struct _m_list_elem *prev;
    struct _m_list *list;
};
	
typedef struct _m_list_elem m_list_elem;

struct _m_list {
    m_list_elem *first;
    m_list_elem *last;
};

typedef struct _m_list m_list;

m_list * list_new();
void list_append(m_list *list, void *item);
/* returns the item for the element removed */
void * list_remove(m_list_elem *elem);


#endif /* _DROPBEAR_LIST_H */
