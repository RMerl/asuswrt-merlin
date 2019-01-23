/* 
   Unix SMB/CIFS implementation.

   SMB2 client utility functions

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/raw/raw_proto.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "libcli/smb_composite/smb_composite.h"
#include "librpc/gen_ndr/ndr_security.h"

/*
  simple close wrapper with SMB2
*/
NTSTATUS smb2_util_close(struct smb2_tree *tree, struct smb2_handle h)
{
	struct smb2_close c;

	ZERO_STRUCT(c);
	c.in.file.handle = h;

	return smb2_close(tree, &c);
}

/*
  unlink a file with SMB2
*/
NTSTATUS smb2_util_unlink(struct smb2_tree *tree, const char *fname)
{
	union smb_unlink io;
	
	ZERO_STRUCT(io);
	io.unlink.in.pattern = fname;

	return smb2_composite_unlink(tree, &io);
}


/*
  rmdir with SMB2
*/
NTSTATUS smb2_util_rmdir(struct smb2_tree *tree, const char *dname)
{
	struct smb_rmdir io;
	
	ZERO_STRUCT(io);
	io.in.path = dname;

	return smb2_composite_rmdir(tree, &io);
}


/*
  mkdir with SMB2
*/
NTSTATUS smb2_util_mkdir(struct smb2_tree *tree, const char *dname)
{
	union smb_mkdir io;
	
	ZERO_STRUCT(io);
	io.mkdir.level = RAW_MKDIR_MKDIR;
	io.mkdir.in.path = dname;

	return smb2_composite_mkdir(tree, &io);
}


/*
  set file attribute with SMB2
*/
NTSTATUS smb2_util_setatr(struct smb2_tree *tree, const char *name, uint32_t attrib)
{
	union smb_setfileinfo io;
	
	ZERO_STRUCT(io);
	io.basic_info.level = RAW_SFILEINFO_BASIC_INFORMATION;
	io.basic_info.in.file.path = name;
	io.basic_info.in.attrib = attrib;

	return smb2_composite_setpathinfo(tree, &io);
}




/* 
   recursively descend a tree deleting all files
   returns the number of files deleted, or -1 on error
*/
int smb2_deltree(struct smb2_tree *tree, const char *dname)
{
	NTSTATUS status;
	uint32_t total_deleted = 0;
	unsigned int count, i;
	union smb_search_data *list;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);
	struct smb2_find f;
	struct smb2_create create_parm;
	bool did_delete;

	/* it might be a file */
	status = smb2_util_unlink(tree, dname);
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return 1;
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_PATH_NOT_FOUND) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_FILE)) {
		talloc_free(tmp_ctx);
		return 0;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
		/* it could be read-only */
		status = smb2_util_setatr(tree, dname, FILE_ATTRIBUTE_NORMAL);
		status = smb2_util_unlink(tree, dname);
	}
	if (NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return 1;
	}

	ZERO_STRUCT(create_parm);
	create_parm.in.desired_access = SEC_FILE_READ_DATA;
	create_parm.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	create_parm.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	create_parm.in.create_disposition = NTCREATEX_DISP_OPEN;
	create_parm.in.fname = dname;

	status = smb2_create(tree, tmp_ctx, &create_parm);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(2,("Failed to open %s - %s\n", dname, nt_errstr(status)));
		talloc_free(tmp_ctx);
		return -1;
	}
	

	do {
		did_delete = false;

		ZERO_STRUCT(f);
		f.in.file.handle       = create_parm.out.file.handle;
		f.in.max_response_size = 0x10000;
		f.in.level             = SMB2_FIND_NAME_INFO;
		f.in.pattern           = "*";
		
		status = smb2_find_level(tree, tmp_ctx, &f, &count, &list);
		if (NT_STATUS_IS_ERR(status)) {
			DEBUG(2,("Failed to list %s - %s\n", 
				 dname, nt_errstr(status)));
			smb2_util_close(tree, create_parm.out.file.handle);
			talloc_free(tmp_ctx);
			return -1;
		}
		
		for (i=0;i<count;i++) {
			char *name;
			if (strcmp(".", list[i].name_info.name.s) == 0 ||
			    strcmp("..", list[i].name_info.name.s) == 0) {
				continue;
			}
			name = talloc_asprintf(tmp_ctx, "%s\\%s", dname, list[i].name_info.name.s);
			status = smb2_util_unlink(tree, name);
			if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
				/* it could be read-only */
				status = smb2_util_setatr(tree, name, FILE_ATTRIBUTE_NORMAL);
				status = smb2_util_unlink(tree, name);
			}
			
			if (NT_STATUS_EQUAL(status, NT_STATUS_FILE_IS_A_DIRECTORY)) {
				int ret;
				ret = smb2_deltree(tree, name);
				if (ret > 0) total_deleted += ret;
			}
			talloc_free(name);
			if (NT_STATUS_IS_OK(status)) {
				total_deleted++;
				did_delete = true;
			}
		}
	} while (did_delete);

	smb2_util_close(tree, create_parm.out.file.handle);

	status = smb2_util_rmdir(tree, dname);
	if (NT_STATUS_EQUAL(status, NT_STATUS_CANNOT_DELETE)) {
		/* it could be read-only */
		status = smb2_util_setatr(tree, dname, FILE_ATTRIBUTE_NORMAL);
		status = smb2_util_rmdir(tree, dname);
	}

	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(2,("Failed to delete %s - %s\n", 
			 dname, nt_errstr(status)));
		talloc_free(tmp_ctx);
		return -1;
	}

	talloc_free(tmp_ctx);

	return total_deleted;
}

/*
  check if two SMB2 file handles are the same
*/
bool smb2_util_handle_equal(const struct smb2_handle h1,
			    const struct smb2_handle h2)
{
	return (h1.data[0] == h2.data[0]) && (h1.data[1] == h2.data[1]);
}
