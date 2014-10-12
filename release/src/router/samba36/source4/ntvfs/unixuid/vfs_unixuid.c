/* 
   Unix SMB/CIFS implementation.

   a pass-thru NTVFS module to setup a security context using unix
   uid/gid

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
#include "system/filesys.h"
#include "system/passwd.h"
#include "auth/auth.h"
#include "ntvfs/ntvfs.h"
#include "libcli/wbclient/wbclient.h"
#define TEVENT_DEPRECATED
#include <tevent.h>

#if defined(UID_WRAPPER)
#if !defined(UID_WRAPPER_REPLACE) && !defined(UID_WRAPPER_NOT_REPLACE)
#define UID_WRAPPER_REPLACE
#include "../uid_wrapper/uid_wrapper.h"
#endif
#else
#define uwrap_enabled() 0
#endif


struct unixuid_private {
	struct wbc_context *wbc_ctx;
	struct unix_sec_ctx *last_sec_ctx;
	struct security_token *last_token;
};



struct unix_sec_ctx {
	uid_t uid;
	gid_t gid;
	unsigned int ngroups;
	gid_t *groups;
};

/*
  pull the current security context into a unix_sec_ctx
*/
static struct unix_sec_ctx *save_unix_security(TALLOC_CTX *mem_ctx)
{
	struct unix_sec_ctx *sec = talloc(mem_ctx, struct unix_sec_ctx);
	if (sec == NULL) {
		return NULL;
	}
	sec->uid = geteuid();
	sec->gid = getegid();
	sec->ngroups = getgroups(0, NULL);
	if (sec->ngroups == -1) {
		talloc_free(sec);
		return NULL;
	}
	sec->groups = talloc_array(sec, gid_t, sec->ngroups);
	if (sec->groups == NULL) {
		talloc_free(sec);
		return NULL;
	}

	if (getgroups(sec->ngroups, sec->groups) != sec->ngroups) {
		talloc_free(sec);
		return NULL;
	}

	return sec;
}

/*
  set the current security context from a unix_sec_ctx
*/
static NTSTATUS set_unix_security(struct unix_sec_ctx *sec)
{
	seteuid(0);

	if (setgroups(sec->ngroups, sec->groups) != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}
	if (setegid(sec->gid) != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}
	if (seteuid(sec->uid) != 0) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static int unixuid_nesting_level;

/*
  called at the start and end of a tevent nesting loop. Needs to save/restore
  unix security context
 */
static int unixuid_event_nesting_hook(struct tevent_context *ev,
				      void *private_data,
				      uint32_t level,
				      bool begin,
				      void *stack_ptr,
				      const char *location)
{
	struct unix_sec_ctx *sec_ctx;

	if (unixuid_nesting_level == 0) {
		/* we don't need to do anything unless we are nested
		   inside of a call in this module */
		return 0;
	}

	if (begin) {
		sec_ctx = save_unix_security(ev);
		if (sec_ctx == NULL) {
			DEBUG(0,("%s: Failed to save security context\n", location));
			return -1;
		}
		*(struct unix_sec_ctx **)stack_ptr = sec_ctx;
		if (seteuid(0) != 0 || setegid(0) != 0) {
			DEBUG(0,("%s: Failed to change to root\n", location));
			return -1;			
		}
	} else {
		/* called when we come out of a nesting level */
		NTSTATUS status;

		sec_ctx = *(struct unix_sec_ctx **)stack_ptr;
		if (sec_ctx == NULL) {
			/* this happens the first time this function
			   is called, as we install the hook while
			   inside an event in unixuid_connect() */
			return 0;
		}

		sec_ctx = talloc_get_type_abort(sec_ctx, struct unix_sec_ctx);
		status = set_unix_security(sec_ctx);
		talloc_free(sec_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("%s: Failed to revert security context (%s)\n", 
				 location, nt_errstr(status)));
			return -1;
		}
	}

	return 0;
}


/*
  form a unix_sec_ctx from the current security_token
*/
static NTSTATUS nt_token_to_unix_security(struct ntvfs_module_context *ntvfs,
					  struct ntvfs_request *req,
					  struct security_token *token,
					  struct unix_sec_ctx **sec)
{
	struct unixuid_private *priv = ntvfs->private_data;
	int i;
	NTSTATUS status;
	struct id_map *ids;
	struct composite_context *ctx;
	*sec = talloc(req, struct unix_sec_ctx);

