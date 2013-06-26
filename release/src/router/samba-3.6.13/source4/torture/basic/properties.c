/* 
   Unix SMB/CIFS implementation.

   show server properties

   Copyright (C) Andrew Tridgell 2004
   
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
#include "libcli/libcli.h"
#include "torture/util.h"

struct bitmapping {
	const char *name;
	uint32_t value;
};

#define BIT_NAME(x) { #x, x }

const static struct bitmapping fs_attr_bits[] = {
	BIT_NAME(FS_ATTR_CASE_SENSITIVE_SEARCH),
	BIT_NAME(FS_ATTR_CASE_PRESERVED_NAMES),
	BIT_NAME(FS_ATTR_UNICODE_ON_DISK),
	BIT_NAME(FS_ATTR_PERSISTANT_ACLS),
	BIT_NAME(FS_ATTR_COMPRESSION),
	BIT_NAME(FS_ATTR_QUOTAS),
	BIT_NAME(FS_ATTR_SPARSE_FILES),
	BIT_NAME(FS_ATTR_REPARSE_POINTS),
	BIT_NAME(FS_ATTR_REMOTE_STORAGE),
	BIT_NAME(FS_ATTR_LFN_SUPPORT),
	BIT_NAME(FS_ATTR_IS_COMPRESSED),
	BIT_NAME(FS_ATTR_OBJECT_IDS),
	BIT_NAME(FS_ATTR_ENCRYPTION),
	BIT_NAME(FS_ATTR_NAMED_STREAMS),
	{ NULL, 0 }
};

const static struct bitmapping capability_bits[] = {
	BIT_NAME(CAP_RAW_MODE),
	BIT_NAME(CAP_MPX_MODE),
	BIT_NAME(CAP_UNICODE),
	BIT_NAME(CAP_LARGE_FILES),
	BIT_NAME(CAP_NT_SMBS),
	BIT_NAME(CAP_RPC_REMOTE_APIS),
	BIT_NAME(CAP_STATUS32),
	BIT_NAME(CAP_LEVEL_II_OPLOCKS),
	BIT_NAME(CAP_LOCK_AND_READ),
	BIT_NAME(CAP_NT_FIND),
	BIT_NAME(CAP_DFS),
	BIT_NAME(CAP_W2K_SMBS),
	BIT_NAME(CAP_LARGE_READX),
	BIT_NAME(CAP_LARGE_WRITEX),
	BIT_NAME(CAP_UNIX),
	BIT_NAME(CAP_EXTENDED_SECURITY),
	{ NULL, 0 }
};

static void show_bits(const struct bitmapping *bm, uint32_t value)
{
	int i;
	for (i=0;bm[i].name;i++) {
		if (value & bm[i].value) {
			d_printf("\t%s\n", bm[i].name);
			value &= ~bm[i].value;
		}
	}
	if (value != 0) {
		d_printf("\tunknown bits: 0x%08x\n", value);
	}
}


/*
  print out server properties
 */
bool torture_test_properties(struct torture_context *torture, 
			     struct smbcli_state *cli)
{
	bool correct = true;
	union smb_fsinfo fs;
	NTSTATUS status;
	
	d_printf("Capabilities: 0x%08x\n", cli->transport->negotiate.capabilities);
	show_bits(capability_bits, cli->transport->negotiate.capabilities);
	d_printf("\n");

	fs.attribute_info.level = RAW_QFS_ATTRIBUTE_INFO;
	status = smb_raw_fsinfo(cli->tree, cli, &fs);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("qfsinfo failed - %s\n", nt_errstr(status));
		correct = false;
	} else {
		d_printf("Filesystem attributes: 0x%08x\n", 
			 fs.attribute_info.out.fs_attr);
		show_bits(fs_attr_bits, fs.attribute_info.out.fs_attr);
		d_printf("max_file_component_length: %d\n", 
			 fs.attribute_info.out.max_file_component_length);
		d_printf("fstype: %s\n", fs.attribute_info.out.fs_type.s);
	}

	return correct;
}


