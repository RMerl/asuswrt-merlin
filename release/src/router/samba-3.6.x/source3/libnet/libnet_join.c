/*
 *  Unix SMB/CIFS implementation.
 *  libnet Join Support
 *  Copyright (C) Gerald (Jerry) Carter 2006
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
#include "ads.h"
#include "librpc/gen_ndr/ndr_libnet_join.h"
#include "libnet/libnet_join.h"
#include "libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/init_samr.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "rpc_client/cli_netlogon.h"
#include "lib/smbconf/smbconf.h"
#include "lib/smbconf/smbconf_reg.h"
#include "../libds/common/flags.h"
#include "secrets.h"
#include "rpc_client/init_lsa.h"
#include "rpc_client/cli_pipe.h"
#include "../libcli/security/security.h"
#include "passdb.h"
#include "libsmb/libsmb.h"

/****************************************************************
****************************************************************/

#define LIBNET_JOIN_DUMP_CTX(ctx, r, f) \
	do { \
		char *str = NULL; \
		str = NDR_PRINT_FUNCTION_STRING(ctx, libnet_JoinCtx, f, r); \
		DEBUG(1,("libnet_Join:\n%s", str)); \
		TALLOC_FREE(str); \
	} while (0)

#define LIBNET_JOIN_IN_DUMP_CTX(ctx, r) \
	LIBNET_JOIN_DUMP_CTX(ctx, r, NDR_IN | NDR_SET_VALUES)
#define LIBNET_JOIN_OUT_DUMP_CTX(ctx, r) \
	LIBNET_JOIN_DUMP_CTX(ctx, r, NDR_OUT)

#define LIBNET_UNJOIN_DUMP_CTX(ctx, r, f) \
	do { \
		char *str = NULL; \
		str = NDR_PRINT_FUNCTION_STRING(ctx, libnet_UnjoinCtx, f, r); \
		DEBUG(1,("libnet_Unjoin:\n%s", str)); \
		TALLOC_FREE(str); \
	} while (0)

#define LIBNET_UNJOIN_IN_DUMP_CTX(ctx, r) \
	LIBNET_UNJOIN_DUMP_CTX(ctx, r, NDR_IN | NDR_SET_VALUES)
#define LIBNET_UNJOIN_OUT_DUMP_CTX(ctx, r) \
	LIBNET_UNJOIN_DUMP_CTX(ctx, r, NDR_OUT)

/****************************************************************
****************************************************************/

static void libnet_join_set_error_string(TALLOC_CTX *mem_ctx,
					 struct libnet_JoinCtx *r,
					 const char *format, ...)
{
	va_list args;

	if (r->out.error_string) {
		return;
	}

	va_start(args, format);
	r->out.error_string = talloc_vasprintf(mem_ctx, format, args);
	va_end(args);
}

/****************************************************************
****************************************************************/

static void libnet_unjoin_set_error_string(TALLOC_CTX *mem_ctx,
					   struct libnet_UnjoinCtx *r,
					   const char *format, ...)
{
	va_list args;

	if (r->out.error_string) {
		return;
	}

	va_start(args, format);
	r->out.error_string = talloc_vasprintf(mem_ctx, format, args);
	va_end(args);
}

