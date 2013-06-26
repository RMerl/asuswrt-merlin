/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2011 Sumit Bose (sbose@redhat.com)

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
#include "rpc_client/cli_lsarpc.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/security/dom_sid.h"
#include "libsmb/libsmb.h"


#define ARG_OTHERSERVER "otherserver="
#define ARG_OTHERUSER "otheruser="
#define ARG_OTHERDOMAINSID "otherdomainsid="
#define ARG_OTHERDOMAIN "otherdomain="
#define ARG_OTHERNETBIOSDOMAIN "other_netbios_domain="
#define ARG_TRUSTPW "trustpw="

enum trust_op {
	TRUST_CREATE,
	TRUST_DELETE
};

struct other_dom_data {
	char *host;
	char *user_name;
	char *domain_sid_str;
	char *dns_domain_name;
	char *domain_name;
};

struct dom_data {
	struct dom_sid *domsid;
	char *dns_domain_name;
	char *domain_name;
};

static NTSTATUS close_handle(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *bind_hnd,
			     struct policy_handle *pol_hnd)
{
	NTSTATUS status;
	NTSTATUS result;

	status = dcerpc_lsa_Close(bind_hnd, mem_ctx, pol_hnd, &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_lsa_Close failed with error [%s].\n",
			  nt_errstr(status)));
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0, ("lsa close failed with error [%s].\n",
			  nt_errstr(result)));
		return result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS delete_trust(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *bind_hnd,
			     struct policy_handle *pol_hnd,
			     struct dom_sid *domsid)
{
	NTSTATUS status;
	struct lsa_DeleteTrustedDomain dr;

	dr.in.handle = pol_hnd;
	dr.in.dom_sid = domsid;

	status = dcerpc_lsa_DeleteTrustedDomain_r(bind_hnd, mem_ctx, &dr);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_lsa_DeleteTrustedDomain_r failed with [%s]\n",
			  nt_errstr(status)));
		return status;
	}
	if (!NT_STATUS_IS_OK(dr.out.result)) {
		DEBUG(0, ("DeleteTrustedDomain returned [%s]\n",
			  nt_errstr(dr.out.result)));
		return dr.out.result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS create_trust(TALLOC_CTX *mem_ctx,
			     struct dcerpc_binding_handle *bind_hnd,
			     struct policy_handle *pol_hnd,
			     const char *trust_name,
			     const char *trust_name_dns,
			     struct dom_sid *domsid,
			     struct lsa_TrustDomainInfoAuthInfoInternal *authinfo)
{
	NTSTATUS status;
	struct lsa_CreateTrustedDomainEx2 r;
	struct lsa_TrustDomainInfoInfoEx trustinfo;
	struct policy_handle trustdom_handle;

	trustinfo.sid = domsid;
	trustinfo.netbios_name.string = trust_name;
	trustinfo.domain_name.string = trust_name_dns;

	trustinfo.trust_direction = LSA_TRUST_DIRECTION_INBOUND |
				    LSA_TRUST_DIRECTION_OUTBOUND;

	trustinfo.trust_type = LSA_TRUST_TYPE_UPLEVEL;

	trustinfo.trust_attributes = LSA_TRUST_ATTRIBUTE_FOREST_TRANSITIVE;

	r.in.policy_handle = pol_hnd;
	r.in.info = &trustinfo;
	r.in.auth_info = authinfo;
	r.in.access_mask = LSA_TRUSTED_SET_POSIX | LSA_TRUSTED_SET_AUTH |
			   LSA_TRUSTED_QUERY_DOMAIN_NAME;
	r.out.trustdom_handle = &trustdom_handle;

	status = dcerpc_lsa_CreateTrustedDomainEx2_r(bind_hnd, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_lsa_CreateTrustedDomainEx2_r failed "
			  "with error [%s].\n", nt_errstr(status)));
		return status;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		DEBUG(0, ("CreateTrustedDomainEx2_r returned [%s].\n",
			  nt_errstr(r.out.result)));
		return r.out.result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS get_domain_info(TALLOC_CTX *mem_ctx,
				struct dcerpc_binding_handle *bind_hdn,
				struct policy_handle *pol_hnd,
				struct dom_data *dom_data)
{
	NTSTATUS status;
	struct lsa_QueryInfoPolicy2 qr;

	qr.in.handle = pol_hnd;
	qr.in.level = LSA_POLICY_INFO_DNS;

	status = dcerpc_lsa_QueryInfoPolicy2_r(bind_hdn, mem_ctx, &qr);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("dcerpc_lsa_QueryInfoPolicy2_r failed "
			  "with error [%s].\n", nt_errstr(status)));
		return status;
	}

	if (!NT_STATUS_IS_OK(qr.out.result)) {
		DEBUG(0, ("QueryInfoPolicy2 returned [%s].\n",
			  nt_errstr(qr.out.result)));
		return qr.out.result;
	}

	dom_data->domain_name = talloc_strdup(mem_ctx,
					      (*qr.out.info)->dns.name.string);
	dom_data->dns_domain_name = talloc_strdup(mem_ctx,
					 (*qr.out.info)->dns.dns_domain.string);
	dom_data->domsid = dom_sid_dup(mem_ctx, (*qr.out.info)->dns.sid);
	if (dom_data->domain_name == NULL ||
	    dom_data->dns_domain_name == NULL ||
	    dom_data->domsid == NULL) {
		DEBUG(0, ("Copying domain data failed.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(0, ("Got the following domain info [%s][%s][%s].\n",
		  dom_data->domain_name, dom_data->dns_domain_name,
		  sid_string_talloc(mem_ctx, dom_data->domsid)));

	return NT_STATUS_OK;
}