	/* we can't do unix security without a user and group */
	if (token->num_sids < 2) {
		return NT_STATUS_ACCESS_DENIED;
	}

	ids = talloc_array(req, struct id_map, token->num_sids);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	(*sec)->ngroups = token->num_sids - 2;
	(*sec)->groups = talloc_array(*sec, gid_t, (*sec)->ngroups);
	NT_STATUS_HAVE_NO_MEMORY((*sec)->groups);

	for (i=0;i<token->num_sids;i++) {
		ZERO_STRUCT(ids[i].xid);
		ids[i].sid = &token->sids[i];
		ids[i].status = ID_UNKNOWN;
	}

	ctx = wbc_sids_to_xids_send(priv->wbc_ctx, ids, token->num_sids, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_sids_to_xids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	if (ids[0].xid.type == ID_TYPE_BOTH ||
	    ids[0].xid.type == ID_TYPE_UID) {
		(*sec)->uid = ids[0].xid.id;
	} else {
		return NT_STATUS_INVALID_SID;
	}

	if (ids[1].xid.type == ID_TYPE_BOTH ||
	    ids[1].xid.type == ID_TYPE_GID) {
		(*sec)->gid = ids[1].xid.id;
	} else {
		return NT_STATUS_INVALID_SID;
	}

	for (i=0;i<(*sec)->ngroups;i++) {
		if (ids[i+2].xid.type == ID_TYPE_BOTH ||
		    ids[i+2].xid.type == ID_TYPE_GID) {
			(*sec)->groups[i] = ids[i+2].xid.id;
		} else {
			return NT_STATUS_INVALID_SID;
		}
	}

	return NT_STATUS_OK;
}

/*
  setup our unix security context according to the session authentication info
*/
static NTSTATUS unixuid_setup_security(struct ntvfs_module_context *ntvfs,
				       struct ntvfs_request *req, struct unix_sec_ctx **sec)
{
	struct unixuid_private *priv = ntvfs->private_data;
	struct security_token *token;
	struct unix_sec_ctx *newsec;
	NTSTATUS status;

	/* If we are asked to set up, but have not had a successful
	 * session setup or tree connect, then these may not be filled
	 * in.  ACCESS_DENIED is the right error code here */
	if (req->session_info == NULL || priv == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	token = req->session_info->security_token;

	*sec = save_unix_security(ntvfs);
	if (*sec == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (token == priv->last_token) {
		newsec = priv->last_sec_ctx;
	} else {
		status = nt_token_to_unix_security(ntvfs, req, token, &newsec);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(*sec);
			return status;
		}
		if (priv->last_sec_ctx) {
			talloc_free(priv->last_sec_ctx);
		}
		priv->last_sec_ctx = newsec;
		priv->last_token = token;
		talloc_steal(priv, newsec);
	}

	status = set_unix_security(newsec);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(*sec);
		return status;
	}

	return NT_STATUS_OK;
}

/*
  this pass through macro operates on request contexts
*/
#define PASS_THRU_REQ(ntvfs, req, op, args) do { \
	NTSTATUS status2; \
	struct unix_sec_ctx *sec; \
	status = unixuid_setup_security(ntvfs, req, &sec); \
	NT_STATUS_NOT_OK_RETURN(status); \
	unixuid_nesting_level++; \
	status = ntvfs_next_##op args; \
	unixuid_nesting_level--; \
	status2 = set_unix_security(sec); \
	talloc_free(sec); \
	if (!NT_STATUS_IS_OK(status2)) smb_panic("Unable to reset security context"); \
} while (0)