#ifdef HAVE_ADS

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_connect_ads(const char *dns_domain_name,
				     const char *netbios_domain_name,
				     const char *dc_name,
				     const char *user_name,
				     const char *password,
				     ADS_STRUCT **ads)
{
	ADS_STATUS status;
	ADS_STRUCT *my_ads = NULL;
	char *cp;

	my_ads = ads_init(dns_domain_name,
			  netbios_domain_name,
			  dc_name);
	if (!my_ads) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	if (user_name) {
		SAFE_FREE(my_ads->auth.user_name);
		my_ads->auth.user_name = SMB_STRDUP(user_name);
		if ((cp = strchr_m(my_ads->auth.user_name, '@'))!=0) {
			*cp++ = '\0';
			SAFE_FREE(my_ads->auth.realm);
			my_ads->auth.realm = smb_xstrdup(cp);
			strupper_m(my_ads->auth.realm);
		}
	}

	if (password) {
		SAFE_FREE(my_ads->auth.password);
		my_ads->auth.password = SMB_STRDUP(password);
	}

	status = ads_connect_user_creds(my_ads);
	if (!ADS_ERR_OK(status)) {
		ads_destroy(&my_ads);
		return status;
	}

	*ads = my_ads;
	return ADS_SUCCESS;
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_join_connect_ads(TALLOC_CTX *mem_ctx,
					  struct libnet_JoinCtx *r)
{
	ADS_STATUS status;

	status = libnet_connect_ads(r->out.dns_domain_name,
				    r->out.netbios_domain_name,
				    r->in.dc_name,
				    r->in.admin_account,
				    r->in.admin_password,
				    &r->in.ads);
	if (!ADS_ERR_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to connect to AD: %s",
			ads_errstr(status));
		return status;
	}

	if (!r->out.netbios_domain_name) {
		r->out.netbios_domain_name = talloc_strdup(mem_ctx,
							   r->in.ads->server.workgroup);
		ADS_ERROR_HAVE_NO_MEMORY(r->out.netbios_domain_name);
	}

	if (!r->out.dns_domain_name) {
		r->out.dns_domain_name = talloc_strdup(mem_ctx,
						       r->in.ads->config.realm);
		ADS_ERROR_HAVE_NO_MEMORY(r->out.dns_domain_name);
	}

	r->out.domain_is_ad = true;

	return ADS_SUCCESS;
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_unjoin_connect_ads(TALLOC_CTX *mem_ctx,
					    struct libnet_UnjoinCtx *r)
{
	ADS_STATUS status;

	status = libnet_connect_ads(r->in.domain_name,
				    r->in.domain_name,
				    r->in.dc_name,
				    r->in.admin_account,
				    r->in.admin_password,
				    &r->in.ads);
	if (!ADS_ERR_OK(status)) {
		libnet_unjoin_set_error_string(mem_ctx, r,
			"failed to connect to AD: %s",
			ads_errstr(status));
	}

	return status;
}

/****************************************************************
 join a domain using ADS (LDAP mods)
****************************************************************/

static ADS_STATUS libnet_join_precreate_machine_acct(TALLOC_CTX *mem_ctx,
						     struct libnet_JoinCtx *r)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	const char *attrs[] = { "dn", NULL };
	bool moved = false;

	status = ads_check_ou_dn(mem_ctx, r->in.ads, &r->in.account_ou);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	status = ads_search_dn(r->in.ads, &res, r->in.account_ou, attrs);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if (ads_count_replies(r->in.ads, res) != 1) {
		ads_msgfree(r->in.ads, res);
		return ADS_ERROR_LDAP(LDAP_NO_SUCH_OBJECT);
	}

	ads_msgfree(r->in.ads, res);

	/* Attempt to create the machine account and bail if this fails.
	   Assume that the admin wants exactly what they requested */

	status = ads_create_machine_acct(r->in.ads,
					 r->in.machine_name,
					 r->in.account_ou);

	if (ADS_ERR_OK(status)) {
		DEBUG(1,("machine account creation created\n"));
		return status;
	} else  if ((status.error_type == ENUM_ADS_ERROR_LDAP) &&
		    (status.err.rc == LDAP_ALREADY_EXISTS)) {
		status = ADS_SUCCESS;
	}

	if (!ADS_ERR_OK(status)) {
		DEBUG(1,("machine account creation failed\n"));
		return status;
	}

	status = ads_move_machine_acct(r->in.ads,
				       r->in.machine_name,
				       r->in.account_ou,
				       &moved);
	if (!ADS_ERR_OK(status)) {
		DEBUG(1,("failure to locate/move pre-existing "
			"machine account\n"));
		return status;
	}

	DEBUG(1,("The machine account %s the specified OU.\n",
		moved ? "was moved into" : "already exists in"));

	return status;
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_unjoin_remove_machine_acct(TALLOC_CTX *mem_ctx,
						    struct libnet_UnjoinCtx *r)
{
	ADS_STATUS status;

	if (!r->in.ads) {
		status = libnet_unjoin_connect_ads(mem_ctx, r);
		if (!ADS_ERR_OK(status)) {
			libnet_unjoin_set_error_string(mem_ctx, r,
				"failed to connect to AD: %s",
				ads_errstr(status));
			return status;
		}
	}

	status = ads_leave_realm(r->in.ads, r->in.machine_name);
	if (!ADS_ERR_OK(status)) {
		libnet_unjoin_set_error_string(mem_ctx, r,
			"failed to leave realm: %s",
			ads_errstr(status));
		return status;
	}

	return ADS_SUCCESS;
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_join_find_machine_acct(TALLOC_CTX *mem_ctx,
						struct libnet_JoinCtx *r)
{
	ADS_STATUS status;
	LDAPMessage *res = NULL;
	char *dn = NULL;

	if (!r->in.machine_name) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	status = ads_find_machine_acct(r->in.ads,
				       &res,
				       r->in.machine_name);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if (ads_count_replies(r->in.ads, res) != 1) {
		status = ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		goto done;
	}

	dn = ads_get_dn(r->in.ads, mem_ctx, res);
	if (!dn) {
		status = ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		goto done;
	}

	r->out.dn = talloc_strdup(mem_ctx, dn);
	if (!r->out.dn) {
		status = ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		goto done;
	}

 done:
	ads_msgfree(r->in.ads, res);
	TALLOC_FREE(dn);

	return status;
}

/****************************************************************
 Set a machines dNSHostName and servicePrincipalName attributes
****************************************************************/

static ADS_STATUS libnet_join_set_machine_spn(TALLOC_CTX *mem_ctx,
					      struct libnet_JoinCtx *r)
{
	ADS_STATUS status;
	ADS_MODLIST mods;
	fstring my_fqdn;
	const char *spn_array[3] = {NULL, NULL, NULL};
	char *spn = NULL;

	/* Find our DN */

	status = libnet_join_find_machine_acct(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	/* Windows only creates HOST/shortname & HOST/fqdn. */

	spn = talloc_asprintf(mem_ctx, "HOST/%s", r->in.machine_name);
	if (!spn) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}
	strupper_m(spn);
	spn_array[0] = spn;

	if (!name_to_fqdn(my_fqdn, r->in.machine_name)
	    || (strchr(my_fqdn, '.') == NULL)) {
		fstr_sprintf(my_fqdn, "%s.%s", r->in.machine_name,
			     r->out.dns_domain_name);
	}

	strlower_m(my_fqdn);

	if (!strequal(my_fqdn, r->in.machine_name)) {
		spn = talloc_asprintf(mem_ctx, "HOST/%s", my_fqdn);
		if (!spn) {
			return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
		}
		spn_array[1] = spn;
	}

	mods = ads_init_mods(mem_ctx);
	if (!mods) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	/* fields of primary importance */

	status = ads_mod_str(mem_ctx, &mods, "dNSHostName", my_fqdn);
	if (!ADS_ERR_OK(status)) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	status = ads_mod_strlist(mem_ctx, &mods, "servicePrincipalName",
				 spn_array);
	if (!ADS_ERR_OK(status)) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	return ads_gen_mod(r->in.ads, r->out.dn, mods);
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_join_set_machine_upn(TALLOC_CTX *mem_ctx,
					      struct libnet_JoinCtx *r)
{
	ADS_STATUS status;
	ADS_MODLIST mods;

	if (!r->in.create_upn) {
		return ADS_SUCCESS;
	}

	/* Find our DN */

	status = libnet_join_find_machine_acct(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	if (!r->in.upn) {
		r->in.upn = talloc_asprintf(mem_ctx,
					    "host/%s@%s",
					    r->in.machine_name,
					    r->out.dns_domain_name);
		if (!r->in.upn) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	/* now do the mods */

	mods = ads_init_mods(mem_ctx);
	if (!mods) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	/* fields of primary importance */

	status = ads_mod_str(mem_ctx, &mods, "userPrincipalName", r->in.upn);
	if (!ADS_ERR_OK(status)) {
		return ADS_ERROR_LDAP(LDAP_NO_MEMORY);
	}

	return ads_gen_mod(r->in.ads, r->out.dn, mods);
}


/****************************************************************
****************************************************************/

static ADS_STATUS libnet_join_set_os_attributes(TALLOC_CTX *mem_ctx,
						struct libnet_JoinCtx *r)
{
	ADS_STATUS status;
	ADS_MODLIST mods;
	char *os_sp = NULL;

	if (!r->in.os_name || !r->in.os_version ) {
		return ADS_SUCCESS;
	}

	/* Find our DN */

	status = libnet_join_find_machine_acct(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	/* now do the mods */

	mods = ads_init_mods(mem_ctx);
	if (!mods) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	os_sp = talloc_asprintf(mem_ctx, "Samba %s", samba_version_string());
	if (!os_sp) {
		return ADS_ERROR(LDAP_NO_MEMORY);
	}

	/* fields of primary importance */

	status = ads_mod_str(mem_ctx, &mods, "operatingSystem",
			     r->in.os_name);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	status = ads_mod_str(mem_ctx, &mods, "operatingSystemVersion",
			     r->in.os_version);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	status = ads_mod_str(mem_ctx, &mods, "operatingSystemServicePack",
			     os_sp);
	if (!ADS_ERR_OK(status)) {
		return status;
	}

	return ads_gen_mod(r->in.ads, r->out.dn, mods);
}

/****************************************************************
****************************************************************/

static bool libnet_join_create_keytab(TALLOC_CTX *mem_ctx,
				      struct libnet_JoinCtx *r)
{
	if (!USE_SYSTEM_KEYTAB) {
		return true;
	}

	if (ads_keytab_create_default(r->in.ads) != 0) {
		return false;
	}

	return true;
}

/****************************************************************
****************************************************************/

static bool libnet_join_derive_salting_principal(TALLOC_CTX *mem_ctx,
						 struct libnet_JoinCtx *r)
{
	uint32_t domain_func;
	ADS_STATUS status;
	const char *salt = NULL;
	char *std_salt = NULL;

	status = ads_domain_func_level(r->in.ads, &domain_func);
	if (!ADS_ERR_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to determine domain functional level: %s",
			ads_errstr(status));
		return false;
	}

	/* go ahead and setup the default salt */

	std_salt = kerberos_standard_des_salt();
	if (!std_salt) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to obtain standard DES salt");
		return false;
	}

	salt = talloc_strdup(mem_ctx, std_salt);
	if (!salt) {
		return false;
	}

	SAFE_FREE(std_salt);

	/* if it's a Windows functional domain, we have to look for the UPN */

	if (domain_func == DS_DOMAIN_FUNCTION_2000) {
		char *upn;

		upn = ads_get_upn(r->in.ads, mem_ctx,
				  r->in.machine_name);
		if (upn) {
			salt = talloc_strdup(mem_ctx, upn);
			if (!salt) {
				return false;
			}
		}
	}

	return kerberos_secrets_store_des_salt(salt);
}

/****************************************************************
****************************************************************/

static ADS_STATUS libnet_join_post_processing_ads(TALLOC_CTX *mem_ctx,
						  struct libnet_JoinCtx *r)
{
	ADS_STATUS status;

	if (!r->in.ads) {
		status = libnet_join_connect_ads(mem_ctx, r);
		if (!ADS_ERR_OK(status)) {
			return status;
		}
	}

	status = libnet_join_set_machine_spn(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to set machine spn: %s",
			ads_errstr(status));
		return status;
	}

	status = libnet_join_set_os_attributes(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to set machine os attributes: %s",
			ads_errstr(status));
		return status;
	}

	status = libnet_join_set_machine_upn(mem_ctx, r);
	if (!ADS_ERR_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to set machine upn: %s",
			ads_errstr(status));
		return status;
	}

	if (!libnet_join_derive_salting_principal(mem_ctx, r)) {
		return ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	}

	if (!libnet_join_create_keytab(mem_ctx, r)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to create kerberos keytab");
		return ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	}

	return ADS_SUCCESS;
}
#endif /* HAVE_ADS */

/****************************************************************
 Store the machine password and domain SID
****************************************************************/

static bool libnet_join_joindomain_store_secrets(TALLOC_CTX *mem_ctx,
						 struct libnet_JoinCtx *r)
{
	if (!secrets_store_domain_sid(r->out.netbios_domain_name,
				      r->out.domain_sid))
	{
		DEBUG(1,("Failed to save domain sid\n"));
		return false;
	}

	if (!secrets_store_machine_password(r->in.machine_password,
					    r->out.netbios_domain_name,
					    r->in.secure_channel_type))
	{
		DEBUG(1,("Failed to save machine password\n"));
		return false;
	}

	return true;
}

/****************************************************************
 Connect dc's IPC$ share
****************************************************************/

static NTSTATUS libnet_join_connect_dc_ipc(const char *dc,
					   const char *user,
					   const char *pass,
					   bool use_kerberos,
					   struct cli_state **cli)
{
	int flags = 0;

	if (use_kerberos) {
		flags |= CLI_FULL_CONNECTION_USE_KERBEROS;
	}

	if (use_kerberos && pass) {
		flags |= CLI_FULL_CONNECTION_FALLBACK_AFTER_KERBEROS;
	}

	return cli_full_connection(cli, NULL,
				   dc,
				   NULL, 0,
				   "IPC$", "IPC",
				   user,
				   NULL,
				   pass,
				   flags,
				   Undefined);
}

/****************************************************************
 Lookup domain dc's info
****************************************************************/

static NTSTATUS libnet_join_lookup_dc_rpc(TALLOC_CTX *mem_ctx,
					  struct libnet_JoinCtx *r,
					  struct cli_state **cli)
{
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct policy_handle lsa_pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	struct dcerpc_binding_handle *b;

	status = libnet_join_connect_dc_ipc(r->in.dc_name,
					    r->in.admin_account,
					    r->in.admin_password,
					    r->in.use_kerberos,
					    cli);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = cli_rpc_pipe_open_noauth(*cli, &ndr_table_lsarpc.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Error connecting to LSA pipe. Error was %s\n",
			nt_errstr(status)));
		goto done;
	}

	b = pipe_hnd->binding_handle;

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					SEC_FLAG_MAXIMUM_ALLOWED, &lsa_pol);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy2(b, mem_ctx,
					     &lsa_pol,
					     LSA_POLICY_INFO_DNS,
					     &info,
					     &result);
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(result)) {
		r->out.domain_is_ad = true;
		r->out.netbios_domain_name = info->dns.name.string;
		r->out.dns_domain_name = info->dns.dns_domain.string;
		r->out.forest_name = info->dns.dns_forest.string;
		r->out.domain_sid = dom_sid_dup(mem_ctx, info->dns.sid);
		NT_STATUS_HAVE_NO_MEMORY(r->out.domain_sid);
	}

	if (!NT_STATUS_IS_OK(status)) {
		status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
						    &lsa_pol,
						    LSA_POLICY_INFO_ACCOUNT_DOMAIN,
						    &info,
						    &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			status = result;
			goto done;
		}

		r->out.netbios_domain_name = info->account_domain.name.string;
		r->out.domain_sid = dom_sid_dup(mem_ctx, info->account_domain.sid);
		NT_STATUS_HAVE_NO_MEMORY(r->out.domain_sid);
	}

	dcerpc_lsa_Close(b, mem_ctx, &lsa_pol, &result);
	TALLOC_FREE(pipe_hnd);

 done:
	return status;
}

