/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Guenther Deschner 2009

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
#include "nsswitch/libwbclient/wbclient.h"
#include "torture/smbtorture.h"
#include "torture/winbind/proto.h"

#define WBC_ERROR_EQUAL(x,y) (x == y)

#define torture_assert_wbc_equal(torture_ctx, got, expected, cmt) \
	do { wbcErr __got = got, __expected = expected; \
	if (!WBC_ERROR_EQUAL(__got, __expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", wbcErrorString(__got), wbcErrorString(__expected), cmt); \
		return false; \
	} \
	} while (0)

#define torture_assert_wbc_ok(torture_ctx,expr,cmt) \
		torture_assert_wbc_equal(torture_ctx,expr,WBC_ERR_SUCCESS,cmt)

static bool test_wbc_ping(struct torture_context *tctx)
{
	torture_assert_wbc_ok(tctx, wbcPing(),
		"wbcPing failed");

	return true;
}

static bool test_wbc_library_details(struct torture_context *tctx)
{
	struct wbcLibraryDetails *details;

	torture_assert_wbc_ok(tctx, wbcLibraryDetails(&details),
		"wbcLibraryDetails failed");
	torture_assert(tctx, details,
		"wbcLibraryDetails returned NULL pointer");

	wbcFreeMemory(details);

	return true;
}

static bool test_wbc_interface_details(struct torture_context *tctx)
{
	struct wbcInterfaceDetails *details;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");
	torture_assert(tctx, details,
		"wbcInterfaceDetails returned NULL pointer");

	wbcFreeMemory(details);

	return true;
}

static bool test_wbc_sidtypestring(struct torture_context *tctx)
{
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_USE_NONE),
				 "SID_NONE", "SID_NONE failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_USER),
				 "SID_USER", "SID_USER failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_DOM_GRP),
				 "SID_DOM_GROUP", "SID_DOM_GROUP failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_DOMAIN),
				 "SID_DOMAIN", "SID_DOMAIN failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_ALIAS),
				 "SID_ALIAS", "SID_ALIAS failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_WKN_GRP),
				 "SID_WKN_GROUP", "SID_WKN_GROUP failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_DELETED),
				 "SID_DELETED", "SID_DELETED failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_INVALID),
				 "SID_INVALID", "SID_INVALID failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_UNKNOWN),
				 "SID_UNKNOWN", "SID_UNKNOWN failed");
	torture_assert_str_equal(tctx, wbcSidTypeString(WBC_SID_NAME_COMPUTER),
				 "SID_COMPUTER", "SID_COMPUTER failed");
	return true;
}

static bool test_wbc_sidtostring(struct torture_context *tctx)
{
	struct wbcDomainSid sid;
	const char *sid_string = "S-1-5-32";
	char *sid_string2;

	torture_assert_wbc_ok(tctx, wbcStringToSid(sid_string, &sid),
		"wbcStringToSid failed");
	torture_assert_wbc_ok(tctx, wbcSidToString(&sid, &sid_string2),
		"wbcSidToString failed");
	torture_assert_str_equal(tctx, sid_string, sid_string2,
		"sid strings differ");

	return true;
}

static bool test_wbc_guidtostring(struct torture_context *tctx)
{
	struct wbcGuid guid;
	const char *guid_string = "f7cf07b4-1487-45c7-824d-8b18cc580811";
	char *guid_string2;

	torture_assert_wbc_ok(tctx, wbcStringToGuid(guid_string, &guid),
		"wbcStringToGuid failed");
	torture_assert_wbc_ok(tctx, wbcGuidToString(&guid, &guid_string2),
		"wbcGuidToString failed");
	torture_assert_str_equal(tctx, guid_string, guid_string2,
		"guid strings differ");

	return true;
}

static bool test_wbc_domain_info(struct torture_context *tctx)
{
	const char *domain_name = NULL;
	struct wbcDomainInfo *info;
	struct wbcInterfaceDetails *details;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");

	domain_name = talloc_strdup(tctx, details->netbios_domain);
	wbcFreeMemory(details);

	torture_assert_wbc_ok(tctx, wbcDomainInfo(domain_name, &info),
		"wbcDomainInfo failed");
	torture_assert(tctx, info,
		"wbcDomainInfo returned NULL pointer");

	return true;
}

static bool test_wbc_users(struct torture_context *tctx)
{
	const char *domain_name = NULL;
	uint32_t num_users;
	const char **users;
	int i;
	struct wbcInterfaceDetails *details;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");

	domain_name = talloc_strdup(tctx, details->netbios_domain);
	wbcFreeMemory(details);

	torture_assert_wbc_ok(tctx, wbcListUsers(domain_name, &num_users, &users),
		"wbcListUsers failed");
	torture_assert(tctx, !(num_users > 0 && !users),
		"wbcListUsers returned invalid results");

	for (i=0; i < MIN(num_users,100); i++) {

		struct wbcDomainSid sid, *sids;
		enum wbcSidType name_type;
		char *domain;
		char *name;
		uint32_t num_sids;

		torture_assert_wbc_ok(tctx, wbcLookupName(domain_name, users[i], &sid, &name_type),
			"wbcLookupName failed");
		torture_assert_int_equal(tctx, name_type, WBC_SID_NAME_USER,
			"wbcLookupName expected WBC_SID_NAME_USER");
		torture_assert_wbc_ok(tctx, wbcLookupSid(&sid, &domain, &name, &name_type),
			"wbcLookupSid failed");
		torture_assert_int_equal(tctx, name_type, WBC_SID_NAME_USER,
			"wbcLookupSid expected WBC_SID_NAME_USER");
		torture_assert(tctx, name,
			"wbcLookupSid returned no name");
		torture_assert_wbc_ok(tctx, wbcLookupUserSids(&sid, true, &num_sids, &sids),
			"wbcLookupUserSids failed");
	}

	return true;
}

static bool test_wbc_groups(struct torture_context *tctx)
{
	const char *domain_name = NULL;
	uint32_t num_groups;
	const char **groups;
	int i;
	struct wbcInterfaceDetails *details;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");

	domain_name = talloc_strdup(tctx, details->netbios_domain);
	wbcFreeMemory(details);

	torture_assert_wbc_ok(tctx, wbcListGroups(domain_name, &num_groups, &groups),
		"wbcListGroups failed");
	torture_assert(tctx, !(num_groups > 0 && !groups),
		"wbcListGroups returned invalid results");

	for (i=0; i < MIN(num_groups,100); i++) {

		struct wbcDomainSid sid;
		enum wbcSidType name_type;
		char *domain;
		char *name;

		torture_assert_wbc_ok(tctx, wbcLookupName(domain_name, groups[i], &sid, &name_type),
			"wbcLookupName failed");
		torture_assert_wbc_ok(tctx, wbcLookupSid(&sid, &domain, &name, &name_type),
			"wbcLookupSid failed");
		torture_assert(tctx, name,
			"wbcLookupSid returned no name");
	}

	return true;
}

static bool test_wbc_trusts(struct torture_context *tctx)
{
	struct wbcDomainInfo *domains;
	size_t num_domains;
	int i;

	torture_assert_wbc_ok(tctx, wbcListTrusts(&domains, &num_domains),
		"wbcListTrusts failed");
	torture_assert(tctx, !(num_domains > 0 && !domains),
		"wbcListTrusts returned invalid results");

	for (i=0; i < MIN(num_domains,100); i++) {

		struct wbcAuthErrorInfo *error;
		/*
		struct wbcDomainSid sid;
		enum wbcSidType name_type;
		char *domain;
		char *name;
		*/
		torture_assert_wbc_ok(tctx, wbcCheckTrustCredentials(domains[i].short_name, &error),
			"wbcCheckTrustCredentials failed");
		/*
		torture_assert_wbc_ok(tctx, wbcLookupName(domains[i].short_name, NULL, &sid, &name_type),
			"wbcLookupName failed");
		torture_assert_int_equal(tctx, name_type, WBC_SID_NAME_DOMAIN,
			"wbcLookupName expected WBC_SID_NAME_DOMAIN");
		torture_assert_wbc_ok(tctx, wbcLookupSid(&sid, &domain, &name, &name_type),
			"wbcLookupSid failed");
		torture_assert_int_equal(tctx, name_type, WBC_SID_NAME_DOMAIN,
			"wbcLookupSid expected WBC_SID_NAME_DOMAIN");
		torture_assert(tctx, name,
			"wbcLookupSid returned no name");
		*/
	}

	return true;
}

static bool test_wbc_lookupdc(struct torture_context *tctx)
{
	const char *domain_name = NULL;
	struct wbcInterfaceDetails *details;
	struct wbcDomainControllerInfo *dc_info;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");

	domain_name = talloc_strdup(tctx, details->netbios_domain);
	wbcFreeMemory(details);

	torture_assert_wbc_ok(tctx, wbcLookupDomainController(domain_name, 0, &dc_info),
		"wbcLookupDomainController failed");

	return true;
}

static bool test_wbc_lookupdcex(struct torture_context *tctx)
{
	const char *domain_name = NULL;
	struct wbcInterfaceDetails *details;
	struct wbcDomainControllerInfoEx *dc_info;

	torture_assert_wbc_ok(tctx, wbcInterfaceDetails(&details),
		"wbcInterfaceDetails failed");

	domain_name = talloc_strdup(tctx, details->netbios_domain);
	wbcFreeMemory(details);

	torture_assert_wbc_ok(tctx, wbcLookupDomainControllerEx(domain_name, NULL, NULL, 0, &dc_info),
		"wbcLookupDomainControllerEx failed");

	return true;
}


struct torture_suite *torture_wbclient(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "WBCLIENT");

	torture_suite_add_simple_test(suite, "wbcPing", test_wbc_ping);
	torture_suite_add_simple_test(suite, "wbcLibraryDetails", test_wbc_library_details);
	torture_suite_add_simple_test(suite, "wbcInterfaceDetails", test_wbc_interface_details);
	torture_suite_add_simple_test(suite, "wbcSidTypeString", test_wbc_sidtypestring);
	torture_suite_add_simple_test(suite, "wbcSidToString", test_wbc_sidtostring);
	torture_suite_add_simple_test(suite, "wbcGuidToString", test_wbc_guidtostring);
	torture_suite_add_simple_test(suite, "wbcDomainInfo", test_wbc_domain_info);
	torture_suite_add_simple_test(suite, "wbcListUsers", test_wbc_users);
	torture_suite_add_simple_test(suite, "wbcListGroups", test_wbc_groups);
	torture_suite_add_simple_test(suite, "wbcListTrusts", test_wbc_trusts);
	torture_suite_add_simple_test(suite, "wbcLookupDomainController", test_wbc_lookupdc);
	torture_suite_add_simple_test(suite, "wbcLookupDomainControllerEx", test_wbc_lookupdcex);

	return suite;
}
