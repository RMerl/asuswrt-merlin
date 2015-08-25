/* $Id: exception.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "test.h"


/**
 * \page page_pjlib_exception_test Test: Exception Handling
 *
 * This file provides implementation of \b exception_test(). It tests the
 * functionality of the exception handling API.
 *
 * @note This test use static ID not acquired through proper registration.
 * This is not recommended, since it may create ID collissions.
 *
 * \section exception_test_sec Scope of the Test
 *
 * Some scenarios tested:
 *  - no exception situation
 *  - basic TRY/CATCH
 *  - multiple exception handlers
 *  - default handlers
 *
 *
 * This file is <b>pjlib-test/exception.c</b>
 *
 * \include pjlib-test/exception.c
 */


#if INCLUDE_EXCEPTION_TEST

#include <pjlib.h>

#define	ID_1	1
#define ID_2	2

static int throw_id_1(int inst_id)
{
    PJ_THROW( inst_id, ID_1 );
    PJ_UNREACHED(return -1;)
}

static int throw_id_2(int inst_id)
{
    PJ_THROW( inst_id, ID_2 );
    PJ_UNREACHED(return -1;)
}

static int try_catch_test(int inst_id)
{
    PJ_USE_EXCEPTION;
    int rc = -200;

    PJ_TRY(inst_id) {
	PJ_THROW(inst_id, ID_1);
    }
    PJ_CATCH_ANY {
	rc = 0;
    }
    PJ_END(inst_id);
    return rc;
}

static int throw_in_handler(int inst_id)
{
    PJ_USE_EXCEPTION;
    int rc = 0;

    PJ_TRY(inst_id) {
	PJ_THROW(inst_id, ID_1);
    }
    PJ_CATCH_ANY {
	if (PJ_GET_EXCEPTION() != ID_1)
	    rc = -300;
	else
	    PJ_THROW(inst_id, ID_2);
    }
    PJ_END(inst_id);
    return rc;
}

static int return_in_handler(int inst_id)
{
    PJ_USE_EXCEPTION;

    PJ_TRY(inst_id) {
	PJ_THROW(inst_id, ID_1);
    }
    PJ_CATCH_ANY {
	return 0;
    }
    PJ_END(inst_id);
    return -400;
}


static int test(int inst_id)
{
    int rc = 0;
    PJ_USE_EXCEPTION;

    /*
     * No exception situation.
     */
    PJ_TRY(inst_id) {
        rc = rc;
    }
    PJ_CATCH_ANY {
        rc = -3;
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;


    /*
     * Basic TRY/CATCH
     */ 
    PJ_TRY(inst_id) {
	rc = throw_id_1(inst_id);

	// should not reach here.
	rc = -10;
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
	if (id != ID_1) {
	    PJ_LOG(3,("", "...error: got unexpected exception %d (%s)", 
		      id, pj_exception_id_name(inst_id, id)));
	    if (!rc) rc = -20;
	}
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;

    /*
     * Multiple exceptions handlers
     */
    PJ_TRY(inst_id) {
	rc = throw_id_2(inst_id);
	// should not reach here.
	rc = -25;
    }
    PJ_CATCH_ANY {
	switch (PJ_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = -30; break;
	case ID_2:
	    if (!rc) rc = 0; break;
	default:
	    if (!rc) rc = -40;
	    break;
	}
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;

    /*
     * Test default handler.
     */
    PJ_TRY(inst_id) {
	rc = throw_id_1(inst_id);
	// should not reach here
	rc = -50;
    }
    PJ_CATCH_ANY {
	switch (PJ_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = 0;
	    break;
	default:
	    if (!rc) rc = -60;
	    break;
	}
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;

    /*
     * Nested handlers
     */
    PJ_TRY(inst_id) {
	rc = try_catch_test(inst_id);
    }
    PJ_CATCH_ANY {
	rc = -70;
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;

    /*
     * Throwing exception inside handler
     */
    rc = -80;
    PJ_TRY(inst_id) {
	int rc2;
	rc2 = throw_in_handler(inst_id);
	if (rc2)
	    rc = rc2;
    }
    PJ_CATCH_ANY {
	if (PJ_GET_EXCEPTION() == ID_2) {
	    rc = 0;
	} else {
	    rc = -90;
	}
    }
    PJ_END(inst_id);

    if (rc != 0)
	return rc;


    /*
     * Return from handler. Returning from the function inside a handler
     * should be okay (though returning from the function inside the
     * PJ_TRY block IS NOT OKAY!!). We want to test to see if handler
     * is cleaned up properly, but not sure how to do this.
     */
    PJ_TRY(inst_id) {
	int rc2;
	rc2 = return_in_handler(inst_id);
	if (rc2)
	    rc = rc2;
    }
    PJ_CATCH_ANY {
	rc = -100;
    }
    PJ_END(inst_id);


    return 0;
}

int exception_test(int inst_id)
{
    int i, rc;
    enum { LOOP = 10 };

    for (i=0; i<LOOP; ++i) {
	if ((rc=test(inst_id)) != 0) {
	    PJ_LOG(3,("", "...failed at i=%d (rc=%d)", i, rc));
	    return rc;
	}
    }
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_exception_test;
#endif	/* INCLUDE_EXCEPTION_TEST */


