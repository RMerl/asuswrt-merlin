/* Internal macros for atomic operations for GNU C Library.
   Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _ATOMIC_H
#define _ATOMIC_H	1

#include <stdlib.h>

#include <bits/atomic.h>

/* Wrapper macros to call pre_NN_post (mem, ...) where NN is the
   bit width of *MEM.  The calling macro puts parens around MEM
   and following args.  */
#define __atomic_val_bysize(pre, post, mem, ...)			      \
  ({									      \
    __typeof (*mem) __result;						      \
    if (sizeof (*mem) == 1)						      \
      __result = pre##_8_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 2)					      \
      __result = pre##_16_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 4)					      \
      __result = pre##_32_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 8)					      \
      __result = pre##_64_##post (mem, __VA_ARGS__);			      \
    else								      \
      abort ();								      \
    __result;								      \
  })
#define __atomic_bool_bysize(pre, post, mem, ...)			      \
  ({									      \
    int __result;							      \
    if (sizeof (*mem) == 1)						      \
      __result = pre##_8_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 2)					      \
      __result = pre##_16_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 4)					      \
      __result = pre##_32_##post (mem, __VA_ARGS__);			      \
    else if (sizeof (*mem) == 8)					      \
      __result = pre##_64_##post (mem, __VA_ARGS__);			      \
    else								      \
      abort ();								      \
    __result;								      \
  })


/* Atomically store NEWVAL in *MEM if *MEM is equal to OLDVAL.
   Return the old *MEM value.  */
#if !defined atomic_compare_and_exchange_val_acq \
    && defined __arch_compare_and_exchange_val_32_acq
# define atomic_compare_and_exchange_val_acq(mem, newval, oldval) \
  __atomic_val_bysize (__arch_compare_and_exchange_val,acq,		      \
		       mem, newval, oldval)
#endif


#ifndef atomic_compare_and_exchange_val_rel
# define atomic_compare_and_exchange_val_rel(mem, newval, oldval)	      \
  atomic_compare_and_exchange_val_acq (mem, newval, oldval)
#endif


/* Atomically store NEWVAL in *MEM if *MEM is equal to OLDVAL.
   Return zero if *MEM was changed or non-zero if no exchange happened.  */
#ifndef atomic_compare_and_exchange_bool_acq
# ifdef __arch_compare_and_exchange_bool_32_acq
#  define atomic_compare_and_exchange_bool_acq(mem, newval, oldval) \
  __atomic_bool_bysize (__arch_compare_and_exchange_bool,acq,		      \
		        mem, newval, oldval)
#  else
#   define atomic_compare_and_exchange_bool_acq(mem, newval, oldval) \
  ({ /* Cannot use __oldval here, because macros later in this file might     \
	call this macro with __oldval argument.	 */			      \
     __typeof (oldval) __old = (oldval);				      \
     atomic_compare_and_exchange_val_acq (mem, newval, __old) != __old;	      \
  })
# endif
#endif


#ifndef atomic_compare_and_exchange_bool_rel
# define atomic_compare_and_exchange_bool_rel(mem, newval, oldval) \
  atomic_compare_and_exchange_bool_acq (mem, newval, oldval)
#endif


/* Store NEWVALUE in *MEM and return the old value.  */
#ifndef atomic_exchange_acq
# define atomic_exchange_acq(mem, newvalue) \
  ({ __typeof (*(mem)) __oldval;					      \
     __typeof (mem) __memp = (mem);					      \
     __typeof (*(mem)) __value = (newvalue);				      \
									      \
     do									      \
       __oldval = (*__memp);						      \
     while (__builtin_expect (atomic_compare_and_exchange_bool_acq (__memp,   \
								    __value,  \
								    __oldval),\
			      0));					      \
									      \
     __oldval; })
#endif

#ifndef atomic_exchange_rel
# define atomic_exchange_rel(mem, newvalue) atomic_exchange_acq (mem, newvalue)
#endif


