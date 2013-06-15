/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Tim Potter 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "rpcclient.h"

static NTSTATUS cmd_netlogon_logon_ctrl2(struct rpc_pipe_client *cli, 
                                         TALLOC_CTX *mem_ctx, int argc, 
                                         const char **argv)
{
	uint32 query_level = 1;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (argc > 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return NT_STATUS_OK;
	}

	result = rpccli_netlogon_logon_ctrl2(cli, mem_ctx, query_level);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	/* Display results */

 done:
	return result;
}

static WERROR cmd_netlogon_getanydcname(struct rpc_pipe_client *cli, 
					TALLOC_CTX *mem_ctx, int argc, 
					const char **argv)
{
	fstring dcname;
	WERROR result = WERR_GENERAL_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s domainname\n", argv[0]);
		return WERR_OK;
	}

	result = rpccli_netlogon_getanydcname(cli, mem_ctx, cli->cli->desthost, argv[1], dcname);

	if (!W_ERROR_IS_OK(result))
		goto done;

	/* Display results */

	printf("%s\n", dcname);

 done:
	return result;
}

static void display_ds_domain_controller_info(TALLOC_CTX *mem_ctx, const struct DS_DOMAIN_CONTROLLER_INFO *info)
{
	d_printf("domain_controller_name: %s\n", info->domain_controller_name);
	d_printf("domain_controller_address: %s\n", info->domain_controller_address);
	d_printf("domain_controller_address_type: %d\n", info->domain_controller_address_type);
	d_printf("domain_guid: %s\n", GUID_string(mem_ctx, info->domain_guid));
	d_printf("domain_name: %s\n", info->domain_name);
	d_printf("dns_forest_name: %s\n", info->dns_forest_name);
	d_printf("flags: 0x%08x\n"
		 "\tIs a PDC:                                   %s\n"
		 "\tIs a GC of the forest:                      %s\n"
		 "\tIs an LDAP server:                          %s\n"
		 "\tSupports DS:                                %s\n"
		 "\tIs running a KDC:                           %s\n"
		 "\tIs running time services:                   %s\n"
		 "\tIs the closest DC:                          %s\n"
		 "\tIs writable:                                %s\n"
		 "\tHas a hardware clock:                       %s\n"
		 "\tIs a non-domain NC serviced by LDAP server: %s\n"
		 "\tDomainControllerName is a DNS name:         %s\n"
		 "\tDomainName is a DNS name:                   %s\n"
		 "\tDnsForestName is a DNS name:                %s\n",
		 info->flags,
		 (info->flags & ADS_PDC) ? "yes" : "no",
		 (info->flags & ADS_GC) ? "yes" : "no",
		 (info->flags & ADS_LDAP) ? "yes" : "no",
		 (info->flags & ADS_DS) ? "yes" : "no",
		 (info->flags & ADS_KDC) ? "yes" : "no",
		 (info->flags & ADS_TIMESERV) ? "yes" : "no",
		 (info->flags & ADS_CLOSEST) ? "yes" : "no",
		 (info->flags & ADS_WRITABLE) ? "yes" : "no",
		 (info->flags & ADS_GOOD_TIMESERV) ? "yes" : "no",
		 (info->flags & ADS_NDNC) ? "yes" : "no",
		 (info->flags & ADS_DNS_CONTROLLER) ? "yes":"no",
		 (info->flags & ADS_DNS_DOMAIN) ? "yes":"no",
		 (info->flags & ADS_DNS_FOREST) ? "yes":"no");

	d_printf("dc_site_name: %s\n", info->dc_site_name);
	d_printf("client_site_name: %s\n", info->client_site_name);
}

