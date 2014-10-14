
/******************************************************************************
 *
 * Module Name: exmutex - ASL Mutex Acquire/Release functions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2007, R. Byron Moore
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <acpi/acpi.h>
#include <acpi/acinterp.h>
#include <acpi/acevents.h>

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exmutex")

/* Local prototypes */
static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread);

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_unlink_mutex
 *
 * PARAMETERS:  obj_desc            - The mutex to be unlinked
 *
 * RETURN:      None
 *
 * DESCRIPTION: Remove a mutex from the "AcquiredMutex" list
 *
 ******************************************************************************/

void acpi_ex_unlink_mutex(union acpi_operand_object *obj_desc)
{
	struct acpi_thread_state *thread = obj_desc->mutex.owner_thread;

	if (!thread) {
		return;
	}

	/* Doubly linked list */

	if (obj_desc->mutex.next) {
		(obj_desc->mutex.next)->mutex.prev = obj_desc->mutex.prev;
	}

	if (obj_desc->mutex.prev) {
		(obj_desc->mutex.prev)->mutex.next = obj_desc->mutex.next;
	} else {
		thread->acquired_mutex_list = obj_desc->mutex.next;
	}
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_link_mutex
 *
 * PARAMETERS:  obj_desc        - The mutex to be linked
 *              Thread          - Current executing thread object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add a mutex to the "AcquiredMutex" list for this walk
 *
 ******************************************************************************/

static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread)
{
	union acpi_operand_object *list_head;

	list_head = thread->acquired_mutex_list;

	/* This object will be the first object in the list */

	obj_desc->mutex.prev = NULL;
	obj_desc->mutex.next = list_head;

	/* Update old first object to point back to this object */

	if (list_head) {
		list_head->mutex.prev = obj_desc;
	}

	/* Update list head */

	thread->acquired_mutex_list = obj_desc;
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_acquire_mutex
 *
 * PARAMETERS:  time_desc           - Timeout integer
 *              obj_desc            - Mutex object
 *              walk_state          - Current method execution state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire an AML mutex
 *
 ******************************************************************************/

acpi_status
acpi_ex_acquire_mutex(union acpi_operand_object *time_desc,
		      union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_acquire_mutex, obj_desc);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Sanity check: we must have a valid thread ID */

	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	/*
	 * Current Sync must be less than or equal to the sync level of the
	 * mutex. This mechanism provides some deadlock prevention
	 */
	if (walk_state->thread->current_sync_level > obj_desc->mutex.sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], current SyncLevel is too large (%d)",
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    walk_state->thread->current_sync_level));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	/* Support for multiple acquires by the owning thread */

	if (obj_desc->mutex.owner_thread) {
		if (obj_desc->mutex.owner_thread->thread_id ==
		    walk_state->thread->thread_id) {
			/*
			 * The mutex is already owned by this thread, just increment the
			 * acquisition depth
			 */
			obj_desc->mutex.acquisition_depth++;
			return_ACPI_STATUS(AE_OK);
		}
	}

	/* Acquire the mutex, wait if necessary. Special case for Global Lock */

	if (obj_desc->mutex.os_mutex == acpi_gbl_global_lock_mutex) {
		status =
		    acpi_ev_acquire_global_lock((u16) time_desc->integer.value);
	} else {
		status = acpi_ex_system_wait_mutex(obj_desc->mutex.os_mutex,
						   (u16) time_desc->integer.
						   value);
	}

	if (ACPI_FAILURE(status)) {

		/* Includes failure from a timeout on time_desc */

		return_ACPI_STATUS(status);
	}

	/* Have the mutex: update mutex and walk info and save the sync_level */

	obj_desc->mutex.owner_thread = walk_state->thread;
	obj_desc->mutex.acquisition_depth = 1;
	obj_desc->mutex.original_sync_level =
	    walk_state->thread->current_sync_level;

	walk_state->thread->current_sync_level = obj_desc->mutex.sync_level;

	/* Link the mutex to the current thread for force-unlock at method exit */

	acpi_ex_link_mutex(obj_desc, walk_state->thread);
	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_release_mutex
 *
 * PARAMETERS:  obj_desc            - The object descriptor for this op
 *              walk_state          - Current method execution state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release a previously acquired Mutex.
 *
 ******************************************************************************/

