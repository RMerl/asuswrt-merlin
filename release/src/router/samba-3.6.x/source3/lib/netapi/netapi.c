/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Guenther Deschner 2007-2008
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

#include "includes.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "secrets.h"
#include "krb5_env.h"

struct libnetapi_ctx *stat_ctx = NULL;
TALLOC_CTX *frame = NULL;
static bool libnetapi_initialized = false;

/****************************************************************
****************************************************************/

static NET_API_STATUS libnetapi_init_private_context(struct libnetapi_ctx *ctx)
{
	struct libnetapi_private_ctx *priv;

	if (!ctx) {
		return W_ERROR_V(WERR_INVALID_PARAM);
	}

	priv = TALLOC_ZERO_P(ctx, struct libnetapi_private_ctx);
	if (!priv) {
		return W_ERROR_V(WERR_NOMEM);
	}

	ctx->private_data = priv;

	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
Create a libnetapi context, for use in non-Samba applications.  This
loads the smb.conf file and sets the debug level to 0, so that
applications are not flooded with debug logs at level 10, when they
were not expecting it.
****************************************************************/

NET_API_STATUS libnetapi_init(struct libnetapi_ctx **context)
{
	if (stat_ctx && libnetapi_initialized) {
		*context = stat_ctx;
		return NET_API_STATUS_SUCCESS;
	}

#if 0
	talloc_enable_leak_report();
#endif
	frame = talloc_stackframe();

	/* Case tables must be loaded before any string comparisons occour */
	load_case_tables_library();

	/* When libnetapi is invoked from an application, it does not
	 * want to be swamped with level 10 debug messages, even if
	 * this has been set for the server in smb.conf */
	lp_set_cmdline("log level", "0");
	setup_logging("libnetapi", DEBUG_STDERR);

	if (!lp_load(get_dyn_CONFIGFILE(), true, false, false, false)) {
		TALLOC_FREE(frame);
		fprintf(stderr, "error loading %s\n", get_dyn_CONFIGFILE() );
		return W_ERROR_V(WERR_GENERAL_FAILURE);
	}

	init_names();
	load_interfaces();
	reopen_logs();

	BlockSignals(True, SIGPIPE);

	return libnetapi_net_init(context);
}

/****************************************************************
Create a libnetapi context, for use inside the 'net' binary.

As we know net has already loaded the smb.conf file, and set the debug
level etc, this avoids doing so again (which causes trouble with -d on
the command line).
****************************************************************/

NET_API_STATUS libnetapi_net_init(struct libnetapi_ctx **context)
{
	NET_API_STATUS status;
	struct libnetapi_ctx *ctx = NULL;

	frame = talloc_stackframe();

	ctx = talloc_zero(frame, struct libnetapi_ctx);
	if (!ctx) {
		TALLOC_FREE(frame);
		return W_ERROR_V(WERR_NOMEM);
	}

	BlockSignals(True, SIGPIPE);

	if (getenv("USER")) {
		ctx->username = talloc_strdup(frame, getenv("USER"));
	} else {
		ctx->username = talloc_strdup(frame, "");
	}
	if (!ctx->username) {
		TALLOC_FREE(frame);
		fprintf(stderr, "libnetapi_init: out of memory\n");
		return W_ERROR_V(WERR_NOMEM);
	}

	status = libnetapi_init_private_context(ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	libnetapi_initialized = true;

	*context = stat_ctx = ctx;

	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
 Return the static libnetapi context
****************************************************************/

NET_API_STATUS libnetapi_getctx(struct libnetapi_ctx **ctx)
{
	if (stat_ctx) {
		*ctx = stat_ctx;
		return NET_API_STATUS_SUCCESS;
	}

	return libnetapi_init(ctx);
}

/****************************************************************
 Free the static libnetapi context
****************************************************************/

NET_API_STATUS libnetapi_free(struct libnetapi_ctx *ctx)
{
	if (!ctx) {
		return NET_API_STATUS_SUCCESS;
	}

	libnetapi_samr_free(ctx);

	libnetapi_shutdown_cm(ctx);

	if (ctx->krb5_cc_env) {
		char *env = getenv(KRB5_ENV_CCNAME);
		if (env && (strequal(ctx->krb5_cc_env, env))) {
			unsetenv(KRB5_ENV_CCNAME);
		}
	}

	gfree_names();
	gfree_loadparm();
	gfree_case_tables();
	gfree_charcnv();
	gfree_interfaces();

	secrets_shutdown();

	TALLOC_FREE(ctx);
	TALLOC_FREE(frame);

	gfree_debugsyms();

	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
 Override the current log level for libnetapi
****************************************************************/

NET_API_STATUS libnetapi_set_debuglevel(struct libnetapi_ctx *ctx,
					const char *debuglevel)
{
	ctx->debuglevel = talloc_strdup(ctx, debuglevel);
	if (!lp_set_cmdline("log level", debuglevel)) {
		return W_ERROR_V(WERR_GENERAL_FAILURE);
	}
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_get_debuglevel(struct libnetapi_ctx *ctx,
					char **debuglevel)
{
	*debuglevel = ctx->debuglevel;
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_username(struct libnetapi_ctx *ctx,
				      const char *username)
{
	TALLOC_FREE(ctx->username);
	ctx->username = talloc_strdup(ctx, username ? username : "");

	if (!ctx->username) {
		return W_ERROR_V(WERR_NOMEM);
	}
	return NET_API_STATUS_SUCCESS;
}

NET_API_STATUS libnetapi_set_password(struct libnetapi_ctx *ctx,
				      const char *password)
{
	TALLOC_FREE(ctx->password);
	ctx->password = talloc_strdup(ctx, password);
	if (!ctx->password) {
		return W_ERROR_V(WERR_NOMEM);
	}
	return NET_API_STATUS_SUCCESS;
}

NET_API_STATUS libnetapi_set_workgroup(struct libnetapi_ctx *ctx,
				       const char *workgroup)
{
	TALLOC_FREE(ctx->workgroup);
	ctx->workgroup = talloc_strdup(ctx, workgroup);
	if (!ctx->workgroup) {
		return W_ERROR_V(WERR_NOMEM);
	}
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_use_kerberos(struct libnetapi_ctx *ctx)
{
	ctx->use_kerberos = true;
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_use_ccache(struct libnetapi_ctx *ctx)
{
	ctx->use_ccache = true;
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

const char *libnetapi_errstr(NET_API_STATUS status)
{
	if (status & 0xc0000000) {
		return get_friendly_nt_error_msg(NT_STATUS(status));
	}

	return get_friendly_werror_msg(W_ERROR(status));
}

/****************************************************************
****************************************************************/

NET_API_STATUS libnetapi_set_error_string(struct libnetapi_ctx *ctx,
					  const char *format, ...)
{
	va_list args;

	TALLOC_FREE(ctx->error_string);

	va_start(args, format);
	ctx->error_string = talloc_vasprintf(ctx, format, args);
	va_end(args);

	if (!ctx->error_string) {
		return W_ERROR_V(WERR_NOMEM);
	}
	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

const char *libnetapi_get_error_string(struct libnetapi_ctx *ctx,
				       NET_API_STATUS status_in)
{
	NET_API_STATUS status;
	struct libnetapi_ctx *tmp_ctx = ctx;

	if (!tmp_ctx) {
		status = libnetapi_getctx(&tmp_ctx);
		if (status != 0) {
			return NULL;
		}
	}

	if (tmp_ctx->error_string) {
		return tmp_ctx->error_string;
	}

	return libnetapi_errstr(status_in);
}

/****************************************************************
****************************************************************/

NET_API_STATUS NetApiBufferAllocate(uint32_t byte_count,
				    void **buffer)
{
	void *buf = NULL;

	if (!buffer) {
		return W_ERROR_V(WERR_INSUFFICIENT_BUFFER);
	}

	if (byte_count == 0) {
		goto done;
	}

	buf = talloc_size(NULL, byte_count);
	if (!buf) {
		return W_ERROR_V(WERR_NOMEM);
	}

 done:
	*buffer = buf;

	return NET_API_STATUS_SUCCESS;
}

/****************************************************************
****************************************************************/

NET_API_STATUS NetApiBufferFree(void *buffer)
{
	if (!buffer) {
		return W_ERROR_V(WERR_INSUFFICIENT_BUFFER);
	}

	talloc_free(buffer);

	return NET_API_STATUS_SUCCESS;
}
