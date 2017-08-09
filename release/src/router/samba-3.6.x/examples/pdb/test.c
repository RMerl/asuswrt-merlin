/*
 * Test password backend for samba
 * Copyright (C) Jelmer Vernooij 2002
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include "includes.h"

static int testsam_debug_level = DBGC_ALL;

#undef DBGC_CLASS
#define DBGC_CLASS testsam_debug_level

/***************************************************************
 Start enumeration of the passwd list.
****************************************************************/

static NTSTATUS testsam_setsampwent(struct pdb_methods *methods, BOOL update, uint32 acb_mask)
{
	DEBUG(10, ("testsam_setsampwent called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************
 End enumeration of the passwd list.
****************************************************************/

static void testsam_endsampwent(struct pdb_methods *methods)
{
	DEBUG(10, ("testsam_endsampwent called\n"));
}

/*****************************************************************
 Get one struct samu from the list (next in line)
*****************************************************************/

static NTSTATUS testsam_getsampwent(struct pdb_methods *methods, struct samu *user)
{
	DEBUG(10, ("testsam_getsampwent called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/******************************************************************
 Lookup a name in the SAM database
******************************************************************/

static NTSTATUS testsam_getsampwnam (struct pdb_methods *methods, struct samu *user, const char *sname)
{
	DEBUG(10, ("testsam_getsampwnam called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************************
 Search by sid
 **************************************************************************/

static NTSTATUS testsam_getsampwsid (struct pdb_methods *methods, struct samu *user, const struct dom_sid *sid)
{
	DEBUG(10, ("testsam_getsampwsid called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************************
 Delete a struct samu
****************************************************************************/

static NTSTATUS testsam_delete_sam_account(struct pdb_methods *methods, struct samu *sam_pass)
{
	DEBUG(10, ("testsam_delete_sam_account called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************************
 Modifies an existing struct samu
****************************************************************************/

static NTSTATUS testsam_update_sam_account (struct pdb_methods *methods, struct samu *newpwd)
{
	DEBUG(10, ("testsam_update_sam_account called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

/***************************************************************************
 Adds an existing struct samu
****************************************************************************/

static NTSTATUS testsam_add_sam_account (struct pdb_methods *methods, struct samu *newpwd)
{
	DEBUG(10, ("testsam_add_sam_account called\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS testsam_init(struct pdb_methods **pdb_method, const char *location)
{
	NTSTATUS nt_status;

	if (!NT_STATUS_IS_OK(nt_status = make_pdb_method( pdb_method ))) {
		return nt_status;
	}

	(*pdb_method)->name = "testsam";

	/* Functions your pdb module doesn't provide should not be
	   set, make_pdb_methods() already provide suitable defaults for missing functions */

	(*pdb_method)->setsampwent = testsam_setsampwent;
	(*pdb_method)->endsampwent = testsam_endsampwent;
	(*pdb_method)->getsampwent = testsam_getsampwent;
	(*pdb_method)->getsampwnam = testsam_getsampwnam;
	(*pdb_method)->getsampwsid = testsam_getsampwsid;
	(*pdb_method)->add_sam_account = testsam_add_sam_account;
	(*pdb_method)->update_sam_account = testsam_update_sam_account;
	(*pdb_method)->delete_sam_account = testsam_delete_sam_account;

	testsam_debug_level = debug_add_class("testsam");
	if (testsam_debug_level == -1) {
		testsam_debug_level = DBGC_ALL;
		DEBUG(0, ("testsam: Couldn't register custom debugging class!\n"));
	} else DEBUG(0, ("testsam: Debug class number of 'testsam': %d\n", testsam_debug_level));
    
	DEBUG(0, ("Initializing testsam\n"));
	if (location)
		DEBUG(10, ("Location: %s\n", location));

	return NT_STATUS_OK;
}

NTSTATUS init_module(void) {
	return smb_register_passdb(PASSDB_INTERFACE_VERSION, "testsam",
				   testsam_init);
}
