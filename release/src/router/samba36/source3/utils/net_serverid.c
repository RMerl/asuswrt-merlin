/*
   Samba Unix/Linux SMB client library
   net serverid commands
   Copyright (C) Volker Lendecke 2010

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
#include "utils/net.h"
#include "dbwrap.h"
#include "serverid.h"
#include "session.h"

static int net_serverid_list_fn(const struct server_id *id,
				uint32_t msg_flags, void *priv)
{
	char *str = procid_str(talloc_tos(), id);
	d_printf("%s %llu 0x%x\n", str, (unsigned long long)id->unique_id,
		 (unsigned int)msg_flags);
	TALLOC_FREE(str);
	return 0;
}

static int net_serverid_list(struct net_context *c, int argc,
			     const char **argv)
{
	if (!serverid_init_readonly(c)) {
		d_printf("failed to open serverid.tdb\n");
		return -1;
	}
	d_printf("pid unique_id msg_flags\n");
	return serverid_traverse_read(net_serverid_list_fn, NULL) ? 0 : -1;
}

static int net_serverid_wipe_fn(struct db_record *rec,
				const struct server_id *id,
				uint32_t msg_flags, void *private_data)
{
	NTSTATUS status;

	if (id->vnn != get_my_vnn()) {
		return 0;
	}
	status = rec->delete_rec(rec);
	if (!NT_STATUS_IS_OK(status)) {
		char *str = procid_str(talloc_tos(), id);
		DEBUG(1, ("Could not delete serverid.tdb record %s: %s\n",
			  str, nt_errstr(status)));
		TALLOC_FREE(str);
	}
	return 0;
}

static int net_serverid_wipe(struct net_context *c, int argc,
			     const char **argv)
{
	return serverid_traverse(net_serverid_wipe_fn, NULL) ? 0 : -1;
}

static int net_serverid_wipedbs_conn(
	struct db_record *rec,
	const struct connections_key *key,
	const struct connections_data *data,
	void *private_data)
{
	if (!serverid_exists(&key->pid)) {
		NTSTATUS status;

		DEBUG(10, ("Deleting connections.tdb record for pid %s\n",
			   procid_str(talloc_tos(), &key->pid)));

		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Could not delete connections.tdb record "
				  "for pid %s: %s\n",
				  procid_str(talloc_tos(), &key->pid),
				  nt_errstr(status)));
		}
	}
	return 0;
}

static int net_serverid_wipedbs_sessionid(struct db_record *rec,
					  const char *key,
					  struct sessionid *session,
					  void *private_data)
{
	if (!serverid_exists(&session->pid)) {
		NTSTATUS status;

		DEBUG(10, ("Deleting sessionid.tdb record for pid %s\n",
			   procid_str(talloc_tos(), &session->pid)));

		status = rec->delete_rec(rec);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Could not delete session.tdb record "
				  "for pid %s: %s\n",
				  procid_str(talloc_tos(), &session->pid),
				  nt_errstr(status)));
		}
	}
	return 0;
}

static int net_serverid_wipedbs(struct net_context *c, int argc,
				const char **argv)
{
	if (!sessionid_init()) {
		d_printf("failed to open sessionid.tdb\n");
		return -1;
	};

	connections_forall(net_serverid_wipedbs_conn, NULL);
	sessionid_traverse(net_serverid_wipedbs_sessionid, NULL);
	return 0;
}

int net_serverid(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"list",
			net_serverid_list,
			NET_TRANSPORT_LOCAL,
			N_("List all entries from serverid.tdb"),
			N_("net serverid list\n"
			   "    List all entries from serverid.tdb")
		},
		{
			"wipe",
			net_serverid_wipe,
			NET_TRANSPORT_LOCAL,
			N_("Wipe the serverid.tdb for the current node"),
			N_("net serverid wipe\n"
			   "    Wipe the serverid.tdb for the current node")
		},
		{
			"wipedbs",
			net_serverid_wipedbs,
			NET_TRANSPORT_LOCAL,
			N_("Clean dead entries from connections.tdb and "
			   "sessionid.tdb"),
			N_("net serverid wipedbs\n"
			   "    Clean dead entries from connections.tdb and "
			   "sessionid.tdb")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net serverid", func);
}
