/* 
   Unix SMB/CIFS mplementation.

   test CLDAP operations
   
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Matthias Dieter Wallnöfer 2009
    
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
#include "libcli/cldap/cldap.h"
#include "libcli/ldap/libcli_ldap.h"
#include "librpc/gen_ndr/netlogon.h"
#include "torture/torture.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"

#define CHECK_STATUS(status, correct) torture_assert_ntstatus_equal(tctx, status, correct, "incorrect status")

#define CHECK_VAL(v, correct) torture_assert_int_equal(tctx, (v), (correct), "incorrect value");

#define CHECK_STRING(v, correct) torture_assert_str_equal(tctx, v, correct, "incorrect value");
/*
  test netlogon operations
*/
static bool test_cldap_netlogon(struct torture_context *tctx, const char *dest)
{
	struct cldap_socket *cldap;
	NTSTATUS status;
	struct cldap_netlogon search, empty_search;
	struct netlogon_samlogon_response n1;
	struct GUID guid;
	int i;
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						dest,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	status = cldap_socket_init(tctx, NULL, NULL, dest_addr, &cldap);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(search);
	search.in.dest_address = NULL;
	search.in.dest_port = 0;
	search.in.acct_control = -1;
	search.in.version = NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	search.in.map_response = true;

	empty_search = search;

	printf("Trying without any attributes\n");
	search = empty_search;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	n1 = search.out.netlogon;

	search.in.user         = "Administrator";
	search.in.realm        = n1.data.nt5_ex.dns_domain;
	search.in.host         = "__cldap_torture__";

	printf("Scanning for netlogon levels\n");
	for (i=0;i<256;i++) {
		search.in.version = i;
		printf("Trying netlogon level %d\n", i);
		status = cldap_netlogon(cldap, tctx, &search);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	printf("Scanning for netlogon level bits\n");
	for (i=0;i<31;i++) {
		search.in.version = (1<<i);
		printf("Trying netlogon level 0x%x\n", i);
		status = cldap_netlogon(cldap, tctx, &search);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	search.in.version = NETLOGON_NT_VERSION_5|NETLOGON_NT_VERSION_5EX|NETLOGON_NT_VERSION_IP;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Trying with User=NULL\n");
	search.in.user = NULL;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");

	printf("Trying with User=Administrator\n");
	search.in.user = "Administrator";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, search.in.user);

	search.in.version = NETLOGON_NT_VERSION_5;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Trying with User=NULL\n");
	search.in.user = NULL;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");

	printf("Trying with User=Administrator\n");
	search.in.user = "Administrator";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, search.in.user);

	search.in.version = NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;

	printf("Trying with a GUID\n");
	search.in.realm       = NULL;
	search.in.domain_guid = GUID_string(tctx, &n1.data.nt5_ex.domain_uuid);
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN_EX);
	CHECK_STRING(GUID_string(tctx, &search.out.netlogon.data.nt5_ex.domain_uuid), search.in.domain_guid);

	printf("Trying with a incorrect GUID\n");
	guid = GUID_random();
	search.in.user        = NULL;
	search.in.domain_guid = GUID_string(tctx, &guid);
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_NOT_FOUND);

	printf("Trying with a AAC\n");
	search.in.acct_control = ACB_WSTRUST|ACB_SVRTRUST;
	search.in.realm = n1.data.nt5_ex.dns_domain;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");

	printf("Trying with a zero AAC\n");
	search.in.acct_control = 0x0;
	search.in.realm = n1.data.nt5_ex.dns_domain;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");

	printf("Trying with a zero AAC and user=Administrator\n");
	search.in.acct_control = 0x0;
	search.in.user = "Administrator";
	search.in.realm = n1.data.nt5_ex.dns_domain;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "Administrator");

	printf("Trying with a bad AAC\n");
	search.in.user = NULL;
	search.in.acct_control = 0xFF00FF00;
	search.in.realm = n1.data.nt5_ex.dns_domain;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");

	printf("Trying with a user only\n");
	search = empty_search;
	search.in.user = "Administrator";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, search.in.user);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	printf("Trying with just a bad username\n");
	search.in.user = "___no_such_user___";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_USER_UNKNOWN_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, search.in.user);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	printf("Trying with just a bad domain\n");
	search = empty_search;
	search.in.realm = "___no_such_domain___";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_NOT_FOUND);

	printf("Trying with a incorrect domain and correct guid\n");
	search.in.domain_guid = GUID_string(tctx, &n1.data.nt5_ex.domain_uuid);
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	printf("Trying with a incorrect domain and incorrect guid\n");
	search.in.domain_guid = GUID_string(tctx, &guid);
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_NOT_FOUND);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	printf("Trying with a incorrect GUID and correct domain\n");
	search.in.domain_guid = GUID_string(tctx, &guid);
	search.in.realm = n1.data.nt5_ex.dns_domain;
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(search.out.netlogon.data.nt5_ex.command, LOGON_SAM_LOGON_RESPONSE_EX);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, "");
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	printf("Proof other results\n");
	search.in.user = "Administrator";
	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.forest, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.dns_domain, n1.data.nt5_ex.dns_domain);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.domain_name, n1.data.nt5_ex.domain_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.pdc_name, n1.data.nt5_ex.pdc_name);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.user_name, search.in.user);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.server_site, n1.data.nt5_ex.server_site);
	CHECK_STRING(search.out.netlogon.data.nt5_ex.client_site, n1.data.nt5_ex.client_site);

	return true;
}

