/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SAMBA_MEMORY_H_
#define _SAMBA_MEMORY_H_

#ifndef SAFE_FREE /* Oh no this is also defined in tdb.h */
/**
 * Free memory if the pointer and zero the pointer.
 *
 * @note You are explicitly allowed to pass NULL pointers -- they will
 * always be ignored.
 **/
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)
#endif

/** 
 * Type-safe version of malloc. Allocated one copy of the 
 * specified data type.
 */
#define malloc_p(type) (type *)malloc(sizeof(type))

/**
 * Allocate an array of elements of one data type. Does type-checking.
 */
#define malloc_array_p(type, count) (type *)realloc_array(NULL, sizeof(type), count, false)

/** 
 * Resize an array of elements of one data type. Does type-checking.
 */
#define realloc_p(p, type, count) (type *)realloc_array(p, sizeof(type), count, false)

/** 
 * zero a structure 
 */
#ifndef ZERO_STRUCT
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))
#endif

/** 
 * zero a structure given a pointer to the structure 
 */
#ifndef ZERO_STRUCTP
#define ZERO_STRUCTP(x) do { if ((x) != NULL) memset((char *)(x), 0, sizeof(*(x))); } while(0)
#endif

/** 
 * zero a structure given a pointer to the structure - no zero check 
 */
#ifndef ZERO_STRUCTPN
#define ZERO_STRUCTPN(x) memset((char *)(x), 0, sizeof(*(x)))
#endif

/* zero an array - note that sizeof(array) must work - ie. it must not be a
   pointer */
#ifndef ZERO_ARRAY
#define ZERO_ARRAY(x) memset((char *)(x), 0, sizeof(x))
#endif

/**
 * work out how many elements there are in a static array 
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

/** 
 * pointer difference macro 
 */
#ifndef PTR_DIFF
#define PTR_DIFF(p1,p2) ((ptrdiff_t)(((const char *)(p1)) - (const char *)(p2)))
#endif

/**
  this is a warning hack. The idea is to use this everywhere that we
  get the "discarding const" warning from gcc. That doesn't actually
  fix the problem of course, but it means that when we do get to
  cleaning them up we can do it by searching the code for
  discard_const.

  It also means that other error types aren't as swamped by the noise
  of hundreds of const warnings, so we are more likely to notice when
  we get new errors.

  Please only add more uses of this macro when you find it
  _really_ hard to fix const warnings. Our aim is to eventually use
  this function in only a very few places.

  Also, please call this via the discard_const_p() macro interface, as that
  makes the return type safe.
*/
#ifndef discard_const
#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))
#endif

/** Type-safe version of discard_const */
#ifndef discard_const_p
#define discard_const_p(type, ptr) ((type *)discard_const(ptr))
#endif

#endif /* _SAMBA_MEMORY_H_ */