/****************************************************************
 Do the domain join unsecure
****************************************************************/

static NTSTATUS libnet_join_joindomain_rpc_unsecure(TALLOC_CTX *mem_ctx,
						    struct libnet_JoinCtx *r,
						    struct cli_state *cli)
{
	struct rpc_pipe_client *pipe_hnd = NULL;
	unsigned char orig_trust_passwd_hash[16];
	unsigned char new_trust_passwd_hash[16];
	fstring trust_passwd;
	NTSTATUS status;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!r->in.machine_password) {
		r->in.machine_password = generate_random_str(mem_ctx, DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);
		NT_STATUS_HAVE_NO_MEMORY(r->in.machine_password);
	}

	E_md4hash(r->in.machine_password, new_trust_passwd_hash);

	/* according to WKSSVC_JOIN_FLAGS_MACHINE_PWD_PASSED */
	fstrcpy(trust_passwd, r->in.admin_password);
	strlower_m(trust_passwd);

	/*
	 * Machine names can be 15 characters, but the max length on
	 * a password is 14.  --jerry
	 */

	trust_passwd[14] = '\0';

	E_md4hash(trust_passwd, orig_trust_passwd_hash);

	status = rpccli_netlogon_set_trust_password(pipe_hnd, mem_ctx,
						    r->in.machine_name,
						    orig_trust_passwd_hash,
						    r->in.machine_password,
						    new_trust_passwd_hash,
						    r->in.secure_channel_type);

	return status;
}

/****************************************************************
 Do the domain join
****************************************************************/

