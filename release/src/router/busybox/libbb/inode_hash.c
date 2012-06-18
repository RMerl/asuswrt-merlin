/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

typedef struct ino_dev_hash_bucket_struct {
	struct ino_dev_hash_bucket_struct *next;
	ino_t ino;
	dev_t dev;
	char name[1];
} ino_dev_hashtable_bucket_t;

#define HASH_SIZE	311		/* Should be prime */
#define hash_inode(i)	((i) % HASH_SIZE)

/* array of [HASH_SIZE] elements */
static ino_dev_hashtable_bucket_t **ino_dev_hashtable;

/*
 * Return name if statbuf->st_ino && statbuf->st_dev are recorded in
 * ino_dev_hashtable, else return NULL
 */
char* FAST_FUNC is_in_ino_dev_hashtable(const struct stat *statbuf)
{
	ino_dev_hashtable_bucket_t *bucket;

	if (!ino_dev_hashtable)
		return NULL;

	bucket = ino_dev_hashtable[hash_inode(statbuf->st_ino)];
	while (bucket != NULL) {
		if ((bucket->ino == statbuf->st_ino)
		 && (bucket->dev == statbuf->st_dev)
		) {
			return bucket->name;
		}
		bucket = bucket->next;
	}
	return NULL;
}

/* Add statbuf to statbuf hash table */
void FAST_FUNC add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name)
{
	int i;
	ino_dev_hashtable_bucket_t *bucket;

	i = hash_inode(statbuf->st_ino);
	if (!name)
		name = "";
	bucket = xmalloc(sizeof(ino_dev_hashtable_bucket_t) + strlen(name));
	bucket->ino = statbuf->st_ino;
	bucket->dev = statbuf->st_dev;
	strcpy(bucket->name, name);

	if (!ino_dev_hashtable)
		ino_dev_hashtable = xzalloc(HASH_SIZE * sizeof(*ino_dev_hashtable));

	bucket->next = ino_dev_hashtable[i];
	ino_dev_hashtable[i] = bucket;
}

#if ENABLE_DU || ENABLE_FEATURE_CLEAN_UP
/* Clear statbuf hash table */
void FAST_FUNC reset_ino_dev_hashtable(void)
{
	int i;
	ino_dev_hashtable_bucket_t *bucket;

	for (i = 0; ino_dev_hashtable && i < HASH_SIZE; i++) {
		while (ino_dev_hashtable[i] != NULL) {
			bucket = ino_dev_hashtable[i]->next;
			free(ino_dev_hashtable[i]);
			ino_dev_hashtable[i] = bucket;
		}
	}
	free(ino_dev_hashtable);
	ino_dev_hashtable = NULL;
}
#endif
