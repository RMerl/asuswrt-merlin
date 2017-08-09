/* 
   Unix SMB/CIFS implementation.

   DRSUapi tests

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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

#include "librpc/gen_ndr/drsuapi.h"

/**
 * Data structure common for most of DRSUAPI tests
 */
struct DsPrivate {
	struct dcerpc_pipe *drs_pipe;
	struct policy_handle bind_handle;
	struct GUID bind_guid;
	struct drsuapi_DsBindInfo28 srv_bind_info;

	const char *domain_obj_dn;
	const char *domain_guid_str;
	const char *domain_dns_name;
	struct GUID domain_guid;
	struct drsuapi_DsGetDCInfo2 dcinfo;
	struct test_join *join;
};


/**
 * Custom torture macro to check dcerpc_drsuapi_ call
 * return values printing more friendly messages
 * \param _tctx torture context
 * \param _p DCERPC pipe handle
 * \param _ntstatus NTSTATUS for dcerpc_drsuapi_ call
 * \param _werr_expected Expected windows error to be returned
 * \param _pr in/out DCEPRC request structure - pointer
 * \param _msg error message prefix
 */
#define torture_drsuapi_assert_call_werr(_tctx, _p, _ntstat, _werr_expected, _pr, _msg) \
	do { \
		NTSTATUS __nt = _ntstat; \
		if (!NT_STATUS_IS_OK(__nt)) { \
			const char *errstr = nt_errstr(__nt); \
			torture_fail(tctx, talloc_asprintf(_tctx, "%s failed - %s", _msg, errstr)); \
		} \
		torture_assert_werr_equal(_tctx, (_pr)->out.result, _werr_expected, _msg); \
	} while(0)

/**
 * Custom torture macro to check dcerpc_drsuapi_ call
 * return values printing more friendly messages
 * \param _tctx torture context
 * \param _p DCERPC pipe handle
 * \param _ntstatus NTSTATUS for dcerpc_drsuapi_ call
 * \param _pr in/out DCEPRC request structure
 * \param _msg error message prefix
 */
#define torture_drsuapi_assert_call(_tctx, _p, _ntstat, _pr, _msg) \
	torture_drsuapi_assert_call_werr(_tctx, _p, _ntstat, WERR_OK, _pr, _msg)

