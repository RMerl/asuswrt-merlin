/*
 *  Unix SMB/CIFS implementation.
 *  NetApi LogonControl Support
 *  Copyright (C) Guenther Deschner 2009
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

#include "../librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/libnetapi.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"

static WERROR construct_data(enum netr_LogonControlCode function_code,
			     const uint8_t *data_in,
			     union netr_CONTROL_DATA_INFORMATION *data_out)
{
	switch (function_code) {
	case NETLOGON_CONTROL_QUERY:
	case NETLOGON_CONTROL_REDISCOVER:
	case NETLOGON_CONTROL_TC_QUERY:
	case NETLOGON_CONTROL_CHANGE_PASSWORD:
	case NETLOGON_CONTROL_TC_VERIFY:
		data_out->domain = (const char *)data_in;
		break;
	case NETLOGON_CONTROL_FIND_USER:
		data_out->user = (const char *)data_in;
		break;
	case NETLOGON_CONTROL_SET_DBFLAG:
		data_out->debug_level = atoi((const char *)data_in);
		break;
	case NETLOGON_CONTROL_FORCE_DNS_REG:
		ZERO_STRUCTP(data_out);
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	return WERR_OK;
}

static WERROR construct_buffer(TALLOC_CTX *mem_ctx,
			       uint32_t level,
			       union netr_CONTROL_QUERY_INFORMATION *q,
			       uint8_t **buffer)
{
	struct NETLOGON_INFO_1 *i1;
	struct NETLOGON_INFO_2 *i2;
	struct NETLOGON_INFO_3 *i3;
	struct NETLOGON_INFO_4 *i4;

	if (!q) {
		return WERR_INVALID_PARAM;
	}

	switch (level) {
	case 1:
		i1 = talloc(mem_ctx, struct NETLOGON_INFO_1);
		W_ERROR_HAVE_NO_MEMORY(i1);

		i1->netlog1_flags			= q->info1->flags;
		i1->netlog1_pdc_connection_status	= W_ERROR_V(q->info1->pdc_connection_status);

		*buffer = (uint8_t *)i1;

		break;
	case 2:
		i2 = talloc(mem_ctx, struct NETLOGON_INFO_2);
		W_ERROR_HAVE_NO_MEMORY(i2);

		i2->netlog2_flags			= q->info2->flags;
		i2->netlog2_pdc_connection_status	= W_ERROR_V(q->info2->pdc_connection_status);
		i2->netlog2_trusted_dc_name		= talloc_strdup(mem_ctx, q->info2->trusted_dc_name);
		i2->netlog2_tc_connection_status	= W_ERROR_V(q->info2->tc_connection_status);

		*buffer = (uint8_t *)i2;

		break;
	case 3:
		i3 = talloc(mem_ctx, struct NETLOGON_INFO_3);
		W_ERROR_HAVE_NO_MEMORY(i3);

		i3->netlog1_flags			= q->info3->flags;
		i3->netlog3_logon_attempts		= q->info3->logon_attempts;
		i3->netlog3_reserved1			= q->info3->unknown1;
		i3->netlog3_reserved2			= q->info3->unknown2;
		i3->netlog3_reserved3			= q->info3->unknown3;
		i3->netlog3_reserved4			= q->info3->unknown4;
		i3->netlog3_reserved5			= q->info3->unknown5;

		*buffer = (uint8_t *)i3;

		break;
	case 4:
		i4 = talloc(mem_ctx, struct NETLOGON_INFO_4);
		W_ERROR_HAVE_NO_MEMORY(i4);

		i4->netlog4_trusted_dc_name		= talloc_strdup(mem_ctx, q->info4->trusted_dc_name);
		i4->netlog4_trusted_domain_name		= talloc_strdup(mem_ctx, q->info4->trusted_domain_name);

		*buffer = (uint8_t *)i4;

		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}
	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR I_NetLogonControl_r(struct libnetapi_ctx *ctx,
			   struct I_NetLogonControl *r)
{
	WERROR werr;
	NTSTATUS status;
	union netr_CONTROL_QUERY_INFORMATION query;
	struct dcerpc_binding_handle *b;

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_netlogon.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	status = dcerpc_netr_LogonControl(b, talloc_tos(),
					  r->in.server_name,
					  r->in.function_code,
					  r->in.query_level,
					  &query,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = construct_buffer(ctx, r->in.query_level, &query,
				r->out.buffer);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR I_NetLogonControl_l(struct libnetapi_ctx *ctx,
			   struct I_NetLogonControl *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, I_NetLogonControl);
}

/****************************************************************
****************************************************************/

WERROR I_NetLogonControl2_r(struct libnetapi_ctx *ctx,
			    struct I_NetLogonControl2 *r)
{
	WERROR werr;
	NTSTATUS status;
	union netr_CONTROL_DATA_INFORMATION data;
	union netr_CONTROL_QUERY_INFORMATION query;
	struct dcerpc_binding_handle *b;

	werr = construct_data(r->in.function_code, r->in.data, &data);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = libnetapi_get_binding_handle(ctx, r->in.server_name,
					    &ndr_table_netlogon.syntax_id,
					    &b);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	switch (r->in.function_code) {
	case NETLOGON_CONTROL_TC_VERIFY:
	case NETLOGON_CONTROL_SET_DBFLAG:
	case NETLOGON_CONTROL_FORCE_DNS_REG:
		status = dcerpc_netr_LogonControl2Ex(b, talloc_tos(),
						     r->in.server_name,
						     r->in.function_code,
						     r->in.query_level,
						     &data,
						     &query,
						     &werr);
		break;
	default:
		status = dcerpc_netr_LogonControl2(b, talloc_tos(),
						   r->in.server_name,
						   r->in.function_code,
						   r->in.query_level,
						   &data,
						   &query,
						   &werr);
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = construct_buffer(ctx, r->in.query_level, &query,
				r->out.buffer);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR I_NetLogonControl2_l(struct libnetapi_ctx *ctx,
			    struct I_NetLogonControl2 *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, I_NetLogonControl2);
}
