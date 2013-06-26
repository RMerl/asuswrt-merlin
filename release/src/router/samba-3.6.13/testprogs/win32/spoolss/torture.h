/*
   Unix SMB/CIFS implementation.
   SMB torture UI functions

   Copyright (C) Jelmer Vernooij 2006

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

#ifndef __TORTURE_UI_H__
#define __TORTURE_UI_H__

/****************************************************************************
****************************************************************************/

enum torture_result {
	TORTURE_OK=0,
	TORTURE_FAIL=1,
	TORTURE_ERROR=2,
	TORTURE_SKIP=3
};

struct torture_context {
	enum torture_result last_result;
	char *last_reason;
	BOOL print;
	BOOL samba3;
};

/****************************************************************************
****************************************************************************/

#define torture_assert(torture_ctx,expr,cmt) do {\
	if (!(expr)) {\
		torture_result(torture_ctx, TORTURE_FAIL, __location__": Expression `%s' failed: %s", __STRING(expr), cmt); \
		return false;\
	}\
} while(0)

#define torture_assert_str_equal(torture_ctx,got,expected,cmt)\
	do { const char *__got = (got), *__expected = (expected); \
	if (strcmp(__got, __expected) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %s, expected %s: %s", \
			__got, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_int_equal(torture_ctx,got,expected,cmt)\
	do { int __got = (got), __expected = (expected); \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %d, expected %d: %s", \
			__got, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_mem_equal(torture_ctx,got,expected,len,cmt)\
	do { const void *__got = (got), *__expected = (expected); \
	if (memcmp(__got, __expected, len) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" of len %d did not match "#expected": %s", (int)len, cmt); \
		return false; \
	} \
	} while(0)

#define torture_skip(torture_ctx,cmt) do {\
		torture_result(torture_ctx, TORTURE_SKIP, __location__": %s", cmt);\
		return true; \
	} while(0)

#define torture_fail(torture_ctx,cmt) do {\
		torture_result(torture_ctx, TORTURE_FAIL, __location__": %s", cmt);\
		return false; \
	} while (0)

#include "torture_proto.h"

#endif
