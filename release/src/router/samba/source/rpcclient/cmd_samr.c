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

extern FILE* out_hnd;


/****************************************************************************
SAM password change
****************************************************************************/
void cmd_sam_ntchange_pwd(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	char *new_passwd;
	BOOL res = True;
	char nt_newpass[516];
	uchar nt_hshhash[16];
	uchar nt_newhash[16];
	uchar nt_oldhash[16];
	char lm_newpass[516];
	uchar lm_newhash[16];
	uchar lm_hshhash[16];
	uchar lm_oldhash[16];

	sid_to_string(sid, &info->dom.level5_sid);
	fstrcpy(domain, info->dom.level5_dom);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	fprintf(out_hnd, "SAM NT Password Change\n");

#if 0
	struct pwd_info new_pwd;
	pwd_read(&new_pwd, "New Password (ONCE: this is test code!):", True);
#endif
	new_passwd = (char*)getpass("New Password (ONCE ONLY - get it right :-)");

	nt_lm_owf_gen(new_passwd, lm_newhash, nt_newhash);
	pwd_get_lm_nt_16(&(smb_cli->pwd), lm_oldhash, nt_oldhash );
	make_oem_passwd_hash(nt_newpass, new_passwd, nt_oldhash, True);
	make_oem_passwd_hash(lm_newpass, new_passwd, lm_oldhash, True);
	E_old_pw_hash(lm_newhash, lm_oldhash, lm_hshhash);
	E_old_pw_hash(lm_newhash, nt_oldhash, nt_hshhash);

	cli_nt_set_ntlmssp_flgs(smb_cli,
		                    NTLMSSP_NEGOTIATE_UNICODE |
		                    NTLMSSP_NEGOTIATE_OEM |
		                    NTLMSSP_NEGOTIATE_SIGN |
		                    NTLMSSP_NEGOTIATE_SEAL |
		                    NTLMSSP_NEGOTIATE_LM_KEY |
		                    NTLMSSP_NEGOTIATE_NTLM |
		                    NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
		                    NTLMSSP_NEGOTIATE_00001000 |
		                    NTLMSSP_NEGOTIATE_00002000);

	/* open SAMR session.  */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_unknown_38(smb_cli, srv_name) : False;

	/* establish a connection. */
	res = res ? do_samr_chgpasswd_user(smb_cli,
	                                   srv_name, smb_cli->user_name,
	                                   nt_newpass, nt_hshhash,
	                                   lm_newpass, lm_hshhash) : False;
	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		fprintf(out_hnd, "NT Password changed OK\n");
	}
	else
	{
		fprintf(out_hnd, "NT Password change FAILED\n");
	}
}


/****************************************************************************
experimental SAM encryted rpc test connection
****************************************************************************/
void cmd_sam_test(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	BOOL res = True;

	sid_to_string(sid, &info->dom.level5_sid);
	fstrcpy(domain, info->dom.level5_dom);

/*
	if (strlen(sid) == 0)
	{
		fprintf(out_hnd, "please use 'lsaquery' first, to ascertain the SID\n");
		return;
	}
*/
	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	fprintf(out_hnd, "SAM Encryption Test\n");

	cli_nt_set_ntlmssp_flgs(smb_cli,
		                    NTLMSSP_NEGOTIATE_UNICODE |
		                    NTLMSSP_NEGOTIATE_OEM |
		                    NTLMSSP_NEGOTIATE_SIGN |
		                    NTLMSSP_NEGOTIATE_SEAL |
		                    NTLMSSP_NEGOTIATE_LM_KEY |
		                    NTLMSSP_NEGOTIATE_NTLM |
		                    NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
		                    NTLMSSP_NEGOTIATE_00001000 |
		                    NTLMSSP_NEGOTIATE_00002000);

	/* open SAMR session.  */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_unknown_38(smb_cli, srv_name) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_sam_test: succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_sam_test: failed\n"));
	}
}


/****************************************************************************
experimental SAM users enum.
****************************************************************************/
void cmd_sam_enum_users(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	DOM_SID sid1;
	int user_idx;
	BOOL res = True;
	BOOL request_user_info  = False;
	BOOL request_group_info = False;
	uint16 num_entries = 0;
	uint16 unk_0 = 0x0;
	uint16 acb_mask = 0;
	uint16 unk_1 = 0x0;
	uint32 admin_rid = 0x304; /* absolutely no idea. */
	fstring tmp;

	sid_to_string(sid, &info->dom.level5_sid);
	fstrcpy(domain, info->dom.level5_dom);

	if (strlen(sid) == 0)
	{
		fprintf(out_hnd, "please use 'lsaquery' first, to ascertain the SID\n");
		return;
	}

	init_dom_sid(&sid1, sid);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	/* a bad way to do token parsing... */
	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		request_user_info  |= strequal(tmp, "-u");
		request_group_info |= strequal(tmp, "-g");
	}

	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		request_user_info  |= strequal(tmp, "-u");
		request_group_info |= strequal(tmp, "-g");
	}

