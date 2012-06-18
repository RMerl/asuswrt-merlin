
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

extern fstring global_myworkgroup;
extern pstring global_myname;
extern DOM_SID global_sam_sid;

extern rid_name domain_group_rids[];
extern rid_name domain_alias_rids[];
extern rid_name builtin_alias_rids[];

/*******************************************************************
  This next function should be replaced with something that
  dynamically returns the correct user info..... JRA.
 ********************************************************************/

static BOOL get_sampwd_entries(SAM_USER_INFO_21 *pw_buf,
				int start_idx,
                                int *total_entries, int *num_entries,
                                int max_num_entries,
                                uint16 acb_mask)
{
	void *vp = NULL;
	struct sam_passwd *pwd = NULL;

	(*num_entries) = 0;
	(*total_entries) = 0;

	if (pw_buf == NULL) return False;

	vp = startsmbpwent(False);
	if (!vp)
	{
		DEBUG(0, ("get_sampwd_entries: Unable to open SMB password database.\n"));
		return False;
	}

	while (((pwd = getsam21pwent(vp)) != NULL) && (*num_entries) < max_num_entries)
	{
		int user_name_len;

		if (start_idx > 0)
		{
			/* skip the requested number of entries.
			   not very efficient, but hey...
			 */
			start_idx--;
			continue;
		}

		user_name_len = strlen(pwd->smb_name);
		init_unistr2(&(pw_buf[(*num_entries)].uni_user_name), pwd->smb_name, user_name_len);
		init_uni_hdr(&(pw_buf[(*num_entries)].hdr_user_name), user_name_len);
		pw_buf[(*num_entries)].user_rid = pwd->user_rid;
		memset((char *)pw_buf[(*num_entries)].nt_pwd, '\0', 16);

		/* Now check if the NT compatible password is available. */
		if (pwd->smb_nt_passwd != NULL)
		{
			memcpy( pw_buf[(*num_entries)].nt_pwd , pwd->smb_nt_passwd, 16);
		}

		pw_buf[(*num_entries)].acb_info = (uint16)pwd->acct_ctrl;

		DEBUG(5, ("entry idx: %d user %s, rid 0x%x, acb %x",
		          (*num_entries), pwd->smb_name,
		          pwd->user_rid, pwd->acct_ctrl));

		if (acb_mask == 0 || IS_BITS_SET_SOME(pwd->acct_ctrl, acb_mask))
		{
			DEBUG(5,(" acb_mask %x accepts\n", acb_mask));
			(*num_entries)++;
		}
		else
		{
			DEBUG(5,(" acb_mask %x rejects\n", acb_mask));
		}

		(*total_entries)++;
	}

	endsmbpwent(vp);

	return (*num_entries) > 0;
}

/*******************************************************************
 samr_reply_unknown_1
 ********************************************************************/
