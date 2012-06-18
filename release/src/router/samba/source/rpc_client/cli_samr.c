/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NT Domain Authentication SMB / MSRPC client
   Copyright (C) Andrew Tridgell 1994-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Jeremy Allison 1999.
   
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

/****************************************************************************
do a SAMR query user groups
****************************************************************************/
BOOL get_samr_query_usergroups(struct cli_state *cli, 
				POLICY_HND *pol_open_domain, uint32 user_rid,
				uint32 *num_groups, DOM_GID *gid)
{
	POLICY_HND pol_open_user;
	if (pol_open_domain == NULL || num_groups == NULL || gid == NULL)
		return False;

	/* send open domain (on user sid) */
	if (!do_samr_open_user(cli,
				pol_open_domain,
				0x02011b, user_rid,
				&pol_open_user))
	{
		return False;
	}

	/* send user groups query */
	if (!do_samr_query_usergroups(cli,
				&pol_open_user,
				num_groups, gid))
	{
		DEBUG(5,("do_samr_query_usergroups: error in query user groups\n"));
	}

	return do_samr_close(cli, &pol_open_user);
}

/****************************************************************************
do a SAMR query user info
****************************************************************************/
BOOL get_samr_query_userinfo(struct cli_state *cli, 
				POLICY_HND *pol_open_domain,
				uint32 info_level,
				uint32 user_rid, SAM_USER_INFO_21 *usr)
{
	POLICY_HND pol_open_user;
	if (pol_open_domain == NULL || usr == NULL)
		return False;

	memset((char *)usr, '\0', sizeof(*usr));

	/* send open domain (on user sid) */
	if (!do_samr_open_user(cli,
				pol_open_domain,
				0x02011b, user_rid,
				&pol_open_user))
	{
		return False;
	}

	/* send user info query */
	if (!do_samr_query_userinfo(cli,
				&pol_open_user,
				info_level, (void*)usr))
	{
		DEBUG(5,("do_samr_query_userinfo: error in query user info, level 0x%x\n",
		          info_level));
	}

	return do_samr_close(cli, &pol_open_user);
}

