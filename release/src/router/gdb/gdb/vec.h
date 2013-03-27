/* Vector API for GDB.
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Nathan Sidwell <nathan@codesourcery.com>

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#if !defined (GDB_VEC_H)
#define GDB_VEC_H

#include <stddef.h>
#include "gdb_string.h"
#include "gdb_assert.h"

/* The macros here implement a set of templated vector types and
   associated interfaces.  These templates are implemented with
   macros, as we're not in C++ land.  The interface functions are
   typesafe and use static inline functions, sometimes backed by
   out-of-line generic functions.

   Because of the different behavior of structure objects, scalar
   objects and of pointers, there are three flavors, one for each of
   these variants.  Both the structure object and pointer variants
   pass pointers to objects around -- in the former case the pointers
   are stored into the vector and in the latter case the pointers are
   dereferenced and the objects copied into the vector.  The scalar
   object variant is suitable for int-like objects, and the vector
   elements are returned by value.

   There are both 'index' and 'iterate' accessors.  The iterator
   returns a boolean iteration condition and updates the iteration
   variable passed by reference.  Because the iterator will be
   inlined, the address-of can be optimized away.

   The vectors are implemented using the trailing array idiom, thus
   they are not resizeable without changing the address of the vector
   object itself.  This means you cannot have variables or fields of
   vector type -- always use a pointer to a vector.  The one exception
   is the final field of a structure, which could be a vector type.
   You will have to use the embedded_size & embedded_init calls to
   create such objects, and they will probably not be resizeable (so
   don't use the 'safe' allocation variants).  The trailing array
   idiom is used (rather than a pointer to an array of data), because,
   if we allow NULL to also represent an empty vector, empty vectors
   occupy minimal space in the structure containing them.

   Each operation that increases the number of active elements is
   available in 'quick' and 'safe' variants.  The former presumes that
   there is sufficient allocated space for the operation to succeed
   (it dies if there is not).  The latter will reallocate the
   vector, if needed.  Reallocation causes an exponential increase in
   vector size.  If you know you will be adding N elements, it would
   be more efficient to use the reserve operation before adding the
   elements with the 'quick' operation.  This will ensure there are at
   least as many elements as you ask for, it will exponentially
   increase if there are too few spare slots.  If you want reserve a
   specific number of slots, but do not want the exponential increase
   (for instance, you know this is the last allocation), use a
   negative number for reservation.  You can also create a vector of a
   specific size from the get go.

   You should prefer the push and pop operations, as they append and
   remove from the end of the vector. If you need to remove several
   items in one go, use the truncate operation.  The insert and remove
   operations allow you to change elements in the middle of the
   vector.  There are two remove operations, one which preserves the
   element ordering 'ordered_remove', and one which does not
   'unordered_remove'.  The latter function copies the end element
   into the removed slot, rather than invoke a memmove operation.  The
   'lower_bound' function will determine where to place an item in the
   array using insert that will maintain sorted order.

   If you need to directly manipulate a vector, then the 'address'
   accessor will return the address of the start of the vector.  Also
   the 'space' predicate will tell you whether there is spare capacity
   in the vector.  You will not normally need to use these two functions.

   Vector types are defined using a DEF_VEC_{O,P,I}(TYPEDEF) macro.
   Variables of vector type are declared using a VEC(TYPEDEF) macro.
   The characters O, P and I indicate whether TYPEDEF is a pointer
   (P), object (O) or integral (I) type.  Be careful to pick the
   correct one, as you'll get an awkward and inefficient API if you
   use the wrong one.  There is a check, which results in a
   compile-time warning, for the P and I versions, but there is no
   check for the O versions, as that is not possible in plain C.

   An example of their use would be,

   DEF_VEC_P(tree);   // non-managed tree vector.

   struct my_struct {
     VEC(tree) *v;      // A (pointer to) a vector of tree pointers.
   };

   struct my_struct *s;

   if (VEC_length(tree, s->v)) { we have some contents }
   VEC_safe_push(tree, s->v, decl); // append some decl onto the end
   for (ix = 0; VEC_iterate(tree, s->v, ix, elt); ix++)
     { do something with elt }

*/

/* Macros to invoke API calls.  A single macro works for both pointer
   and object vectors, but the argument and return types might well be
   different.  In each macro, T is the typedef of the vector elements.
   Some of these macros pass the vector, V, by reference (by taking
   its address), this is noted in the descriptions.  */

/* Length of vector
   unsigned VEC_T_length(const VEC(T) *v);

   Return the number of active elements in V.  V can be NULL, in which
   case zero is returned.  */

#define VEC_length(T,V)	(VEC_OP(T,length)(V))


/* Check if vector is empty
   int VEC_T_empty(const VEC(T) *v);

   Return nonzero if V is an empty vector (or V is NULL), zero otherwise.  */

#define VEC_empty(T,V)	(VEC_length (T,V) == 0)