#ifdef DEBUG_TESTING
	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		num_entries = (uint16)strtol(tmp, (char**)NULL, 16);
	}

	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		unk_0 = (uint16)strtol(tmp, (char**)NULL, 16);
	}

	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		acb_mask = (uint16)strtol(tmp, (char**)NULL, 16);
	}

	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		unk_1 = (uint16)strtol(tmp, (char**)NULL, 16);
	}
#endif

	fprintf(out_hnd, "SAM Enumerate Users\n");
	fprintf(out_hnd, "From: %s To: %s Domain: %s SID: %s\n",
	                  info->myhostname, srv_name, domain, sid);

#ifdef DEBUG_TESTING
	DEBUG(5,("Number of entries:%d unk_0:%04x acb_mask:%04x unk_1:%04x\n",
	          num_entries, unk_0, acb_mask, unk_1));
#endif

	/* open SAMR session.  negotiate credentials */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_connect(smb_cli, 
				srv_name, 0x00000020,
				&info->dom.samr_pol_connect) : False;

	/* connect to the domain */
	res = res ? do_samr_open_domain(smb_cli, 
	            &info->dom.samr_pol_connect, admin_rid, &sid1,
	            &info->dom.samr_pol_open_domain) : False;

	/* read some users */
	res = res ? do_samr_enum_dom_users(smb_cli, 
				&info->dom.samr_pol_open_domain,
	            num_entries, unk_0, acb_mask, unk_1, 0xffff,
				&info->dom.sam, &info->dom.num_sam_entries) : False;

	if (res && info->dom.num_sam_entries == 0)
	{
		fprintf(out_hnd, "No users\n");
	}

	if (request_user_info || request_group_info)
	{
		/* query all the users */
		user_idx = 0;

		while (res && user_idx < info->dom.num_sam_entries)
		{
			uint32 user_rid = info->dom.sam[user_idx].smb_userid;
			SAM_USER_INFO_21 usr;

			fprintf(out_hnd, "User RID: %8x  User Name: %s\n",
					  user_rid,
					  info->dom.sam[user_idx].acct_name);

			if (request_user_info)
			{
				/* send user info query, level 0x15 */
				if (get_samr_query_userinfo(smb_cli,
							&info->dom.samr_pol_open_domain,
							0x15, user_rid, &usr))
				{
					display_sam_user_info_21(out_hnd, ACTION_HEADER   , &usr);
					display_sam_user_info_21(out_hnd, ACTION_ENUMERATE, &usr);
					display_sam_user_info_21(out_hnd, ACTION_FOOTER   , &usr);
				}
			}

			if (request_group_info)
			{
				uint32 num_groups;
				DOM_GID gid[LSA_MAX_GROUPS];

				/* send user group query */
				if (get_samr_query_usergroups(smb_cli,
							&info->dom.samr_pol_open_domain,
							user_rid, &num_groups, gid))
				{
					display_group_rid_info(out_hnd, ACTION_HEADER   , num_groups, gid);
					display_group_rid_info(out_hnd, ACTION_ENUMERATE, num_groups, gid);
					display_group_rid_info(out_hnd, ACTION_FOOTER   , num_groups, gid);
				}
			}

			user_idx++;
		}
	}

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_open_domain) : False;

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_connect) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (info->dom.sam != NULL)
	{
		free(info->dom.sam);
	}

	if (res)
	{
		DEBUG(5,("cmd_sam_enum_users: succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_sam_enum_users: failed\n"));
	}
}


