/* 
   Unix SMB/CIFS implementation.

   helper functions for task based servers (nbtd, winbind etc)

   Copyright (C) Andrew Tridgell 2005
   
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
#include "process_model.h"
#include "lib/messaging/irpc.h"
#include "param/param.h"
#include "librpc/gen_ndr/ndr_irpc.h"

/*
  terminate a task service
*/
void task_server_terminate(struct task_server *task, const char *reason, bool fatal)
{
	struct tevent_context *event_ctx = task->event_ctx;
	const struct model_ops *model_ops = task->model_ops;
	DEBUG(0,("task_server_terminate: [%s]\n", reason));

	if (fatal) {
		struct samba_terminate r;
		struct server_id *sid;

		sid = irpc_servers_byname(task->msg_ctx, task, "samba");

		r.in.reason = reason;
		IRPC_CALL(task->msg_ctx, sid[0],
			  irpc, SAMBA_TERMINATE,
			  &r, NULL);
	}

	model_ops->terminate(event_ctx, task->lp_ctx, reason);
	
	/* don't free this above, it might contain the 'reason' being printed */
	talloc_free(task);
}

/* used for the callback from the process model code */
struct task_state {
	void (*task_init)(struct task_server *);
	const struct model_ops *model_ops;
};


/*
  called by the process model code when the new task starts up. This then calls
  the server specific startup code
*/
static void task_server_callback(struct tevent_context *event_ctx, 
				 struct loadparm_context *lp_ctx,
				 struct server_id server_id, void *private_data)
{
	struct task_state *state = talloc_get_type(private_data, struct task_state);
	struct task_server *task;

	task = talloc(event_ctx, struct task_server);
	if (task == NULL) return;

	task->event_ctx = event_ctx;
	task->model_ops = state->model_ops;
	task->server_id = server_id;
	task->lp_ctx = lp_ctx;

	task->msg_ctx = messaging_init(task, 
				       lp_messaging_path(task, task->lp_ctx),
				       task->server_id, 
				       lp_iconv_convenience(task->lp_ctx),
				       task->event_ctx);
	if (!task->msg_ctx) {
		task_server_terminate(task, "messaging_init() failed", true);
		return;
	}

	state->task_init(task);
}

/*
  startup a task based server
*/
NTSTATUS task_server_startup(struct tevent_context *event_ctx, 
			     struct loadparm_context *lp_ctx,
			     const char *service_name, 
			     const struct model_ops *model_ops, 
			     void (*task_init)(struct task_server *))
{
	struct task_state *state;

	state = talloc(event_ctx, struct task_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	state->task_init = task_init;
	state->model_ops = model_ops;
	
	model_ops->new_task(event_ctx, lp_ctx, service_name, task_server_callback, state);

	return NT_STATUS_OK;
}

/*
  setup a task title 
*/
void task_server_set_title(struct task_server *task, const char *title)
{
	task->model_ops->set_title(task->event_ctx, title);
}
