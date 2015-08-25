/* $Id: rand.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/rand.h>
#include <pj/log.h>
#include "test.h"

#if INCLUDE_RAND_TEST

#define COUNT  1024
static int values[COUNT];

/*
 * rand_test(), simply generates COUNT number of random number and
 * check that there's no duplicate numbers.
 */
int rand_test(void)
{
    int i;

    for (i=0; i<COUNT; ++i) {
	int j;

	values[i] = pj_rand();
	for (j=0; j<i; ++j) {
	    if (values[i] == values[j]) {
		PJ_LOG(3,("test", "error: duplicate value %d at %d-th index",
			 values[i], i));
		return -10;
	    }
	}
    }

    return 0;
}

#endif	/* INCLUDE_RAND_TEST */

