/*
   Unix SMB/CIFS implementation.
   Implement a stack of talloc contexts
   Copyright (C) Volker Lendecke 2007
   Copyright (C) Jeremy Allison 2009 - made thread safe.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Implement a stack of talloc frames.
 *
 * When a new talloc stackframe is allocated with talloc_stackframe(), then
 * the TALLOC_CTX returned with talloc_tos() is reset to that new
 * frame. Whenever that stack frame is TALLOC_FREE()'ed, then the reverse
 * happens: The previous talloc_tos() is restored.
 *
 * This API is designed to be robust in the sense that if someone forgets to
 * TALLOC_FREE() a stackframe, then the next outer one correctly cleans up and
 * resets the talloc_tos().
 *
 * This robustness feature means that we can't rely on a linked list with
 * talloc destructors because in a hierarchy of talloc destructors the parent
 * destructor is called before its children destructors. The child destructor
 * called after the parent would set the talloc_tos() to the wrong value.
 */

#include "includes.h"

struct talloc_stackframe {
	int talloc_stacksize;
	int talloc_stack_arraysize;
	TALLOC_CTX **talloc_stack;
};

/*
 * In the single threaded case this is a pointer
 * to the global talloc_stackframe. In the MT-case
 * this is the pointer to the thread-specific key
 * used to look up the per-thread talloc_stackframe
 * pointer.
 */

static void *global_ts;

/* Variable to ensure TLS value is only initialized once. */
static smb_thread_once_t ts_initialized = SMB_THREAD_ONCE_INIT;

static void talloc_stackframe_init(void * unused)
{
	if (SMB_THREAD_CREATE_TLS("talloc_stackframe", global_ts)) {
		smb_panic("talloc_stackframe_init create_tls failed");
	}
}

static struct talloc_stackframe *talloc_stackframe_create(void)
{
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef calloc
#undef calloc
#endif
#endif
	struct talloc_stackframe *ts = (struct talloc_stackframe *)calloc(
		1, sizeof(struct talloc_stackframe));
#if defined(PARANOID_MALLOC_CHECKER)
#define calloc(n, s) __ERROR_DONT_USE_MALLOC_DIRECTLY
#endif

	if (!ts) {
		smb_panic("talloc_stackframe_init malloc failed");
	}

	SMB_THREAD_ONCE(&ts_initialized, talloc_stackframe_init, NULL);

	if (SMB_THREAD_SET_TLS(global_ts, ts)) {
		smb_panic("talloc_stackframe_init set_tls failed");
	}
	return ts;
}

static int talloc_pop(TALLOC_CTX *frame)
{
	struct talloc_stackframe *ts =
		(struct talloc_stackframe *)SMB_THREAD_GET_TLS(global_ts);
	int i;

	for (i=ts->talloc_stacksize-1; i>0; i--) {
		if (frame == ts->talloc_stack[i]) {
			break;
		}
		TALLOC_FREE(ts->talloc_stack[i]);
	}

	ts->talloc_stack[i] = NULL;
	ts->talloc_stacksize = i;
	return 0;
}

/*
 * Create a new talloc stack frame.
 *
 * When free'd, it frees all stack frames that were created after this one and
 * not explicitly freed.
 */

static TALLOC_CTX *talloc_stackframe_internal(size_t poolsize)
{
	TALLOC_CTX **tmp, *top, *parent;
	struct talloc_stackframe *ts =
		(struct talloc_stackframe *)SMB_THREAD_GET_TLS(global_ts);

	if (ts == NULL) {
		ts = talloc_stackframe_create();
	}

	if (ts->talloc_stack_arraysize < ts->talloc_stacksize + 1) {
		tmp = talloc_realloc(NULL, ts->talloc_stack, TALLOC_CTX *,
					   ts->talloc_stacksize + 1);
		if (tmp == NULL) {
			goto fail;
		}
		ts->talloc_stack = tmp;
		ts->talloc_stack_arraysize = ts->talloc_stacksize + 1;
        }

	if (ts->talloc_stacksize == 0) {
		parent = ts->talloc_stack;
	} else {
		parent = ts->talloc_stack[ts->talloc_stacksize-1];
	}

	if (poolsize) {
		top = talloc_pool(parent, poolsize);
	} else {
		top = talloc_new(parent);
	}

	if (top == NULL) {
		goto fail;
	}

	talloc_set_destructor(top, talloc_pop);

	ts->talloc_stack[ts->talloc_stacksize++] = top;
	return top;

 fail:
	smb_panic("talloc_stackframe failed");
	return NULL;
}

TALLOC_CTX *talloc_stackframe(void)
{
	return talloc_stackframe_internal(0);
}

TALLOC_CTX *talloc_stackframe_pool(size_t poolsize)
{
	return talloc_stackframe_internal(poolsize);
}

/*
 * Get us the current top of the talloc stack.
 */

TALLOC_CTX *talloc_tos(void)
{
	struct talloc_stackframe *ts =
		(struct talloc_stackframe *)SMB_THREAD_GET_TLS(global_ts);

	if (ts == NULL || ts->talloc_stacksize == 0) {
		talloc_stackframe();
		ts = (struct talloc_stackframe *)SMB_THREAD_GET_TLS(global_ts);
		DEBUG(0, ("no talloc stackframe around, leaking memory\n"));
	}

	return ts->talloc_stack[ts->talloc_stacksize-1];
}
