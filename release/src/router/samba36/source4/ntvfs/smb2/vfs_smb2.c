/* 
   Unix SMB/CIFS implementation.

   CIFS-to-SMB2 NTVFS filesystem backend

   Copyright (C) Andrew Tridgell 2008

   largely based on vfs_cifs.c which was 
      Copyright (C) Andrew Tridgell 2003
      Copyright (C) James J Myers 2003 <myersjj@samba.org>

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
  this implements a CIFS->CIFS NTVFS filesystem backend. 
  
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "ntvfs/ntvfs.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

struct cvfs_file {
	struct cvfs_file *prev, *next;
	uint16_t fnum;
	struct ntvfs_handle *h;
};

/* this is stored in ntvfs_private */
struct cvfs_private {
	struct smb2_tree *tree;
	struct smb2_transport *transport;
	struct ntvfs_module_context *ntvfs;
	struct async_info *pending;
	struct cvfs_file *files;

	/* a handle on the root of the share */
	/* TODO: leaving this handle open could prevent other users
	   from opening the share with exclusive access. We probably
	   need to open it on demand */
	struct smb2_handle roothandle;
};


/* a structure used to pass information to an async handler */
struct async_info {
	struct async_info *next, *prev;
	struct cvfs_private *cvfs;
	struct ntvfs_request *req;
	void *c_req;
	struct composite_context *c_comp;
	struct cvfs_file *f;
	void *parms;
};

#define SETUP_FILE_HERE(f) do { \
	f = ntvfs_handle_get_backend_data(io->generic.in.file.ntvfs, ntvfs); \
	if (!f) return NT_STATUS_INVALID_HANDLE; \
	io->generic.in.file.fnum = f->fnum; \
} while (0)

#define SETUP_FILE do { \
	struct cvfs_file *f; \
	SETUP_FILE_HERE(f); \
} while (0)

#define SMB2_SERVER		"smb2:server"
#define SMB2_USER		"smb2:user"
#define SMB2_PASSWORD		"smb2:password"
#define SMB2_DOMAIN		"smb2:domain"
#define SMB2_SHARE		"smb2:share"
#define SMB2_USE_MACHINE_ACCT	"smb2:use-machine-account"

#define SMB2_USE_MACHINE_ACCT_DEFAULT	false

/*
  a handler for oplock break events from the server - these need to be passed
  along to the client
 */
static bool oplock_handler(struct smbcli_transport *transport, uint16_t tid, uint16_t fnum, uint8_t level, void *p_private)
{
	struct cvfs_private *p = p_private;
	NTSTATUS status;
	struct ntvfs_handle *h = NULL;
	struct cvfs_file *f;

	for (f=p->files; f; f=f->next) {
		if (f->fnum != fnum) continue;
		h = f->h;
		break;
	}

	if (!h) {
		DEBUG(5,("vfs_smb2: ignoring oplock break level %d for fnum %d\n", level, fnum));
		return true;
	}

	DEBUG(5,("vfs_smb2: sending oplock break level %d for fnum %d\n", level, fnum));
	status = ntvfs_send_oplock_break(p->ntvfs, h, level);
	if (!NT_STATUS_IS_OK(status)) return false;
	return true;
}

/*
  return a handle to the root of the share
*/
static NTSTATUS smb2_get_roothandle(struct smb2_tree *tree, struct smb2_handle *handle)
{
	struct smb2_create io;
	NTSTATUS status;

	ZERO_STRUCT(io);
	io.in.oplock_level = 0;
	io.in.desired_access = SEC_STD_SYNCHRONIZE | SEC_DIR_READ_ATTRIBUTE | SEC_DIR_LIST;
	io.in.file_attributes   = 0;
	io.in.create_disposition = NTCREATEX_DISP_OPEN;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.in.create_options = 0;
	io.in.fname = NULL;

	status = smb2_create(tree, tree, &io);
	NT_STATUS_NOT_OK_RETURN(status);

	*handle = io.out.file.handle;

	return NT_STATUS_OK;
}