static NTSTATUS libnet_join_joindomain_rpc(TALLOC_CTX *mem_ctx,
					   struct libnet_JoinCtx *r,
					   struct cli_state *cli)
{
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct policy_handle sam_pol, domain_pol, user_pol;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL, result;
	char *acct_name;
	struct lsa_String lsa_acct_name;
	uint32_t user_rid;
	uint32_t acct_flags = ACB_WSTRUST;
	struct samr_Ids user_rids;
	struct samr_Ids name_types;
	union samr_UserInfo user_info;
	struct dcerpc_binding_handle *b = NULL;
	unsigned int old_timeout = 0;

	struct samr_CryptPassword crypt_pwd;
	struct samr_CryptPasswordEx crypt_pwd_ex;

	ZERO_STRUCT(sam_pol);
	ZERO_STRUCT(domain_pol);
	ZERO_STRUCT(user_pol);

	switch (r->in.secure_channel_type) {
	case SEC_CHAN_WKSTA:
		acct_flags = ACB_WSTRUST;
		break;
	case SEC_CHAN_BDC:
		acct_flags = ACB_SVRTRUST;
		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!r->in.machine_password) {
		r->in.machine_password = generate_random_str(mem_ctx, DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);
		NT_STATUS_HAVE_NO_MEMORY(r->in.machine_password);
	}

	/* Open the domain */

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_samr.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Error connecting to SAM pipe. Error was %s\n",
			nt_errstr(status)));
		goto done;
	}

	b = pipe_hnd->binding_handle;

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      SAMR_ACCESS_ENUM_DOMAINS
				      | SAMR_ACCESS_LOOKUP_DOMAIN,
				      &sam_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&sam_pol,
					SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1
					| SAMR_DOMAIN_ACCESS_CREATE_USER
					| SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					r->out.domain_sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Create domain user */

	acct_name = talloc_asprintf(mem_ctx, "%s$", r->in.machine_name);
	strlower_m(acct_name);

	init_lsa_String(&lsa_acct_name, acct_name);

	if (r->in.join_flags & WKSSVC_JOIN_FLAGS_ACCOUNT_CREATE) {
		uint32_t access_desired =
			SEC_GENERIC_READ | SEC_GENERIC_WRITE | SEC_GENERIC_EXECUTE |
			SEC_STD_WRITE_DAC | SEC_STD_DELETE |
			SAMR_USER_ACCESS_SET_PASSWORD |
			SAMR_USER_ACCESS_GET_ATTRIBUTES |
			SAMR_USER_ACCESS_SET_ATTRIBUTES;
		uint32_t access_granted = 0;

		DEBUG(10,("Creating account with desired access mask: %d\n",
			access_desired));

		status = dcerpc_samr_CreateUser2(b, mem_ctx,
						 &domain_pol,
						 &lsa_acct_name,
						 acct_flags,
						 access_desired,
						 &user_pol,
						 &access_granted,
						 &user_rid,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		status = result;
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {

			DEBUG(10,("Creation of workstation account failed: %s\n",
				nt_errstr(status)));

			/* If NT_STATUS_ACCESS_DENIED then we have a valid
			   username/password combo but the user does not have
			   administrator access. */

			if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
				libnet_join_set_error_string(mem_ctx, r,
					"User specified does not have "
					"administrator privileges");
			}

			goto done;
		}

		if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
			if (!(r->in.join_flags &
			      WKSSVC_JOIN_FLAGS_DOMAIN_JOIN_IF_JOINED)) {
				goto done;
			}
		}

		/* We *must* do this.... don't ask... */

		if (NT_STATUS_IS_OK(status)) {
			dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		}
	}

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &user_rids,
					 &name_types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}
	if (user_rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (name_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	if (name_types.ids[0] != SID_NAME_USER) {
		DEBUG(0,("%s is not a user account (type=%d)\n",
			acct_name, name_types.ids[0]));
		status = NT_STATUS_INVALID_WORKSTATION;
		goto done;
	}

	user_rid = user_rids.ids[0];

	/* Open handle on user */

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      &domain_pol,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      user_rid,
				      &user_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Fill in the additional account flags now */

	acct_flags |= ACB_PWNOEXP;

	/* Set account flags on machine account */
	ZERO_STRUCT(user_info.info16);
	user_info.info16.acct_flags = acct_flags;

	status = dcerpc_samr_SetUserInfo(b, mem_ctx,
					 &user_pol,
					 16,
					 &user_info,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		dcerpc_samr_DeleteUser(b, mem_ctx,
				       &user_pol,
				       &result);

		libnet_join_set_error_string(mem_ctx, r,
			"Failed to set account flags for machine account (%s)\n",
			nt_errstr(status));
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;

		dcerpc_samr_DeleteUser(b, mem_ctx,
				       &user_pol,
				       &result);

		libnet_join_set_error_string(mem_ctx, r,
			"Failed to set account flags for machine account (%s)\n",
			nt_errstr(status));
		goto done;
	}

	/* Set password on machine account - first try level 26 */

	/*
	 * increase the timeout as password filter modules on the DC
	 * might delay the operation for a significant amount of time
	 */
	old_timeout = rpccli_set_timeout(pipe_hnd, 600000);

	init_samr_CryptPasswordEx(r->in.machine_password,
				  &cli->user_session_key,
				  &crypt_pwd_ex);

	user_info.info26.password = crypt_pwd_ex;
	user_info.info26.password_expired = PASS_DONT_CHANGE_AT_NEXT_LOGON;

	status = dcerpc_samr_SetUserInfo2(b, mem_ctx,
					  &user_pol,
					  26,
					  &user_info,
					  &result);

	if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE)) {

		/* retry with level 24 */

		init_samr_CryptPassword(r->in.machine_password,
					&cli->user_session_key,
					&crypt_pwd);

		user_info.info24.password = crypt_pwd;
		user_info.info24.password_expired = PASS_DONT_CHANGE_AT_NEXT_LOGON;

		status = dcerpc_samr_SetUserInfo2(b, mem_ctx,
						  &user_pol,
						  24,
						  &user_info,
						  &result);
	}

	old_timeout = rpccli_set_timeout(pipe_hnd, old_timeout);

	if (!NT_STATUS_IS_OK(status)) {

		dcerpc_samr_DeleteUser(b, mem_ctx,
				       &user_pol,
				       &result);

		libnet_join_set_error_string(mem_ctx, r,
			"Failed to set password for machine account (%s)\n",
			nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;

		dcerpc_samr_DeleteUser(b, mem_ctx,
				       &user_pol,
				       &result);

		libnet_join_set_error_string(mem_ctx, r,
			"Failed to set password for machine account (%s)\n",
			nt_errstr(status));
		goto done;
	}

	status = NT_STATUS_OK;

 done:
	if (!pipe_hnd) {
		return status;
	}

	if (is_valid_policy_hnd(&sam_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &sam_pol, &result);
	}
	if (is_valid_policy_hnd(&domain_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);
	}
	if (is_valid_policy_hnd(&user_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
	}
	TALLOC_FREE(pipe_hnd);

	return status;
}

/****************************************************************
****************************************************************/

NTSTATUS libnet_join_ok(const char *netbios_domain_name,
			const char *machine_name,
			const char *dc_name)
{
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS status;
	char *machine_password = NULL;
	char *machine_account = NULL;

	if (!dc_name) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!secrets_init()) {
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	machine_password = secrets_fetch_machine_password(netbios_domain_name,
							  NULL, NULL);
	if (!machine_password) {
		return NT_STATUS_NO_TRUST_LSA_SECRET;
	}

	if (asprintf(&machine_account, "%s$", machine_name) == -1) {
		SAFE_FREE(machine_password);
		return NT_STATUS_NO_MEMORY;
	}

	status = cli_full_connection(&cli, NULL,
				     dc_name,
				     NULL, 0,
				     "IPC$", "IPC",
				     machine_account,
				     NULL,
				     machine_password,
				     0,
				     Undefined);
	free(machine_account);
	free(machine_password);

	if (!NT_STATUS_IS_OK(status)) {
		status = cli_full_connection(&cli, NULL,
					     dc_name,
					     NULL, 0,
					     "IPC$", "IPC",
					     "",
					     NULL,
					     "",
					     0,
					     Undefined);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = get_schannel_session_key(cli, netbios_domain_name,
					  &neg_flags, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_NETWORK_RESPONSE)) {
			cli_shutdown(cli);
			return NT_STATUS_OK;
		}

		DEBUG(0,("libnet_join_ok: failed to get schannel session "
			"key from server %s for domain %s. Error was %s\n",
		cli->desthost, netbios_domain_name, nt_errstr(status)));
		cli_shutdown(cli);
		return status;
	}

	if (!lp_client_schannel()) {
		cli_shutdown(cli);
		return NT_STATUS_OK;
	}

	status = cli_rpc_pipe_open_schannel_with_key(
		cli, &ndr_table_netlogon.syntax_id, NCACN_NP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		netbios_domain_name, &netlogon_pipe->dc, &pipe_hnd);

	cli_shutdown(cli);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("libnet_join_ok: failed to open schannel session "
			"on netlogon pipe to server %s for domain %s. "
			"Error was %s\n",
			cli->desthost, netbios_domain_name, nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static WERROR libnet_join_post_verify(TALLOC_CTX *mem_ctx,
				      struct libnet_JoinCtx *r)
{
	NTSTATUS status;

	status = libnet_join_ok(r->out.netbios_domain_name,
				r->in.machine_name,
				r->in.dc_name);
	if (!NT_STATUS_IS_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to verify domain membership after joining: %s",
			get_friendly_nt_error_msg(status));
		return WERR_SETUP_NOT_JOINED;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static bool libnet_join_unjoindomain_remove_secrets(TALLOC_CTX *mem_ctx,
						    struct libnet_UnjoinCtx *r)
{
	if (!secrets_delete_machine_password_ex(lp_workgroup())) {
		return false;
	}

	if (!secrets_delete_domain_sid(lp_workgroup())) {
		return false;
	}

	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS libnet_join_unjoindomain_rpc(TALLOC_CTX *mem_ctx,
					     struct libnet_UnjoinCtx *r)
{
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct policy_handle sam_pol, domain_pol, user_pol;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL, result;
	char *acct_name;
	uint32_t user_rid;
	struct lsa_String lsa_acct_name;
	struct samr_Ids user_rids;
	struct samr_Ids name_types;
	union samr_UserInfo *info = NULL;
	struct dcerpc_binding_handle *b = NULL;

	ZERO_STRUCT(sam_pol);
	ZERO_STRUCT(domain_pol);
	ZERO_STRUCT(user_pol);

	status = libnet_join_connect_dc_ipc(r->in.dc_name,
					    r->in.admin_account,
					    r->in.admin_password,
					    r->in.use_kerberos,
					    &cli);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* Open the domain */

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_samr.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Error connecting to SAM pipe. Error was %s\n",
			nt_errstr(status)));
		goto done;
	}

	b = pipe_hnd->binding_handle;

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      &sam_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&sam_pol,
					SEC_FLAG_MAXIMUM_ALLOWED,
					r->in.domain_sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Create domain user */

	acct_name = talloc_asprintf(mem_ctx, "%s$", r->in.machine_name);
	strlower_m(acct_name);

	init_lsa_String(&lsa_acct_name, acct_name);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_pol,
					 1,
					 &lsa_acct_name,
					 &user_rids,
					 &name_types,
					 &result);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}
	if (user_rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (name_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	if (name_types.ids[0] != SID_NAME_USER) {
		DEBUG(0, ("%s is not a user account (type=%d)\n", acct_name,
			name_types.ids[0]));
		status = NT_STATUS_INVALID_WORKSTATION;
		goto done;
	}

	user_rid = user_rids.ids[0];

	/* Open handle on user */

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      &domain_pol,
				      SEC_FLAG_MAXIMUM_ALLOWED,
				      user_rid,
				      &user_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get user info */

	status = dcerpc_samr_QueryUserInfo(b, mem_ctx,
					   &user_pol,
					   16,
					   &info,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		goto done;
	}

	/* now disable and setuser info */

	info->info16.acct_flags |= ACB_DISABLED;

	status = dcerpc_samr_SetUserInfo(b, mem_ctx,
					 &user_pol,
					 16,
					 info,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
		goto done;
	}
	status = result;
	dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);

done:
	if (pipe_hnd && b) {
		if (is_valid_policy_hnd(&domain_pol)) {
			dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);
		}
		if (is_valid_policy_hnd(&sam_pol)) {
			dcerpc_samr_Close(b, mem_ctx, &sam_pol, &result);
		}
		TALLOC_FREE(pipe_hnd);
	}

	if (cli) {
		cli_shutdown(cli);
	}

	return status;
}

