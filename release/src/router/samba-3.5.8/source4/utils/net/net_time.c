/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 

   Copyright (C) 2004 Stefan Metzmacher (metze@samba.org)

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
#include "libnet/libnet.h"
#include "utils/net/net.h"
#include "system/time.h"
#include "lib/events/events.h"

/*
 * Code for getting the remote time
 */

int net_time(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	union libnet_RemoteTOD r;
	const char *server_name;
	struct tm *tm;
	char timestr[64];

	if (argc > 0 && argv[0]) {
		server_name = argv[0];
	} else {
		return net_time_usage(ctx, argc, argv);
	}

	libnetctx = libnet_context_init(ctx->event_ctx, ctx->lp_ctx);
	if (!libnetctx) {
		return -1;	
	}
	libnetctx->cred = ctx->credentials;

	/* prepare to get the time */
	r.generic.level			= LIBNET_REMOTE_TOD_GENERIC;
	r.generic.in.server_name	= server_name;

	/* get the time */
	status = libnet_RemoteTOD(libnetctx, ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("net_time: %s\n",r.generic.out.error_string));
		return -1;
	}

	ZERO_STRUCT(timestr);
	tm = localtime(&r.generic.out.time);
	strftime(timestr, sizeof(timestr)-1, "%c %Z",tm);

	printf("%s\n",timestr);

	talloc_free(libnetctx);

	return 0;
}

int net_time_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net time <server> [options]\n");
	return 0;	
}
