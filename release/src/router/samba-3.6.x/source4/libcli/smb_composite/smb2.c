/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2008
   
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
  a composite API for making SMB-like calls using SMB2. This is useful
  as SMB2 often requires more than one requests where a single SMB
  request would do. In converting code that uses SMB to use SMB2,
  these routines make life a lot easier
*/


#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/smb2/smb2_calls.h"

/*
  continue after a SMB2 close
 */
static void continue_close(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	NTSTATUS status;
	struct smb2_close close_parm;

	status = smb2_close_recv(req, &close_parm);
	composite_error(ctx, status);	
}

/*
  continue after the create in a composite unlink
 */
static void continue_unlink(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	struct smb2_tree *tree = req->tree;
	struct smb2_create create_parm;
	struct smb2_close close_parm;
	NTSTATUS status;

	status = smb2_create_recv(req, ctx, &create_parm);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(ctx, status);
		return;
	}

	ZERO_STRUCT(close_parm);
	close_parm.in.file.handle = create_parm.out.file.handle;
	
	req = smb2_close_send(tree, &close_parm);
	composite_continue_smb2(ctx, req, continue_close, ctx);
}

/*
  composite SMB2 unlink call
*/
struct composite_context *smb2_composite_unlink_send(struct smb2_tree *tree, 
						     union smb_unlink *io)
{
	struct composite_context *ctx;
	struct smb2_create create_parm;
	struct smb2_request *req;

	ctx = composite_create(tree, tree->session->transport->socket->event.ctx);
	if (ctx == NULL) return NULL;

	/* check for wildcards - we could support these with a
	   search, but for now they aren't necessary */
	if (strpbrk(io->unlink.in.pattern, "*?<>") != NULL) {
		composite_error(ctx, NT_STATUS_NOT_SUPPORTED);
		return ctx;
	}

	ZERO_STRUCT(create_parm);
	create_parm.in.desired_access     = SEC_STD_DELETE;
	create_parm.in.create_disposition = NTCREATEX_DISP_OPEN;
	create_parm.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	create_parm.in.create_options = 
		NTCREATEX_OPTIONS_DELETE_ON_CLOSE |
		NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	create_parm.in.fname = io->unlink.in.pattern;
	if (create_parm.in.fname[0] == '\\') {
		create_parm.in.fname++;
	}

	req = smb2_create_send(tree, &create_parm);

	composite_continue_smb2(ctx, req, continue_unlink, ctx);
	return ctx;
}


/*
  composite unlink call - sync interface
*/
NTSTATUS smb2_composite_unlink(struct smb2_tree *tree, union smb_unlink *io)
{
	struct composite_context *c = smb2_composite_unlink_send(tree, io);
	return composite_wait_free(c);
}




/*
  continue after the create in a composite mkdir
 */
static void continue_mkdir(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	struct smb2_tree *tree = req->tree;
	struct smb2_create create_parm;
	struct smb2_close close_parm;
	NTSTATUS status;

	status = smb2_create_recv(req, ctx, &create_parm);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(ctx, status);
		return;
	}

	ZERO_STRUCT(close_parm);
	close_parm.in.file.handle = create_parm.out.file.handle;
	
	req = smb2_close_send(tree, &close_parm);
	composite_continue_smb2(ctx, req, continue_close, ctx);
}

/*
  composite SMB2 mkdir call
*/
struct composite_context *smb2_composite_mkdir_send(struct smb2_tree *tree, 
						     union smb_mkdir *io)
{
	struct composite_context *ctx;
	struct smb2_create create_parm;
	struct smb2_request *req;

	ctx = composite_create(tree, tree->session->transport->socket->event.ctx);
	if (ctx == NULL) return NULL;

	ZERO_STRUCT(create_parm);

	create_parm.in.desired_access = SEC_FLAG_MAXIMUM_ALLOWED;
	create_parm.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	create_parm.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	create_parm.in.file_attributes   = FILE_ATTRIBUTE_DIRECTORY;
	create_parm.in.create_disposition = NTCREATEX_DISP_CREATE;
	create_parm.in.fname = io->mkdir.in.path;
	if (create_parm.in.fname[0] == '\\') {
		create_parm.in.fname++;
	}

	req = smb2_create_send(tree, &create_parm);

	composite_continue_smb2(ctx, req, continue_mkdir, ctx);

	return ctx;
}


/*
  composite mkdir call - sync interface
*/
NTSTATUS smb2_composite_mkdir(struct smb2_tree *tree, union smb_mkdir *io)
{
	struct composite_context *c = smb2_composite_mkdir_send(tree, io);
	return composite_wait_free(c);
}



/*
  continue after the create in a composite rmdir
 */
