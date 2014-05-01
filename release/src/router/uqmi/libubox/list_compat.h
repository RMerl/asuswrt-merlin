/*
 * Copyright (C) 2011-2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIST_COMPAT_H
#define __LIST_COMPAT_H

#define list_entity				list_head

#define list_init_head(_list)			INIT_LIST_HEAD(_list)
#define list_add_head(_head, _list)		list_add(_list, _head)
#define list_add_after(_after, _list)		list_add(_list, _after)
#define list_add_before(_before, _list)		list_add_tail(_list, _before)
#define list_remove(_list)			list_del(_list)
#define list_is_empty(_list)			list_empty(_list)
#define list_next_element(_element, _member)	list_entry((_element)->_member.next, typeof(*(_element)), _member)


#endif
