/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)
   Copyright (C) Tim Potter     2001
   Copyright (C) 2008 Guenther Deschner

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "rpc_client/cli_pipe.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/init_samr.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "rpc_client/cli_netlogon.h"
#include "secrets.h"
#include "rpc_client/init_lsa.h"
#include "libsmb/libsmb.h"

/* Macro for checking RPC error codes to make things more readable */

#define CHECK_RPC_ERR(rpc, msg) \
        if (!NT_STATUS_IS_OK(status = rpc)) { \
                DEBUG(0, (msg ": %s\n", nt_errstr(status))); \
                goto done; \
        }

#define CHECK_DCERPC_ERR(rpc, msg) \
	if (!NT_STATUS_IS_OK(status = rpc)) { \
		DEBUG(0, (msg ": %s\n", nt_errstr(status))); \
		goto done; \
	} \
	if (!NT_STATUS_IS_OK(result)) { \
		status = result; \
		DEBUG(0, (msg ": %s\n", nt_errstr(result))); \
		goto done; \
	}


#define CHECK_RPC_ERR_DEBUG(rpc, debug_args) \
        if (!NT_STATUS_IS_OK(status = rpc)) { \
                DEBUG(0, debug_args); \
                goto done; \
        }

#define CHECK_DCERPC_ERR_DEBUG(rpc, debug_args) \
	if (!NT_STATUS_IS_OK(status = rpc)) { \
		DEBUG(0, debug_args); \
		goto done; \
	} \
	if (!NT_STATUS_IS_OK(result)) { \
		status = result; \
		DEBUG(0, debug_args); \
		goto done; \
	}


/**
 * confirm that a domain join is still valid
 *
 * @return A shell status integer (0 for success)
 *
 **/
NTSTATUS net_rpc_join_ok(struct net_context *c, const char *domain,
			 const char *server, struct sockaddr_storage *pss)
{
	enum security_types sec;
	unsigned int conn_flags = NET_FLAGS_PDC;
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS ntret = NT_STATUS_UNSUCCESSFUL;

	sec = (enum security_types)lp_security();

	if (sec == SEC_ADS) {
		/* Connect to IPC$ using machine account's credentials. We don't use anonymous
		   connection here, as it may be denied by server's local policy. */
		net_use_machine_account(c);

	} else {
		/* some servers (e.g. WinNT) don't accept machine-authenticated
		   smb connections */
		conn_flags |= NET_FLAGS_ANONYMOUS;
	}

	/* Connect to remote machine */
	ntret = net_make_ipc_connection_ex(c, domain, server, pss, conn_flags,
					   &cli);
	if (!NT_STATUS_IS_OK(ntret)) {
		return ntret;
	}

	/* Setup the creds as though we're going to do schannel... */
	ntret = get_schannel_session_key(cli, domain, &neg_flags,
					 &netlogon_pipe);

	/* We return NT_STATUS_INVALID_NETWORK_RESPONSE if the server is refusing
	   to negotiate schannel, but the creds were set up ok. That'll have to do. */

        if (!NT_STATUS_IS_OK(ntret)) {
		if (NT_STATUS_EQUAL(ntret, NT_STATUS_INVALID_NETWORK_RESPONSE)) {
			cli_shutdown(cli);
			return NT_STATUS_OK;
		} else {
			DEBUG(0,("net_rpc_join_ok: failed to get schannel session "
					"key from server %s for domain %s. Error was %s\n",
				cli->desthost, domain, nt_errstr(ntret) ));
			cli_shutdown(cli);
			return ntret;
		}
	}

	/* Only do the rest of the schannel test if the client is allowed to do this. */
	if (!lp_client_schannel()) {
		cli_shutdown(cli);
		/* We're good... */
		return ntret;
	}

	ntret = cli_rpc_pipe_open_schannel_with_key(
		cli, &ndr_table_netlogon.syntax_id, NCACN_NP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		domain, &netlogon_pipe->dc, &pipe_hnd);

	if (!NT_STATUS_IS_OK(ntret)) {
		DEBUG(0,("net_rpc_join_ok: failed to open schannel session "
				"on netlogon pipe to server %s for domain %s. Error was %s\n",
			cli->desthost, domain, nt_errstr(ntret) ));
		/*
		 * Note: here, we have:
		 * (pipe_hnd != NULL) if and only if NT_STATUS_IS_OK(ntret)
		 */
	}

	cli_shutdown(cli);
	return ntret;
}

