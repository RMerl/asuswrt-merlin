/* $Id: assert.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_ASSERT_H__
#define __PJ_ASSERT_H__

/**
 * @file assert.h
 * @brief Assertion macro pj_assert().
 */

#include <pj/config.h>
#include <pj/compat/assert.h>

/**
 * @defgroup pj_assert Assertion Macro
 * @ingroup PJ_MISC
 * @{
 *
 * Assertion and other helper macros for sanity checking.
 */

/**
 * @hideinitializer
 * Check during debug build that an expression is true. If the expression
 * computes to false during run-time, then the program will stop at the
 * offending statements.
 * For release build, this macro will not do anything.
 *
 * @param expr	    The expression to be evaluated.
 */
#ifndef pj_assert
#   define pj_assert(expr)   assert(expr)
#endif


/**
 * @hideinitializer
 * If #PJ_ENABLE_EXTRA_CHECK is declared and the value is non-zero, then 
 * #PJ_ASSERT_RETURN macro will evaluate the expression in @a expr during
 * run-time. If the expression yields false, assertion will be triggered
 * and the current function will return with the specified return value.
 *
 * If #PJ_ENABLE_EXTRA_CHECK is not declared or is zero, then no run-time
 * checking will be performed. The macro simply evaluates to pj_assert(expr).
 */
#if defined(PJ_ENABLE_EXTRA_CHECK) && PJ_ENABLE_EXTRA_CHECK != 0
#   define PJ_ASSERT_RETURN(expr,retval)    \
	    do { \
		if (!(expr)) { pj_assert(expr); return retval; } \
	    } while (0)
#else
#   define PJ_ASSERT_RETURN(expr,retval)    pj_assert(expr)
#endif

/**
 * @hideinitializer
 * If #PJ_ENABLE_EXTRA_CHECK is declared and non-zero, then 
 * #PJ_ASSERT_ON_FAIL macro will evaluate the expression in @a expr during
 * run-time. If the expression yields false, assertion will be triggered
 * and @a exec_on_fail will be executed.
 *
 * If #PJ_ENABLE_EXTRA_CHECK is not declared or is zero, then no run-time
 * checking will be performed. The macro simply evaluates to pj_assert(expr).
 */
#if defined(PJ_ENABLE_EXTRA_CHECK) && PJ_ENABLE_EXTRA_CHECK != 0
#   define PJ_ASSERT_ON_FAIL(expr,exec_on_fail)    \
	    do { \
		pj_assert(expr); \
		if (!(expr)) exec_on_fail; \
	    } while (0)
#else
#   define PJ_ASSERT_ON_FAIL(expr,exec_on_fail)    pj_assert(expr)
#endif

/** @} */

#endif	/* __PJ_ASSERT_H__ */

