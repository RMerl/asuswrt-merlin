/* 
   Unix SMB/CIFS implementation.
   default print NTVFS backend
   Copyright (C) Andrew Tridgell  2003

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
/*
  this implements the print backend, called by the NTVFS subsystem to
  handle requests on printing shares
*/

#include "includes.h"
#include "libcli/raw/ioctl.h"
#include "ntvfs/ntvfs.h"
#include "param/param.h"

/*
  connect to a share - used when a tree_connect operation comes
  in. For printing shares this should check that the spool directory
  is available
*/
static NTSTATUS print_connect(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_tcon* tcon)
{
	ntvfs->ctx->fs_type = talloc_strdup(ntvfs->ctx, "NTFS");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->fs_type);

	ntvfs->ctx->dev_type = talloc_strdup(ntvfs->ctx, "LPT1:");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->dev_type);

	if (tcon->generic.level == RAW_TCON_TCONX) {
		tcon->tconx.out.fs_type = ntvfs->ctx->fs_type;
		tcon->tconx.out.dev_type = ntvfs->ctx->dev_type;
	}

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS print_disconnect(struct ntvfs_module_context *ntvfs)
{
	return NT_STATUS_OK;
}

/*
  lots of operations are not allowed on printing shares - mostly return NT_STATUS_ACCESS_DENIED
*/
static NTSTATUS print_unlink(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_unlink *unl)
{
	return NT_STATUS_ACCESS_DENIED;
}


/*
  ioctl - used for job query
*/
static NTSTATUS print_ioctl(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_ioctl *io)
{
	char *p;

	if (io->generic.level != RAW_IOCTL_IOCTL) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	if (io->ioctl.in.request == IOCTL_QUERY_JOB_INFO) {

		/* a request for the print job id of an open print job */
		io->ioctl.out.blob = data_blob_talloc(req, NULL, 32);

		data_blob_clear(&io->ioctl.out.blob);

		p = (char *)io->ioctl.out.blob.data;
		SSVAL(p,0, 1 /* REWRITE: fsp->rap_print_jobid */);
		push_string(p+2, lpcfg_netbios_name(ntvfs->ctx->lp_ctx), 15, STR_TERMINATE|STR_ASCII);
		push_string(p+18, ntvfs->ctx->config->name, 13, STR_TERMINATE|STR_ASCII);
		return NT_STATUS_OK;
	}

	return NT_STATUS_INVALID_PARAMETER;
}


/*
  initialialise the print backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_print_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	/* fill in the name and type */
	ops.name = "default";
	ops.type = NTVFS_PRINT;
	
	/* fill in all the operations */
	ops.connect = print_connect;
	ops.disconnect = print_disconnect;
	ops.unlink = print_unlink;
	ops.ioctl = print_ioctl;

	/* register ourselves with the NTVFS subsystem. We register under the name 'default'
	   as we wish to be the default backend */
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register PRINT backend!\n"));
	}

	return ret;
}
