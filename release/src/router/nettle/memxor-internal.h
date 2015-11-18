/* memxor-internal.h

   Copyright (C) 2010, 2014 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#ifndef NETTLE_MEMXOR_INTERNAL_H_INCLUDED
#define NETTLE_MEMXOR_INTERNAL_H_INCLUDED

#include "nettle-types.h"

/* The word_t type is intended to be the native word size. */
#if defined(__x86_64__) || defined(__arch64__)
/* Including on M$ windows, where unsigned long is only 32 bits */
typedef uint64_t word_t;
#else
typedef unsigned long int word_t;
#endif

#define ALIGN_OFFSET(p) ((uintptr_t) (p) % sizeof(word_t))

#ifndef WORDS_BIGENDIAN
#define MERGE(w0, sh_1, w1, sh_2) \
  (((w0) >> (sh_1)) | ((w1) << (sh_2)))
#else
#define MERGE(w0, sh_1, w1, sh_2) \
  (((w0) << (sh_1)) | ((w1) >> (sh_2)))
#endif

#ifndef WORDS_BIGENDIAN
#define READ_PARTIAL(r,p,n) do {			\
    word_t _rp_x;					\
    unsigned _rp_i;					\
    for (_rp_i = (n), _rp_x = (p)[--_rp_i]; _rp_i > 0;)	\
      _rp_x = (_rp_x << CHAR_BIT) | (p)[--_rp_i];	\
    (r) = _rp_x;					\
  } while (0)
#else
#define READ_PARTIAL(r,p,n) do {			\
    word_t _rp_x;						\
    unsigned _rp_i;						\
    for (_rp_x = (p)[0], _rp_i = 1; _rp_i < (n); _rp_i++)	\
      _rp_x = (_rp_x << CHAR_BIT) | (p)[_rp_i];			\
    (r) = _rp_x;						\
  } while (0)
#endif

#endif /* NETTLE_MEMXOR_INTERNAL_H_INCLUDED */
