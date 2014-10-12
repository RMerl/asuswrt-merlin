/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service periodic handling
   
   Copyright (C) Stefan Metzmacher 2007
    
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
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "dsdb/repl/drepl_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

static void dreplsrv_periodic_run(struct dreplsrv_service *service);

static void dreplsrv_periodic_handler_te(struct tevent_context *ev, struct tevent_timer *te,
					 struct timeval t, void *ptr)
{
	struct dreplsrv_service *service = talloc_get_type(ptr, struct dreplsrv_service);
	WERROR status;

	service->periodic.te = NULL;

	dreplsrv_periodic_run(service);

	status = dreplsrv_periodic_schedule(service, service->periodic.interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(service->task, win_errstr(status), false);
		return;
	}
}

WERROR dreplsrv_periodic_schedule(struct dreplsrv_service *service, uint32_t next_interval)
{
	TALLOC_CTX *tmp_mem;
	struct tevent_timer *new_te;
	struct timeval next_time;

	/* prevent looping */
	if (next_interval == 0) next_interval = 1;

	next_time = timeval_current_ofs(next_interval, 50);

	if (service->periodic.te) {
		/*
		 * if the timestamp of the new event is higher,
		 * as current next we don't need to reschedule
		 */
		if (timeval_compare(&next_time, &service->periodic.next_event) > 0) {
			return WERR_OK;
		}
	}

	/* reset the next scheduled timestamp */
	service->periodic.next_event = next_time;

	new_te = event_add_timed(service->task->event_ctx, service,
			         service->periodic.next_event,
			         dreplsrv_periodic_handler_te, service);
	W_ERROR_HAVE_NO_MEMORY(new_te);

	tmp_mem = talloc_new(service);
	DEBUG(4,("dreplsrv_periodic_schedule(%u) %sscheduled for: %s\n",
		next_interval,
		(service->periodic.te?"re":""),
		nt_time_string(tmp_mem, timeval_to_nttime(&next_time))));
	talloc_free(tmp_mem);

	talloc_free(service->periodic.te);
	service->periodic.te = new_te;

	return WERR_OK;
}

static void dreplsrv_periodic_run(struct dreplsrv_service *service)
{
	TALLOC_CTX *mem_ctx;

	DEBUG(4,("dreplsrv_periodic_run(): schedule pull replication\n"));

	/*
	 * KCC or some administrative tool
	 * might have changed Topology graph
	 * i.e. repsFrom/repsTo
	 */
	dreplsrv_refresh_partitions(service);

	mem_ctx = talloc_new(service);
	dreplsrv_schedule_pull_replication(service, mem_ctx);
	talloc_free(mem_ctx);

	DEBUG(4,("dreplsrv_periodic_run(): run pending_ops memory=%u\n", 
		 (unsigned)talloc_total_blocks(service)));

	dreplsrv_ridalloc_check_rid_pool(service);

	dreplsrv_run_pending_ops(service);
}

/*
  run the next pending op, either a notify or a pull
 */
void dreplsrv_run_pending_ops(struct dreplsrv_service *s)
{
	if (!s->ops.notifies && !s->ops.pending) {
		return;
	}
	if (!s->ops.notifies ||
	    (s->ops.pending &&
	     s->ops.notifies->schedule_time > s->ops.pending->schedule_time)) {
		dreplsrv_run_pull_ops(s);
	} else {
		dreplsrv_notify_run_ops(s);
	}
}
