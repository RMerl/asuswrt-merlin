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
#include "librpc/gen_ndr/libnetapi.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"
#include "librpc/gen_ndr/ndr_libnetapi.h"

/****************************************************************
 NetJoinDomain
****************************************************************/

NET_API_STATUS NetJoinDomain(const char * server /* [in] [unique] */,
			     const char * domain /* [in] [ref] */,
			     const char * account_ou /* [in] [unique] */,
			     const char * account /* [in] [unique] */,
			     const char * password /* [in] [unique] */,
			     uint32_t join_flags /* [in] */)
{
	struct NetJoinDomain r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server = server;
	r.in.domain = domain;
	r.in.account_ou = account_ou;
	r.in.account = account;
	r.in.password = password;
	r.in.join_flags = join_flags;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetJoinDomain, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server)) {
		werr = NetJoinDomain_l(ctx, &r);
	} else {
		werr = NetJoinDomain_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetJoinDomain, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUnjoinDomain
****************************************************************/

NET_API_STATUS NetUnjoinDomain(const char * server_name /* [in] [unique] */,
			       const char * account /* [in] [unique] */,
			       const char * password /* [in] [unique] */,
			       uint32_t unjoin_flags /* [in] */)
{
	struct NetUnjoinDomain r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.account = account;
	r.in.password = password;
	r.in.unjoin_flags = unjoin_flags;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUnjoinDomain, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUnjoinDomain_l(ctx, &r);
	} else {
		werr = NetUnjoinDomain_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUnjoinDomain, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGetJoinInformation
****************************************************************/

NET_API_STATUS NetGetJoinInformation(const char * server_name /* [in] [unique] */,
				     const char * *name_buffer /* [out] [ref] */,
				     uint16_t *name_type /* [out] [ref] */)
{
	struct NetGetJoinInformation r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;

	/* Out parameters */
	r.out.name_buffer = name_buffer;
	r.out.name_type = name_type;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGetJoinInformation, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGetJoinInformation_l(ctx, &r);
	} else {
		werr = NetGetJoinInformation_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGetJoinInformation, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGetJoinableOUs
****************************************************************/

NET_API_STATUS NetGetJoinableOUs(const char * server_name /* [in] [unique] */,
				 const char * domain /* [in] [ref] */,
				 const char * account /* [in] [unique] */,
				 const char * password /* [in] [unique] */,
				 uint32_t *ou_count /* [out] [ref] */,
				 const char * **ous /* [out] [ref] */)
{
	struct NetGetJoinableOUs r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.domain = domain;
	r.in.account = account;
	r.in.password = password;

	/* Out parameters */
	r.out.ou_count = ou_count;
	r.out.ous = ous;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGetJoinableOUs, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGetJoinableOUs_l(ctx, &r);
	} else {
		werr = NetGetJoinableOUs_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGetJoinableOUs, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetRenameMachineInDomain
****************************************************************/

NET_API_STATUS NetRenameMachineInDomain(const char * server_name /* [in] */,
					const char * new_machine_name /* [in] */,
					const char * account /* [in] */,
					const char * password /* [in] */,
					uint32_t rename_options /* [in] */)
{
	struct NetRenameMachineInDomain r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.new_machine_name = new_machine_name;
	r.in.account = account;
	r.in.password = password;
	r.in.rename_options = rename_options;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetRenameMachineInDomain, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetRenameMachineInDomain_l(ctx, &r);
	} else {
		werr = NetRenameMachineInDomain_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetRenameMachineInDomain, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetServerGetInfo
****************************************************************/

NET_API_STATUS NetServerGetInfo(const char * server_name /* [in] [unique] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */)
{
	struct NetServerGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetServerGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetServerGetInfo_l(ctx, &r);
	} else {
		werr = NetServerGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetServerGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetServerSetInfo
****************************************************************/

NET_API_STATUS NetServerSetInfo(const char * server_name /* [in] [unique] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_error /* [out] [ref] */)
{
	struct NetServerSetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_error = parm_error;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetServerSetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetServerSetInfo_l(ctx, &r);
	} else {
		werr = NetServerSetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetServerSetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGetDCName
****************************************************************/

NET_API_STATUS NetGetDCName(const char * server_name /* [in] [unique] */,
			    const char * domain_name /* [in] [unique] */,
			    uint8_t **buffer /* [out] [ref] */)
{
	struct NetGetDCName r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.domain_name = domain_name;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGetDCName, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGetDCName_l(ctx, &r);
	} else {
		werr = NetGetDCName_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGetDCName, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGetAnyDCName
****************************************************************/

NET_API_STATUS NetGetAnyDCName(const char * server_name /* [in] [unique] */,
			       const char * domain_name /* [in] [unique] */,
			       uint8_t **buffer /* [out] [ref] */)
{
	struct NetGetAnyDCName r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.domain_name = domain_name;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGetAnyDCName, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGetAnyDCName_l(ctx, &r);
	} else {
		werr = NetGetAnyDCName_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGetAnyDCName, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 DsGetDcName
****************************************************************/

NET_API_STATUS DsGetDcName(const char * server_name /* [in] [unique] */,
			   const char * domain_name /* [in] [ref] */,
			   struct GUID *domain_guid /* [in] [unique] */,
			   const char * site_name /* [in] [unique] */,
			   uint32_t flags /* [in] */,
			   struct DOMAIN_CONTROLLER_INFO **dc_info /* [out] [ref] */)
{
	struct DsGetDcName r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.domain_name = domain_name;
	r.in.domain_guid = domain_guid;
	r.in.site_name = site_name;
	r.in.flags = flags;

	/* Out parameters */
	r.out.dc_info = dc_info;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(DsGetDcName, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = DsGetDcName_l(ctx, &r);
	} else {
		werr = DsGetDcName_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(DsGetDcName, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserAdd
****************************************************************/

NET_API_STATUS NetUserAdd(const char * server_name /* [in] [unique] */,
			  uint32_t level /* [in] */,
			  uint8_t *buffer /* [in] [ref] */,
			  uint32_t *parm_error /* [out] [ref] */)
{
	struct NetUserAdd r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_error = parm_error;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserAdd, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserAdd_l(ctx, &r);
	} else {
		werr = NetUserAdd_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserAdd, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserDel
****************************************************************/

NET_API_STATUS NetUserDel(const char * server_name /* [in] [unique] */,
			  const char * user_name /* [in] [ref] */)
{
	struct NetUserDel r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserDel, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserDel_l(ctx, &r);
	} else {
		werr = NetUserDel_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserDel, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserEnum
****************************************************************/

NET_API_STATUS NetUserEnum(const char * server_name /* [in] [unique] */,
			   uint32_t level /* [in] */,
			   uint32_t filter /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetUserEnum r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.filter = filter;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserEnum, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserEnum_l(ctx, &r);
	} else {
		werr = NetUserEnum_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserEnum, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserChangePassword
****************************************************************/

NET_API_STATUS NetUserChangePassword(const char * domain_name /* [in] */,
				     const char * user_name /* [in] */,
				     const char * old_password /* [in] */,
				     const char * new_password /* [in] */)
{
	struct NetUserChangePassword r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.domain_name = domain_name;
	r.in.user_name = user_name;
	r.in.old_password = old_password;
	r.in.new_password = new_password;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserChangePassword, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(domain_name)) {
		werr = NetUserChangePassword_l(ctx, &r);
	} else {
		werr = NetUserChangePassword_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserChangePassword, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserGetInfo
****************************************************************/

NET_API_STATUS NetUserGetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */)
{
	struct NetUserGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserGetInfo_l(ctx, &r);
	} else {
		werr = NetUserGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserSetInfo
****************************************************************/

NET_API_STATUS NetUserSetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t *buffer /* [in] [ref] */,
			      uint32_t *parm_err /* [out] [ref] */)
{
	struct NetUserSetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserSetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserSetInfo_l(ctx, &r);
	} else {
		werr = NetUserSetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserSetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserGetGroups
****************************************************************/

NET_API_STATUS NetUserGetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */)
{
	struct NetUserGetGroups r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserGetGroups, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserGetGroups_l(ctx, &r);
	} else {
		werr = NetUserGetGroups_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserGetGroups, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserSetGroups
****************************************************************/

NET_API_STATUS NetUserSetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */)
{
	struct NetUserSetGroups r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;
	r.in.level = level;
	r.in.buffer = buffer;
	r.in.num_entries = num_entries;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserSetGroups, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserSetGroups_l(ctx, &r);
	} else {
		werr = NetUserSetGroups_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserSetGroups, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserGetLocalGroups
****************************************************************/

NET_API_STATUS NetUserGetLocalGroups(const char * server_name /* [in] */,
				     const char * user_name /* [in] */,
				     uint32_t level /* [in] */,
				     uint32_t flags /* [in] */,
				     uint8_t **buffer /* [out] [ref] */,
				     uint32_t prefmaxlen /* [in] */,
				     uint32_t *entries_read /* [out] [ref] */,
				     uint32_t *total_entries /* [out] [ref] */)
{
	struct NetUserGetLocalGroups r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.user_name = user_name;
	r.in.level = level;
	r.in.flags = flags;
	r.in.prefmaxlen = prefmaxlen;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserGetLocalGroups, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserGetLocalGroups_l(ctx, &r);
	} else {
		werr = NetUserGetLocalGroups_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserGetLocalGroups, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserModalsGet
****************************************************************/

NET_API_STATUS NetUserModalsGet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */)
{
	struct NetUserModalsGet r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserModalsGet, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserModalsGet_l(ctx, &r);
	} else {
		werr = NetUserModalsGet_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserModalsGet, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetUserModalsSet
****************************************************************/

NET_API_STATUS NetUserModalsSet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */)
{
	struct NetUserModalsSet r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetUserModalsSet, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetUserModalsSet_l(ctx, &r);
	} else {
		werr = NetUserModalsSet_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetUserModalsSet, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetQueryDisplayInformation
****************************************************************/

NET_API_STATUS NetQueryDisplayInformation(const char * server_name /* [in] [unique] */,
					  uint32_t level /* [in] */,
					  uint32_t idx /* [in] */,
					  uint32_t entries_requested /* [in] */,
					  uint32_t prefmaxlen /* [in] */,
					  uint32_t *entries_read /* [out] [ref] */,
					  void **buffer /* [out] [noprint,ref] */)
{
	struct NetQueryDisplayInformation r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.idx = idx;
	r.in.entries_requested = entries_requested;
	r.in.prefmaxlen = prefmaxlen;

	/* Out parameters */
	r.out.entries_read = entries_read;
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetQueryDisplayInformation, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetQueryDisplayInformation_l(ctx, &r);
	} else {
		werr = NetQueryDisplayInformation_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetQueryDisplayInformation, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupAdd
****************************************************************/

NET_API_STATUS NetGroupAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buffer /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */)
{
	struct NetGroupAdd r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupAdd, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupAdd_l(ctx, &r);
	} else {
		werr = NetGroupAdd_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupAdd, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupDel
****************************************************************/

NET_API_STATUS NetGroupDel(const char * server_name /* [in] */,
			   const char * group_name /* [in] */)
{
	struct NetGroupDel r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupDel, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupDel_l(ctx, &r);
	} else {
		werr = NetGroupDel_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupDel, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupEnum
****************************************************************/

NET_API_STATUS NetGroupEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetGroupEnum r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupEnum, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupEnum_l(ctx, &r);
	} else {
		werr = NetGroupEnum_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupEnum, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupSetInfo
****************************************************************/

NET_API_STATUS NetGroupSetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buffer /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */)
{
	struct NetGroupSetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupSetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupSetInfo_l(ctx, &r);
	} else {
		werr = NetGroupSetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupSetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupGetInfo
****************************************************************/

NET_API_STATUS NetGroupGetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buffer /* [out] [ref] */)
{
	struct NetGroupGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupGetInfo_l(ctx, &r);
	} else {
		werr = NetGroupGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupAddUser
****************************************************************/

NET_API_STATUS NetGroupAddUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */)
{
	struct NetGroupAddUser r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.user_name = user_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupAddUser, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupAddUser_l(ctx, &r);
	} else {
		werr = NetGroupAddUser_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupAddUser, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupDelUser
****************************************************************/

NET_API_STATUS NetGroupDelUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */)
{
	struct NetGroupDelUser r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.user_name = user_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupDelUser, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupDelUser_l(ctx, &r);
	} else {
		werr = NetGroupDelUser_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupDelUser, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupGetUsers
****************************************************************/

NET_API_STATUS NetGroupGetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */,
				uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetGroupGetUsers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupGetUsers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupGetUsers_l(ctx, &r);
	} else {
		werr = NetGroupGetUsers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupGetUsers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetGroupSetUsers
****************************************************************/

NET_API_STATUS NetGroupSetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */)
{
	struct NetGroupSetUsers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;
	r.in.num_entries = num_entries;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetGroupSetUsers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetGroupSetUsers_l(ctx, &r);
	} else {
		werr = NetGroupSetUsers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetGroupSetUsers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupAdd
****************************************************************/

NET_API_STATUS NetLocalGroupAdd(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */)
{
	struct NetLocalGroupAdd r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupAdd, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupAdd_l(ctx, &r);
	} else {
		werr = NetLocalGroupAdd_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupAdd, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupDel
****************************************************************/

NET_API_STATUS NetLocalGroupDel(const char * server_name /* [in] */,
				const char * group_name /* [in] */)
{
	struct NetLocalGroupDel r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupDel, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupDel_l(ctx, &r);
	} else {
		werr = NetLocalGroupDel_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupDel, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupGetInfo
****************************************************************/

NET_API_STATUS NetLocalGroupGetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t **buffer /* [out] [ref] */)
{
	struct NetLocalGroupGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupGetInfo_l(ctx, &r);
	} else {
		werr = NetLocalGroupGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupSetInfo
****************************************************************/

NET_API_STATUS NetLocalGroupSetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t *buffer /* [in] [ref] */,
				    uint32_t *parm_err /* [out] [ref] */)
{
	struct NetLocalGroupSetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupSetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupSetInfo_l(ctx, &r);
	} else {
		werr = NetLocalGroupSetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupSetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupEnum
****************************************************************/

NET_API_STATUS NetLocalGroupEnum(const char * server_name /* [in] */,
				 uint32_t level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */,
				 uint32_t prefmaxlen /* [in] */,
				 uint32_t *entries_read /* [out] [ref] */,
				 uint32_t *total_entries /* [out] [ref] */,
				 uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetLocalGroupEnum r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupEnum, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupEnum_l(ctx, &r);
	} else {
		werr = NetLocalGroupEnum_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupEnum, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupAddMembers
****************************************************************/

NET_API_STATUS NetLocalGroupAddMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */)
{
	struct NetLocalGroupAddMembers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;
	r.in.total_entries = total_entries;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupAddMembers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupAddMembers_l(ctx, &r);
	} else {
		werr = NetLocalGroupAddMembers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupAddMembers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupDelMembers
****************************************************************/

NET_API_STATUS NetLocalGroupDelMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */)
{
	struct NetLocalGroupDelMembers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;
	r.in.total_entries = total_entries;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupDelMembers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupDelMembers_l(ctx, &r);
	} else {
		werr = NetLocalGroupDelMembers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupDelMembers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupGetMembers
****************************************************************/

NET_API_STATUS NetLocalGroupGetMembers(const char * server_name /* [in] */,
				       const char * local_group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t **buffer /* [out] [ref] */,
				       uint32_t prefmaxlen /* [in] */,
				       uint32_t *entries_read /* [out] [ref] */,
				       uint32_t *total_entries /* [out] [ref] */,
				       uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetLocalGroupGetMembers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.local_group_name = local_group_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupGetMembers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupGetMembers_l(ctx, &r);
	} else {
		werr = NetLocalGroupGetMembers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupGetMembers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetLocalGroupSetMembers
****************************************************************/

NET_API_STATUS NetLocalGroupSetMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */)
{
	struct NetLocalGroupSetMembers r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.group_name = group_name;
	r.in.level = level;
	r.in.buffer = buffer;
	r.in.total_entries = total_entries;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetLocalGroupSetMembers, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetLocalGroupSetMembers_l(ctx, &r);
	} else {
		werr = NetLocalGroupSetMembers_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetLocalGroupSetMembers, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetRemoteTOD
****************************************************************/

NET_API_STATUS NetRemoteTOD(const char * server_name /* [in] */,
			    uint8_t **buffer /* [out] [ref] */)
{
	struct NetRemoteTOD r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetRemoteTOD, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetRemoteTOD_l(ctx, &r);
	} else {
		werr = NetRemoteTOD_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetRemoteTOD, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShareAdd
****************************************************************/

NET_API_STATUS NetShareAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buffer /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */)
{
	struct NetShareAdd r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShareAdd, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShareAdd_l(ctx, &r);
	} else {
		werr = NetShareAdd_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShareAdd, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShareDel
****************************************************************/

NET_API_STATUS NetShareDel(const char * server_name /* [in] */,
			   const char * net_name /* [in] */,
			   uint32_t reserved /* [in] */)
{
	struct NetShareDel r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.net_name = net_name;
	r.in.reserved = reserved;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShareDel, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShareDel_l(ctx, &r);
	} else {
		werr = NetShareDel_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShareDel, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShareEnum
****************************************************************/

NET_API_STATUS NetShareEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetShareEnum r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShareEnum, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShareEnum_l(ctx, &r);
	} else {
		werr = NetShareEnum_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShareEnum, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShareGetInfo
****************************************************************/

NET_API_STATUS NetShareGetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buffer /* [out] [ref] */)
{
	struct NetShareGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.net_name = net_name;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShareGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShareGetInfo_l(ctx, &r);
	} else {
		werr = NetShareGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShareGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShareSetInfo
****************************************************************/

NET_API_STATUS NetShareSetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buffer /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */)
{
	struct NetShareSetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.net_name = net_name;
	r.in.level = level;
	r.in.buffer = buffer;

	/* Out parameters */
	r.out.parm_err = parm_err;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShareSetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShareSetInfo_l(ctx, &r);
	} else {
		werr = NetShareSetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShareSetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetFileClose
****************************************************************/

NET_API_STATUS NetFileClose(const char * server_name /* [in] */,
			    uint32_t fileid /* [in] */)
{
	struct NetFileClose r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.fileid = fileid;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetFileClose, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetFileClose_l(ctx, &r);
	} else {
		werr = NetFileClose_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetFileClose, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetFileGetInfo
****************************************************************/

NET_API_STATUS NetFileGetInfo(const char * server_name /* [in] */,
			      uint32_t fileid /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */)
{
	struct NetFileGetInfo r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.fileid = fileid;
	r.in.level = level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetFileGetInfo, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetFileGetInfo_l(ctx, &r);
	} else {
		werr = NetFileGetInfo_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetFileGetInfo, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetFileEnum
****************************************************************/

NET_API_STATUS NetFileEnum(const char * server_name /* [in] */,
			   const char * base_path /* [in] */,
			   const char * user_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */)
{
	struct NetFileEnum r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.base_path = base_path;
	r.in.user_name = user_name;
	r.in.level = level;
	r.in.prefmaxlen = prefmaxlen;
	r.in.resume_handle = resume_handle;

	/* Out parameters */
	r.out.buffer = buffer;
	r.out.entries_read = entries_read;
	r.out.total_entries = total_entries;
	r.out.resume_handle = resume_handle;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetFileEnum, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetFileEnum_l(ctx, &r);
	} else {
		werr = NetFileEnum_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetFileEnum, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShutdownInit
****************************************************************/

NET_API_STATUS NetShutdownInit(const char * server_name /* [in] */,
			       const char * message /* [in] */,
			       uint32_t timeout /* [in] */,
			       uint8_t force_apps /* [in] */,
			       uint8_t do_reboot /* [in] */)
{
	struct NetShutdownInit r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.message = message;
	r.in.timeout = timeout;
	r.in.force_apps = force_apps;
	r.in.do_reboot = do_reboot;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShutdownInit, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShutdownInit_l(ctx, &r);
	} else {
		werr = NetShutdownInit_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShutdownInit, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 NetShutdownAbort
****************************************************************/

NET_API_STATUS NetShutdownAbort(const char * server_name /* [in] */)
{
	struct NetShutdownAbort r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;

	/* Out parameters */

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(NetShutdownAbort, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = NetShutdownAbort_l(ctx, &r);
	} else {
		werr = NetShutdownAbort_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(NetShutdownAbort, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 I_NetLogonControl
****************************************************************/

NET_API_STATUS I_NetLogonControl(const char * server_name /* [in] */,
				 uint32_t function_code /* [in] */,
				 uint32_t query_level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */)
{
	struct I_NetLogonControl r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.function_code = function_code;
	r.in.query_level = query_level;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(I_NetLogonControl, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = I_NetLogonControl_l(ctx, &r);
	} else {
		werr = I_NetLogonControl_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(I_NetLogonControl, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

/****************************************************************
 I_NetLogonControl2
****************************************************************/

NET_API_STATUS I_NetLogonControl2(const char * server_name /* [in] */,
				  uint32_t function_code /* [in] */,
				  uint32_t query_level /* [in] */,
				  uint8_t *data /* [in] [ref] */,
				  uint8_t **buffer /* [out] [ref] */)
{
	struct I_NetLogonControl2 r;
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status;
	WERROR werr;
	TALLOC_CTX *frame = talloc_stackframe();

	status = libnetapi_getctx(&ctx);
	if (status != 0) {
		TALLOC_FREE(frame);
		return status;
	}

	/* In parameters */
	r.in.server_name = server_name;
	r.in.function_code = function_code;
	r.in.query_level = query_level;
	r.in.data = data;

	/* Out parameters */
	r.out.buffer = buffer;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(I_NetLogonControl2, &r);
	}

	if (LIBNETAPI_LOCAL_SERVER(server_name)) {
		werr = I_NetLogonControl2_l(ctx, &r);
	} else {
		werr = I_NetLogonControl2_r(ctx, &r);
	}

	r.out.result = W_ERROR_V(werr);

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(I_NetLogonControl2, &r);
	}

	TALLOC_FREE(frame);
	return (NET_API_STATUS)r.out.result;
}

