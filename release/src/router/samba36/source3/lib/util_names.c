/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2007
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) James Peach 2006
   Copyright (C) Andrew Bartlett 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

static char *smb_myname;
static char *smb_myworkgroup;

/***********************************************************************
 Allocate and set myname. Ensure upper case.
***********************************************************************/

bool set_global_myname(const char *myname)
{
	SAFE_FREE(smb_myname);
	smb_myname = SMB_STRDUP(myname);
	if (!smb_myname)
		return False;
	strupper_m(smb_myname);
	return True;
}

const char *global_myname(void)
{
	return smb_myname;
}

/***********************************************************************
 Allocate and set myworkgroup. Ensure upper case.
***********************************************************************/

bool set_global_myworkgroup(const char *myworkgroup)
{
	SAFE_FREE(smb_myworkgroup);
	smb_myworkgroup = SMB_STRDUP(myworkgroup);
	if (!smb_myworkgroup)
		return False;
	strupper_m(smb_myworkgroup);
	return True;
}

const char *lp_workgroup(void)
{
	return smb_myworkgroup;
}

/******************************************************************
 get the default domain/netbios name to be used when dealing
 with our passdb list of accounts
******************************************************************/

const char *get_global_sam_name(void)
{
	if (IS_DC) {
		return lp_workgroup();
	}
	return global_myname();
}

void gfree_netbios_names(void)
{
	SAFE_FREE( smb_myname );
	SAFE_FREE( smb_myworkgroup );
}
