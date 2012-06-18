/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 

   Copyright (C) Rafal Szczesniak <mimir@samba.org>  2005

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
#include "lib/events/events.h"
#include "auth/credentials/credentials.h"

static int net_user_add(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *lnet_ctx;
	struct libnet_CreateUser r;
	char *user_name;

	/* command line argument preparation */
	switch (argc) {
	case 0:
		return net_user_usage(ctx, argc, argv);
		break;
	case 1:
		user_name = talloc_strdup(ctx, argv[0]);
		break;
	default:
		return net_user_usage(ctx, argc, argv);
	}

	/* libnet context init and its params */
	lnet_ctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!lnet_ctx) return -1;

	lnet_ctx->cred = ctx->credentials;

	/* calling CreateUser function */
	r.in.user_name       = user_name;
	r.in.domain_name     = cli_credentials_get_domain(lnet_ctx->cred);

	status = libnet_CreateUser(lnet_ctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to add user account: %s\n",
			  r.out.error_string));
		return -1;
	}
	
	talloc_free(lnet_ctx);
	return 0;
}

static int net_user_delete(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *lnet_ctx;
	struct libnet_DeleteUser r;
	char *user_name;

	/* command line argument preparation */
	switch (argc) {
	case 0:
		return net_user_usage(ctx, argc, argv);
		break;
	case 1:
		user_name = talloc_strdup(ctx, argv[0]);
		break;
	default:
		return net_user_usage(ctx, argc, argv);
	}

	/* libnet context init and its params */
	lnet_ctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!lnet_ctx) return -1;

	lnet_ctx->cred = ctx->credentials;

	/* calling DeleteUser function */
	r.in.user_name       = user_name;
	r.in.domain_name     = cli_credentials_get_domain(lnet_ctx->cred);

	status = libnet_DeleteUser(lnet_ctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to delete user account: %s\n",
			  r.out.error_string));
		return -1;
	}
	
	talloc_free(lnet_ctx);
	return 0;
}


static const struct net_functable net_user_functable[] = {
	{ "add", "create new user account\n", net_user_add, net_user_usage },
	{ "delete", "delete an existing user account\n", net_user_delete, net_user_usage },
	{ NULL, NULL }
};


int net_user(struct net_context *ctx, int argc, const char **argv)
{
	return net_run_function(ctx, argc, argv, net_user_functable, net_user_usage);
}


int net_user_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net user <command> [options]\n");
	return 0;
}
