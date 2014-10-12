/* 
   Unix SMB/CIFS implementation.
   process model manager - main loop
   Copyright (C) Andrew Tridgell 1992-2003
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

#include "includes.h"
#include "smbd/process_model.h"
#include "param/param.h"

/* the list of currently registered process models */
static struct process_model {
	const struct model_ops *ops;
	bool initialised;
} *models = NULL;
static int num_models;


/*
  return the operations structure for a named backend of the specified type
*/
static struct process_model *process_model_byname(const char *name)
{
	int i;

	for (i=0;i<num_models;i++) {
		if (strcmp(models[i].ops->name, name) == 0) {
			return &models[i];
		}
	}

	return NULL;
}


/*
  setup the events for the chosen process model
*/
_PUBLIC_ const struct model_ops *process_model_startup(const char *model)
{
	struct process_model *m;

	m = process_model_byname(model);
	if (m == NULL) {
		DEBUG(0,("Unknown process model '%s'\n", model));
		exit(-1);
	}

	if (!m->initialised) {
		m->initialised = true;
		m->ops->model_init();
	}

	return m->ops;
}

/*
  register a process model. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.  
*/
_PUBLIC_ NTSTATUS register_process_model(const struct model_ops *ops)
{
	if (process_model_byname(ops->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("PROCESS_MODEL '%s' already registered\n", 
			 ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	models = talloc_realloc(NULL, models, struct process_model, num_models+1);
	if (!models) {
		smb_panic("out of memory in register_process_model");
	}

	models[num_models].ops = ops;
	models[num_models].initialised = false;

	num_models++;

	DEBUG(3,("PROCESS_MODEL '%s' registered\n", ops->name));

	return NT_STATUS_OK;
}

_PUBLIC_ NTSTATUS process_model_init(struct loadparm_context *lp_ctx)
{
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_process_model_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_process_model_MODULES };
	init_module_fn *shared_init;
	static bool initialised;

	if (initialised) {
		return NT_STATUS_OK;
	}
	initialised = true;

	shared_init = load_samba_modules(NULL, lp_ctx, "process_model");
	
	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	return NT_STATUS_OK;
}

/*
  return the PROCESS_MODEL module version, and the size of some critical types
  This can be used by process model modules to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
const struct process_model_critical_sizes *process_model_version(void)
{
	static const struct process_model_critical_sizes critical_sizes = {
		PROCESS_MODEL_VERSION,
		sizeof(struct model_ops)
	};

	return &critical_sizes;
}
