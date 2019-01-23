/* 
   Unix SMB/CIFS implementation.
   NTVFS base code

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004

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
  this implements the core code for all NTVFS modules. Backends register themselves here.
*/

#include "includes.h"
#include "../lib/util/dlinklist.h"
#include "ntvfs/ntvfs.h"
#include "param/param.h"

/* the list of currently registered NTVFS backends, note that there
 * can be more than one backend with the same name, as long as they
 * have different typesx */
static struct ntvfs_backend {
	const struct ntvfs_ops *ops;
} *backends = NULL;
static int num_backends;

/*
  register a NTVFS backend. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.  

  The 'type' is used to specify whether this is for a disk, printer or IPC$ share
*/
NTSTATUS ntvfs_register(const struct ntvfs_ops *ops,
				 const struct ntvfs_critical_sizes *const sizes)
{
	struct ntvfs_ops *new_ops;

	if (ntvfs_interface_differs(sizes)) {
		DEBUG(0, ("NTVFS backend '%s' for type %d "
			  "failed version check\n",
			  ops->name, (int)ops->type));
		return NT_STATUS_BAD_FUNCTION_TABLE;
	}

	if (ntvfs_backend_byname(ops->name, ops->type) != NULL) {
		/* its already registered! */
		DEBUG(0,("NTVFS backend '%s' for type %d already registered\n", 
			 ops->name, (int)ops->type));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	backends = realloc_p(backends, struct ntvfs_backend, num_backends+1);
	if (!backends) {
		smb_panic("out of memory in ntvfs_register");
	}

	new_ops = smb_xmemdup(ops, sizeof(*ops));
	new_ops->name = smb_xstrdup(ops->name);

	backends[num_backends].ops = new_ops;

	num_backends++;

	DEBUG(3,("NTVFS backend '%s' for type %d registered\n", 
		 ops->name,ops->type));

	return NT_STATUS_OK;
}


/*
  return the operations structure for a named backend of the specified type
*/
const struct ntvfs_ops *ntvfs_backend_byname(const char *name, enum ntvfs_type type)
{
	int i;

	for (i=0;i<num_backends;i++) {
		if (backends[i].ops->type == type && 
		    strcmp(backends[i].ops->name, name) == 0) {
			return backends[i].ops;
		}
	}

	return NULL;
}


/*
  return the NTVFS interface version, and the size of some critical types
  This can be used by backends to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/

static const NTVFS_CURRENT_CRITICAL_SIZES(critical_sizes);

const struct ntvfs_critical_sizes *ntvfs_interface_version(void)
{
	return &critical_sizes;
}

bool ntvfs_interface_differs(const struct ntvfs_critical_sizes *const iface)
{
	/* The comparison would be easier with memcmp, but compiler-interset
	 * alignment padding is not guaranteed to be zeroed.
	 */

#define FIELD_DIFFERS(field) (iface->field != critical_sizes.field)

	if (FIELD_DIFFERS(interface_version))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_critical_sizes))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_context))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_module_context))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_ops))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_async_state))
		return true;

	if (FIELD_DIFFERS(sizeof_ntvfs_request))
		return true;

	/* Versions match. */
	return false;

#undef FIELD_DIFFERS
}

/*
  initialise a connection structure to point at a NTVFS backend
*/
NTSTATUS ntvfs_init_connection(TALLOC_CTX *mem_ctx, struct share_config *scfg, enum ntvfs_type type,
			       enum protocol_types protocol,
			       uint64_t ntvfs_client_caps,
			       struct tevent_context *ev, struct messaging_context *msg,
			       struct loadparm_context *lp_ctx,
			       struct server_id server_id, struct ntvfs_context **_ctx)
{
	const char **handlers = share_string_list_option(mem_ctx, scfg, SHARE_NTVFS_HANDLER);
	int i;
	struct ntvfs_context *ctx;

	if (!handlers) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ctx = talloc_zero(mem_ctx, struct ntvfs_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);
	ctx->protocol		= protocol;
	ctx->client_caps	= ntvfs_client_caps;
	ctx->type		= type;
	ctx->config		= talloc_steal(ctx, scfg);
	ctx->event_ctx		= ev;
	ctx->msg_ctx		= msg;
	ctx->server_id		= server_id;
	ctx->lp_ctx		= lp_ctx;

	for (i=0; handlers[i]; i++) {
		struct ntvfs_module_context *ntvfs;

		ntvfs = talloc_zero(ctx, struct ntvfs_module_context);
		NT_STATUS_HAVE_NO_MEMORY(ntvfs);
		ntvfs->ctx = ctx;
		ntvfs->ops = ntvfs_backend_byname(handlers[i], ctx->type);
		if (!ntvfs->ops) {
			DEBUG(1,("ntvfs_init_connection: failed to find backend=%s, type=%d\n",
				handlers[i], ctx->type));
			return NT_STATUS_INTERNAL_ERROR;
		}
		ntvfs->depth = i;
		DLIST_ADD_END(ctx->modules, ntvfs, struct ntvfs_module_context *);
	}

	if (!ctx->modules) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	*_ctx = ctx;
	return NT_STATUS_OK;
}

/*
  adds the IPC$ share, needed for RPC calls
 */
static NTSTATUS ntvfs_add_ipc_share(struct loadparm_context *lp_ctx)
{
	struct loadparm_service *ipc;

	if (lpcfg_service(lp_ctx, "IPC$")) {
		/* it has already been defined in smb.conf or elsewhere */
		return NT_STATUS_OK;
	}

	ipc = lpcfg_add_service(lp_ctx, NULL, "IPC$");
	NT_STATUS_HAVE_NO_MEMORY(ipc);

	lpcfg_do_service_parameter(lp_ctx, ipc, "comment", "IPC Service");
	lpcfg_do_service_parameter(lp_ctx, ipc, "path", "/dev/null");
	lpcfg_do_service_parameter(lp_ctx, ipc, "ntvfs handler", "default");
	lpcfg_do_service_parameter(lp_ctx, ipc, "browseable", "No");
	lpcfg_do_service_parameter(lp_ctx, ipc, "fstype", "IPC");

	return NT_STATUS_OK;
}

NTSTATUS ntvfs_init(struct loadparm_context *lp_ctx)
{
	static bool initialized = false;
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_ntvfs_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_ntvfs_MODULES };
	init_module_fn *shared_init;

	if (initialized) return NT_STATUS_OK;
	initialized = true;
	
	shared_init = load_samba_modules(NULL, lp_ctx, "ntvfs");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	ntvfs_add_ipc_share(lp_ctx);
	
	return NT_STATUS_OK;
}