/*
  test cldap netlogon server type flags
*/
static bool test_cldap_netlogon_flags(struct torture_context *tctx,
	const char *dest)
{
	struct cldap_socket *cldap;
	NTSTATUS status;
	struct cldap_netlogon search;
	struct netlogon_samlogon_response n1;
	uint32_t server_type;
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						dest,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	/* cldap_socket_init should now know about the dest. address */
	status = cldap_socket_init(tctx, NULL, NULL, dest_addr, &cldap);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Printing out netlogon server type flags: %s\n", dest);

	ZERO_STRUCT(search);
	search.in.dest_address = NULL;
	search.in.dest_port = 0;
	search.in.acct_control = -1;
	search.in.version = NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	search.in.map_response = true;

	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	n1 = search.out.netlogon;
	if (n1.ntver == NETLOGON_NT_VERSION_5)
		server_type = n1.data.nt5.server_type;
	else if (n1.ntver == NETLOGON_NT_VERSION_5EX)
		server_type = n1.data.nt5_ex.server_type;	

	printf("The word is: %i\n", server_type);
	if (server_type & NBT_SERVER_PDC)
		printf("NBT_SERVER_PDC ");
	if (server_type & NBT_SERVER_GC)
		printf("NBT_SERVER_GC ");
	if (server_type & NBT_SERVER_LDAP)
		printf("NBT_SERVER_LDAP ");
	if (server_type & NBT_SERVER_DS)
		printf("NBT_SERVER_DS ");
	if (server_type & NBT_SERVER_KDC)
		printf("NBT_SERVER_KDC ");
	if (server_type & NBT_SERVER_TIMESERV)
		printf("NBT_SERVER_TIMESERV ");
	if (server_type & NBT_SERVER_CLOSEST)
		printf("NBT_SERVER_CLOSEST ");
	if (server_type & NBT_SERVER_WRITABLE)
		printf("NBT_SERVER_WRITABLE ");
	if (server_type & NBT_SERVER_GOOD_TIMESERV)
		printf("NBT_SERVER_GOOD_TIMESERV ");
	if (server_type & NBT_SERVER_NDNC)
		printf("NBT_SERVER_NDNC ");
	if (server_type & NBT_SERVER_SELECT_SECRET_DOMAIN_6)
		printf("NBT_SERVER_SELECT_SECRET_DOMAIN_6");
	if (server_type & NBT_SERVER_FULL_SECRET_DOMAIN_6)
		printf("NBT_SERVER_FULL_SECRET_DOMAIN_6");
	if (server_type & DS_DNS_CONTROLLER)
		printf("DS_DNS_CONTROLLER ");
	if (server_type & DS_DNS_DOMAIN)
		printf("DS_DNS_DOMAIN ");
	if (server_type & DS_DNS_FOREST_ROOT)
		printf("DS_DNS_FOREST_ROOT ");

	printf("\n");

	return true;
}

/*
  convert a ldap result message to a ldb message. This allows us to
  use the convenient ldif dump routines in ldb to print out cldap
  search results
*/
static struct ldb_message *ldap_msg_to_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, struct ldap_SearchResEntry *res)
{
	struct ldb_message *msg;

	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_new(msg, ldb, res->dn);
	msg->num_elements = res->num_attributes;
	msg->elements = talloc_steal(msg, res->attributes);
	return msg;
}

/*
  dump a set of cldap results
*/
static void cldap_dump_results(struct cldap_search *search)
{
	struct ldb_ldif ldif;
	struct ldb_context *ldb;

	if (!search || !(search->out.response)) {
		return;
	}

	/* we need a ldb context to use ldb_ldif_write_file() */
	ldb = ldb_init(NULL, NULL);

	ZERO_STRUCT(ldif);
	ldif.msg = ldap_msg_to_ldb(ldb, ldb, search->out.response);

	ldb_ldif_write_file(ldb, stdout, &ldif);

	talloc_free(ldb);
}


