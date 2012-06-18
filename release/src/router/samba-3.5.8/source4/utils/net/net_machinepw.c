/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility

   Copyright (C) 2008 Volker Lendecke

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
#include "utils/net/net.h"
#include "libnet/libnet.h"
#include "libcli/security/security.h"
#include "param/secrets.h"
#include "param/param.h"
#include "lib/util/util_ldb.h"

int net_machinepw_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net machinepw <accountname>\n");
	return -1;
}

int net_machinepw(struct net_context *ctx, int argc, const char **argv)
{
	struct ldb_context *secrets;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev;
	struct ldb_message **msgs;
	int num_records;
	const char *attrs[] = { "secret", NULL };
	const char *secret;

	if (argc != 1) {
		net_machinepw_usage(ctx, argc, argv);
		return -1;
	}

	mem_ctx = talloc_new(ctx);
	if (mem_ctx == NULL) {
		d_fprintf(stderr, "talloc_new failed\n");
		return -1;
	}

	ev = event_context_init(mem_ctx);
	if (ev == NULL) {
		d_fprintf(stderr, "event_context_init failed\n");
		goto fail;
	}

	secrets = secrets_db_connect(mem_ctx, ev, ctx->lp_ctx);
	if (secrets == NULL) {
		d_fprintf(stderr, "secrets_db_connect failed\n");
		goto fail;
	}

	num_records = gendb_search(secrets, mem_ctx, NULL, &msgs, attrs,
				   "(&(objectclass=primaryDomain)"
				   "(samaccountname=%s))", argv[0]);
	if (num_records != 1) {
		d_fprintf(stderr, "gendb_search returned %d records, "
			  "expected 1\n", num_records);
		goto fail;
	}

	secret = ldb_msg_find_attr_as_string(msgs[0], "secret", NULL);
	if (secret == NULL) {
		d_fprintf(stderr, "machine account contains no secret\n");
		goto fail;
	}

	printf("%s\n", secret);
	talloc_free(mem_ctx);
	return 0;

 fail:
	talloc_free(mem_ctx);
	return -1;
}