/* Get the final element of the vector.
   T VEC_T_last(VEC(T) *v); // Integer
   T VEC_T_last(VEC(T) *v); // Pointer
   T *VEC_T_last(VEC(T) *v); // Object

   Return the final element.  V must not be empty.  */

#define VEC_last(T,V)	(VEC_OP(T,last)(V VEC_ASSERT_INFO))

/* Index into vector
   T VEC_T_index(VEC(T) *v, unsigned ix); // Integer
   T VEC_T_index(VEC(T) *v, unsigned ix); // Pointer
   T *VEC_T_index(VEC(T) *v, unsigned ix); // Object

   Return the IX'th element.  If IX must be in the domain of V.  */

#define VEC_index(T,V,I) (VEC_OP(T,index)(V,I VEC_ASSERT_INFO))

/* Iterate over vector
   int VEC_T_iterate(VEC(T) *v, unsigned ix, T &ptr); // Integer
   int VEC_T_iterate(VEC(T) *v, unsigned ix, T &ptr); // Pointer
   int VEC_T_iterate(VEC(T) *v, unsigned ix, T *&ptr); // Object

   Return iteration condition and update PTR to point to the IX'th
   element.  At the end of iteration, sets PTR to NULL.  Use this to
   iterate over the elements of a vector as follows,

     for (ix = 0; VEC_iterate(T,v,ix,ptr); ix++)
       continue;  */

#define VEC_iterate(T,V,I,P)	(VEC_OP(T,iterate)(V,I,&(P)))

/* Allocate new vector.
   VEC(T,A) *VEC_T_alloc(int reserve);

   Allocate a new vector with space for RESERVE objects.  If RESERVE
   is zero, NO vector is created.  */

#define VEC_alloc(T,N)	(VEC_OP(T,alloc)(N))

/* Free a vector.
   void VEC_T_free(VEC(T,A) *&);

   Free a vector and set it to NULL.  */

#define VEC_free(T,V)	(VEC_OP(T,free)(&V))

/* Use these to determine the required size and initialization of a
   vector embedded within another structure (as the final member).

   size_t VEC_T_embedded_size(int reserve);
   void VEC_T_embedded_init(VEC(T) *v, int reserve);

   These allow the caller to perform the memory allocation.  */

#define VEC_embedded_size(T,N)	 (VEC_OP(T,embedded_size)(N))
#define VEC_embedded_init(T,O,N) (VEC_OP(T,embedded_init)(VEC_BASE(O),N))

/* Copy a vector.
   VEC(T,A) *VEC_T_copy(VEC(T) *);

   Copy the live elements of a vector into a new vector.  The new and
   old vectors need not be allocated by the same mechanism.  */

#define VEC_copy(T,V) (VEC_OP(T,copy)(V))

/* Determine if a vector has additional capacity.

   int VEC_T_space (VEC(T) *v,int reserve)

   If V has space for RESERVE additional entries, return nonzero.  You
   usually only need to use this if you are doing your own vector
   reallocation, for instance on an embedded vector.  This returns
   nonzero in exactly the same circumstances that VEC_T_reserve
   will.  */

#define VEC_space(T,V,R) (VEC_OP(T,space)(V,R VEC_ASSERT_INFO))

/* Reserve space.
   int VEC_T_reserve(VEC(T,A) *&v, int reserve);

   Ensure that V has at least abs(RESERVE) slots available.  The
   signedness of RESERVE determines the reallocation behavior.  A
   negative value will not create additional headroom beyond that
   requested.  A positive value will create additional headroom.  Note
   this can cause V to be reallocated.  Returns nonzero iff
   reallocation actually occurred.  */

#define VEC_reserve(T,V,R) (VEC_OP(T,reserve)(&(V),R VEC_ASSERT_INFO))

/* Push object with no reallocation
   T *VEC_T_quick_push (VEC(T) *v, T obj); // Integer
   T *VEC_T_quick_push (VEC(T) *v, T obj); // Pointer
   T *VEC_T_quick_push (VEC(T) *v, T *obj); // Object

   Push a new element onto the end, returns a pointer to the slot
   filled in. For object vectors, the new value can be NULL, in which
   case NO initialization is performed.  There must
   be sufficient space in the vector.  */

#define VEC_quick_push(T,V,O) (VEC_OP(T,quick_push)(V,O VEC_ASSERT_INFO))

/* Push object with reallocation
   T *VEC_T_safe_push (VEC(T,A) *&v, T obj); // Integer
   T *VEC_T_safe_push (VEC(T,A) *&v, T obj); // Pointer
   T *VEC_T_safe_push (VEC(T,A) *&v, T *obj); // Object

   Push a new element onto the end, returns a pointer to the slot
   filled in. For object vectors, the new value can be NULL, in which
   case NO initialization is performed.  Reallocates V, if needed.  */

