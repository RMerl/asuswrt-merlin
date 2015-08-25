/* $Id: rand.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_RAND_H__
#define __PJ_RAND_H__

/**
 * @file rand.h
 * @brief Random Number Generator.
 */

#include <pj/config.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJ_RAND Random Number Generator
 * @ingroup PJ_MISC
 * @{
 * This module contains functions for generating random numbers.
 * This abstraction is needed not only because not all platforms have
 * \a rand() and \a srand(), but also on some platforms \a rand()
 * only has 16-bit randomness, which is not good enough.
 */

/**
 * Put in seed to random number generator.
 *
 * @param seed	    Seed value.
 */
PJ_DECL(void) pj_srand(unsigned int seed);


/**
 * Generate random integer with 32bit randomness.
 *
 * @return a random integer.
 */
PJ_DECL(int) pj_rand(void);


/** @} */


PJ_END_DECL


#endif	/* __PJ_RAND_H__ */

