/*
 * Samba Unix/Linux SMB client library
 * Interface to the g_lock facility
 * Copyright (C) Volker Lendecke 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "net.h"
#include "g_lock.h"
#include "messages.h"

static bool net_g_lock_init(TALLOC_CTX *mem_ctx,
			    struct tevent_context **pev,
			    struct messaging_context **pmsg,
			    struct g_lock_ctx **pg_ctx)
{
	struct tevent_context *ev = NULL;
	struct messaging_context *msg = NULL;
	struct g_lock_ctx *g_ctx = NULL;

	ev = tevent_context_init(mem_ctx);
	if (ev == NULL) {
		d_fprintf(stderr, "ERROR: could not init event context\n");
		goto fail;
	}
	msg = messaging_init(mem_ctx, procid_self(), ev);
	if (msg == NULL) {
		d_fprintf(stderr, "ERROR: could not init messaging context\n");
		goto fail;
	}
	g_ctx = g_lock_ctx_init(mem_ctx, msg);
	if (g_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init g_lock context\n");
		goto fail;
	}

	*pev = ev;
	*pmsg = msg;
	*pg_ctx = g_ctx;
	return true;
fail:
	TALLOC_FREE(g_ctx);
	TALLOC_FREE(msg);
	TALLOC_FREE(ev);
	return false;
}

struct net_g_lock_do_state {
	const char *cmd;
	int result;
};

static void net_g_lock_do_fn(void *private_data)
{
	struct net_g_lock_do_state *state =
		(struct net_g_lock_do_state *)private_data;
	state->result = system(state->cmd);
}

static int net_g_lock_do(struct net_context *c, int argc, const char **argv)
{
	struct net_g_lock_do_state state;
	const char *name, *cmd;
	int timeout;
	NTSTATUS status;

	if (argc != 3) {
		d_printf("Usage: net g_lock do <lockname> <timeout> "
			 "<command>\n");
		return -1;
	}
	name = argv[0];
	timeout = atoi(argv[1]);
	cmd = argv[2];

	state.cmd = cmd;
	state.result = -1;

	status = g_lock_do(name, G_LOCK_WRITE,
			   timeval_set(timeout / 1000, timeout % 1000),
			   procid_self(), net_g_lock_do_fn, &state);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "ERROR: g_lock_do failed: %s\n",
			  nt_errstr(status));
		goto done;
	}
	if (state.result == -1) {
		d_fprintf(stderr, "ERROR: system() returned %s\n",
			  strerror(errno));
		goto done;
	}
	d_fprintf(stderr, "command returned %d\n", state.result);

done:
	return state.result;
}

static int net_g_lock_dump_fn(struct server_id pid, enum g_lock_type lock_type,
			      void *private_data)
{
	char *pidstr;

	pidstr = procid_str(talloc_tos(), &pid);
	d_printf("%s: %s (%s)\n", pidstr,
		 (lock_type & 1) ? "WRITE" : "READ",
		 (lock_type & G_LOCK_PENDING) ? "pending" : "holder");
	TALLOC_FREE(pidstr);
	return 0;
}

static int net_g_lock_dump(struct net_context *c, int argc, const char **argv)
{
	struct tevent_context *ev = NULL;
	struct messaging_context *msg = NULL;
	struct g_lock_ctx *g_ctx = NULL;
	NTSTATUS status;
	int ret = -1;

	if (argc != 1) {
		d_printf("Usage: net g_lock dump <lockname>\n");
		return -1;
	}

	if (!net_g_lock_init(talloc_tos(), &ev, &msg, &g_ctx)) {
		goto done;
	}

	status = g_lock_dump(g_ctx, argv[0], net_g_lock_dump_fn, NULL);

	ret = 0;
done:
	TALLOC_FREE(g_ctx);
	TALLOC_FREE(msg);
	TALLOC_FREE(ev);
	return ret;
}

static int net_g_lock_locks_fn(const char *name, void *private_data)
{
	d_printf("%s\n", name);
	return 0;
}

static int net_g_lock_locks(struct net_context *c, int argc, const char **argv)
{
	struct tevent_context *ev = NULL;
	struct messaging_context *msg = NULL;
	struct g_lock_ctx *g_ctx = NULL;
	int ret = -1;

	if (argc != 0) {
		d_printf("Usage: net g_lock locks\n");
		return -1;
	}

	if (!net_g_lock_init(talloc_tos(), &ev, &msg, &g_ctx)) {
		goto done;
	}

	ret = g_lock_locks(g_ctx, net_g_lock_locks_fn, NULL);
done:
	TALLOC_FREE(g_ctx);
	TALLOC_FREE(msg);
	TALLOC_FREE(ev);
	return ret;
}

int net_g_lock(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"do",
			net_g_lock_do,
			NET_TRANSPORT_LOCAL,
			N_("Execute a shell command under a lock"),
			N_("net g_lock do <lock name> <timeout> <command>\n")
		},
		{
			"locks",
			net_g_lock_locks,
			NET_TRANSPORT_LOCAL,
			N_("List all locknames"),
			N_("net g_lock locks\n")
		},
		{
			"dump",
			net_g_lock_dump,
			NET_TRANSPORT_LOCAL,
			N_("Dump a g_lock locking table"),
			N_("net g_lock dump <lock name>\n")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net g_lock", func);
}