/*
  test cldap netlogon server type flag "NBT_SERVER_FOREST_ROOT"
*/
static bool test_cldap_netlogon_flag_ds_dns_forest(struct torture_context *tctx,
	const char *dest)
{
	struct cldap_socket *cldap;
	NTSTATUS status;
	struct cldap_netlogon search;
	uint32_t server_type;
	struct netlogon_samlogon_response n1;
	bool result = true;
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						dest,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	/* cldap_socket_init should now know about the dest. address */
	status = cldap_socket_init(tctx, NULL, NULL, dest_addr, &cldap);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Testing netlogon server type flag NBT_SERVER_FOREST_ROOT: ");

	ZERO_STRUCT(search);
	search.in.dest_address = NULL;
	search.in.dest_port = 0;
	search.in.acct_control = -1;
	search.in.version = NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	search.in.map_response = true;

	status = cldap_netlogon(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	n1 = search.out.netlogon;
	if (n1.ntver == NETLOGON_NT_VERSION_5)
		server_type = n1.data.nt5.server_type;
	else if (n1.ntver == NETLOGON_NT_VERSION_5EX)
		server_type = n1.data.nt5_ex.server_type;

	if (server_type & DS_DNS_FOREST_ROOT) {
		struct cldap_search search2;
		const char *attrs[] = { "defaultNamingContext", "rootDomainNamingContext", 
			NULL };
		struct ldb_context *ldb;
		struct ldb_message *msg;

		/* Trying to fetch the attributes "defaultNamingContext" and
		   "rootDomainNamingContext" */
		ZERO_STRUCT(search2);
		search2.in.dest_address = dest;
		search2.in.dest_port = lpcfg_cldap_port(tctx->lp_ctx);
		search2.in.timeout = 10;
		search2.in.retries = 3;
		search2.in.filter = "(objectclass=*)";
		search2.in.attributes = attrs;

		status = cldap_search(cldap, tctx, &search2);
		CHECK_STATUS(status, NT_STATUS_OK);

		ldb = ldb_init(NULL, NULL);

		msg = ldap_msg_to_ldb(ldb, ldb, search2.out.response);

		/* Try to compare the two attributes */
		if (ldb_msg_element_compare(ldb_msg_find_element(msg, attrs[0]),
			ldb_msg_find_element(msg, attrs[1])))
			result = false;

		talloc_free(ldb);
	}

	if (result)
		printf("passed\n");
	else
		printf("failed\n");

	return result;
}

/*
  test generic cldap operations
*/
static bool test_cldap_generic(struct torture_context *tctx, const char *dest)
{
	struct cldap_socket *cldap;
	NTSTATUS status;
	struct cldap_search search;
	const char *attrs1[] = { "currentTime", "highestCommittedUSN", NULL };
	const char *attrs2[] = { "currentTime", "highestCommittedUSN", "netlogon", NULL };
	const char *attrs3[] = { "netlogon", NULL };
	struct tsocket_address *dest_addr;
	int ret;

	ret = tsocket_address_inet_from_strings(tctx, "ip",
						dest,
						lpcfg_cldap_port(tctx->lp_ctx),
						&dest_addr);
	CHECK_VAL(ret, 0);

	/* cldap_socket_init should now know about the dest. address */
	status = cldap_socket_init(tctx, NULL, NULL, dest_addr, &cldap);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(search);
	search.in.dest_address = NULL;
	search.in.dest_port = 0;
	search.in.timeout = 10;
	search.in.retries = 3;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("fetching whole rootDSE\n");
	search.in.filter = "(objectclass=*)";
	search.in.attributes = NULL;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (DEBUGLVL(3)) cldap_dump_results(&search);

	printf("fetching currentTime and USN\n");
	search.in.filter = "(objectclass=*)";
	search.in.attributes = attrs1;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	if (DEBUGLVL(3)) cldap_dump_results(&search);

	printf("Testing currentTime, USN and netlogon\n");
	search.in.filter = "(objectclass=*)";
	search.in.attributes = attrs2;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (DEBUGLVL(3)) cldap_dump_results(&search);

	printf("Testing objectClass=* and netlogon\n");
	search.in.filter = "(objectclass2=*)";
	search.in.attributes = attrs3;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (DEBUGLVL(3)) cldap_dump_results(&search);

	printf("Testing a false expression\n");
	search.in.filter = "(&(objectclass=*)(highestCommittedUSN=2))";
	search.in.attributes = attrs1;

	status = cldap_search(cldap, tctx, &search);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (DEBUGLVL(3)) cldap_dump_results(&search);	

	return true;	
}

bool torture_cldap(struct torture_context *torture)
{
	bool ret = true;
	const char *host = torture_setting_string(torture, "host", NULL);

	ret &= test_cldap_netlogon(torture, host);
	ret &= test_cldap_netlogon_flags(torture, host);
	ret &= test_cldap_netlogon_flag_ds_dns_forest(torture, host);
	ret &= test_cldap_generic(torture, host);

	return ret;
}

