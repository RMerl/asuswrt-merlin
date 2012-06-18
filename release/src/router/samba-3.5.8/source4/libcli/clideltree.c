/* 
   Unix SMB/CIFS implementation.
   useful function for deleting a whole directory tree
   Copyright (C) Andrew Tridgell 2003
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "system/dir.h"

struct delete_state {
	struct smbcli_tree *tree;
	int total_deleted;
	bool failed;
};

/* 
   callback function for torture_deltree() 
*/
static void delete_fn(struct clilist_file_info *finfo, const char *name, void *state)
{
	struct delete_state *dstate = (struct delete_state *)state;
	char *s, *n;
	if (ISDOT(finfo->name) || ISDOTDOT(finfo->name)) {
		return;
	}

	n = strdup(name);
	n[strlen(n)-1] = 0;
	asprintf(&s, "%s%s", n, finfo->name);

	if (finfo->attrib & FILE_ATTRIBUTE_READONLY) {
		if (NT_STATUS_IS_ERR(smbcli_setatr(dstate->tree, s, 0, 0))) {
			DEBUG(2,("Failed to remove READONLY on %s - %s\n",
				 s, smbcli_errstr(dstate->tree)));			
		}
	}

	if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY) {
		char *s2;
		asprintf(&s2, "%s\\*", s);
		smbcli_unlink(dstate->tree, s2);
		smbcli_list(dstate->tree, s2, 
			 FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, 
			 delete_fn, state);
		free(s2);
		if (NT_STATUS_IS_ERR(smbcli_rmdir(dstate->tree, s))) {
			DEBUG(2,("Failed to delete %s - %s\n", 
				 s, smbcli_errstr(dstate->tree)));
			dstate->failed = true;
		}
		dstate->total_deleted++;
	} else {
		if (NT_STATUS_IS_ERR(smbcli_unlink(dstate->tree, s))) {
			DEBUG(2,("Failed to delete %s - %s\n", 
				 s, smbcli_errstr(dstate->tree)));
			dstate->failed = true;
		}
		dstate->total_deleted++;
	}
	free(s);
	free(n);
}

/* 
   recursively descend a tree deleting all files
   returns the number of files deleted, or -1 on error
*/
int smbcli_deltree(struct smbcli_tree *tree, const char *dname)
{
	char *mask;
	struct delete_state dstate;
	NTSTATUS status;

	dstate.tree = tree;
	dstate.total_deleted = 0;
	dstate.failed = false;

	/* it might be a file */
	status = smbcli_unlink(tree, dname);
	if (NT_STATUS_IS_OK(smbcli_unlink(tree, dname))) {
		return 1;
	}
	if (NT_STATUS_EQUAL(smbcli_nt_error(tree), NT_STATUS_OBJECT_NAME_NOT_FOUND) ||
	    NT_STATUS_EQUAL(smbcli_nt_error(tree), NT_STATUS_OBJECT_PATH_NOT_FOUND) ||
	    NT_STATUS_EQUAL(smbcli_nt_error(tree), NT_STATUS_NO_SUCH_FILE) ||
	    NT_STATUS_EQUAL(smbcli_nt_error(tree), NT_STATUS_DOS(ERRDOS, ERRbadfile))) {
		return 0;
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
		/* it could be read-only */
		status = smbcli_setatr(tree, dname, FILE_ATTRIBUTE_NORMAL, 0);
		if (NT_STATUS_IS_OK(smbcli_unlink(tree, dname))) {
			return 1;
		}
	}

	asprintf(&mask, "%s\\*", dname);
	smbcli_unlink(dstate.tree, mask);
	smbcli_list(dstate.tree, mask, 
		 FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, 
		 delete_fn, &dstate);
	free(mask);

	status = smbcli_rmdir(dstate.tree, dname);
	if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
		/* it could be read-only */
		status = smbcli_setatr(dstate.tree, dname, FILE_ATTRIBUTE_NORMAL, 0);
		status = smbcli_rmdir(dstate.tree, dname);
	}
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(2,("Failed to delete %s - %s\n", 
			 dname, smbcli_errstr(dstate.tree)));
		return -1;
	}
	dstate.total_deleted++;

	if (dstate.failed) {
		return -1;
	}

	return dstate.total_deleted;
}