static void samr_reply_close_hnd(SAMR_Q_CLOSE_HND *q_u,
				prs_struct *rdata)
{
	SAMR_R_CLOSE_HND r_u;

	/* set up the SAMR unknown_1 response */
	memset((char *)r_u.pol.data, '\0', POL_HND_SIZE);

	/* close the policy handle */
	if (close_lsa_policy_hnd(&(q_u->pol)))
	{
		r_u.status = 0;
	}
	else
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_INVALID;
	}

	DEBUG(5,("samr_reply_close_hnd: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_close_hnd("", &r_u, rdata, 0);

	DEBUG(5,("samr_reply_close_hnd: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_close_hnd
 ********************************************************************/
static BOOL api_samr_close_hnd( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_CLOSE_HND q_u;

	/* grab the samr unknown 1 */
	samr_io_q_close_hnd("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_close_hnd(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_open_domain
 ********************************************************************/
static void samr_reply_open_domain(SAMR_Q_OPEN_DOMAIN *q_u,
				prs_struct *rdata)
{
	SAMR_R_OPEN_DOMAIN r_u;
	BOOL pol_open = False;

	r_u.status = 0x0;

	/* find the connection policy handle. */
	if (r_u.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->connect_pol)) == -1))
	{
		r_u.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	/* get a (unique) handle.  open a policy on it. */
	if (r_u.status == 0x0 && !(pol_open = open_lsa_policy_hnd(&(r_u.domain_pol))))
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* associate the domain SID with the (unique) handle. */
	if (r_u.status == 0x0 && !set_lsa_policy_samr_sid(&(r_u.domain_pol), &(q_u->dom_sid.sid)))
	{
		/* oh, whoops.  don't know what error message to return, here */
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (r_u.status != 0 && pol_open)
	{
		close_lsa_policy_hnd(&(r_u.domain_pol));
	}

	DEBUG(5,("samr_open_domain: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_open_domain("", &r_u, rdata, 0);

	DEBUG(5,("samr_open_domain: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_open_domain
 ********************************************************************/
static BOOL api_samr_open_domain( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_OPEN_DOMAIN q_u;

	/* grab the samr open */
	samr_io_q_open_domain("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_open_domain(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_unknown_2c
 ********************************************************************/
static void samr_reply_unknown_2c(SAMR_Q_UNKNOWN_2C *q_u,
				prs_struct *rdata)
{
	SAMR_R_UNKNOWN_2C r_u;
	uint32 status = 0x0;

	/* find the policy handle.  open a policy on it. */
	if (status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->user_pol)) == -1))
	{
		status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	/* find the user's rid */
	if ((status == 0x0) && (get_lsa_policy_samr_rid(&(q_u->user_pol)) == 0xffffffff))
	{
		status = NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	init_samr_r_unknown_2c(&r_u, status);

	DEBUG(5,("samr_unknown_2c: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_unknown_2c("", &r_u, rdata, 0);

	DEBUG(5,("samr_unknown_2c: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_unknown_2c
 ********************************************************************/
static BOOL api_samr_unknown_2c( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_UNKNOWN_2C q_u;

	/* grab the samr open */
	samr_io_q_unknown_2c("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_unknown_2c(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_unknown_3
 ********************************************************************/
static void samr_reply_unknown_3(SAMR_Q_UNKNOWN_3 *q_u,
				prs_struct *rdata)
{
	SAMR_R_UNKNOWN_3 r_u;
	DOM_SID3 sid[MAX_SAM_SIDS];
	uint32 rid;
	uint32 status;

	status = 0x0;

	/* find the policy handle.  open a policy on it. */
	if (status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->user_pol)) == -1))
	{
		status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	/* find the user's rid */
	if (status == 0x0 && (rid = get_lsa_policy_samr_rid(&(q_u->user_pol))) == 0xffffffff)
	{
		status = NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (status == 0x0)
	{
		DOM_SID user_sid;
		DOM_SID everyone_sid;

		user_sid = global_sam_sid;

		SMB_ASSERT_ARRAY(user_sid.sub_auths, user_sid.num_auths+1);

		/*
		 * Add the user RID.
		 */
		user_sid.sub_auths[user_sid.num_auths++] = rid;
		
			string_to_sid(&everyone_sid, "S-1-1");

			/* maybe need another 1 or 2 (S-1-5-0x20-0x220 and S-1-5-20-0x224) */
			/* these two are DOMAIN_ADMIN and DOMAIN_ACCT_OP group RIDs */
			init_dom_sid3(&(sid[0]), 0x035b, 0x0002, &everyone_sid);
			init_dom_sid3(&(sid[1]), 0x0044, 0x0002, &user_sid);
	}

	init_samr_r_unknown_3(&r_u,
				0x0001, 0x8004,
				0x00000014, 0x0002, 0x0070,
				2, sid, status);

	DEBUG(5,("samr_unknown_3: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_unknown_3("", &r_u, rdata, 0);

	DEBUG(5,("samr_unknown_3: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_unknown_3
 ********************************************************************/
static BOOL api_samr_unknown_3( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_UNKNOWN_3 q_u;

	/* grab the samr open */
	samr_io_q_unknown_3("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_unknown_3(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_enum_dom_users
 ********************************************************************/
static void samr_reply_enum_dom_users(SAMR_Q_ENUM_DOM_USERS *q_u,
				prs_struct *rdata)
{
	SAMR_R_ENUM_DOM_USERS r_e;
	SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES];
	int num_entries;
	int total_entries;

	r_e.status = 0x0;
	r_e.total_num_entries = 0;

	/* find the policy handle.  open a policy on it. */
	if (r_e.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		r_e.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	DEBUG(5,("samr_reply_enum_dom_users: %d\n", __LINE__));

	become_root(True);
	get_sampwd_entries(pass, 0, &total_entries, &num_entries, MAX_SAM_ENTRIES, q_u->acb_mask);
	unbecome_root(True);

	init_samr_r_enum_dom_users(&r_e, total_entries,
	                           q_u->unknown_0, num_entries,
	                           pass, r_e.status);

	/* store the response in the SMB stream */
	samr_io_r_enum_dom_users("", &r_e, rdata, 0);

	DEBUG(5,("samr_enum_dom_users: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_enum_dom_users
 ********************************************************************/
static BOOL api_samr_enum_dom_users( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_ENUM_DOM_USERS q_e;

	/* grab the samr open */
	samr_io_q_enum_dom_users("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_enum_dom_users(&q_e, rdata);

	return True;
}


/*******************************************************************
 samr_reply_enum_dom_groups
 ********************************************************************/
static void samr_reply_enum_dom_groups(SAMR_Q_ENUM_DOM_GROUPS *q_u,
				prs_struct *rdata)
{
	SAMR_R_ENUM_DOM_GROUPS r_e;
	SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES];
	int num_entries;
	BOOL got_grps;
	char *dummy_group = "Domain Admins";

	r_e.status = 0x0;
	r_e.num_entries = 0;

	/* find the policy handle.  open a policy on it. */
	if (r_e.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		r_e.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	DEBUG(5,("samr_reply_enum_dom_groups: %d\n", __LINE__));

	got_grps = True;
	num_entries = 1;
	init_unistr2(&(pass[0].uni_user_name), dummy_group, strlen(dummy_group));
	pass[0].user_rid = DOMAIN_GROUP_RID_ADMINS;

	if (r_e.status == 0 && got_grps)
	{
		init_samr_r_enum_dom_groups(&r_e, q_u->start_idx, num_entries, pass, r_e.status);
	}

	/* store the response in the SMB stream */
	samr_io_r_enum_dom_groups("", &r_e, rdata, 0);

	DEBUG(5,("samr_enum_dom_groups: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_enum_dom_groups
 ********************************************************************/
static BOOL api_samr_enum_dom_groups( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_ENUM_DOM_GROUPS q_e;

	/* grab the samr open */
	samr_io_q_enum_dom_groups("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_enum_dom_groups(&q_e, rdata);

	return True;
}


/*******************************************************************
 samr_reply_enum_dom_aliases
 ********************************************************************/
static void samr_reply_enum_dom_aliases(SAMR_Q_ENUM_DOM_ALIASES *q_u,
				prs_struct *rdata)
{
	SAMR_R_ENUM_DOM_ALIASES r_e;
	SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES];
	int num_entries = 0;
	DOM_SID sid;
	fstring sid_str;
	fstring sam_sid_str;

	r_e.status = 0x0;
	r_e.num_entries = 0;

	/* find the policy handle.  open a policy on it. */
	if (r_e.status == 0x0 && !get_lsa_policy_samr_sid(&q_u->pol, &sid))
	{
		r_e.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	sid_to_string(sid_str, &sid);
	sid_to_string(sam_sid_str, &global_sam_sid);

	DEBUG(5,("samr_reply_enum_dom_aliases: sid %s\n", sid_str));

	/* well-known aliases */
	if (strequal(sid_str, "S-1-5-32"))
	{
		char *name;
		while (num_entries < MAX_SAM_ENTRIES && ((name = builtin_alias_rids[num_entries].name) != NULL))
		{
			init_unistr2(&(pass[num_entries].uni_user_name), name, strlen(name));
			pass[num_entries].user_rid = builtin_alias_rids[num_entries].rid;
			num_entries++;
		}
	}
	else if (strequal(sid_str, sam_sid_str))
	{
		/* local aliases */
		/* oops!  there's no code to deal with this */
		DEBUG(3,("samr_reply_enum_dom_aliases: enum of aliases in our domain not supported yet\n"));
		num_entries = 0;
	}
		
	init_samr_r_enum_dom_aliases(&r_e, num_entries, pass, r_e.status);

	/* store the response in the SMB stream */
	samr_io_r_enum_dom_aliases("", &r_e, rdata, 0);

	DEBUG(5,("samr_enum_dom_aliases: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_enum_dom_aliases
 ********************************************************************/
static BOOL api_samr_enum_dom_aliases( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_ENUM_DOM_ALIASES q_e;

	/* grab the samr open */
	samr_io_q_enum_dom_aliases("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_enum_dom_aliases(&q_e, rdata);

	return True;
}


/*******************************************************************
 samr_reply_query_dispinfo
 ********************************************************************/
static void samr_reply_query_dispinfo(SAMR_Q_QUERY_DISPINFO *q_u,
				prs_struct *rdata)
{
	SAMR_R_QUERY_DISPINFO r_e;
	SAM_INFO_CTR ctr;
	SAM_INFO_1 info1;
	SAM_INFO_2 info2;
	SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES];
	int num_entries = 0;
	int total_entries = 0;
	BOOL got_pwds;
	uint16 switch_level = 0x0;

	ZERO_STRUCT(r_e);

	r_e.status = 0x0;

	DEBUG(5,("samr_reply_query_dispinfo: %d\n", __LINE__));

	/* find the policy handle.  open a policy on it. */
	if (r_e.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		r_e.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
		DEBUG(5,("samr_reply_query_dispinfo: invalid handle\n"));
	}

	if (r_e.status == 0x0)
	{
		become_root(True);
		got_pwds = get_sampwd_entries(pass, q_u->start_idx, &total_entries, &num_entries, MAX_SAM_ENTRIES, 0);
		unbecome_root(True);

		switch (q_u->switch_level)
		{
			case 0x1:
			{
			
				/* query disp info is for users */
				switch_level = 0x1;
				init_sam_info_1(&info1, ACB_NORMAL,
					q_u->start_idx, num_entries, pass);

				ctr.sam.info1 = &info1;

				break;
			}
			case 0x2:
			{
				/* query disp info is for servers */
				switch_level = 0x2;
				init_sam_info_2(&info2, ACB_WSTRUST,
					q_u->start_idx, num_entries, pass);

				ctr.sam.info2 = &info2;

				break;
			}
		}
	}

	if (r_e.status == 0 && got_pwds)
	{
		init_samr_r_query_dispinfo(&r_e, switch_level, &ctr, r_e.status);
	}

	/* store the response in the SMB stream */
	samr_io_r_query_dispinfo("", &r_e, rdata, 0);

	DEBUG(5,("samr_query_dispinfo: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_query_dispinfo
 ********************************************************************/
static BOOL api_samr_query_dispinfo( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_QUERY_DISPINFO q_e;

	/* grab the samr open */
	samr_io_q_query_dispinfo("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_query_dispinfo(&q_e, rdata);

	return True;
}


/*******************************************************************
 samr_reply_query_aliasinfo
 ********************************************************************/
static void samr_reply_query_aliasinfo(SAMR_Q_QUERY_ALIASINFO *q_u,
				prs_struct *rdata)
{
	SAMR_R_QUERY_ALIASINFO r_e;

	r_e.status = 0x0;
	r_e.ptr = 0;

	/* find the policy handle.  open a policy on it. */
	if (r_e.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		r_e.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	DEBUG(5,("samr_reply_query_aliasinfo: %d\n", __LINE__));

	if (r_e.status == 0x0)
	{
		if (q_u->switch_level != 3)
		{
			r_e.status = NT_STATUS_INVALID_INFO_CLASS;
		}
	}

	init_samr_r_query_aliasinfo(&r_e, q_u->switch_level,
	                    "<account description>",
		                r_e.status);

	/* store the response in the SMB stream */
	samr_io_r_query_aliasinfo("", &r_e, rdata, 0);

	DEBUG(5,("samr_query_aliasinfo: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_query_aliasinfo
 ********************************************************************/
static BOOL api_samr_query_aliasinfo( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_QUERY_ALIASINFO q_e;

	/* grab the samr open */
	samr_io_q_query_aliasinfo("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_query_aliasinfo(&q_e, rdata);

	return True;
}


/*******************************************************************
 samr_reply_lookup_ids
 ********************************************************************/
static void samr_reply_lookup_ids(SAMR_Q_LOOKUP_IDS *q_u,
				prs_struct *rdata)
{
	uint32 rid[MAX_SAM_ENTRIES];
	uint32 status     = 0;
	int num_rids = q_u->num_sids1;

	SAMR_R_LOOKUP_IDS r_u;

	DEBUG(5,("samr_lookup_ids: %d\n", __LINE__));

	if (num_rids > MAX_SAM_ENTRIES)
	{
		num_rids = MAX_SAM_ENTRIES;
		DEBUG(5,("samr_lookup_ids: truncating entries to %d\n", num_rids));
	}

#if 0
	int i;
	SMB_ASSERT_ARRAY(q_u->uni_user_name, num_rids);

	for (i = 0; i < num_rids && status == 0; i++)
	{
		struct sam_passwd *sam_pass;
		fstring user_name;


		fstrcpy(user_name, unistrn2(q_u->uni_user_name[i].buffer,
		                            q_u->uni_user_name[i].uni_str_len));

		/* find the user account */
		become_root(True);
		sam_pass = get_smb21pwd_entry(user_name, 0);
		unbecome_root(True);

		if (sam_pass == NULL)
		{
			status = 0xC0000000 | NT_STATUS_NO_SUCH_USER;
			rid[i] = 0;
		}
		else
		{
			rid[i] = sam_pass->user_rid;
		}
	}
#endif

	num_rids = 1;
	rid[0] = BUILTIN_ALIAS_RID_USERS;

	init_samr_r_lookup_ids(&r_u, num_rids, rid, status);

	/* store the response in the SMB stream */
	samr_io_r_lookup_ids("", &r_u, rdata, 0);

	DEBUG(5,("samr_lookup_ids: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_lookup_ids
 ********************************************************************/
static BOOL api_samr_lookup_ids( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_LOOKUP_IDS q_u;

	/* grab the samr 0x10 */
	samr_io_q_lookup_ids("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_lookup_ids(&q_u, rdata);

	return True;
}

/*******************************************************************
 samr_reply_lookup_names
 ********************************************************************/

static BOOL samr_reply_lookup_names(SAMR_Q_LOOKUP_NAMES *q_u,
				prs_struct *rdata)
{
	uint32 rid[MAX_SAM_ENTRIES];
	uint8  type[MAX_SAM_ENTRIES];
	uint32 status = 0;
	int i;
	int num_rids = q_u->num_names1;
	DOM_SID pol_sid;

	SAMR_R_LOOKUP_NAMES r_u;

	DEBUG(5,("samr_lookup_names: %d\n", __LINE__));

	ZERO_ARRAY(rid);
	ZERO_ARRAY(type);

	if (!get_lsa_policy_samr_sid(&q_u->pol, &pol_sid)) {
        status = 0xC0000000 | NT_STATUS_OBJECT_TYPE_MISMATCH;
		init_samr_r_lookup_names(&r_u, 0, rid, type, status);
		if(!samr_io_r_lookup_names("", &r_u, rdata, 0)) {
			DEBUG(0,("samr_reply_lookup_names: failed to marshall SAMR_R_LOOKUP_NAMES.\n"));
        	return False;
		}
		return True;
    }

	if (num_rids > MAX_SAM_ENTRIES) {
		num_rids = MAX_SAM_ENTRIES;
		DEBUG(5,("samr_lookup_names: truncating entries to %d\n", num_rids));
	}

	SMB_ASSERT_ARRAY(q_u->uni_name, num_rids);

	for (i = 0; i < num_rids; i++) {
		fstring name;

		status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

		rid [i] = 0xffffffff;
		type[i] = SID_NAME_UNKNOWN;

		fstrcpy(name, dos_unistrn2(q_u->uni_name[i].buffer,
				q_u->uni_name[i].uni_str_len));

		if(sid_equal(&pol_sid, &global_sam_sid)) {
			DOM_SID sid;

			if(lookup_local_name(global_myname, name, &sid, &type[i])) {
				sid_split_rid( &sid, &rid[i]);
				status = 0;
			}
		}
	}

	init_samr_r_lookup_names(&r_u, num_rids, rid, type, status);

	/* store the response in the SMB stream */
	if(!samr_io_r_lookup_names("", &r_u, rdata, 0)) {
		DEBUG(0,("samr_reply_lookup_names: failed to marshall SAMR_R_LOOKUP_NAMES.\n"));
		return False;
	}

	DEBUG(5,("samr_lookup_names: %d\n", __LINE__));

	return True;
}

/*******************************************************************
 api_samr_lookup_names
 ********************************************************************/

static BOOL api_samr_lookup_names( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_LOOKUP_NAMES q_u;

	memset(&q_u, '\0', sizeof(q_u));

	/* grab the samr lookup names */
	if(!samr_io_q_lookup_names("", &q_u, data, 0)) {
		DEBUG(0,("api_samr_lookup_names: failed to unmarshall SAMR_Q_LOOKUP_NAMES.\n"));
		return False;
	}

	/* construct reply.  always indicate success */
	if(!samr_reply_lookup_names(&q_u, rdata))
		return False;

	return True;
}

/*******************************************************************
 samr_reply_chgpasswd_user
 ********************************************************************/

static BOOL samr_reply_chgpasswd_user(SAMR_Q_CHGPASSWD_USER *q_u,
				prs_struct *rdata)
{
	SAMR_R_CHGPASSWD_USER r_u;
	uint32 status = 0x0;
	fstring user_name;
	fstring wks;

	fstrcpy(user_name, dos_unistrn2(q_u->uni_user_name.buffer, q_u->uni_user_name.uni_str_len));
	fstrcpy(wks      , dos_unistrn2(q_u->uni_dest_host.buffer, q_u->uni_dest_host.uni_str_len));

	DEBUG(5,("samr_chgpasswd_user: user: %s wks: %s\n", user_name, wks));

	if (!pass_oem_change(user_name,
	                     q_u->lm_newpass.pass, q_u->lm_oldhash.hash,
	                     q_u->nt_newpass.pass, q_u->nt_oldhash.hash))
	{
		status = 0xC0000000 | NT_STATUS_WRONG_PASSWORD;
	}

	init_samr_r_chgpasswd_user(&r_u, status);

	/* store the response in the SMB stream */
	if(!samr_io_r_chgpasswd_user("", &r_u, rdata, 0)) {
		DEBUG(0,("samr_reply_chgpasswd_user: Failed to marshall SAMR_R_CHGPASSWD_USER struct.\n" ));
		return False;
	}

	DEBUG(5,("samr_chgpasswd_user: %d\n", __LINE__));
	return True;
}

/*******************************************************************
 api_samr_chgpasswd_user
 ********************************************************************/

static BOOL api_samr_chgpasswd_user( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_CHGPASSWD_USER q_u;

	/* unknown 38 command */
	if (!samr_io_q_chgpasswd_user("", &q_u, data, 0)) {
		DEBUG(0,("api_samr_chgpasswd_user: samr_io_q_chgpasswd_user failed to parse RPC packet.\n"));
		return False;
	}

	/* construct reply. */
	if(!samr_reply_chgpasswd_user(&q_u, rdata)) {
		DEBUG(0,("api_samr_chgpasswd_user: samr_reply_chgpasswd_user failed to create reply packet.\n"));
		return False;
	}

	return True;
}


/*******************************************************************
 samr_reply_unknown_38
 ********************************************************************/
static void samr_reply_unknown_38(SAMR_Q_UNKNOWN_38 *q_u,
				prs_struct *rdata)
{
	SAMR_R_UNKNOWN_38 r_u;

	DEBUG(5,("samr_unknown_38: %d\n", __LINE__));

	init_samr_r_unknown_38(&r_u);

	/* store the response in the SMB stream */
	samr_io_r_unknown_38("", &r_u, rdata, 0);

	DEBUG(5,("samr_unknown_38: %d\n", __LINE__));
}

/*******************************************************************
 api_samr_unknown_38
 ********************************************************************/
static BOOL api_samr_unknown_38( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_UNKNOWN_38 q_u;

	/* unknown 38 command */
	samr_io_q_unknown_38("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_unknown_38(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_unknown_12
 ********************************************************************/
static void samr_reply_unknown_12(SAMR_Q_UNKNOWN_12 *q_u,
				prs_struct *rdata)
{
	fstring group_names[MAX_SAM_ENTRIES];
	uint32  group_attrs[MAX_SAM_ENTRIES];
	uint32 status     = 0;
	int num_gids = q_u->num_gids1;

	SAMR_R_UNKNOWN_12 r_u;

	DEBUG(5,("samr_unknown_12: %d\n", __LINE__));

	/* find the policy handle.  open a policy on it. */
	if (status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	if (status == 0x0)
	{
		int i;
		if (num_gids > MAX_SAM_ENTRIES)
		{
			num_gids = MAX_SAM_ENTRIES;
			DEBUG(5,("samr_unknown_12: truncating entries to %d\n", num_gids));
		}

		for (i = 0; i < num_gids && status == 0; i++)
		{
			fstrcpy(group_names[i], "dummy group");
			group_attrs[i] = 0x2;
		}
	}

	init_samr_r_unknown_12(&r_u, num_gids, group_names, group_attrs, status);

	/* store the response in the SMB stream */
	samr_io_r_unknown_12("", &r_u, rdata, 0);

	DEBUG(5,("samr_unknown_12: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_unknown_12
 ********************************************************************/
static BOOL api_samr_unknown_12( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_UNKNOWN_12 q_u;

	/* grab the samr lookup names */
	samr_io_q_unknown_12("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_unknown_12(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_open_user
 ********************************************************************/
static void samr_reply_open_user(SAMR_Q_OPEN_USER *q_u,
				prs_struct *rdata,
				int status)
{
	SAMR_R_OPEN_USER r_u;
	struct sam_passwd *sam_pass;
	BOOL pol_open = False;

	/* set up the SAMR open_user response */
	memset((char *)r_u.user_pol.data, '\0', POL_HND_SIZE);

	r_u.status = 0x0;

	/* find the policy handle.  open a policy on it. */
	if (r_u.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->domain_pol)) == -1))
	{
		r_u.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	/* get a (unique) handle.  open a policy on it. */
	if (r_u.status == 0x0 && !(pol_open = open_lsa_policy_hnd(&(r_u.user_pol))))
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	become_root(True);
	sam_pass = getsam21pwrid(q_u->user_rid);
	unbecome_root(True);

	/* check that the RID exists in our domain. */
	if (r_u.status == 0x0 && sam_pass == NULL)
	{
		r_u.status = 0xC0000000 | NT_STATUS_NO_SUCH_USER;
	}

	/* associate the RID with the (unique) handle. */
	if (r_u.status == 0x0 && !set_lsa_policy_samr_rid(&(r_u.user_pol), q_u->user_rid))
	{
		/* oh, whoops.  don't know what error message to return, here */
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (r_u.status != 0 && pol_open)
	{
		close_lsa_policy_hnd(&(r_u.user_pol));
	}

	DEBUG(5,("samr_open_user: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_open_user("", &r_u, rdata, 0);

	DEBUG(5,("samr_open_user: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_open_user
 ********************************************************************/
static BOOL api_samr_open_user( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_OPEN_USER q_u;

	/* grab the samr unknown 22 */
	samr_io_q_open_user("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_open_user(&q_u, rdata, 0x0);

	return True;
}


/*************************************************************************
 get_user_info_10
 *************************************************************************/
static BOOL get_user_info_10(SAM_USER_INFO_10 *id10, uint32 user_rid)
{
	struct smb_passwd *smb_pass;

	if (!pdb_rid_is_user(user_rid))
	{
		DEBUG(4,("RID 0x%x is not a user RID\n", user_rid));
		return False;
	}

	become_root(True);
	smb_pass = getsmbpwrid(user_rid);
	unbecome_root(True);

	if (smb_pass == NULL)
	{
		DEBUG(4,("User 0x%x not found\n", user_rid));
		return False;
	}

	DEBUG(3,("User:[%s]\n", smb_pass->smb_name));

	init_sam_user_info10(id10, smb_pass->acct_ctrl); 

	return True;
}

/*************************************************************************
 get_user_info_21
 *************************************************************************/
static BOOL get_user_info_21(SAM_USER_INFO_21 *id21, uint32 user_rid)
{
	NTTIME dummy_time;
	struct sam_passwd *sam_pass;
	LOGON_HRS hrs;
	int i;

	if (!pdb_rid_is_user(user_rid))
	{
		DEBUG(4,("RID 0x%x is not a user RID\n", user_rid));
		return False;
	}

	become_root(True);
	sam_pass = getsam21pwrid(user_rid);
	unbecome_root(True);

	if (sam_pass == NULL)
	{
		DEBUG(4,("User 0x%x not found\n", user_rid));
		return False;
	}

	DEBUG(3,("User:[%s]\n", sam_pass->smb_name));

	dummy_time.low  = 0xffffffff;
	dummy_time.high = 0x7fffffff;

	DEBUG(5,("get_user_info_21 - TODO: convert unix times to NTTIMEs\n"));

	/* create a LOGON_HRS structure */
	hrs.len = sam_pass->hours_len;
	SMB_ASSERT_ARRAY(hrs.hours, hrs.len);
	for (i = 0; i < hrs.len; i++)
	{
		hrs.hours[i] = sam_pass->hours[i];
	}

	init_sam_user_info21(id21,

			   &dummy_time, /* logon_time */
			   &dummy_time, /* logoff_time */
			   &dummy_time, /* kickoff_time */
			   &dummy_time, /* pass_last_set_time */
			   &dummy_time, /* pass_can_change_time */
			   &dummy_time, /* pass_must_change_time */

			   sam_pass->smb_name, /* user_name */
			   sam_pass->full_name, /* full_name */
			   sam_pass->home_dir, /* home_dir */
			   sam_pass->dir_drive, /* dir_drive */
			   sam_pass->logon_script, /* logon_script */
			   sam_pass->profile_path, /* profile_path */
			   sam_pass->acct_desc, /* description */
			   sam_pass->workstations, /* workstations user can log in from */
			   sam_pass->unknown_str, /* don't know, yet */
			   sam_pass->munged_dial, /* dialin info.  contains dialin path and tel no */

			   sam_pass->user_rid, /* RID user_id */
			   sam_pass->group_rid, /* RID group_id */
		       sam_pass->acct_ctrl,

	           sam_pass->unknown_3, /* unknown_3 */
		       sam_pass->logon_divs, /* divisions per week */
			   &hrs, /* logon hours */
		       sam_pass->unknown_5,
		       sam_pass->unknown_6);

	return True;
}

/*******************************************************************
 samr_reply_query_userinfo
 ********************************************************************/
static void samr_reply_query_userinfo(SAMR_Q_QUERY_USERINFO *q_u,
				prs_struct *rdata)
{
	SAMR_R_QUERY_USERINFO r_u;
#if 0
	SAM_USER_INFO_11 id11;
#endif
	SAM_USER_INFO_10 id10;
	SAM_USER_INFO_21 id21;
	void *info = NULL;

	uint32 status = 0x0;
	uint32 rid = 0x0;

	DEBUG(5,("samr_reply_query_userinfo: %d\n", __LINE__));

	/* search for the handle */
	if (status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		status = NT_STATUS_INVALID_HANDLE;
	}

	/* find the user's rid */
	if (status == 0x0 && (rid = get_lsa_policy_samr_rid(&(q_u->pol))) == 0xffffffff)
	{
		status = NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	DEBUG(5,("samr_reply_query_userinfo: rid:0x%x\n", rid));

	/* ok!  user info levels (there are lots: see MSDEV help), off we go... */
	if (status == 0x0)
	{
		switch (q_u->switch_value)
		{
			case 0x10:
			{
				info = (void*)&id10;
				status = get_user_info_10(&id10, rid) ? 0 : NT_STATUS_NO_SUCH_USER;
				break;
			}
#if 0
/* whoops - got this wrong.  i think.  or don't understand what's happening. */
			case 0x11:
			{
				NTTIME expire;
				info = (void*)&id11;
				
				expire.low  = 0xffffffff;
				expire.high = 0x7fffffff;

				make_sam_user_info11(&id11, &expire, "BROOKFIELDS$", 0x03ef, 0x201, 0x0080);

				break;
			}
#endif
			case 21:
			{
				info = (void*)&id21;
				status = get_user_info_21(&id21, rid) ? 0 : NT_STATUS_NO_SUCH_USER;
				break;
			}

			default:
			{
				status = NT_STATUS_INVALID_INFO_CLASS;

				break;
			}
		}
	}

	init_samr_r_query_userinfo(&r_u, q_u->switch_value, info, status);

	/* store the response in the SMB stream */
	samr_io_r_query_userinfo("", &r_u, rdata, 0);

	DEBUG(5,("samr_reply_query_userinfo: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_query_userinfo
 ********************************************************************/
static BOOL api_samr_query_userinfo( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_QUERY_USERINFO q_u;

	/* grab the samr unknown 24 */
	samr_io_q_query_userinfo("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_query_userinfo(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_query_usergroups
 ********************************************************************/
static void samr_reply_query_usergroups(SAMR_Q_QUERY_USERGROUPS *q_u,
				prs_struct *rdata)
{
	SAMR_R_QUERY_USERGROUPS r_u;
	uint32 status = 0x0;

	struct sam_passwd *sam_pass;
	DOM_GID *gids = NULL;
	int num_groups = 0;
	uint32 rid;

	DEBUG(5,("samr_query_usergroups: %d\n", __LINE__));

	/* find the policy handle.  open a policy on it. */
	if (status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->pol)) == -1))
	{
		status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
	}

	/* find the user's rid */
	if (status == 0x0 && (rid = get_lsa_policy_samr_rid(&(q_u->pol))) == 0xffffffff)
	{
		status = NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (status == 0x0)
	{
		become_root(True);
		sam_pass = getsam21pwrid(rid);
		unbecome_root(True);

		if (sam_pass == NULL)
		{
			status = 0xC0000000 | NT_STATUS_NO_SUCH_USER;
		}
	}

	if (status == 0x0)
	{
		pstring groups;
		get_domain_user_groups(groups, sam_pass->smb_name);
                gids = NULL;
		num_groups = make_dom_gids(groups, &gids);
	}

	/* construct the response.  lkclXXXX: gids are not copied! */
	init_samr_r_query_usergroups(&r_u, num_groups, gids, status);

	/* store the response in the SMB stream */
	samr_io_r_query_usergroups("", &r_u, rdata, 0);

	if (gids)
	{
		free((char *)gids);
	}

	DEBUG(5,("samr_query_usergroups: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_query_usergroups
 ********************************************************************/
static BOOL api_samr_query_usergroups( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_QUERY_USERGROUPS q_u;
	/* grab the samr unknown 32 */
	samr_io_q_query_usergroups("", &q_u, data, 0);

	/* construct reply. */
	samr_reply_query_usergroups(&q_u, rdata);

	return True;
}


/*******************************************************************
 samr_reply_query_dom_info
 ********************************************************************/
static void samr_reply_query_dom_info(SAMR_Q_QUERY_DOMAIN_INFO *q_u,
				prs_struct *rdata)
{
	SAMR_R_QUERY_DOMAIN_INFO r_u;
	SAM_UNK_CTR ctr;
	uint16 switch_value = 0x0;
	uint32 status = 0x0;

	ZERO_STRUCT(r_u);
	ZERO_STRUCT(ctr);

	r_u.ctr = &ctr;

	DEBUG(5,("samr_reply_query_dom_info: %d\n", __LINE__));

	/* find the policy handle.  open a policy on it. */
	if (r_u.status == 0x0 && (find_lsa_policy_by_hnd(&(q_u->domain_pol)) == -1))
	{
		r_u.status = 0xC0000000 | NT_STATUS_INVALID_HANDLE;
		DEBUG(5,("samr_reply_query_dom_info: invalid handle\n"));
	}

	if (status == 0x0)
	{
		switch (q_u->switch_value)
		{
			case 0x02:
			{
				switch_value = 0x2;
				init_unk_info2(&ctr.info.inf2, global_myworkgroup, global_myname);

				break;
			}
			default:
			{
				status = 0xC0000000 | NT_STATUS_INVALID_INFO_CLASS;
				break;
			}
		}
	}

	init_samr_r_query_dom_info(&r_u, switch_value, &ctr, status);

	/* store the response in the SMB stream */
	samr_io_r_query_dom_info("", &r_u, rdata, 0);

	DEBUG(5,("samr_query_dom_info: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_query_dom_info
 ********************************************************************/
static BOOL api_samr_query_dom_info( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_QUERY_DOMAIN_INFO q_e;

	/* grab the samr unknown 8 command */
	samr_io_q_query_dom_info("", &q_e, data, 0);

	/* construct reply. */
	samr_reply_query_dom_info(&q_e, rdata);

	return True;
}



/*******************************************************************
 samr_reply_unknown_32
 ********************************************************************/
static void samr_reply_unknown_32(SAMR_Q_UNKNOWN_32 *q_u,
				prs_struct *rdata,
				int status)
{
	int i;
	SAMR_R_UNKNOWN_32 r_u;

	/* set up the SAMR unknown_32 response */
	memset((char *)r_u.pol.data, '\0', POL_HND_SIZE);
	if (status == 0)
	{
		for (i = 4; i < POL_HND_SIZE; i++)
		{
			r_u.pol.data[i] = i+1;
		}
	}

	init_dom_rid4(&(r_u.rid4), 0x0030, 0, 0);
	r_u.status    = status;

	DEBUG(5,("samr_unknown_32: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_unknown_32("", &r_u, rdata, 0);

	DEBUG(5,("samr_unknown_32: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_unknown_32
 ********************************************************************/
static BOOL api_samr_unknown_32( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	uint32 status = 0;
	struct sam_passwd *sam_pass;
	fstring mach_acct;

	SAMR_Q_UNKNOWN_32 q_u;

	/* grab the samr unknown 32 */
	samr_io_q_unknown_32("", &q_u, data, 0);

	/* find the machine account: tell the caller if it exists.
	   lkclXXXX i have *no* idea if this is a problem or not
	   or even if you are supposed to construct a different
	   reply if the account already exists...
	 */

	fstrcpy(mach_acct, dos_unistrn2(q_u.uni_mach_acct.buffer,
	                            q_u.uni_mach_acct.uni_str_len));

	become_root(True);
	sam_pass = getsam21pwnam(mach_acct);
	unbecome_root(True);

	if (sam_pass != NULL)
	{
		/* machine account exists: say so */
		status = 0xC0000000 | NT_STATUS_USER_EXISTS;
	}
	else
	{
		/* this could cause trouble... */
		DEBUG(0,("trouble!\n"));
		status = 0;
	}

	/* construct reply. */
	samr_reply_unknown_32(&q_u, rdata, status);

	return True;
}


/*******************************************************************
 samr_reply_connect_anon
 ********************************************************************/
static void samr_reply_connect_anon(SAMR_Q_CONNECT_ANON *q_u,
				prs_struct *rdata)
{
	SAMR_R_CONNECT_ANON r_u;
	BOOL pol_open = False;

	/* set up the SAMR connect_anon response */

	r_u.status = 0x0;
	/* get a (unique) handle.  open a policy on it. */
	if (r_u.status == 0x0 && !(pol_open = open_lsa_policy_hnd(&(r_u.connect_pol))))
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* associate the domain SID with the (unique) handle. */
	if (r_u.status == 0x0 && !set_lsa_policy_samr_pol_status(&(r_u.connect_pol), q_u->unknown_0))
	{
		/* oh, whoops.  don't know what error message to return, here */
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (r_u.status != 0 && pol_open)
	{
		close_lsa_policy_hnd(&(r_u.connect_pol));
	}

	DEBUG(5,("samr_connect_anon: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_connect_anon("", &r_u, rdata, 0);

	DEBUG(5,("samr_connect_anon: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_connect_anon
 ********************************************************************/
static BOOL api_samr_connect_anon( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_CONNECT_ANON q_u;

	/* grab the samr open policy */
	samr_io_q_connect_anon("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_connect_anon(&q_u, rdata);

	return True;
}

/*******************************************************************
 samr_reply_connect
 ********************************************************************/
static void samr_reply_connect(SAMR_Q_CONNECT *q_u,
				prs_struct *rdata)
{
	SAMR_R_CONNECT r_u;
	BOOL pol_open = False;

	/* set up the SAMR connect response */

	r_u.status = 0x0;
	/* get a (unique) handle.  open a policy on it. */
	if (r_u.status == 0x0 && !(pol_open = open_lsa_policy_hnd(&(r_u.connect_pol))))
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* associate the domain SID with the (unique) handle. */
	if (r_u.status == 0x0 && !set_lsa_policy_samr_pol_status(&(r_u.connect_pol), q_u->unknown_0))
	{
		/* oh, whoops.  don't know what error message to return, here */
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (r_u.status != 0 && pol_open)
	{
		close_lsa_policy_hnd(&(r_u.connect_pol));
	}

	DEBUG(5,("samr_connect: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_connect("", &r_u, rdata, 0);

	DEBUG(5,("samr_connect: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_connect
 ********************************************************************/
static BOOL api_samr_connect( uint16 vuid, prs_struct *data, prs_struct *rdata)
{
	SAMR_Q_CONNECT q_u;

	/* grab the samr open policy */
	samr_io_q_connect("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_connect(&q_u, rdata);

	return True;
}

/*******************************************************************
 samr_reply_open_alias
 ********************************************************************/
static void samr_reply_open_alias(SAMR_Q_OPEN_ALIAS *q_u,
				prs_struct *rdata)
{
	SAMR_R_OPEN_ALIAS r_u;
	BOOL pol_open = False;

	/* set up the SAMR open_alias response */

	r_u.status = 0x0;
	/* get a (unique) handle.  open a policy on it. */
	if (r_u.status == 0x0 && !(pol_open = open_lsa_policy_hnd(&(r_u.pol))))
	{
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* associate a RID with the (unique) handle. */
	if (r_u.status == 0x0 && !set_lsa_policy_samr_rid(&(r_u.pol), q_u->rid_alias))
	{
		/* oh, whoops.  don't know what error message to return, here */
		r_u.status = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (r_u.status != 0 && pol_open)
	{
		close_lsa_policy_hnd(&(r_u.pol));
	}

	DEBUG(5,("samr_open_alias: %d\n", __LINE__));

	/* store the response in the SMB stream */
	samr_io_r_open_alias("", &r_u, rdata, 0);

	DEBUG(5,("samr_open_alias: %d\n", __LINE__));

}

/*******************************************************************
 api_samr_open_alias
 ********************************************************************/
static BOOL api_samr_open_alias( uint16 vuid, prs_struct *data, prs_struct *rdata)
                                
{
	SAMR_Q_OPEN_ALIAS q_u;

	/* grab the samr open policy */
	samr_io_q_open_alias("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	samr_reply_open_alias(&q_u, rdata);

	return True;
}

/*******************************************************************
 array of \PIPE\samr operations
 ********************************************************************/
static struct api_struct api_samr_cmds [] =
{
	{ "SAMR_CLOSE_HND"        , SAMR_CLOSE_HND        , api_samr_close_hnd        },
	{ "SAMR_CONNECT"          , SAMR_CONNECT          , api_samr_connect          },
	{ "SAMR_CONNECT_ANON"     , SAMR_CONNECT_ANON     , api_samr_connect_anon     },
	{ "SAMR_ENUM_DOM_USERS"   , SAMR_ENUM_DOM_USERS   , api_samr_enum_dom_users   },
	{ "SAMR_ENUM_DOM_GROUPS"  , SAMR_ENUM_DOM_GROUPS  , api_samr_enum_dom_groups  },
	{ "SAMR_ENUM_DOM_ALIASES" , SAMR_ENUM_DOM_ALIASES , api_samr_enum_dom_aliases },
	{ "SAMR_LOOKUP_IDS"       , SAMR_LOOKUP_IDS       , api_samr_lookup_ids       },
	{ "SAMR_LOOKUP_NAMES"     , SAMR_LOOKUP_NAMES     , api_samr_lookup_names     },
	{ "SAMR_OPEN_USER"        , SAMR_OPEN_USER        , api_samr_open_user        },
	{ "SAMR_QUERY_USERINFO"   , SAMR_QUERY_USERINFO   , api_samr_query_userinfo   },
	{ "SAMR_QUERY_DOMAIN_INFO", SAMR_QUERY_DOMAIN_INFO, api_samr_query_dom_info        },
	{ "SAMR_QUERY_USERGROUPS" , SAMR_QUERY_USERGROUPS , api_samr_query_usergroups },
	{ "SAMR_QUERY_DISPINFO"   , SAMR_QUERY_DISPINFO   , api_samr_query_dispinfo   },
	{ "SAMR_QUERY_ALIASINFO"  , SAMR_QUERY_ALIASINFO  , api_samr_query_aliasinfo  },
	{ "SAMR_0x32"             , 0x32                  , api_samr_unknown_32       },
	{ "SAMR_UNKNOWN_12"       , SAMR_UNKNOWN_12       , api_samr_unknown_12       },
	{ "SAMR_UNKNOWN_38"       , SAMR_UNKNOWN_38       , api_samr_unknown_38       },
	{ "SAMR_CHGPASSWD_USER"   , SAMR_CHGPASSWD_USER   , api_samr_chgpasswd_user   },
	{ "SAMR_OPEN_ALIAS"       , SAMR_OPEN_ALIAS       , api_samr_open_alias       },
	{ "SAMR_OPEN_DOMAIN"      , SAMR_OPEN_DOMAIN      , api_samr_open_domain      },
	{ "SAMR_UNKNOWN_3"        , SAMR_UNKNOWN_3        , api_samr_unknown_3        },
	{ "SAMR_UNKNOWN_2C"       , SAMR_UNKNOWN_2C       , api_samr_unknown_2c       },
	{ NULL                    , 0                     , NULL                      }
};

/*******************************************************************
 receives a samr pipe and responds.
 ********************************************************************/
BOOL api_samr_rpc(pipes_struct *p, prs_struct *data)
{
    return api_rpcTNP(p, "api_samr_rpc", api_samr_cmds, data);
}
