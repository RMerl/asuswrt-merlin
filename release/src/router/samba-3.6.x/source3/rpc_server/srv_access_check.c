/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell                   1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton      1996-1997,
 *  Copyright (C) Paul Ashton                       1997,
 *  Copyright (C) Marc Jacobsen			    1999,
 *  Copyright (C) Jeremy Allison                    2001-2008,
 *  Copyright (C) Jean François Micouleau           1998-2001,
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com>   2002,
 *  Copyright (C) Gerald (Jerry) Carter             2003-2004,
 *  Copyright (C) Simo Sorce                        2003.
 *  Copyright (C) Volker Lendecke		    2005.
 *  Copyright (C) Guenther Deschner		    2008.
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
#include "rpc_server/srv_access_check.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"

/*******************************************************************
 Checks if access to an object should be granted, and returns that
 level of access for further checks.

 If the user has either of needed_priv_1 or needed_priv_2 then they
 get the rights in rights_mask in addition to any calulated rights.

 This handles the unusual case where we need to allow two different
 privileges to obtain exactly the same rights, which occours only in
 SAMR.
********************************************************************/

NTSTATUS access_check_object( struct security_descriptor *psd, struct security_token *token,
			      enum sec_privilege needed_priv_1, enum sec_privilege needed_priv_2,
			      uint32 rights_mask,
			      uint32 des_access, uint32 *acc_granted,
			      const char *debug )
{
	NTSTATUS status = NT_STATUS_ACCESS_DENIED;
	uint32 saved_mask = 0;
	bool priv_granted = false;

	/* check privileges; certain SAM access bits should be overridden
	   by privileges (mostly having to do with creating/modifying/deleting
	   users and groups) */

	if ((needed_priv_1 != SEC_PRIV_INVALID && security_token_has_privilege(token, needed_priv_1)) ||
	    (needed_priv_2 != SEC_PRIV_INVALID && security_token_has_privilege(token, needed_priv_2))) {
		priv_granted = true;
		saved_mask = (des_access & rights_mask);
		des_access &= ~saved_mask;

		DEBUG(4,("access_check_object: user rights access mask [0x%x]\n",
			rights_mask));
	}


	/* check the security descriptor first */

	status = se_access_check(psd, token, des_access, acc_granted);
	if (NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* give root a free pass */

	if ( geteuid() == sec_initial_uid() ) {

		DEBUG(4,("%s: ACCESS should be DENIED  (requested: %#010x)\n", debug, des_access));
		DEBUGADD(4,("but overritten by euid == sec_initial_uid()\n"));

		priv_granted = true;
		*acc_granted = des_access;

		status = NT_STATUS_OK;
		goto done;
	}


done:
	if (priv_granted) {
		/* add in any bits saved during the privilege check (only
		   matters if status is ok) */

		*acc_granted |= rights_mask;
	}

	DEBUG(4,("%s: access %s (requested: 0x%08x, granted: 0x%08x)\n",
		debug, NT_STATUS_IS_OK(status) ? "GRANTED" : "DENIED",
		des_access, *acc_granted));

	return status;
}


/*******************************************************************
 Map any MAXIMUM_ALLOWED_ACCESS request to a valid access set.
********************************************************************/

void map_max_allowed_access(const struct security_token *nt_token,
			    const struct security_unix_token *unix_token,
			    uint32_t *pacc_requested)
{
	if (!((*pacc_requested) & MAXIMUM_ALLOWED_ACCESS)) {
		return;
	}
	*pacc_requested &= ~MAXIMUM_ALLOWED_ACCESS;

	/* At least try for generic read|execute - Everyone gets that. */
	*pacc_requested = GENERIC_READ_ACCESS|GENERIC_EXECUTE_ACCESS;

	/* root gets anything. */
	if (unix_token->uid == sec_initial_uid()) {
		*pacc_requested |= GENERIC_ALL_ACCESS;
		return;
	}

	/* Full Access for 'BUILTIN\Administrators' and 'BUILTIN\Account Operators */

	if (security_token_has_sid(nt_token, &global_sid_Builtin_Administrators) ||
			security_token_has_sid(nt_token, &global_sid_Builtin_Account_Operators)) {
		*pacc_requested |= GENERIC_ALL_ACCESS;
		return;
	}

	/* Full access for DOMAIN\Domain Admins. */
	if ( IS_DC ) {
		struct dom_sid domadmin_sid;
		sid_compose(&domadmin_sid, get_global_sam_sid(),
			    DOMAIN_RID_ADMINS);
		if (security_token_has_sid(nt_token, &domadmin_sid)) {
			*pacc_requested |= GENERIC_ALL_ACCESS;
			return;
		}
	}
	/* TODO ! Check privileges. */
}
