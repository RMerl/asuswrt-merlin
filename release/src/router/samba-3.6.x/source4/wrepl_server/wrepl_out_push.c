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
#include "nbt_server/wins/winsdb.h"

static void wreplsrv_out_partner_push(struct wreplsrv_partner *partner, bool propagate);

static void wreplsrv_push_handler_creq(struct composite_context *creq)
{
	struct wreplsrv_partner *partner = talloc_get_type(creq->async.private_data, struct wreplsrv_partner);
	struct wreplsrv_push_notify_io *old_notify_io;

	partner->push.last_status = wreplsrv_push_notify_recv(partner->push.creq);
	partner->push.creq = NULL;

	old_notify_io = partner->push.notify_io;
	partner->push.notify_io = NULL;

	if (NT_STATUS_IS_OK(partner->push.last_status)) {
		partner->push.error_count = 0;
		DEBUG(2,("wreplsrv_push_notify(%s): %s\n",
			 partner->address, nt_errstr(partner->push.last_status)));
		goto done;
	}

	partner->push.error_count++;

	if (partner->push.error_count > 1) {
		DEBUG(1,("wreplsrv_push_notify(%s): %s: error_count: %u: giving up\n",
			 partner->address, nt_errstr(partner->push.last_status),
			 partner->push.error_count));
		goto done;
	}

	DEBUG(1,("wreplsrv_push_notify(%s): %s: error_count: %u: retry\n",
		 partner->address, nt_errstr(partner->push.last_status),
		 partner->push.error_count));
	wreplsrv_out_partner_push(partner, old_notify_io->in.propagate);
done:
	talloc_free(old_notify_io);
}

static void wreplsrv_out_partner_push(struct wreplsrv_partner *partner, bool propagate)
{
	/* a push for this partner is currently in progress, so we're done */
	if (partner->push.creq) return;

	/* now prepare the push notify */
	partner->push.notify_io = talloc(partner, struct wreplsrv_push_notify_io);
	if (!partner->push.notify_io) {
		goto nomem;
	}

	partner->push.notify_io->in.partner	= partner;
	partner->push.notify_io->in.inform	= partner->push.use_inform;
	partner->push.notify_io->in.propagate	= propagate;
	partner->push.creq = wreplsrv_push_notify_send(partner->push.notify_io, partner->push.notify_io);
	if (!partner->push.creq) {
		DEBUG(1,("wreplsrv_push_notify_send(%s) failed nomem?\n",
			 partner->address));
		goto nomem;
	}

	partner->push.creq->async.fn		= wreplsrv_push_handler_creq;
	partner->push.creq->async.private_data	= partner;

	return;
nomem:
	talloc_free(partner->push.notify_io);
	partner->push.notify_io = NULL;
	DEBUG(1,("wreplsrv_out_partner_push(%s,%u) failed nomem? (ignoring)\n",
		 partner->address, propagate));
	return;
}

static uint32_t wreplsrv_calc_change_count(struct wreplsrv_partner *partner, uint64_t maxVersionID)
{
	uint64_t tmp_diff = UINT32_MAX;

	/* catch an overflow */
	if (partner->push.maxVersionID > maxVersionID) {
		goto done;
	}

	tmp_diff = maxVersionID - partner->push.maxVersionID;

	if (tmp_diff > UINT32_MAX) {
		tmp_diff = UINT32_MAX;
		goto done;
	}

done:
	partner->push.maxVersionID = maxVersionID;
	return (uint32_t)(tmp_diff & UINT32_MAX);
}

NTSTATUS wreplsrv_out_push_run(struct wreplsrv_service *service)
{
	struct wreplsrv_partner *partner;
	uint64_t seqnumber;
	uint32_t change_count;

	seqnumber = winsdb_get_maxVersion(service->wins_db);

	for (partner = service->partners; partner; partner = partner->next) {
		/* if it's not a push partner, go to the next partner */
		if (!(partner->type & WINSREPL_PARTNER_PUSH)) continue;

		/* if push notifies are disabled for this partner, go to the next partner */
		if (partner->push.change_count == 0) continue;

		/* get the actual change count for the partner */
		change_count = wreplsrv_calc_change_count(partner, seqnumber);

		/* if the configured change count isn't reached, go to the next partner */
		if (change_count < partner->push.change_count) continue;

		wreplsrv_out_partner_push(partner, false);
	}

	return NT_STATUS_OK;
}