/****************************************************************************
experimental SAM user query.
****************************************************************************/
void cmd_sam_query_user(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	DOM_SID sid1;
	int user_idx = 0;  /* FIXME maybe ... */
	BOOL res = True;
	uint32 admin_rid = 0x304; /* absolutely no idea. */
	fstring rid_str ;
	fstring info_str;
	uint32 user_rid = 0;
	uint32 info_level = 0x15;

	SAM_USER_INFO_21 usr;

	sid_to_string(sid, &info->dom.level5_sid);
	fstrcpy(domain, info->dom.level5_dom);

	if (strlen(sid) == 0)
	{
		fprintf(out_hnd, "please use 'lsaquery' first, to ascertain the SID\n");
		return;
	}

	init_dom_sid(&sid1, sid);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	if (next_token(NULL, rid_str , NULL, sizeof(rid_str )) &&
	    next_token(NULL, info_str, NULL, sizeof(info_str)))
	{
		user_rid   = (uint32)strtol(rid_str , (char**)NULL, 16);
		info_level = (uint32)strtol(info_str, (char**)NULL, 10);
	}

	fprintf(out_hnd, "SAM Query User: rid %x info level %d\n",
	                  user_rid, info_level);
	fprintf(out_hnd, "From: %s To: %s Domain: %s SID: %s\n",
	                  info->myhostname, srv_name, domain, sid);

	/* open SAMR session.  negotiate credentials */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_connect(smb_cli,
				srv_name, 0x00000020,
				&info->dom.samr_pol_connect) : False;

	/* connect to the domain */
	res = res ? do_samr_open_domain(smb_cli,
	            &info->dom.samr_pol_connect, admin_rid, &sid1,
	            &info->dom.samr_pol_open_domain) : False;

	fprintf(out_hnd, "User RID: %8x  User Name: %s\n",
			  user_rid,
			  info->dom.sam[user_idx].acct_name);

	/* send user info query, level */
	if (get_samr_query_userinfo(smb_cli,
					&info->dom.samr_pol_open_domain,
					info_level, user_rid, &usr))
	{
		if (info_level == 0x15)
		{
			display_sam_user_info_21(out_hnd, ACTION_HEADER   , &usr);
			display_sam_user_info_21(out_hnd, ACTION_ENUMERATE, &usr);
			display_sam_user_info_21(out_hnd, ACTION_FOOTER   , &usr);
		}
	}

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_connect) : False;

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_open_domain) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_sam_query_user: succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_sam_query_user: failed\n"));
	}
}


/****************************************************************************
experimental SAM groups query.
****************************************************************************/
void cmd_sam_query_groups(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	DOM_SID sid1;
	BOOL res = True;
	fstring info_str;
	uint32 switch_value = 2;
	uint32 admin_rid = 0x304; /* absolutely no idea. */

	sid_to_string(sid, &info->dom.level5_sid);
	fstrcpy(domain, info->dom.level5_dom);

	if (strlen(sid) == 0)
	{
		fprintf(out_hnd, "please use 'lsaquery' first, to ascertain the SID\n");
		return;
	}

	init_dom_sid(&sid1, sid);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	if (next_token(NULL, info_str, NULL, sizeof(info_str)))
	{
		switch_value = (uint32)strtol(info_str, (char**)NULL, 10);
	}

	fprintf(out_hnd, "SAM Query Groups: info level %d\n", switch_value);
	fprintf(out_hnd, "From: %s To: %s Domain: %s SID: %s\n",
	                  info->myhostname, srv_name, domain, sid);

	/* open SAMR session.  negotiate credentials */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_connect(smb_cli, 
				srv_name, 0x00000020,
				&info->dom.samr_pol_connect) : False;

	/* connect to the domain */
	res = res ? do_samr_open_domain(smb_cli, 
	            &info->dom.samr_pol_connect, admin_rid, &sid1,
	            &info->dom.samr_pol_open_domain) : False;

	/* send a samr 0x8 command */
	res = res ? do_samr_query_dom_info(smb_cli,
	            &info->dom.samr_pol_open_domain, switch_value) : False;

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_connect) : False;

	res = res ? do_samr_close(smb_cli, 
	            &info->dom.samr_pol_open_domain) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_sam_query_groups: succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_sam_query_groups: failed\n"));
	}
}