#define VEC_safe_push(T,V,O) (VEC_OP(T,safe_push)(&(V),O VEC_ASSERT_INFO))

/* Pop element off end
   T VEC_T_pop (VEC(T) *v);		// Integer
   T VEC_T_pop (VEC(T) *v);		// Pointer
   void VEC_T_pop (VEC(T) *v);		// Object

   Pop the last element off the end. Returns the element popped, for
   pointer vectors.  */

#define VEC_pop(T,V)	(VEC_OP(T,pop)(V VEC_ASSERT_INFO))

/* Truncate to specific length
   void VEC_T_truncate (VEC(T) *v, unsigned len);

   Set the length as specified.  The new length must be less than or
   equal to the current length.  This is an O(1) operation.  */

#define VEC_truncate(T,V,I)		\
	(VEC_OP(T,truncate)(V,I VEC_ASSERT_INFO))

/* Grow to a specific length.
   void VEC_T_safe_grow (VEC(T,A) *&v, int len);

   Grow the vector to a specific length.  The LEN must be as
   long or longer than the current length.  The new elements are
   uninitialized.  */

#define VEC_safe_grow(T,V,I)		\
	(VEC_OP(T,safe_grow)(&(V),I VEC_ASSERT_INFO))

/* Replace element
   T VEC_T_replace (VEC(T) *v, unsigned ix, T val); // Integer
   T VEC_T_replace (VEC(T) *v, unsigned ix, T val); // Pointer
   T *VEC_T_replace (VEC(T) *v, unsigned ix, T *val);  // Object

   Replace the IXth element of V with a new value, VAL.  For pointer
   vectors returns the original value. For object vectors returns a
   pointer to the new value.  For object vectors the new value can be
   NULL, in which case no overwriting of the slot is actually
   performed.  */

#define VEC_replace(T,V,I,O) (VEC_OP(T,replace)(V,I,O VEC_ASSERT_INFO))

/* Insert object with no reallocation
   T *VEC_T_quick_insert (VEC(T) *v, unsigned ix, T val); // Integer
   T *VEC_T_quick_insert (VEC(T) *v, unsigned ix, T val); // Pointer
   T *VEC_T_quick_insert (VEC(T) *v, unsigned ix, T *val); // Object

   Insert an element, VAL, at the IXth position of V. Return a pointer
   to the slot created.  For vectors of object, the new value can be
   NULL, in which case no initialization of the inserted slot takes
   place. There must be sufficient space.  */

#define VEC_quick_insert(T,V,I,O) \
	(VEC_OP(T,quick_insert)(V,I,O VEC_ASSERT_INFO))

/* Insert object with reallocation
   T *VEC_T_safe_insert (VEC(T,A) *&v, unsigned ix, T val); // Integer
   T *VEC_T_safe_insert (VEC(T,A) *&v, unsigned ix, T val); // Pointer
   T *VEC_T_safe_insert (VEC(T,A) *&v, unsigned ix, T *val); // Object

   Insert an element, VAL, at the IXth position of V. Return a pointer
   to the slot created.  For vectors of object, the new value can be
   NULL, in which case no initialization of the inserted slot takes
   place. Reallocate V, if necessary.  */

#define VEC_safe_insert(T,V,I,O)	\
	(VEC_OP(T,safe_insert)(&(V),I,O VEC_ASSERT_INFO))

/* Remove element retaining order
   T VEC_T_ordered_remove (VEC(T) *v, unsigned ix); // Integer
   T VEC_T_ordered_remove (VEC(T) *v, unsigned ix); // Pointer
   void VEC_T_ordered_remove (VEC(T) *v, unsigned ix); // Object

   Remove an element from the IXth position of V. Ordering of
   remaining elements is preserved.  For pointer vectors returns the
   removed object.  This is an O(N) operation due to a memmove.  */

#define VEC_ordered_remove(T,V,I)	\
	(VEC_OP(T,ordered_remove)(V,I VEC_ASSERT_INFO))

/* Remove element destroying order
   T VEC_T_unordered_remove (VEC(T) *v, unsigned ix); // Integer
   T VEC_T_unordered_remove (VEC(T) *v, unsigned ix); // Pointer
   void VEC_T_unordered_remove (VEC(T) *v, unsigned ix); // Object

   Remove an element from the IXth position of V. Ordering of
   remaining elements is destroyed.  For pointer vectors returns the
   removed object.  This is an O(1) operation.  */

#define VEC_unordered_remove(T,V,I)	\
	(VEC_OP(T,unordered_remove)(V,I VEC_ASSERT_INFO))

/* Remove a block of elements
   void VEC_T_block_remove (VEC(T) *v, unsigned ix, unsigned len);

   Remove LEN elements starting at the IXth.  Ordering is retained.
   This is an O(1) operation.  */

#define VEC_block_remove(T,V,I,L)	\
	(VEC_OP(T,block_remove)(V,I,L) VEC_ASSERT_INFO)

