/* 
   Unix SMB/CIFS implementation.
   connection claim routines
   Copyright (C) Andrew Tridgell 1998

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "dbwrap.h"
#include "auth.h"

/****************************************************************************
 Delete a connection record.
****************************************************************************/

bool yield_connection(connection_struct *conn, const char *name)
{
	struct db_record *rec;
	NTSTATUS status;

	DEBUG(3,("Yielding connection to %s\n",name));

	rec = connections_fetch_entry(talloc_tos(), conn, name);
	if (rec == NULL) {
		DEBUG(0, ("connections_fetch_entry failed\n"));
		return False;
	}

	status = rec->delete_rec(rec);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG( NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND) ? 3 : 0,
		       ("deleting connection record returned %s\n",
			nt_errstr(status)));
	}

	TALLOC_FREE(rec);
	return NT_STATUS_IS_OK(status);
}

struct count_stat {
	int curr_connections;
	const char *name;
	bool Clear;
};

/****************************************************************************
 Count the entries belonging to a service in the connection db.
****************************************************************************/

static int count_fn(struct db_record *rec,
		    const struct connections_key *ckey,
		    const struct connections_data *crec,
		    void *udp)
{
	struct count_stat *cs = (struct count_stat *)udp;

	if (crec->cnum == -1) {
		return 0;
	}

	/* If the pid was not found delete the entry from connections.tdb */

	if (cs->Clear && !process_exists(crec->pid) && (errno == ESRCH)) {
		NTSTATUS status;
		DEBUG(2,("pid %s doesn't exist - deleting connections %d [%s]\n",
			 procid_str_static(&crec->pid), crec->cnum,
			 crec->servicename));

		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("count_fn: tdb_delete failed with error %s\n",
				 nt_errstr(status)));
		}
		return 0;
	}

	if (strequal(crec->servicename, cs->name))
		cs->curr_connections++;

	return 0;
}

/****************************************************************************
 Claim an entry in the connections database.
****************************************************************************/

int count_current_connections( const char *sharename, bool clear  )
{
	struct count_stat cs;

	cs.curr_connections = 0;
	cs.name = sharename;
	cs.Clear = clear;

	/*
	 * This has a race condition, but locking the chain before hand is worse
	 * as it leads to deadlock.
	 */

	if (connections_forall(count_fn, &cs) == -1) {
		DEBUG(0,("count_current_connections: traverse of "
			 "connections.tdb failed\n"));
		return False;
	}

	return cs.curr_connections;
}

/****************************************************************************
 Claim an entry in the connections database.
****************************************************************************/

bool claim_connection(connection_struct *conn, const char *name)
{
	struct db_record *rec;
	struct connections_data crec;
	TDB_DATA dbuf;
	NTSTATUS status;

	DEBUG(5,("claiming [%s]\n", name));

	if (!(rec = connections_fetch_entry(talloc_tos(), conn, name))) {
		DEBUG(0, ("connections_fetch_entry failed\n"));
		return False;
	}

	/* fill in the crec */
	ZERO_STRUCT(crec);
	crec.magic = 0x280267;
	crec.pid = sconn_server_id(conn->sconn);
	crec.cnum = conn->cnum;
	crec.uid = conn->session_info->utok.uid;
	crec.gid = conn->session_info->utok.gid;
	strlcpy(crec.servicename, lp_servicename(SNUM(conn)),
		sizeof(crec.servicename));
	crec.start = time(NULL);

	strlcpy(crec.machine,get_remote_machine_name(),sizeof(crec.machine));
	strlcpy(crec.addr, conn->sconn->client_id.addr, sizeof(crec.addr));

	dbuf.dptr = (uint8 *)&crec;
	dbuf.dsize = sizeof(crec);

	status = rec->store(rec, dbuf, TDB_REPLACE);

	TALLOC_FREE(rec);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("claim_connection: tdb_store failed with error %s.\n",
			 nt_errstr(status)));
		return False;
	}

	return True;
}
