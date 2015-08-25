/* $Id: hash_test.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/hash.h>
#include <pj/rand.h>
#include <pj/log.h>
#include <pj/pool.h>
#include "test.h"

#if INCLUDE_HASH_TEST

#define HASH_COUNT  31

static int hash_test_with_key(pj_pool_t *pool, unsigned char key)
{
    pj_hash_table_t *ht;
    unsigned value = 0x12345;
    pj_hash_iterator_t it_buf, *it;
    unsigned *entry;

    ht = pj_hash_create(pool, HASH_COUNT);
    if (!ht)
	return -10;

    pj_hash_set(pool, ht, &key, sizeof(key), 0, &value);

    entry = (unsigned*) pj_hash_get(ht, &key, sizeof(key), NULL);
    if (!entry)
	return -20;

    if (*entry != value)
	return -30;

    if (pj_hash_count(ht) != 1)
	return -30;

    it = pj_hash_first(ht, &it_buf);
    if (it == NULL)
	return -40;

    entry = (unsigned*) pj_hash_this(ht, it);
    if (!entry)
	return -50;

    if (*entry != value)
	return -60;

    it = pj_hash_next(ht, it);
    if (it != NULL)
	return -70;

    /* Erase item */

    pj_hash_set(NULL, ht, &key, sizeof(key), 0, NULL);

    if (pj_hash_get(ht, &key, sizeof(key), NULL) != NULL)
	return -80;

    if (pj_hash_count(ht) != 0)
	return -90;

    it = pj_hash_first(ht, &it_buf);
    if (it != NULL)
	return -100;

    return 0;
}


static int hash_collision_test(pj_pool_t *pool)
{
    enum {
	COUNT = HASH_COUNT * 4
    };
    pj_hash_table_t *ht;
    pj_hash_iterator_t it_buf, *it;
    unsigned char *values;
    unsigned i;

    ht = pj_hash_create(pool, HASH_COUNT);
    if (!ht)
	return -200;

    values = (unsigned char*) pj_pool_alloc(pool, COUNT);

    for (i=0; i<COUNT; ++i) {
	values[i] = (unsigned char)i;
	pj_hash_set(pool, ht, &i, sizeof(i), 0, &values[i]);
    }

    if (pj_hash_count(ht) != COUNT)
	return -210;

    for (i=0; i<COUNT; ++i) {
	unsigned char *entry;
	entry = (unsigned char*) pj_hash_get(ht, &i, sizeof(i), NULL);
	if (!entry)
	    return -220;
	if (*entry != values[i])
	    return -230;
    }

    i = 0;
    it = pj_hash_first(ht, &it_buf);
    while (it) {
	++i;
	it = pj_hash_next(ht, it);
    }

    if (i != COUNT)
	return -240;

    return 0;
}


/*
 * Hash table test.
 */
int hash_test(void)
{
    pj_pool_t *pool = pj_pool_create(mem, "hash", 512, 512, NULL);
    int rc;
    unsigned i;

    /* Test to fill in each row in the table */
    for (i=0; i<=HASH_COUNT; ++i) {
	rc = hash_test_with_key(pool, (unsigned char)i);
	if (rc != 0) {
	    pj_pool_release(pool);
	    return rc;
	}
    }

    /* Collision test */
    rc = hash_collision_test(pool);
    if (rc != 0) {
	pj_pool_release(pool);
	return rc;
    }

    pj_pool_release(pool);
    return 0;
}

#endif	/* INCLUDE_HASH_TEST */