/* Get the address of the array of elements
   T *VEC_T_address (VEC(T) v)

   If you need to directly manipulate the array (for instance, you
   want to feed it to qsort), use this accessor.  */

#define VEC_address(T,V)		(VEC_OP(T,address)(V))

/* Find the first index in the vector not less than the object.
   unsigned VEC_T_lower_bound (VEC(T) *v, const T val,
                               int (*lessthan) (const T, const T)); // Integer
   unsigned VEC_T_lower_bound (VEC(T) *v, const T val,
                               int (*lessthan) (const T, const T)); // Pointer
   unsigned VEC_T_lower_bound (VEC(T) *v, const T *val,
                               int (*lessthan) (const T*, const T*)); // Object

   Find the first position in which VAL could be inserted without
   changing the ordering of V.  LESSTHAN is a function that returns
   true if the first argument is strictly less than the second.  */

#define VEC_lower_bound(T,V,O,LT)    \
       (VEC_OP(T,lower_bound)(V,O,LT VEC_ASSERT_INFO))

/* Reallocate an array of elements with prefix.  */
extern void *vec_p_reserve (void *, int);
extern void *vec_o_reserve (void *, int, size_t, size_t);
#define vec_free_(V) xfree (V)

#define VEC_ASSERT_INFO ,__FILE__,__LINE__
#define VEC_ASSERT_DECL ,const char *file_,unsigned line_
#define VEC_ASSERT_PASS ,file_,line_
#define vec_assert(expr, op) \
  ((void)((expr) ? 0 : (gdb_assert_fail (op, file_, line_, ASSERT_FUNCTION), 0)))

#define VEC(T) VEC_##T
#define VEC_OP(T,OP) VEC_##T##_##OP

#define VEC_T(T)							  \
typedef struct VEC(T)							  \
{									  \
  unsigned num;								  \
  unsigned alloc;							  \
  T vec[1];								  \
} VEC(T)

/* Vector of integer-like object.  */
#define DEF_VEC_I(T)							  \
static inline void VEC_OP (T,must_be_integral_type) (void)		  \
{									  \
  (void)~(T)0;								  \
}									  \
									  \
VEC_T(T);								  \
DEF_VEC_FUNC_P(T)							  \
DEF_VEC_ALLOC_FUNC_I(T)							  \
struct vec_swallow_trailing_semi

/* Vector of pointer to object.  */
#define DEF_VEC_P(T)							  \
static inline void VEC_OP (T,must_be_pointer_type) (void)		  \
{									  \
  (void)((T)1 == (void *)1);						  \
}									  \
									  \
VEC_T(T);								  \
DEF_VEC_FUNC_P(T)							  \
DEF_VEC_ALLOC_FUNC_P(T)							  \
struct vec_swallow_trailing_semi

/* Vector of object.  */
#define DEF_VEC_O(T)							  \
VEC_T(T);								  \
DEF_VEC_FUNC_O(T)							  \
DEF_VEC_ALLOC_FUNC_O(T)							  \
struct vec_swallow_trailing_semi

#define DEF_VEC_ALLOC_FUNC_I(T)						  \
static inline VEC(T) *VEC_OP (T,alloc)					  \
     (int alloc_)							  \
{									  \
  /* We must request exact size allocation, hence the negation.  */	  \
  return (VEC(T) *) vec_o_reserve (NULL, -alloc_,			  \
                                   offsetof (VEC(T),vec), sizeof (T));	  \
}									  \
									  \
static inline VEC(T) *VEC_OP (T,copy) (VEC(T) *vec_)			  \
{									  \
  size_t len_ = vec_ ? vec_->num : 0;					  \
  VEC (T) *new_vec_ = NULL;						  \
									  \
  if (len_)								  \
    {									  \
      /* We must request exact size allocation, hence the negation. */	  \
      new_vec_ = (VEC (T) *)						  \
	vec_o_reserve (NULL, -len_, offsetof (VEC(T),vec), sizeof (T));	  \
									  \
      new_vec_->num = len_;						  \
      memcpy (new_vec_->vec, vec_->vec, sizeof (T) * len_);		  \
    }									  \
  return new_vec_;							  \
}									  \
									  \
static inline void VEC_OP (T,free)					  \
     (VEC(T) **vec_)							  \
{									  \
  if (*vec_)								  \
    vec_free_ (*vec_);							  \
  *vec_ = NULL;								  \
}									  \
									  \
static inline int VEC_OP (T,reserve)					  \
     (VEC(T) **vec_, int alloc_ VEC_ASSERT_DECL)			  \
{									  \
  int extend = !VEC_OP (T,space)					  \
	(*vec_, alloc_ < 0 ? -alloc_ : alloc_ VEC_ASSERT_PASS);		  \
									  \
  if (extend)								  \
    *vec_ = (VEC(T) *) vec_o_reserve (*vec_, alloc_,			  \
				      offsetof (VEC(T),vec), sizeof (T)); \
									  \
  return extend;							  \
}									  \
									  \
