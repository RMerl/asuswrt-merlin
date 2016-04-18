/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Jeremy Allison			   2001.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../librpc/gen_ndr/ndr_lsa.h"
#include "../librpc/gen_ndr/ndr_samr.h"
#include "auth.h"
#include "ntdomain.h"
#include "rpc_server/rpc_ncacn_np.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/*
 * Handle database - stored per pipe.
 */

struct dcesrv_handle {
	struct dcesrv_handle *prev, *next;
	struct policy_handle wire_handle;
	uint32_t access_granted;
	void *data;
};

struct handle_list {
	struct dcesrv_handle *handles;	/* List of pipe handles. */
	size_t count;			/* Current number of handles. */
	size_t pipe_ref_count;		/* Number of pipe handles referring
					 * to this tree. */
};

/* This is the max handles across all instances of a pipe name. */
#ifndef MAX_OPEN_POLS
#define MAX_OPEN_POLS 2048
#endif

/****************************************************************************
 Hack as handles need to be persisant over lsa pipe closes so long as a samr
 pipe is open. JRA.
****************************************************************************/

static bool is_samr_lsa_pipe(const struct ndr_syntax_id *syntax)
{
	return
#ifdef SAMR_SUPPORT
		ndr_syntax_id_equal(syntax, &ndr_table_samr.syntax_id) ||
#endif
#ifdef LSA_SUPPORT
		ndr_syntax_id_equal(syntax, &ndr_table_lsarpc.syntax_id) ||
#endif
		false;
}

size_t num_pipe_handles(struct pipes_struct *p)
{
	if (p->pipe_handles == NULL) {
		return 0;
	}
	return p->pipe_handles->count;
}

/****************************************************************************
 Initialise a policy handle list on a pipe. Handle list is shared between all
 pipes of the same name.
****************************************************************************/

bool init_pipe_handles(struct pipes_struct *p, const struct ndr_syntax_id *syntax)
{
	struct pipes_struct *plist;
	struct handle_list *hl;

	for (plist = get_first_internal_pipe();
	     plist;
	     plist = get_next_internal_pipe(plist)) {
		if (ndr_syntax_id_equal(syntax, &plist->syntax)) {
			break;
		}
		if (is_samr_lsa_pipe(&plist->syntax)
		    && is_samr_lsa_pipe(syntax)) {
			/*
			 * samr and lsa share a handle space (same process
			 * under Windows?)
			 */
			break;
		}
	}

	if (plist != NULL) {
		hl = plist->pipe_handles;
		if (hl == NULL) {
			return false;
		}
	} else {
		/*
		 * First open, we have to create the handle list
		 */
		hl = talloc_zero(NULL, struct handle_list);
		if (hl == NULL) {
			return false;
		}

		DEBUG(10,("init_pipe_handle_list: created handle list for "
			  "pipe %s\n",
			  get_pipe_name_from_syntax(talloc_tos(), syntax)));
	}

	/*
	 * One more pipe is using this list.
	 */

	hl->pipe_ref_count++;

	/*
	 * Point this pipe at this list.
	 */

	p->pipe_handles = hl;

	DEBUG(10,("init_pipe_handle_list: pipe_handles ref count = %lu for "
		  "pipe %s\n", (unsigned long)p->pipe_handles->pipe_ref_count,
		  get_pipe_name_from_syntax(talloc_tos(), syntax)));

	return True;
}

/****************************************************************************
  find first available policy slot.  creates a policy handle for you.

  If "data_ptr" is given, this must be a talloc'ed object, create_policy_hnd
  talloc_moves this into the handle. If the policy_hnd is closed,
  data_ptr is TALLOC_FREE()'ed
****************************************************************************/

static struct dcesrv_handle *create_rpc_handle_internal(struct pipes_struct *p,
				struct policy_handle *hnd, void *data_ptr)
{
	struct dcesrv_handle *rpc_hnd;
	static uint32 pol_hnd_low  = 0;
	static uint32 pol_hnd_high = 0;
	time_t t = time(NULL);

	if (p->pipe_handles->count > MAX_OPEN_POLS) {
		DEBUG(0,("create_policy_hnd: ERROR: too many handles (%d) on this pipe.\n",
				(int)p->pipe_handles->count));
		return NULL;
	}

