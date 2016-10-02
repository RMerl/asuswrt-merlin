/* trunnel.c -- copied from Trunnel v1.4.4
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
/* trunnel.c -- Helper functions to implement trunnel.
 *
 * Copyright 2014-2015, The Tor Project, Inc.
 * See license at the end of this file for copying information.
 *
 * See trunnel-impl.h for documentation of these functions.
 */

#include <stdlib.h>
#include <string.h>
#include "trunnel-impl.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
	__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define IS_LITTLE_ENDIAN 1
#elif defined(BYTE_ORDER) && defined(ORDER_LITTLE_ENDIAN) &&     \
	BYTE_ORDER == __ORDER_LITTLE_ENDIAN
#  define IS_LITTLE_ENDIAN 1
#elif defined(_WIN32)
#  define IS_LITTLE_ENDIAN 1
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define BSWAP64(x) OSSwapLittleToHostInt64(x)
#elif defined(sun) || defined(__sun)
#  include <sys/byteorder.h>
#  ifndef _BIG_ENDIAN
#    define IS_LITTLE_ENDIAN
#  endif
#else
# if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  include <sys/endian.h>
# else
#  include <endian.h>
# endif
#  if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
	__BYTE_ORDER == __LITTLE_ENDIAN
#    define IS_LITTLE_ENDIAN
#  endif
#endif

#ifdef _WIN32
uint16_t
trunnel_htons(uint16_t s)
{
  return (s << 8) | (s >> 8);
}
uint16_t
trunnel_ntohs(uint16_t s)
{
  return (s << 8) | (s >> 8);
}
uint32_t
trunnel_htonl(uint32_t s)
{
  return (s << 24) |
         ((s << 8)&0xff0000) |
         ((s >> 8)&0xff00) |
         (s >> 24);
}
uint32_t
trunnel_ntohl(uint32_t s)
{
  return (s << 24) |
         ((s << 8)&0xff0000) |
         ((s >> 8)&0xff00) |
         (s >> 24);
}
#endif

uint64_t
trunnel_htonll(uint64_t a)
{
#ifdef IS_LITTLE_ENDIAN
  return trunnel_htonl((uint32_t)(a>>32))
    | (((uint64_t)trunnel_htonl((uint32_t)a))<<32);
#else
  return a;
#endif
}

uint64_t
trunnel_ntohll(uint64_t a)
{
  return trunnel_htonll(a);
}

#ifdef TRUNNEL_DEBUG_FAILING_ALLOC
/** Used for debugging and running tricky test cases: Makes the nth
 * memoryation allocation call from now fail.
 */
int trunnel_provoke_alloc_failure = 0;
#endif

void *
trunnel_dynarray_expand(size_t *allocated_p, void *ptr,
                        size_t howmanymore, size_t eltsize)
{
  size_t newsize = howmanymore + *allocated_p;
  void *newarray = NULL;
  if (newsize < 8)
    newsize = 8;
  if (newsize < *allocated_p * 2)
    newsize = *allocated_p * 2;
  if (newsize <= *allocated_p || newsize < howmanymore)
    return NULL;
  newarray = trunnel_reallocarray(ptr, newsize, eltsize);
  if (newarray == NULL)
    return NULL;

  *allocated_p = newsize;
  return newarray;
}

#ifndef trunnel_reallocarray
void *
trunnel_reallocarray(void *a, size_t x, size_t y)
{
#ifdef TRUNNEL_DEBUG_FAILING_ALLOC
   if (trunnel_provoke_alloc_failure) {
     if (--trunnel_provoke_alloc_failure == 0)
       return NULL;
   }
#endif
   if (x > SIZE_MAX / y)
     return NULL;
   return trunnel_realloc(a, x * y);
}
#endif

const char *
trunnel_string_getstr(trunnel_string_t *str)
{
  trunnel_assert(str->allocated_ >= str->n_);
  if (str->allocated_ == str->n_) {
    TRUNNEL_DYNARRAY_EXPAND(char, str, 1, {});
  }
  str->elts_[str->n_] = 0;
  return str->elts_;
trunnel_alloc_failed:
  return NULL;
}

int
trunnel_string_setstr0(trunnel_string_t *str, const char *val, size_t len,
                       uint8_t *errcode_ptr)
{
  if (len == SIZE_MAX)
    goto trunnel_alloc_failed;
  if (str->allocated_ <= len) {
    TRUNNEL_DYNARRAY_EXPAND(char, str, len + 1 - str->allocated_, {});
  }
  memcpy(str->elts_, val, len);
  str->n_ = len;
  str->elts_[len] = 0;
  return 0;
trunnel_alloc_failed:
  *errcode_ptr = 1;
  return -1;
}

int
trunnel_string_setlen(trunnel_string_t *str, size_t newlen,
                      uint8_t *errcode_ptr)
{
  if (newlen == SIZE_MAX)
    goto trunnel_alloc_failed;
  if (str->allocated_ < newlen + 1) {
    TRUNNEL_DYNARRAY_EXPAND(char, str, newlen + 1 - str->allocated_, {});
  }
  if (str->n_ < newlen) {
    memset(& (str->elts_[str->n_]), 0, (newlen - str->n_));
  }
  str->n_ = newlen;
  str->elts_[newlen] = 0;
  return 0;

 trunnel_alloc_failed:
  *errcode_ptr = 1;
  return -1;
}

void *
trunnel_dynarray_setlen(size_t *allocated_p, size_t *len_p,
                        void *ptr, size_t newlen,
                        size_t eltsize, trunnel_free_fn_t free_fn,
                        uint8_t *errcode_ptr)
{
  if (*allocated_p < newlen) {
    void *newptr = trunnel_dynarray_expand(allocated_p, ptr,
                                           newlen - *allocated_p, eltsize);
    if (newptr == NULL)
      goto trunnel_alloc_failed;
    ptr = newptr;
  }
  if (free_fn && *len_p > newlen) {
    size_t i;
    void **elts = (void **) ptr;
    for (i = newlen; i < *len_p; ++i) {
      free_fn(elts[i]);
      elts[i] = NULL;
    }
  }
  if (*len_p < newlen) {
    memset( ((char*)ptr) + (eltsize * *len_p), 0, (newlen - *len_p) * eltsize);
  }
  *len_p = newlen;
  return ptr;
 trunnel_alloc_failed:
  *errcode_ptr = 1;
  return NULL;
}

/*
Copyright 2014  The Tor Project, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

    * Neither the names of the copyright owners nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
