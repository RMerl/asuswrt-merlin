/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _u2ec_list_h_
#define _u2ec_list_h_

#ifdef __cplusplus
extern "C" {
#endif

struct u2ec_list_head
{
	struct u2ec_list_head *next, *prev;
};

#define U2EC_LIST_HEAD(_name) struct u2ec_list_head _name = {&_name, &_name}
#define U2EC_INIT_LIST(_ptr) do{ (_ptr)->next = (_ptr); (_ptr)->prev = (_ptr); }while(0)

#define U2EC_LIST_ADD(_new, _prev, _next) do{	\
	(_new)->prev = (_prev);	\
	(_new)->next = (_next);	\
	(_prev)->next = (_new);	\
	(_next)->prev = (_new); \
}while(0)

#define U2EC_LIST_DEL(_del) do{			\
	(_del)->prev->next = (_del)->next;	\
	(_del)->next->prev = (_del)->prev;	\
	(_del)->prev = (struct u2ec_list_head *)0;\
	(_del)->next = (struct u2ec_list_head *)0;\
}while(0)

#define u2ec_list_for_each(_pos, _head)	\
	for (_pos = (_head)->next;	\
		_pos != (_head);	\
		_pos = (_pos)->next)

#define u2ec_list_for_each_safe(_pos, n, _head)	\
	for (_pos = (_head)->next, n = (_pos)->next;	\
		_pos != (_head);	\
		_pos = n, n = (_pos)->next)

static void inline u2ec_list_add_tail(struct u2ec_list_head *tail, struct u2ec_list_head *head)
{
	U2EC_LIST_ADD(tail, head->prev, head);
}

static void inline u2ec_list_del(struct u2ec_list_head *del)
{
	U2EC_LIST_DEL(del);
}

static int inline u2ec_list_empty(struct u2ec_list_head *h)
{
	return h->next == h;
}

#ifdef __cplusplus
}
#endif

#endif
