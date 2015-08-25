/* $Id: list_i.h 3553 2011-05-05 06:14:19Z nanang $ */
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


/* Internal */
PJ_INLINE(void) pj_link_node(pj_list_type *prev, pj_list_type *next)
{
    ((pj_list*)prev)->next = next;
    ((pj_list*)next)->prev = prev;
}

PJ_IDEF(void) pj_list_insert_after(pj_list_type *pos, pj_list_type *node)
{
    ((pj_list*)node)->prev = pos;
    ((pj_list*)node)->next = ((pj_list*)pos)->next;
    ((pj_list*) ((pj_list*)pos)->next) ->prev = node;
    ((pj_list*)pos)->next = node;
}


PJ_IDEF(void) pj_list_insert_before(pj_list_type *pos, pj_list_type *node)
{
    pj_list_insert_after(((pj_list*)pos)->prev, node);
}


PJ_IDEF(void) pj_list_insert_nodes_after(pj_list_type *pos, pj_list_type *lst)
{
    pj_list *lst_last = (pj_list *) ((pj_list*)lst)->prev;
    pj_list *pos_next = (pj_list *) ((pj_list*)pos)->next;

    pj_link_node(pos, lst);
    pj_link_node(lst_last, pos_next);
}

PJ_IDEF(void) pj_list_insert_nodes_before(pj_list_type *pos, pj_list_type *lst)
{
    pj_list_insert_nodes_after(((pj_list*)pos)->prev, lst);
}

PJ_IDEF(void) pj_list_merge_last(pj_list_type *lst1, pj_list_type *lst2)
{
    if (!pj_list_empty(lst2)) {
	pj_link_node(((pj_list*)lst1)->prev, ((pj_list*)lst2)->next);
	pj_link_node(((pj_list*)lst2)->prev, lst1);
	pj_list_init(lst2);
    }
}

PJ_IDEF(void) pj_list_merge_first(pj_list_type *lst1, pj_list_type *lst2)
{
    if (!pj_list_empty(lst2)) {
	pj_link_node(((pj_list*)lst2)->prev, ((pj_list*)lst1)->next);
	pj_link_node(((pj_list*)lst1), ((pj_list*)lst2)->next);
	pj_list_init(lst2);
    }
}

PJ_IDEF(void) pj_list_erase(pj_list_type *node)
{
    pj_link_node( ((pj_list*)node)->prev, ((pj_list*)node)->next);

    /* It'll be safer to init the next/prev fields to itself, to
     * prevent multiple erase() from corrupting the list. See
     * ticket #520 for one sample bug.
     */
    pj_list_init(node);
}


PJ_IDEF(pj_list_type*) pj_list_find_node(pj_list_type *list, pj_list_type *node)
{
    pj_list *p = (pj_list *) ((pj_list*)list)->next;
    while (p != list && p != node)
	p = (pj_list *) p->next;

    return p==node ? p : NULL;
}


PJ_IDEF(pj_list_type*) pj_list_search(pj_list_type *list, void *value,
	       		int (*comp)(void *value, const pj_list_type *node))
{
    pj_list *p = (pj_list *) ((pj_list*)list)->next;
    while (p != list && (*comp)(value, p) != 0)
	p = (pj_list *) p->next;

    return p==list ? NULL : p;
}


PJ_IDEF(pj_size_t) pj_list_size(const pj_list_type *list)
{
    const pj_list *node = (const pj_list*) ((const pj_list*)list)->next;
    pj_size_t count = 0;

    while (node != list) {
	++count;
	node = (pj_list*)node->next;
    }

    return count;
}