static WERROR cmd_netlogon_dsr_getdcname(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx, int argc,
					 const char **argv)
{
	WERROR result;
	uint32 flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->cli->desthost;
	const char *domain_name;
	struct GUID domain_guid = GUID_zero();
	struct GUID site_guid = GUID_zero();
	struct DS_DOMAIN_CONTROLLER_INFO *info = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [domainname] [domain_name] [domain_guid] [site_guid] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2)
		domain_name = argv[1];

	if (argc >= 3) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[2], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 4) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[3], &site_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 5)
		sscanf(argv[4], "%x", &flags);
	
	result = rpccli_netlogon_dsr_getdcname(cli, mem_ctx, server_name, domain_name, 
					       &domain_guid, &site_guid, flags,
					       &info);

	if (W_ERROR_IS_OK(result)) {
		d_printf("DsGetDcName gave\n");
		display_ds_domain_controller_info(mem_ctx, info);
		return WERR_OK;
	}

	printf("rpccli_netlogon_dsr_getdcname returned %s\n",
	       dos_errstr(result));

	return result;
}

static WERROR cmd_netlogon_dsr_getdcnameex(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx, int argc,
					   const char **argv)
{
	WERROR result;
	uint32 flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->cli->desthost;
	const char *domain_name;
	const char *site_name = NULL;
	struct GUID domain_guid = GUID_zero();
	struct DS_DOMAIN_CONTROLLER_INFO *info = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [domainname] [domain_name] [domain_guid] [site_name] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2)
		domain_name = argv[1];

	if (argc >= 3) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[2], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 4)
		site_name = argv[3];

	if (argc >= 5)
		sscanf(argv[4], "%x", &flags);

	result = rpccli_netlogon_dsr_getdcnameex(cli, mem_ctx, server_name, domain_name, 
						 &domain_guid, site_name, flags,
						 &info);

	if (W_ERROR_IS_OK(result)) {
		d_printf("DsGetDcNameEx gave\n");
		display_ds_domain_controller_info(mem_ctx, info);
		return WERR_OK;
	}

	printf("rpccli_netlogon_dsr_getdcnameex returned %s\n",
	       dos_errstr(result));

	return result;
}

static WERROR cmd_netlogon_dsr_getdcnameex2(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx, int argc,
					    const char **argv)
{
	WERROR result;
	uint32 flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->cli->desthost;
	const char *domain_name;
	const char *client_account = NULL;
	uint32 mask = 0;
	const char *site_name = NULL;
	struct GUID domain_guid = GUID_zero();
	struct DS_DOMAIN_CONTROLLER_INFO *info = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [domainname] [client_account] [acb_mask] [domain_name] [domain_guid] [site_name] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2)
		client_account = argv[1];

	if (argc >= 3)
		mask = atoi(argv[2]);
	
	if (argc >= 4)
		domain_name = argv[3];

	if (argc >= 5) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[4], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 6)
		site_name = argv[5];

	if (argc >= 7)
		sscanf(argv[6], "%x", &flags);

	result = rpccli_netlogon_dsr_getdcnameex2(cli, mem_ctx, server_name, 
						  client_account, mask,
						  domain_name, &domain_guid,
						  site_name, flags,
						  &info);

	if (W_ERROR_IS_OK(result)) {
		d_printf("DsGetDcNameEx2 gave\n");
		display_ds_domain_controller_info(mem_ctx, info);
		return WERR_OK;
	}

	printf("rpccli_netlogon_dsr_getdcnameex2 returned %s\n",
	       dos_errstr(result));

	return result;
}


static WERROR cmd_netlogon_dsr_getsitename(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx, int argc,
					   const char **argv)
{
	WERROR result;
	char *sitename;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s computername\n", argv[0]);
		return WERR_OK;
	}

	result = rpccli_netlogon_dsr_getsitename(cli, mem_ctx, argv[1], &sitename);

	if (!W_ERROR_IS_OK(result)) {
		printf("rpccli_netlogon_dsr_gesitename returned %s\n",
		       nt_errstr(werror_to_ntstatus(result)));
		return result;
	}

	printf("Computer %s is on Site: %s\n", argv[1], sitename);

	return WERR_OK;
}

static NTSTATUS cmd_netlogon_logon_ctrl(struct rpc_pipe_client *cli, 
                                        TALLOC_CTX *mem_ctx, int argc, 
                                        const char **argv)
{
#if 0
	uint32 query_level = 1;
#endif
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

	if (argc > 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return NT_STATUS_OK;
	}

#if 0
	result = cli_netlogon_logon_ctrl(cli, mem_ctx, query_level);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}