/*
  connect to a share - used when a tree_connect operation comes in.
*/
static NTSTATUS unixuid_connect(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req, union smb_tcon *tcon)
{
	struct unixuid_private *priv;
	NTSTATUS status;

	priv = talloc(ntvfs, struct unixuid_private);
	if (!priv) {
		return NT_STATUS_NO_MEMORY;
	}

	priv->wbc_ctx = wbc_init(priv, ntvfs->ctx->msg_ctx,
				    ntvfs->ctx->event_ctx);
	if (priv->wbc_ctx == NULL) {
		talloc_free(priv);
		return NT_STATUS_INTERNAL_ERROR;
	}

	priv->last_sec_ctx = NULL;
	priv->last_token = NULL;
	ntvfs->private_data = priv;

	tevent_loop_set_nesting_hook(ntvfs->ctx->event_ctx, 
				     unixuid_event_nesting_hook,
				     &unixuid_nesting_level);

	/* we don't use PASS_THRU_REQ here, as the connect operation runs with 
	   root privileges. This allows the backends to setup any database
	   links they might need during the connect. */
	status = ntvfs_next_connect(ntvfs, req, tcon);

	return status;
}

/*
  disconnect from a share
*/
static NTSTATUS unixuid_disconnect(struct ntvfs_module_context *ntvfs)
{
	struct unixuid_private *priv = ntvfs->private_data;
	NTSTATUS status;

	talloc_free(priv);
	ntvfs->private_data = NULL;

	status = ntvfs_next_disconnect(ntvfs);
 
	return status;
}


/*
  delete a file
*/
static NTSTATUS unixuid_unlink(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      union smb_unlink *unl)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, unlink, (ntvfs, req, unl));

	return status;
}

/*
  ioctl interface
*/
static NTSTATUS unixuid_ioctl(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_ioctl *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, ioctl, (ntvfs, req, io));

	return status;
}

/*
  check if a directory exists
*/
static NTSTATUS unixuid_chkpath(struct ntvfs_module_context *ntvfs,
			        struct ntvfs_request *req,
				union smb_chkpath *cp)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, chkpath, (ntvfs, req, cp));

	return status;
}

/*
  return info on a pathname
*/
static NTSTATUS unixuid_qpathinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_fileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, qpathinfo, (ntvfs, req, info));

	return status;
}

/*
  query info on a open file
*/
static NTSTATUS unixuid_qfileinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_fileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, qfileinfo, (ntvfs, req, info));

	return status;
}


/*
  set info on a pathname
*/
static NTSTATUS unixuid_setpathinfo(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, union smb_setfileinfo *st)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, setpathinfo, (ntvfs, req, st));

	return status;
}

/*
  open a file
*/
static NTSTATUS unixuid_open(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_open *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, open, (ntvfs, req, io));

	return status;
}

/*
  create a directory
*/
static NTSTATUS unixuid_mkdir(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_mkdir *md)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, mkdir, (ntvfs, req, md));

	return status;
}

/*
  remove a directory
*/
static NTSTATUS unixuid_rmdir(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, struct smb_rmdir *rd)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, rmdir, (ntvfs, req, rd));

	return status;
}

/*
  rename a set of files
*/
static NTSTATUS unixuid_rename(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_rename *ren)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, rename, (ntvfs, req, ren));

	return status;
}

/*
  copy a set of files
*/
static NTSTATUS unixuid_copy(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, struct smb_copy *cp)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, copy, (ntvfs, req, cp));

	return status;
}

/*
  read from a file
*/
static NTSTATUS unixuid_read(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_read *rd)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, read, (ntvfs, req, rd));

	return status;
}

/*
  write to a file
*/
static NTSTATUS unixuid_write(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_write *wr)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, write, (ntvfs, req, wr));

	return status;
}

/*
  seek in a file
*/
static NTSTATUS unixuid_seek(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_seek *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, seek, (ntvfs, req, io));

	return status;
}

/*
  flush a file
*/
static NTSTATUS unixuid_flush(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      union smb_flush *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, flush, (ntvfs, req, io));

	return status;
}

/*
  close a file
*/
static NTSTATUS unixuid_close(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_close *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, close, (ntvfs, req, io));

	return status;
}

/*
  exit - closing files
*/
static NTSTATUS unixuid_exit(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, exit, (ntvfs, req));

	return status;
}

/*
  logoff - closing files
*/
static NTSTATUS unixuid_logoff(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req)
{
	struct unixuid_private *priv = ntvfs->private_data;
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, logoff, (ntvfs, req));

	priv->last_token = NULL;

	return status;
}

