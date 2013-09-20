#ifndef __DLIST_H
#define __DLIST_H 1

#include <sys/types.h>

typedef void (*dlistDestroyFunc)(void *data);
typedef struct _dlist {
	size_t size;
	struct dlistnode *save;
	struct dlistnode *list;
	dlistDestroyFunc destroy;
} *dlist;

dlist dlInit(dlistDestroyFunc destroy);
void dlInsertTop(dlist ref, void *data);
void dlInsertEnd(dlist ref, void *data);
void dlCopy(dlist src, dlist dest);
void dlReset(dlist ref);
void *dlGetNext(dlist ref);
void dlDestroy(dlist ref);
void dlistPop(dlist ref);
void *dlGetTop(dlist ref);

#endif

