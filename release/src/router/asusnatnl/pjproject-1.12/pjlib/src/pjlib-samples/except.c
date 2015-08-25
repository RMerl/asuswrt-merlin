/* $Id: except.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/except.h>
#include <pj/rand.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * \page page_pjlib_samples_except_c Example: Exception Handling
 *
 * Below is sample program to demonstrate how to use exception handling.
 *
 * \includelineno pjlib-samples/except.c
 */

static pj_exception_id_t NO_MEMORY, OTHER_EXCEPTION;

static void randomly_throw_exception()
{
    if (pj_rand() % 2)
        PJ_THROW(OTHER_EXCEPTION);
}

static void *my_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr)
        PJ_THROW(NO_MEMORY);
    return ptr;
}

static int test_exception()
{
    PJ_USE_EXCEPTION;
    
    PJ_TRY {
        void *data = my_malloc(200);
        free(data);
        randomly_throw_exception();
    }
    PJ_CATCH_ANY {
        pj_exception_id_t x_id;
        
        x_id = PJ_GET_EXCEPTION();
        printf("Caught exception %d (%s)\n", 
            x_id, pj_exception_id_name(x_id));
    }
    PJ_END
        return 1;
}

int main()
{
    pj_status_t rc;
    
    // Error handling is omited for clarity.
    
    rc = pj_init();

    rc = pj_exception_id_alloc("No Memory", &NO_MEMORY);
    rc = pj_exception_id_alloc("Other Exception", &OTHER_EXCEPTION);
    
    return test_exception();
}

