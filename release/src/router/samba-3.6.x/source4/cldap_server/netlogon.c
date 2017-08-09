/* 
   Unix SMB/CIFS implementation.

   CLDAP server - netlogon handling

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008
   
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
#include <ldb.h>
#include <ldb_errors.h>
#include "lib/events/events.h"
#include "smbd/service_task.h"
#include "cldap_server/cldap_server.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "libcli/ldap/ldap_ndr.h"
#include "libcli/security/security.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "ldb_wrap.h"
#include "system/network.h"
#include "lib/socket/netif.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "libds/common/flag_mapping.h"

/*
  fill in the cldap netlogon union for a given version
*/
NTSTATUS fill_netlogon_samlogon_response(struct ldb_context *sam_ctx,
					 TALLOC_CTX *mem_ctx,
					 const char *domain,
					 const char *netbios_domain,
					 struct dom_sid *domain_sid,
					 const char *domain_guid,
					 const char *user,
					 uint32_t acct_control,
					 const char *src_address,
					 uint32_t version,
					 struct loadparm_context *lp_ctx,
					 struct netlogon_samlogon_response *netlogon,
					 bool fill_on_blank_request)
{
	const char *dom_attrs[] = {"objectGUID", NULL};
	const char *none_attrs[] = {NULL};
	struct ldb_result *dom_res = NULL, *user_res = NULL;
	int ret;
	const char **services = lpcfg_server_services(lp_ctx);
	uint32_t server_type;
	const char *pdc_name;
	struct GUID domain_uuid;
	const char *dns_domain;
	const char *forest_domain;
	const char *pdc_dns_name;
	const char *flatname;
	const char *server_site;
	const char *client_site;
	const char *pdc_ip;
	struct ldb_dn *domain_dn = NULL;
	struct interface *ifaces;
	bool user_known, am_rodc;
	NTSTATUS status;

	/* the domain parameter could have an optional trailing "." */
	if (domain && domain[strlen(domain)-1] == '.') {
		domain = talloc_strndup(mem_ctx, domain, strlen(domain)-1);
		NT_STATUS_HAVE_NO_MEMORY(domain);
	}

	/* Lookup using long or short domainname */
	if (domain && (strcasecmp_m(domain, lpcfg_dnsdomain(lp_ctx)) == 0)) {
		domain_dn = ldb_get_default_basedn(sam_ctx);
	}
	if (netbios_domain && (strcasecmp_m(netbios_domain, lpcfg_sam_name(lp_ctx)) == 0)) {
		domain_dn = ldb_get_default_basedn(sam_ctx);
	}
	if (domain_dn) {
		const char *domain_identifier = domain != NULL ? domain
							: netbios_domain;
		ret = ldb_search(sam_ctx, mem_ctx, &dom_res,
				 domain_dn, LDB_SCOPE_BASE, dom_attrs,
				 "objectClass=domain");
		if (ret != LDB_SUCCESS) {
			DEBUG(2,("Error finding domain '%s'/'%s' in sam: %s\n",
				 domain_identifier,
				 ldb_dn_get_linearized(domain_dn),
				 ldb_errstring(sam_ctx)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
		if (dom_res->count != 1) {
			DEBUG(2,("Error finding domain '%s'/'%s' in sam\n",
				 domain_identifier,
				 ldb_dn_get_linearized(domain_dn)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
	}

	/* Lookup using GUID or SID */
	if ((dom_res == NULL) && (domain_guid || domain_sid)) {
		if (domain_guid) {
			struct GUID binary_guid;
			struct ldb_val guid_val;

			/* By this means, we ensure we don't have funny stuff in the GUID */

			status = GUID_from_string(domain_guid, &binary_guid);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}

			/* And this gets the result into the binary format we want anyway */
			status = GUID_to_ndr_blob(&binary_guid, mem_ctx, &guid_val);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
			ret = ldb_search(sam_ctx, mem_ctx, &dom_res,
						 NULL, LDB_SCOPE_SUBTREE, 
						 dom_attrs, 
						 "(&(objectCategory=DomainDNS)(objectGUID=%s))", 
						 ldb_binary_encode(mem_ctx, guid_val));
		} else { /* domain_sid case */
			struct dom_sid *sid;
			struct ldb_val sid_val;
			enum ndr_err_code ndr_err;
			
			/* Rather than go via the string, just push into the NDR form */
			ndr_err = ndr_push_struct_blob(&sid_val, mem_ctx, &sid,
						       (ndr_push_flags_fn_t)ndr_push_dom_sid);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			ret = ldb_search(sam_ctx, mem_ctx, &dom_res,
						 NULL, LDB_SCOPE_SUBTREE, 
						 dom_attrs, 
						 "(&(objectCategory=DomainDNS)(objectSid=%s))",
						 ldb_binary_encode(mem_ctx, sid_val));
		}
		
		if (ret != LDB_SUCCESS) {
			DEBUG(2,("Unable to find a correct reference to GUID '%s' or SID '%s' in sam: %s\n",
				 domain_guid, dom_sid_string(mem_ctx, domain_sid),
				 ldb_errstring(sam_ctx)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		} else if (dom_res->count == 1) {
			/* Ok, now just check it is our domain */
			if (ldb_dn_compare(ldb_get_default_basedn(sam_ctx),
					   dom_res->msgs[0]->dn) != 0) {
				DEBUG(2,("The GUID '%s' or SID '%s' doesn't identify our domain\n",
					 domain_guid,
					 dom_sid_string(mem_ctx, domain_sid)));
				return NT_STATUS_NO_SUCH_DOMAIN;
			}
		} else {
			DEBUG(2,("Unable to find a correct reference to GUID '%s' or SID '%s' in sam\n",
				 domain_guid, dom_sid_string(mem_ctx, domain_sid)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
	}

	if (dom_res == NULL && fill_on_blank_request) {
		/* blank inputs gives our domain - tested against
		   w2k8r2. Without this ADUC on Win7 won't start */
		domain_dn = ldb_get_default_basedn(sam_ctx);
		ret = ldb_search(sam_ctx, mem_ctx, &dom_res,
				 domain_dn, LDB_SCOPE_BASE, dom_attrs,
				 "objectClass=domain");
		if (ret != LDB_SUCCESS) {
			DEBUG(2,("Error finding domain '%s'/'%s' in sam: %s\n",
				 lpcfg_dnsdomain(lp_ctx),
				 ldb_dn_get_linearized(domain_dn),
				 ldb_errstring(sam_ctx)));
			return NT_STATUS_NO_SUCH_DOMAIN;
		}
	}

        if (dom_res == NULL) {
		DEBUG(2,(__location__ ": Unable to get domain information with no inputs\n"));
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	/* work around different inputs for not-specified users */
	if (!user) {
		user = "";
	}

	/* Enquire about any valid username with just a CLDAP packet -
	 * if kerberos didn't also do this, the security folks would
	 * scream... */
	if (user[0]) {							\
		/* Only allow some bits to be enquired:  [MS-ATDS] 7.3.3.2 */
		if (acct_control == (uint32_t)-1) {
			acct_control = 0;
		}
		acct_control = acct_control & (ACB_TEMPDUP | ACB_NORMAL | ACB_DOMTRUST | ACB_WSTRUST | ACB_SVRTRUST);

		/* We must exclude disabled accounts, but otherwise do the bitwise match the client asked for */
		ret = ldb_search(sam_ctx, mem_ctx, &user_res,
					 dom_res->msgs[0]->dn, LDB_SCOPE_SUBTREE, 
					 none_attrs, 
					 "(&(objectClass=user)(samAccountName=%s)"
					 "(!(userAccountControl:" LDB_OID_COMPARATOR_AND ":=%u))"
					 "(userAccountControl:" LDB_OID_COMPARATOR_OR ":=%u))", 
					 ldb_binary_encode_string(mem_ctx, user),
					 UF_ACCOUNTDISABLE, ds_acb2uf(acct_control));
		if (ret != LDB_SUCCESS) {
			DEBUG(2,("Unable to find reference to user '%s' with ACB 0x%8x under %s: %s\n",
				 user, acct_control, ldb_dn_get_linearized(dom_res->msgs[0]->dn),
				 ldb_errstring(sam_ctx)));
			return NT_STATUS_NO_SUCH_USER;
		} else if (user_res->count == 1) {
			user_known = true;
		} else {
			user_known = false;
		}

	} else {
		user_known = true;
	}
		
	server_type      = 
		DS_SERVER_DS | DS_SERVER_TIMESERV |
		DS_SERVER_CLOSEST |
		DS_SERVER_GOOD_TIMESERV;

#if 0
	/* w2k8-r2 as a DC does not claim these */
	server_type |= DS_DNS_CONTROLLER | DS_DNS_DOMAIN;
#endif

	if (samdb_is_pdc(sam_ctx)) {
		server_type |= DS_SERVER_PDC;
	}

	if (dsdb_functional_level(sam_ctx) >= DS_DOMAIN_FUNCTION_2008) {
		server_type |= DS_SERVER_FULL_SECRET_DOMAIN_6;
	}

	if (samdb_is_gc(sam_ctx)) {
		server_type |= DS_SERVER_GC;
	}

	if (str_list_check(services, "ldap")) {
		server_type |= DS_SERVER_LDAP;
	}

	if (str_list_check(services, "kdc")) {
		server_type |= DS_SERVER_KDC;
	}

	if (samdb_rodc(sam_ctx, &am_rodc) == LDB_SUCCESS && !am_rodc) {
		server_type |= DS_SERVER_WRITABLE;
	}

#if 0
	/* w2k8-r2 as a sole DC does not claim this */
	if (ldb_dn_compare(ldb_get_root_basedn(sam_ctx), ldb_get_default_basedn(sam_ctx)) == 0) {
		server_type |= DS_DNS_FOREST_ROOT;
	}
#endif

	pdc_name         = talloc_asprintf(mem_ctx, "\\\\%s",
					   lpcfg_netbios_name(lp_ctx));
	NT_STATUS_HAVE_NO_MEMORY(pdc_name);
	domain_uuid      = samdb_result_guid(dom_res->msgs[0], "objectGUID");
	dns_domain       = lpcfg_dnsdomain(lp_ctx);
	forest_domain    = samdb_forest_name(sam_ctx, mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(forest_domain);
	pdc_dns_name     = talloc_asprintf(mem_ctx, "%s.%s", 
					   strlower_talloc(mem_ctx, 
							   lpcfg_netbios_name(lp_ctx)),
					   dns_domain);
	NT_STATUS_HAVE_NO_MEMORY(pdc_dns_name);
	flatname         = lpcfg_workgroup(lp_ctx);
	server_site      = samdb_server_site_name(sam_ctx, mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(server_site);
	client_site      = samdb_client_site_name(sam_ctx, mem_ctx,
						  src_address, NULL);
	NT_STATUS_HAVE_NO_MEMORY(client_site);
	load_interfaces(mem_ctx, lpcfg_interfaces(lp_ctx), &ifaces);
	/*
	 * TODO: the caller should pass the address which the client
	 * used to trigger this call, as the client is able to reach
	 * this ip.
	 */
	if (src_address) {
		pdc_ip = iface_best_ip(ifaces, src_address);
	} else {
		pdc_ip = iface_n_ip(ifaces, 0);
	}
	ZERO_STRUCTP(netlogon);

	/* check if either of these bits is present */
	if (version & (NETLOGON_NT_VERSION_5EX|NETLOGON_NT_VERSION_5EX_WITH_IP)) {
		uint32_t extra_flags = 0;
		netlogon->ntver = NETLOGON_NT_VERSION_5EX;

		/* could check if the user exists */
		if (user_known) {
			netlogon->data.nt5_ex.command      = LOGON_SAM_LOGON_RESPONSE_EX;
		} else {
			netlogon->data.nt5_ex.command      = LOGON_SAM_LOGON_USER_UNKNOWN_EX;
		}
		netlogon->data.nt5_ex.pdc_name     = pdc_name;
		netlogon->data.nt5_ex.user_name    = user;
		netlogon->data.nt5_ex.domain_name  = flatname;
		netlogon->data.nt5_ex.domain_uuid  = domain_uuid;
		netlogon->data.nt5_ex.forest       = forest_domain;
		netlogon->data.nt5_ex.dns_domain   = dns_domain;
		netlogon->data.nt5_ex.pdc_dns_name = pdc_dns_name;
		netlogon->data.nt5_ex.server_site  = server_site;
		netlogon->data.nt5_ex.client_site  = client_site;
		if (version & NETLOGON_NT_VERSION_5EX_WITH_IP) {
			/* Clearly this needs to be fixed up for IPv6 */
			extra_flags = NETLOGON_NT_VERSION_5EX_WITH_IP;
			netlogon->data.nt5_ex.sockaddr.sockaddr_family    = 2;
			netlogon->data.nt5_ex.sockaddr.pdc_ip       = pdc_ip;
			netlogon->data.nt5_ex.sockaddr.remaining = data_blob_talloc_zero(mem_ctx, 8);
		}
		netlogon->data.nt5_ex.server_type  = server_type;
		netlogon->data.nt5_ex.nt_version   = NETLOGON_NT_VERSION_1|NETLOGON_NT_VERSION_5EX|extra_flags;
		netlogon->data.nt5_ex.lmnt_token   = 0xFFFF;
		netlogon->data.nt5_ex.lm20_token   = 0xFFFF;

	} else if (version & NETLOGON_NT_VERSION_5) {
		netlogon->ntver = NETLOGON_NT_VERSION_5;

		/* could check if the user exists */
		if (user_known) {
			netlogon->data.nt5.command      = LOGON_SAM_LOGON_RESPONSE;
		} else {
			netlogon->data.nt5.command      = LOGON_SAM_LOGON_USER_UNKNOWN;
		}
		netlogon->data.nt5.pdc_name     = pdc_name;
		netlogon->data.nt5.user_name    = user;
		netlogon->data.nt5.domain_name  = flatname;
		netlogon->data.nt5.domain_uuid  = domain_uuid;
		netlogon->data.nt5.forest       = forest_domain;
		netlogon->data.nt5.dns_domain   = dns_domain;
		netlogon->data.nt5.pdc_dns_name = pdc_dns_name;
		netlogon->data.nt5.pdc_ip       = pdc_ip;
		netlogon->data.nt5.server_type  = server_type;
		netlogon->data.nt5.nt_version   = NETLOGON_NT_VERSION_1|NETLOGON_NT_VERSION_5;
		netlogon->data.nt5.lmnt_token   = 0xFFFF;
		netlogon->data.nt5.lm20_token   = 0xFFFF;

	} else /* (version & NETLOGON_NT_VERSION_1) and all other cases */ {
		netlogon->ntver = NETLOGON_NT_VERSION_1;
		/* could check if the user exists */
		if (user_known) {
			netlogon->data.nt4.command      = LOGON_SAM_LOGON_RESPONSE;
		} else {
			netlogon->data.nt4.command      = LOGON_SAM_LOGON_USER_UNKNOWN;
		}
		netlogon->data.nt4.pdc_name    = pdc_name;
		netlogon->data.nt4.user_name   = user;
		netlogon->data.nt4.domain_name = flatname;
		netlogon->data.nt4.nt_version  = NETLOGON_NT_VERSION_1;
		netlogon->data.nt4.lmnt_token  = 0xFFFF;
		netlogon->data.nt4.lm20_token  = 0xFFFF;
	}

	return NT_STATUS_OK;
}


/*
  handle incoming cldap requests
*/
void cldapd_netlogon_request(struct cldap_socket *cldap,
			     struct cldapd_server *cldapd,
			     TALLOC_CTX *tmp_ctx,
			     uint32_t message_id,
			     struct ldb_parse_tree *tree,
			     struct tsocket_address *src)
{
	unsigned int i;
	const char *domain = NULL;
	const char *host = NULL;
	const char *user = NULL;
	const char *domain_guid = NULL;
	struct dom_sid *domain_sid = NULL;
	int acct_control = -1;
	int version = -1;
	struct netlogon_samlogon_response netlogon;
	NTSTATUS status = NT_STATUS_INVALID_PARAMETER;

	if (tree->operation != LDB_OP_AND) goto failed;

	/* extract the query elements */
	for (i=0;i<tree->u.list.num_elements;i++) {
		struct ldb_parse_tree *t = tree->u.list.elements[i];
		if (t->operation != LDB_OP_EQUALITY) goto failed;
		if (strcasecmp(t->u.equality.attr, "DnsDomain") == 0) {
			domain = talloc_strndup(tmp_ctx, 
						(const char *)t->u.equality.value.data,
						t->u.equality.value.length);
		}
		if (strcasecmp(t->u.equality.attr, "Host") == 0) {
			host = talloc_strndup(tmp_ctx, 
					      (const char *)t->u.equality.value.data,
					      t->u.equality.value.length);
		}
		if (strcasecmp(t->u.equality.attr, "DomainGuid") == 0) {
			NTSTATUS enc_status;
			struct GUID guid;
			enc_status = ldap_decode_ndr_GUID(tmp_ctx, 
							  t->u.equality.value, &guid);
			if (NT_STATUS_IS_OK(enc_status)) {
				domain_guid = GUID_string(tmp_ctx, &guid);
			}
		}
		if (strcasecmp(t->u.equality.attr, "DomainSid") == 0) {
			enum ndr_err_code ndr_err;

			domain_sid = talloc(tmp_ctx, struct dom_sid);
			if (domain_sid == NULL) {
				goto failed;
			}
			ndr_err = ndr_pull_struct_blob(&t->u.equality.value,
						       domain_sid, domain_sid,
						       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				talloc_free(domain_sid);
				goto failed;
			}
		}
		if (strcasecmp(t->u.equality.attr, "User") == 0) {
			user = talloc_strndup(tmp_ctx, 
					      (const char *)t->u.equality.value.data,
					      t->u.equality.value.length);
		}
		if (strcasecmp(t->u.equality.attr, "NtVer") == 0 &&
		    t->u.equality.value.length == 4) {
			version = IVAL(t->u.equality.value.data, 0);
		}
		if (strcasecmp(t->u.equality.attr, "AAC") == 0 &&
		    t->u.equality.value.length == 4) {
			acct_control = IVAL(t->u.equality.value.data, 0);
		}
	}

	if ((domain == NULL) && (domain_guid == NULL) && (domain_sid == NULL)) {
		domain = lpcfg_dnsdomain(cldapd->task->lp_ctx);
	}

	if (version == -1) {
		goto failed;
	}

	DEBUG(5,("cldap netlogon query domain=%s host=%s user=%s version=%d guid=%s\n",
		 domain, host, user, version, domain_guid));

	status = fill_netlogon_samlogon_response(cldapd->samctx, tmp_ctx,
						 domain, NULL, domain_sid,
						 domain_guid,
						 user, acct_control,
						 tsocket_address_inet_addr_string(src, tmp_ctx),
						 version, cldapd->task->lp_ctx,
						 &netlogon, false);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	status = cldap_netlogon_reply(cldap, message_id, src, version, &netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	return;
	
failed:
	DEBUG(2,("cldap netlogon query failed domain=%s host=%s version=%d - %s\n",
		 domain, host, version, nt_errstr(status)));
	cldap_empty_reply(cldap, message_id, src);
}