/* Add VALUE to *MEM and return the old value of *MEM.  */
#ifndef atomic_exchange_and_add
# define atomic_exchange_and_add(mem, value) \
  ({ __typeof (*(mem)) __oldval;					      \
     __typeof (mem) __memp = (mem);					      \
     __typeof (*(mem)) __value = (value);				      \
									      \
     do									      \
       __oldval = (*__memp);						      \
     while (__builtin_expect (atomic_compare_and_exchange_bool_acq (__memp,   \
								    __oldval  \
								    + __value,\
								    __oldval),\
			      0));					      \
									      \
     __oldval; })
#endif


#ifndef atomic_add
# define atomic_add(mem, value) (void) atomic_exchange_and_add ((mem), (value))
#endif


#ifndef atomic_increment
# define atomic_increment(mem) atomic_add ((mem), 1)
#endif


#ifndef atomic_increment_val
# define atomic_increment_val(mem) (atomic_exchange_and_add ((mem), 1) + 1)
#endif


/* Add one to *MEM and return true iff it's now zero.  */
#ifndef atomic_increment_and_test
# define atomic_increment_and_test(mem) \
  (atomic_exchange_and_add ((mem), 1) + 1 == 0)
#endif


#ifndef atomic_decrement
# define atomic_decrement(mem) atomic_add ((mem), -1)
#endif


#ifndef atomic_decrement_val
# define atomic_decrement_val(mem) (atomic_exchange_and_add ((mem), -1) - 1)
#endif


/* Subtract 1 from *MEM and return true iff it's now zero.  */
#ifndef atomic_decrement_and_test
# define atomic_decrement_and_test(mem) \
  (atomic_exchange_and_add ((mem), -1) == 1)
#endif


/* Decrement *MEM if it is > 0, and return the old value.  */
#ifndef atomic_decrement_if_positive
# define atomic_decrement_if_positive(mem) \
  ({ __typeof (*(mem)) __oldval;					      \
     __typeof (mem) __memp = (mem);					      \
									      \
     do									      \
       {								      \
	 __oldval = *__memp;						      \
	 if (__builtin_expect (__oldval <= 0, 0))			      \
	   break;							      \
       }								      \
     while (__builtin_expect (atomic_compare_and_exchange_bool_acq (__memp,   \
								    __oldval  \
								    - 1,      \
								    __oldval),\
			      0));\
     __oldval; })
#endif


#ifndef atomic_add_negative
# define atomic_add_negative(mem, value)				      \
  ({ __typeof (value) __aan_value = (value);				      \
     atomic_exchange_and_add (mem, __aan_value) < -__aan_value; })
#endif


#ifndef atomic_add_zero
# define atomic_add_zero(mem, value)					      \
  ({ __typeof (value) __aaz_value = (value);				      \
     atomic_exchange_and_add (mem, __aaz_value) == -__aaz_value; })
#endif


#ifndef atomic_bit_set
# define atomic_bit_set(mem, bit) \
  (void) atomic_bit_test_set(mem, bit)
#endif


#ifndef atomic_bit_test_set
# define atomic_bit_test_set(mem, bit) \
  ({ __typeof (*(mem)) __oldval;					      \
     __typeof (mem) __memp = (mem);					      \
     __typeof (*(mem)) __mask = ((__typeof (*(mem))) 1 << (bit));	      \
									      \
     do									      \
       __oldval = (*__memp);						      \
     while (__builtin_expect (atomic_compare_and_exchange_bool_acq (__memp,   \
								    __oldval  \
								    | __mask, \
								    __oldval),\
			      0));					      \
									      \
     __oldval & __mask; })
#endif


#ifndef atomic_full_barrier
# define atomic_full_barrier() __asm__ ("" ::: "memory")
#endif


#ifndef atomic_read_barrier
# define atomic_read_barrier() atomic_full_barrier ()
#endif


#ifndef atomic_write_barrier
# define atomic_write_barrier() atomic_full_barrier ()
#endif


#ifndef atomic_delay
# define atomic_delay() do { /* nothing */ } while (0)
#endif

#endif	/* atomic.h */
