/*
 *  Unix SMB/CIFS implementation.
 *  NetApi testsuite
 *  Copyright (C) Guenther Deschner 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <popt.h>

void popt_common_callback(poptContext con,
			 enum poptCallbackReason reason,
			 const struct poptOption *opt,
			 const char *arg, const void *data);

extern struct poptOption popt_common_netapi_examples[];

#define POPT_COMMON_LIBNETAPI_EXAMPLES { NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_common_netapi_examples, 0, "Common samba netapi example options:", NULL },

NET_API_STATUS test_netuseradd(const char *hostname,
			       const char *username);

NET_API_STATUS netapitest_localgroup(struct libnetapi_ctx *ctx,
				     const char *hostname);
NET_API_STATUS netapitest_user(struct libnetapi_ctx *ctx,
			       const char *hostname);
NET_API_STATUS netapitest_group(struct libnetapi_ctx *ctx,
				const char *hostname);
NET_API_STATUS netapitest_display(struct libnetapi_ctx *ctx,
				  const char *hostname);
NET_API_STATUS netapitest_share(struct libnetapi_ctx *ctx,
				const char *hostname);
NET_API_STATUS netapitest_file(struct libnetapi_ctx *ctx,
			       const char *hostname);
NET_API_STATUS netapitest_server(struct libnetapi_ctx *ctx,
				 const char *hostname);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

#define NETAPI_STATUS(x,y,fn) \
	printf("FAILURE: line %d: %s failed with status: %s (%d)\n", \
		__LINE__, fn, libnetapi_get_error_string(x,y), y);

#define NETAPI_STATUS_MSG(x,y,fn,z) \
	printf("FAILURE: line %d: %s failed with status: %s (%d), %s\n", \
		__LINE__, fn, libnetapi_get_error_string(x,y), y, z);

#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))