/*
  connect to a share - used when a tree_connect operation comes in.
*/
static NTSTATUS cvfs_connect(struct ntvfs_module_context *ntvfs, 
			     struct ntvfs_request *req,
			     union smb_tcon* tcon)
{
	NTSTATUS status;
	struct cvfs_private *p;
	const char *host, *user, *pass, *domain, *remote_share, *sharename;
	struct composite_context *creq;
	struct share_config *scfg = ntvfs->ctx->config;
	struct smb2_tree *tree;
	struct cli_credentials *credentials;
	bool machine_account;
	struct smbcli_options options;

	switch (tcon->generic.level) {
	case RAW_TCON_TCON:
		sharename = tcon->tcon.in.service;
		break;
	case RAW_TCON_TCONX:
		sharename = tcon->tconx.in.path;
		break;
	case RAW_TCON_SMB2:
		sharename = tcon->smb2.in.path;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	if (strncmp(sharename, "\\\\", 2) == 0) {
		char *str = strchr(sharename+2, '\\');
		if (str) {
			sharename = str + 1;
		}
	}

	/* Here we need to determine which server to connect to.
	 * For now we use parametric options, type cifs.
	 * Later we will use security=server and auth_server.c.
	 */
	host = share_string_option(scfg, SMB2_SERVER, NULL);
	user = share_string_option(scfg, SMB2_USER, NULL);
	pass = share_string_option(scfg, SMB2_PASSWORD, NULL);
	domain = share_string_option(scfg, SMB2_DOMAIN, NULL);
	remote_share = share_string_option(scfg, SMB2_SHARE, NULL);
	if (!remote_share) {
		remote_share = sharename;
	}

	machine_account = share_bool_option(scfg, SMB2_USE_MACHINE_ACCT, SMB2_USE_MACHINE_ACCT_DEFAULT);

	p = talloc_zero(ntvfs, struct cvfs_private);
	if (!p) {
		return NT_STATUS_NO_MEMORY;
	}

	ntvfs->private_data = p;

	if (!host) {
		DEBUG(1,("CIFS backend: You must supply server\n"));
		return NT_STATUS_INVALID_PARAMETER;
	} 
	
	if (user && pass) {
		DEBUG(5, ("CIFS backend: Using specified password\n"));
		credentials = cli_credentials_init(p);
		if (!credentials) {
			return NT_STATUS_NO_MEMORY;
		}
		cli_credentials_set_conf(credentials, ntvfs->ctx->lp_ctx);
		cli_credentials_set_username(credentials, user, CRED_SPECIFIED);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		cli_credentials_set_password(credentials, pass, CRED_SPECIFIED);
	} else if (machine_account) {
		DEBUG(5, ("CIFS backend: Using machine account\n"));
		credentials = cli_credentials_init(p);
		cli_credentials_set_conf(credentials, ntvfs->ctx->lp_ctx);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		status = cli_credentials_set_machine_account(credentials, ntvfs->ctx->lp_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else if (req->session_info->credentials) {
		DEBUG(5, ("CIFS backend: Using delegated credentials\n"));
		credentials = req->session_info->credentials;
	} else {
		DEBUG(1,("CIFS backend: NO delegated credentials found: You must supply server, user and password or the client must supply delegated credentials\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	lpcfg_smbcli_options(ntvfs->ctx->lp_ctx, &options);

	creq = smb2_connect_send(p, host,
			lpcfg_parm_string_list(p, ntvfs->ctx->lp_ctx, NULL, "smb2", "ports", NULL),
				remote_share, 
				 lpcfg_resolve_context(ntvfs->ctx->lp_ctx),
				 credentials,
				 ntvfs->ctx->event_ctx, &options,
				 lpcfg_socket_options(ntvfs->ctx->lp_ctx),
				 lpcfg_gensec_settings(p, ntvfs->ctx->lp_ctx)
				 );

	status = smb2_connect_recv(creq, p, &tree);
	NT_STATUS_NOT_OK_RETURN(status);

	status = smb2_get_roothandle(tree, &p->roothandle);
	NT_STATUS_NOT_OK_RETURN(status);

	p->tree = tree;
	p->transport = p->tree->session->transport;
	p->ntvfs = ntvfs;

	ntvfs->ctx->fs_type = talloc_strdup(ntvfs->ctx, "NTFS");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->fs_type);
	ntvfs->ctx->dev_type = talloc_strdup(ntvfs->ctx, "A:");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->dev_type);

	if (tcon->generic.level == RAW_TCON_TCONX) {
		tcon->tconx.out.fs_type = ntvfs->ctx->fs_type;
		tcon->tconx.out.dev_type = ntvfs->ctx->dev_type;
	}

	/* we need to receive oplock break requests from the server */
	/* TODO: enable oplocks 
	smbcli_oplock_handler(p->transport, oplock_handler, p);
	*/
	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS cvfs_disconnect(struct ntvfs_module_context *ntvfs)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct async_info *a, *an;

	/* first cleanup pending requests */
	for (a=p->pending; a; a = an) {
		an = a->next;
		talloc_free(a->c_req);
		talloc_free(a);
	}

	talloc_free(p);
	ntvfs->private_data = NULL;

	return NT_STATUS_OK;
}

/*
  destroy an async info structure
*/
static int async_info_destructor(struct async_info *async)
{
	DLIST_REMOVE(async->cvfs->pending, async);
	return 0;
}

/*
  a handler for simple async SMB2 replies
  this handler can only be used for functions that don't return any
  parameters (those that just return a status code)
 */
static void async_simple_smb2(struct smb2_request *c_req)
{
	struct async_info *async = c_req->async.private_data;
	struct ntvfs_request *req = async->req;

	smb2_request_receive(c_req);
	req->async_states->status = smb2_request_destroy(c_req);
	talloc_free(async);
	req->async_states->send_fn(req);
}

/*
  a handler for simple async composite replies
  this handler can only be used for functions that don't return any
  parameters (those that just return a status code)
 */
static void async_simple_composite(struct composite_context *c_req)
{
	struct async_info *async = c_req->async.private_data;
	struct ntvfs_request *req = async->req;

	req->async_states->status = composite_wait_free(c_req);
	talloc_free(async);
	req->async_states->send_fn(req);
}


/* save some typing for the simple functions */
#define ASYNC_RECV_TAIL_F(io, async_fn, file) do { \
	if (!c_req) return NT_STATUS_UNSUCCESSFUL; \
	{ \
		struct async_info *async; \
		async = talloc(req, struct async_info); \
		if (!async) return NT_STATUS_NO_MEMORY; \
		async->parms = io; \
		async->req = req; \
		async->f = file; \
		async->cvfs = p; \
		async->c_req = c_req; \
		DLIST_ADD(p->pending, async); \
		c_req->async.private_data = async; \
		talloc_set_destructor(async, async_info_destructor); \
	} \
	c_req->async.fn = async_fn; \
	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC; \
	return NT_STATUS_OK; \
} while (0)

#define ASYNC_RECV_TAIL(io, async_fn) ASYNC_RECV_TAIL_F(io, async_fn, NULL)

#define SIMPLE_ASYNC_TAIL ASYNC_RECV_TAIL(NULL, async_simple_smb2)
#define SIMPLE_COMPOSITE_TAIL ASYNC_RECV_TAIL(NULL, async_simple_composite)

#define CHECK_ASYNC(req) do { \
	if (!(req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) { \
		DEBUG(0,("SMB2 proxy backend does not support sync operation at %s\n", \
			 __location__)); \
		return NT_STATUS_NOT_IMPLEMENTED; \
	}} while (0)

/*
  delete a file - the dirtype specifies the file types to include in the search. 
  The name can contain CIFS wildcards, but rarely does (except with OS/2 clients)

  BUGS:
     - doesn't handle wildcards
     - doesn't obey attrib restrictions
*/
static NTSTATUS cvfs_unlink(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req, union smb_unlink *unl)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct composite_context *c_req;

	CHECK_ASYNC(req);

	c_req = smb2_composite_unlink_send(p->tree, unl);

	SIMPLE_COMPOSITE_TAIL;
}

/*
  ioctl interface
*/
static NTSTATUS cvfs_ioctl(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req, union smb_ioctl *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  check if a directory exists
*/
static NTSTATUS cvfs_chkpath(struct ntvfs_module_context *ntvfs, 
			     struct ntvfs_request *req, union smb_chkpath *cp)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct smb2_request *c_req;
	struct smb2_find f;

	CHECK_ASYNC(req);
	
	/* SMB2 doesn't have a chkpath operation, and also doesn't
	 have a query path info call, so the best seems to be to do a
	 find call, using the roothandle we established at connect
	 time */
	ZERO_STRUCT(f);
	f.in.file.handle	= p->roothandle;
	f.in.level              = SMB2_FIND_DIRECTORY_INFO;
	f.in.pattern		= cp->chkpath.in.path;
	/* SMB2 find doesn't accept \ or the empty string - this is the best
	   approximation */
	if (strcmp(f.in.pattern, "\\") == 0 || 
	    strcmp(f.in.pattern, "") == 0) {
		f.in.pattern		= "?";
	}
	f.in.continue_flags	= SMB2_CONTINUE_FLAG_SINGLE | SMB2_CONTINUE_FLAG_RESTART;
	f.in.max_response_size	= 0x1000;
	
	c_req = smb2_find_send(p->tree, &f);

	SIMPLE_ASYNC_TAIL;
}

/*
  return info on a pathname
*/
static NTSTATUS cvfs_qpathinfo(struct ntvfs_module_context *ntvfs, 
			       struct ntvfs_request *req, union smb_fileinfo *info)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  query info on a open file
*/
static NTSTATUS cvfs_qfileinfo(struct ntvfs_module_context *ntvfs, 
			       struct ntvfs_request *req, union smb_fileinfo *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  set info on a pathname
*/
static NTSTATUS cvfs_setpathinfo(struct ntvfs_module_context *ntvfs, 
				 struct ntvfs_request *req, union smb_setfileinfo *st)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  open a file
*/
static NTSTATUS cvfs_open(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req, union smb_open *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  create a directory
*/
static NTSTATUS cvfs_mkdir(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req, union smb_mkdir *md)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct composite_context *c_req;

	CHECK_ASYNC(req);

	c_req = smb2_composite_mkdir_send(p->tree, md);

	SIMPLE_COMPOSITE_TAIL;
}

/*
  remove a directory
*/
static NTSTATUS cvfs_rmdir(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req, struct smb_rmdir *rd)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct composite_context *c_req;

	CHECK_ASYNC(req);

	c_req = smb2_composite_rmdir_send(p->tree, rd);

	SIMPLE_COMPOSITE_TAIL;
}

/*
  rename a set of files
*/
static NTSTATUS cvfs_rename(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req, union smb_rename *ren)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  copy a set of files
*/
static NTSTATUS cvfs_copy(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req, struct smb_copy *cp)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  read from a file
*/
static NTSTATUS cvfs_read(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req, union smb_read *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  write to a file
*/
static NTSTATUS cvfs_write(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req, union smb_write *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  seek in a file
*/
static NTSTATUS cvfs_seek(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req,
			  union smb_seek *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  flush a file
*/
static NTSTATUS cvfs_flush(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req,
			   union smb_flush *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  close a file
*/
static NTSTATUS cvfs_close(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req, union smb_close *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  exit - closing files open by the pid
*/
static NTSTATUS cvfs_exit(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  logoff - closing files open by the user
*/
static NTSTATUS cvfs_logoff(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req)
{
	/* we can't do this right in the cifs backend .... */
	return NT_STATUS_OK;
}

/*
  setup for an async call - nothing to do yet
*/
static NTSTATUS cvfs_async_setup(struct ntvfs_module_context *ntvfs, 
				 struct ntvfs_request *req, 
				 void *private_data)
{
	return NT_STATUS_OK;
}

/*
  cancel an async call
*/
static NTSTATUS cvfs_cancel(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  lock a byte range
*/
static NTSTATUS cvfs_lock(struct ntvfs_module_context *ntvfs, 
			  struct ntvfs_request *req, union smb_lock *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  set info on a open file
*/
static NTSTATUS cvfs_setfileinfo(struct ntvfs_module_context *ntvfs, 
				 struct ntvfs_request *req, 
				 union smb_setfileinfo *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  a handler for async fsinfo replies
 */
static void async_fsinfo(struct smb2_request *c_req)
{
	struct async_info *async = c_req->async.private_data;
	struct ntvfs_request *req = async->req;
	req->async_states->status = smb2_getinfo_fs_recv(c_req, req, async->parms);
	talloc_free(async);
	req->async_states->send_fn(req);
}

/*
  return filesystem space info
*/
static NTSTATUS cvfs_fsinfo(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req, union smb_fsinfo *fs)
{
	struct cvfs_private *p = ntvfs->private_data;
	struct smb2_request *c_req;
	enum smb_fsinfo_level level = fs->generic.level;

	CHECK_ASYNC(req);

	switch (level) {
		/* some levels go straight through */
	case RAW_QFS_VOLUME_INFORMATION:
	case RAW_QFS_SIZE_INFORMATION:
	case RAW_QFS_DEVICE_INFORMATION:
	case RAW_QFS_ATTRIBUTE_INFORMATION:
	case RAW_QFS_QUOTA_INFORMATION:
	case RAW_QFS_FULL_SIZE_INFORMATION:
	case RAW_QFS_OBJECTID_INFORMATION:
		break;

		/* some get mapped */
	case RAW_QFS_VOLUME_INFO:
		level = RAW_QFS_VOLUME_INFORMATION;
		break;
	case RAW_QFS_SIZE_INFO:
		level = RAW_QFS_SIZE_INFORMATION;
		break;
	case RAW_QFS_DEVICE_INFO:
		level = RAW_QFS_DEVICE_INFORMATION;
		break;
	case RAW_QFS_ATTRIBUTE_INFO:
		level = RAW_QFS_ATTRIBUTE_INFO;
		break;

	default:
		/* the rest get refused for now */
		DEBUG(0,("fsinfo level %u not possible on SMB2\n",
			 (unsigned)fs->generic.level));
		break;
	}

	fs->generic.level = level;
	fs->generic.handle = p->roothandle;

	c_req = smb2_getinfo_fs_send(p->tree, fs);

	ASYNC_RECV_TAIL(fs, async_fsinfo);
}

/*
  return print queue info
*/
static NTSTATUS cvfs_lpq(struct ntvfs_module_context *ntvfs, 
			 struct ntvfs_request *req, union smb_lpq *lpq)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS cvfs_search_first(struct ntvfs_module_context *ntvfs, 
				  struct ntvfs_request *req, union smb_search_first *io, 
				  void *search_private, 
				  bool (*callback)(void *, const union smb_search_data *))
{
	struct cvfs_private *p = ntvfs->private_data;
	struct smb2_find f;
	enum smb_search_data_level smb2_level;
	unsigned int count, i;
	union smb_search_data *data;
	NTSTATUS status;

	if (io->generic.level != RAW_SEARCH_TRANS2) {
		DEBUG(0,("We only support trans2 search in smb2 backend\n"));
		return NT_STATUS_NOT_SUPPORTED;
	}

	switch (io->generic.data_level) {
	case RAW_SEARCH_DATA_DIRECTORY_INFO:
		smb2_level = SMB2_FIND_DIRECTORY_INFO;
		break;
	case RAW_SEARCH_DATA_FULL_DIRECTORY_INFO:
		smb2_level = SMB2_FIND_FULL_DIRECTORY_INFO;
		break;
	case RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO:
		smb2_level = SMB2_FIND_BOTH_DIRECTORY_INFO;
		break;
	case RAW_SEARCH_DATA_NAME_INFO:
		smb2_level = SMB2_FIND_NAME_INFO;
		break;
	case RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO:
		smb2_level = SMB2_FIND_ID_FULL_DIRECTORY_INFO;
		break;
	case RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO:
		smb2_level = SMB2_FIND_ID_BOTH_DIRECTORY_INFO;
		break;
	default:
		DEBUG(0,("Unsupported search level %u for smb2 backend\n",
			 (unsigned)io->generic.data_level));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	/* we do the search on the roothandle. This only works because
	   search is synchronous, otherwise we'd have no way to
	   distinguish multiple searches happening at once
	*/
	ZERO_STRUCT(f);
	f.in.file.handle	= p->roothandle;
	f.in.level              = smb2_level;
	f.in.pattern		= io->t2ffirst.in.pattern;
	while (f.in.pattern[0] == '\\') {
		f.in.pattern++;
	}
	f.in.continue_flags	= 0;
	f.in.max_response_size	= 0x10000;

	status = smb2_find_level(p->tree, req, &f, &count, &data);
	NT_STATUS_NOT_OK_RETURN(status);	

	for (i=0;i<count;i++) {
		if (!callback(search_private, &data[i])) break;
	}

	io->t2ffirst.out.handle = 0;
	io->t2ffirst.out.count = i;
	/* TODO: fix end_of_file */
	io->t2ffirst.out.end_of_search = 1;

	talloc_free(data);
	
	return NT_STATUS_OK;
}

/* continue a search */
static NTSTATUS cvfs_search_next(struct ntvfs_module_context *ntvfs, 
				 struct ntvfs_request *req, union smb_search_next *io, 
				 void *search_private, 
				 bool (*callback)(void *, const union smb_search_data *))
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/* close a search */
static NTSTATUS cvfs_search_close(struct ntvfs_module_context *ntvfs, 
				  struct ntvfs_request *req, union smb_search_close *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/* SMBtrans - not used on file shares */
static NTSTATUS cvfs_trans(struct ntvfs_module_context *ntvfs, 
			   struct ntvfs_request *req,
			   struct smb_trans2 *trans2)
{
	return NT_STATUS_ACCESS_DENIED;
}

/* change notify request - always async */
static NTSTATUS cvfs_notify(struct ntvfs_module_context *ntvfs, 
			    struct ntvfs_request *req,
			    union smb_notify *io)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  initialise the CIFS->CIFS backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_smb2_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	/* fill in the name and type */
	ops.name = "smb2";
	ops.type = NTVFS_DISK;
	
	/* fill in all the operations */
	ops.connect = cvfs_connect;
	ops.disconnect = cvfs_disconnect;
	ops.unlink = cvfs_unlink;
	ops.chkpath = cvfs_chkpath;
	ops.qpathinfo = cvfs_qpathinfo;
	ops.setpathinfo = cvfs_setpathinfo;
	ops.open = cvfs_open;
	ops.mkdir = cvfs_mkdir;
	ops.rmdir = cvfs_rmdir;
	ops.rename = cvfs_rename;
	ops.copy = cvfs_copy;
	ops.ioctl = cvfs_ioctl;
	ops.read = cvfs_read;
	ops.write = cvfs_write;
	ops.seek = cvfs_seek;
	ops.flush = cvfs_flush;	
	ops.close = cvfs_close;
	ops.exit = cvfs_exit;
	ops.lock = cvfs_lock;
	ops.setfileinfo = cvfs_setfileinfo;
	ops.qfileinfo = cvfs_qfileinfo;
	ops.fsinfo = cvfs_fsinfo;
	ops.lpq = cvfs_lpq;
	ops.search_first = cvfs_search_first;
	ops.search_next = cvfs_search_next;
	ops.search_close = cvfs_search_close;
	ops.trans = cvfs_trans;
	ops.logoff = cvfs_logoff;
	ops.async_setup = cvfs_async_setup;
	ops.cancel = cvfs_cancel;
	ops.notify = cvfs_notify;

	/* register ourselves with the NTVFS subsystem. We register
	   under the name 'smb2'. */
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register SMB2 backend\n"));
	}
	
	return ret;
}