static NTSTATUS connect_and_get_info(TALLOC_CTX *mem_ctx,
				     struct net_context *net_ctx,
				     struct cli_state **cli,
				     struct rpc_pipe_client **pipe_hnd,
				     struct policy_handle *pol_hnd,
				     struct dom_data *dom_data)
{
	NTSTATUS status;
	NTSTATUS result;

	status = net_make_ipc_connection_ex(net_ctx, NULL, NULL, NULL,
					    NET_FLAGS_PDC, cli);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to connect to [%s] with error [%s]\n",
			  net_ctx->opt_host, nt_errstr(status)));
		return status;
	}

	status = cli_rpc_pipe_open_noauth(*cli, &ndr_table_lsarpc.syntax_id, pipe_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to initialise lsa pipe with error [%s]\n",
			  nt_errstr(status)));
		return status;
	}

	status = dcerpc_lsa_open_policy2((*pipe_hnd)->binding_handle,
					 mem_ctx,
					 (*pipe_hnd)->srv_name_slash,
					 false,
					 (LSA_POLICY_VIEW_LOCAL_INFORMATION |
					  LSA_POLICY_TRUST_ADMIN |
					  LSA_POLICY_CREATE_SECRET),
					 pol_hnd,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to open policy handle with error [%s]\n",
			  nt_errstr(status)));
		return status;
	}
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(0, ("lsa_open_policy2 with error [%s]\n",
			  nt_errstr(result)));
		return result;
	}

	status = get_domain_info(mem_ctx, (*pipe_hnd)->binding_handle,
				 pol_hnd, dom_data);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("get_domain_info failed with error [%s].\n",
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

static bool get_trust_domain_passwords_auth_blob(TALLOC_CTX *mem_ctx,
						 const char *password,
						 DATA_BLOB *auth_blob)
{
	struct trustDomainPasswords auth_struct;
	struct AuthenticationInformation *auth_info_array;
	enum ndr_err_code ndr_err;
	size_t converted_size;

	generate_random_buffer(auth_struct.confounder,
			       sizeof(auth_struct.confounder));

	auth_info_array = talloc_array(mem_ctx,
				       struct AuthenticationInformation, 1);
	if (auth_info_array == NULL) {
		return false;
	}

	auth_info_array[0].AuthType = TRUST_AUTH_TYPE_CLEAR;
	if (!convert_string_talloc(mem_ctx, CH_UNIX, CH_UTF16, password,
				  strlen(password),
				  &auth_info_array[0].AuthInfo.clear.password,
				  &converted_size, true)) {
		return false;
	}

	auth_info_array[0].AuthInfo.clear.size = converted_size;

	auth_struct.outgoing.count = 1;
	auth_struct.outgoing.current.count = 1;
	auth_struct.outgoing.current.array = auth_info_array;
	auth_struct.outgoing.previous.count = 0;
	auth_struct.outgoing.previous.array = NULL;

	auth_struct.incoming.count = 1;
	auth_struct.incoming.current.count = 1;
	auth_struct.incoming.current.array = auth_info_array;
	auth_struct.incoming.previous.count = 0;
	auth_struct.incoming.previous.array = NULL;

	ndr_err = ndr_push_struct_blob(auth_blob, mem_ctx, &auth_struct,
				       (ndr_push_flags_fn_t)ndr_push_trustDomainPasswords);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}

	return true;
}

