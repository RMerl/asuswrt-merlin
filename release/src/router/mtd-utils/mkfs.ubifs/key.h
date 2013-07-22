/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */

/*
 * This header contains various key-related definitions and helper function.
 * UBIFS allows several key schemes, so we access key fields only via these
 * helpers. At the moment only one key scheme is supported.
 *
 * Simple key scheme
 * ~~~~~~~~~~~~~~~~~
 *
 * Keys are 64-bits long. First 32-bits are inode number (parent inode number
 * in case of direntry key). Next 3 bits are node type. The last 29 bits are
 * 4KiB offset in case of inode node, and direntry hash in case of a direntry
 * node. We use "r5" hash borrowed from reiserfs.
 */

#ifndef __UBIFS_KEY_H__
#define __UBIFS_KEY_H__

/**
 * key_mask_hash - mask a valid hash value.
 * @val: value to be masked
 *
 * We use hash values as offset in directories, so values %0 and %1 are
 * reserved for "." and "..". %2 is reserved for "end of readdir" marker. This
 * function makes sure the reserved values are not used.
 */
static inline uint32_t key_mask_hash(uint32_t hash)
{
	hash &= UBIFS_S_KEY_HASH_MASK;
	if (unlikely(hash <= 2))
		hash += 3;
	return hash;
}

/**
 * key_r5_hash - R5 hash function (borrowed from reiserfs).
 * @s: direntry name
 * @len: name length
 */
static inline uint32_t key_r5_hash(const char *s, int len)
{
	uint32_t a = 0;
	const signed char *str = (const signed char *)s;

	len = len;
	while (*str) {
		a += *str << 4;
		a += *str >> 4;
		a *= 11;
		str++;
	}

	return key_mask_hash(a);
}

/**
 * key_test_hash - testing hash function.
 * @str: direntry name
 * @len: name length
 */
static inline uint32_t key_test_hash(const char *str, int len)
{
	uint32_t a = 0;

	len = min_t(uint32_t, len, 4);
	memcpy(&a, str, len);
	return key_mask_hash(a);
}

/**
 * ino_key_init - initialize inode key.
 * @c: UBIFS file-system description object
 * @key: key to initialize
 * @inum: inode number
 */
static inline void ino_key_init(union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = UBIFS_INO_KEY << UBIFS_S_KEY_BLOCK_BITS;
}

/**
 * dent_key_init - initialize directory entry key.
 * @c: UBIFS file-system description object
 * @key: key to initialize
 * @inum: parent inode number
 * @nm: direntry name and length
 */
static inline void dent_key_init(const struct ubifs_info *c,
				 union ubifs_key *key, ino_t inum,
				 const struct qstr *nm)
{
	uint32_t hash = c->key_hash(nm->name, nm->len);

	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->u32[0] = inum;
	key->u32[1] = hash | (UBIFS_DENT_KEY << UBIFS_S_KEY_HASH_BITS);
}

/**
 * data_key_init - initialize data key.
 * @c: UBIFS file-system description object
 * @key: key to initialize
 * @inum: inode number
 * @block: block number
 */
static inline void data_key_init(union ubifs_key *key, ino_t inum,
				 unsigned int block)
{
	ubifs_assert(!(block & ~UBIFS_S_KEY_BLOCK_MASK));
	key->u32[0] = inum;
	key->u32[1] = block | (UBIFS_DATA_KEY << UBIFS_S_KEY_BLOCK_BITS);
}

/**
 * key_write - transform a key from in-memory format.
 * @c: UBIFS file-system description object
 * @from: the key to transform
 * @to: the key to store the result
 */
static inline void key_write(const union ubifs_key *from, void *to)
{
	union ubifs_key *t = to;

	t->j32[0] = cpu_to_le32(from->u32[0]);
	t->j32[1] = cpu_to_le32(from->u32[1]);
	memset(to + 8, 0, UBIFS_MAX_KEY_LEN - 8);
}

/**
 * key_write_idx - transform a key from in-memory format for the index.
 * @c: UBIFS file-system description object
 * @from: the key to transform
 * @to: the key to store the result
 */
static inline void key_write_idx(const union ubifs_key *from, void *to)
{
	union ubifs_key *t = to;

	t->j32[0] = cpu_to_le32(from->u32[0]);
	t->j32[1] = cpu_to_le32(from->u32[1]);
}

/**
 * keys_cmp - compare keys.
 * @c: UBIFS file-system description object
 * @key1: the first key to compare
 * @key2: the second key to compare
 *
 * This function compares 2 keys and returns %-1 if @key1 is less than
 * @key2, 0 if the keys are equivalent and %1 if @key1 is greater than @key2.
 */
static inline int keys_cmp(const union ubifs_key *key1,
			   const union ubifs_key *key2)
{
	if (key1->u32[0] < key2->u32[0])
		return -1;
	if (key1->u32[0] > key2->u32[0])
		return 1;
	if (key1->u32[1] < key2->u32[1])
		return -1;
	if (key1->u32[1] > key2->u32[1])
		return 1;

	return 0;
}

#endif /* !__UBIFS_KEY_H__ */
