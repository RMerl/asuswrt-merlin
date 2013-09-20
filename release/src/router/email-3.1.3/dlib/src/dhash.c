#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dlib.h"
#include "dutil.h"
#include "dhash.h"
#include "dlist.h"

typedef struct {
	char *key;
	void *val;
} dhashval;

/**
 * Obviously creates a hash value and returns it
 */
static unsigned int
hash(const char *key, unsigned int tableSize)
{
	register const char *ptr = key;
	register unsigned int value = 0;

	while (*ptr != '\0') {
		value = (value * 37) + (int) *ptr++;
	}
	return value % tableSize;
}

/**
 * Transverse the list provided in search of the key.
 */
static dhashval *
getValueFromList(dlist top, const char *key)
{
	dhashval *keyval = NULL;
	while ((keyval = (dhashval *)dlGetNext(top))) {
		if (strcmp(keyval->key, key) == 0) {
			break;
		}
	}
	return keyval;
}

/**
 * Initialize the hash table.
 *
 * Params
 * 	size - The size you want the table to be (will go up by next prime)
 *
 * Return
 * 	A new dhash
 */
dhash
dhInit(size_t size, dhashDestroyFunc destroy)
{
	uint i;
	dhash table = xmalloc(sizeof(struct _dhash));
	table->tableSize = nextprime(size);
	table->destroy = destroy;
	table->hashed = xmalloc(table->tableSize * sizeof(struct _dlist));
	for (i=0; i < table->tableSize; i++) {
		table->hashed[i] = dlInit(NULL);
	}
	return table;
}

/**
 * Lookup a key in the hash table and return it's key/val pair.
 *
 * Params
 * 	table - The dhash table to use
 * 	key - A key to fine in the hash
 *
 * Return
 * 	The data they hashed if it's found.
 * 	NULL if nothing is found.
 */
void *
dhGetItem(dhash table, const char *key)
{
	unsigned int index = 0;
	dlist list = NULL;
	dhashval *keyval = NULL;

	index = hash(key, table->tableSize);
	list = table->hashed[index];
	if (list)  { 
		keyval = getValueFromList(list, key);
	}
	if (!keyval) {
		return NULL;
	}
	dlReset(list);
	return keyval->val;
}

/**
 * Insert a key and value into the hashed array.
 *
 * Params
 * 	hash - The hash table to insert into
 * 	keyval - A dhashval struct with a populated key/val
 */
void
dhInsert(dhash top, const char *key, const void *val)
{
	unsigned int index = 0;
	dhashval *item = xmalloc(sizeof(dhashval));
	item->key = xstrdup(key);
	item->val = (void *)val;
	index = hash(item->key, top->tableSize);
	dlInsertTop(top->hashed[index], item);
}

/**
 * Free all allocated memory in the hashed array
 *
 * Params
 * 	table - The hash table to free
 */
void
dhDestroy(dhash table)
{
	uint i;
	dhashval *next;
	if (table) {
		for (i=0; i < table->tableSize; i++) {
			dlist this = table->hashed[i];
			while ((next = (dhashval *)dlGetNext(this))) {
				xfree(next->key);
				if (table->destroy) {
					table->destroy(next->val);
				}
				xfree(next);
			}
			dlDestroy(this);
		}
		xfree(table->hashed);
		xfree(table);
	}
}