static void continue_rmdir(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	struct smb2_tree *tree = req->tree;
	struct smb2_create create_parm;
	struct smb2_close close_parm;
	NTSTATUS status;

	status = smb2_create_recv(req, ctx, &create_parm);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(ctx, status);
		return;
	}

	ZERO_STRUCT(close_parm);
	close_parm.in.file.handle = create_parm.out.file.handle;
	
	req = smb2_close_send(tree, &close_parm);
	composite_continue_smb2(ctx, req, continue_close, ctx);
}

/*
  composite SMB2 rmdir call
*/
struct composite_context *smb2_composite_rmdir_send(struct smb2_tree *tree, 
						    struct smb_rmdir *io)
{
	struct composite_context *ctx;
	struct smb2_create create_parm;
	struct smb2_request *req;

	ctx = composite_create(tree, tree->session->transport->socket->event.ctx);
	if (ctx == NULL) return NULL;

	ZERO_STRUCT(create_parm);
	create_parm.in.desired_access     = SEC_STD_DELETE;
	create_parm.in.create_disposition = NTCREATEX_DISP_OPEN;
	create_parm.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	create_parm.in.create_options = 
		NTCREATEX_OPTIONS_DIRECTORY |
		NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	create_parm.in.fname = io->in.path;
	if (create_parm.in.fname[0] == '\\') {
		create_parm.in.fname++;
	}

	req = smb2_create_send(tree, &create_parm);

	composite_continue_smb2(ctx, req, continue_rmdir, ctx);
	return ctx;
}


/*
  composite rmdir call - sync interface
*/
NTSTATUS smb2_composite_rmdir(struct smb2_tree *tree, struct smb_rmdir *io)
{
	struct composite_context *c = smb2_composite_rmdir_send(tree, io);
	return composite_wait_free(c);
}


/*
  continue after the setfileinfo in a composite setpathinfo
 */
static void continue_setpathinfo_close(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	struct smb2_tree *tree = req->tree;
	struct smb2_close close_parm;
	NTSTATUS status;
	union smb_setfileinfo *io2 = talloc_get_type(ctx->private_data, 
						     union smb_setfileinfo);

	status = smb2_setinfo_recv(req);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(ctx, status);
		return;
	}

	ZERO_STRUCT(close_parm);
	close_parm.in.file.handle = io2->generic.in.file.handle;
	
	req = smb2_close_send(tree, &close_parm);
	composite_continue_smb2(ctx, req, continue_close, ctx);
}


/*
  continue after the create in a composite setpathinfo
 */
static void continue_setpathinfo(struct smb2_request *req)
{
	struct composite_context *ctx = talloc_get_type(req->async.private_data, 
							struct composite_context);
	struct smb2_tree *tree = req->tree;
	struct smb2_create create_parm;
	NTSTATUS status;
	union smb_setfileinfo *io2 = talloc_get_type(ctx->private_data, 
						     union smb_setfileinfo);

	status = smb2_create_recv(req, ctx, &create_parm);
	if (!NT_STATUS_IS_OK(status)) {
		composite_error(ctx, status);
		return;
	}

	io2->generic.in.file.handle = create_parm.out.file.handle;

	req = smb2_setinfo_file_send(tree, io2);
	composite_continue_smb2(ctx, req, continue_setpathinfo_close, ctx);
}


/*
  composite SMB2 setpathinfo call
*/
struct composite_context *smb2_composite_setpathinfo_send(struct smb2_tree *tree, 
							  union smb_setfileinfo *io)
{
	struct composite_context *ctx;
	struct smb2_create create_parm;
	struct smb2_request *req;
	union smb_setfileinfo *io2;

	ctx = composite_create(tree, tree->session->transport->socket->event.ctx);
	if (ctx == NULL) return NULL;

	ZERO_STRUCT(create_parm);
	create_parm.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	create_parm.in.create_disposition = NTCREATEX_DISP_OPEN;
	create_parm.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	create_parm.in.create_options = 0;
	create_parm.in.fname = io->generic.in.file.path;
	if (create_parm.in.fname[0] == '\\') {
		create_parm.in.fname++;
	}

	req = smb2_create_send(tree, &create_parm);

	io2 = talloc(ctx, union smb_setfileinfo);
	if (composite_nomem(io2, ctx)) {
		return ctx;
	}
	*io2 = *io;

	ctx->private_data = io2;

	composite_continue_smb2(ctx, req, continue_setpathinfo, ctx);
	return ctx;
}


/*
  composite setpathinfo call
 */
NTSTATUS smb2_composite_setpathinfo(struct smb2_tree *tree, union smb_setfileinfo *io)
{
	struct composite_context *c = smb2_composite_setpathinfo_send(tree, io);
	return composite_wait_free(c);	
}
