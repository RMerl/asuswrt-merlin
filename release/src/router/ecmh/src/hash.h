/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: $
 $Id: $
 $Date: $
**************************************/

#ifndef ECMH_HASH_H
#define ECMH_HASH_H

#include "linklist.h"

/* Hash size */
#define HASH_SIZE 7919

struct hash
{
	struct list *items[HASH_SIZE];
};

/* Prototypes. */
struct hash	*hash_new();
void		hash_free(struct list *);
void		hash_add(struct hash *, void *);
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