static inline void VEC_OP (T,safe_grow)					  \
     (VEC(T) **vec_, int size_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (size_ >= 0 && VEC_OP(T,length) (*vec_) <= (unsigned)size_,  \
	"safe_grow");							  \
  VEC_OP (T,reserve) (vec_, (int)(*vec_ ? (*vec_)->num : 0) - size_	  \
			VEC_ASSERT_PASS);				  \
  (*vec_)->num = size_;							  \
}									  \
									  \
static inline T *VEC_OP (T,safe_push)					  \
     (VEC(T) **vec_, const T obj_ VEC_ASSERT_DECL)			  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_push) (*vec_, obj_ VEC_ASSERT_PASS);		  \
}									  \
									  \
static inline T *VEC_OP (T,safe_insert)					  \
     (VEC(T) **vec_, unsigned ix_, const T obj_ VEC_ASSERT_DECL)	  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_insert) (*vec_, ix_, obj_ VEC_ASSERT_PASS);	  \
}

#define DEF_VEC_FUNC_P(T)						  \
static inline unsigned VEC_OP (T,length) (const VEC(T) *vec_)		  \
{									  \
  return vec_ ? vec_->num : 0;						  \
}									  \
									  \
static inline T VEC_OP (T,last)						  \
	(const VEC(T) *vec_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (vec_ && vec_->num, "last");				  \
									  \
  return vec_->vec[vec_->num - 1];					  \
}									  \
									  \
static inline T VEC_OP (T,index)					  \
     (const VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  vec_assert (vec_ && ix_ < vec_->num, "index");			  \
									  \
  return vec_->vec[ix_];						  \
}									  \
									  \
static inline int VEC_OP (T,iterate)					  \
     (const VEC(T) *vec_, unsigned ix_, T *ptr)				  \
{									  \
  if (vec_ && ix_ < vec_->num)						  \
    {									  \
      *ptr = vec_->vec[ix_];						  \
      return 1;								  \
    }									  \
  else									  \
    {									  \
      *ptr = 0;								  \
      return 0;								  \
    }									  \
}									  \
									  \
static inline size_t VEC_OP (T,embedded_size)				  \
     (int alloc_)							  \
{									  \
  return offsetof (VEC(T),vec) + alloc_ * sizeof(T);			  \
}									  \
									  \
static inline void VEC_OP (T,embedded_init)				  \
     (VEC(T) *vec_, int alloc_)						  \
{									  \
  vec_->num = 0;							  \
  vec_->alloc = alloc_;							  \
}									  \
									  \
static inline int VEC_OP (T,space)					  \
     (VEC(T) *vec_, int alloc_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (alloc_ >= 0, "space");					  \
  return vec_ ? vec_->alloc - vec_->num >= (unsigned)alloc_ : !alloc_;	  \
}									  \
									  \
static inline T *VEC_OP (T,quick_push)					  \
     (VEC(T) *vec_, T obj_ VEC_ASSERT_DECL)				  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (vec_->num < vec_->alloc, "quick_push");			  \
  slot_ = &vec_->vec[vec_->num++];					  \
  *slot_ = obj_;							  \
									  \
  return slot_;								  \
}									  \
									  \
static inline T VEC_OP (T,pop) (VEC(T) *vec_ VEC_ASSERT_DECL)		  \
{									  \
  T obj_;								  \
									  \
  vec_assert (vec_->num, "pop");					  \
  obj_ = vec_->vec[--vec_->num];					  \
									  \
  return obj_;								  \
}									  \
									  \
static inline void VEC_OP (T,truncate)					  \
     (VEC(T) *vec_, unsigned size_ VEC_ASSERT_DECL)			  \
{									  \
  vec_assert (vec_ ? vec_->num >= size_ : !size_, "truncate");		  \
  if (vec_)								  \
    vec_->num = size_;							  \
}									  \
									  \
static inline T VEC_OP (T,replace)					  \
     (VEC(T) *vec_, unsigned ix_, T obj_ VEC_ASSERT_DECL)		  \
{									  \
  T old_obj_;								  \
									  \
  vec_assert (ix_ < vec_->num, "replace");				  \
  old_obj_ = vec_->vec[ix_];						  \
  vec_->vec[ix_] = obj_;						  \
									  \
  return old_obj_;							  \
}									  \
									  \
static inline T *VEC_OP (T,quick_insert)				  \
     (VEC(T) *vec_, unsigned ix_, T obj_ VEC_ASSERT_DECL)		  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (vec_->num < vec_->alloc && ix_ <= vec_->num, "quick_insert"); \
  slot_ = &vec_->vec[ix_];						  \
  memmove (slot_ + 1, slot_, (vec_->num++ - ix_) * sizeof (T));		  \
  *slot_ = obj_;							  \
									  \
  return slot_;								  \
}									  \
									  \
