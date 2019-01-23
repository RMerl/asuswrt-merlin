/*
 *  Unix SMB/CIFS implementation.
 *  Routines to operate on various trust relationships
 *  Copyright (C) Andrew Bartlett                   2001
 *  Copyright (C) Rafal Szczesniak                  2003
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
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "rpc_client/cli_netlogon.h"
#include "rpc_client/cli_pipe.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "secrets.h"
#include "passdb.h"
#include "libsmb/libsmb.h"

/*********************************************************
 Change the domain password on the PDC.
 Store the password ourselves, but use the supplied password
 Caller must have already setup the connection to the NETLOGON pipe
**********************************************************/

NTSTATUS trust_pw_change_and_store_it(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, 
				      const char *domain,
				      const char *account_name,
				      unsigned char orig_trust_passwd_hash[16],
				      enum netr_SchannelType sec_channel_type)
{
	unsigned char new_trust_passwd_hash[16];
	char *new_trust_passwd;
	NTSTATUS nt_status;

	switch (sec_channel_type) {
#ifdef NETLOGON_SUPPORT
	case SEC_CHAN_WKSTA:
	case SEC_CHAN_DOMAIN:
		break;
#endif
	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	/* Create a random machine account password */
	new_trust_passwd = generate_random_str(mem_ctx, DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);

	if (new_trust_passwd == NULL) {
		DEBUG(0, ("talloc_strdup failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	E_md4hash(new_trust_passwd, new_trust_passwd_hash);

	nt_status = rpccli_netlogon_set_trust_password(cli, mem_ctx,
						       account_name,
						       orig_trust_passwd_hash,
						       new_trust_passwd,
						       new_trust_passwd_hash,
						       sec_channel_type);

	if (NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("%s : trust_pw_change_and_store_it: Changed password.\n", 
			 current_timestring(talloc_tos(), False)));
		/*
		 * Return the result of trying to write the new password
		 * back into the trust account file.
		 */

		switch (sec_channel_type) {

		case SEC_CHAN_WKSTA:
			if (!secrets_store_machine_password(new_trust_passwd, domain, sec_channel_type)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
			break;

		case SEC_CHAN_DOMAIN: {
			char *pwd;
			struct dom_sid sid;
			time_t pass_last_set_time;

			/* we need to get the sid first for the
			 * pdb_set_trusteddom_pw call */

			if (!pdb_get_trusteddom_pw(domain, &pwd, &sid, &pass_last_set_time)) {
				nt_status = NT_STATUS_TRUSTED_RELATIONSHIP_FAILURE;
			}
			if (!pdb_set_trusteddom_pw(domain, new_trust_passwd, &sid)) {
				nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
			break;
		}
		default:
			break;
		}
	}

	return nt_status;
}

/*********************************************************
 Change the domain password on the PDC.
 Do most of the legwork ourselfs.  Caller must have
 already setup the connection to the NETLOGON pipe
**********************************************************/

NTSTATUS trust_pw_find_change_and_store_it(struct rpc_pipe_client *cli, 
					   TALLOC_CTX *mem_ctx, 
					   const char *domain) 
{
	unsigned char old_trust_passwd_hash[16];
	enum netr_SchannelType sec_channel_type = SEC_CHAN_NULL;
	const char *account_name;

	if (!get_trust_pw_hash(domain, old_trust_passwd_hash, &account_name,
			       &sec_channel_type)) {
		DEBUG(0, ("could not fetch domain secrets for domain %s!\n", domain));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return trust_pw_change_and_store_it(cli, mem_ctx, domain,
					    account_name,
					    old_trust_passwd_hash,
					    sec_channel_type);
}

/*********************************************************************
 Enumerate the list of trusted domains from a DC
*********************************************************************/

bool enumerate_domain_trusts( TALLOC_CTX *mem_ctx, const char *domain,
                                     char ***domain_names, uint32 *num_domains,
				     struct dom_sid **sids )
{
	struct policy_handle 	pol;
	NTSTATUS status, result;
	fstring 	dc_name;
	struct sockaddr_storage	dc_ss;
	uint32 		enum_ctx = 0;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *lsa_pipe = NULL;
	struct lsa_DomainList dom_list;
	int i;
	struct dcerpc_binding_handle *b = NULL;

	*domain_names = NULL;
	*num_domains = 0;
	*sids = NULL;

#ifndef NETLOGON_SUPPORT
	return False;
#endif


	/* lookup a DC first */

	if ( !get_dc_name(domain, NULL, dc_name, &dc_ss) ) {
		DEBUG(3,("enumerate_domain_trusts: can't locate a DC for domain %s\n",
			domain));
		return False;
	}

	/* setup the anonymous connection */

	status = cli_full_connection( &cli, global_myname(), dc_name, &dc_ss, 0, "IPC$", "IPC",
		"", "", "", 0, Undefined);
	if ( !NT_STATUS_IS_OK(status) )
		goto done;

	/* open the LSARPC_PIPE	*/

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					  &lsa_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	b = lsa_pipe->binding_handle;

	/* get a handle */

	status = rpccli_lsa_open_policy(lsa_pipe, mem_ctx, True,
		LSA_POLICY_VIEW_LOCAL_INFORMATION, &pol);
	if ( !NT_STATUS_IS_OK(status) )
		goto done;

	/* Lookup list of trusted domains */

	status = dcerpc_lsa_EnumTrustDom(b, mem_ctx,
					 &pol,
					 &enum_ctx,
					 &dom_list,
					 (uint32_t)-1,
					 &result);
	if ( !NT_STATUS_IS_OK(status) )
		goto done;
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	*num_domains = dom_list.count;

	*domain_names = TALLOC_ZERO_ARRAY(mem_ctx, char *, *num_domains);
	if (!*domain_names) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	*sids = TALLOC_ZERO_ARRAY(mem_ctx, struct dom_sid, *num_domains);
	if (!*sids) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i< *num_domains; i++) {
		(*domain_names)[i] = CONST_DISCARD(char *, dom_list.domains[i].name.string);
		(*sids)[i] = *dom_list.domains[i].sid;
	}

done:
	/* cleanup */
	if (cli) {
		DEBUG(10,("enumerate_domain_trusts: shutting down connection...\n"));
		cli_shutdown( cli );
	}

	return NT_STATUS_IS_OK(status);
}

NTSTATUS change_trust_account_password( const char *domain, const char *remote_machine)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct sockaddr_storage pdc_ss;
	fstring dc_name;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *netlogon_pipe = NULL;

#ifndef NETLOGON_SUPPORT
	return NT_STATUS_UNSUCCESSFUL;
#endif

	DEBUG(5,("change_trust_account_password: Attempting to change trust account password in domain %s....\n",
		domain));

	if (remote_machine == NULL || !strcmp(remote_machine, "*")) {
		/* Use the PDC *only* for this */

		if ( !get_pdc_ip(domain, &pdc_ss) ) {
			DEBUG(0,("Can't get IP for PDC for domain %s\n", domain));
			goto failed;
		}

		if ( !name_status_find( domain, 0x1b, 0x20, &pdc_ss, dc_name) )
			goto failed;
	} else {
		/* supoport old deprecated "smbpasswd -j DOMAIN -r MACHINE" behavior */
		fstrcpy( dc_name, remote_machine );
	}

	/* if this next call fails, then give up.  We can't do
	   password changes on BDC's  --jerry */

	if (!NT_STATUS_IS_OK(cli_full_connection(&cli, global_myname(), dc_name,
					   NULL, 0,
					   "IPC$", "IPC",
					   "", "",
					   "", 0, Undefined))) {
		DEBUG(0,("modify_trust_password: Connection to %s failed!\n", dc_name));
		nt_status = NT_STATUS_UNSUCCESSFUL;
		goto failed;
	}

	/*
	 * Ok - we have an anonymous connection to the IPC$ share.
	 * Now start the NT Domain stuff :-).
	 */

	/* Shouldn't we open this with schannel ? JRA. */

	nt_status = cli_rpc_pipe_open_noauth(
		cli, &ndr_table_netlogon.syntax_id, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("modify_trust_password: unable to open the domain client session to machine %s. Error was : %s.\n",
			dc_name, nt_errstr(nt_status)));
		cli_shutdown(cli);
		cli = NULL;
		goto failed;
	}

	nt_status = trust_pw_find_change_and_store_it(
		netlogon_pipe, netlogon_pipe, domain);

	cli_shutdown(cli);
	cli = NULL;

failed:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("%s : change_trust_account_password: Failed to change password for domain %s.\n",
			current_timestring(talloc_tos(), False), domain));
	}
	else
		DEBUG(5,("change_trust_account_password: sucess!\n"));

	return nt_status;
}
