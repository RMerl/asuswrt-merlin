/* 
   Unix SMB/CIFS implementation.
   client trans2 calls
   Copyright (C) James J Myers 2003	<myersjj@samba.org>
   
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

/****************************************************************************
send a qpathinfo call
****************************************************************************/
NTSTATUS smbcli_qpathinfo(struct smbcli_tree *tree, const char *fname, 
		       time_t *c_time, time_t *a_time, time_t *m_time, 
		       size_t *size, uint16_t *mode)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("smbcli_qpathinfo");
	if (!mem_ctx) return NT_STATUS_NO_MEMORY;

	parms.standard.level = RAW_FILEINFO_STANDARD;
	parms.standard.in.file.path = fname;

	status = smb_raw_pathinfo(tree, mem_ctx, &parms);
	talloc_free(mem_ctx);
	if (!NT_STATUS_IS_OK(status))
		return status;

	if (c_time) {
		*c_time = parms.standard.out.create_time;
	}
	if (a_time) {
		*a_time = parms.standard.out.access_time;
	}
	if (m_time) {
		*m_time = parms.standard.out.write_time;
	}
	if (size) {
		*size = parms.standard.out.size;
	}
	if (mode) {
		*mode = parms.standard.out.attrib;
	}

	return status;
}

/****************************************************************************
send a qpathinfo call with the SMB_QUERY_FILE_ALL_INFO info level
****************************************************************************/
NTSTATUS smbcli_qpathinfo2(struct smbcli_tree *tree, const char *fname, 
			time_t *c_time, time_t *a_time, time_t *m_time, 
			time_t *w_time, size_t *size, uint16_t *mode,
			ino_t *ino)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("smbcli_qfilename");
	if (!mem_ctx) return NT_STATUS_NO_MEMORY;

	parms.all_info.level = RAW_FILEINFO_ALL_INFO;
	parms.all_info.in.file.path = fname;

	status = smb_raw_pathinfo(tree, mem_ctx, &parms);
	talloc_free(mem_ctx);
	if (!NT_STATUS_IS_OK(status))
		return status;

	if (c_time) {
		*c_time = nt_time_to_unix(parms.all_info.out.create_time);
	}
	if (a_time) {
		*a_time = nt_time_to_unix(parms.all_info.out.access_time);
	}
	if (m_time) {
		*m_time = nt_time_to_unix(parms.all_info.out.change_time);
	}
	if (w_time) {
		*w_time = nt_time_to_unix(parms.all_info.out.write_time);
	}
	if (size) {
		*size = parms.all_info.out.size;
	}
	if (mode) {
		*mode = parms.all_info.out.attrib;
	}

	return status;
}


/****************************************************************************
send a qfileinfo QUERY_FILE_NAME_INFO call
****************************************************************************/
NTSTATUS smbcli_qfilename(struct smbcli_tree *tree, int fnum, const char **name)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("smbcli_qfilename");
	if (!mem_ctx) return NT_STATUS_NO_MEMORY;

	parms.name_info.level = RAW_FILEINFO_NAME_INFO;
	parms.name_info.in.file.fnum = fnum;

	status = smb_raw_fileinfo(tree, mem_ctx, &parms);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		*name = NULL;
		return status;
	}

	*name = strdup(parms.name_info.out.fname.s);

	talloc_free(mem_ctx);

	return status;
}


/****************************************************************************
send a qfileinfo call
****************************************************************************/
NTSTATUS smbcli_qfileinfo(struct smbcli_tree *tree, int fnum, 
		       uint16_t *mode, size_t *size,
		       time_t *c_time, time_t *a_time, time_t *m_time, 
		       time_t *w_time, ino_t *ino)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	mem_ctx = talloc_init("smbcli_qfileinfo");
	if (!mem_ctx) 
		return NT_STATUS_NO_MEMORY;

	parms.all_info.level = RAW_FILEINFO_ALL_INFO;
	parms.all_info.in.file.fnum = fnum;

	status = smb_raw_fileinfo(tree, mem_ctx, &parms);
	talloc_free(mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (c_time) {
		*c_time = nt_time_to_unix(parms.all_info.out.create_time);
	}
	if (a_time) {
		*a_time = nt_time_to_unix(parms.all_info.out.access_time);
	}
	if (m_time) {
		*m_time = nt_time_to_unix(parms.all_info.out.change_time);
	}
	if (w_time) {
		*w_time = nt_time_to_unix(parms.all_info.out.write_time);
	}
	if (mode) {
		*mode = parms.all_info.out.attrib;
	}
	if (size) {
		*size = (size_t)parms.all_info.out.size;
	}
	if (ino) {
		*ino = 0;
	}

	return status;
}


/****************************************************************************
send a qpathinfo SMB_QUERY_FILE_ALT_NAME_INFO call
****************************************************************************/
NTSTATUS smbcli_qpathinfo_alt_name(struct smbcli_tree *tree, const char *fname, 
				const char **alt_name)
{
	union smb_fileinfo parms;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	parms.alt_name_info.level = RAW_FILEINFO_ALT_NAME_INFO;
	parms.alt_name_info.in.file.path = fname;

	mem_ctx = talloc_init("smbcli_qpathinfo_alt_name");
	if (!mem_ctx) return NT_STATUS_NO_MEMORY;

	status = smb_raw_pathinfo(tree, mem_ctx, &parms);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		*alt_name = NULL;
		return smbcli_nt_error(tree);
	}

	if (!parms.alt_name_info.out.fname.s) {
		*alt_name = strdup("");
	} else {
		*alt_name = strdup(parms.alt_name_info.out.fname.s);
	}

	talloc_free(mem_ctx);

	return NT_STATUS_OK;
}
