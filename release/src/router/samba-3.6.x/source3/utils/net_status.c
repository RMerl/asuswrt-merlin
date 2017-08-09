/*
   Samba Unix/Linux SMB client library
   net status command -- possible replacement for smbstatus
   Copyright (C) 2003 Volker Lendecke (vl@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "session.h"
#include "messages.h"

int net_status_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("  net status sessions [parseable] "
		   "Show list of open sessions\n"));
	d_printf(_("  net status shares [parseable]   "
		   "Show list of open shares\n"));
	return -1;
}

static int show_session(const char *key, struct sessionid *session,
			void *private_data)
{
	bool *parseable = (bool *)private_data;

	if (!process_exists(session->pid)) {
		return 0;
	}

	if (*parseable) {
		d_printf("%s\\%s\\%s\\%s\\%s\n",
			 procid_str_static(&session->pid),
			 uidtoname(session->uid),
			 gidtoname(session->gid),
			 session->remote_machine, session->hostname);
	} else {
		d_printf("%7s   %-12s  %-12s  %-12s (%s)\n",
			 procid_str_static(&session->pid),
			 uidtoname(session->uid),
			 gidtoname(session->gid),
			 session->remote_machine, session->hostname);
	}

	return 0;
}

static int net_status_sessions(struct net_context *c, int argc, const char **argv)
{
	bool parseable;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net status sessions [parseable]\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Display open user sessions.\n"
			   "    If parseable is specified, output is machine-"
			   "readable."));
		return 0;
	}

	if (argc == 0) {
		parseable = false;
	} else if ((argc == 1) && strequal(argv[0], "parseable")) {
		parseable = true;
	} else {
		return net_status_usage(c, argc, argv);
	}

	if (!parseable) {
		d_printf(_("PID     Username      Group         Machine"
			   "                        \n"
		           "-------------------------------------------"
			   "------------------------\n"));
	}

	sessionid_traverse_read(show_session, &parseable);
	return 0;
}

static int show_share(struct db_record *rec,
		      const struct connections_key *key,
		      const struct connections_data *crec,
		      void *state)
{
	if (crec->cnum == -1)
		return 0;

	if (!process_exists(crec->pid)) {
		return 0;
	}

	d_printf("%-10.10s   %s   %-12s  %s",
	       crec->servicename, procid_str_static(&crec->pid),
	       crec->machine,
	       time_to_asc(crec->start));

	return 0;
}

struct sessionids {
	int num_entries;
	struct sessionid *entries;
};

static int collect_pids(const char *key, struct sessionid *session,
			void *private_data)
{
	struct sessionids *ids = (struct sessionids *)private_data;

	if (!process_exists(session->pid))
		return 0;

	ids->num_entries += 1;
	ids->entries = SMB_REALLOC_ARRAY(ids->entries, struct sessionid, ids->num_entries);
	if (!ids->entries) {
		ids->num_entries = 0;
		return 0;
	}
	ids->entries[ids->num_entries-1] = *session;

	return 0;
}

static int show_share_parseable(const struct connections_key *key,
				const struct connections_data *crec,
				void *state)
{
	struct sessionids *ids = (struct sessionids *)state;
	int i;
	bool guest = true;

	if (crec->cnum == -1)
		return 0;

	if (!process_exists(crec->pid)) {
		return 0;
	}

	for (i=0; i<ids->num_entries; i++) {
		struct server_id id = ids->entries[i].pid;
		if (procid_equal(&id, &crec->pid)) {
			guest = false;
			break;
		}
	}

	d_printf("%s\\%s\\%s\\%s\\%s\\%s\\%s",
		 crec->servicename,procid_str_static(&crec->pid),
		 guest ? "" : uidtoname(ids->entries[i].uid),
		 guest ? "" : gidtoname(ids->entries[i].gid),
		 crec->machine,
		 guest ? "" : ids->entries[i].hostname,
		 time_to_asc(crec->start));

	return 0;
}

static int net_status_shares_parseable(struct net_context *c, int argc, const char **argv)
{
	struct sessionids ids;

	ids.num_entries = 0;
	ids.entries = NULL;

	sessionid_traverse_read(collect_pids, &ids);

	connections_forall_read(show_share_parseable, &ids);

	SAFE_FREE(ids.entries);

	return 0;
}

static int net_status_shares(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net status shares [parseable]\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Display open user shares.\n"
			   "    If parseable is specified, output is machine-"
			   "readable."));
		return 0;
	}

	if (argc == 0) {

		d_printf(_("\nService      pid     machine       "
			   "Connected at\n"
		           "-------------------------------------"
			   "------------------\n"));

		connections_forall(show_share, NULL);

		return 0;
	}

	if ((argc != 1) || !strequal(argv[0], "parseable")) {
		return net_status_usage(c, argc, argv);
	}

	return net_status_shares_parseable(c, argc, argv);
}

int net_status(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"sessions",
			net_status_sessions,
			NET_TRANSPORT_LOCAL,
			N_("Show list of open sessions"),
			N_("net status sessions [parseable]\n"
			   "    If parseable is specified, output is presented "
			   "in a machine-parseable fashion.")
		},
		{
			"shares",
			net_status_shares,
			NET_TRANSPORT_LOCAL,
			N_("Show list of open shares"),
			N_("net status shares [parseable]\n"
			   "    If parseable is specified, output is presented "
			   "in a machine-parseable fashion.")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (!sessionid_init_readonly()) {
		d_printf("failed to open sessionid.tdb\n");
		return -1;
	}

	return net_run_function(c, argc, argv, "net status", func);
}