/**
 * Join a domain using the administrator username and password
 *
 * @param argc  Standard main() style argc
 * @param argc  Standard main() style argv.  Initial components are already
 *              stripped.  Currently not used.
 * @return A shell status integer (0 for success)
 *
 **/

int net_rpc_join_newstyle(struct net_context *c, int argc, const char **argv)
{

	/* libsmb variables */

	struct cli_state *cli;
	TALLOC_CTX *mem_ctx;
        uint32 acb_info = ACB_WSTRUST;
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	enum netr_SchannelType sec_channel_type;
	struct rpc_pipe_client *pipe_hnd = NULL;
	struct dcerpc_binding_handle *b = NULL;

	/* rpc variables */

	struct policy_handle lsa_pol, sam_pol, domain_pol, user_pol;
	struct dom_sid *domain_sid;
	uint32 user_rid;

	/* Password stuff */

	char *clear_trust_password = NULL;
	struct samr_CryptPassword crypt_pwd;
	uchar md4_trust_password[16];
	union samr_UserInfo set_info;

	/* Misc */

	NTSTATUS status, result;
	int retval = 1;
	const char *domain = NULL;
	char *acct_name;
	struct lsa_String lsa_acct_name;
	uint32 acct_flags=0;
	uint32_t access_granted = 0;
	union lsa_PolicyInformation *info = NULL;
	struct samr_Ids user_rids;
	struct samr_Ids name_types;


	/* check what type of join */
	if (argc >= 0) {
		sec_channel_type = get_sec_channel_type(argv[0]);
	} else {
		sec_channel_type = get_sec_channel_type(NULL);
	}

	switch (sec_channel_type) {
	case SEC_CHAN_WKSTA:
		acb_info = ACB_WSTRUST;
		break;
	case SEC_CHAN_BDC:
		acb_info = ACB_SVRTRUST;
		break;
#if 0
	case SEC_CHAN_DOMAIN:
		acb_info = ACB_DOMTRUST;
		break;
#endif
	default:
		DEBUG(0,("secure channel type %d not yet supported\n",
			sec_channel_type));
		break;
	}

	/* Make authenticated connection to remote machine */

	status = net_make_ipc_connection(c, NET_FLAGS_PDC, &cli);
	if (!NT_STATUS_IS_OK(status)) {
		return 1;
	}

	if (!(mem_ctx = talloc_init("net_rpc_join_newstyle"))) {
		DEBUG(0, ("Could not initialise talloc context\n"));
		goto done;
	}

	/* Fetch domain sid */

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_lsarpc.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Error connecting to LSA pipe. Error was %s\n",
			nt_errstr(status) ));
		goto done;
	}

	b = pipe_hnd->binding_handle;

	CHECK_RPC_ERR(rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					  SEC_FLAG_MAXIMUM_ALLOWED,
					  &lsa_pol),
		      "error opening lsa policy handle");

	CHECK_DCERPC_ERR(dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
						    &lsa_pol,
						    LSA_POLICY_INFO_ACCOUNT_DOMAIN,
						    &info,
						    &result),
		      "error querying info policy");

	domain = info->account_domain.name.string;
	domain_sid = info->account_domain.sid;

	dcerpc_lsa_Close(b, mem_ctx, &lsa_pol, &result);
	TALLOC_FREE(pipe_hnd); /* Done with this pipe */

	/* Bail out if domain didn't get set. */
	if (!domain) {
		DEBUG(0, ("Could not get domain name.\n"));
		goto done;
	}

	/* Create domain user */
	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_samr.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Error connecting to SAM pipe. Error was %s\n",
			nt_errstr(status) ));
		goto done;
	}

	b = pipe_hnd->binding_handle;

	CHECK_DCERPC_ERR(dcerpc_samr_Connect2(b, mem_ctx,
					      pipe_hnd->desthost,
					      SAMR_ACCESS_ENUM_DOMAINS
					      | SAMR_ACCESS_LOOKUP_DOMAIN,
					      &sam_pol,
					      &result),
		      "could not connect to SAM database");


	CHECK_DCERPC_ERR(dcerpc_samr_OpenDomain(b, mem_ctx,
						&sam_pol,
						SAMR_DOMAIN_ACCESS_LOOKUP_INFO_1
						| SAMR_DOMAIN_ACCESS_CREATE_USER
						| SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
						domain_sid,
						&domain_pol,
						&result),
		      "could not open domain");

	/* Create domain user */
	if ((acct_name = talloc_asprintf(mem_ctx, "%s$", global_myname())) == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	strlower_m(acct_name);

	init_lsa_String(&lsa_acct_name, acct_name);

	acct_flags = SEC_GENERIC_READ | SEC_GENERIC_WRITE | SEC_GENERIC_EXECUTE |
		     SEC_STD_WRITE_DAC | SEC_STD_DELETE |
		     SAMR_USER_ACCESS_SET_PASSWORD |
		     SAMR_USER_ACCESS_GET_ATTRIBUTES |
		     SAMR_USER_ACCESS_SET_ATTRIBUTES;

	DEBUG(10, ("Creating account with flags: %d\n",acct_flags));

	status = dcerpc_samr_CreateUser2(b, mem_ctx,
					 &domain_pol,
					 &lsa_acct_name,
					 acb_info,
					 acct_flags,
					 &user_pol,
					 &access_granted,
					 &user_rid,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, NT_STATUS_USER_EXISTS)) {
		status = result;
		d_fprintf(stderr,_("Creation of workstation account failed\n"));

		/* If NT_STATUS_ACCESS_DENIED then we have a valid
		   username/password combo but the user does not have
		   administrator access. */

		if (NT_STATUS_V(result) == NT_STATUS_V(NT_STATUS_ACCESS_DENIED))
			d_fprintf(stderr, _("User specified does not have "
					    "administrator privileges\n"));

		goto done;
	}

	/* We *must* do this.... don't ask... */

	if (NT_STATUS_IS_OK(result)) {
		dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
	}

	CHECK_DCERPC_ERR_DEBUG(dcerpc_samr_LookupNames(b, mem_ctx,
						       &domain_pol,
						       1,
						       &lsa_acct_name,
						       &user_rids,
						       &name_types,
						       &result),
			    ("error looking up rid for user %s: %s/%s\n",
			     acct_name, nt_errstr(status), nt_errstr(result)));

	if (user_rids.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}
	if (name_types.count != 1) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto done;
	}

	if (name_types.ids[0] != SID_NAME_USER) {
		DEBUG(0, ("%s is not a user account (type=%d)\n", acct_name, name_types.ids[0]));
		goto done;
	}

	user_rid = user_rids.ids[0];

	/* Open handle on user */

	CHECK_DCERPC_ERR_DEBUG(
		dcerpc_samr_OpenUser(b, mem_ctx,
				     &domain_pol,
				     SEC_FLAG_MAXIMUM_ALLOWED,
				     user_rid,
				     &user_pol,
				     &result),
		("could not re-open existing user %s: %s/%s\n",
		 acct_name, nt_errstr(status), nt_errstr(result)));
	
	/* Create a random machine account password */

	clear_trust_password = generate_random_str(talloc_tos(), DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);
	E_md4hash(clear_trust_password, md4_trust_password);

	/* Set password on machine account */

	init_samr_CryptPassword(clear_trust_password,
				&cli->user_session_key,
				&crypt_pwd);

	set_info.info24.password = crypt_pwd;
	set_info.info24.password_expired = PASS_DONT_CHANGE_AT_NEXT_LOGON;

	CHECK_DCERPC_ERR(dcerpc_samr_SetUserInfo2(b, mem_ctx,
						  &user_pol,
						  24,
						  &set_info,
						  &result),
		      "error setting trust account password");

	/* Why do we have to try to (re-)set the ACB to be the same as what
	   we passed in the samr_create_dom_user() call?  When a NT
	   workstation is joined to a domain by an administrator the
	   acb_info is set to 0x80.  For a normal user with "Add
	   workstations to the domain" rights the acb_info is 0x84.  I'm
	   not sure whether it is supposed to make a difference or not.  NT
	   seems to cope with either value so don't bomb out if the set
	   userinfo2 level 0x10 fails.  -tpot */

	set_info.info16.acct_flags = acb_info;

	/* Ignoring the return value is necessary for joining a domain
	   as a normal user with "Add workstation to domain" privilege. */

	status = dcerpc_samr_SetUserInfo(b, mem_ctx,
					 &user_pol,
					 16,
					 &set_info,
					 &result);

	dcerpc_samr_Close(b, mem_ctx, &user_pol, &result);
	TALLOC_FREE(pipe_hnd); /* Done with this pipe */

	/* Now check the whole process from top-to-bottom */

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Error connecting to NETLOGON pipe. Error was %s\n",
			nt_errstr(status) ));
		goto done;
	}

	status = rpccli_netlogon_setup_creds(pipe_hnd,
					cli->desthost, /* server name */
					domain,        /* domain */
					global_myname(), /* client name */
					global_myname(), /* machine account name */
                                        md4_trust_password,
                                        sec_channel_type,
                                        &neg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Error in domain join verification (credential setup failed): %s\n\n",
			  nt_errstr(status)));

		if ( NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) &&
		     (sec_channel_type == SEC_CHAN_BDC) ) {
			d_fprintf(stderr, _("Please make sure that no computer "
					    "account\nnamed like this machine "
					    "(%s) exists in the domain\n"),
				 global_myname());
		}

		goto done;
	}

	/* We can only check the schannel connection if the client is allowed
	   to do this and the server supports it. If not, just assume success
	   (after all the rpccli_netlogon_setup_creds() succeeded, and we'll
	   do the same again (setup creds) in net_rpc_join_ok(). JRA. */

	if (lp_client_schannel() && (neg_flags & NETLOGON_NEG_SCHANNEL)) {
		struct rpc_pipe_client *netlogon_schannel_pipe;

		status = cli_rpc_pipe_open_schannel_with_key(
			cli, &ndr_table_netlogon.syntax_id, NCACN_NP,
			DCERPC_AUTH_LEVEL_PRIVACY, domain, &pipe_hnd->dc,
			&netlogon_schannel_pipe);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Error in domain join verification (schannel setup failed): %s\n\n",
				  nt_errstr(status)));

			if ( NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) &&
			     (sec_channel_type == SEC_CHAN_BDC) ) {
				d_fprintf(stderr, _("Please make sure that no "
						    "computer account\nnamed "
						    "like this machine (%s) "
						    "exists in the domain\n"),
					 global_myname());
			}

			goto done;
		}
		TALLOC_FREE(netlogon_schannel_pipe);
	}

	TALLOC_FREE(pipe_hnd);

	/* Now store the secret in the secrets database */

	strupper_m(CONST_DISCARD(char *, domain));

	if (!secrets_store_domain_sid(domain, domain_sid)) {
		DEBUG(0, ("error storing domain sid for %s\n", domain));
		goto done;
	}

	if (!secrets_store_machine_password(clear_trust_password, domain, sec_channel_type)) {
		DEBUG(0, ("error storing plaintext domain secrets for %s\n", domain));
	}

	/* double-check, connection from scratch */
	status = net_rpc_join_ok(c, domain, cli->desthost, &cli->dest_ss);
	retval = NT_STATUS_IS_OK(status) ? 0 : -1;

done:

	/* Display success or failure */

	if (domain) {
		if (retval != 0) {
			fprintf(stderr,_("Unable to join domain %s.\n"),domain);
		} else {
			printf(_("Joined domain %s.\n"),domain);
		}
	}

	cli_shutdown(cli);

	TALLOC_FREE(clear_trust_password);

	return retval;
}

/**
 * check that a join is OK
 *
 * @return A shell status integer (0 for success)
 *
 **/
int net_rpc_testjoin(struct net_context *c, int argc, const char **argv)
{
	NTSTATUS nt_status;

	if (c->display_usage) {
		d_printf(_("Usage\n"
			   "net rpc testjoin\n"
			   "    Test if a join is OK\n"));
		return 0;
	}

	/* Display success or failure */
	nt_status = net_rpc_join_ok(c, c->opt_target_workgroup, NULL, NULL);
	if (!NT_STATUS_IS_OK(nt_status)) {
		fprintf(stderr, _("Join to domain '%s' is not valid: %s\n"),
			c->opt_target_workgroup, nt_errstr(nt_status));
		return -1;
	}

	printf(_("Join to '%s' is OK\n"), c->opt_target_workgroup);
	return 0;
}