acpi_status
acpi_ex_release_mutex(union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_release_mutex);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* The mutex must have been previously acquired in order to release it */

	if (!obj_desc->mutex.owner_thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], not acquired",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_MUTEX_NOT_ACQUIRED);
	}

	/* Sanity check: we must have a valid thread ID */

	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	/*
	 * The Mutex is owned, but this thread must be the owner.
	 * Special case for Global Lock, any thread can release
	 */
	if ((obj_desc->mutex.owner_thread->thread_id !=
	     walk_state->thread->thread_id)
	    && (obj_desc->mutex.os_mutex != acpi_gbl_global_lock_mutex)) {
		ACPI_ERROR((AE_INFO,
			    "Thread %lX cannot release Mutex [%4.4s] acquired by thread %lX",
			    (unsigned long)walk_state->thread->thread_id,
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    (unsigned long)obj_desc->mutex.owner_thread->
			    thread_id));
		return_ACPI_STATUS(AE_AML_NOT_OWNER);
	}

	/*
	 * The sync level of the mutex must be less than or equal to the current
	 * sync level
	 */
	if (obj_desc->mutex.sync_level > walk_state->thread->current_sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], incorrect SyncLevel",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	/* Match multiple Acquires with multiple Releases */

	obj_desc->mutex.acquisition_depth--;
	if (obj_desc->mutex.acquisition_depth != 0) {

		/* Just decrement the depth and return */

		return_ACPI_STATUS(AE_OK);
	}

	/* Unlink the mutex from the owner's list */

	acpi_ex_unlink_mutex(obj_desc);

	/* Release the mutex, special case for Global Lock */

	if (obj_desc->mutex.os_mutex == acpi_gbl_global_lock_mutex) {
		status = acpi_ev_release_global_lock();
	} else {
		acpi_os_release_mutex(obj_desc->mutex.os_mutex);
	}

	/* Update the mutex and restore sync_level */

	obj_desc->mutex.owner_thread = NULL;
	walk_state->thread->current_sync_level =
	    obj_desc->mutex.original_sync_level;

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_release_all_mutexes
 *
 * PARAMETERS:  Thread          - Current executing thread object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release all mutexes held by this thread
 *
 * NOTE: This function is called as the thread is exiting the interpreter.
 * Mutexes are not released when an individual control method is exited, but
 * only when the parent thread actually exits the interpreter. This allows one
 * method to acquire a mutex, and a different method to release it, as long as
 * this is performed underneath a single parent control method.
 *
 ******************************************************************************/

void acpi_ex_release_all_mutexes(struct acpi_thread_state *thread)
{
	union acpi_operand_object *next = thread->acquired_mutex_list;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_ENTRY();

	/* Traverse the list of owned mutexes, releasing each one */

	while (next) {
		obj_desc = next;
		next = obj_desc->mutex.next;

		obj_desc->mutex.prev = NULL;
		obj_desc->mutex.next = NULL;
		obj_desc->mutex.acquisition_depth = 0;

		/* Release the mutex, special case for Global Lock */

		if (obj_desc->mutex.os_mutex == acpi_gbl_global_lock_mutex) {

			/* Ignore errors */

			(void)acpi_ev_release_global_lock();
		} else {
			acpi_os_release_mutex(obj_desc->mutex.os_mutex);
		}

		/* Mark mutex unowned */

		obj_desc->mutex.owner_thread = NULL;

		/* Update Thread sync_level (Last mutex is the important one) */

		thread->current_sync_level =
		    obj_desc->mutex.original_sync_level;
	}
}
