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
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/errno.h>
#include <pj/string.h>

static long thread_local_id[PJSUA_MAX_INSTANCES];
static int is_initialized;

static void thread_local_id_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(thread_local_id); i++)
	{
		thread_local_id[i] = -1;
	}

	is_initialized = 1;
}

#if defined(PJ_HAS_EXCEPTION_NAMES) && PJ_HAS_EXCEPTION_NAMES != 0
    static const char *exception_id_names[PJSUA_MAX_INSTANCES][PJ_MAX_EXCEPTION_ID];
#else
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    static int last_exception_id = 1;
#endif  /* PJ_HAS_EXCEPTION_NAMES */


#if !defined(PJ_EXCEPTION_USE_WIN32_SEH) || PJ_EXCEPTION_USE_WIN32_SEH==0
PJ_DEF(void) pj_throw_exception_(int inst_id, int exception_id)
{
    struct pj_exception_state_t *handler;

    handler = (struct pj_exception_state_t*) 
	      pj_thread_local_get(thread_local_id[inst_id]);
    if (handler == NULL) {
        PJ_LOG(1,("except.c", "!!!FATAL: unhandled exception %s!\n", 
		   pj_exception_id_name(inst_id, exception_id)));
        pj_assert(handler != NULL);
        /* This will crash the system! */
    }
    pj_pop_exception_handler_(inst_id, handler);
    pj_longjmp(handler->state, exception_id);
}

static void exception_cleanup(int inst_id)
{
    if (thread_local_id[inst_id] != -1) {
	pj_thread_local_free(thread_local_id[inst_id]);
	thread_local_id[inst_id] = -1;
    }

#if defined(PJ_HAS_EXCEPTION_NAMES) && PJ_HAS_EXCEPTION_NAMES != 0
    {
	unsigned i;
	for (i=0; i<PJ_MAX_EXCEPTION_ID; ++i)
	    exception_id_names[inst_id][i] = NULL;
    }
#else
    last_exception_id = 1;
#endif
}

PJ_DEF(void) pj_push_exception_handler_(int inst_id, struct pj_exception_state_t *rec)
{
    struct pj_exception_state_t *parent_handler = NULL;

	thread_local_id_initialize();

    if (thread_local_id[inst_id] == -1) {
	pj_thread_local_alloc(&thread_local_id[inst_id]);
	pj_assert(thread_local_id[inst_id] != -1);
	pj_atexit(inst_id, &exception_cleanup);
    }
    parent_handler = (struct pj_exception_state_t *)
		      pj_thread_local_get(thread_local_id[inst_id]);
    rec->prev = parent_handler;
    pj_thread_local_set(thread_local_id[inst_id], rec);
}

PJ_DEF(void) pj_pop_exception_handler_(int inst_id, struct pj_exception_state_t *rec)
{
    struct pj_exception_state_t *handler;

    handler = (struct pj_exception_state_t *)
	      pj_thread_local_get(thread_local_id[inst_id]);
    if (handler && handler==rec) {
	pj_thread_local_set(thread_local_id[inst_id], handler->prev);
    }
}
#endif

#if defined(PJ_HAS_EXCEPTION_NAMES) && PJ_HAS_EXCEPTION_NAMES != 0
PJ_DEF(pj_status_t) pj_exception_id_alloc( int inst_id,
										  const char *name,
                                          pj_exception_id_t *id)
{
    unsigned i;

    pj_enter_critical_section(inst_id);

    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    for (i=1; i<PJ_MAX_EXCEPTION_ID; ++i) {
        if (exception_id_names[inst_id][i] == NULL) {
            exception_id_names[inst_id][i] = name;
            *id = i;
            pj_leave_critical_section(inst_id);
            return PJ_SUCCESS;
        }
    }

    pj_leave_critical_section(inst_id);
    return PJ_ETOOMANY;
}

PJ_DEF(pj_status_t) pj_exception_id_free( int inst_id, pj_exception_id_t id )
{
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    PJ_ASSERT_RETURN(id>0 && id<PJ_MAX_EXCEPTION_ID, PJ_EINVAL);
    
    pj_enter_critical_section(inst_id);
    exception_id_names[inst_id][id] = NULL;
    pj_leave_critical_section(inst_id);

    return PJ_SUCCESS;

}

PJ_DEF(const char*) pj_exception_id_name(int inst_id, pj_exception_id_t id)
{
    static char unknown_name[32];

    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    PJ_ASSERT_RETURN(id>0 && id<PJ_MAX_EXCEPTION_ID, "<Invalid ID>");

    if (exception_id_names[inst_id][id] == NULL) {
        pj_ansi_snprintf(unknown_name, sizeof(unknown_name), 
			 "exception %d", id);
	return unknown_name;
    }

    return exception_id_names[inst_id][id];
}

#else   /* PJ_HAS_EXCEPTION_NAMES */
PJ_DEF(pj_status_t) pj_exception_id_alloc( const char *name,
                                           pj_exception_id_t *id)
{
    PJ_ASSERT_RETURN(last_exception_id < PJ_MAX_EXCEPTION_ID-1, PJ_ETOOMANY);

    *id = last_exception_id++;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_exception_id_free( pj_exception_id_t id )
{
    return PJ_SUCCESS;
}

PJ_DEF(const char*) pj_exception_id_name(pj_exception_id_t id)
{
    return "";
}

#endif  /* PJ_HAS_EXCEPTION_NAMES */



