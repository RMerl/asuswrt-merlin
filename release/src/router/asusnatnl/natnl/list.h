/*
 * Project: udptunnel
 * File: list.h
 *
 * Copyright (C) 2009 Daniel Meekins
 * Contact: dmeekins - gmail
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIST_H
#define LIST_H

//#include "common.h"

#ifdef WIN32
#define _inline_ __inline
#else
#define _inline_ inline
#endif

#include <pj/types.h>


#define LIST_INIT_SIZE 10 /* Start off an array with 10 elements */

typedef struct list {
    void **obj_arr; /* Array of pointers to each object */
    size_t obj_sz;  /* Number of bytes each individual objects takes up */
    int num_objs;   /* Number of object pointers in the array */
    int length;     /* Actual length of the pointer array */
    int sort;       /* 0 - don't sort, 1 - keep sorted */
    /* Function pointers to use for specific type of data types */
    int (*obj_cmp)(const void *, const void *, size_t);
    void* (*obj_copy)(void *, const void *, size_t);
    void (*obj_free)(void **);
	
	// +Roger - UDT disconnect mutex
	pj_mutex_t *disconn_lock;

} list_t;

#define LIST_LEN(l) ((l)->num_objs)

list_t *list_create(int inst_id, 
					int obj_sz,
                    int (*obj_cmp)(const void *, const void *, size_t),
                    void* (*obj_copy)(void *, const void *, size_t),
					void (*obj_free)(void **), int sort);
list_t *list_create2(int inst_id, 
					 int obj_sz,
					int (*obj_cmp)(const void *, const void *, size_t),
					void* (*obj_copy)(void *, const void *, size_t),
					void (*obj_free)(void **), int sort, int init_size);
void *list_add(list_t *list, void *obj, int copy);
void *list_add2(list_t *list, void *obj, int copy, int check_exists);
void *list_get(list_t *list, void *obj);
void *list_get_at(list_t *list, int i);
int list_get_index(list_t *list, void *obj);
list_t *list_copy(int inst_id, list_t *src);
void list_action(list_t *list, void (*action)(void *));
void list_delete(list_t *list, void *obj);
void list_delete_at(list_t *list, int i);
void list_free(list_t **list);

static _inline_ int int_cmp(int *i, int *j, size_t sz)
{
    return *i - *j;
}

#define p_int_cmp ((int (*)(const void *, const void *, size_t))&int_cmp)

#endif /* LIST_H */