	rpc_hnd = talloc_zero(p->pipe_handles, struct dcesrv_handle);
	if (!rpc_hnd) {
		DEBUG(0,("create_policy_hnd: ERROR: out of memory!\n"));
		return NULL;
	}

	if (data_ptr != NULL) {
		rpc_hnd->data = talloc_move(rpc_hnd, &data_ptr);
	}

	pol_hnd_low++;
	if (pol_hnd_low == 0) {
		pol_hnd_high++;
	}

	/* first bit must be null */
	SIVAL(&rpc_hnd->wire_handle.handle_type, 0 , 0);

	/* second bit is incrementing */
	SIVAL(&rpc_hnd->wire_handle.uuid.time_low, 0 , pol_hnd_low);
	SSVAL(&rpc_hnd->wire_handle.uuid.time_mid, 0 , pol_hnd_high);
	SSVAL(&rpc_hnd->wire_handle.uuid.time_hi_and_version, 0, (pol_hnd_high >> 16));

	/* split the current time into two 16 bit values */

	/* something random */
	SSVAL(rpc_hnd->wire_handle.uuid.clock_seq, 0, (t >> 16));
	/* something random */
	SSVAL(rpc_hnd->wire_handle.uuid.node, 0, t);
	/* something more random */
	SIVAL(rpc_hnd->wire_handle.uuid.node, 2, sys_getpid());

	DLIST_ADD(p->pipe_handles->handles, rpc_hnd);
	p->pipe_handles->count++;

	*hnd = rpc_hnd->wire_handle;

	DEBUG(4, ("Opened policy hnd[%d] ", (int)p->pipe_handles->count));
	dump_data(4, (uint8_t *)hnd, sizeof(*hnd));

	return rpc_hnd;
}

bool create_policy_hnd(struct pipes_struct *p, struct policy_handle *hnd,
		       void *data_ptr)
{
	struct dcesrv_handle *rpc_hnd;

	rpc_hnd = create_rpc_handle_internal(p, hnd, data_ptr);
	if (rpc_hnd == NULL) {
		return false;
	}
	return true;
}

/****************************************************************************
  find policy by handle - internal version.
****************************************************************************/

static struct dcesrv_handle *find_policy_by_hnd_internal(struct pipes_struct *p,
				const struct policy_handle *hnd, void **data_p)
{
	struct dcesrv_handle *h;
	unsigned int count;

	if (data_p) {
		*data_p = NULL;
	}

	count = 0;
	for (h = p->pipe_handles->handles; h != NULL; h = h->next) {
		if (memcmp(&h->wire_handle, hnd, sizeof(*hnd)) == 0) {
			DEBUG(4,("Found policy hnd[%u] ", count));
			dump_data(4, (uint8 *)hnd, sizeof(*hnd));
			if (data_p) {
				*data_p = h->data;
			}
			return h;
		}
		count++;
	}

	DEBUG(4,("Policy not found: "));
	dump_data(4, (uint8_t *)hnd, sizeof(*hnd));

	p->fault_state = DCERPC_FAULT_CONTEXT_MISMATCH;

	return NULL;
}

/****************************************************************************
  find policy by handle
****************************************************************************/

bool find_policy_by_hnd(struct pipes_struct *p, const struct policy_handle *hnd,
			void **data_p)
{
	struct dcesrv_handle *rpc_hnd;

	rpc_hnd = find_policy_by_hnd_internal(p, hnd, data_p);
	if (rpc_hnd == NULL) {
		return false;
	}
	return true;
}

/****************************************************************************
  Close a policy.
****************************************************************************/

bool close_policy_hnd(struct pipes_struct *p, struct policy_handle *hnd)
{
	struct dcesrv_handle *rpc_hnd;

	rpc_hnd = find_policy_by_hnd_internal(p, hnd, NULL);

	if (rpc_hnd == NULL) {
		DEBUG(3, ("Error closing policy (policy not found)\n"));
		return false;
	}

	DEBUG(3,("Closed policy\n"));

	p->pipe_handles->count--;

	DLIST_REMOVE(p->pipe_handles->handles, rpc_hnd);
	TALLOC_FREE(rpc_hnd);

	return true;
}