static inline T VEC_OP (T,ordered_remove)				  \
     (VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  T *slot_;								  \
  T obj_;								  \
									  \
  vec_assert (ix_ < vec_->num, "ordered_remove");			  \
  slot_ = &vec_->vec[ix_];						  \
  obj_ = *slot_;							  \
  memmove (slot_, slot_ + 1, (--vec_->num - ix_) * sizeof (T));		  \
									  \
  return obj_;								  \
}									  \
									  \
static inline T VEC_OP (T,unordered_remove)				  \
     (VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  T *slot_;								  \
  T obj_;								  \
									  \
  vec_assert (ix_ < vec_->num, "unordered_remove");			  \
  slot_ = &vec_->vec[ix_];						  \
  obj_ = *slot_;							  \
  *slot_ = vec_->vec[--vec_->num];					  \
									  \
  return obj_;								  \
}									  \
									  \
static inline void VEC_OP (T,block_remove)				  \
     (VEC(T) *vec_, unsigned ix_, unsigned len_ VEC_ASSERT_DECL)	  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (ix_ + len_ <= vec_->num, "block_remove");			  \
  slot_ = &vec_->vec[ix_];						  \
  vec_->num -= len_;							  \
  memmove (slot_, slot_ + len_, (vec_->num - ix_) * sizeof (T));	  \
}									  \
									  \
static inline T *VEC_OP (T,address)					  \
     (VEC(T) *vec_)							  \
{									  \
  return vec_ ? vec_->vec : 0;						  \
}									  \
									  \
static inline unsigned VEC_OP (T,lower_bound)				  \
     (VEC(T) *vec_, const T obj_,					  \
      int (*lessthan_)(const T, const T) VEC_ASSERT_DECL)		  \
{									  \
   unsigned int len_ = VEC_OP (T, length) (vec_);			  \
   unsigned int half_, middle_;						  \
   unsigned int first_ = 0;						  \
   while (len_ > 0)							  \
     {									  \
        T middle_elem_;							  \
        half_ = len_ >> 1;						  \
        middle_ = first_;						  \
        middle_ += half_;						  \
        middle_elem_ = VEC_OP (T,index) (vec_, middle_ VEC_ASSERT_PASS);  \
        if (lessthan_ (middle_elem_, obj_))				  \
          {								  \
             first_ = middle_;						  \
             ++first_;							  \
             len_ = len_ - half_ - 1;					  \
          }								  \
        else								  \
          len_ = half_;							  \
     }									  \
   return first_;							  \
}

#define DEF_VEC_ALLOC_FUNC_P(T)						  \
static inline VEC(T) *VEC_OP (T,alloc)					  \
     (int alloc_)							  \
{									  \
  /* We must request exact size allocation, hence the negation.  */	  \
  return (VEC(T) *) vec_p_reserve (NULL, -alloc_);			  \
}									  \
									  \
static inline void VEC_OP (T,free)					  \
     (VEC(T) **vec_)							  \
{									  \
  if (*vec_)								  \
    vec_free_ (*vec_);							  \
  *vec_ = NULL;								  \
}									  \
									  \
static inline VEC(T) *VEC_OP (T,copy) (VEC(T) *vec_)			  \
{									  \
  size_t len_ = vec_ ? vec_->num : 0;					  \
  VEC (T) *new_vec_ = NULL;						  \
									  \
  if (len_)								  \
    {									  \
      /* We must request exact size allocation, hence the negation. */	  \
      new_vec_ = (VEC (T) *)(vec_p_reserve (NULL, -len_));		  \
									  \
      new_vec_->num = len_;						  \
      memcpy (new_vec_->vec, vec_->vec, sizeof (T) * len_);		  \
    }									  \
  return new_vec_;							  \
}									  \
									  \
static inline int VEC_OP (T,reserve)					  \
     (VEC(T) **vec_, int alloc_ VEC_ASSERT_DECL)			  \
{									  \
  int extend = !VEC_OP (T,space)					  \
	(*vec_, alloc_ < 0 ? -alloc_ : alloc_ VEC_ASSERT_PASS);		  \
									  \
  if (extend)								  \
    *vec_ = (VEC(T) *) vec_p_reserve (*vec_, alloc_);			  \
									  \
  return extend;							  \
}									  \
									  \
static inline void VEC_OP (T,safe_grow)					  \
     (VEC(T) **vec_, int size_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (size_ >= 0 && VEC_OP(T,length) (*vec_) <= (unsigned)size_,  \
	"safe_grow");							  \
  VEC_OP (T,reserve)							  \
	(vec_, (int)(*vec_ ? (*vec_)->num : 0) - size_ VEC_ASSERT_PASS);  \
  (*vec_)->num = size_;							  \
}									  \
									  \
static inline T *VEC_OP (T,safe_push)					  \
     (VEC(T) **vec_, T obj_ VEC_ASSERT_DECL)				  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_push) (*vec_, obj_ VEC_ASSERT_PASS);		  \
}									  \
									  \
