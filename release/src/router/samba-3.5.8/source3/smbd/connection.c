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

/****************************************************************************
 Delete a connection record.
****************************************************************************/

bool yield_connection(connection_struct *conn, const char *name)
{
	struct db_record *rec;
	NTSTATUS status;

	DEBUG(3,("Yielding connection to %s\n",name));

	if (!(rec = connections_fetch_entry(NULL, conn, name))) {
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
	pid_t mypid;
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

	cs.mypid = sys_getpid();
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
 Count the number of connections open across all shares.
****************************************************************************/

int count_all_current_connections(void)
{
        return count_current_connections(NULL, True /* clear stale entries */);
}

/****************************************************************************
 Claim an entry in the connections database.
****************************************************************************/

bool claim_connection(connection_struct *conn, const char *name,
		      uint32 msg_flags)
{
	struct db_record *rec;
	struct connections_data crec;
	TDB_DATA dbuf;
	NTSTATUS status;
	char addr[INET6_ADDRSTRLEN];

	DEBUG(5,("claiming [%s]\n", name));

	if (!(rec = connections_fetch_entry(talloc_tos(), conn, name))) {
		DEBUG(0, ("connections_fetch_entry failed\n"));
		return False;
	}

	/* fill in the crec */
	ZERO_STRUCT(crec);
	crec.magic = 0x280267;
	crec.pid = procid_self();
	crec.cnum = conn?conn->cnum:-1;
	if (conn) {
		crec.uid = conn->server_info->utok.uid;
		crec.gid = conn->server_info->utok.gid;
		strlcpy(crec.servicename, lp_servicename(SNUM(conn)),
			sizeof(crec.servicename));
	}
	crec.start = time(NULL);
	crec.bcast_msg_flags = msg_flags;

	strlcpy(crec.machine,get_remote_machine_name(),sizeof(crec.machine));
	strlcpy(crec.addr,conn?conn->client_address:
			client_addr(get_client_fd(),addr,sizeof(addr)),
		sizeof(crec.addr));

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

bool register_message_flags(bool doreg, uint32 msg_flags)
{
	struct db_record *rec;
	struct connections_data *pcrec;
	NTSTATUS status;

	DEBUG(10,("register_message_flags: %s flags 0x%x\n",
		doreg ? "adding" : "removing",
		(unsigned int)msg_flags ));

	if (!(rec = connections_fetch_entry(NULL, NULL, ""))) {
		DEBUG(0, ("connections_fetch_entry failed\n"));
		return False;
	}

	if (rec->value.dsize != sizeof(struct connections_data)) {
		DEBUG(0,("register_message_flags: Got wrong record size\n"));
		TALLOC_FREE(rec);
		return False;
	}

	pcrec = (struct connections_data *)rec->value.dptr;
	if (doreg)
		pcrec->bcast_msg_flags |= msg_flags;
	else
		pcrec->bcast_msg_flags &= ~msg_flags;

	status = rec->store(rec, rec->value, TDB_REPLACE);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("register_message_flags: tdb_store failed: %s.\n",
			 nt_errstr(status)));
		TALLOC_FREE(rec);
		return False;
	}

	DEBUG(10,("register_message_flags: new flags 0x%x\n",
		(unsigned int)pcrec->bcast_msg_flags ));

	TALLOC_FREE(rec);

	return True;
}
