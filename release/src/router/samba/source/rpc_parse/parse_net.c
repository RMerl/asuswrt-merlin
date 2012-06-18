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

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL net_io_neg_flags(char *desc, NEG_FLAGS *neg, prs_struct *ps, int depth)
{
	if (neg == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_neg_flags");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("neg_flags", ps, depth, &neg->neg_flags))
		return False;

	return True;
}

/*******************************************************************
 Inits a NETLOGON_INFO_3 structure.
********************************************************************/

static void init_netinfo_3(NETLOGON_INFO_3 *info, uint32 flags, uint32 logon_attempts)
{
	info->flags          = flags;
	info->logon_attempts = logon_attempts;
	info->reserved_1     = 0x0;
	info->reserved_2     = 0x0;
	info->reserved_3     = 0x0;
	info->reserved_4     = 0x0;
	info->reserved_5     = 0x0;
}

/*******************************************************************
 Reads or writes a NETLOGON_INFO_3 structure.
********************************************************************/

static BOOL net_io_netinfo_3(char *desc,  NETLOGON_INFO_3 *info, prs_struct *ps, int depth)
{
	if (info == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_netinfo_3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("flags         ", ps, depth, &info->flags))
		return False;
	if(!prs_uint32("logon_attempts", ps, depth, &info->logon_attempts))
		return False;
	if(!prs_uint32("reserved_1    ", ps, depth, &info->reserved_1))
		return False;
	if(!prs_uint32("reserved_2    ", ps, depth, &info->reserved_2))
		return False;
	if(!prs_uint32("reserved_3    ", ps, depth, &info->reserved_3))
		return False;
	if(!prs_uint32("reserved_4    ", ps, depth, &info->reserved_4))
		return False;
	if(!prs_uint32("reserved_5    ", ps, depth, &info->reserved_5))
		return False;

	return True;
}


/*******************************************************************
 Inits a NETLOGON_INFO_1 structure.
********************************************************************/

static void init_netinfo_1(NETLOGON_INFO_1 *info, uint32 flags, uint32 pdc_status)
{
	info->flags      = flags;
	info->pdc_status = pdc_status;
}

/*******************************************************************
 Reads or writes a NETLOGON_INFO_1 structure.
********************************************************************/

static BOOL net_io_netinfo_1(char *desc, NETLOGON_INFO_1 *info, prs_struct *ps, int depth)
{
	if (info == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_netinfo_1");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("flags     ", ps, depth, &info->flags))
		return False;
	if(!prs_uint32("pdc_status", ps, depth, &info->pdc_status))
		return False;

	return True;
}

/*******************************************************************
 Inits a NETLOGON_INFO_2 structure.
********************************************************************/

static void init_netinfo_2(NETLOGON_INFO_2 *info, uint32 flags, uint32 pdc_status,
				uint32 tc_status, char *trusted_dc_name)
{
	int len_dc_name = strlen(trusted_dc_name);
	info->flags      = flags;
	info->pdc_status = pdc_status;
	info->ptr_trusted_dc_name = 1;
	info->tc_status  = tc_status;

	if (trusted_dc_name != NULL)
		init_unistr2(&(info->uni_trusted_dc_name), trusted_dc_name, len_dc_name+1);
	else
		init_unistr2(&(info->uni_trusted_dc_name), "", 1);
}

/*******************************************************************
 Reads or writes a NETLOGON_INFO_2 structure.
********************************************************************/

static BOOL net_io_netinfo_2(char *desc, NETLOGON_INFO_2 *info, prs_struct *ps, int depth)
{
	if (info == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_netinfo_2");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("flags              ", ps, depth, &info->flags))
		return False;
	if(!prs_uint32("pdc_status         ", ps, depth, &info->pdc_status))
		return False;
	if(!prs_uint32("ptr_trusted_dc_name", ps, depth, &info->ptr_trusted_dc_name))
		return False;
	if(!prs_uint32("tc_status          ", ps, depth, &info->tc_status))
		return False;

	if (info->ptr_trusted_dc_name != 0) {
		if(!smb_io_unistr2("unistr2", &info->uni_trusted_dc_name, info->ptr_trusted_dc_name, ps, depth))
			return False;
	}

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an NET_Q_LOGON_CTRL2 structure.
********************************************************************/

BOOL net_io_q_logon_ctrl2(char *desc, NET_Q_LOGON_CTRL2 *q_l, prs_struct *ps, int depth)
{
	if (q_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_logon_ctrl2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr          ", ps, depth, &q_l->ptr))
		return False;

	if(!smb_io_unistr2 ("", &q_l->uni_server_name, q_l->ptr, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("function_code", ps, depth, &q_l->function_code))
		return False;
	if(!prs_uint32("query_level  ", ps, depth, &q_l->query_level))
		return False;
	if(!prs_uint32("switch_value ", ps, depth, &q_l->switch_value))
		return False;

	return True;
}

/*******************************************************************
 Inits an NET_R_LOGON_CTRL2 structure.
********************************************************************/

void init_r_logon_ctrl2(NET_R_LOGON_CTRL2 *r_l, uint32 query_level,
				uint32 flags, uint32 pdc_status, uint32 logon_attempts,
				uint32 tc_status, char *trusted_domain_name)
{
	DEBUG(5,("make_r_logon_ctrl2\n"));

	r_l->switch_value  = query_level; /* should only be 0x1 */

	switch (query_level) {
	case 1:
		r_l->ptr = 1; /* undocumented pointer */
		init_netinfo_1(&r_l->logon.info1, flags, pdc_status);	
		r_l->status = 0;
		break;
	case 2:
		r_l->ptr = 1; /* undocumented pointer */
		init_netinfo_2(&r_l->logon.info2, flags, pdc_status,
		               tc_status, trusted_domain_name);	
		r_l->status = 0;
		break;
	case 3:
		r_l->ptr = 1; /* undocumented pointer */
		init_netinfo_3(&(r_l->logon.info3), flags, logon_attempts);	
		r_l->status = 0;
		break;
	default:
		DEBUG(2,("init_r_logon_ctrl2: unsupported switch value %d\n",
			r_l->switch_value));
		r_l->ptr = 0; /* undocumented pointer */

		/* take a guess at an error code... */
		r_l->status = NT_STATUS_INVALID_INFO_CLASS;
		break;
	}
}

/*******************************************************************
 Reads or writes an NET_R_LOGON_CTRL2 structure.
********************************************************************/

BOOL net_io_r_logon_ctrl2(char *desc, NET_R_LOGON_CTRL2 *r_l, prs_struct *ps, int depth)
{
	if (r_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_logon_ctrl2");
	depth++;

	if(!prs_uint32("switch_value ", ps, depth, &r_l->switch_value))
		return False;
	if(!prs_uint32("ptr          ", ps, depth, &r_l->ptr))
		return False;

	if (r_l->ptr != 0) {
		switch (r_l->switch_value) {
		case 1:
			if(!net_io_netinfo_1("", &r_l->logon.info1, ps, depth))
				return False;
			break;
		case 2:
			if(!net_io_netinfo_2("", &r_l->logon.info2, ps, depth))
				return False;
			break;
		case 3:
			if(!net_io_netinfo_3("", &r_l->logon.info3, ps, depth))
				return False;
			break;
		default:
			DEBUG(2,("net_io_r_logon_ctrl2: unsupported switch value %d\n",
				r_l->switch_value));
			break;
		}
	}

	if(!prs_uint32("status       ", ps, depth, &r_l->status))
		return False;

	return True;
}

/*******************************************************************
 Inits an NET_R_TRUST_DOM_LIST structure.
********************************************************************/

void init_r_trust_dom(NET_R_TRUST_DOM_LIST *r_t,
			uint32 num_doms, char *dom_name)
{
	int i = 0;

	DEBUG(5,("make_r_trust_dom\n"));

	for (i = 0; i < MAX_TRUST_DOMS; i++) {
		r_t->uni_trust_dom_name[i].uni_str_len = 0;
		r_t->uni_trust_dom_name[i].uni_max_len = 0;
	}
	if (num_doms > MAX_TRUST_DOMS)
		num_doms = MAX_TRUST_DOMS;

	for (i = 0; i < num_doms; i++) {
		fstring domain_name;
		fstrcpy(domain_name, dom_name);
		strupper(domain_name);
		init_unistr2(&r_t->uni_trust_dom_name[i], domain_name, strlen(domain_name)+1);
		/* the use of UNISTR2 here is non-standard. */
		r_t->uni_trust_dom_name[i].undoc = 0x1;
	}
	
	r_t->status = 0;
}

/*******************************************************************
 Reads or writes an NET_R_TRUST_DOM_LIST structure.
********************************************************************/

BOOL net_io_r_trust_dom(char *desc, NET_R_TRUST_DOM_LIST *r_t, prs_struct *ps, int depth)
{
	int i;
	if (r_t == NULL)
		 return False;

	prs_debug(ps, depth, desc, "net_io_r_trust_dom");
	depth++;

	for (i = 0; i < MAX_TRUST_DOMS; i++) {
		if (r_t->uni_trust_dom_name[i].uni_str_len == 0)
			break;
		if(!smb_io_unistr2("", &r_t->uni_trust_dom_name[i], True, ps, depth))
			 return False;
	}

	if(!prs_uint32("status", ps, depth, &r_t->status))
		 return False;

	return True;
}


/*******************************************************************
 Reads or writes an NET_Q_TRUST_DOM_LIST structure.
********************************************************************/

BOOL net_io_q_trust_dom(char *desc, NET_Q_TRUST_DOM_LIST *q_l, prs_struct *ps, int depth)
{
	if (q_l == NULL)
		 return False;

	prs_debug(ps, depth, desc, "net_io_q_trust_dom");
	depth++;

	if(!prs_uint32("ptr          ", ps, depth, &q_l->ptr))
		 return False;
	if(!smb_io_unistr2 ("", &q_l->uni_server_name, q_l->ptr, ps, depth))
		 return False;

	if(!prs_align(ps))
		 return False;

	if(!prs_uint32("function_code", ps, depth, &q_l->function_code))
		 return False;

	return True;
}

/*******************************************************************
 Inits an NET_Q_REQ_CHAL structure.
********************************************************************/

void init_q_req_chal(NET_Q_REQ_CHAL *q_c,
				char *logon_srv, char *logon_clnt,
				DOM_CHAL *clnt_chal)
{
	DEBUG(5,("make_q_req_chal: %d\n", __LINE__));

	q_c->undoc_buffer = 1; /* don't know what this buffer is */

	init_unistr2(&q_c->uni_logon_srv, logon_srv , strlen(logon_srv )+1);
	init_unistr2(&q_c->uni_logon_clnt, logon_clnt, strlen(logon_clnt)+1);

	memcpy(q_c->clnt_chal.data, clnt_chal->data, sizeof(clnt_chal->data));

	DEBUG(5,("make_q_req_chal: %d\n", __LINE__));
}

/*******************************************************************
 Reads or writes an NET_Q_REQ_CHAL structure.
********************************************************************/

BOOL net_io_q_req_chal(char *desc,  NET_Q_REQ_CHAL *q_c, prs_struct *ps, int depth)
{
	int old_align;

	if (q_c == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_req_chal");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!prs_uint32("undoc_buffer", ps, depth, &q_c->undoc_buffer))
		return False;

	if(!smb_io_unistr2("", &q_c->uni_logon_srv, True, ps, depth)) /* logon server unicode string */
		return False;
	if(!smb_io_unistr2("", &q_c->uni_logon_clnt, True, ps, depth)) /* logon client unicode string */
		return False;

	old_align = ps->align;
	ps->align = 0;
	/* client challenge is _not_ aligned after the unicode strings */
	if(!smb_io_chal("", &q_c->clnt_chal, ps, depth)) {
		/* client challenge */
		ps->align = old_align;
		return False;
	}
	ps->align = old_align;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_r_req_chal(char *desc, NET_R_REQ_CHAL *r_c, prs_struct *ps, int depth)
{
	if (r_c == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_req_chal");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!smb_io_chal("", &r_c->srv_chal, ps, depth)) /* server challenge */
		return False;

	if(!prs_uint32("status", ps, depth, &r_c->status))
		return False;

	return True;
}


/*******************************************************************
 Inits a NET_Q_AUTH_2 struct.
********************************************************************/

void init_q_auth_2(NET_Q_AUTH_2 *q_a,
		char *logon_srv, char *acct_name, uint16 sec_chan, char *comp_name,
		DOM_CHAL *clnt_chal, uint32 clnt_flgs)
{
	DEBUG(5,("init_q_auth_2: %d\n", __LINE__));

	init_log_info(&q_a->clnt_id, logon_srv, acct_name, sec_chan, comp_name);
	memcpy(q_a->clnt_chal.data, clnt_chal->data, sizeof(clnt_chal->data));
	q_a->clnt_flgs.neg_flags = clnt_flgs;

	DEBUG(5,("init_q_auth_2: %d\n", __LINE__));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_q_auth_2(char *desc, NET_Q_AUTH_2 *q_a, prs_struct *ps, int depth)
{
	int old_align;
	if (q_a == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_auth_2");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!smb_io_log_info ("", &q_a->clnt_id, ps, depth)) /* client identification info */
		return False;
	/* client challenge is _not_ aligned */
	old_align = ps->align;
	ps->align = 0;
	if(!smb_io_chal("", &q_a->clnt_chal, ps, depth)) {
		/* client-calculated credentials */
		ps->align = old_align;
		return False;
	}
	ps->align = old_align;
	if(!net_io_neg_flags("", &q_a->clnt_flgs, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_r_auth_2(char *desc, NET_R_AUTH_2 *r_a, prs_struct *ps, int depth)
{
	if (r_a == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_auth_2");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!smb_io_chal("", &r_a->srv_chal, ps, depth)) /* server challenge */
		return False;
	if(!net_io_neg_flags("", &r_a->srv_flgs, ps, depth))
		return False;

	if(!prs_uint32("status", ps, depth, &r_a->status))
		return False;

	return True;
}


/*******************************************************************
 Inits a NET_Q_SRV_PWSET.
********************************************************************/

void init_q_srv_pwset(NET_Q_SRV_PWSET *q_s, char *logon_srv, char *acct_name, 
                uint16 sec_chan, char *comp_name, DOM_CRED *cred, char nt_cypher[16])
{
	DEBUG(5,("make_q_srv_pwset\n"));

	init_clnt_info(&q_s->clnt_id, logon_srv, acct_name, sec_chan, comp_name, cred);

	memcpy(q_s->pwd, nt_cypher, sizeof(q_s->pwd)); 
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_q_srv_pwset(char *desc, NET_Q_SRV_PWSET *q_s, prs_struct *ps, int depth)
{
	if (q_s == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_srv_pwset");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!smb_io_clnt_info("", &q_s->clnt_id, ps, depth)) /* client identification/authentication info */
		return False;
	if(!prs_uint8s (False, "pwd", ps, depth, q_s->pwd, 16)) /* new password - undocumented */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_r_srv_pwset(char *desc, NET_R_SRV_PWSET *r_s, prs_struct *ps, int depth)
{
	if (r_s == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_srv_pwset");
	depth++;

	if(!prs_align(ps))
		return False;
    
	if(!smb_io_cred("", &r_s->srv_cred, ps, depth)) /* server challenge */
		return False;

	if(!prs_uint32("status", ps, depth, &r_s->status))
		return False;

	return True;
}

/*************************************************************************
 Init DOM_SID2 array from a string containing multiple sids
 *************************************************************************/

static int init_dom_sid2s(char *sids_str, DOM_SID2 *sids, int max_sids)
{
	char *ptr;
	pstring s2;
	int count = 0;

	DEBUG(4,("init_dom_sid2s: %s\n", sids_str ? sids_str:""));

	if(sids_str) {
		for (count = 0, ptr = sids_str; 
	   	  next_token(&ptr, s2, NULL, sizeof(s2)) && count < max_sids; count++) {
			DOM_SID tmpsid;
			string_to_sid(&tmpsid, s2);
			init_dom_sid2(&sids[count], &tmpsid);
		}
	}

	return count;
}

/*******************************************************************
 Inits a NET_ID_INFO_1 structure.
********************************************************************/

void init_id_info1(NET_ID_INFO_1 *id, char *domain_name,
				uint32 param_ctrl, uint32 log_id_low, uint32 log_id_high,
				char *user_name, char *wksta_name,
				char sess_key[16],
				unsigned char lm_cypher[16], unsigned char nt_cypher[16])
{
	int len_domain_name = strlen(domain_name);
	int len_user_name   = strlen(user_name  );
	int len_wksta_name  = strlen(wksta_name );

	unsigned char lm_owf[16];
	unsigned char nt_owf[16];

	DEBUG(5,("make_id_info1: %d\n", __LINE__));

	id->ptr_id_info1 = 1;

	init_uni_hdr(&id->hdr_domain_name, len_domain_name);

	id->param_ctrl = param_ctrl;
	init_logon_id(&id->logon_id, log_id_low, log_id_high);

	init_uni_hdr(&id->hdr_user_name, len_user_name);
	init_uni_hdr(&id->hdr_wksta_name, len_wksta_name);

	if (lm_cypher && nt_cypher) {
		unsigned char key[16];
#ifdef DEBUG_PASSWORD
		DEBUG(100,("lm cypher:"));
		dump_data(100, (char *)lm_cypher, 16);

		DEBUG(100,("nt cypher:"));
		dump_data(100, (char *)nt_cypher, 16);
#endif

		memset(key, 0, 16);
		memcpy(key, sess_key, 8);

		memcpy(lm_owf, lm_cypher, 16);
		SamOEMhash(lm_owf, key, False);
		memcpy(nt_owf, nt_cypher, 16);
		SamOEMhash(nt_owf, key, False);

#ifdef DEBUG_PASSWORD
		DEBUG(100,("encrypt of lm owf password:"));
		dump_data(100, (char *)lm_owf, 16);

		DEBUG(100,("encrypt of nt owf password:"));
		dump_data(100, (char *)nt_owf, 16);
#endif
		/* set up pointers to cypher blocks */
		lm_cypher = lm_owf;
		nt_cypher = nt_owf;
	}

	init_owf_info(&id->lm_owf, lm_cypher);
	init_owf_info(&id->nt_owf, nt_cypher);

	init_unistr2(&id->uni_domain_name, domain_name, len_domain_name);
	init_unistr2(&id->uni_user_name, user_name, len_user_name);
	init_unistr2(&id->uni_wksta_name, wksta_name, len_wksta_name);
}

/*******************************************************************
 Reads or writes an NET_ID_INFO_1 structure.
********************************************************************/

static BOOL net_io_id_info1(char *desc,  NET_ID_INFO_1 *id, prs_struct *ps, int depth)
{
	if (id == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_id_info1");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_id_info1", ps, depth, &id->ptr_id_info1))
		return False;

	if (id->ptr_id_info1 != 0) {
		if(!smb_io_unihdr("unihdr", &id->hdr_domain_name, ps, depth))
			return False;

		if(!prs_uint32("param_ctrl", ps, depth, &id->param_ctrl))
			return False;
		if(!smb_io_logon_id("", &id->logon_id, ps, depth))
			return False;

		if(!smb_io_unihdr("unihdr", &id->hdr_user_name, ps, depth))
			return False;
		if(!smb_io_unihdr("unihdr", &id->hdr_wksta_name, ps, depth))
			return False;

		if(!smb_io_owf_info("", &id->lm_owf, ps, depth))
			return False;
		if(!smb_io_owf_info("", &id->nt_owf, ps, depth))
			return False;

		if(!smb_io_unistr2("unistr2", &id->uni_domain_name,
				id->hdr_domain_name.buffer, ps, depth))
			return False;
		if(!smb_io_unistr2("unistr2", &id->uni_user_name,
				id->hdr_user_name.buffer, ps, depth))
			return False;
		if(!smb_io_unistr2("unistr2", &id->uni_wksta_name,
				id->hdr_wksta_name.buffer, ps, depth))
			return False;
	}

	return True;
}

/*******************************************************************
Inits a NET_ID_INFO_2 structure.

This is a network logon packet. The log_id parameters
are what an NT server would generate for LUID once the
user is logged on. I don't think we care about them.

Note that this has no access to the NT and LM hashed passwords,
so it forwards the challenge, and the NT and LM responses (24
bytes each) over the secure channel to the Domain controller
for it to say yea or nay. This is the preferred method of 
checking for a logon as it doesn't export the password
hashes to anyone who has compromised the secure channel. JRA.
********************************************************************/

void init_id_info2(NET_ID_INFO_2 *id, char *domain_name,
				uint32 param_ctrl, uint32 log_id_low, uint32 log_id_high,
				char *user_name, char *wksta_name,
				unsigned char lm_challenge[8],
				unsigned char lm_chal_resp[24],
				unsigned char nt_chal_resp[24])
{
	int len_domain_name = strlen(domain_name);
	int len_user_name   = strlen(user_name  );
	int len_wksta_name  = strlen(wksta_name );
	int nt_chal_resp_len = ((nt_chal_resp != NULL) ? 24 : 0);
	int lm_chal_resp_len = ((lm_chal_resp != NULL) ? 24 : 0);
	unsigned char lm_owf[24];
	unsigned char nt_owf[24];

	DEBUG(5,("init_id_info2: %d\n", __LINE__));

	id->ptr_id_info2 = 1;

	init_uni_hdr(&id->hdr_domain_name, len_domain_name);

	id->param_ctrl = param_ctrl;
	init_logon_id(&id->logon_id, log_id_low, log_id_high);

	init_uni_hdr(&id->hdr_user_name, len_user_name);
	init_uni_hdr(&id->hdr_wksta_name, len_wksta_name);

	if (nt_chal_resp) {
		/* oops.  can only send what-ever-it-is direct */
		memcpy(nt_owf, nt_chal_resp, 24);
		nt_chal_resp = nt_owf;
	}
	if (lm_chal_resp) {
		/* oops.  can only send what-ever-it-is direct */
		memcpy(lm_owf, lm_chal_resp, 24);
		lm_chal_resp = lm_owf;
	}

	memcpy(id->lm_chal, lm_challenge, sizeof(id->lm_chal));
	init_str_hdr(&id->hdr_nt_chal_resp, 24, nt_chal_resp_len, (nt_chal_resp != NULL) ? 1 : 0);
	init_str_hdr(&id->hdr_lm_chal_resp, 24, lm_chal_resp_len, (lm_chal_resp != NULL) ? 1 : 0);

	init_unistr2(&id->uni_domain_name, domain_name, len_domain_name);
	init_unistr2(&id->uni_user_name, user_name, len_user_name);
	init_unistr2(&id->uni_wksta_name, wksta_name, len_wksta_name);

	init_string2(&id->nt_chal_resp, (char *)nt_chal_resp, nt_chal_resp_len);
	init_string2(&id->lm_chal_resp, (char *)lm_chal_resp, lm_chal_resp_len);
}

/*******************************************************************
 Reads or writes an NET_ID_INFO_2 structure.
********************************************************************/

static BOOL net_io_id_info2(char *desc,  NET_ID_INFO_2 *id, prs_struct *ps, int depth)
{
	if (id == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_id_info2");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_id_info2", ps, depth, &id->ptr_id_info2))
		return False;

	if (id->ptr_id_info2 != 0) {
		if(!smb_io_unihdr("unihdr", &id->hdr_domain_name, ps, depth))
			return False;

		if(!prs_uint32("param_ctrl", ps, depth, &id->param_ctrl))
			return False;
		if(!smb_io_logon_id("", &id->logon_id, ps, depth))
			return False;

		if(!smb_io_unihdr("unihdr", &id->hdr_user_name, ps, depth))
			return False;
		if(!smb_io_unihdr("unihdr", &id->hdr_wksta_name, ps, depth))
			return False;

		if(!prs_uint8s (False, "lm_chal", ps, depth, id->lm_chal, 8)) /* lm 8 byte challenge */
			return False;

		if(!smb_io_strhdr("hdr_nt_chal_resp", &id->hdr_nt_chal_resp, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_lm_chal_resp", &id->hdr_lm_chal_resp, ps, depth))
			return False;

		if(!smb_io_unistr2("uni_domain_name", &id->uni_domain_name,
				id->hdr_domain_name.buffer, ps, depth))
			return False;
		if(!smb_io_unistr2("uni_user_name  ", &id->uni_user_name,
				id->hdr_user_name.buffer, ps, depth))
			return False;
		if(!smb_io_unistr2("uni_wksta_name ", &id->uni_wksta_name,
				id->hdr_wksta_name.buffer, ps, depth))
			return False;
		if(!smb_io_string2("nt_chal_resp", &id->nt_chal_resp,
				id->hdr_nt_chal_resp.buffer, ps, depth))
			return False;
		if(!smb_io_string2("lm_chal_resp", &id->lm_chal_resp,
				id->hdr_lm_chal_resp.buffer, ps, depth))
			return False;
	}

	return True;
}


/*******************************************************************
 Inits a DOM_SAM_INFO structure.
********************************************************************/

void init_sam_info(DOM_SAM_INFO *sam,
				char *logon_srv, char *comp_name, DOM_CRED *clnt_cred,
				DOM_CRED *rtn_cred, uint16 logon_level,
				NET_ID_INFO_CTR *ctr)
{
	DEBUG(5,("init_sam_info: %d\n", __LINE__));

	init_clnt_info2(&(sam->client), logon_srv, comp_name, clnt_cred);

	if (rtn_cred != NULL) {
		sam->ptr_rtn_cred = 1;
		memcpy(&sam->rtn_cred, rtn_cred, sizeof(sam->rtn_cred));
	} else {
		sam->ptr_rtn_cred = 0;
	}

	sam->logon_level  = logon_level;
	sam->ctr          = ctr;
}

/*******************************************************************
 Reads or writes a DOM_SAM_INFO structure.
********************************************************************/

static BOOL net_io_id_info_ctr(char *desc, NET_ID_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_sam_info");
	depth++;

	/* don't 4-byte align here! */

	if(!prs_uint16("switch_value ", ps, depth, &ctr->switch_value))
		return False;

	switch (ctr->switch_value) {
	case 1:
		if(!net_io_id_info1("", &ctr->auth.id1, ps, depth))
			return False;
		break;
	case 2:
		if(!net_io_id_info2("", &ctr->auth.id2, ps, depth))
			return False;
		break;
	default:
		/* PANIC! */
		DEBUG(4,("smb_io_sam_info: unknown switch_value!\n"));
		break;
	}

	return True;
}

/*******************************************************************
 Reads or writes a DOM_SAM_INFO structure.
 ********************************************************************/

static BOOL smb_io_sam_info(char *desc, DOM_SAM_INFO *sam, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_sam_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_clnt_info2("", &sam->client, ps, depth))
		return False;

	if(!prs_uint32("ptr_rtn_cred ", ps, depth, &sam->ptr_rtn_cred))
		return False;
	if(!smb_io_cred("", &sam->rtn_cred, ps, depth))
		return False;

	if(!prs_uint16("logon_level  ", ps, depth, &sam->logon_level))
		return False;

	if (sam->logon_level != 0 && sam->ctr != NULL) {
		if(!net_io_id_info_ctr("logon_info", sam->ctr, ps, depth))
			return False;
	}

	return True;
}

/*************************************************************************
 Init
 *************************************************************************/

void init_net_user_info3(NET_USER_INFO_3 *usr,

	NTTIME *logon_time,
	NTTIME *logoff_time,
	NTTIME *kickoff_time,
	NTTIME *pass_last_set_time,
	NTTIME *pass_can_change_time,
	NTTIME *pass_must_change_time,

	char *user_name,
	char *full_name,
	char *logon_script,
	char *profile_path,
	char *home_dir,
	char *dir_drive,

	uint16 logon_count,
	uint16 bad_pw_count,

	uint32 user_id,
	uint32 group_id,
	uint32 num_groups,
	DOM_GID *gids,
	uint32 user_flgs,

	char sess_key[16],

	char *logon_srv,
	char *logon_dom,

	DOM_SID *dom_sid,
	char *other_sids)
{
	/* only cope with one "other" sid, right now. */
	/* need to count the number of space-delimited sids */
	int i;
	int num_other_sids = 0;

	int len_user_name    = strlen(user_name   );
	int len_full_name    = strlen(full_name   );
	int len_logon_script = strlen(logon_script);
	int len_profile_path = strlen(profile_path);
	int len_home_dir     = strlen(home_dir    );
	int len_dir_drive    = strlen(dir_drive   );

	int len_logon_srv    = strlen(logon_srv);
	int len_logon_dom    = strlen(logon_dom);

    memset(usr, '\0', sizeof(*usr));

	usr->ptr_user_info = 1; /* yes, we're bothering to put USER_INFO data here */

	usr->logon_time            = *logon_time;
	usr->logoff_time           = *logoff_time;
	usr->kickoff_time          = *kickoff_time;
	usr->pass_last_set_time    = *pass_last_set_time;
	usr->pass_can_change_time  = *pass_can_change_time;
	usr->pass_must_change_time = *pass_must_change_time;

	init_uni_hdr(&usr->hdr_user_name, len_user_name);
	init_uni_hdr(&usr->hdr_full_name, len_full_name);
	init_uni_hdr(&usr->hdr_logon_script, len_logon_script);
	init_uni_hdr(&usr->hdr_profile_path, len_profile_path);
	init_uni_hdr(&usr->hdr_home_dir, len_home_dir);
	init_uni_hdr(&usr->hdr_dir_drive, len_dir_drive);

	usr->logon_count = logon_count;
	usr->bad_pw_count = bad_pw_count;

	usr->user_id = user_id;
	usr->group_id = group_id;
	usr->num_groups = num_groups;
	usr->buffer_groups = 1; /* indicates fill in groups, below, even if there are none */
	usr->user_flgs = user_flgs;

	if (sess_key != NULL)
		memcpy(usr->user_sess_key, sess_key, sizeof(usr->user_sess_key));
	else
		memset((char *)usr->user_sess_key, '\0', sizeof(usr->user_sess_key));

	init_uni_hdr(&usr->hdr_logon_srv, len_logon_srv);
	init_uni_hdr(&usr->hdr_logon_dom, len_logon_dom);

	usr->buffer_dom_id = dom_sid ? 1 : 0; /* yes, we're bothering to put a domain SID in */

	memset((char *)usr->padding, '\0', sizeof(usr->padding));

	num_other_sids = init_dom_sid2s(other_sids, usr->other_sids, LSA_MAX_SIDS);

	usr->num_other_sids = num_other_sids;
	usr->buffer_other_sids = (num_other_sids != 0) ? 1 : 0; 
	
	init_unistr2(&usr->uni_user_name, user_name, len_user_name);
	init_unistr2(&usr->uni_full_name, full_name, len_full_name);
	init_unistr2(&usr->uni_logon_script, logon_script, len_logon_script);
	init_unistr2(&usr->uni_profile_path, profile_path, len_profile_path);
	init_unistr2(&usr->uni_home_dir, home_dir, len_home_dir);
	init_unistr2(&usr->uni_dir_drive, dir_drive, len_dir_drive);

	usr->num_groups2 = num_groups;

	SMB_ASSERT_ARRAY(usr->gids, num_groups);

	for (i = 0; i < num_groups; i++)
		usr->gids[i] = gids[i];

	init_unistr2(&usr->uni_logon_srv, logon_srv, len_logon_srv);
	init_unistr2(&usr->uni_logon_dom, logon_dom, len_logon_dom);

	init_dom_sid2(&usr->dom_sid, dom_sid);
	/* "other" sids are set up above */
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL net_io_user_info3(char *desc, NET_USER_INFO_3 *usr, prs_struct *ps, int depth)
{
	int i;

	if (usr == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_lsa_user_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_user_info ", ps, depth, &usr->ptr_user_info))
		return False;

	if (usr->ptr_user_info == 0)
		return True;

	if(!smb_io_time("time", &usr->logon_time, ps, depth)) /* logon time */
		return False;
	if(!smb_io_time("time", &usr->logoff_time, ps, depth)) /* logoff time */
		return False;
	if(!smb_io_time("time", &usr->kickoff_time, ps, depth)) /* kickoff time */
		return False;
	if(!smb_io_time("time", &usr->pass_last_set_time, ps, depth)) /* password last set time */
		return False;
	if(!smb_io_time("time", &usr->pass_can_change_time , ps, depth)) /* password can change time */
		return False;
	if(!smb_io_time("time", &usr->pass_must_change_time, ps, depth)) /* password must change time */
		return False;

	if(!smb_io_unihdr("unihdr", &usr->hdr_user_name, ps, depth)) /* username unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_full_name, ps, depth)) /* user's full name unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_logon_script, ps, depth)) /* logon script unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_profile_path, ps, depth)) /* profile path unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_home_dir, ps, depth)) /* home directory unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_dir_drive, ps, depth)) /* home directory drive unicode string header */
		return False;

	if(!prs_uint16("logon_count   ", ps, depth, &usr->logon_count))  /* logon count */
		return False;
	if(!prs_uint16("bad_pw_count  ", ps, depth, &usr->bad_pw_count)) /* bad password count */
		return False;

	if(!prs_uint32("user_id       ", ps, depth, &usr->user_id))       /* User ID */
		return False;
	if(!prs_uint32("group_id      ", ps, depth, &usr->group_id))      /* Group ID */
		return False;
	if(!prs_uint32("num_groups    ", ps, depth, &usr->num_groups))    /* num groups */
		return False;
	if(!prs_uint32("buffer_groups ", ps, depth, &usr->buffer_groups)) /* undocumented buffer pointer to groups. */
		return False;
	if(!prs_uint32("user_flgs     ", ps, depth, &usr->user_flgs))     /* user flags */
		return False;

	if(!prs_uint8s(False, "user_sess_key", ps, depth, usr->user_sess_key, 16)) /* unused user session key */
		return False;

	if(!smb_io_unihdr("unihdr", &usr->hdr_logon_srv, ps, depth)) /* logon server unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &usr->hdr_logon_dom, ps, depth)) /* logon domain unicode string header */
		return False;

	if(!prs_uint32("buffer_dom_id ", ps, depth, &usr->buffer_dom_id)) /* undocumented logon domain id pointer */
		return False;
	if(!prs_uint8s (False, "padding       ", ps, depth, usr->padding, 40)) /* unused padding bytes? */
		return False;

	if(!prs_uint32("num_other_sids", ps, depth, &usr->num_other_sids)) /* 0 - num_sids */
		return False;
	if(!prs_uint32("buffer_other_sids", ps, depth, &usr->buffer_other_sids)) /* NULL - undocumented pointer to SIDs. */
		return False;
		
	if(!smb_io_unistr2("unistr2", &usr->uni_user_name, usr->hdr_user_name.buffer, ps, depth)) /* username unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_full_name, usr->hdr_full_name.buffer, ps, depth)) /* user's full name unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_logon_script, usr->hdr_logon_script.buffer, ps, depth)) /* logon script unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_profile_path, usr->hdr_profile_path.buffer, ps, depth)) /* profile path unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_home_dir, usr->hdr_home_dir.buffer, ps, depth)) /* home directory unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_dir_drive, usr->hdr_dir_drive.buffer, ps, depth)) /* home directory drive unicode string */
		return False;

	if(!prs_align(ps))
		return False;
	if(!prs_uint32("num_groups2   ", ps, depth, &usr->num_groups2))        /* num groups */
		return False;
	SMB_ASSERT_ARRAY(usr->gids, usr->num_groups2);
	for (i = 0; i < usr->num_groups2; i++) {
		if(!smb_io_gid("", &usr->gids[i], ps, depth)) /* group info */
			return False;
	}

	if(!smb_io_unistr2("unistr2", &usr->uni_logon_srv, usr->hdr_logon_srv.buffer, ps, depth)) /* logon server unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &usr->uni_logon_dom, usr->hdr_logon_srv.buffer, ps, depth)) /* logon domain unicode string */
		return False;

	if(!smb_io_dom_sid2("", &usr->dom_sid, ps, depth))           /* domain SID */
		return False;

	SMB_ASSERT_ARRAY(usr->other_sids, usr->num_other_sids);

	for (i = 0; i < usr->num_other_sids; i++) {
		if(!smb_io_dom_sid2("", &usr->other_sids[i], ps, depth)) /* other domain SIDs */
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_q_sam_logon(char *desc, NET_Q_SAM_LOGON *q_l, prs_struct *ps, int depth)
{
	if (q_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_sam_logon");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_sam_info("", &q_l->sam_id, ps, depth))           /* domain SID */
		return False;

	if(!prs_uint16("validation_level", ps, depth, &q_l->validation_level))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_r_sam_logon(char *desc, NET_R_SAM_LOGON *r_l, prs_struct *ps, int depth)
{
	if (r_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_sam_logon");
	depth++;

	if(!prs_uint32("buffer_creds", ps, depth, &r_l->buffer_creds)) /* undocumented buffer pointer */
		return False;
	if(!smb_io_cred("", &r_l->srv_creds, ps, depth)) /* server credentials.  server time stamp appears to be ignored. */
		return False;

	if(!prs_uint16("switch_value", ps, depth, &r_l->switch_value))
		return False;
	if(!prs_align(ps))
		return False;

	if (r_l->switch_value != 0) {
		if(!net_io_user_info3("", r_l->user, ps, depth))
			return False;
	}

	if(!prs_uint32("auth_resp   ", ps, depth, &r_l->auth_resp)) /* 1 - Authoritative response; 0 - Non-Auth? */
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_l->status))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_q_sam_logoff(char *desc,  NET_Q_SAM_LOGOFF *q_l, prs_struct *ps, int depth)
{
	if (q_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_q_sam_logoff");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_sam_info("", &q_l->sam_id, ps, depth))           /* domain SID */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL net_io_r_sam_logoff(char *desc, NET_R_SAM_LOGOFF *r_l, prs_struct *ps, int depth)
{
	if (r_l == NULL)
		return False;

	prs_debug(ps, depth, desc, "net_io_r_sam_logoff");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("buffer_creds", ps, depth, &r_l->buffer_creds)) /* undocumented buffer pointer */
		return False;
	if(!smb_io_cred("", &r_l->srv_creds, ps, depth)) /* server credentials.  server time stamp appears to be ignored. */
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_l->status))
		return False;

	return True;
}