#endif

	/* Display results */

	return result;
}

/* Display sam synchronisation information */

static void display_sam_sync(uint32 num_deltas, SAM_DELTA_HDR *hdr_deltas,
                             SAM_DELTA_CTR *deltas)
{
        fstring name;
        uint32 i, j;

        for (i = 0; i < num_deltas; i++) {
                switch (hdr_deltas[i].type) {
                case SAM_DELTA_DOMAIN_INFO:
                        unistr2_to_ascii(name,
                                         &deltas[i].domain_info.uni_dom_name,
                                         sizeof(name) - 1);
                        printf("Domain: %s\n", name);
                        break;
                case SAM_DELTA_GROUP_INFO:
                        unistr2_to_ascii(name,
                                         &deltas[i].group_info.uni_grp_name,
                                         sizeof(name) - 1);
                        printf("Group: %s\n", name);
                        break;
                case SAM_DELTA_ACCOUNT_INFO:
                        unistr2_to_ascii(name, 
                                         &deltas[i].account_info.uni_acct_name,
                                         sizeof(name) - 1);
                        printf("Account: %s\n", name);
                        break;
                case SAM_DELTA_ALIAS_INFO:
                        unistr2_to_ascii(name, 
                                         &deltas[i].alias_info.uni_als_name,
                                         sizeof(name) - 1);
                        printf("Alias: %s\n", name);
                        break;
                case SAM_DELTA_ALIAS_MEM: {
                        SAM_ALIAS_MEM_INFO *alias = &deltas[i].als_mem_info;

                        for (j = 0; j < alias->num_members; j++) {
                                fstring sid_str;

                                sid_to_string(sid_str, &alias->sids[j].sid);

                                printf("%s\n", sid_str);
                        }
                        break;
                }
                case SAM_DELTA_GROUP_MEM: {
                        SAM_GROUP_MEM_INFO *group = &deltas[i].grp_mem_info;

                        for (j = 0; j < group->num_members; j++)
                                printf("rid 0x%x, attrib 0x%08x\n", 
                                          group->rids[j], group->attribs[j]);
                        break;
                }
                case SAM_DELTA_MODIFIED_COUNT: {
                        SAM_DELTA_MOD_COUNT *mc = &deltas[i].mod_count;

                        printf("sam sequence update: 0x%04x\n", mc->seqnum);
                        break;
                }                                  
                default:
                        printf("unknown delta type 0x%02x\n", 
                                  hdr_deltas[i].type);
                        break;
                }
        }
}

/* Perform sam synchronisation */

static NTSTATUS cmd_netlogon_sam_sync(struct rpc_pipe_client *cli, 
                                      TALLOC_CTX *mem_ctx, int argc,
                                      const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
        uint32 database_id = 0, num_deltas;
        SAM_DELTA_HDR *hdr_deltas;
        SAM_DELTA_CTR *deltas;

        if (argc > 2) {
                fprintf(stderr, "Usage: %s [database_id]\n", argv[0]);
                return NT_STATUS_OK;
        }

        if (argc == 2)
                database_id = atoi(argv[1]);

        /* Synchronise sam database */

	result = rpccli_netlogon_sam_sync(cli, mem_ctx, database_id,
				       0, &num_deltas, &hdr_deltas, &deltas);

	if (!NT_STATUS_IS_OK(result))
		goto done;

        /* Display results */

        display_sam_sync(num_deltas, hdr_deltas, deltas);

 done:
        return result;
}

/* Perform sam delta synchronisation */

static NTSTATUS cmd_netlogon_sam_deltas(struct rpc_pipe_client *cli, 
                                        TALLOC_CTX *mem_ctx, int argc,
                                        const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
        uint32 database_id, num_deltas, tmp;
        SAM_DELTA_HDR *hdr_deltas;
        SAM_DELTA_CTR *deltas;
        uint64 seqnum;

        if (argc != 3) {
                fprintf(stderr, "Usage: %s database_id seqnum\n", argv[0]);
                return NT_STATUS_OK;
        }

        database_id = atoi(argv[1]);
        tmp = atoi(argv[2]);

        seqnum = tmp & 0xffff;

	result = rpccli_netlogon_sam_deltas(cli, mem_ctx, database_id,
					 seqnum, &num_deltas, 
					 &hdr_deltas, &deltas);

	if (!NT_STATUS_IS_OK(result))
		goto done;

        /* Display results */

        display_sam_sync(num_deltas, hdr_deltas, deltas);
        
 done:
        return result;
}

