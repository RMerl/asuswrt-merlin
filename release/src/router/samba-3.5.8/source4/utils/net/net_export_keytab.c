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
#include "param/param.h"

static int net_export_keytab_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net export keytab <keytab>\n");
	return 0;	
}

static int net_export_keytab_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Dumps kerberos keys of the domain into a keytab.\n");
	return 0;	
}

static int net_export_keytab(struct net_context *ctx, int argc, const char **argv) 
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_export_keytab r;

	switch (argc) {
	case 0:
		return net_export_keytab_usage(ctx, argc, argv);
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

	status = libnet_export_keytab(libnetctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("libnet_export_keytab returned %s: %s\n",
			 nt_errstr(status),
			 r.out.error_string));
		return -1;
	}

	talloc_free(libnetctx);

	return 0;
}

/* main function table */
static const struct net_functable net_export_functable[] = {
	{"keytab", "dump keys into a keytab\n", net_export_keytab, net_export_keytab_usage},
	{NULL, NULL, NULL, NULL}
};

int net_export(struct net_context *ctx, int argc, const char **argv) 
{
	int rc;

	switch (argc) {
	case 0:
		rc = net_export_usage(ctx, argc, argv);
		return rc;
	case 1:
	default:
		rc = net_run_function(ctx, argc, argv, net_export_functable, 
				      net_export_usage);
		return rc;
	}

	return 0;
}

int net_export_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net export keytab <keytab>\n");
	return 0;	
}

int net_export_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Dumps the sam of the domain we are joined to.\n");
	return 0;	
}