static int parse_trust_args(TALLOC_CTX *mem_ctx, int argc, const char **argv, struct other_dom_data **_o, char **_trustpw)
{
	size_t c;
	struct other_dom_data *o = NULL;
	char *trustpw = NULL;
	int ret = EFAULT;

	if (argc == 0) {
		return EINVAL;
	}

	o = talloc_zero(mem_ctx, struct other_dom_data);
	if (o == NULL) {
		DEBUG(0, ("talloc_zero failed.\n"));
		return ENOMEM;
	}

	for (c = 0; c < argc; c++) {
		if (strnequal(argv[c], ARG_OTHERSERVER, sizeof(ARG_OTHERSERVER)-1)) {
			o->host = talloc_strdup(o, argv[c] + sizeof(ARG_OTHERSERVER)-1);
			if (o->host == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else if (strnequal(argv[c], ARG_OTHERUSER, sizeof(ARG_OTHERUSER)-1)) {
			o->user_name = talloc_strdup(o, argv[c] + sizeof(ARG_OTHERUSER)-1);
			if (o->user_name == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else if (strnequal(argv[c], ARG_OTHERDOMAINSID, sizeof(ARG_OTHERDOMAINSID)-1)) {
			o->domain_sid_str = talloc_strdup(o, argv[c] + sizeof(ARG_OTHERDOMAINSID)-1);
			if (o->domain_sid_str == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else if (strnequal(argv[c], ARG_OTHERDOMAIN, sizeof(ARG_OTHERDOMAIN)-1)) {
			o->dns_domain_name = talloc_strdup(o, argv[c] + sizeof(ARG_OTHERDOMAIN)-1);
			if (o->dns_domain_name == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else if (strnequal(argv[c], ARG_OTHERNETBIOSDOMAIN, sizeof(ARG_OTHERNETBIOSDOMAIN)-1)) {
			o->domain_name = talloc_strdup(o, argv[c] + sizeof(ARG_OTHERNETBIOSDOMAIN)-1);
			if (o->domain_name == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else if (strnequal(argv[c], ARG_TRUSTPW, sizeof(ARG_TRUSTPW)-1)) {
			trustpw = talloc_strdup(mem_ctx, argv[c] + sizeof(ARG_TRUSTPW)-1);
			if (trustpw == NULL) {
				ret = ENOMEM;
				goto failed;
			}
		} else {
			DEBUG(0, ("Unsupported option [%s].\n", argv[c]));
			ret = EINVAL;
			goto failed;
		}
	}

	*_o = o;
	*_trustpw = trustpw;

	return 0;

failed:
	talloc_free(o);
	talloc_free(trustpw);
	return ret;
}

static void print_trust_delete_usage(void)
{
	d_printf(  "%s\n"
		   "net rpc trust delete [options]\n"
		   "\nOptions:\n"
		   "\totherserver=DC in other domain\n"
		   "\totheruser=Admin user in other domain\n"
		   "\totherdomainsid=SID of other domain\n"
		   "\nExamples:\n"
		   "\tnet rpc trust delete otherserver=oname otheruser=ouser -S lname -U luser\n"
		   "\tnet rpc trust delete otherdomainsid=S-... -S lname -U luser\n"
		   "  %s\n",
		 _("Usage:"),
		 _("Remove trust between two domains"));
}

static void print_trust_usage(void)
{
	d_printf(  "%s\n"
		   "net rpc trust create [options]\n"
		   "\nOptions:\n"
		   "\totherserver=DC in other domain\n"
		   "\totheruser=Admin user in other domain\n"
		   "\totherdomainsid=SID of other domain\n"
		   "\tother_netbios_domain=NetBIOS/short name of other domain\n"
		   "\totherdomain=Full/DNS name of other domain\n"
		   "\ttrustpw=Trust password\n"
		   "\nExamples:\n"
		   "\tnet rpc trust create otherserver=oname otheruser=ouser -S lname -U luser\n"
		   "\tnet rpc trust create otherdomainsid=S-... other_netbios_domain=odom otherdomain=odom.org trustpw=secret -S lname -U luser\n"
		   "  %s\n",
		 _("Usage:"),
		 _("Create trust between two domains"));
}

static int rpc_trust_common(struct net_context *net_ctx, int argc,
			    const char **argv, enum trust_op op)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	int ret;
	int success = -1;
	struct cli_state *cli[2] = {NULL, NULL};
	struct rpc_pipe_client *pipe_hnd[2] = {NULL, NULL};
	struct policy_handle pol_hnd[2];
	struct lsa_TrustDomainInfoAuthInfoInternal authinfo;
	DATA_BLOB auth_blob;
	char *trust_pw = NULL;
	struct other_dom_data *other_dom_data;
	struct net_context *other_net_ctx = NULL;
	struct dom_data dom_data[2];
	void (*usage)(void);

	switch (op) {
		case TRUST_CREATE:
			usage = print_trust_usage;
			break;
		case TRUST_DELETE:
			usage = print_trust_delete_usage;
			break;
		default:
			DEBUG(0, ("Unsupported trust operation.\n"));
			return -1;
	}

	if (net_ctx->display_usage) {
		usage();
		return 0;
	}

	mem_ctx = talloc_init("trust op");
	if (mem_ctx == NULL) {
		DEBUG(0, ("talloc_init failed.\n"));
		return -1;
	}

	ret = parse_trust_args(mem_ctx, argc, argv, &other_dom_data, &trust_pw);
	if (ret != 0) {
		if (ret == EINVAL) {
			usage();
		} else {
			DEBUG(0, ("Failed to parse arguments.\n"));
		}
		goto done;
	}

	if (other_dom_data->host != 0) {
		other_net_ctx = talloc_zero(other_dom_data, struct net_context);
		if (other_net_ctx == NULL) {
			DEBUG(0, ("talloc_zero failed.\n"));
			goto done;
		}

		other_net_ctx->opt_host = other_dom_data->host;
		other_net_ctx->opt_user_name = other_dom_data->user_name;
	} else {
		dom_data[1].domsid = dom_sid_parse_talloc(mem_ctx,
						other_dom_data->domain_sid_str);
		dom_data[1].domain_name = other_dom_data->domain_name;
		dom_data[1].dns_domain_name = other_dom_data->dns_domain_name;

		if (dom_data[1].domsid == NULL ||
		    (op == TRUST_CREATE &&
		     (dom_data[1].domain_name == NULL ||
		      dom_data[1].dns_domain_name == NULL))) {
			DEBUG(0, ("Missing required argument.\n"));
			usage();
			goto done;
		}
	}

	status = connect_and_get_info(mem_ctx, net_ctx, &cli[0], &pipe_hnd[0],
				      &pol_hnd[0], &dom_data[0]);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("connect_and_get_info failed with error [%s]\n",
			  nt_errstr(status)));
		goto done;
	}

	if (other_net_ctx != NULL) {
		status = connect_and_get_info(mem_ctx, other_net_ctx,
					      &cli[1], &pipe_hnd[1],
					      &pol_hnd[1], &dom_data[1]);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("connect_and_get_info failed with error [%s]\n",
				  nt_errstr(status)));
			goto done;
		}
	}

	if (op == TRUST_CREATE) {
		if (trust_pw == NULL) {
			if (other_net_ctx == NULL) {
				DEBUG(0, ("Missing either trustpw or otherhost.\n"));
				goto done;
			}

			DEBUG(0, ("Using random trust password.\n"));
	/* FIXME: why only 8 characters work? Would it be possible to use a
	 * random binary password? */
			trust_pw = generate_random_str(mem_ctx, 8);
			if (trust_pw == NULL) {
				DEBUG(0, ("generate_random_str failed.\n"));
				goto done;
			}
		} else {
			DEBUG(0, ("Using user provided password.\n"));
		}

		if (!get_trust_domain_passwords_auth_blob(mem_ctx, trust_pw,
							  &auth_blob)) {
			DEBUG(0, ("get_trust_domain_passwords_auth_blob failed\n"));
			goto done;
		}

		authinfo.auth_blob.data = (uint8_t *)talloc_memdup(
							mem_ctx,
							auth_blob.data,
							auth_blob.length);
		if (authinfo.auth_blob.data == NULL) {
			goto done;
		}
		authinfo.auth_blob.size = auth_blob.length;

		arcfour_crypt_blob(authinfo.auth_blob.data,
				   authinfo.auth_blob.size,
				   &cli[0]->user_session_key);

		status = create_trust(mem_ctx, pipe_hnd[0]->binding_handle,
				      &pol_hnd[0],
				      dom_data[1].domain_name,
				      dom_data[1].dns_domain_name,
				      dom_data[1].domsid,
				      &authinfo);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("create_trust failed with error [%s].\n",
			nt_errstr(status)));
			goto done;
		}

		if (other_net_ctx != NULL) {
			talloc_free(authinfo.auth_blob.data);
			authinfo.auth_blob.data = (uint8_t *)talloc_memdup(
								mem_ctx,
								auth_blob.data,
								auth_blob.length);
			if (authinfo.auth_blob.data == NULL) {
				goto done;
			}
			authinfo.auth_blob.size = auth_blob.length;

			arcfour_crypt_blob(authinfo.auth_blob.data,
					   authinfo.auth_blob.size,
					   &cli[1]->user_session_key);

			status = create_trust(mem_ctx,
					      pipe_hnd[1]->binding_handle,
					      &pol_hnd[1],
					      dom_data[0].domain_name,
					      dom_data[0].dns_domain_name,
					      dom_data[0].domsid, &authinfo);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("create_trust failed with error [%s].\n",
				nt_errstr(status)));
				goto done;
			}
		}
	} else if (op == TRUST_DELETE) {
		status = delete_trust(mem_ctx, pipe_hnd[0]->binding_handle,
				      &pol_hnd[0], dom_data[1].domsid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("delete_trust failed with [%s].\n",
				  nt_errstr(status)));
			goto done;
		}

		if (other_net_ctx != NULL) {
			status = delete_trust(mem_ctx,
					      pipe_hnd[1]->binding_handle,
					      &pol_hnd[1], dom_data[0].domsid);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("delete_trust failed with [%s].\n",
					  nt_errstr(status)));
				goto done;
			}
		}
	}

	status = close_handle(mem_ctx, pipe_hnd[0]->binding_handle,
			      &pol_hnd[0]);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("close_handle failed with error [%s].\n",
			  nt_errstr(status)));
		goto done;
	}

	if (other_net_ctx != NULL) {
		status = close_handle(mem_ctx, pipe_hnd[1]->binding_handle,
				      &pol_hnd[1]);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("close_handle failed with error [%s].\n",
				  nt_errstr(status)));
			goto done;
		}
	}

	success = 0;

done:
	cli_shutdown(cli[0]);
	cli_shutdown(cli[1]);
	talloc_destroy(mem_ctx);
	return success;
}

static int rpc_trust_create(struct net_context *net_ctx, int argc,
			    const char **argv)
{
	return rpc_trust_common(net_ctx, argc, argv, TRUST_CREATE);
}

static int rpc_trust_delete(struct net_context *net_ctx, int argc,
			    const char **argv)
{
	return rpc_trust_common(net_ctx, argc, argv, TRUST_DELETE);
}

int net_rpc_trust(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"create",
			rpc_trust_create,
			NET_TRANSPORT_RPC,
			N_("Create trusts"),
			N_("net rpc trust create\n"
			   "    Create trusts")
		},
		{
			"delete",
			rpc_trust_delete,
			NET_TRANSPORT_RPC,
			N_("Remove trusts"),
			N_("net rpc trust delete\n"
			   "    Remove trusts")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc trust", func);
}
