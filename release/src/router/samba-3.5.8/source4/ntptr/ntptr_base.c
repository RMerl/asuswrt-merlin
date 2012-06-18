/* 
   Unix SMB/CIFS implementation.

   NTPTR base code

   Copyright (C) Stefan (metze) Metzmacher 2005

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
  this implements the core code for all NTPTR modules. Backends register themselves here.
*/

#include "includes.h"
#include "ntptr/ntptr.h"
#include "param/param.h"

/* the list of currently registered NTPTR backends */
static struct ntptr_backend {
	const struct ntptr_ops *ops;
} *backends = NULL;
static int num_backends;

/*
  register a NTPTR backend. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.
*/
NTSTATUS ntptr_register(const void *_ops)
{
	const struct ntptr_ops *ops = _ops;
	struct ntptr_ops *new_ops;

	if (ntptr_backend_byname(ops->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("NTPTR backend '%s' already registered\n", 
			 ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	backends = realloc_p(backends, struct ntptr_backend, num_backends+1);
	if (!backends) {
		smb_panic("out of memory in ntptr_register");
	}

	new_ops = smb_xmemdup(ops, sizeof(*ops));
	new_ops->name = smb_xstrdup(ops->name);

	backends[num_backends].ops = new_ops;

	num_backends++;

	DEBUG(3,("NTPTR backend '%s'\n", 
		 ops->name));

	return NT_STATUS_OK;
}

NTSTATUS ntptr_init(struct loadparm_context *lp_ctx)
{
	extern NTSTATUS ntptr_simple_ldb_init(void);
	init_module_fn static_init[] = { STATIC_ntptr_MODULES };
	init_module_fn *shared_init = load_samba_modules(NULL, lp_ctx, "ntptr");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);
	
	return NT_STATUS_OK;	
}


/*
  return the operations structure for a named backend
*/
const struct ntptr_ops *ntptr_backend_byname(const char *name)
{
	int i;

	for (i=0;i<num_backends;i++) {
		if (strcmp(backends[i].ops->name, name) == 0) {
			return backends[i].ops;
		}
	}

	return NULL;
}


/*
  return the NTPTR interface version, and the size of some critical types
  This can be used by backends to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
static const struct ntptr_critical_sizes critical_sizes = {
	.interface_version		= NTPTR_INTERFACE_VERSION,
	.sizeof_ntptr_critical_sizes	= sizeof(struct ntptr_critical_sizes),
	.sizeof_ntptr_context		= sizeof(struct ntptr_context),
	.sizeof_ntptr_ops		= sizeof(struct ntptr_ops),
};
const struct ntptr_critical_sizes *ntptr_interface_version(void)
{
	return &critical_sizes;
}

/*
  create a ntptr_context with a specified NTPTR backend
*/
NTSTATUS ntptr_init_context(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx,
			    struct loadparm_context *lp_ctx,
			    const char *providor, struct ntptr_context **_ntptr)
{
	NTSTATUS status;
	struct ntptr_context *ntptr;

	if (!providor) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	ntptr = talloc(mem_ctx, struct ntptr_context);
	NT_STATUS_HAVE_NO_MEMORY(ntptr);
	ntptr->private_data	= NULL;
	ntptr->ops		= ntptr_backend_byname(providor);
	ntptr->ev_ctx		= ev_ctx;
	ntptr->lp_ctx		= lp_ctx;

	if (!ntptr->ops) {
		DEBUG(1,("ntptr_init_context: failed to find NTPTR providor='%s'\n",
			 providor));
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = ntptr->ops->init_context(ntptr);
	NT_STATUS_NOT_OK_RETURN(status);

	*_ntptr = ntptr;
	return NT_STATUS_OK;
}
