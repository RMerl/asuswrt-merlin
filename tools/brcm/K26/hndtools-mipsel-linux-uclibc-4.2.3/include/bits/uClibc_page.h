/*  Copyright (C) 2004     Erik Andersen
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Supply an architecture specific value for PAGE_SIZE and friends.  */

#ifndef _UCLIBC_PAGE_H
#define _UCLIBC_PAGE_H

/* PAGE_SIZE of mips is sortof wierd, and depends on how the kernel
 * happens to have been configured.  It might use 4KB, 16K or 64K
 * pages.  To avoid using the current kernel configuration settings,
 * uClibc will simply use 4KB on mips and call it good. */
#if 0
#define PAGE_SHIFT	16
#define PAGE_SHIFT	14
#endif
#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#endif /* _UCLIBC_PAGE_H */