/*
  async setup
*/
static NTSTATUS unixuid_async_setup(struct ntvfs_module_context *ntvfs,
				    struct ntvfs_request *req, 
				    void *private_data)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, async_setup, (ntvfs, req, private_data));

	return status;
}

/*
  cancel an async request
*/
static NTSTATUS unixuid_cancel(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, cancel, (ntvfs, req));

	return status;
}

/*
  change notify
*/
static NTSTATUS unixuid_notify(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_notify *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, notify, (ntvfs, req, info));

	return status;
}

/*
  lock a byte range
*/
static NTSTATUS unixuid_lock(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_lock *lck)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, lock, (ntvfs, req, lck));

	return status;
}

/*
  set info on a open file
*/
static NTSTATUS unixuid_setfileinfo(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, 
				   union smb_setfileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, setfileinfo, (ntvfs, req, info));

	return status;
}


/*
  return filesystem space info
*/
static NTSTATUS unixuid_fsinfo(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_fsinfo *fs)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, fsinfo, (ntvfs, req, fs));

	return status;
}

/*
  return print queue info
*/
static NTSTATUS unixuid_lpq(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_lpq *lpq)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, lpq, (ntvfs, req, lpq));

	return status;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS unixuid_search_first(struct ntvfs_module_context *ntvfs,
				    struct ntvfs_request *req, union smb_search_first *io, 
				    void *search_private, 
				    bool (*callback)(void *, const union smb_search_data *))
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_first, (ntvfs, req, io, search_private, callback));

	return status;
}

/* continue a search */
static NTSTATUS unixuid_search_next(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, union smb_search_next *io, 
				   void *search_private, 
				   bool (*callback)(void *, const union smb_search_data *))
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_next, (ntvfs, req, io, search_private, callback));

	return status;
}

/* close a search */
static NTSTATUS unixuid_search_close(struct ntvfs_module_context *ntvfs,
				    struct ntvfs_request *req, union smb_search_close *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_close, (ntvfs, req, io));

	return status;
}

/* SMBtrans - not used on file shares */
static NTSTATUS unixuid_trans(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, struct smb_trans2 *trans2)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, trans, (ntvfs, req, trans2));

	return status;
}

/*
  initialise the unixuid backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_unixuid_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	/* fill in all the operations */
	ops.connect = unixuid_connect;
	ops.disconnect = unixuid_disconnect;
	ops.unlink = unixuid_unlink;
	ops.chkpath = unixuid_chkpath;
	ops.qpathinfo = unixuid_qpathinfo;
	ops.setpathinfo = unixuid_setpathinfo;
	ops.open = unixuid_open;
	ops.mkdir = unixuid_mkdir;
	ops.rmdir = unixuid_rmdir;
	ops.rename = unixuid_rename;
	ops.copy = unixuid_copy;
	ops.ioctl = unixuid_ioctl;
	ops.read = unixuid_read;
	ops.write = unixuid_write;
	ops.seek = unixuid_seek;
	ops.flush = unixuid_flush;	
	ops.close = unixuid_close;
	ops.exit = unixuid_exit;
	ops.lock = unixuid_lock;
	ops.setfileinfo = unixuid_setfileinfo;
	ops.qfileinfo = unixuid_qfileinfo;
	ops.fsinfo = unixuid_fsinfo;
	ops.lpq = unixuid_lpq;
	ops.search_first = unixuid_search_first;
	ops.search_next = unixuid_search_next;
	ops.search_close = unixuid_search_close;
	ops.trans = unixuid_trans;
	ops.logoff = unixuid_logoff;
	ops.async_setup = unixuid_async_setup;
	ops.cancel = unixuid_cancel;
	ops.notify = unixuid_notify;

	ops.name = "unixuid";

	/* we register under all 3 backend types, as we are not type specific */
	ops.type = NTVFS_DISK;	
	ret = ntvfs_register(&ops, &vers);
	if (!NT_STATUS_IS_OK(ret)) goto failed;

	ops.type = NTVFS_PRINT;	
	ret = ntvfs_register(&ops, &vers);
	if (!NT_STATUS_IS_OK(ret)) goto failed;

	ops.type = NTVFS_IPC;	
	ret = ntvfs_register(&ops, &vers);
	if (!NT_STATUS_IS_OK(ret)) goto failed;
	
failed:
	return ret;
}