/****************************************************************************
 Close a pipe - free the handle set if it was the last pipe reference.
****************************************************************************/

void close_policy_by_pipe(struct pipes_struct *p)
{
	p->pipe_handles->pipe_ref_count--;

	if (p->pipe_handles->pipe_ref_count == 0) {
		/*
		 * Last pipe open on this list - free the list.
		 */
		TALLOC_FREE(p->pipe_handles);

		DEBUG(10,("close_policy_by_pipe: deleted handle list for "
			  "pipe %s\n",
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
	}
}

/*******************************************************************
Shall we allow access to this rpc?  Currently this function
implements the 'restrict anonymous' setting by denying access to
anonymous users if the restrict anonymous level is > 0.  Further work
will be checking a security descriptor to determine whether a user
token has enough access to access the pipe.
********************************************************************/

bool pipe_access_check(struct pipes_struct *p)
{
	/* Don't let anonymous users access this RPC if restrict
	   anonymous > 0 */

	if (lp_restrict_anonymous() > 0) {

		/* schannel, so we must be ok */
		if (p->pipe_bound &&
		    (p->auth.auth_type == DCERPC_AUTH_TYPE_SCHANNEL)) {
			return True;
		}

		if (p->session_info->guest) {
			return False;
		}
	}

	return True;
}

void *_policy_handle_create(struct pipes_struct *p, struct policy_handle *hnd,
			    uint32_t access_granted, size_t data_size,
			    const char *type, NTSTATUS *pstatus)
{
	struct dcesrv_handle *rpc_hnd;
	void *data;

	if (p->pipe_handles->count > MAX_OPEN_POLS) {
		DEBUG(0, ("policy_handle_create: ERROR: too many handles (%d) "
			  "on pipe %s.\n", (int)p->pipe_handles->count,
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		*pstatus = NT_STATUS_INSUFFICIENT_RESOURCES;
		return NULL;
	}

	data = talloc_size(talloc_tos(), data_size);
	if (data == NULL) {
		*pstatus = NT_STATUS_NO_MEMORY;
		return NULL;
	}
	talloc_set_name_const(data, type);

	rpc_hnd = create_rpc_handle_internal(p, hnd, data);
	if (rpc_hnd == NULL) {
		TALLOC_FREE(data);
		*pstatus = NT_STATUS_NO_MEMORY;
		return NULL;
	}
	rpc_hnd->access_granted = access_granted;
	*pstatus = NT_STATUS_OK;
	return data;
}

void *_policy_handle_find(struct pipes_struct *p,
			  const struct policy_handle *hnd,
			  uint32_t access_required,
			  uint32_t *paccess_granted,
			  const char *name, const char *location,
			  NTSTATUS *pstatus)
{
	struct dcesrv_handle *rpc_hnd;
	void *data;

	rpc_hnd = find_policy_by_hnd_internal(p, hnd, &data);
	if (rpc_hnd == NULL) {
		*pstatus = NT_STATUS_INVALID_HANDLE;
		return NULL;
	}
	if (strcmp(name, talloc_get_name(data)) != 0) {
		DEBUG(10, ("expected %s, got %s\n", name,
			   talloc_get_name(data)));
		*pstatus = NT_STATUS_INVALID_HANDLE;
		return NULL;
	}
	if ((access_required & rpc_hnd->access_granted) != access_required) {
		if (geteuid() == sec_initial_uid()) {
			DEBUG(4, ("%s: ACCESS should be DENIED (granted: "
				  "%#010x; required: %#010x)\n", location,
				  rpc_hnd->access_granted, access_required));
			DEBUGADD(4,("but overwritten by euid == 0\n"));
			goto okay;
		}
		DEBUG(2,("%s: ACCESS DENIED (granted: %#010x; required: "
			 "%#010x)\n", location, rpc_hnd->access_granted,
			 access_required));
		*pstatus = NT_STATUS_ACCESS_DENIED;
		return NULL;
	}

 okay:
	DEBUG(10, ("found handle of type %s\n", talloc_get_name(data)));
	if (paccess_granted != NULL) {
		*paccess_granted = rpc_hnd->access_granted;
	}
	*pstatus = NT_STATUS_OK;
	return data;
}