/****************************************************************************
experimental SAM aliases query.
****************************************************************************/
void cmd_sam_enum_aliases(struct client_info *info)
{
	fstring srv_name;
	fstring domain;
	fstring sid;
	DOM_SID sid1;
	BOOL res = True;
	BOOL request_user_info  = False;
	BOOL request_alias_info = False;
	uint32 admin_rid = 0x304; /* absolutely no idea. */
	fstring tmp;

	uint32 num_aliases = 3;
	uint32 alias_rid[3] = { DOMAIN_GROUP_RID_ADMINS, DOMAIN_GROUP_RID_USERS, DOMAIN_GROUP_RID_GUESTS };
	fstring alias_names [3];
	uint32  num_als_usrs[3];

	sid_to_string(sid, &info->dom.level3_sid);
	fstrcpy(domain, info->dom.level3_dom);
#if 0
	fstrcpy(sid   , "S-1-5-20");
#endif
	if (strlen(sid) == 0)
	{
		fprintf(out_hnd, "please use 'lsaquery' first, to ascertain the SID\n");
		return;
	}

	init_dom_sid(&sid1, sid);

	fstrcpy(srv_name, "\\\\");
	fstrcat(srv_name, info->dest_host);
	strupper(srv_name);

	/* a bad way to do token parsing... */
	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		request_user_info  |= strequal(tmp, "-u");
		request_alias_info |= strequal(tmp, "-g");
	}

	if (next_token(NULL, tmp, NULL, sizeof(tmp)))
	{
		request_user_info  |= strequal(tmp, "-u");
		request_alias_info |= strequal(tmp, "-g");
	}

	fprintf(out_hnd, "SAM Enumerate Aliases\n");
	fprintf(out_hnd, "From: %s To: %s Domain: %s SID: %s\n",
	                  info->myhostname, srv_name, domain, sid);

	/* open SAMR session.  negotiate credentials */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SAMR) : False;

	/* establish a connection. */
	res = res ? do_samr_connect(smb_cli,
				srv_name, 0x00000020,
				&info->dom.samr_pol_connect) : False;

	/* connect to the domain */
	res = res ? do_samr_open_domain(smb_cli,
	            &info->dom.samr_pol_connect, admin_rid, &sid1,
	            &info->dom.samr_pol_open_domain) : False;

	/* send a query on the aliase */
	res = res ? do_samr_query_unknown_12(smb_cli,
	            &info->dom.samr_pol_open_domain, admin_rid, num_aliases, alias_rid,
	            &num_aliases, alias_names, num_als_usrs) : False;

	if (res)
	{
		display_alias_name_info(out_hnd, ACTION_HEADER   , num_aliases, alias_names, num_als_usrs);
		display_alias_name_info(out_hnd, ACTION_ENUMERATE, num_aliases, alias_names, num_als_usrs);
		display_alias_name_info(out_hnd, ACTION_FOOTER   , num_aliases, alias_names, num_als_usrs);
	}

#if 0

	/* read some users */
	res = res ? do_samr_enum_dom_users(smb_cli,
				&info->dom.samr_pol_open_domain,
	            num_entries, unk_0, acb_mask, unk_1, 0xffff,
				info->dom.sam, &info->dom.num_sam_entries) : False;

	if (res && info->dom.num_sam_entries == 0)
	{
		fprintf(out_hnd, "No users\n");
	}

	if (request_user_info || request_alias_info)
	{
		/* query all the users */
		user_idx = 0;

		while (res && user_idx < info->dom.num_sam_entries)
		{
			uint32 user_rid = info->dom.sam[user_idx].smb_userid;
			SAM_USER_INFO_21 usr;

			fprintf(out_hnd, "User RID: %8x  User Name: %s\n",
					  user_rid,
					  info->dom.sam[user_idx].acct_name);

			if (request_user_info)
			{
				/* send user info query, level 0x15 */
				if (get_samr_query_userinfo(smb_cli,
							&info->dom.samr_pol_open_domain,
							0x15, user_rid, &usr))
				{
					display_sam_user_info_21(out_hnd, ACTION_HEADER   , &usr);
					display_sam_user_info_21(out_hnd, ACTION_ENUMERATE, &usr);
					display_sam_user_info_21(out_hnd, ACTION_FOOTER   , &usr);
				}
			}

			if (request_alias_info)
			{
				uint32 num_aliases;
				DOM_GID gid[LSA_MAX_GROUPS];

				/* send user aliase query */
				if (get_samr_query_useraliases(smb_cli, 
							&info->dom.samr_pol_open_domain,
							user_rid, &num_aliases, gid))
				{
					display_alias_info(out_hnd, ACTION_HEADER   , num_aliases, gid);
					display_alias_info(out_hnd, ACTION_ENUMERATE, num_aliases, gid);
					display_alias_info(out_hnd, ACTION_FOOTER   , num_aliases, gid);
				}
			}

			user_idx++;
		}
	}
#endif

	res = res ? do_samr_close(smb_cli, 
	            &info->dom.samr_pol_connect) : False;

	res = res ? do_samr_close(smb_cli,
	            &info->dom.samr_pol_open_domain) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_sam_enum_users: succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_sam_enum_users: failed\n"));
	}
}


