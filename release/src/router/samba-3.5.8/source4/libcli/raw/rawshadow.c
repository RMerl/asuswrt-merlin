/* 
   Unix SMB/CIFS implementation.

   shadow copy file operations

   Copyright (C) Andrew Tridgell 2007
   
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
#include "libcli/raw/ioctl.h"

/* 
   get shadow volume data
*/
_PUBLIC_ NTSTATUS smb_raw_shadow_data(struct smbcli_tree *tree, 
				      TALLOC_CTX *mem_ctx, struct smb_shadow_copy *info)
{
	union smb_ioctl nt;
	NTSTATUS status;
	DATA_BLOB blob;
	uint32_t dlength;
	int i;
	uint32_t ofs;

	nt.ntioctl.level        = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function  = FSCTL_GET_SHADOW_COPY_DATA;
	nt.ntioctl.in.file.fnum = info->in.file.fnum;
	nt.ntioctl.in.fsctl     = true;
	nt.ntioctl.in.filter    = 0;
	nt.ntioctl.in.max_data  = info->in.max_data;
	nt.ntioctl.in.blob      = data_blob(NULL, 0);

	status = smb_raw_ioctl(tree, mem_ctx, &nt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	blob = nt.ntioctl.out.blob;

	if (blob.length < 12) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	
	info->out.num_volumes = IVAL(blob.data, 0);
	info->out.num_names   = IVAL(blob.data, 4);
	dlength               = IVAL(blob.data, 8);
	if (dlength > blob.length - 12) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	info->out.names = talloc_array(mem_ctx, const char *, info->out.num_names);
	NT_STATUS_HAVE_NO_MEMORY(info->out.names);

	ofs = 12;
	for (i=0;i<info->out.num_names;i++) {
		size_t len;
		len = smbcli_blob_pull_ucs2(info->out.names, 
				      &blob, &info->out.names[i],
				      blob.data+ofs, -1, STR_TERMINATE);
		if (len == 0) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}
		ofs += len;
	}
	
	return status;
}
