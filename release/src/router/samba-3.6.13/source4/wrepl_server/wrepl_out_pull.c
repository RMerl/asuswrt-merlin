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
#include "librpc/gen_ndr/winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "libcli/composite/composite.h"

static void wreplsrv_out_pull_reschedule(struct wreplsrv_partner *partner, uint32_t interval)
{
	NTSTATUS status;

	partner->pull.next_run = timeval_current_ofs(interval, 0);
	status = wreplsrv_periodic_schedule(partner->service, interval);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("wreplsrv_periodic_schedule() failed\n"));
	}
}

static void wreplsrv_pull_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_partner *partner = talloc_get_type(creq->async.private_data, struct wreplsrv_partner);
	struct wreplsrv_pull_cycle_io *old_cycle_io;
	struct wrepl_table inform_in;

	partner->pull.last_status = wreplsrv_pull_cycle_recv(partner->pull.creq);
	partner->pull.creq = NULL;

	old_cycle_io = partner->pull.cycle_io;
	partner->pull.cycle_io = NULL;

	if (NT_STATUS_IS_OK(partner->pull.last_status)) {
		partner->pull.error_count = 0;
		DEBUG(1,("wreplsrv_pull_cycle(%s): %s\n",
			 partner->address, nt_errstr(partner->pull.last_status)));
		goto done;
	}

	partner->pull.error_count++;

	if (partner->pull.error_count > 1) {
		uint32_t retry_interval;

		retry_interval = partner->pull.error_count * partner->pull.retry_interval;
		retry_interval = MIN(retry_interval, partner->pull.interval);

		DEBUG(0,("wreplsrv_pull_cycle(%s): %s: error_count: %u: reschedule(%u)\n",
			 partner->address, nt_errstr(partner->pull.last_status),
			 partner->pull.error_count, retry_interval));

		wreplsrv_out_pull_reschedule(partner, retry_interval);
		goto done;
	}

	DEBUG(0,("wreplsrv_pull_cycle(%s): %s: error_count:%u retry\n",
		 partner->address, nt_errstr(partner->pull.last_status),
		 partner->pull.error_count));
	inform_in.partner_count = old_cycle_io->in.num_owners;
	inform_in.partners	= old_cycle_io->in.owners;
	wreplsrv_out_partner_pull(partner, &inform_in);
done:
	talloc_free(old_cycle_io);
}

void wreplsrv_out_partner_pull(struct wreplsrv_partner *partner, struct wrepl_table *inform_in)
{
	/* there's already a pull in progress, so we're done */
	if (partner->pull.creq) return;

	partner->pull.cycle_io = talloc(partner, struct wreplsrv_pull_cycle_io);
	if (!partner->pull.cycle_io) {
		goto nomem;
	}

	partner->pull.cycle_io->in.partner	= partner;
	partner->pull.cycle_io->in.wreplconn	= NULL;
	if (inform_in) {
		partner->pull.cycle_io->in.num_owners	= inform_in->partner_count;
		partner->pull.cycle_io->in.owners	= inform_in->partners;
		talloc_steal(partner->pull.cycle_io, inform_in->partners);
	} else {
		partner->pull.cycle_io->in.num_owners	= 0;
		partner->pull.cycle_io->in.owners	= NULL;
	}
	partner->pull.creq = wreplsrv_pull_cycle_send(partner->pull.cycle_io, partner->pull.cycle_io);
	if (!partner->pull.creq) {
		DEBUG(1,("wreplsrv_pull_cycle_send(%s) failed\n",
			 partner->address));
		goto nomem;
	}

	partner->pull.creq->async.fn		= wreplsrv_pull_handler_creq;
	partner->pull.creq->async.private_data	= partner;

	return;
nomem:
	talloc_free(partner->pull.cycle_io);
	partner->pull.cycle_io = NULL;
	DEBUG(0,("wreplsrv_out_partner_pull(%s): no memory? (ignoring)\n",
		partner->address));
	return;
}

NTSTATUS wreplsrv_out_pull_run(struct wreplsrv_service *service)
{
	struct wreplsrv_partner *partner;

	for (partner = service->partners; partner; partner = partner->next) {
		/* if it's not a pull partner, go to the next partner */
		if (!(partner->type & WINSREPL_PARTNER_PULL)) continue;

		/* if pulling is disabled for the partner, go to the next one */
		if (partner->pull.interval == 0) continue;

		/* if the next timer isn't reached, go to the next partner */
		if (!timeval_expired(&partner->pull.next_run)) continue;

		wreplsrv_out_pull_reschedule(partner, partner->pull.interval);

		wreplsrv_out_partner_pull(partner, NULL);
	}

	return NT_STATUS_OK;
}