/* Log on a domain user */

static NTSTATUS cmd_netlogon_sam_logon(struct rpc_pipe_client *cli, 
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	int logon_type = NET_LOGON_TYPE;
	const char *username, *password;
	int auth_level = 2;
	uint32 logon_param = 0;
	const char *workstation = NULL;

	/* Check arguments */

	if (argc < 3 || argc > 7) {
		fprintf(stderr, "Usage: samlogon <username> <password> [workstation]"
			"[logon_type (1 or 2)] [auth level (2 or 3)] [logon_parameter]\n");
		return NT_STATUS_OK;
	}

	username = argv[1];
	password = argv[2];

	if (argc >= 4) 
		workstation = argv[3];

	if (argc >= 5)
		sscanf(argv[4], "%i", &logon_type);

	if (argc >= 6)
		sscanf(argv[5], "%i", &auth_level);

	if (argc == 7)
		sscanf(argv[6], "%x", &logon_param);

	/* Perform the sam logon */

	result = rpccli_netlogon_sam_logon(cli, mem_ctx, logon_param, lp_workgroup(), username, password, workstation, logon_type);

	if (!NT_STATUS_IS_OK(result))
		goto done;

 done:
	return result;
}

/* Change the trust account password */

static NTSTATUS cmd_netlogon_change_trust_pw(struct rpc_pipe_client *cli, 
					     TALLOC_CTX *mem_ctx, int argc,
					     const char **argv)
{
        NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

        /* Check arguments */

        if (argc > 1) {
                fprintf(stderr, "Usage: change_trust_pw");
                return NT_STATUS_OK;
        }

        /* Perform the sam logon */

	result = trust_pw_find_change_and_store_it(cli, mem_ctx,
						   lp_workgroup());

	if (!NT_STATUS_IS_OK(result))
		goto done;

 done:
        return result;
}


/* List of commands exported by this module */

struct cmd_set netlogon_commands[] = {

	{ "NETLOGON" },

	{ "logonctrl2", RPC_RTYPE_NTSTATUS, cmd_netlogon_logon_ctrl2, NULL, PI_NETLOGON, NULL, "Logon Control 2",     "" },
	{ "getanydcname", RPC_RTYPE_WERROR, NULL, cmd_netlogon_getanydcname, PI_NETLOGON, NULL, "Get trusted DC name",     "" },
	{ "dsr_getdcname", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcname, PI_NETLOGON, NULL, "Get trusted DC name",     "" },
	{ "dsr_getdcnameex", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcnameex, PI_NETLOGON, NULL, "Get trusted DC name",     "" },
	{ "dsr_getdcnameex2", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcnameex2, PI_NETLOGON, NULL, "Get trusted DC name",     "" },
	{ "dsr_getsitename", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getsitename, PI_NETLOGON, NULL, "Get sitename",     "" },
	{ "logonctrl",  RPC_RTYPE_NTSTATUS, cmd_netlogon_logon_ctrl,  NULL, PI_NETLOGON, NULL, "Logon Control",       "" },
	{ "samsync",    RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_sync,    NULL, PI_NETLOGON, NULL, "Sam Synchronisation", "" },
	{ "samdeltas",  RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_deltas,  NULL, PI_NETLOGON, NULL, "Query Sam Deltas",    "" },
	{ "samlogon",   RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_logon,   NULL, PI_NETLOGON, NULL, "Sam Logon",           "" },
	{ "change_trust_pw",   RPC_RTYPE_NTSTATUS, cmd_netlogon_change_trust_pw,   NULL, PI_NETLOGON, NULL, "Change Trust Account Password",           "" },

	{ NULL }
};