/****************************************************************
****************************************************************/

static WERROR do_join_modify_vals_config(struct libnet_JoinCtx *r)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct smbconf_ctx *ctx;

	err = smbconf_init_reg(r, &ctx, NULL);
	if (!SBC_ERROR_IS_OK(err)) {
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	if (!(r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE)) {

		err = smbconf_set_global_parameter(ctx, "security", "user");
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		err = smbconf_set_global_parameter(ctx, "workgroup",
						   r->in.domain_name);
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		smbconf_delete_global_parameter(ctx, "realm");
		goto done;
	}

	err = smbconf_set_global_parameter(ctx, "security", "domain");
	if (!SBC_ERROR_IS_OK(err)) {
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	err = smbconf_set_global_parameter(ctx, "workgroup",
					   r->out.netbios_domain_name);
	if (!SBC_ERROR_IS_OK(err)) {
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	if (r->out.domain_is_ad) {
		err = smbconf_set_global_parameter(ctx, "security", "ads");
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		err = smbconf_set_global_parameter(ctx, "realm",
						   r->out.dns_domain_name);
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}
	}

 done:
	smbconf_shutdown(ctx);
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR do_unjoin_modify_vals_config(struct libnet_UnjoinCtx *r)
{
	WERROR werr = WERR_OK;
	sbcErr err;
	struct smbconf_ctx *ctx;

	err = smbconf_init_reg(r, &ctx, NULL);
	if (!SBC_ERROR_IS_OK(err)) {
		werr = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	if (r->in.unjoin_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE) {

		err = smbconf_set_global_parameter(ctx, "security", "user");
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		err = smbconf_delete_global_parameter(ctx, "workgroup");
		if (!SBC_ERROR_IS_OK(err)) {
			werr = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		smbconf_delete_global_parameter(ctx, "realm");
	}

 done:
	smbconf_shutdown(ctx);
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR do_JoinConfig(struct libnet_JoinCtx *r)
{
	WERROR werr;

	if (!W_ERROR_IS_OK(r->out.result)) {
		return r->out.result;
	}

	if (!r->in.modify_config) {
		return WERR_OK;
	}

	werr = do_join_modify_vals_config(r);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	lp_load(get_dyn_CONFIGFILE(),true,false,false,true);

	r->out.modified_config = true;
	r->out.result = werr;

	return werr;
}

/****************************************************************
****************************************************************/

static WERROR libnet_unjoin_config(struct libnet_UnjoinCtx *r)
{
	WERROR werr;

	if (!W_ERROR_IS_OK(r->out.result)) {
		return r->out.result;
	}

	if (!r->in.modify_config) {
		return WERR_OK;
	}

	werr = do_unjoin_modify_vals_config(r);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	lp_load(get_dyn_CONFIGFILE(),true,false,false,true);

	r->out.modified_config = true;
	r->out.result = werr;

	return werr;
}

/****************************************************************
****************************************************************/

static bool libnet_parse_domain_dc(TALLOC_CTX *mem_ctx,
				   const char *domain_str,
				   const char **domain_p,
				   const char **dc_p)
{
	char *domain = NULL;
	char *dc = NULL;
	const char *p = NULL;

	if (!domain_str || !domain_p || !dc_p) {
		return false;
	}

	p = strchr_m(domain_str, '\\');

	if (p != NULL) {
		domain = talloc_strndup(mem_ctx, domain_str,
					 PTR_DIFF(p, domain_str));
		dc = talloc_strdup(mem_ctx, p+1);
		if (!dc) {
			return false;
		}
	} else {
		domain = talloc_strdup(mem_ctx, domain_str);
		dc = NULL;
	}
	if (!domain) {
		return false;
	}

	*domain_p = domain;

	if (!*dc_p && dc) {
		*dc_p = dc;
	}

	return true;
}

/****************************************************************
****************************************************************/

static WERROR libnet_join_pre_processing(TALLOC_CTX *mem_ctx,
					 struct libnet_JoinCtx *r)
{
	if (!r->in.domain_name) {
		libnet_join_set_error_string(mem_ctx, r,
			"No domain name defined");
		return WERR_INVALID_PARAM;
	}

	if (!libnet_parse_domain_dc(mem_ctx, r->in.domain_name,
				    &r->in.domain_name,
				    &r->in.dc_name)) {
		libnet_join_set_error_string(mem_ctx, r,
			"Failed to parse domain name");
		return WERR_INVALID_PARAM;
	}

	if (IS_DC) {
		return WERR_SETUP_DOMAIN_CONTROLLER;
	}

	if (!secrets_init()) {
		libnet_join_set_error_string(mem_ctx, r,
			"Unable to open secrets database");
		return WERR_CAN_NOT_COMPLETE;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static void libnet_join_add_dom_rids_to_builtins(struct dom_sid *domain_sid)
{
	NTSTATUS status;

	/* Try adding dom admins to builtin\admins. Only log failures. */
	status = create_builtin_administrators(domain_sid);
	if (NT_STATUS_EQUAL(status, NT_STATUS_PROTOCOL_UNREACHABLE)) {
		DEBUG(10,("Unable to auto-add domain administrators to "
			  "BUILTIN\\Administrators during join because "
			  "winbindd must be running."));
	} else if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("Failed to auto-add domain administrators to "
			  "BUILTIN\\Administrators during join: %s\n",
			  nt_errstr(status)));
	}

	/* Try adding dom users to builtin\users. Only log failures. */
	status = create_builtin_users(domain_sid);
	if (NT_STATUS_EQUAL(status, NT_STATUS_PROTOCOL_UNREACHABLE)) {
		DEBUG(10,("Unable to auto-add domain users to BUILTIN\\users "
			  "during join because winbindd must be running."));
	} else if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("Failed to auto-add domain administrators to "
			  "BUILTIN\\Administrators during join: %s\n",
			  nt_errstr(status)));
	}
}

/****************************************************************
****************************************************************/

static WERROR libnet_join_post_processing(TALLOC_CTX *mem_ctx,
					  struct libnet_JoinCtx *r)
{
	WERROR werr;

	if (!W_ERROR_IS_OK(r->out.result)) {
		return r->out.result;
	}

	werr = do_JoinConfig(r);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	if (!(r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE)) {
		return WERR_OK;
	}

	saf_join_store(r->out.netbios_domain_name, r->in.dc_name);
	if (r->out.dns_domain_name) {
		saf_join_store(r->out.dns_domain_name, r->in.dc_name);
	}

#ifdef HAVE_ADS
	if (r->out.domain_is_ad &&
	    !(r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_UNSECURE)) {
		ADS_STATUS ads_status;

		ads_status  = libnet_join_post_processing_ads(mem_ctx, r);
		if (!ADS_ERR_OK(ads_status)) {
			return WERR_GENERAL_FAILURE;
		}
	}
#endif /* HAVE_ADS */

	libnet_join_add_dom_rids_to_builtins(r->out.domain_sid);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static int libnet_destroy_JoinCtx(struct libnet_JoinCtx *r)
{
	if (r->in.ads) {
		ads_destroy(&r->in.ads);
	}

	return 0;
}

/****************************************************************
****************************************************************/

static int libnet_destroy_UnjoinCtx(struct libnet_UnjoinCtx *r)
{
	if (r->in.ads) {
		ads_destroy(&r->in.ads);
	}

	return 0;
}

/****************************************************************
****************************************************************/

WERROR libnet_init_JoinCtx(TALLOC_CTX *mem_ctx,
			   struct libnet_JoinCtx **r)
{
	struct libnet_JoinCtx *ctx;

	ctx = talloc_zero(mem_ctx, struct libnet_JoinCtx);
	if (!ctx) {
		return WERR_NOMEM;
	}

	talloc_set_destructor(ctx, libnet_destroy_JoinCtx);

	ctx->in.machine_name = talloc_strdup(mem_ctx, global_myname());
	W_ERROR_HAVE_NO_MEMORY(ctx->in.machine_name);

	ctx->in.secure_channel_type = SEC_CHAN_WKSTA;

	*r = ctx;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR libnet_init_UnjoinCtx(TALLOC_CTX *mem_ctx,
			     struct libnet_UnjoinCtx **r)
{
	struct libnet_UnjoinCtx *ctx;

	ctx = talloc_zero(mem_ctx, struct libnet_UnjoinCtx);
	if (!ctx) {
		return WERR_NOMEM;
	}

	talloc_set_destructor(ctx, libnet_destroy_UnjoinCtx);

	ctx->in.machine_name = talloc_strdup(mem_ctx, global_myname());
	W_ERROR_HAVE_NO_MEMORY(ctx->in.machine_name);

	*r = ctx;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR libnet_join_check_config(TALLOC_CTX *mem_ctx,
				       struct libnet_JoinCtx *r)
{
	bool valid_security = false;
	bool valid_workgroup = false;
	bool valid_realm = false;

	/* check if configuration is already set correctly */

	valid_workgroup = strequal(lp_workgroup(), r->out.netbios_domain_name);

	switch (r->out.domain_is_ad) {
		case false:
			valid_security = (lp_security() == SEC_DOMAIN);
			if (valid_workgroup && valid_security) {
				/* nothing to be done */
				return WERR_OK;
			}
			break;
		case true:
			valid_realm = strequal(lp_realm(), r->out.dns_domain_name);
			switch (lp_security()) {
			case SEC_DOMAIN:
			case SEC_ADS:
				valid_security = true;
			}

			if (valid_workgroup && valid_realm && valid_security) {
				/* nothing to be done */
				return WERR_OK;
			}
			break;
	}

	/* check if we are supposed to manipulate configuration */

	if (!r->in.modify_config) {

		char *wrong_conf = talloc_strdup(mem_ctx, "");

		if (!valid_workgroup) {
			wrong_conf = talloc_asprintf_append(wrong_conf,
				"\"workgroup\" set to '%s', should be '%s'",
				lp_workgroup(), r->out.netbios_domain_name);
			W_ERROR_HAVE_NO_MEMORY(wrong_conf);
		}

		if (!valid_realm) {
			wrong_conf = talloc_asprintf_append(wrong_conf,
				"\"realm\" set to '%s', should be '%s'",
				lp_realm(), r->out.dns_domain_name);
			W_ERROR_HAVE_NO_MEMORY(wrong_conf);
		}

		if (!valid_security) {
			const char *sec = NULL;
			switch (lp_security()) {
			case SEC_SHARE: sec = "share"; break;
			case SEC_USER:  sec = "user"; break;
			case SEC_DOMAIN: sec = "domain"; break;
			case SEC_ADS: sec = "ads"; break;
			}
			wrong_conf = talloc_asprintf_append(wrong_conf,
				"\"security\" set to '%s', should be %s",
				sec, r->out.domain_is_ad ?
				"either 'domain' or 'ads'" : "'domain'");
			W_ERROR_HAVE_NO_MEMORY(wrong_conf);
		}

		libnet_join_set_error_string(mem_ctx, r,
			"Invalid configuration (%s) and configuration modification "
			"was not requested", wrong_conf);
		return WERR_CAN_NOT_COMPLETE;
	}

	/* check if we are able to manipulate configuration */

	if (!lp_config_backend_is_registry()) {
		libnet_join_set_error_string(mem_ctx, r,
			"Configuration manipulation requested but not "
			"supported by backend");
		return WERR_NOT_SUPPORTED;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR libnet_DomainJoin(TALLOC_CTX *mem_ctx,
				struct libnet_JoinCtx *r)
{
	NTSTATUS status;
	WERROR werr;
	struct cli_state *cli = NULL;
#ifdef HAVE_ADS
	ADS_STATUS ads_status;
#endif /* HAVE_ADS */

	if (!r->in.dc_name) {
		struct netr_DsRGetDCNameInfo *info;
		const char *dc;
		status = dsgetdcname(mem_ctx,
				     r->in.msg_ctx,
				     r->in.domain_name,
				     NULL,
				     NULL,
				     DS_FORCE_REDISCOVERY |
				     DS_DIRECTORY_SERVICE_REQUIRED |
				     DS_WRITABLE_REQUIRED |
				     DS_RETURN_DNS_NAME,
				     &info);
		if (!NT_STATUS_IS_OK(status)) {
			libnet_join_set_error_string(mem_ctx, r,
				"failed to find DC for domain %s",
				r->in.domain_name,
				get_friendly_nt_error_msg(status));
			return WERR_DCNOTFOUND;
		}

		dc = strip_hostname(info->dc_unc);
		r->in.dc_name = talloc_strdup(mem_ctx, dc);
		W_ERROR_HAVE_NO_MEMORY(r->in.dc_name);
	}

	status = libnet_join_lookup_dc_rpc(mem_ctx, r, &cli);
	if (!NT_STATUS_IS_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to lookup DC info for domain '%s' over rpc: %s",
			r->in.domain_name, get_friendly_nt_error_msg(status));
		return ntstatus_to_werror(status);
	}

	werr = libnet_join_check_config(mem_ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

#ifdef HAVE_ADS

	create_local_private_krb5_conf_for_domain(
		r->out.dns_domain_name, r->out.netbios_domain_name,
		NULL, &cli->dest_ss, cli->desthost);

	if (r->out.domain_is_ad && r->in.account_ou &&
	    !(r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_UNSECURE)) {

		ads_status = libnet_join_connect_ads(mem_ctx, r);
		if (!ADS_ERR_OK(ads_status)) {
			return WERR_DEFAULT_JOIN_REQUIRED;
		}

		ads_status = libnet_join_precreate_machine_acct(mem_ctx, r);
		if (!ADS_ERR_OK(ads_status)) {
			libnet_join_set_error_string(mem_ctx, r,
				"failed to precreate account in ou %s: %s",
				r->in.account_ou,
				ads_errstr(ads_status));
			return WERR_DEFAULT_JOIN_REQUIRED;
		}

		r->in.join_flags &= ~WKSSVC_JOIN_FLAGS_ACCOUNT_CREATE;
	}
#endif /* HAVE_ADS */

	if ((r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_UNSECURE) &&
	    (r->in.join_flags & WKSSVC_JOIN_FLAGS_MACHINE_PWD_PASSED)) {
		status = libnet_join_joindomain_rpc_unsecure(mem_ctx, r, cli);
	} else {
		status = libnet_join_joindomain_rpc(mem_ctx, r, cli);
	}
	if (!NT_STATUS_IS_OK(status)) {
		libnet_join_set_error_string(mem_ctx, r,
			"failed to join domain '%s' over rpc: %s",
			r->in.domain_name, get_friendly_nt_error_msg(status));
		if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
			return WERR_SETUP_ALREADY_JOINED;
		}
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!libnet_join_joindomain_store_secrets(mem_ctx, r)) {
		werr = WERR_SETUP_NOT_JOINED;
		goto done;
	}

	werr = WERR_OK;

 done:
	if (cli) {
		cli_shutdown(cli);
	}

	return werr;
}

/****************************************************************
****************************************************************/

static WERROR libnet_join_rollback(TALLOC_CTX *mem_ctx,
				   struct libnet_JoinCtx *r)
{
	WERROR werr;
	struct libnet_UnjoinCtx *u = NULL;

	werr = libnet_init_UnjoinCtx(mem_ctx, &u);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	u->in.debug		= r->in.debug;
	u->in.dc_name		= r->in.dc_name;
	u->in.domain_name	= r->in.domain_name;
	u->in.admin_account	= r->in.admin_account;
	u->in.admin_password	= r->in.admin_password;
	u->in.modify_config	= r->in.modify_config;
	u->in.unjoin_flags	= WKSSVC_JOIN_FLAGS_JOIN_TYPE |
				  WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE;

	werr = libnet_Unjoin(mem_ctx, u);
	TALLOC_FREE(u);

	return werr;
}

/****************************************************************
****************************************************************/

WERROR libnet_Join(TALLOC_CTX *mem_ctx,
		   struct libnet_JoinCtx *r)
{
	WERROR werr;

	if (r->in.debug) {
		LIBNET_JOIN_IN_DUMP_CTX(mem_ctx, r);
	}

	ZERO_STRUCT(r->out);

	werr = libnet_join_pre_processing(mem_ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE) {
		werr = libnet_DomainJoin(mem_ctx, r);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}
	}

	werr = libnet_join_post_processing(mem_ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE) {
		werr = libnet_join_post_verify(mem_ctx, r);
		if (!W_ERROR_IS_OK(werr)) {
			libnet_join_rollback(mem_ctx, r);
		}
	}

 done:
	r->out.result = werr;

	if (r->in.debug) {
		LIBNET_JOIN_OUT_DUMP_CTX(mem_ctx, r);
	}
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR libnet_DomainUnjoin(TALLOC_CTX *mem_ctx,
				  struct libnet_UnjoinCtx *r)
{
	NTSTATUS status;

	if (!r->in.domain_sid) {
		struct dom_sid sid;
		if (!secrets_fetch_domain_sid(lp_workgroup(), &sid)) {
			libnet_unjoin_set_error_string(mem_ctx, r,
				"Unable to fetch domain sid: are we joined?");
			return WERR_SETUP_NOT_JOINED;
		}
		r->in.domain_sid = dom_sid_dup(mem_ctx, &sid);
		W_ERROR_HAVE_NO_MEMORY(r->in.domain_sid);
	}

	if (!(r->in.unjoin_flags & WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE) && 
	    !r->in.delete_machine_account) {
		libnet_join_unjoindomain_remove_secrets(mem_ctx, r);
		return WERR_OK;
	}

	if (!r->in.dc_name) {
		struct netr_DsRGetDCNameInfo *info;
		const char *dc;
		status = dsgetdcname(mem_ctx,
				     r->in.msg_ctx,
				     r->in.domain_name,
				     NULL,
				     NULL,
				     DS_DIRECTORY_SERVICE_REQUIRED |
				     DS_WRITABLE_REQUIRED |
				     DS_RETURN_DNS_NAME,
				     &info);
		if (!NT_STATUS_IS_OK(status)) {
			libnet_unjoin_set_error_string(mem_ctx, r,
				"failed to find DC for domain %s",
				r->in.domain_name,
				get_friendly_nt_error_msg(status));
			return WERR_DCNOTFOUND;
		}

		dc = strip_hostname(info->dc_unc);
		r->in.dc_name = talloc_strdup(mem_ctx, dc);
		W_ERROR_HAVE_NO_MEMORY(r->in.dc_name);
	}

#ifdef HAVE_ADS
	/* for net ads leave, try to delete the account.  If it works, 
	   no sense in disabling.  If it fails, we can still try to 
	   disable it. jmcd */

	if (r->in.delete_machine_account) {
		ADS_STATUS ads_status;
		ads_status = libnet_unjoin_connect_ads(mem_ctx, r);
		if (ADS_ERR_OK(ads_status)) {
			/* dirty hack */
			r->out.dns_domain_name = 
				talloc_strdup(mem_ctx,
					      r->in.ads->server.realm);
			ads_status = 
				libnet_unjoin_remove_machine_acct(mem_ctx, r);
		}
		if (!ADS_ERR_OK(ads_status)) {
			libnet_unjoin_set_error_string(mem_ctx, r,
				"failed to remove machine account from AD: %s",
				ads_errstr(ads_status));
		} else {
			r->out.deleted_machine_account = true;
			W_ERROR_HAVE_NO_MEMORY(r->out.dns_domain_name);
			libnet_join_unjoindomain_remove_secrets(mem_ctx, r);
			return WERR_OK;
		}
	}
#endif /* HAVE_ADS */

	/* The WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE flag really means 
	   "disable".  */
	if (r->in.unjoin_flags & WKSSVC_JOIN_FLAGS_ACCOUNT_DELETE) {
		status = libnet_join_unjoindomain_rpc(mem_ctx, r);
		if (!NT_STATUS_IS_OK(status)) {
			libnet_unjoin_set_error_string(mem_ctx, r,
				"failed to disable machine account via rpc: %s",
				get_friendly_nt_error_msg(status));
			if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
				return WERR_SETUP_NOT_JOINED;
			}
			return ntstatus_to_werror(status);
		}

		r->out.disabled_machine_account = true;
	}

	/* If disable succeeded or was not requested at all, we 
	   should be getting rid of our end of things */

	libnet_join_unjoindomain_remove_secrets(mem_ctx, r);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR libnet_unjoin_pre_processing(TALLOC_CTX *mem_ctx,
					   struct libnet_UnjoinCtx *r)
{
	if (!r->in.domain_name) {
		libnet_unjoin_set_error_string(mem_ctx, r,
			"No domain name defined");
		return WERR_INVALID_PARAM;
	}

	if (!libnet_parse_domain_dc(mem_ctx, r->in.domain_name,
				    &r->in.domain_name,
				    &r->in.dc_name)) {
		libnet_unjoin_set_error_string(mem_ctx, r,
			"Failed to parse domain name");
		return WERR_INVALID_PARAM;
	}

	if (IS_DC) {
		return WERR_SETUP_DOMAIN_CONTROLLER;
	}

	if (!secrets_init()) {
		libnet_unjoin_set_error_string(mem_ctx, r,
			"Unable to open secrets database");
		return WERR_CAN_NOT_COMPLETE;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR libnet_unjoin_post_processing(TALLOC_CTX *mem_ctx,
					    struct libnet_UnjoinCtx *r)
{
	saf_delete(r->out.netbios_domain_name);
	saf_delete(r->out.dns_domain_name);

	return libnet_unjoin_config(r);
}

/****************************************************************
****************************************************************/

WERROR libnet_Unjoin(TALLOC_CTX *mem_ctx,
		     struct libnet_UnjoinCtx *r)
{
	WERROR werr;

	if (r->in.debug) {
		LIBNET_UNJOIN_IN_DUMP_CTX(mem_ctx, r);
	}

	werr = libnet_unjoin_pre_processing(mem_ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	if (r->in.unjoin_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE) {
		werr = libnet_DomainUnjoin(mem_ctx, r);
		if (!W_ERROR_IS_OK(werr)) {
			libnet_unjoin_config(r);
			goto done;
		}
	}

	werr = libnet_unjoin_post_processing(mem_ctx, r);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

 done:
	r->out.result = werr;

	if (r->in.debug) {
		LIBNET_UNJOIN_OUT_DUMP_CTX(mem_ctx, r);
	}

	return werr;
}
