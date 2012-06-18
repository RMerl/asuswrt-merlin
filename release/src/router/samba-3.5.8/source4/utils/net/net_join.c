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
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/events/events.h"

int net_join(struct net_context *ctx, int argc, const char **argv) 
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	struct libnet_Join *r;
	char *tmp;
	const char *domain_name;
	enum netr_SchannelType secure_channel_type = SEC_CHAN_WKSTA;

	switch (argc) {
		case 0: /* no args -> fail */
			return net_join_usage(ctx, argc, argv);
		case 1: /* only DOMAIN */
			tmp = talloc_strdup(ctx, argv[0]);
			break;
		case 2: /* DOMAIN and role */
			tmp = talloc_strdup(ctx, argv[0]);
			if (strcasecmp(argv[1], "BDC") == 0) {
				secure_channel_type = SEC_CHAN_BDC;
			} else if (strcasecmp(argv[1], "MEMBER") == 0) {
				secure_channel_type = SEC_CHAN_WKSTA;
			} else {
				d_fprintf(stderr, "net_join: Invalid 2nd argument (%s) must be MEMBER or BDC\n", argv[1]);
				return net_join_usage(ctx, argc, argv);
			}
			break;
		default: /* too many args -> fail */
			return net_join_usage(ctx, argc, argv);
	}

	domain_name = tmp;

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;	
	}
	libnetctx->cred = ctx->credentials;
	r = talloc(ctx, struct libnet_Join);
	if (!r) {
		return -1;
	}
	/* prepare parameters for the join */
	r->in.netbios_name		= lp_netbios_name(ctx->lp_ctx);
	r->in.domain_name		= domain_name;
	r->in.join_type           	= secure_channel_type;
	r->in.level			= LIBNET_JOIN_AUTOMATIC;
	r->out.error_string		= NULL;

	/* do the domain join */
	status = libnet_Join(libnetctx, r, r);
	
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "Joining domain failed: %s\n",
			  r->out.error_string ? r->out.error_string : nt_errstr(status));
		talloc_free(r);
		talloc_free(libnetctx);
		return -1;
	}
	d_printf("Joined domain %s (%s)\n", r->out.domain_name, dom_sid_string(ctx, r->out.domain_sid));

	talloc_free(libnetctx);
	return 0;
}

int net_join_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net join <domain> [BDC | MEMBER] [options]\n");
	return 0;	
}

int net_join_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Joins domain as either member or backup domain controller.\n");
	return 0;	
}