static inline T *VEC_OP (T,safe_insert)					  \
     (VEC(T) **vec_, unsigned ix_, T obj_ VEC_ASSERT_DECL)		  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_insert) (*vec_, ix_, obj_ VEC_ASSERT_PASS);	  \
}

#define DEF_VEC_FUNC_O(T)						  \
static inline unsigned VEC_OP (T,length) (const VEC(T) *vec_)		  \
{									  \
  return vec_ ? vec_->num : 0;						  \
}									  \
									  \
static inline T *VEC_OP (T,last) (VEC(T) *vec_ VEC_ASSERT_DECL)		  \
{									  \
  vec_assert (vec_ && vec_->num, "last");				  \
									  \
  return &vec_->vec[vec_->num - 1];					  \
}									  \
									  \
static inline T *VEC_OP (T,index)					  \
     (VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  vec_assert (vec_ && ix_ < vec_->num, "index");			  \
									  \
  return &vec_->vec[ix_];						  \
}									  \
									  \
static inline int VEC_OP (T,iterate)					  \
     (VEC(T) *vec_, unsigned ix_, T **ptr)				  \
{									  \
  if (vec_ && ix_ < vec_->num)						  \
    {									  \
      *ptr = &vec_->vec[ix_];						  \
      return 1;								  \
    }									  \
  else									  \
    {									  \
      *ptr = 0;								  \
      return 0;								  \
    }									  \
}									  \
									  \
static inline size_t VEC_OP (T,embedded_size)				  \
     (int alloc_)							  \
{									  \
  return offsetof (VEC(T),vec) + alloc_ * sizeof(T);			  \
}									  \
									  \
static inline void VEC_OP (T,embedded_init)				  \
     (VEC(T) *vec_, int alloc_)						  \
{									  \
  vec_->num = 0;							  \
  vec_->alloc = alloc_;							  \
}									  \
									  \
static inline int VEC_OP (T,space)					  \
     (VEC(T) *vec_, int alloc_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (alloc_ >= 0, "space");					  \
  return vec_ ? vec_->alloc - vec_->num >= (unsigned)alloc_ : !alloc_;	  \
}									  \
									  \
static inline T *VEC_OP (T,quick_push)					  \
     (VEC(T) *vec_, const T *obj_ VEC_ASSERT_DECL)			  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (vec_->num < vec_->alloc, "quick_push");			  \
  slot_ = &vec_->vec[vec_->num++];					  \
  if (obj_)								  \
    *slot_ = *obj_;							  \
									  \
  return slot_;								  \
}									  \
									  \
static inline void VEC_OP (T,pop) (VEC(T) *vec_ VEC_ASSERT_DECL)	  \
{									  \
  vec_assert (vec_->num, "pop");					  \
  --vec_->num;								  \
}									  \
									  \
static inline void VEC_OP (T,truncate)					  \
     (VEC(T) *vec_, unsigned size_ VEC_ASSERT_DECL)			  \
{									  \
  vec_assert (vec_ ? vec_->num >= size_ : !size_, "truncate");		  \
  if (vec_)								  \
    vec_->num = size_;							  \
}									  \
									  \
static inline T *VEC_OP (T,replace)					  \
     (VEC(T) *vec_, unsigned ix_, const T *obj_ VEC_ASSERT_DECL)	  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (ix_ < vec_->num, "replace");				  \
  slot_ = &vec_->vec[ix_];						  \
  if (obj_)								  \
    *slot_ = *obj_;							  \
									  \
  return slot_;								  \
}									  \
									  \
static inline T *VEC_OP (T,quick_insert)				  \
     (VEC(T) *vec_, unsigned ix_, const T *obj_ VEC_ASSERT_DECL)	  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (vec_->num < vec_->alloc && ix_ <= vec_->num, "quick_insert"); \
  slot_ = &vec_->vec[ix_];						  \
  memmove (slot_ + 1, slot_, (vec_->num++ - ix_) * sizeof (T));		  \
  if (obj_)								  \
    *slot_ = *obj_;							  \
									  \
  return slot_;								  \
}									  \
									  \
static inline void VEC_OP (T,ordered_remove)				  \
     (VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (ix_ < vec_->num, "ordered_remove");			  \
  slot_ = &vec_->vec[ix_];						  \
  memmove (slot_, slot_ + 1, (--vec_->num - ix_) * sizeof (T));		  \
}									  \
									  \
static inline void VEC_OP (T,unordered_remove)				  \
     (VEC(T) *vec_, unsigned ix_ VEC_ASSERT_DECL)			  \
{									  \
  vec_assert (ix_ < vec_->num, "unordered_remove");			  \
  vec_->vec[ix_] = vec_->vec[--vec_->num];				  \
}									  \
									  \
