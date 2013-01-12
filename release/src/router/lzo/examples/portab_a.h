/* portab_a.h -- advanced portability layer

   This file is part of the LZO real-time data compression library.

   Copyright (C) 2011 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2010 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2009 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2008 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2007 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2006 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2005 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2004 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2003 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2002 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2001 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer
   All Rights Reserved.

   The LZO library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The LZO library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the LZO library; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Markus F.X.J. Oberhumer
   <markus@oberhumer.com>
   http://www.oberhumer.com/opensource/lzo/
 */


/*************************************************************************
// use the ACC library for the hard work
**************************************************************************/

#if defined(LZO_HAVE_CONFIG_H)
#  define ACC_CONFIG_NO_HEADER 1
#endif

#define ACC_WANT_ACC_INCD_H 1
#define ACC_WANT_ACC_INCE_H 1
#if defined(__LZO_MMODEL_HUGE) || defined(ACC_WANT_ACCLIB_GETOPT) || defined(WANT_LZO_UCLOCK) || defined(WANT_LZO_WILDARGV)
#  define ACC_WANT_ACC_INCI_H 1
#  define ACC_WANT_ACC_LIB_H 1
#endif
#include "src/miniacc.h"

#if defined(WANT_LZO_MALLOC)
#  if defined(__LZO_MMODEL_HUGE)
#    define ACC_WANT_ACCLIB_HALLOC 1
#  else
#    define acc_halloc(a)           (malloc(a))
#    define acc_hfree(a)            (free(a))
#  endif
#endif
#if defined(WANT_LZO_FREAD)
#  if defined(__LZO_MMODEL_HUGE)
#    define ACC_WANT_ACCLIB_HFREAD 1
#  else
#    define acc_hfread(f,b,s)       (fread(b,1,s,f))
#    define acc_hfwrite(f,b,s)      (fwrite(b,1,s,f))
#  endif
#endif
#if defined(WANT_LZO_UCLOCK)
#  define ACC_WANT_ACCLIB_PCLOCK 1
#  if 0 && (LZO_ARCH_AMD64 || LZO_ARCH_I386)
#    define __ACCLIB_PCLOCK_USE_RDTSC 1
#    define ACC_WANT_ACCLIB_RDTSC 1
#  endif
#endif
#if defined(WANT_LZO_WILDARGV)
#  define ACC_WANT_ACCLIB_WILDARGV 1
#endif
#if (__ACCLIB_REQUIRE_HMEMCPY_CH) && !defined(__ACCLIB_HMEMCPY_CH_INCLUDED)
#  define ACC_WANT_ACCLIB_HMEMCPY 1
#endif
#include "src/miniacc.h"


/*************************************************************************
// finally pull into the LZO namespace
**************************************************************************/

#undef lzo_malloc
#undef lzo_free
#undef lzo_fread
#undef lzo_fwrite
#if defined(WANT_LZO_MALLOC)
#  if defined(acc_halloc)
#    define lzo_malloc(a)       acc_halloc(a)
#  else
#    define lzo_malloc(a)       __ACCLIB_FUNCNAME(acc_halloc)(a)
#  endif
#  if defined(acc_hfree)
#    define lzo_free(a)         acc_hfree(a)
#  else
#    define lzo_free(a)         __ACCLIB_FUNCNAME(acc_hfree)(a)
#  endif
#endif
#if defined(WANT_LZO_FREAD)
#  if defined(acc_hfread)
#    define lzo_fread(f,b,s)    acc_hfread(f,b,s)
#  else
#    define lzo_fread(f,b,s)    __ACCLIB_FUNCNAME(acc_hfread)(f,b,s)
#  endif
#  if defined(acc_hfwrite)
#    define lzo_fwrite(f,b,s)   acc_hfwrite(f,b,s)
#  else
#    define lzo_fwrite(f,b,s)   __ACCLIB_FUNCNAME(acc_hfwrite)(f,b,s)
#  endif
#endif
#if defined(WANT_LZO_UCLOCK)
#  define lzo_uclock_handle_t   acc_pclock_handle_t
#  define lzo_uclock_t          acc_pclock_t
#  define lzo_uclock_open(a)    __ACCLIB_FUNCNAME(acc_pclock_open_default)(a)
#  define lzo_uclock_close(a)   __ACCLIB_FUNCNAME(acc_pclock_close)(a)
#  define lzo_uclock_read(a,b)  __ACCLIB_FUNCNAME(acc_pclock_read)(a,b)
#  define lzo_uclock_get_elapsed(a,b,c) __ACCLIB_FUNCNAME(acc_pclock_get_elapsed)(a,b,c)
#endif
#if defined(WANT_LZO_WILDARGV)
#  define lzo_wildargv(a,b)     __ACCLIB_FUNCNAME(acc_wildargv)(a,b)
#endif


/*
vi:ts=4:et
*/

