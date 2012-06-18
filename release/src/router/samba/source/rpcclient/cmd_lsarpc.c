/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NT Domain Authentication SMB / MSRPC client
   Copyright (C) Andrew Tridgell 1994-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   
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



#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

#define DEBUG_TESTING

extern struct cli_state *smb_cli;
extern int smb_tidx;

extern FILE* out_hnd;


/****************************************************************************
nt lsa query
****************************************************************************/
void cmd_lsa_query_info(struct client_info *info)
{
	fstring srv_name;

	BOOL res = True;

	fstrcpy(info->dom.level3_dom, "");
	fstrcpy(info->dom.level5_dom, "");
	ZERO_STRUCT(info->dom.level3_sid);
	ZERO_STRUCT(info->dom.level5_sid);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->myhostname);
	strupper(srv_name);

	DEBUG(4,("cmd_lsa_query_info: server:%s\n", srv_name));

	DEBUG(5, ("cmd_lsa_query_info: smb_cli->fd:%d\n", smb_cli->fd));

	/* open LSARPC session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_LSARPC) : False;

	/* lookup domain controller; receive a policy handle */
	res = res ? do_lsa_open_policy(smb_cli,
				srv_name,
				&info->dom.lsa_info_pol, False) : False;

	/* send client info query, level 3.  receive domain name and sid */
	res = res ? do_lsa_query_info_pol(smb_cli, 
	                                  &info->dom.lsa_info_pol, 0x03,
	                                  info->dom.level3_dom,
	                                  &info->dom.level3_sid) : False;

	/* send client info query, level 5.  receive domain name and sid */
	res = res ? do_lsa_query_info_pol(smb_cli,
	                        &info->dom.lsa_info_pol, 0x05,
				info->dom.level5_dom,
	                        &info->dom.level5_sid) : False;

	res = res ? do_lsa_close(smb_cli, &info->dom.lsa_info_pol) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		BOOL domain_something = False;
		fstring sid;
		DEBUG(5,("cmd_lsa_query_info: query succeeded\n"));

		fprintf(out_hnd, "LSA Query Info Policy\n");

		if (info->dom.level3_dom[0] != 0)
		{
			sid_to_string(sid, &info->dom.level3_sid);
			fprintf(out_hnd, "Domain Member     - Domain: %s SID: %s\n",
				info->dom.level3_dom, sid);
			domain_something = True;
		}
		if (info->dom.level5_dom[0] != 0)
		{
			sid_to_string(sid, &info->dom.level5_sid);
			fprintf(out_hnd, "Domain Controller - Domain: %s SID: %s\n",
				info->dom.level5_dom, sid);
			domain_something = True;
		}
		if (!domain_something)
		{
			fprintf(out_hnd, "%s is not a Domain Member or Controller\n",
			    info->dest_host);
		}
	}
	else
	{
		DEBUG(5,("cmd_lsa_query_info: query failed\n"));
	}
}

/****************************************************************************
nt lsa query
****************************************************************************/
void cmd_lsa_lookup_sids(struct client_info *info)
{
	fstring temp;
	int i;
	fstring sid_name;
	fstring srv_name;
	DOM_SID sid[10];
	DOM_SID *sids[10];
	int num_sids = 0;
	char **names = NULL;
	int num_names = 0;

	BOOL res = True;

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->myhostname);
	strupper(srv_name);

	DEBUG(4,("cmd_lsa_lookup_sids: server: %s\n", srv_name));

	while (num_sids < 10 && next_token(NULL, temp, NULL, sizeof(temp)))
	{
		if (strnequal("S-", temp, 2))
		{
			fstrcpy(sid_name, temp);
		}
		else
		{
			sid_to_string(sid_name, &info->dom.level5_sid);

			if (sid_name[0] == 0)
			{
				fprintf(out_hnd, "please use lsaquery first or specify a complete SID\n");
				return;
			}
				
			fstrcat(sid_name, "-");
			fstrcat(sid_name, temp);
		}
		init_dom_sid(&sid[num_sids], sid_name);
		sids[num_sids] = &sid[num_sids];
		num_sids++;
	}

	if (num_sids == 0)
	{
		fprintf(out_hnd, "lookupsid RID or SID\n");
		return;
	}

	/* open LSARPC session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_LSARPC) : False;

	/* lookup domain controller; receive a policy handle */
	res = res ? do_lsa_open_policy(smb_cli,
				srv_name,
				&info->dom.lsa_info_pol, True) : False;

	/* send lsa lookup sids call */
	res = res ? do_lsa_lookup_sids(smb_cli, 
	                               &info->dom.lsa_info_pol,
	                               num_sids, sids,
	                               &names, &num_names) : False;

	res = res ? do_lsa_close(smb_cli, &info->dom.lsa_info_pol) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_lsa_lookup_sids: query succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_lsa_lookup_sids: query failed\n"));
	}
	if (names != NULL)
	{
		fprintf(out_hnd,"Lookup SIDS:\n");
		for (i = 0; i < num_names; i++)
		{
			sid_to_string(temp, sids[i]);
			fprintf(out_hnd, "SID: %s -> %s\n", temp, names[i]);
			if (names[i] != NULL)
			{
				free(names[i]);
			}
		}
		free(names);
	}
}