static inline void VEC_OP (T,block_remove)				  \
     (VEC(T) *vec_, unsigned ix_, unsigned len_ VEC_ASSERT_DECL)	  \
{									  \
  T *slot_;								  \
									  \
  vec_assert (ix_ + len_ <= vec_->num, "block_remove");			  \
  slot_ = &vec_->vec[ix_];						  \
  vec_->num -= len_;							  \
  memmove (slot_, slot_ + len_, (vec_->num - ix_) * sizeof (T));	  \
}									  \
									  \
static inline T *VEC_OP (T,address)					  \
     (VEC(T) *vec_)							  \
{									  \
  return vec_ ? vec_->vec : 0;						  \
}									  \
									  \
static inline unsigned VEC_OP (T,lower_bound)				  \
     (VEC(T) *vec_, const T *obj_,					  \
      int (*lessthan_)(const T *, const T *) VEC_ASSERT_DECL)		  \
{									  \
   unsigned int len_ = VEC_OP (T, length) (vec_);			  \
   unsigned int half_, middle_;						  \
   unsigned int first_ = 0;						  \
   while (len_ > 0)							  \
     {									  \
        T *middle_elem_;						  \
        half_ = len_ >> 1;						  \
        middle_ = first_;						  \
        middle_ += half_;						  \
        middle_elem_ = VEC_OP (T,index) (vec_, middle_ VEC_ASSERT_PASS);  \
        if (lessthan_ (middle_elem_, obj_))				  \
          {								  \
             first_ = middle_;						  \
             ++first_;							  \
             len_ = len_ - half_ - 1;					  \
          }								  \
        else								  \
          len_ = half_;							  \
     }									  \
   return first_;							  \
}

#define DEF_VEC_ALLOC_FUNC_O(T)						  \
static inline VEC(T) *VEC_OP (T,alloc)					  \
     (int alloc_)							  \
{									  \
  /* We must request exact size allocation, hence the negation.  */	  \
  return (VEC(T) *) vec_o_reserve (NULL, -alloc_,			  \
                                   offsetof (VEC(T),vec), sizeof (T));	  \
}									  \
									  \
static inline VEC(T) *VEC_OP (T,copy) (VEC(T) *vec_)			  \
{									  \
  size_t len_ = vec_ ? vec_->num : 0;					  \
  VEC (T) *new_vec_ = NULL;						  \
									  \
  if (len_)								  \
    {									  \
      /* We must request exact size allocation, hence the negation. */	  \
      new_vec_ = (VEC (T) *)						  \
	vec_o_reserve  (NULL, -len_, offsetof (VEC(T),vec), sizeof (T));  \
									  \
      new_vec_->num = len_;						  \
      memcpy (new_vec_->vec, vec_->vec, sizeof (T) * len_);		  \
    }									  \
  return new_vec_;							  \
}									  \
									  \
static inline void VEC_OP (T,free)					  \
     (VEC(T) **vec_)							  \
{									  \
  if (*vec_)								  \
    vec_free_ (*vec_);							  \
  *vec_ = NULL;								  \
}									  \
									  \
static inline int VEC_OP (T,reserve)					  \
     (VEC(T) **vec_, int alloc_ VEC_ASSERT_DECL)			  \
{									  \
  int extend = !VEC_OP (T,space) (*vec_, alloc_ < 0 ? -alloc_ : alloc_	  \
				  VEC_ASSERT_PASS);			  \
									  \
  if (extend)								  \
    *vec_ = (VEC(T) *)							  \
	vec_o_reserve (*vec_, alloc_, offsetof (VEC(T),vec), sizeof (T)); \
									  \
  return extend;							  \
}									  \
									  \
static inline void VEC_OP (T,safe_grow)					  \
     (VEC(T) **vec_, int size_ VEC_ASSERT_DECL)				  \
{									  \
  vec_assert (size_ >= 0 && VEC_OP(T,length) (*vec_) <= (unsigned)size_,  \
	"safe_grow");							  \
  VEC_OP (T,reserve)							  \
	(vec_, (int)(*vec_ ? (*vec_)->num : 0) - size_ VEC_ASSERT_PASS);  \
  (*vec_)->num = size_;							  \
}									  \
									  \
static inline T *VEC_OP (T,safe_push)					  \
     (VEC(T) **vec_, const T *obj_ VEC_ASSERT_DECL)			  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_push) (*vec_, obj_ VEC_ASSERT_PASS);		  \
}									  \
									  \
static inline T *VEC_OP (T,safe_insert)					  \
     (VEC(T) **vec_, unsigned ix_, const T *obj_ VEC_ASSERT_DECL)	  \
{									  \
  VEC_OP (T,reserve) (vec_, 1 VEC_ASSERT_PASS);				  \
									  \
  return VEC_OP (T,quick_insert) (*vec_, ix_, obj_ VEC_ASSERT_PASS);	  \
}

#endif /* GDB_VEC_H */
