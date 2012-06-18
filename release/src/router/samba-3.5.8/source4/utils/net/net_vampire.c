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
#include "utils/net/net.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/samr.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/events/events.h"

static int net_samdump_keytab_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net samdump keytab <keytab>\n");
	return 0;	
}

static int net_samdump_keytab_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Dumps kerberos keys of a domain into a keytab.\n");
	return 0;	
}

static int net_samdump_keytab(struct net_context *ctx, int argc, const char **argv) 
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_SamDump_keytab r;

	switch (argc) {
	case 0:
		return net_samdump_keytab_usage(ctx, argc, argv);
		break;
	case 1:
		r.in.keytab_name = argv[0];
		break;
	}

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;	
	}
	libnetctx->cred = ctx->credentials;

	r.out.error_string = NULL;
	r.in.machine_account = NULL;
	r.in.binding_string = NULL;

	status = libnet_SamDump_keytab(libnetctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("libnet_SamDump returned %s: %s\n",
			 nt_errstr(status),
			 r.out.error_string));
		return -1;
	}

	talloc_free(libnetctx);

	return 0;
}

/* main function table */
static const struct net_functable net_samdump_functable[] = {
	{"keytab", "dump keys into a keytab\n", net_samdump_keytab, net_samdump_keytab_usage},
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
	d_printf("net samdump\n");
	d_printf("net samdump keytab <keytab>\n");
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
	r.in.session_info = system_session(libnetctx, ctx->lp_ctx);

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
	d_printf("net samsync\n");
	return 0;	
}

int net_samsync_ldb_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Synchronise into the local ldb the SAM of a domain.\n");
	return 0;	
}

int net_vampire(struct net_context *ctx, int argc, const char **argv) 
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_Vampire *r;
	char *tmp, *targetdir = NULL;
	const char *domain_name;

	switch (argc) {
		case 0: /* no args -> fail */
			return net_vampire_usage(ctx, argc, argv);
		case 1: /* only DOMAIN */
			tmp = talloc_strdup(ctx, argv[0]);
			break;
		case 2: /* domain and target dir */
			tmp = talloc_strdup(ctx, argv[0]);
			targetdir = talloc_strdup(ctx, argv[1]);
			break;
		default: /* too many args -> fail */
			return net_vampire_usage(ctx, argc, argv);
	}

	domain_name = tmp;

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;	
	}
	libnetctx->cred = ctx->credentials;
	r = talloc(ctx, struct libnet_Vampire);
	if (!r) {
		return -1;
	}
	/* prepare parameters for the vampire */
	r->in.netbios_name  = lp_netbios_name(ctx->lp_ctx);
	r->in.domain_name   = domain_name;
	r->in.targetdir	    = targetdir;
	r->out.error_string = NULL;

	/* do the domain vampire */
	status = libnet_Vampire(libnetctx, r, r);
	
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "Vampire of domain failed: %s\n",
			  r->out.error_string ? r->out.error_string : nt_errstr(status));
		talloc_free(r);
		talloc_free(libnetctx);
		return -1;
	}
	d_printf("Vampired domain %s (%s)\n", r->out.domain_name, dom_sid_string(ctx, r->out.domain_sid));

	talloc_free(libnetctx);
	return 0;
}

int net_vampire_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net vampire <domain> [options]\n");
	return 0;	
}

int net_vampire_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Join and synchronise a remote AD domain to the local server.\n");
	return 0;	
}
