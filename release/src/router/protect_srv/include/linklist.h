/*
 * Generic linked list
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
 * customized for ecmh by Jeroen Massar
 */

#ifndef __LINKLIST_H
#define __LINKLIST_H

struct listnode 
{
	struct listnode	*next;
	struct listnode	*prev;
	void		*data;
};

struct list 
{
	struct listnode	*head;
	struct listnode	*tail;
	int		count;
	void		(*del)(void *val);
};

#define nextnode(X)	((X) = (X)->next)
#define listhead(X)	((X)->head)
#define listcount(X)	((X)->count)
#define list_isempty(X)	((X)->head == NULL && (X)->tail == NULL)
#define getdata(X)	((X)->data)

/* Prototypes. */
struct list	*list_new(void);
void		list_free(struct list *);
void		listnode_add(struct list *, void *);
void		listnode_delete(struct list *, void *);
void		list_delete (struct list *);
void		list_delete_all_node (struct list *);
void		list_delete_node (struct list *, struct listnode *);
/* Move the node to the front - for int_find() - jeroen */
void		list_movefront_node(struct list *, struct listnode *);

/* List iteration macro. */
#define LIST_LOOP(L,V,N) \
  for ((N) = (L)->head; (N); (N) = (N)->next) \
    if (((V) = (N)->data) != NULL)

/* List iteration macro. */
#define LIST_LOOP2(L,V,N,M) \
  for ((M) = (L)->head; (M);) \
  {\
    (N) = (M); \
    (M) = (N)->next; \
    if (((V) = (N)->data) != NULL)

#define LIST_LOOP2_END }

#endif /* __LINKLIST_H */

