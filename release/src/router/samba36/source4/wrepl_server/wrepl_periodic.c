/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "librpc/gen_ndr/winsrepl.h"
#include "wrepl_server/wrepl_server.h"

static NTSTATUS wreplsrv_periodic_run(struct wreplsrv_service *service)
{
	NTSTATUS status;

	status = wreplsrv_load_partners(service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = wreplsrv_scavenging_run(service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = wreplsrv_out_pull_run(service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = wreplsrv_out_push_run(service);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

static void wreplsrv_periodic_handler_te(struct tevent_context *ev, struct tevent_timer *te,
					 struct timeval t, void *ptr)
{
	struct wreplsrv_service *service = talloc_get_type(ptr, struct wreplsrv_service);
	NTSTATUS status;

	service->periodic.te = NULL;

	status = wreplsrv_periodic_schedule(service, service->config.periodic_interval);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(service->task, nt_errstr(status), false);
		return;
	}

	status = wreplsrv_periodic_run(service);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("wresrv_periodic_run() failed: %s\n", nt_errstr(status)));
	}
}

NTSTATUS wreplsrv_periodic_schedule(struct wreplsrv_service *service, uint32_t next_interval)
{
	TALLOC_CTX *tmp_mem;
	struct tevent_timer *new_te;
	struct timeval next_time;

	/* prevent looping */
	if (next_interval == 0) next_interval = 1;

	next_time = timeval_current_ofs(next_interval, 5000);

	if (service->periodic.te) {
		/*
		 * if the timestamp of the new event is higher,
		 * as current next we don't need to reschedule
		 */
		if (timeval_compare(&next_time, &service->periodic.next_event) > 0) {
			return NT_STATUS_OK;
		}
	}

	/* reset the next scheduled timestamp */
	service->periodic.next_event = next_time;

	new_te = event_add_timed(service->task->event_ctx, service,
			         service->periodic.next_event,
			         wreplsrv_periodic_handler_te, service);
	NT_STATUS_HAVE_NO_MEMORY(new_te);

	tmp_mem = talloc_new(service);
	DEBUG(6,("wreplsrv_periodic_schedule(%u) %sscheduled for: %s\n",
		next_interval,
		(service->periodic.te?"re":""),
		nt_time_string(tmp_mem, timeval_to_nttime(&next_time))));
	talloc_free(tmp_mem);

	talloc_free(service->periodic.te);
	service->periodic.te = new_te;

	return NT_STATUS_OK;
}

NTSTATUS wreplsrv_setup_periodic(struct wreplsrv_service *service)
{
	NTSTATUS status;

	status = wreplsrv_periodic_schedule(service, 0);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}
