#ifndef __DHASH_H
#define __DHASH_H  1

#include <sys/types.h>
#include "dlist.h"

typedef void (*dhashDestroyFunc)(void *data);
typedef struct _dhash {
	unsigned int tableSize;
	dlist *hashed;
	dhashDestroyFunc destroy;
} *dhash;

dhash dhInit(size_t size, dhashDestroyFunc destroy);
void *dhGetItem(dhash table, const char *key);
void dhInsert(dhash table, const char *key, const void *data);
void dhDestroy(dhash table);

#endif /* __DHASH_H */