/****************************************************************************
do a SAMR change user password command
****************************************************************************/
BOOL do_samr_chgpasswd_user(struct cli_state *cli,
		char *srv_name, char *user_name,
		char nt_newpass[516], uchar nt_oldhash[16],
		char lm_newpass[516], uchar lm_oldhash[16])
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_CHGPASSWD_USER q_e;
	SAMR_R_CHGPASSWD_USER r_e;

	/* create and send a MSRPC command with api SAMR_CHGPASSWD_USER */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Change User Password. server:%s username:%s\n",
	        srv_name, user_name));

	init_samr_q_chgpasswd_user(&q_e, srv_name, user_name,
	                           nt_newpass, nt_oldhash,
	                           lm_newpass, lm_oldhash);

	/* turn parameters into data stream */
	if(!samr_io_q_chgpasswd_user("", &q_e, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_CHGPASSWD_USER, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_chgpasswd_user("", &r_e, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_e.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_CHGPASSWD_USER: %s\n", get_nt_error_msg(r_e.status)));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR unknown 0x38 command
****************************************************************************/
BOOL do_samr_unknown_38(struct cli_state *cli, char *srv_name)
{
	prs_struct data;
	prs_struct rdata;

	SAMR_Q_UNKNOWN_38 q_e;
	SAMR_R_UNKNOWN_38 r_e;

	/* create and send a MSRPC command with api SAMR_ENUM_DOM_USERS */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Unknown 38 server:%s\n", srv_name));

	init_samr_q_unknown_38(&q_e, srv_name);

	/* turn parameters into data stream */
	if(!samr_io_q_unknown_38("", &q_e, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_UNKNOWN_38, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_unknown_38("", &r_e, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_e.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_UNKNOWN_38: %s\n", get_nt_error_msg(r_e.status)));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR unknown 0x8 command
****************************************************************************/
BOOL do_samr_query_dom_info(struct cli_state *cli, 
				POLICY_HND *domain_pol, uint16 switch_value)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_QUERY_DOMAIN_INFO q_e;
	SAMR_R_QUERY_DOMAIN_INFO r_e;

	if (domain_pol == NULL)
		return False;

	/* create and send a MSRPC command with api SAMR_ENUM_DOM_USERS */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Unknown 8 switch:%d\n", switch_value));

	/* store the parameters */
	init_samr_q_query_dom_info(&q_e, domain_pol, switch_value);

	/* turn parameters into data stream */
	if(!samr_io_q_query_dom_info("", &q_e, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_QUERY_DOMAIN_INFO, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_query_dom_info("", &r_e, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_e.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_QUERY_DOMAIN_INFO: %s\n", get_nt_error_msg(r_e.status)));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR enumerate users
****************************************************************************/
BOOL do_samr_enum_dom_users(struct cli_state *cli, 
				POLICY_HND *pol, uint16 num_entries, uint16 unk_0,
				uint16 acb_mask, uint16 unk_1, uint32 size,
				struct acct_info **sam,
				int *num_sam_users)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_ENUM_DOM_USERS q_e;
	SAMR_R_ENUM_DOM_USERS r_e;
	int i;
	int name_idx = 0;

	if (pol == NULL || num_sam_users == NULL)
		return False;

	/* create and send a MSRPC command with api SAMR_ENUM_DOM_USERS */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Enum SAM DB max size:%x\n", size));

	/* store the parameters */
	init_samr_q_enum_dom_users(&q_e, pol,
	                           num_entries, unk_0,
	                           acb_mask, unk_1, size);

	/* turn parameters into data stream */
	if(!samr_io_q_enum_dom_users("", &q_e, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_ENUM_DOM_USERS, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_enum_dom_users("", &r_e, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_e.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_ENUM_DOM_USERS: %s\n", get_nt_error_msg(r_e.status)));
		prs_mem_free(&rdata);
		return False;
	}

	*num_sam_users = r_e.num_entries2;
	if (*num_sam_users > MAX_SAM_ENTRIES) {
		*num_sam_users = MAX_SAM_ENTRIES;
		DEBUG(2,("do_samr_enum_dom_users: sam user entries limited to %d\n",
		          *num_sam_users));
	}

	*sam = (struct acct_info*) malloc(sizeof(struct acct_info) * (*num_sam_users));
				    
	if ((*sam) == NULL)
		*num_sam_users = 0;

	for (i = 0; i < *num_sam_users; i++) {
		(*sam)[i].smb_userid = r_e.sam[i].rid;
		if (r_e.sam[i].hdr_name.buffer) {
			char *acct_name = dos_unistrn2(r_e.uni_acct_name[name_idx].buffer,
			                           r_e.uni_acct_name[name_idx].uni_str_len);
			fstrcpy((*sam)[i].acct_name, acct_name);
			name_idx++;
		} else {
			memset((char *)(*sam)[i].acct_name, '\0', sizeof((*sam)[i].acct_name));
		}

		DEBUG(5,("do_samr_enum_dom_users: idx: %4d rid: %8x acct: %s\n",
		          i, (*sam)[i].smb_userid, (*sam)[i].acct_name));
	}

	prs_mem_free(&rdata  );

	return True;
}

/****************************************************************************
do a SAMR Connect
****************************************************************************/
BOOL do_samr_connect(struct cli_state *cli, 
				char *srv_name, uint32 unknown_0,
				POLICY_HND *connect_pol)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_CONNECT q_o;
	SAMR_R_CONNECT r_o;

	if (srv_name == NULL || connect_pol == NULL)
		return False;

	/* create and send a MSRPC command with api SAMR_CONNECT */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Open Policy server:%s undoc value:%x\n",
				srv_name, unknown_0));

	/* store the parameters */
	init_samr_q_connect(&q_o, srv_name, unknown_0);

	/* turn parameters into data stream */
	if(!samr_io_q_connect("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_CONNECT, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_connect("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_CONNECT: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	memcpy(connect_pol, &r_o.connect_pol, sizeof(r_o.connect_pol));

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Open User
****************************************************************************/
BOOL do_samr_open_user(struct cli_state *cli, 
				POLICY_HND *pol, uint32 unk_0, uint32 rid, 
				POLICY_HND *user_pol)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_OPEN_USER q_o;
	SAMR_R_OPEN_USER r_o;

	if (pol == NULL || user_pol == NULL)
		return False;

	/* create and send a MSRPC command with api SAMR_OPEN_USER */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Open User.  unk_0: %08x RID:%x\n",
	          unk_0, rid));

	/* store the parameters */
	init_samr_q_open_user(&q_o, pol, unk_0, rid);

	/* turn parameters into data stream */
	if(!samr_io_q_open_user("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_OPEN_USER, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_open_user("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_OPEN_USER: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	memcpy(user_pol, &r_o.user_pol, sizeof(r_o.user_pol));

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Open Domain
****************************************************************************/
BOOL do_samr_open_domain(struct cli_state *cli, 
				POLICY_HND *connect_pol, uint32 rid, DOM_SID *sid,
				POLICY_HND *domain_pol)
{
	prs_struct data;
	prs_struct rdata;
	pstring sid_str;
	SAMR_Q_OPEN_DOMAIN q_o;
	SAMR_R_OPEN_DOMAIN r_o;

	if (connect_pol == NULL || sid == NULL || domain_pol == NULL)
		return False;

	/* create and send a MSRPC command with api SAMR_OPEN_DOMAIN */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	sid_to_string(sid_str, sid);
	DEBUG(4,("SAMR Open Domain.  SID:%s RID:%x\n", sid_str, rid));

	/* store the parameters */
	init_samr_q_open_domain(&q_o, connect_pol, rid, sid);

	/* turn parameters into data stream */
	if(!samr_io_q_open_domain("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_OPEN_DOMAIN, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_open_domain("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_OPEN_DOMAIN: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	memcpy(domain_pol, &r_o.domain_pol, sizeof(r_o.domain_pol));

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Query Unknown 12
****************************************************************************/
BOOL do_samr_query_unknown_12(struct cli_state *cli, 
				POLICY_HND *pol, uint32 rid, uint32 num_gids, uint32 *gids,
				uint32 *num_aliases,
				fstring als_names    [MAX_LOOKUP_SIDS],
				uint32  num_als_users[MAX_LOOKUP_SIDS])
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_UNKNOWN_12 q_o;
	SAMR_R_UNKNOWN_12 r_o;

	if (pol == NULL || rid == 0 || num_gids == 0 || gids == NULL ||
	    num_aliases == NULL || als_names == NULL || num_als_users == NULL )
			return False;

	/* create and send a MSRPC command with api SAMR_UNKNOWN_12 */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Query Unknown 12.\n"));

	/* store the parameters */
	init_samr_q_unknown_12(&q_o, pol, rid, num_gids, gids);

	/* turn parameters into data stream */
	if(!samr_io_q_unknown_12("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_UNKNOWN_12, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_unknown_12("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_UNKNOWN_12: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ptr_aliases != 0 && r_o.ptr_als_usrs != 0 &&
	    r_o.num_als_usrs1 == r_o.num_aliases1) {
		int i;

		*num_aliases = r_o.num_aliases1;

		for (i = 0; i < r_o.num_aliases1; i++) {
			fstrcpy(als_names[i], dos_unistrn2(r_o.uni_als_name[i].buffer,
						r_o.uni_als_name[i].uni_str_len));
		}
		for (i = 0; i < r_o.num_als_usrs1; i++) {
			num_als_users[i] = r_o.num_als_usrs[i];
		}
	} else if (r_o.ptr_aliases == 0 && r_o.ptr_als_usrs == 0) {
		*num_aliases = 0;
	} else {
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Query User Groups
****************************************************************************/
BOOL do_samr_query_usergroups(struct cli_state *cli, 
				POLICY_HND *pol, uint32 *num_groups, DOM_GID *gid)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_QUERY_USERGROUPS q_o;
	SAMR_R_QUERY_USERGROUPS r_o;

	if (pol == NULL || gid == NULL || num_groups == 0)
		return False;

	/* create and send a MSRPC command with api SAMR_QUERY_USERGROUPS */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Query User Groups.\n"));

	/* store the parameters */
	init_samr_q_query_usergroups(&q_o, pol);

	/* turn parameters into data stream */
	if(!samr_io_q_query_usergroups("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_QUERY_USERGROUPS, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	/* get user info */
	r_o.gid = gid;

	if(!samr_io_r_query_usergroups("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_QUERY_USERGROUPS: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	*num_groups = r_o.num_entries;

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Query User Info
****************************************************************************/
BOOL do_samr_query_userinfo(struct cli_state *cli, 
				POLICY_HND *pol, uint16 switch_value, void* usr)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_QUERY_USERINFO q_o;
	SAMR_R_QUERY_USERINFO r_o;

	if (pol == NULL || usr == NULL || switch_value == 0)
		return False;

	/* create and send a MSRPC command with api SAMR_QUERY_USERINFO */

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	DEBUG(4,("SAMR Query User Info.  level: %d\n", switch_value));

	/* store the parameters */
	init_samr_q_query_userinfo(&q_o, pol, switch_value);

	/* turn parameters into data stream */
	if(!samr_io_q_query_userinfo("", &q_o,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_QUERY_USERINFO, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	/* get user info */
	r_o.info.id = usr;

	if(!samr_io_r_query_userinfo("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_R_QUERY_USERINFO: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.switch_value != switch_value) {
		DEBUG(0,("SAMR_R_QUERY_USERINFO: received incorrect level %d\n",
		          r_o.switch_value));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ptr == 0) {
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);

	return True;
}

/****************************************************************************
do a SAMR Close
****************************************************************************/
BOOL do_samr_close(struct cli_state *cli, POLICY_HND *hnd)
{
	prs_struct data;
	prs_struct rdata;
	SAMR_Q_CLOSE_HND q_c;
	SAMR_R_CLOSE_HND r_c;
	int i;

	if (hnd == NULL)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SAMR_CLOSE_HND */

	DEBUG(4,("SAMR Close\n"));

	/* store the parameters */
	init_samr_q_close_hnd(&q_c, hnd);

	/* turn parameters into data stream */
	if(!samr_io_q_close_hnd("", &q_c,  &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SAMR_CLOSE_HND, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!samr_io_r_close_hnd("", &r_c, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_c.status != 0) {
		/* report error code */
		DEBUG(0,("SAMR_CLOSE_HND: %s\n", get_nt_error_msg(r_c.status)));
		prs_mem_free(&rdata);
		return False;
	}

	/* check that the returned policy handle is all zeros */

	for (i = 0; i < sizeof(r_c.pol.data); i++) {
		if (r_c.pol.data[i] != 0) {
			DEBUG(0,("SAMR_CLOSE_HND: non-zero handle returned\n"));
			prs_mem_free(&rdata);
			return False;
		}
	}	

	prs_mem_free(&rdata);

	return True;
}
