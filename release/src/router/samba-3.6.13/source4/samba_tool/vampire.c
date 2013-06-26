/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility

   Copyright (C) 2004 Stefan Metzmacher <metze@samba.org>
   Copyright (C) 2005 Andrew Bartlett <abartlet@samba.org>

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
#include "samba_tool/samba_tool.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/samr.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/events/events.h"

/* main function table */
static const struct net_functable net_samdump_functable[] = {
	{NULL, NULL, NULL, NULL}
};

int net_samdump(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_SamDump r;
	int rc;

	switch (argc) {
	case 0:
		break;
	case 1:
	default:
		rc = net_run_function(ctx, argc, argv, net_samdump_functable,
				      net_samdump_usage);
		return rc;
	}

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;
	}
	libnetctx->cred = ctx->credentials;

	r.out.error_string = NULL;
	r.in.machine_account = NULL;
	r.in.binding_string = NULL;

	status = libnet_SamDump(libnetctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("libnet_SamDump returned %s: %s\n",
			 nt_errstr(status),
			 r.out.error_string));
		return -1;
	}

	talloc_free(libnetctx);

	return 0;
}

int net_samdump_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("samba-tool samdump\n");
	return 0;
}

int net_samdump_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Dumps the sam of the domain we are joined to.\n");
	return 0;
}

int net_samsync_ldb(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_samsync_ldb r;

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;
	}
	libnetctx->cred = ctx->credentials;

	r.out.error_string = NULL;
	r.in.machine_account = NULL;
	r.in.binding_string = NULL;

	/* Needed to override the ACLs on ldb */
	r.in.session_info = system_session(ctx->lp_ctx);

	status = libnet_samsync_ldb(libnetctx, libnetctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("libnet_samsync_ldb returned %s: %s\n",
			 nt_errstr(status),
			 r.out.error_string));
		return -1;
	}

	talloc_free(libnetctx);

	return 0;
}

int net_samsync_ldb_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("samba-tool samsync\n");
	return 0;
}

int net_samsync_ldb_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Synchronise into the local ldb the SAM of a domain.\n");
	return 0;
}
