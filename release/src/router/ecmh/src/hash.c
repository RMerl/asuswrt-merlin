/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: $
 $Id: $
 $Date: $
**************************************/

#include "ecmh.h"

struct hash *hash_new()
{
	struct hash	*hash;
	unsigned int	i;

	hash = malloc(sizeof(struct hash));
	if (!hash) return NULL;
	memset(hash, 0, sizeof(struct hash));
	for (i=0;i<HASH_SIZE;i++)
	{
		hash.items[i] = list_new();
	}
	return hash;
}

void hash_free(struct hash *hash);
{
	unsigned int i;

	if (!hash) return;

	/* Free the lists */
	for (i=0;i<HASH_SIZE;i++)
	{
		list_free(hash.items[i]);
	}
}

void hashnode_add(struct hash *hash, void *key, unsigned int keylen, void *val)
{
	unsigned int index = hash_make(key, keylen)%HASH_SIZE;
	listnode_add(hash.items[index], val);
}

void hashnode_delete(struct hash *hash, void *key, unsigned int keylen, void *val)
{
	unsigned int index = hash_make(key, keylen)%HASH_SIZE;
	listnode_delete(hash.items[index], val);
}

/*
 * MMH Basic hash function
 * Implementation as per the MMH paper, but adapted for variable length messages
 */
unsigned long hash_make(uint16_t *msg, int len)
{
	int64_t		stmp = 0;	/* temporary variables */
	uint64_t	utmp = 0;

	uint64_t	sum = 0;	/* running sum */
	uint64_t	ret = 0;	/* return value; */
	int i;

	static char keyc[] =		/* 2x 26 + 10 = 62 per line -> 4x = 248 bytes key */
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static uint16_t *key = (uint16_t *)&keyc;

	/* The lenght is in bytes, while we MMH per word */
	len/=2;

	for (i=0;i<len;i++) sum += key[i] * (uint64_t)msg[i];

	stmp = (sum  & 0xffffffff) - ((sum  >> 32) * 15); /* lo - hi * 15 */
	utmp = (stmp & 0xffffffff) - ((stmp >> 32) * 15); /* lo - hi * 15 */

	ret = utmp & 0xffffffff;
	if (utmp > 0xffffffff)		/* if larger than p - substract 15 again */
	{
		ret -= 15;
	}

	return (unsigned long)ret;
}

