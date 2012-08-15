/*
 * support.h - Various useful things. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c)      2006 Szabolcs Szakacsits
 * Copyright (c)      2006 Yura Pakhuchiy
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_SUPPORT_H
#define _NTFS_SUPPORT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "logging.h"

/*
 * Our mailing list. Use this define to prevent typos in email address.
 */
#define NTFS_DEV_LIST	"linux-ntfs-dev@lists.sf.net"

/*
 * Generic macro to convert pointers to values for comparison purposes.
 */
#ifndef p2n
#define p2n(p)		((ptrdiff_t)((ptrdiff_t*)(p)))
#endif

/*
 * The classic min and max macros.
 */
#ifndef min
#define min(a,b)	((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)	((a) >= (b) ? (a) : (b))
#endif

/*
 * Useful macro for determining the offset of a struct member.
 */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * Round up and down @num to 2 in power of @order.
 */
#define ROUND_UP(num,order)	(((num) + ((1 << (order)) - 1)) & \
				~((1 << (order)) - 1))
#define ROUND_DOWN(num,order)	((num) & ~((1 << (order)) - 1))

/*
 * Simple bit operation macros. NOTE: These are NOT atomic.
 */
#define test_bit(bit, var)	      ((var) & (1 << (bit)))
#define set_bit(bit, var)	      (var) |= 1 << (bit)
#define clear_bit(bit, var)	      (var) &= ~(1 << (bit))

#define test_and_set_bit(bit, var)			\
({							\
	const BOOL old_state = test_bit(bit, var);	\
	set_bit(bit, var);				\
	old_state;					\
})

#define test_and_clear_bit(bit, var)			\
({							\
	const BOOL old_state = test_bit(bit, var);	\
	clear_bit(bit, var);				\
	old_state;					\
})

/* Memory allocation with logging. */
extern void *ntfs_calloc(size_t size);
extern void *ntfs_malloc(size_t size);

#endif /* defined _NTFS_SUPPORT_H */
