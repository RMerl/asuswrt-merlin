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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/*
 * Handle database - stored per pipe.
 */

struct policy {
	struct policy *next, *prev;

	struct policy_handle pol_hnd;

	uint32_t access_granted;

	void *data_ptr;
};

struct handle_list {
	struct policy *Policy; 	/* List of policies. */
	size_t count;			/* Current number of handles. */
	size_t pipe_ref_count;	/* Number of pipe handles referring to this list. */
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
	return (ndr_syntax_id_equal(syntax, &ndr_table_samr.syntax_id)
		|| ndr_syntax_id_equal(syntax, &ndr_table_lsarpc.syntax_id));
}

size_t num_pipe_handles(struct handle_list *list)
{
	if (list == NULL) {
		return 0;
	}
	return list->count;
}

/****************************************************************************
 Initialise a policy handle list on a pipe. Handle list is shared between all
 pipes of the same name.
****************************************************************************/

bool init_pipe_handle_list(pipes_struct *p, const struct ndr_syntax_id *syntax)
{
	pipes_struct *plist;
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
		hl = SMB_MALLOC_P(struct handle_list);
		if (hl == NULL) {
			return false;
		}
		ZERO_STRUCTP(hl);

		DEBUG(10,("init_pipe_handles: created handle list for "
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

	DEBUG(10,("init_pipe_handles: pipe_handles ref count = %lu for pipe %s\n",
		  (unsigned long)p->pipe_handles->pipe_ref_count,
		  get_pipe_name_from_syntax(talloc_tos(), syntax)));

	return True;
}

/****************************************************************************
  find first available policy slot.  creates a policy handle for you.

  If "data_ptr" is given, this must be a talloc'ed object, create_policy_hnd
  talloc_moves this into the handle. If the policy_hnd is closed,
  data_ptr is TALLOC_FREE()'ed
****************************************************************************/

static struct policy *create_policy_hnd_internal(pipes_struct *p,
						 struct policy_handle *hnd,
						 void *data_ptr)
{
	static uint32 pol_hnd_low  = 0;
	static uint32 pol_hnd_high = 0;
	time_t t = time(NULL);

	struct policy *pol;

	if (p->pipe_handles->count > MAX_OPEN_POLS) {
		DEBUG(0,("create_policy_hnd: ERROR: too many handles (%d) on this pipe.\n",
				(int)p->pipe_handles->count));
		return NULL;
	}

	pol = TALLOC_ZERO_P(NULL, struct policy);
	if (!pol) {
		DEBUG(0,("create_policy_hnd: ERROR: out of memory!\n"));
		return NULL;
	}

	if (data_ptr != NULL) {
		pol->data_ptr = talloc_move(pol, &data_ptr);
	}

	pol_hnd_low++;
	if (pol_hnd_low == 0)
		(pol_hnd_high)++;

	SIVAL(&pol->pol_hnd.handle_type, 0 , 0);  /* first bit must be null */
	SIVAL(&pol->pol_hnd.uuid.time_low, 0 , pol_hnd_low ); /* second bit is incrementing */
	SSVAL(&pol->pol_hnd.uuid.time_mid, 0 , pol_hnd_high); /* second bit is incrementing */
	SSVAL(&pol->pol_hnd.uuid.time_hi_and_version, 0 , (pol_hnd_high>>16)); /* second bit is incrementing */

	/* split the current time into two 16 bit values */

	SSVAL(pol->pol_hnd.uuid.clock_seq, 0, (t>>16)); /* something random */
	SSVAL(pol->pol_hnd.uuid.node, 0, t); /* something random */

	SIVAL(pol->pol_hnd.uuid.node, 2, sys_getpid()); /* something more random */

	DLIST_ADD(p->pipe_handles->Policy, pol);
	p->pipe_handles->count++;

	*hnd = pol->pol_hnd;
	
	DEBUG(4,("Opened policy hnd[%d] ", (int)p->pipe_handles->count));
	dump_data(4, (uint8 *)hnd, sizeof(*hnd));

	return pol;
}

bool create_policy_hnd(pipes_struct *p, struct policy_handle *hnd,
		       void *data_ptr)
{
	return create_policy_hnd_internal(p, hnd, data_ptr) != NULL;
}

/****************************************************************************
  find policy by handle - internal version.
****************************************************************************/

static struct policy *find_policy_by_hnd_internal(pipes_struct *p,
						  const struct policy_handle *hnd,
						  void **data_p)
{
	struct policy *pol;
	size_t i;

	if (data_p)
		*data_p = NULL;

	for (i = 0, pol=p->pipe_handles->Policy;pol;pol=pol->next, i++) {
		if (memcmp(&pol->pol_hnd, hnd, sizeof(*hnd)) == 0) {
			DEBUG(4,("Found policy hnd[%d] ", (int)i));
			dump_data(4, (uint8 *)hnd, sizeof(*hnd));
			if (data_p)
				*data_p = pol->data_ptr;
			return pol;
		}
	}

	DEBUG(4,("Policy not found: "));
	dump_data(4, (uint8 *)hnd, sizeof(*hnd));

	p->bad_handle_fault_state = True;

	return NULL;
}

/****************************************************************************
  find policy by handle
****************************************************************************/

bool find_policy_by_hnd(pipes_struct *p, const struct policy_handle *hnd,
			void **data_p)
{
	return find_policy_by_hnd_internal(p, hnd, data_p) == NULL ? False : True;
}

/****************************************************************************
  Close a policy.
****************************************************************************/

bool close_policy_hnd(pipes_struct *p, struct policy_handle *hnd)
{
	struct policy *pol = find_policy_by_hnd_internal(p, hnd, NULL);

	if (!pol) {
		DEBUG(3,("Error closing policy\n"));
		return False;
	}

	DEBUG(3,("Closed policy\n"));

	p->pipe_handles->count--;

	DLIST_REMOVE(p->pipe_handles->Policy, pol);

	TALLOC_FREE(pol);

	return True;
}

/****************************************************************************
 Close a pipe - free the handle list if it was the last pipe reference.
****************************************************************************/

void close_policy_by_pipe(pipes_struct *p)
{
	p->pipe_handles->pipe_ref_count--;

	if (p->pipe_handles->pipe_ref_count == 0) {
		/*
		 * Last pipe open on this list - free the list.
		 */
		while (p->pipe_handles->Policy)
			close_policy_hnd(p, &p->pipe_handles->Policy->pol_hnd);

		p->pipe_handles->Policy = NULL;
		p->pipe_handles->count = 0;

		SAFE_FREE(p->pipe_handles);
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

bool pipe_access_check(pipes_struct *p)
{
	/* Don't let anonymous users access this RPC if restrict
	   anonymous > 0 */

	if (lp_restrict_anonymous() > 0) {

		/* schannel, so we must be ok */
		if (p->pipe_bound && (p->auth.auth_type == PIPE_AUTH_TYPE_SCHANNEL)) {
			return True;
		}

		if (p->server_info->guest) {
			return False;
		}
	}

	return True;
}

void *_policy_handle_create(struct pipes_struct *p, struct policy_handle *hnd,
			    uint32_t access_granted, size_t data_size,
			    const char *type, NTSTATUS *pstatus)
{
	struct policy *pol;
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

	pol = create_policy_hnd_internal(p, hnd, data);
	if (pol == NULL) {
		TALLOC_FREE(data);
		*pstatus = NT_STATUS_NO_MEMORY;
		return NULL;
	}
	pol->access_granted = access_granted;
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
	struct policy *pol;
	void *data;

	pol = find_policy_by_hnd_internal(p, hnd, &data);
	if (pol == NULL) {
		*pstatus = NT_STATUS_INVALID_HANDLE;
		return NULL;
	}
	if (strcmp(name, talloc_get_name(data)) != 0) {
		DEBUG(10, ("expected %s, got %s\n", name,
			   talloc_get_name(data)));
		*pstatus = NT_STATUS_INVALID_HANDLE;
		return NULL;
	}
	if ((access_required & pol->access_granted) != access_required) {
		if (geteuid() == sec_initial_uid()) {
			DEBUG(4, ("%s: ACCESS should be DENIED (granted: "
				  "%#010x; required: %#010x)\n", location,
				  pol->access_granted, access_required));
			DEBUGADD(4,("but overwritten by euid == 0\n"));
			goto okay;
		}
		DEBUG(2,("%s: ACCESS DENIED (granted: %#010x; required: "
			 "%#010x)\n", location, pol->access_granted,
			 access_required));
		*pstatus = NT_STATUS_ACCESS_DENIED;
		return NULL;
	}

 okay:
	DEBUG(10, ("found handle of type %s\n", talloc_get_name(data)));
	if (paccess_granted != NULL) {
		*paccess_granted = pol->access_granted;
	}
	*pstatus = NT_STATUS_OK;
	return data;
}
