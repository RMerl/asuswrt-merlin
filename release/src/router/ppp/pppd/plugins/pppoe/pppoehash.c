/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <pppoe.h>


#define PPPOE_HASH_SIZE 16


static inline int keycmp(char *a, char *b, int x, int y){
    return x==y && !memcmp(a,b,x);
}

static int hash_con(int key_len, char* key)
{
    int i = 0;
    char hash[sizeof(int)]={0,};

    for (i = 0; i < key_len ; ++i)
	hash[i% sizeof(int)] = hash[i%sizeof(int)] ^ key[i];

    i = (*((int*)hash)) ;
    i &= PPPOE_HASH_SIZE - 1;

    return i;
}	

static struct pppoe_con *con_ht[PPPOE_HASH_SIZE] = { 0, };

struct pppoe_con *get_con(int len, char *key)
{
    int hash = hash_con(len, key);
    struct pppoe_con *ret;

    ret = con_ht[hash];

    while (ret && !keycmp(ret->key,key, ret->key_len, len))
	ret = ret->next;

    return ret;
}

int store_con(struct pppoe_con *pc)
{
    int hash = hash_con(pc->key_len, pc->key);
    struct pppoe_con *ret;

    ret = con_ht[hash];
    while (ret) {
	if (!keycmp(ret->key, pc->key, ret->key_len, pc->key_len))
	    return -EALREADY;
	
	ret = ret->next;
    }

    if (!ret) {
	pc->next = con_ht[hash];
	con_ht[hash] = pc;
    }

    return 0;
}

struct pppoe_con *delete_con(unsigned long len, char *key)
{
    int hash = hash_con(len, key);
    struct pppoe_con *ret, **src;

    ret = con_ht[hash];
    src = &con_ht[hash];

    while (ret) {
	if (keycmp(ret->key,key, ret->key_len, len)) {
	    *src = ret->next;
	    break;
	}
	
	src = &ret->next;
	ret = ret->next;
    }

    return ret;
}

