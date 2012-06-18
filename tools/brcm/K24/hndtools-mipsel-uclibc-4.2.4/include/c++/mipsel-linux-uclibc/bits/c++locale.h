// Wrapper for underlying C-language localization -*- C++ -*-

// Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

//
// ISO C++ 14882: 22.8  Standard locale categories.
//

// Written by Benjamin Kosnik <bkoz@redhat.com>

#ifndef _C_LOCALE_H
#define _C_LOCALE_H 1

#pragma GCC system_header

#include <cstring>              // get std::strlen
#include <cstdio>               // get std::snprintf or std::sprintf
#include <clocale>
#include <langinfo.h>		// For codecvt
#ifdef __UCLIBC_MJN3_ONLY__
#warning fix this
#endif
#ifdef __UCLIBC_HAS_LOCALE__
#include <iconv.h>		// For codecvt using iconv, iconv_t
#endif
#ifdef __UCLIBC_HAS_GETTEXT_AWARENESS__
#include <libintl.h> 		// For messages
#endif

#ifdef __UCLIBC_MJN3_ONLY__
#warning what is _GLIBCXX_C_LOCALE_GNU for
#endif
#define _GLIBCXX_C_LOCALE_GNU 1

#ifdef __UCLIBC_MJN3_ONLY__
#warning fix categories
#endif
// #define _GLIBCXX_NUM_CATEGORIES 6
#define _GLIBCXX_NUM_CATEGORIES 0
 
#ifdef __UCLIBC_HAS_XLOCALE__
namespace __gnu_cxx
{
  extern "C" __typeof(uselocale) __uselocale;
}
#endif

namespace std
{
#ifdef __UCLIBC_HAS_XLOCALE__
  typedef __locale_t		__c_locale;
#else
  typedef int*			__c_locale;
#endif

  // Convert numeric value of type _Tv to string and return length of
  // string.  If snprintf is available use it, otherwise fall back to
  // the unsafe sprintf which, in general, can be dangerous and should
  // be avoided.
  template<typename _Tv>
    int
    __convert_from_v(char* __out, const int __size, const char* __fmt,
#ifdef __UCLIBC_HAS_XLOCALE__
		     _Tv __v, const __c_locale& __cloc, int __prec)
    {
      __c_locale __old = __gnu_cxx::__uselocale(__cloc);
#else
		     _Tv __v, const __c_locale&, int __prec)
    {
# ifdef __UCLIBC_HAS_LOCALE__
      char* __old = std::setlocale(LC_ALL, NULL);
      char* __sav = new char[std::strlen(__old) + 1];
      std::strcpy(__sav, __old);
      std::setlocale(LC_ALL, "C");
# endif
#endif

      const int __ret = std::snprintf(__out, __size, __fmt, __prec, __v);

#ifdef __UCLIBC_HAS_XLOCALE__
      __gnu_cxx::__uselocale(__old);
#elif defined __UCLIBC_HAS_LOCALE__
      std::setlocale(LC_ALL, __sav);
      delete [] __sav;
#endif
      return __ret;
    }
}

#endif
