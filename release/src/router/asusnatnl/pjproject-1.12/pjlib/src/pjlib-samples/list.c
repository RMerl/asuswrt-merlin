/* $Id: list.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/list.h>
#include <pj/assert.h>
#include <pj/log.h>

/**
 * \page page_pjlib_samples_list_c Example: List Manipulation
 *
 * Below is sample program to demonstrate how to manipulate linked list.
 *
 * \includelineno pjlib-samples/list.c
 */

struct my_node
{
    // This must be the first member declared in the struct!
    PJ_DECL_LIST_MEMBER(struct my_node);
    int value;
};


int main()
{
    struct my_node nodes[10];
    struct my_node list;
    struct my_node *it;
    int i;
    
    // Initialize the list as empty.
    pj_list_init(&list);
    
    // Insert nodes.
    for (i=0; i<10; ++i) {
        nodes[i].value = i;
        pj_list_insert_before(&list, &nodes[i]);
    }
    
    // Iterate list nodes.
    it = list.next;
    while (it != &list) {
        PJ_LOG(3,("list", "value = %d", it->value));
        it = it->next;
    }
    
    // Erase all nodes.
    for (i=0; i<10; ++i) {
        pj_list_erase(&nodes[i]);
    }
    
    // List must be empty by now.
    pj_assert( pj_list_empty(&list) );
    
    return 0;
};
