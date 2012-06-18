/*
   Unix SMB/CIFS implementation.
   test suite for srvsvc rpc operations

   Copyright (C) Jelmer Vernooij 2004
   Copyright (C) Guenther Deschner 2008,2009

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
#include "torture/torture.h"
#include "librpc/gen_ndr/ndr_svcctl_c.h"
#include "librpc/gen_ndr/ndr_svcctl.h"
#include "torture/rpc/rpc.h"
#include "param/param.h"

#define TORTURE_DEFAULT_SERVICE "Spooler"

static bool test_OpenSCManager(struct dcerpc_pipe *p, struct torture_context *tctx, struct policy_handle *h)
{
	struct svcctl_OpenSCManagerW r;

	r.in.MachineName = NULL;
	r.in.DatabaseName = NULL;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = h;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_svcctl_OpenSCManagerW(p, tctx, &r),
				   "OpenSCManager failed!");

	return true;
}

static bool test_CloseServiceHandle(struct dcerpc_pipe *p, struct torture_context *tctx, struct policy_handle *h)
{
	struct svcctl_CloseServiceHandle r;

	r.in.handle = h;
	r.out.handle = h;
	torture_assert_ntstatus_ok(tctx,
				   dcerpc_svcctl_CloseServiceHandle(p, tctx, &r),
				   "CloseServiceHandle failed");

	return true;
}

static bool test_OpenService(struct dcerpc_pipe *p, struct torture_context *tctx,
			     struct policy_handle *h, const char *name, struct policy_handle *s)
{
	struct svcctl_OpenServiceW r;

	r.in.scmanager_handle = h;
	r.in.ServiceName = name;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = s;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_svcctl_OpenServiceW(p, tctx, &r),
				   "OpenServiceW failed!");
	torture_assert_werr_ok(tctx, r.out.result, "OpenServiceW failed!");

	return true;

}

static bool test_QueryServiceStatus(struct torture_context *tctx,
				    struct dcerpc_pipe *p)
{
	struct svcctl_QueryServiceStatus r;
	struct policy_handle h, s;
	struct SERVICE_STATUS service_status;
	NTSTATUS status;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.handle = &s;
	r.out.service_status = &service_status;

	status = dcerpc_svcctl_QueryServiceStatus(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "QueryServiceStatus failed!");
	torture_assert_werr_ok(tctx, r.out.result, "QueryServiceStatus failed!");

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_QueryServiceStatusEx(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct svcctl_QueryServiceStatusEx r;
	struct policy_handle h, s;
	NTSTATUS status;

	uint32_t info_level = SVC_STATUS_PROCESS_INFO;
	uint8_t *buffer;
	uint32_t offered = 0;
	uint32_t needed = 0;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	buffer = talloc(tctx, uint8_t);

	r.in.handle = &s;
	r.in.info_level = info_level;
	r.in.offered = offered;
	r.out.buffer = buffer;
	r.out.needed = &needed;

	status = dcerpc_svcctl_QueryServiceStatusEx(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "QueryServiceStatusEx failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		r.in.offered = needed;
		buffer = talloc_array(tctx, uint8_t, needed);
		r.out.buffer = buffer;

		status = dcerpc_svcctl_QueryServiceStatusEx(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "QueryServiceStatusEx failed!");
		torture_assert_werr_ok(tctx, r.out.result, "QueryServiceStatusEx failed!");
	}

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_QueryServiceConfigW(struct torture_context *tctx,
				     struct dcerpc_pipe *p)
{
	struct svcctl_QueryServiceConfigW r;
	struct QUERY_SERVICE_CONFIG query;
	struct policy_handle h, s;
	NTSTATUS status;

	uint32_t offered = 0;
	uint32_t needed = 0;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.handle = &s;
	r.in.offered = offered;
	r.out.query = &query;
	r.out.needed = &needed;

	status = dcerpc_svcctl_QueryServiceConfigW(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfigW failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		r.in.offered = needed;
		status = dcerpc_svcctl_QueryServiceConfigW(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfigW failed!");
	}

	torture_assert_werr_ok(tctx, r.out.result, "QueryServiceConfigW failed!");

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_QueryServiceConfig2W(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct svcctl_QueryServiceConfig2W r;
	struct policy_handle h, s;
	NTSTATUS status;

	uint32_t info_level = SERVICE_CONFIG_DESCRIPTION;
	uint8_t *buffer;
	uint32_t offered = 0;
	uint32_t needed = 0;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	buffer = talloc(tctx, uint8_t);

	r.in.handle = &s;
	r.in.info_level = info_level;
	r.in.offered = offered;
	r.out.buffer = buffer;
	r.out.needed = &needed;

	status = dcerpc_svcctl_QueryServiceConfig2W(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfig2W failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		r.in.offered = needed;
		buffer = talloc_array(tctx, uint8_t, needed);
		r.out.buffer = buffer;

		status = dcerpc_svcctl_QueryServiceConfig2W(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfig2W failed!");
		torture_assert_werr_ok(tctx, r.out.result, "QueryServiceConfig2W failed!");
	}

	r.in.info_level = SERVICE_CONFIG_FAILURE_ACTIONS;
	r.in.offered = offered;
	r.out.buffer = buffer;
	r.out.needed = &needed;

	status = dcerpc_svcctl_QueryServiceConfig2W(p, tctx, &r);
	torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfig2W failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		r.in.offered = needed;
		buffer = talloc_array(tctx, uint8_t, needed);
		r.out.buffer = buffer;

		status = dcerpc_svcctl_QueryServiceConfig2W(p, tctx, &r);
		torture_assert_ntstatus_ok(tctx, status, "QueryServiceConfig2W failed!");
		torture_assert_werr_ok(tctx, r.out.result, "QueryServiceConfig2W failed!");
	}

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_QueryServiceObjectSecurity(struct torture_context *tctx,
					    struct dcerpc_pipe *p)
{
	struct svcctl_QueryServiceObjectSecurity r;
	struct policy_handle h, s;

	uint8_t *buffer;
	uint32_t needed;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.handle = &s;
	r.in.security_flags = 0;
	r.in.offered = 0;
	r.out.buffer = NULL;
	r.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_svcctl_QueryServiceObjectSecurity(p, tctx, &r),
		"QueryServiceObjectSecurity failed!");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"QueryServiceObjectSecurity failed!");

	r.in.security_flags = SECINFO_DACL;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_svcctl_QueryServiceObjectSecurity(p, tctx, &r),
		"QueryServiceObjectSecurity failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		r.in.offered = needed;
		buffer = talloc_array(tctx, uint8_t, needed);
		r.out.buffer = buffer;
		torture_assert_ntstatus_ok(tctx,
			dcerpc_svcctl_QueryServiceObjectSecurity(p, tctx, &r),
			"QueryServiceObjectSecurity failed!");
	}

	torture_assert_werr_ok(tctx, r.out.result, "QueryServiceObjectSecurity failed!");

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_StartServiceW(struct torture_context *tctx,
			       struct dcerpc_pipe *p)
{
	struct svcctl_StartServiceW r;
	struct policy_handle h, s;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.handle = &s;
	r.in.NumArgs = 0;
	r.in.Arguments = NULL;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_svcctl_StartServiceW(p, tctx, &r),
		"StartServiceW failed!");
	torture_assert_werr_equal(tctx, r.out.result,
		WERR_SERVICE_ALREADY_RUNNING,
		"StartServiceW failed!");

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_ControlService(struct torture_context *tctx,
				struct dcerpc_pipe *p)
{
	struct svcctl_ControlService r;
	struct policy_handle h, s;
	struct SERVICE_STATUS service_status;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.handle = &s;
	r.in.control = 0;
	r.out.service_status = &service_status;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_svcctl_ControlService(p, tctx, &r),
		"ControlService failed!");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"ControlService failed!");

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_EnumServicesStatus(struct torture_context *tctx, struct dcerpc_pipe *p)
{
	struct svcctl_EnumServicesStatusW r;
	struct policy_handle h;
	int i;
	NTSTATUS status;
	uint32_t resume_handle = 0;
	struct ENUM_SERVICE_STATUSW *service = NULL;
	uint32_t needed = 0;
	uint32_t services_returned = 0;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	r.in.handle = &h;
	r.in.type = SERVICE_TYPE_WIN32;
	r.in.state = SERVICE_STATE_ALL;
	r.in.offered = 0;
	r.in.resume_handle = &resume_handle;
	r.out.service = NULL;
	r.out.resume_handle = &resume_handle;
	r.out.services_returned = &services_returned;
	r.out.needed = &needed;

	status = dcerpc_svcctl_EnumServicesStatusW(p, tctx, &r);

	torture_assert_ntstatus_ok(tctx, status, "EnumServicesStatus failed!");

	if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
		r.in.offered = needed;
		r.out.service = talloc_array(tctx, uint8_t, needed);

		status = dcerpc_svcctl_EnumServicesStatusW(p, tctx, &r);

		torture_assert_ntstatus_ok(tctx, status, "EnumServicesStatus failed!");
		torture_assert_werr_ok(tctx, r.out.result, "EnumServicesStatus failed");
	}

	if (services_returned > 0) {

		enum ndr_err_code ndr_err;
		DATA_BLOB blob;
		struct ndr_pull *ndr;

		blob.length = r.in.offered;
		blob.data = talloc_steal(tctx, r.out.service);

		ndr = ndr_pull_init_blob(&blob, tctx, lp_iconv_convenience(tctx->lp_ctx));

		service = talloc_array(tctx, struct ENUM_SERVICE_STATUSW, services_returned);
		if (!service) {
			return false;
		}

		ndr_err = ndr_pull_ENUM_SERVICE_STATUSW_array(
				ndr, services_returned, service);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return false;
		}
	}

	for(i = 0; i < services_returned; i++) {

		torture_assert(tctx, service[i].service_name,
			"Service without name returned!");

		printf("%-20s   \"%s\", Type: %d, State: %d\n",
			service[i].service_name, service[i].display_name,
			service[i].status.type, service[i].status.state);
	}

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_EnumDependentServicesW(struct torture_context *tctx,
					struct dcerpc_pipe *p)
{
	struct svcctl_EnumDependentServicesW r;
	struct policy_handle h, s;
	uint32_t needed;
	uint32_t services_returned;
	uint32_t i;
	uint32_t states[] = { SERVICE_STATE_ACTIVE,
			      SERVICE_STATE_INACTIVE,
			      SERVICE_STATE_ALL };

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_OpenService(p, tctx, &h, TORTURE_DEFAULT_SERVICE, &s))
		return false;

	r.in.service = &s;
	r.in.offered = 0;
	r.in.state = 0;
	r.out.service_status = NULL;
	r.out.services_returned = &services_returned;
	r.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_svcctl_EnumDependentServicesW(p, tctx, &r),
		"EnumDependentServicesW failed!");

	torture_assert_werr_equal(tctx, r.out.result, WERR_INVALID_PARAM,
		"EnumDependentServicesW failed!");

	for (i=0; i<ARRAY_SIZE(states); i++) {

		r.in.state = states[i];

		torture_assert_ntstatus_ok(tctx,
			dcerpc_svcctl_EnumDependentServicesW(p, tctx, &r),
			"EnumDependentServicesW failed!");

		if (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA)) {
			r.in.offered = needed;
			r.out.service_status = talloc_array(tctx, uint8_t, needed);

			torture_assert_ntstatus_ok(tctx,
				dcerpc_svcctl_EnumDependentServicesW(p, tctx, &r),
				"EnumDependentServicesW failed!");

		}

		torture_assert_werr_ok(tctx, r.out.result,
			"EnumDependentServicesW failed");
	}

	if (!test_CloseServiceHandle(p, tctx, &s))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

static bool test_SCManager(struct torture_context *tctx,
						   struct dcerpc_pipe *p)
{
	struct policy_handle h;

	if (!test_OpenSCManager(p, tctx, &h))
		return false;

	if (!test_CloseServiceHandle(p, tctx, &h))
		return false;

	return true;
}

struct torture_suite *torture_rpc_svcctl(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "SVCCTL");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "svcctl", &ndr_table_svcctl);

	torture_rpc_tcase_add_test(tcase, "SCManager",
				   test_SCManager);
	torture_rpc_tcase_add_test(tcase, "EnumServicesStatus",
				   test_EnumServicesStatus);
	torture_rpc_tcase_add_test(tcase, "EnumDependentServicesW",
				   test_EnumDependentServicesW);
	torture_rpc_tcase_add_test(tcase, "QueryServiceStatus",
				   test_QueryServiceStatus);
	torture_rpc_tcase_add_test(tcase, "QueryServiceStatusEx",
				   test_QueryServiceStatusEx);
	torture_rpc_tcase_add_test(tcase, "QueryServiceConfigW",
				   test_QueryServiceConfigW);
	torture_rpc_tcase_add_test(tcase, "QueryServiceConfig2W",
				   test_QueryServiceConfig2W);
	torture_rpc_tcase_add_test(tcase, "QueryServiceObjectSecurity",
				   test_QueryServiceObjectSecurity);
	torture_rpc_tcase_add_test(tcase, "StartServiceW",
				   test_StartServiceW);
	torture_rpc_tcase_add_test(tcase, "ControlService",
				   test_ControlService);

	return suite;
}
