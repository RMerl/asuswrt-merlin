/* Generic definitions for spinlock initializers.
   Copyright (C) 2000, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Initial value of a spinlock.  Most platforms should use zero,
   unless they only implement a "test and clear" operation instead of
   the usual "test and set". */
#define __LT_SPINLOCK_INIT 0

/* Macros for lock initializers, using the above definition. */
#define __LOCK_INITIALIZER { 0, __LT_SPINLOCK_INIT }
#define __ALT_LOCK_INITIALIZER { 0, __LT_SPINLOCK_INIT }
#define __ATOMIC_INITIALIZER { 0, __LT_SPINLOCK_INIT }
