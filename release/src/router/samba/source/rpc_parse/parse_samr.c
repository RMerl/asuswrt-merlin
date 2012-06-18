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

extern int DEBUGLEVEL;


/*******************************************************************
 Inits a SAMR_Q_CLOSE_HND structure.
********************************************************************/

void init_samr_q_close_hnd(SAMR_Q_CLOSE_HND *q_c, POLICY_HND *hnd)
{
	DEBUG(5,("init_samr_q_close_hnd\n"));

	memcpy(&q_c->pol, hnd, sizeof(q_c->pol));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_close_hnd(char *desc,  SAMR_Q_CLOSE_HND *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_close_hnd");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_close_hnd(char *desc,  SAMR_R_CLOSE_HND *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_close_hnd");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &r_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

void init_samr_q_open_domain(SAMR_Q_OPEN_DOMAIN *q_u,
				POLICY_HND *connect_pol, uint32 rid,
				DOM_SID *sid)
{
	DEBUG(5,("samr_init_q_open_domain\n"));

	memcpy(&q_u->connect_pol, connect_pol, sizeof(q_u->connect_pol));
	q_u->rid = rid;
	init_dom_sid2(&q_u->dom_sid, sid);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_open_domain(char *desc, SAMR_Q_OPEN_DOMAIN *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_open_domain");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("connect_pol", &q_u->connect_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("rid", ps, depth, &q_u->rid))
		return False;

	if(!smb_io_dom_sid2("sid", &q_u->dom_sid, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_open_domain(char *desc, SAMR_R_OPEN_DOMAIN *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_open_domain");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("domain_pol", &r_u->domain_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

void init_samr_q_unknown_2c(SAMR_Q_UNKNOWN_2C *q_u, POLICY_HND *user_pol)
{
	DEBUG(5,("samr_init_q_unknown_2c\n"));

	memcpy(&q_u->user_pol, user_pol, sizeof(q_u->user_pol));
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_unknown_2c(char *desc,  SAMR_Q_UNKNOWN_2C *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_unknown_2c");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("user_pol", &q_u->user_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a structure.
********************************************************************/

void init_samr_r_unknown_2c(SAMR_R_UNKNOWN_2C *q_u, uint32 status)
{
	DEBUG(5,("samr_init_r_unknown_2c\n"));

	q_u->unknown_0 = 0x00160000;
	q_u->unknown_1 = 0x00000000;
	q_u->status    = status;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_unknown_2c(char *desc,  SAMR_R_UNKNOWN_2C *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_2c");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0", ps, depth, &r_u->unknown_0))
		return False;
	if(!prs_uint32("unknown_1", ps, depth, &r_u->unknown_1))
		return False;
	if(!prs_uint32("status   ", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_UNKNOWN_3 structure.
********************************************************************/

void init_samr_q_unknown_3(SAMR_Q_UNKNOWN_3 *q_u,
				POLICY_HND *user_pol, uint16 switch_value)
{
	DEBUG(5,("samr_init_q_unknown_3\n"));

	memcpy(&q_u->user_pol, user_pol, sizeof(q_u->user_pol));
	q_u->switch_value = switch_value;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_unknown_3(char *desc,  SAMR_Q_UNKNOWN_3 *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_unknown_3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("user_pol", &q_u->user_pol, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_value", ps, depth, &q_u->switch_value))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_QUERY_DOMAIN_INFO structure.
********************************************************************/

void init_samr_q_query_dom_info(SAMR_Q_QUERY_DOMAIN_INFO *q_u,
				POLICY_HND *domain_pol, uint16 switch_value)
{
	DEBUG(5,("init_samr_q_query_dom_info\n"));

	memcpy(&q_u->domain_pol, domain_pol, sizeof(q_u->domain_pol));
	q_u->switch_value = switch_value;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_query_dom_info(char *desc,  SAMR_Q_QUERY_DOMAIN_INFO *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_query_dom_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("domain_pol", &q_u->domain_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_value", ps, depth, &q_u->switch_value))
		return False;
	if(!prs_align(ps))
		return False;


	return True;
}

/*******************************************************************
 Inits a structure.
********************************************************************/

void init_unk_info2(SAM_UNK_INFO_2 *u_2, char *domain, char *server)
{
	int len_domain = strlen(domain);
	int len_server = strlen(server);

	u_2->unknown_0 = 0x00000000;
	u_2->unknown_1 = 0x80000000;
	u_2->unknown_2 = 0x00000000;

	u_2->ptr_0 = 1;
	init_uni_hdr(&u_2->hdr_domain, len_domain);
	init_uni_hdr(&u_2->hdr_server, len_server);

	u_2->seq_num = 0x10000000;
	u_2->unknown_3 = 0x00000000;
	
	u_2->unknown_4  = 0x00000001;
	u_2->unknown_5  = 0x00000003;
	u_2->unknown_6  = 0x00000001;
	u_2->num_domain_usrs  = 0x00000008;
	u_2->num_domain_grps = 0x00000003;
	u_2->num_local_grps = 0x00000003;

	memset(u_2->padding, 0, sizeof(u_2->padding)); /* 12 bytes zeros */

	init_unistr2(&u_2->uni_domain, domain, len_domain);
	init_unistr2(&u_2->uni_server, server, len_server);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL sam_io_unk_info2(char *desc, SAM_UNK_INFO_2 *u_2, prs_struct *ps, int depth)
{
	if (u_2 == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_unk_info2");
	depth++;

	if(!prs_uint32("unknown_0", ps, depth, &u_2->unknown_0)) /* 0x0000 0000 */
		return False;
	if(!prs_uint32("unknown_1", ps, depth, &u_2->unknown_1)) /* 0x8000 0000 */
		return False;
	if(!prs_uint32("unknown_2", ps, depth, &u_2->unknown_2)) /* 0x0000 0000 */
		return False;

	if(!prs_uint32("ptr_0", ps, depth, &u_2->ptr_0))     /* pointer to unknown structure */
		return False;
	if(!smb_io_unihdr("hdr_domain", &u_2->hdr_domain, ps, depth)) /* domain name unicode header */
		return False;
	if(!smb_io_unihdr("hdr_server", &u_2->hdr_server, ps, depth)) /* server name unicode header */
		return False;

	/* put all the data in here, at the moment, including what the above
	   pointer is referring to
	 */

	if(!prs_uint32("seq_num ", ps, depth, &u_2->seq_num )) /* 0x0000 0099 or 0x1000 0000 */
		return False;
	if(!prs_uint32("unknown_3 ", ps, depth, &u_2->unknown_3 )) /* 0x0000 0000 */
		return False;
	
	if(!prs_uint32("unknown_4 ", ps, depth, &u_2->unknown_4 )) /* 0x0000 0001 */
		return False;
	if(!prs_uint32("unknown_5 ", ps, depth, &u_2->unknown_5 )) /* 0x0000 0003 */
		return False;
	if(!prs_uint32("unknown_6 ", ps, depth, &u_2->unknown_6 )) /* 0x0000 0001 */
		return False;
	if(!prs_uint32("num_domain_usrs ", ps, depth, &u_2->num_domain_usrs )) /* 0x0000 0008 */
		return False;
	if(!prs_uint32("num_domain_grps", ps, depth, &u_2->num_domain_grps)) /* 0x0000 0003 */
		return False;
	if(!prs_uint32("num_local_grps", ps, depth, &u_2->num_local_grps)) /* 0x0000 0003 */
		return False;

	if(!prs_uint8s(False, "padding", ps, depth, u_2->padding, sizeof(u_2->padding))) /* 12 bytes zeros */
		return False;

	if(!smb_io_unistr2( "uni_domain", &u_2->uni_domain, u_2->hdr_domain.buffer, ps, depth)) /* domain name unicode string */
		return False;
	if(!smb_io_unistr2( "uni_server", &u_2->uni_server, u_2->hdr_server.buffer, ps, depth)) /* server name unicode string */
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_QUERY_DOMAIN_INFO structure.
********************************************************************/

void init_samr_r_query_dom_info(SAMR_R_QUERY_DOMAIN_INFO *r_u, 
				uint16 switch_value, SAM_UNK_CTR *ctr,
				uint32 status)
{
	DEBUG(5,("init_samr_r_query_dom_info\n"));

	r_u->ptr_0 = 0;
	r_u->switch_value = 0;
	r_u->status = status; /* return status */

	if (status == 0) {
		r_u->switch_value = switch_value;
		r_u->ptr_0 = 1;
		r_u->ctr = ctr;
	}
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_query_dom_info(char *desc, SAMR_R_QUERY_DOMAIN_INFO *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_query_dom_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_0       ", ps, depth, &r_u->ptr_0))
		return False;
	if(!prs_uint16("switch_value", ps, depth, &r_u->switch_value))
		return False;
	if(!prs_align(ps))
		return False;

	if (r_u->ptr_0 != 0 && r_u->ctr != NULL) {
		switch (r_u->switch_value) {
		case 0x02:
			if(!sam_io_unk_info2("unk_inf2", &r_u->ctr->info.inf2, ps, depth))
				return False;
			break;
		default:
			DEBUG(3,("samr_io_r_query_dom_info: unknown switch level 0x%x\n",
			          r_u->switch_value));
			return False;
		}
	}

	return True;
}


/*******************************************************************
 Inits a DOM_SID3 structure.
 Calculate length by adding up the size of the components.
 ********************************************************************/

void init_dom_sid3(DOM_SID3 *sid3, uint16 unk_0, uint16 unk_1, DOM_SID *sid)
{
	sid3->sid = *sid;
	sid3->len = 2 + 8 + sid3->sid.num_auths * 4;
}

/*******************************************************************
 Reads or writes a SAM_SID3 structure.

 this one's odd, because the length (in bytes) is specified at the beginning.
 the length _includes_ the length of the length, too :-)

********************************************************************/

static BOOL sam_io_dom_sid3(char *desc,  DOM_SID3 *sid3, prs_struct *ps, int depth)
{
	if (sid3 == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_dom_sid3");
	depth++;

	if(!prs_uint16("len", ps, depth, &sid3->len))
		return False;
	if(!prs_align(ps))
		return False;
	if(!smb_io_dom_sid("", &sid3->sid, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_UNKNOWN3 structure.

unknown_2   : 0x0001
unknown_3   : 0x8004

unknown_4,5 : 0x0000 0014

unknown_6   : 0x0002
unknown_7   : 0x5800 or 0x0070

********************************************************************/

static void init_sam_sid_stuff(SAM_SID_STUFF *stf,
				uint16 unknown_2, uint16 unknown_3,
				uint32 unknown_4, uint16 unknown_6, uint16 unknown_7,
				int num_sid3s, DOM_SID3 sid3[MAX_SAM_SIDS])
{
	stf->unknown_2 = unknown_2;
	stf->unknown_3 = unknown_3;

	memset((char *)stf->padding1, '\0', sizeof(stf->padding1));

	stf->unknown_4 = unknown_4;
	stf->unknown_5 = unknown_4;

	stf->unknown_6 = unknown_6;
	stf->unknown_7 = unknown_7;

	stf->num_sids  = num_sid3s;

	stf->padding2  = 0x0000;

	memcpy(stf->sid, sid3, sizeof(DOM_SID3) * num_sid3s);
}

/*******************************************************************
 Reads or writes a SAM_SID_STUFF structure.
********************************************************************/

static BOOL sam_io_sid_stuff(char *desc,  SAM_SID_STUFF *stf, prs_struct *ps, int depth)
{
	int i;

	if (stf == NULL)
		return False;

	DEBUG(5,("init_sam_sid_stuff\n"));

	if(!prs_uint16("unknown_2", ps, depth, &stf->unknown_2))
		return False;
	if(!prs_uint16("unknown_3", ps, depth, &stf->unknown_3))
		return False;

	if(!prs_uint8s(False, "padding1", ps, depth, stf->padding1, sizeof(stf->padding1)))
		return False;
	
	if(!prs_uint32("unknown_4", ps, depth, &stf->unknown_4))
		return False;
	if(!prs_uint32("unknown_5", ps, depth, &stf->unknown_5))
		return False;
	if(!prs_uint16("unknown_6", ps, depth, &stf->unknown_6))
		return False;
	if(!prs_uint16("unknown_7", ps, depth, &stf->unknown_7))
		return False;

	if(!prs_uint32("num_sids ", ps, depth, &stf->num_sids ))
		return False;
	if(!prs_uint16("padding2 ", ps, depth, &stf->padding2 ))
		return False;

	SMB_ASSERT_ARRAY(stf->sid, stf->num_sids);

	for (i = 0; i < stf->num_sids; i++) {
		if(!sam_io_dom_sid3("", &(stf->sid[i]), ps, depth))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits or writes a SAMR_R_UNKNOWN3 structure.
********************************************************************/

void init_samr_r_unknown_3(SAMR_R_UNKNOWN_3 *r_u,
				uint16 unknown_2, uint16 unknown_3,
				uint32 unknown_4, uint16 unknown_6, uint16 unknown_7,
				int num_sid3s, DOM_SID3 sid3[MAX_SAM_SIDS],
				uint32 status)
{
	DEBUG(5,("samr_init_r_unknown_3\n"));

	r_u->ptr_0 = 0;
	r_u->ptr_1 = 0;

	if (status == 0x0) {
		r_u->ptr_0 = 1;
		r_u->ptr_1 = 1;
		init_sam_sid_stuff(&(r_u->sid_stuff), unknown_2, unknown_3,
	               unknown_4, unknown_6, unknown_7,
	               num_sid3s, sid3);
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a SAMR_R_UNKNOWN_3 structure.

this one's odd, because the daft buggers use a different mechanism
for writing out the array of sids. they put the number of sids in
only one place: they've calculated the length of each sid and jumped
by that amount.  then, retrospectively, the length of the whole buffer
is put at the beginning of the data stream.

wierd.  

********************************************************************/

BOOL samr_io_r_unknown_3(char *desc,  SAMR_R_UNKNOWN_3 *r_u, prs_struct *ps, int depth)
{
	int ptr_len0=0;
	int ptr_len1=0;
	int ptr_sid_stuff = 0;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_0         ", ps, depth, &r_u->ptr_0))
		return False;

	if (ps->io) {
		/* reading.  do the length later */
		if(!prs_uint32("sid_stuff_len0", ps, depth, &r_u->sid_stuff_len0))
			return False;
	} else {
		/* storing */
		ptr_len0 = prs_offset(ps);
		if(!prs_set_offset(ps, ptr_len0 + 4))
			return False;
	}

	if (r_u->ptr_0 != 0) {
		if(!prs_uint32("ptr_1         ", ps, depth, &r_u->ptr_1))
			return False;
		if (ps->io) {
			/* reading.  do the length later */
			if(!prs_uint32("sid_stuff_len1", ps, depth, &r_u->sid_stuff_len1))
				return False;
		} else {
			/* storing */
			ptr_len1 = prs_offset(ps);
			if(!prs_set_offset(ps, ptr_len1 + 4))
				return False;
		}

		if (r_u->ptr_1 != 0) {
			ptr_sid_stuff = prs_offset(ps);
			if(!sam_io_sid_stuff("", &r_u->sid_stuff, ps, depth))
				return False;
		}
	}

	if (!(ps->io)) {
		/* storing not reading.  do the length, now. */

		if (ptr_sid_stuff != 0) {
			int old_len = prs_offset(ps);
			uint32 sid_stuff_len = old_len - ptr_sid_stuff;

			if(!prs_set_offset(ps, ptr_len0))
				return False;
			if(!prs_uint32("sid_stuff_len0", ps, depth, &sid_stuff_len))
				return False;

			if(!prs_set_offset(ps, ptr_len1))
				return False;
			if(!prs_uint32("sid_stuff_len1", ps, depth, &sid_stuff_len))
				return False;

			if(!prs_set_offset(ps, old_len))
				return False;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a SAM_STR1 structure.
********************************************************************/

static BOOL sam_io_sam_str1(char *desc, SAM_STR1 *sam, uint32 acct_buf,
				uint32 name_buf, uint32 desc_buf, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_str1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("unistr2", &sam->uni_acct_name, acct_buf, ps, depth)) /* account name unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &sam->uni_full_name, name_buf, ps, depth)) /* full name unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &sam->uni_acct_desc, desc_buf, ps, depth)) /* account description unicode string */
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_ENTRY1 structure.
********************************************************************/

static void init_sam_entry1(SAM_ENTRY1 *sam, uint32 user_idx, 
				uint32 len_sam_name, uint32 len_sam_full, uint32 len_sam_desc,
				uint32 rid_user, uint16 acb_info)
{
	DEBUG(5,("init_sam_entry1\n"));

	sam->user_idx = user_idx;
	sam->rid_user = rid_user;
	sam->acb_info = acb_info;
	sam->pad      = 0;

	init_uni_hdr(&sam->hdr_acct_name, len_sam_name);
	init_uni_hdr(&sam->hdr_user_name, len_sam_full);
	init_uni_hdr(&sam->hdr_user_desc, len_sam_desc);
}

/*******************************************************************
 Reads or writes a SAM_ENTRY1 structure.
********************************************************************/

static BOOL sam_io_sam_entry1(char *desc,  SAM_ENTRY1 *sam, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_entry1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("user_idx ", ps, depth, &sam->user_idx))
		return False;

	if(!prs_uint32("rid_user ", ps, depth, &sam->rid_user))
		return False;
	if(!prs_uint16("acb_info ", ps, depth, &sam->acb_info))
		return False;
	if(!prs_uint16("pad      ", ps, depth, &sam->pad))
		return False;

	if(!smb_io_unihdr("unihdr", &sam->hdr_acct_name, ps, depth)) /* account name unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &sam->hdr_user_name, ps, depth)) /* account name unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &sam->hdr_user_desc, ps, depth)) /* account name unicode string header */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a SAM_STR2 structure.
********************************************************************/

static BOOL sam_io_sam_str2(char *desc,  SAM_STR2 *sam, uint32 acct_buf, uint32 desc_buf, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_str2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("unistr2", &sam->uni_srv_name, acct_buf, ps, depth)) /* account name unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &sam->uni_srv_desc, desc_buf, ps, depth)) /* account description unicode string */
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_ENTRY2 structure.
********************************************************************/

static void init_sam_entry2(SAM_ENTRY2 *sam, uint32 user_idx, 
				uint32 len_sam_name, uint32 len_sam_desc,
				uint32 rid_user, uint16 acb_info)
{
	DEBUG(5,("init_sam_entry2\n"));

	sam->user_idx = user_idx;
	sam->rid_user = rid_user;
	sam->acb_info = acb_info;
	sam->pad      = 0;

	init_uni_hdr(&sam->hdr_srv_name, len_sam_name);
	init_uni_hdr(&sam->hdr_srv_desc, len_sam_desc);
}

/*******************************************************************
 Reads or writes a SAM_ENTRY2 structure.
********************************************************************/

static BOOL sam_io_sam_entry2(char *desc,  SAM_ENTRY2 *sam, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_entry2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("user_idx ", ps, depth, &sam->user_idx))
		return False;

	if(!prs_uint32("rid_user ", ps, depth, &sam->rid_user))
		return False;
	if(!prs_uint16("acb_info ", ps, depth, &sam->acb_info))
		return False;
	if(!prs_uint16("pad      ", ps, depth, &sam->pad))
		return False;

	if(!smb_io_unihdr("unihdr", &sam->hdr_srv_name, ps, depth)) /* account name unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &sam->hdr_srv_desc, ps, depth)) /* account name unicode string header */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a SAM_STR3 structure.
********************************************************************/

static BOOL sam_io_sam_str3(char *desc,  SAM_STR3 *sam, uint32 acct_buf, uint32 desc_buf, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_str3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("unistr2", &sam->uni_grp_name, acct_buf, ps, depth)) /* account name unicode string */
		return False;
	if(!smb_io_unistr2("unistr2", &sam->uni_grp_desc, desc_buf, ps, depth)) /* account description unicode string */
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_ENTRY3 structure.
********************************************************************/

static void init_sam_entry3(SAM_ENTRY3 *sam, uint32 grp_idx, 
				uint32 len_grp_name, uint32 len_grp_desc, uint32 rid_grp)
{
	DEBUG(5,("init_sam_entry3\n"));

	sam->grp_idx = grp_idx;
	sam->rid_grp = rid_grp;
	sam->attr    = 0x07; /* group rid attributes - gets ignored by nt 4.0 */

	init_uni_hdr(&sam->hdr_grp_name, len_grp_name);
	init_uni_hdr(&sam->hdr_grp_desc, len_grp_desc);
}

/*******************************************************************
 Reads or writes a SAM_ENTRY3 structure.
********************************************************************/

static BOOL sam_io_sam_entry3(char *desc,  SAM_ENTRY3 *sam, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_entry3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("grp_idx", ps, depth, &sam->grp_idx))
		return False;

	if(!prs_uint32("rid_grp", ps, depth, &sam->rid_grp))
		return False;
	if(!prs_uint32("attr   ", ps, depth, &sam->attr))
		return False;

	if(!smb_io_unihdr("unihdr", &sam->hdr_grp_name, ps, depth)) /* account name unicode string header */
		return False;
	if(!smb_io_unihdr("unihdr", &sam->hdr_grp_desc, ps, depth)) /* account name unicode string header */
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_ENTRY structure.
********************************************************************/

static void init_sam_entry(SAM_ENTRY *sam, uint32 len_sam_name, uint32 rid)
{
	DEBUG(5,("init_sam_entry\n"));

	sam->rid = rid;
	init_uni_hdr(&sam->hdr_name, len_sam_name);
}

/*******************************************************************
 Reads or writes a SAM_ENTRY structure.
********************************************************************/

static BOOL sam_io_sam_entry(char *desc,  SAM_ENTRY *sam, prs_struct *ps, int depth)
{
	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_entry");
	depth++;

	if(!prs_align(ps))
		return False;
	if(!prs_uint32("rid", ps, depth, &sam->rid))
		return False;
	if(!smb_io_unihdr("unihdr", &sam->hdr_name, ps, depth)) /* account name unicode string header */
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_Q_ENUM_DOM_USERS structure.
********************************************************************/

void init_samr_q_enum_dom_users(SAMR_Q_ENUM_DOM_USERS *q_e, POLICY_HND *pol,
				uint16 req_num_entries, uint16 unk_0,
				uint16 acb_mask, uint16 unk_1, uint32 size)
{
	DEBUG(5,("init_q_enum_dom_users\n"));

	memcpy(&q_e->pol, pol, sizeof(*pol));

	q_e->req_num_entries = req_num_entries; /* zero indicates lots */
	q_e->unknown_0 = unk_0; /* this gets returned in the response */
	q_e->acb_mask  = acb_mask;
	q_e->unknown_1 = unk_1;
	q_e->max_size = size;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_enum_dom_users(char *desc,  SAMR_Q_ENUM_DOM_USERS *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_enum_dom_users");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_e->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("req_num_entries", ps, depth, &q_e->req_num_entries))
		return False;
	if(!prs_uint16("unknown_0      ", ps, depth, &q_e->unknown_0))
		return False;

	if(!prs_uint16("acb_mask       ", ps, depth, &q_e->acb_mask))
		return False;
	if(!prs_uint16("unknown_1      ", ps, depth, &q_e->unknown_1))
		return False;

	if(!prs_uint32("max_size       ", ps, depth, &q_e->max_size))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_R_ENUM_DOM_USERS structure.
********************************************************************/

void init_samr_r_enum_dom_users(SAMR_R_ENUM_DOM_USERS *r_u,
		uint16 total_num_entries, uint16 unk_0,
		uint32 num_sam_entries, SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES], uint32 status)
{
	int i;

	DEBUG(5,("init_samr_r_enum_dom_users\n"));

	if (num_sam_entries >= MAX_SAM_ENTRIES) {
		num_sam_entries = MAX_SAM_ENTRIES;
		DEBUG(5,("limiting number of entries to %d\n",
			 num_sam_entries));
	}

	r_u->total_num_entries = total_num_entries;
	r_u->unknown_0         = unk_0;

	if (total_num_entries > 0) {
		r_u->ptr_entries1 = 1;
		r_u->ptr_entries2 = 1;
		r_u->num_entries2 = num_sam_entries;
		r_u->num_entries3 = num_sam_entries;

		SMB_ASSERT_ARRAY(r_u->sam, num_sam_entries);
		SMB_ASSERT_ARRAY(r_u->uni_acct_name, num_sam_entries);

		for (i = 0; i < num_sam_entries; i++) {
			init_sam_entry(&(r_u->sam[i]),
			               pass[i].uni_user_name.uni_str_len,
			               pass[i].user_rid);

			copy_unistr2(&r_u->uni_acct_name[i], &(pass[i].uni_user_name));
		}

		r_u->num_entries4 = num_sam_entries;
	} else {
		r_u->ptr_entries1 = 0;
		r_u->num_entries2 = num_sam_entries;
		r_u->ptr_entries2 = 1;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_enum_dom_users(char *desc,  SAMR_R_ENUM_DOM_USERS *r_u, prs_struct *ps, int depth)
{
	int i;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_enum_dom_users");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint16("total_num_entries", ps, depth, &r_u->total_num_entries))
		return False;
	if(!prs_uint16("unknown_0        ", ps, depth, &r_u->unknown_0))
		return False;
	if(!prs_uint32("ptr_entries1", ps, depth, &r_u->ptr_entries1))
		return False;

	if (r_u->total_num_entries != 0 && r_u->ptr_entries1 != 0) {
		if(!prs_uint32("num_entries2", ps, depth, &r_u->num_entries2))
			return False;
		if(!prs_uint32("ptr_entries2", ps, depth, &r_u->ptr_entries2))
			return False;
		if(!prs_uint32("num_entries3", ps, depth, &r_u->num_entries3))
			return False;

		SMB_ASSERT_ARRAY(r_u->sam, r_u->num_entries2);

		for (i = 0; i < r_u->num_entries2; i++) {
			if(!sam_io_sam_entry("", &r_u->sam[i], ps, depth))
				return False;
		}

		SMB_ASSERT_ARRAY(r_u->uni_acct_name, r_u->num_entries2);

		for (i = 0; i < r_u->num_entries2; i++) {
			if(!smb_io_unistr2("", &r_u->uni_acct_name[i],
					r_u->sam[i].hdr_name.buffer, ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;

		if(!prs_uint32("num_entries4", ps, depth, &r_u->num_entries4))
			return False;
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_ENUM_DOM_ALIASES structure.
********************************************************************/

void init_samr_q_enum_dom_aliases(SAMR_Q_ENUM_DOM_ALIASES *q_e, POLICY_HND *pol, uint32 size)
{
	DEBUG(5,("init_q_enum_dom_aliases\n"));

	memcpy(&q_e->pol, pol, sizeof(*pol));

	q_e->unknown_0 = 0;
	q_e->max_size = size;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_enum_dom_aliases(char *desc,  SAMR_Q_ENUM_DOM_ALIASES *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_enum_dom_aliases");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_e->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0", ps, depth, &q_e->unknown_0))
		return False;
	if(!prs_uint32("max_size ", ps, depth, &q_e->max_size ))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_R_ENUM_DOM_ALIASES structure.
********************************************************************/

void init_samr_r_enum_dom_aliases(SAMR_R_ENUM_DOM_ALIASES *r_u,
		uint32 num_sam_entries, SAM_USER_INFO_21 grps[MAX_SAM_ENTRIES],
		uint32 status)
{
	int i;

	DEBUG(5,("init_samr_r_enum_dom_aliases\n"));

	if (num_sam_entries >= MAX_SAM_ENTRIES) {
		num_sam_entries = MAX_SAM_ENTRIES;
		DEBUG(5,("limiting number of entries to %d\n", 
			 num_sam_entries));
	}

	r_u->num_entries  = num_sam_entries;

	if (num_sam_entries > 0) {
		r_u->ptr_entries  = 1;
		r_u->num_entries2 = num_sam_entries;
		r_u->ptr_entries2 = 1;
		r_u->num_entries3 = num_sam_entries;

		SMB_ASSERT_ARRAY(r_u->sam, num_sam_entries);

		for (i = 0; i < num_sam_entries; i++) {
			init_sam_entry(&r_u->sam[i],
			               grps[i].uni_user_name.uni_str_len,
			               grps[i].user_rid);

			copy_unistr2(&r_u->uni_grp_name[i], &(grps[i].uni_user_name));
		}

		r_u->num_entries4 = num_sam_entries;
	} else {
		r_u->ptr_entries = 0;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_enum_dom_aliases(char *desc,  SAMR_R_ENUM_DOM_ALIASES *r_u, prs_struct *ps, int depth)
{
	int i;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_enum_dom_aliases");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries", ps, depth, &r_u->num_entries))
		return False;
	if(!prs_uint32("ptr_entries", ps, depth, &r_u->ptr_entries))
		return False;

	if (r_u->num_entries != 0 && r_u->ptr_entries != 0) {
		if(!prs_uint32("num_entries2", ps, depth, &r_u->num_entries2))
			return False;
		if(!prs_uint32("ptr_entries2", ps, depth, &r_u->ptr_entries2))
			return False;
		if(!prs_uint32("num_entries3", ps, depth, &r_u->num_entries3))
			return False;

		SMB_ASSERT_ARRAY(r_u->sam, r_u->num_entries);

		for (i = 0; i < r_u->num_entries; i++) {
			if(!sam_io_sam_entry("", &r_u->sam[i], ps, depth))
				return False;
		}

		for (i = 0; i < r_u->num_entries; i++) {
			if(!smb_io_unistr2("", &r_u->uni_grp_name[i], r_u->sam[i].hdr_name.buffer, ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;

		if(!prs_uint32("num_entries4", ps, depth, &r_u->num_entries4))
			return False;
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_Q_QUERY_DISPINFO structure.
********************************************************************/

void init_samr_q_query_dispinfo(SAMR_Q_QUERY_DISPINFO *q_e, POLICY_HND *pol,
				uint16 switch_level, uint32 start_idx, uint32 size)
{
	DEBUG(5,("init_q_query_dispinfo\n"));

	memcpy(&q_e->pol, pol, sizeof(*pol));

	q_e->switch_level = switch_level;

	q_e->unknown_0 = 0;
	q_e->start_idx = start_idx;
	q_e->unknown_1 = 0x000007d0;
	q_e->max_size  = size;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_query_dispinfo(char *desc,  SAMR_Q_QUERY_DISPINFO *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_query_dispinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_e->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_level", ps, depth, &q_e->switch_level))
		return False;
	if(!prs_uint16("unknown_0   ", ps, depth, &q_e->unknown_0))
		return False;
	if(!prs_uint32("start_idx   ", ps, depth, &q_e->start_idx))
		return False;
	if(!prs_uint32("unknown_1   ", ps, depth, &q_e->unknown_1))
		return False;
	if(!prs_uint32("max_size    ", ps, depth, &q_e->max_size))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAM_INFO_2 structure.
********************************************************************/

void init_sam_info_2(SAM_INFO_2 *sam, uint32 acb_mask,
		uint32 start_idx, uint32 num_sam_entries,
		SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES])
{
	int i;
	int entries_added;

	DEBUG(5,("init_sam_info_2\n"));

	if (num_sam_entries >= MAX_SAM_ENTRIES) {
		num_sam_entries = MAX_SAM_ENTRIES;
		DEBUG(5,("limiting number of entries to %d\n", 
			 num_sam_entries));
	}

	for (i = start_idx, entries_added = 0; i < num_sam_entries; i++) {
		if (IS_BITS_SET_ALL(pass[i].acb_info, acb_mask)) {
			init_sam_entry2(&sam->sam[entries_added],
			                start_idx + entries_added + 1,
			                pass[i].uni_user_name.uni_str_len,
			                pass[i].uni_acct_desc.uni_str_len,
			                pass[i].user_rid,
			                pass[i].acb_info);

			copy_unistr2(&sam->str[entries_added].uni_srv_name, &pass[i].uni_user_name);
			copy_unistr2(&sam->str[entries_added].uni_srv_desc, &pass[i].uni_acct_desc);

			entries_added++;
		}

		sam->num_entries   = entries_added;
		sam->ptr_entries   = 1;
		sam->num_entries2  = entries_added;
	}
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL sam_io_sam_info_2(char *desc,  SAM_INFO_2 *sam, prs_struct *ps, int depth)
{
	int i;

	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_info_2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries  ", ps, depth, &sam->num_entries))
		return False;
	if(!prs_uint32("ptr_entries  ", ps, depth, &sam->ptr_entries))
		return False;

	if(!prs_uint32("num_entries2 ", ps, depth, &sam->num_entries2))
		return False;

	SMB_ASSERT_ARRAY(sam->sam, sam->num_entries);

	for (i = 0; i < sam->num_entries; i++) {
		if(!sam_io_sam_entry2("", &sam->sam[i], ps, depth))
			return False;
	}

	for (i = 0; i < sam->num_entries; i++) {
		if(!sam_io_sam_str2 ("", &sam->str[i],
				 sam->sam[i].hdr_srv_name.buffer,
				 sam->sam[i].hdr_srv_desc.buffer,
				 ps, depth))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a SAM_INFO_1 structure.
********************************************************************/

void init_sam_info_1(SAM_INFO_1 *sam, uint32 acb_mask,
		uint32 start_idx, uint32 num_sam_entries,
		SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES])
{
	int i;
	int entries_added;

	DEBUG(5,("init_sam_info_1\n"));

	if (num_sam_entries >= MAX_SAM_ENTRIES) {
		num_sam_entries = MAX_SAM_ENTRIES;
		DEBUG(5,("limiting number of entries to %d\n", 
			 num_sam_entries));
	}

	for (i = start_idx, entries_added = 0; i < num_sam_entries; i++) {
		if (IS_BITS_SET_ALL(pass[i].acb_info, acb_mask)) {
			init_sam_entry1(&sam->sam[entries_added],
						start_idx + entries_added + 1,
						pass[i].uni_user_name.uni_str_len,
						pass[i].uni_full_name.uni_str_len, 
						pass[i].uni_acct_desc.uni_str_len,
						pass[i].user_rid,
						pass[i].acb_info);

			copy_unistr2(&sam->str[entries_added].uni_acct_name, &pass[i].uni_user_name);
			copy_unistr2(&sam->str[entries_added].uni_full_name, &pass[i].uni_full_name);
			copy_unistr2(&sam->str[entries_added].uni_acct_desc, &pass[i].uni_acct_desc);

			entries_added++;
		}
	}

	sam->num_entries   = entries_added;
	sam->ptr_entries   = 1;
	sam->num_entries2  = entries_added;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL sam_io_sam_info_1(char *desc,  SAM_INFO_1 *sam, prs_struct *ps, int depth)
{
	int i;

	if (sam == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_sam_info_1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries  ", ps, depth, &sam->num_entries))
		return False;
	if(!prs_uint32("ptr_entries  ", ps, depth, &sam->ptr_entries))
		return False;

	if(!prs_uint32("num_entries2 ", ps, depth, &sam->num_entries2))
		return False;

	SMB_ASSERT_ARRAY(sam->sam, sam->num_entries);

	for (i = 0; i < sam->num_entries; i++) {
		if(!sam_io_sam_entry1("", &sam->sam[i], ps, depth))
			return False;
	}

	for (i = 0; i < sam->num_entries; i++) {
		if(!sam_io_sam_str1 ("", &sam->str[i],
							 sam->sam[i].hdr_acct_name.buffer,
							 sam->sam[i].hdr_user_name.buffer,
							 sam->sam[i].hdr_user_desc.buffer,
							 ps, depth))
		return False;
	}

	return True;
}

/*******************************************************************
 Inits a SAMR_R_QUERY_DISPINFO structure.
********************************************************************/

void init_samr_r_query_dispinfo(SAMR_R_QUERY_DISPINFO *r_u,
		uint16 switch_level, SAM_INFO_CTR *ctr, uint32 status)
{
	DEBUG(5,("init_samr_r_query_dispinfo\n"));

	if (status == 0x0) {
		r_u->unknown_0 = 0x0000001;
		r_u->unknown_1 = 0x0000001;
	} else {
		r_u->unknown_0 = 0x0;
		r_u->unknown_1 = 0x0;
	}

	r_u->switch_level = switch_level;
	r_u->ctr = ctr;
	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_query_dispinfo(char *desc,  SAMR_R_QUERY_DISPINFO *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_query_dispinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0    ", ps, depth, &r_u->unknown_0))
		return False;
	if(!prs_uint32("unknown_1    ", ps, depth, &r_u->unknown_1))
		return False;
	if(!prs_uint16("switch_level ", ps, depth, &r_u->switch_level))
		return False;

	if(!prs_align(ps))
		return False;

	switch (r_u->switch_level) {
	case 0x1:
		if(!sam_io_sam_info_1("users", r_u->ctr->sam.info1, ps, depth))
			return False;
		break;
	case 0x2:
		if(!sam_io_sam_info_2("servers", r_u->ctr->sam.info2, ps, depth))
			return False;
		break;
	default:
		DEBUG(5,("samr_io_r_query_dispinfo: unknown switch value\n"));
		break;
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_ENUM_DOM_GROUPS structure.
********************************************************************/

void init_samr_q_enum_dom_groups(SAMR_Q_ENUM_DOM_GROUPS *q_e, POLICY_HND *pol,
				uint16 switch_level, uint32 start_idx, uint32 size)
{
	DEBUG(5,("init_q_enum_dom_groups\n"));

	memcpy(&q_e->pol, pol, sizeof(*pol));

	q_e->switch_level = switch_level;

	q_e->unknown_0 = 0;
	q_e->start_idx = start_idx;
	q_e->unknown_1 = 0x000007d0;
	q_e->max_size  = size;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_enum_dom_groups(char *desc,  SAMR_Q_ENUM_DOM_GROUPS *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_enum_dom_groups");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_e->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_level", ps, depth, &q_e->switch_level))
		return False;
	if(!prs_uint16("unknown_0   ", ps, depth, &q_e->unknown_0))
		return False;
	if(!prs_uint32("start_idx   ", ps, depth, &q_e->start_idx))
		return False;
	if(!prs_uint32("unknown_1   ", ps, depth, &q_e->unknown_1))
		return False;
	if(!prs_uint32("max_size    ", ps, depth, &q_e->max_size))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_R_ENUM_DOM_GROUPS structure.
********************************************************************/

void init_samr_r_enum_dom_groups(SAMR_R_ENUM_DOM_GROUPS *r_u,
		uint32 start_idx, uint32 num_sam_entries,
		SAM_USER_INFO_21 pass[MAX_SAM_ENTRIES],
		uint32 status)
{
	int i;
	int entries_added;

	DEBUG(5,("init_samr_r_enum_dom_groups\n"));

	if (num_sam_entries >= MAX_SAM_ENTRIES) {
		num_sam_entries = MAX_SAM_ENTRIES;
		DEBUG(5,("limiting number of entries to %d\n", 
			 num_sam_entries));
	}

	if (status == 0x0) {
		for (i = start_idx, entries_added = 0; i < num_sam_entries; i++) {
			init_sam_entry3(&r_u->sam[entries_added],
			                start_idx + entries_added + 1,
			                pass[i].uni_user_name.uni_str_len,
			                pass[i].uni_acct_desc.uni_str_len,
			                pass[i].user_rid);

			copy_unistr2(&r_u->str[entries_added].uni_grp_name, 
						&pass[i].uni_user_name);
			copy_unistr2(&r_u->str[entries_added].uni_grp_desc, 
						&pass[i].uni_acct_desc);

			entries_added++;
		}

		if (entries_added > 0) {
			r_u->unknown_0 = 0x0000492;
			r_u->unknown_1 = 0x000049a;
		} else {
			r_u->unknown_0 = 0x0;
			r_u->unknown_1 = 0x0;
		}
		r_u->switch_level  = 3;
		r_u->num_entries   = entries_added;
		r_u->ptr_entries   = 1;
		r_u->num_entries2  = entries_added;
	} else {
		r_u->switch_level = 0;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_enum_dom_groups(char *desc,  SAMR_R_ENUM_DOM_GROUPS *r_u, prs_struct *ps, int depth)
{
	int i;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_enum_dom_groups");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0    ", ps, depth, &r_u->unknown_0))
		return False;
	if(!prs_uint32("unknown_1    ", ps, depth, &r_u->unknown_1))
		return False;
	if(!prs_uint32("switch_level ", ps, depth, &r_u->switch_level))
		return False;

	if (r_u->switch_level != 0) {
		if(!prs_uint32("num_entries  ", ps, depth, &r_u->num_entries))
			return False;
		if(!prs_uint32("ptr_entries  ", ps, depth, &r_u->ptr_entries))
			return False;

		if(!prs_uint32("num_entries2 ", ps, depth, &r_u->num_entries2))
			return False;

		SMB_ASSERT_ARRAY(r_u->sam, r_u->num_entries);

		for (i = 0; i < r_u->num_entries; i++) {
			if(!sam_io_sam_entry3("", &r_u->sam[i], ps, depth))
				return False;
		}

		for (i = 0; i < r_u->num_entries; i++) {
			if(!sam_io_sam_str3 ("", &r_u->str[i],
			                     r_u->sam[i].hdr_grp_name.buffer,
			                     r_u->sam[i].hdr_grp_desc.buffer,
			                     ps, depth))
				return False;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_QUERY_ALIASINFO structure.
********************************************************************/

void init_samr_q_query_aliasinfo(SAMR_Q_QUERY_ALIASINFO *q_e,
				POLICY_HND *pol,
				uint16 switch_level)
{
	DEBUG(5,("init_q_query_aliasinfo\n"));

	memcpy(&q_e->pol, pol, sizeof(*pol));

	q_e->switch_level = switch_level;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_query_aliasinfo(char *desc,  SAMR_Q_QUERY_ALIASINFO *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_query_aliasinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_e->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_level", ps, depth, &q_e->switch_level))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_QUERY_ALIASINFO structure.
********************************************************************/

void init_samr_r_query_aliasinfo(SAMR_R_QUERY_ALIASINFO *r_u,
		uint16 switch_value, char *acct_desc,
		uint32 status)
{
	DEBUG(5,("init_samr_r_query_aliasinfo\n"));

	r_u->ptr = 0;

	if (status == 0) {
		r_u->switch_value = switch_value;

		switch (switch_value) {
			case 3:
			{
				int acct_len = acct_desc ? strlen(acct_desc) : 0;

				r_u->ptr = 1;

				init_uni_hdr(&r_u->alias.info3.hdr_acct_desc, acct_len);
				init_unistr2(&r_u->alias.info3.uni_acct_desc, acct_desc, acct_len);

				break;
			}
			default:
				DEBUG(4,("init_samr_r_query_aliasinfo: unsupported switch level\n"));
				break;
		}
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_query_aliasinfo(char *desc,  SAMR_R_QUERY_ALIASINFO *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_query_aliasinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr         ", ps, depth, &r_u->ptr))
		return False;
	
	if (r_u->ptr != 0) {
		if(!prs_uint16("switch_value", ps, depth, &r_u->switch_value))
			return False;
		if(!prs_align(ps))
			return False;

		if (r_u->switch_value != 0) {
			switch (r_u->switch_value) {
				case 3:
					if(!smb_io_unihdr ("", &r_u->alias.info3.hdr_acct_desc, ps, depth))
						return False;
					if(!smb_io_unistr2("", &r_u->alias.info3.uni_acct_desc, 
							r_u->alias.info3.hdr_acct_desc.buffer, ps, depth))
						return False;
					break;
				default:
					DEBUG(4,("samr_io_r_query_aliasinfo: unsupported switch level\n"));
					break;
			}
		}
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a SAMR_Q_LOOKUP_IDS structure.
********************************************************************/

BOOL samr_io_q_lookup_ids(char *desc,  SAMR_Q_LOOKUP_IDS *q_u, prs_struct *ps, int depth)
{
	fstring tmp;
	int i;

	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_lookup_ids");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &(q_u->pol), ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_sids1", ps, depth, &q_u->num_sids1))
		return False;
	if(!prs_uint32("ptr      ", ps, depth, &q_u->ptr))
		return False;
	if(!prs_uint32("num_sids2", ps, depth, &q_u->num_sids2))
		return False;

	SMB_ASSERT_ARRAY(q_u->ptr_sid, q_u->num_sids2);

	for (i = 0; i < q_u->num_sids2; i++) {
		slprintf(tmp, sizeof(tmp) - 1, "ptr[%02d]", i);
		if(!prs_uint32(tmp, ps, depth, &q_u->ptr_sid[i]))
			return False;
	}

	for (i = 0; i < q_u->num_sids2; i++) {
		if (q_u->ptr_sid[i] != 0) {
			slprintf(tmp, sizeof(tmp)-1, "sid[%02d]", i);
			if(!smb_io_dom_sid2(tmp, &q_u->sid[i], ps, depth))
				return False;
		}
	}

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_LOOKUP_IDS structure.
********************************************************************/

void init_samr_r_lookup_ids(SAMR_R_LOOKUP_IDS *r_u,
		uint32 num_rids, uint32 *rid, uint32 status)
{
	int i;

	DEBUG(5,("init_samr_r_lookup_ids\n"));

	if (status == 0x0) {
		r_u->num_entries  = num_rids;
		r_u->ptr = 1;
		r_u->num_entries2 = num_rids;

		SMB_ASSERT_ARRAY(r_u->rid, num_rids);

		for (i = 0; i < num_rids; i++) {
			r_u->rid[i] = rid[i];
		}
	} else {
		r_u->num_entries  = 0;
		r_u->ptr = 0;
		r_u->num_entries2 = 0;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_lookup_ids(char *desc,  SAMR_R_LOOKUP_IDS *r_u, prs_struct *ps, int depth)
{
	fstring tmp;
	int i;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_lookup_ids");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries", ps, depth, &r_u->num_entries))
		return False;
	if(!prs_uint32("ptr        ", ps, depth, &r_u->ptr))
		return False;
	if(!prs_uint32("num_entries2", ps, depth, &r_u->num_entries2))
		return False;

	if (r_u->num_entries != 0) {
		SMB_ASSERT_ARRAY(r_u->rid, r_u->num_entries2);

		for (i = 0; i < r_u->num_entries2; i++) {
			slprintf(tmp, sizeof(tmp)-1, "rid[%02d]", i);
			if(!prs_uint32(tmp, ps, depth, &r_u->rid[i]))
				return False;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_lookup_names(char *desc,  SAMR_Q_LOOKUP_NAMES *q_u, prs_struct *ps, int depth)
{
	int i;

	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_lookup_names");
	depth++;

	prs_align(ps);

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_names1", ps, depth, &q_u->num_names1))
		return False;
	if(!prs_uint32("flags     ", ps, depth, &q_u->flags))
		return False;
	if(!prs_uint32("ptr      ", ps, depth, &q_u->ptr))
		return False;
	if(!prs_uint32("num_names2", ps, depth, &q_u->num_names2))
		return False;

	SMB_ASSERT_ARRAY(q_u->hdr_name, q_u->num_names2);

	for (i = 0; i < q_u->num_names2; i++) {
		if(!smb_io_unihdr ("", &q_u->hdr_name[i], ps, depth))
			return False;
	}
	for (i = 0; i < q_u->num_names2; i++) {
		if(!smb_io_unistr2("", &q_u->uni_name[i], q_u->hdr_name[i].buffer, ps, depth))
			return False;
	}

	return True;
}


/*******************************************************************
 Inits a SAMR_R_LOOKUP_NAMES structure.
********************************************************************/

void init_samr_r_lookup_names(SAMR_R_LOOKUP_NAMES *r_u,
			uint32 num_rids, uint32 *rid, uint8 *type, uint32 status)
{
	int i;

	DEBUG(5,("init_samr_r_lookup_names\n"));

	if (status == 0x0) {
		r_u->num_types1 = num_rids;
		r_u->ptr_types  = 1;
		r_u->num_types2 = num_rids;

		r_u->num_rids1 = num_rids;
		r_u->ptr_rids  = 1;
		r_u->num_rids2 = num_rids;

		SMB_ASSERT_ARRAY(r_u->rid, num_rids);

		for (i = 0; i < num_rids; i++) {
			r_u->rid [i] = rid [i];
			r_u->type[i] = type[i];
		}
	} else {
		r_u->num_types1 = 0;
		r_u->ptr_types  = 0;
		r_u->num_types2 = 0;

		r_u->num_rids1 = 0;
		r_u->ptr_rids  = 0;
		r_u->num_rids2 = 0;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_lookup_names(char *desc,  SAMR_R_LOOKUP_NAMES *r_u, prs_struct *ps, int depth)
{
	int i;
	fstring tmp;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_lookup_names");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_rids1", ps, depth, &r_u->num_rids1))
		return False;
	if(!prs_uint32("ptr_rids ", ps, depth, &r_u->ptr_rids ))
		return False;

	if (r_u->ptr_rids != 0) {
		if(!prs_uint32("num_rids2", ps, depth, &r_u->num_rids2))
			return False;

		if (r_u->num_rids2 != r_u->num_rids1) {
			/* RPC fault */
			return False;
		}

		for (i = 0; i < r_u->num_rids2; i++) {
			slprintf(tmp, sizeof(tmp) - 1, "rid[%02d]  ", i);
			if(!prs_uint32(tmp, ps, depth, &r_u->rid[i]))
				return False;
		}
	}

	if(!prs_uint32("num_types1", ps, depth, &r_u->num_types1))
		return False;
	if(!prs_uint32("ptr_types ", ps, depth, &r_u->ptr_types))
		return False;

	if (r_u->ptr_types != 0) {
		if(!prs_uint32("num_types2", ps, depth, &r_u->num_types2))
			return False;

		if (r_u->num_types2 != r_u->num_types1) {
			/* RPC fault */
			return False;
		}

		for (i = 0; i < r_u->num_types2; i++) {
			slprintf(tmp, sizeof(tmp) - 1, "type[%02d]  ", i);
			if(!prs_uint32(tmp, ps, depth, &r_u->type[i]))
				return False;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_unknown_12(char *desc,  SAMR_Q_UNKNOWN_12 *q_u, prs_struct *ps, int depth)
{
	int i;
	fstring tmp;

	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_unknown_12");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_gids1", ps, depth, &q_u->num_gids1))
		return False;
	if(!prs_uint32("rid      ", ps, depth, &q_u->rid))
		return False;
	if(!prs_uint32("ptr      ", ps, depth, &q_u->ptr))
		return False;
	if(!prs_uint32("num_gids2", ps, depth, &q_u->num_gids2))
		return False;

	SMB_ASSERT_ARRAY(q_u->gid, q_u->num_gids2);

	for (i = 0; i < q_u->num_gids2; i++) {
		slprintf(tmp, sizeof(tmp) - 1, "gid[%02d]  ", i);
		if(!prs_uint32(tmp, ps, depth, &q_u->gid[i]))
			return False;
	}

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_UNKNOWN_12 structure.
********************************************************************/

void init_samr_r_unknown_12(SAMR_R_UNKNOWN_12 *r_u,
		uint32 num_aliases, fstring *als_name, uint32 *num_als_usrs,
		uint32 status)
{
	int i;

	DEBUG(5,("init_samr_r_unknown_12\n"));

	if (status == 0x0) {
		r_u->num_aliases1 = num_aliases;
		r_u->ptr_aliases  = 1;
		r_u->num_aliases2 = num_aliases;

		r_u->num_als_usrs1 = num_aliases;
		r_u->ptr_als_usrs  = 1;
		r_u->num_als_usrs2 = num_aliases;

		SMB_ASSERT_ARRAY(r_u->hdr_als_name, num_aliases);

		for (i = 0; i < num_aliases; i++) {
			int als_len = als_name[i] != NULL ? strlen(als_name[i]) : 0;
			init_uni_hdr(&r_u->hdr_als_name[i], als_len);
			init_unistr2(&r_u->uni_als_name[i], als_name[i], als_len);
			r_u->num_als_usrs[i] = num_als_usrs[i];
		}
	} else {
		r_u->num_aliases1 = num_aliases;
		r_u->ptr_aliases  = 0;
		r_u->num_aliases2 = num_aliases;

		r_u->num_als_usrs1 = num_aliases;
		r_u->ptr_als_usrs  = 0;
		r_u->num_als_usrs2 = num_aliases;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_unknown_12(char *desc,  SAMR_R_UNKNOWN_12 *r_u, prs_struct *ps, int depth)
{
	int i;
	fstring tmp;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_12");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_aliases1", ps, depth, &r_u->num_aliases1))
		return False;
	if(!prs_uint32("ptr_aliases ", ps, depth, &r_u->ptr_aliases ))
		return False;
	if(!prs_uint32("num_aliases2", ps, depth, &r_u->num_aliases2))
		return False;

	if (r_u->ptr_aliases != 0 && r_u->num_aliases1 != 0) {
		SMB_ASSERT_ARRAY(r_u->hdr_als_name, r_u->num_aliases2);

		for (i = 0; i < r_u->num_aliases2; i++) {
			slprintf(tmp, sizeof(tmp) - 1, "als_hdr[%02d]  ", i);
			if(!smb_io_unihdr ("", &r_u->hdr_als_name[i], ps, depth))
				return False;
		}
		for (i = 0; i < r_u->num_aliases2; i++) {
			slprintf(tmp, sizeof(tmp) - 1, "als_str[%02d]  ", i);
			if(!smb_io_unistr2("", &r_u->uni_als_name[i], r_u->hdr_als_name[i].buffer, ps, depth))
				return False;
		}
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_als_usrs1", ps, depth, &r_u->num_als_usrs1))
		return False;
	if(!prs_uint32("ptr_als_usrs ", ps, depth, &r_u->ptr_als_usrs))
		return False;
	if(!prs_uint32("num_als_usrs2", ps, depth, &r_u->num_als_usrs2))
		return False;

	if (r_u->ptr_als_usrs != 0 && r_u->num_als_usrs1 != 0) {
		SMB_ASSERT_ARRAY(r_u->num_als_usrs, r_u->num_als_usrs2);

		for (i = 0; i < r_u->num_als_usrs2; i++) {
			slprintf(tmp, sizeof(tmp) - 1, "als_usrs[%02d]  ", i);
			if(!prs_uint32(tmp, ps, depth, &r_u->num_als_usrs[i]))
				return False;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}


/*******************************************************************
 Inits a SAMR_Q_OPEN_USER struct.
********************************************************************/

void init_samr_q_open_user(SAMR_Q_OPEN_USER *q_u,
				POLICY_HND *pol,
				uint32 unk_0, uint32 rid)
{
	DEBUG(5,("samr_init_q_open_user\n"));

	memcpy(&q_u->domain_pol, pol, sizeof(q_u->domain_pol));
	
	q_u->unknown_0 = unk_0;
	q_u->user_rid  = rid;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_open_user(char *desc,  SAMR_Q_OPEN_USER *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_open_user");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("domain_pol", &q_u->domain_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0", ps, depth, &q_u->unknown_0))
		return False;
	if(!prs_uint32("user_rid ", ps, depth, &q_u->user_rid))
		return False;

	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_open_user(char *desc,  SAMR_R_OPEN_USER *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_open_user");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("user_pol", &r_u->user_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_QUERY_USERGROUPS structure.
********************************************************************/

void init_samr_q_query_usergroups(SAMR_Q_QUERY_USERGROUPS *q_u,
				POLICY_HND *hnd)
{
	DEBUG(5,("init_samr_q_query_usergroups\n"));

	memcpy(&q_u->pol, hnd, sizeof(q_u->pol));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_query_usergroups(char *desc,  SAMR_Q_QUERY_USERGROUPS *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_query_usergroups");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_QUERY_USERGROUPS structure.
********************************************************************/

void init_samr_r_query_usergroups(SAMR_R_QUERY_USERGROUPS *r_u,
		uint32 num_gids, DOM_GID *gid, uint32 status)
{
	DEBUG(5,("init_samr_r_query_usergroups\n"));

	if (status == 0x0) {
		r_u->ptr_0        = 1;
		r_u->num_entries  = num_gids;
		r_u->ptr_1        = 1;
		r_u->num_entries2 = num_gids;

		r_u->gid = gid;
	} else {
		r_u->ptr_0       = 0;
		r_u->num_entries = 0;
		r_u->ptr_1       = 0;
	}

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_query_usergroups(char *desc,  SAMR_R_QUERY_USERGROUPS *r_u, prs_struct *ps, int depth)
{
	int i;

	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_query_usergroups");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_0       ", ps, depth, &r_u->ptr_0))
		return False;

	if (r_u->ptr_0 != 0) {
		if(!prs_uint32("num_entries ", ps, depth, &r_u->num_entries))
			return False;
		if(!prs_uint32("ptr_1       ", ps, depth, &r_u->ptr_1))
			return False;

		if (r_u->num_entries != 0) {
			if(!prs_uint32("num_entries2", ps, depth, &r_u->num_entries2))
				return False;

			for (i = 0; i < r_u->num_entries2; i++) {
				if(!smb_io_gid("", &r_u->gid[i], ps, depth))
					return False;
			}
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_QUERY_USERINFO structure.
********************************************************************/

void init_samr_q_query_userinfo(SAMR_Q_QUERY_USERINFO *q_u,
				POLICY_HND *hnd, uint16 switch_value)
{
	DEBUG(5,("init_samr_q_query_userinfo\n"));

	memcpy(&q_u->pol, hnd, sizeof(q_u->pol));
	q_u->switch_value = switch_value;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_query_userinfo(char *desc,  SAMR_Q_QUERY_USERINFO *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_query_userinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16("switch_value", ps, depth, &q_u->switch_value)) /* 0x0015 or 0x0011 */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a LOGON_HRS structure.
********************************************************************/

static BOOL sam_io_logon_hrs(char *desc,  LOGON_HRS *hrs, prs_struct *ps, int depth)
{
	if (hrs == NULL)
		return False;

	prs_debug(ps, depth, desc, "sam_io_logon_hrs");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32 (       "len  ", ps, depth, &hrs->len))
		return False;

	if (hrs->len > 64) {
		DEBUG(5,("sam_io_logon_hrs: truncating length\n"));
		hrs->len = 64;
	}

	if(!prs_uint8s (False, "hours", ps, depth, hrs->hours, hrs->len))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_USER_INFO_10 structure.
********************************************************************/

void init_sam_user_info10(SAM_USER_INFO_10 *usr,
				uint32 acb_info)
{
	DEBUG(5,("init_sam_user_info10\n"));

	usr->acb_info = acb_info;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL sam_io_user_info10(char *desc,  SAM_USER_INFO_10 *usr, prs_struct *ps, int depth)
{
	if (usr == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_user_info10");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("acb_info", ps, depth, &usr->acb_info))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAM_USER_INFO_11 structure.
********************************************************************/

void init_sam_user_info11(SAM_USER_INFO_11 *usr,
				NTTIME *expiry,
				char *mach_acct,
				uint32 rid_user,
				uint32 rid_group,
				uint16 acct_ctrl)
				
{
	int len_mach_acct;

	DEBUG(5,("init_sam_user_info11\n"));

	len_mach_acct = strlen(mach_acct);

	memcpy(&usr->expiry,expiry, sizeof(usr->expiry)); /* expiry time or something? */
	memset((char *)usr->padding_1, '\0', sizeof(usr->padding_1)); /* 0 - padding 24 bytes */

	init_uni_hdr(&usr->hdr_mach_acct, len_mach_acct);  /* unicode header for machine account */
	usr->padding_2 = 0;               /* 0 - padding 4 bytes */

	usr->ptr_1        = 1;            /* pointer */
	memset((char *)usr->padding_3, '\0', sizeof(usr->padding_3)); /* 0 - padding 32 bytes */
	usr->padding_4    = 0;            /* 0 - padding 4 bytes */

	usr->ptr_2        = 1;            /* pointer */
	usr->padding_5    = 0;            /* 0 - padding 4 bytes */

	usr->ptr_3        = 1;          /* pointer */
	memset((char *)usr->padding_6, '\0', sizeof(usr->padding_6)); /* 0 - padding 32 bytes */

	usr->rid_user     = rid_user; 
	usr->rid_group    = rid_group;

	usr->acct_ctrl    = acct_ctrl;
	usr->unknown_3    = 0x0000;

	usr->unknown_4    = 0x003f;       /* 0x003f      - 16 bit unknown */
	usr->unknown_5    = 0x003c;       /* 0x003c      - 16 bit unknown */

	memset((char *)usr->padding_7, '\0', sizeof(usr->padding_7)); /* 0 - padding 16 bytes */
	usr->padding_8    = 0;            /* 0 - padding 4 bytes */
	
	init_unistr2(&usr->uni_mach_acct, mach_acct, len_mach_acct);  /* unicode string for machine account */

	memset((char *)usr->padding_9, '\0', sizeof(usr->padding_9)); /* 0 - padding 48 bytes */
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL sam_io_user_info11(char *desc,  SAM_USER_INFO_11 *usr, prs_struct *ps, int depth)
{
	if (usr == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_24");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint8s (False, "padding_0", ps, depth, usr->padding_0, sizeof(usr->padding_0))) 
		return False;

	if(!smb_io_time("time", &(usr->expiry), ps, depth))
		return False;

	if(!prs_uint8s (False, "padding_1", ps, depth, usr->padding_1, sizeof(usr->padding_1)))
		return False;

	if(!smb_io_unihdr ("unihdr", &usr->hdr_mach_acct, ps, depth))
		return False;
	if(!prs_uint32(        "padding_2", ps, depth, &usr->padding_2))
		return False;

	if(!prs_uint32(        "ptr_1    ", ps, depth, &usr->ptr_1))
		return False;
	if(!prs_uint8s (False, "padding_3", ps, depth, usr->padding_3, sizeof(usr->padding_3)))
		return False;
	if(!prs_uint32(        "padding_4", ps, depth, &usr->padding_4))
		return False;

	if(!prs_uint32(        "ptr_2    ", ps, depth, &usr->ptr_2))
		return False;
	if(!prs_uint32(        "padding_5", ps, depth, &usr->padding_5))
		return False;

	if(!prs_uint32(        "ptr_3    ", ps, depth, &usr->ptr_3))
		return False;
	if(!prs_uint8s(False, "padding_6", ps, depth, usr->padding_6, sizeof(usr->padding_6)))
		return False;

	if(!prs_uint32(        "rid_user ", ps, depth, &usr->rid_user))
		return False;
	if(!prs_uint32(        "rid_group", ps, depth, &usr->rid_group))
		return False;
	if(!prs_uint16(        "acct_ctrl", ps, depth, &usr->acct_ctrl))
		return False;
	if(!prs_uint16(        "unknown_3", ps, depth, &usr->unknown_3))
		return False;
	if(!prs_uint16(        "unknown_4", ps, depth, &usr->unknown_4))
		return False;
	if(!prs_uint16(        "unknown_5", ps, depth, &usr->unknown_5))
		return False;

	if(!prs_uint8s (False, "padding_7", ps, depth, usr->padding_7, sizeof(usr->padding_7)))
		return False;
	if(!prs_uint32(        "padding_8", ps, depth, &usr->padding_8))
		return False;
	
	if(!smb_io_unistr2("unistr2", &usr->uni_mach_acct, True, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint8s(False, "padding_9", ps, depth, usr->padding_9, sizeof(usr->padding_9)))
		return False;

	return True;
}

/*************************************************************************
 init_sam_user_info21

 unknown_3 = 0x00ff ffff
 unknown_5 = 0x0002 0000
 unknown_6 = 0x0000 04ec 

 *************************************************************************/

void init_sam_user_info21(SAM_USER_INFO_21 *usr,
	NTTIME *logon_time,
	NTTIME *logoff_time,
	NTTIME *kickoff_time,
	NTTIME *pass_last_set_time,
	NTTIME *pass_can_change_time,
	NTTIME *pass_must_change_time,

	char *user_name,
	char *full_name,
	char *home_dir,
	char *dir_drive,
	char *logon_script,
	char *profile_path,
	char *description,
	char *workstations,
	char *unknown_str,
	char *munged_dial,

	uint32 user_rid,
	uint32 group_rid,
	uint16 acb_info, 

	uint32 unknown_3,
	uint16 logon_divs,
	LOGON_HRS *hrs,
	uint32 unknown_5,
	uint32 unknown_6)
{
	int len_user_name    = user_name    != NULL ? strlen(user_name   ) : 0;
	int len_full_name    = full_name    != NULL ? strlen(full_name   ) : 0;
	int len_home_dir     = home_dir     != NULL ? strlen(home_dir    ) : 0;
	int len_dir_drive    = dir_drive    != NULL ? strlen(dir_drive   ) : 0;
	int len_logon_script = logon_script != NULL ? strlen(logon_script) : 0;
	int len_profile_path = profile_path != NULL ? strlen(profile_path) : 0;
	int len_description  = description  != NULL ? strlen(description ) : 0;
	int len_workstations = workstations != NULL ? strlen(workstations) : 0;
	int len_unknown_str  = unknown_str  != NULL ? strlen(unknown_str ) : 0;
	int len_munged_dial  = munged_dial  != NULL ? strlen(munged_dial ) : 0;

	usr->logon_time            = *logon_time;
	usr->logoff_time           = *logoff_time;
	usr->kickoff_time          = *kickoff_time;
	usr->pass_last_set_time    = *pass_last_set_time;
	usr->pass_can_change_time  = *pass_can_change_time;
	usr->pass_must_change_time = *pass_must_change_time;

	init_uni_hdr(&usr->hdr_user_name, len_user_name);
	init_uni_hdr(&usr->hdr_full_name, len_full_name);
	init_uni_hdr(&usr->hdr_home_dir, len_home_dir);
	init_uni_hdr(&usr->hdr_dir_drive, len_dir_drive);
	init_uni_hdr(&usr->hdr_logon_script, len_logon_script);
	init_uni_hdr(&usr->hdr_profile_path, len_profile_path);
	init_uni_hdr(&usr->hdr_acct_desc, len_description);
	init_uni_hdr(&usr->hdr_workstations, len_workstations);
	init_uni_hdr(&usr->hdr_unknown_str, len_unknown_str);
	init_uni_hdr(&usr->hdr_munged_dial, len_munged_dial);

	memset((char *)usr->nt_pwd, '\0', sizeof(usr->nt_pwd));
	memset((char *)usr->lm_pwd, '\0', sizeof(usr->lm_pwd));

	usr->user_rid  = user_rid;
	usr->group_rid = group_rid;
	usr->acb_info = acb_info;
	usr->unknown_3 = unknown_3; /* 0x00ff ffff */

	usr->logon_divs = logon_divs; /* should be 168 (hours/week) */
	usr->ptr_logon_hrs = hrs ? 1 : 0;
	usr->unknown_5 = unknown_5; /* 0x0002 0000 */

	memset((char *)usr->padding1, '\0', sizeof(usr->padding1));

	init_unistr2(&usr->uni_user_name, user_name, len_user_name);
	init_unistr2(&usr->uni_full_name, full_name, len_full_name);
	init_unistr2(&usr->uni_home_dir, home_dir, len_home_dir);
	init_unistr2(&usr->uni_dir_drive, dir_drive, len_dir_drive);
	init_unistr2(&usr->uni_logon_script, logon_script, len_logon_script);
	init_unistr2(&usr->uni_profile_path, profile_path, len_profile_path);
	init_unistr2(&usr->uni_acct_desc, description, len_description);
	init_unistr2(&usr->uni_workstations, workstations, len_workstations);
	init_unistr2(&usr->uni_unknown_str, unknown_str, len_unknown_str);
	init_unistr2(&usr->uni_munged_dial, munged_dial, len_munged_dial);

	usr->unknown_6 = unknown_6; /* 0x0000 04ec */
	usr->padding4 = 0;

	if (hrs)
		memcpy(&(usr->logon_hrs), hrs, sizeof(usr->logon_hrs));
	else
		memset(&(usr->logon_hrs), 0xff, sizeof(usr->logon_hrs));
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL sam_io_user_info21(char *desc,  SAM_USER_INFO_21 *usr, prs_struct *ps, int depth)
{
	if (usr == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_user_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_time("logon_time           ", &usr->logon_time, ps, depth))
		return False;
	if(!smb_io_time("logoff_time          ", &usr->logoff_time, ps, depth)) 
		return False;
	if(!smb_io_time("kickoff_time         ", &usr->kickoff_time, ps, depth)) 
		return False;
	if(!smb_io_time("pass_last_set_time   ", &usr->pass_last_set_time, ps, depth)) 
		return False;
	if(!smb_io_time("pass_can_change_time ", &usr->pass_can_change_time, ps, depth)) 
		return False;
	if(!smb_io_time("pass_must_change_time", &usr->pass_must_change_time, ps, depth)) 
		return False;

	if(!smb_io_unihdr("hdr_user_name   ", &usr->hdr_user_name, ps, depth)) /* username unicode string header */
		return False;
	if(!smb_io_unihdr("hdr_full_name   ", &usr->hdr_full_name, ps, depth)) /* user's full name unicode string header */
		return False;
	if(!smb_io_unihdr("hdr_home_dir    ", &usr->hdr_home_dir, ps, depth)) /* home directory unicode string header */
		return False;
	if(!smb_io_unihdr("hdr_dir_drive   ", &usr->hdr_dir_drive, ps, depth)) /* home directory drive */
		return False;
	if(!smb_io_unihdr("hdr_logon_script", &usr->hdr_logon_script, ps, depth)) /* logon script unicode string header */
		return False;
	if(!smb_io_unihdr("hdr_profile_path", &usr->hdr_profile_path, ps, depth)) /* profile path unicode string header */
		return False;
	if(!smb_io_unihdr("hdr_acct_desc   ", &usr->hdr_acct_desc, ps, depth)) /* account description */
		return False;
	if(!smb_io_unihdr("hdr_workstations", &usr->hdr_workstations, ps, depth)) /* workstations user can log on from */
		return False;
	if(!smb_io_unihdr("hdr_unknown_str ", &usr->hdr_unknown_str, ps, depth)) /* unknown string */
		return False;
	if(!smb_io_unihdr("hdr_munged_dial ", &usr->hdr_munged_dial, ps, depth)) /* workstations user can log on from */
		return False;

	if(!prs_uint8s (False, "lm_pwd        ", ps, depth, usr->lm_pwd, sizeof(usr->lm_pwd)))
		return False;
	if(!prs_uint8s (False, "nt_pwd        ", ps, depth, usr->nt_pwd, sizeof(usr->nt_pwd)))
		return False;

	if(!prs_uint32("user_rid      ", ps, depth, &usr->user_rid))       /* User ID */
		return False;
	if(!prs_uint32("group_rid     ", ps, depth, &usr->group_rid))      /* Group ID */
		return False;
	if(!prs_uint16("acb_info      ", ps, depth, &usr->acb_info))      /* Group ID */
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_3     ", ps, depth, &usr->unknown_3))
		return False;
	if(!prs_uint16("logon_divs    ", ps, depth, &usr->logon_divs))     /* logon divisions per week */
		return False;
	if(!prs_align(ps))
		return False;
	if(!prs_uint32("ptr_logon_hrs ", ps, depth, &usr->ptr_logon_hrs))
		return False;
	if(!prs_uint32("unknown_5     ", ps, depth, &usr->unknown_5))
		return False;

	if(!prs_uint8s (False, "padding1      ", ps, depth, usr->padding1, sizeof(usr->padding1)))
		return False;

	/* here begins pointed-to data */

	if(!smb_io_unistr2("uni_user_name   ", &usr->uni_user_name, usr->hdr_user_name.buffer, ps, depth)) /* username unicode string */
		return False;
	if(!smb_io_unistr2("uni_full_name   ", &usr->uni_full_name, usr->hdr_full_name.buffer, ps, depth)) /* user's full name unicode string */
		return False;
	if(!smb_io_unistr2("uni_home_dir    ", &usr->uni_home_dir, usr->hdr_home_dir.buffer, ps, depth)) /* home directory unicode string */
		return False;
	if(!smb_io_unistr2("uni_dir_drive   ", &usr->uni_dir_drive, usr->hdr_dir_drive.buffer, ps, depth)) /* home directory drive unicode string */
		return False;
	if(!smb_io_unistr2("uni_logon_script", &usr->uni_logon_script, usr->hdr_logon_script.buffer, ps, depth)) /* logon script unicode string */
		return False;
	if(!smb_io_unistr2("uni_profile_path", &usr->uni_profile_path, usr->hdr_profile_path.buffer, ps, depth)) /* profile path unicode string */
		return False;
	if(!smb_io_unistr2("uni_acct_desc   ", &usr->uni_acct_desc, usr->hdr_acct_desc.buffer, ps, depth)) /* user description unicode string */
		return False;
	if(!smb_io_unistr2("uni_workstations", &usr->uni_workstations, usr->hdr_workstations.buffer, ps, depth)) /* worksations user can log on from */
		return False;
	if(!smb_io_unistr2("uni_unknown_str ", &usr->uni_unknown_str, usr->hdr_unknown_str .buffer, ps, depth)) /* unknown string */
		return False;
	if(!smb_io_unistr2("uni_munged_dial ", &usr->uni_munged_dial, usr->hdr_munged_dial .buffer, ps, depth)) /* worksations user can log on from */
		return False;

	if(!prs_uint32("unknown_6     ", ps, depth, &usr->unknown_6))
		return False;
	if(!prs_uint32("padding4      ", ps, depth, &usr->padding4))
		return False;

	if (usr->ptr_logon_hrs) {
		if(!sam_io_logon_hrs("logon_hrs", &usr->logon_hrs, ps, depth))
			return False;
		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a SAMR_R_QUERY_USERINFO structure.
********************************************************************/

void init_samr_r_query_userinfo(SAMR_R_QUERY_USERINFO *r_u,
				uint16 switch_value, void *info, uint32 status)
{
	DEBUG(5,("init_samr_r_query_userinfo\n"));

	r_u->ptr = 0;
	r_u->switch_value = 0;

	if (status == 0) {
		r_u->switch_value = switch_value;

		switch (switch_value) {
		case 0x10:
			r_u->ptr = 1;
			r_u->info.id10 = (SAM_USER_INFO_10*)info;
			break;

		case 0x11:
			r_u->ptr = 1;
			r_u->info.id11 = (SAM_USER_INFO_11*)info;
			break;

		case 21:
			r_u->ptr = 1;
			r_u->info.id21 = (SAM_USER_INFO_21*)info;
			break;

		default:
			DEBUG(4,("init_samr_r_query_aliasinfo: unsupported switch level\n"));
			break;
		}
	}

	r_u->status = status;         /* return status */
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_query_userinfo(char *desc,  SAMR_R_QUERY_USERINFO *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_query_userinfo");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr         ", ps, depth, &r_u->ptr))
		return False;
	if(!prs_uint16("switch_value", ps, depth, &r_u->switch_value))
		return False;
	if(!prs_align(ps))
		return False;

	if (r_u->ptr != 0 && r_u->switch_value != 0) {
		switch (r_u->switch_value) {
		case 0x10:
			if (r_u->info.id10 != NULL) {
				if(!sam_io_user_info10("", r_u->info.id10, ps, depth))
					return False;
			} else {
				DEBUG(2,("samr_io_r_query_userinfo: info pointer not initialised\n"));
				return False;
			}
			break;
/*
		case 0x11:
			if (r_u->info.id11 != NULL) {
				if(!sam_io_user_info11("", r_u->info.id11, ps, depth))
					return False;
			} else {
				DEBUG(2,("samr_io_r_query_userinfo: info pointer not initialised\n"));
				return False;
			}
			break;
*/
		case 21:
			if (r_u->info.id21 != NULL) {
				if(!sam_io_user_info21("", r_u->info.id21, ps, depth))
					return False;
			} else {
				DEBUG(2,("samr_io_r_query_userinfo: info pointer not initialised\n"));
				return False;
			}
			break;
		default:
			DEBUG(2,("samr_io_r_query_userinfo: unknown switch level\n"));
			break;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_unknown_32(char *desc,  SAMR_Q_UNKNOWN_32 *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_unknown_32");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &q_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!smb_io_unihdr ("", &q_u->hdr_mach_acct, ps, depth))
		return False;
	if(!smb_io_unistr2("", &q_u->uni_mach_acct, q_u->hdr_mach_acct.buffer, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("acct_ctrl", ps, depth, &q_u->acct_ctrl))
		return False;
	if(!prs_uint16("unknown_1", ps, depth, &q_u->unknown_1))
		return False;
	if(!prs_uint16("unknown_2", ps, depth, &q_u->unknown_2))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_unknown_32(char *desc,  SAMR_R_UNKNOWN_32 *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_32");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &r_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_CONNECT structure.
********************************************************************/

void init_samr_q_connect(SAMR_Q_CONNECT *q_u,
				char *srv_name, uint32 unknown_0)
{
	int len_srv_name = strlen(srv_name);

	DEBUG(5,("init_q_connect\n"));

	/* make PDC server name \\server */
	q_u->ptr_srv_name = len_srv_name > 0 ? 1 : 0; 
	init_unistr2(&q_u->uni_srv_name, srv_name, len_srv_name+1);  

	/* example values: 0x0000 0002 */
	q_u->unknown_0 = unknown_0; 
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_connect(char *desc,  SAMR_Q_CONNECT *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_connect");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name", ps, depth, &q_u->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_u->uni_srv_name, q_u->ptr_srv_name, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0   ", ps, depth, &q_u->unknown_0))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_connect(char *desc,  SAMR_R_CONNECT *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_connect");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("connect_pol", &r_u->connect_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_CONNECT_ANON structure.
********************************************************************/

void init_samr_q_connect_anon(SAMR_Q_CONNECT_ANON *q_u)
{
	DEBUG(5,("init_q_connect_anon\n"));

	q_u->ptr       = 1;
	q_u->unknown_0 = 0x5c; /* server name (?!!) */
	q_u->unknown_1 = 0x01;
	q_u->unknown_2 = 0x20;
}


/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_connect_anon(char *desc,  SAMR_Q_CONNECT_ANON *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_connect_anon");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr      ", ps, depth, &q_u->ptr))
		return False;
	if(!prs_uint16("unknown_0", ps, depth, &q_u->unknown_0))
		return False;
	if(!prs_uint16("unknown_1", ps, depth, &q_u->unknown_1))
		return False;
	if(!prs_uint32("unknown_2", ps, depth, &q_u->unknown_2))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_connect_anon(char *desc,  SAMR_R_CONNECT_ANON *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_connect_anon");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("connect_pol", &r_u->connect_pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_OPEN_ALIAS structure.
********************************************************************/
void init_samr_q_open_alias(SAMR_Q_OPEN_ALIAS *q_u,
				uint32 unknown_0, uint32 rid)
{
	DEBUG(5,("init_q_open_alias\n"));

	/* example values: 0x0000 0008 */
	q_u->unknown_0 = unknown_0; 

	q_u->rid_alias = rid; 
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_open_alias(char *desc,  SAMR_Q_OPEN_ALIAS *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_open_alias");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("unknown_0", ps, depth, &q_u->unknown_0))
		return False;
	if(!prs_uint32("rid_alias", ps, depth, &q_u->rid_alias))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_open_alias(char *desc,  SAMR_R_OPEN_ALIAS *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_open_alias");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("pol", &r_u->pol, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_Q_UNKNOWN_12 structure.
********************************************************************/

void init_samr_q_unknown_12(SAMR_Q_UNKNOWN_12 *q_u,
		POLICY_HND *pol, uint32 rid,
		uint32 num_gids, uint32 *gid)
{
	int i;

	DEBUG(5,("init_samr_r_unknwon_12\n"));

	memcpy(&q_u->pol, pol, sizeof(*pol));

	q_u->num_gids1 = num_gids;
	q_u->rid       = rid;
	q_u->ptr       = 0;
	q_u->num_gids2 = num_gids;

	for (i = 0; i < num_gids; i++) {
		q_u->gid[i] = gid[i];
	}
}

/*******************************************************************
 Inits a SAMR_Q_UNKNOWN_21 structure.
********************************************************************/

void init_samr_q_unknown_21(SAMR_Q_UNKNOWN_21 *q_c,
				POLICY_HND *hnd, uint16 unk_1, uint16 unk_2)
{
	DEBUG(5,("init_samr_q_unknown_21\n"));

	memcpy(&q_c->group_pol, hnd, sizeof(q_c->group_pol));
	q_c->unknown_1 = unk_1;
	q_c->unknown_2 = unk_2;
}


/*******************************************************************
 Inits a SAMR_Q_UNKNOWN_13 structure.
********************************************************************/

void init_samr_q_unknown_13(SAMR_Q_UNKNOWN_13 *q_c,
				POLICY_HND *hnd, uint16 unk_1, uint16 unk_2)
{
	DEBUG(5,("init_samr_q_unknown_13\n"));

	memcpy(&q_c->alias_pol, hnd, sizeof(q_c->alias_pol));
	q_c->unknown_1 = unk_1;
	q_c->unknown_2 = unk_2;
}

/*******************************************************************
 Inits a SAMR_Q_UNKNOWN_38 structure.
********************************************************************/
void init_samr_q_unknown_38(SAMR_Q_UNKNOWN_38 *q_u, char *srv_name)
{
	int len_srv_name = strlen(srv_name);

	DEBUG(5,("init_q_unknown_38\n"));

	q_u->ptr = 1;
	init_uni_hdr(&q_u->hdr_srv_name, len_srv_name);
	init_unistr2(&q_u->uni_srv_name, srv_name, len_srv_name);  

}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_unknown_38(char *desc,  SAMR_Q_UNKNOWN_38 *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_unknown_38");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr", ps, depth, &q_u->ptr))
		return False;

	if (q_u->ptr != 0) {
		if(!smb_io_unihdr ("", &q_u->hdr_srv_name, ps, depth))
			return False;
		if(!smb_io_unistr2("", &q_u->uni_srv_name, q_u->hdr_srv_name.buffer, ps, depth))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a SAMR_R_UNKNOWN_38 structure.
********************************************************************/

void init_samr_r_unknown_38(SAMR_R_UNKNOWN_38 *r_u)
{
	DEBUG(5,("init_r_unknown_38\n"));

	r_u->unk_0 = 0;
	r_u->unk_1 = 0;
	r_u->unk_2 = 0;
	r_u->unk_3 = 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_unknown_38(char *desc,  SAMR_R_UNKNOWN_38 *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_unknown_38");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint16("unk_0", ps, depth, &r_u->unk_0))
		return False;
	if(!prs_align(ps))
		return False;
	if(!prs_uint16("unk_1", ps, depth, &r_u->unk_1))
		return False;
	if(!prs_align(ps))
		return False;
	if(!prs_uint16("unk_2", ps, depth, &r_u->unk_2))
		return False;
	if(!prs_align(ps))
		return False;
	if(!prs_uint16("unk_3", ps, depth, &r_u->unk_3))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
make a SAMR_ENC_PASSWD structure.
********************************************************************/

void init_enc_passwd(SAMR_ENC_PASSWD *pwd, char pass[512])
{
	pwd->ptr = 1;
	memcpy(pwd->pass, pass, sizeof(pwd->pass)); 
}

/*******************************************************************
 Reads or writes a SAMR_ENC_PASSWD structure.
********************************************************************/

BOOL samr_io_enc_passwd(char *desc, SAMR_ENC_PASSWD *pwd, prs_struct *ps, int depth)
{
	if (pwd == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_enc_passwd");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr", ps, depth, &pwd->ptr))
		return False;
	if(!prs_uint8s(False, "pwd", ps, depth, pwd->pass, sizeof(pwd->pass)))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_ENC_HASH structure.
********************************************************************/

void init_enc_hash(SAMR_ENC_HASH *hsh, uchar hash[16])
{
	hsh->ptr = 1;
	memcpy(hsh->hash, hash, sizeof(hsh->hash));
}

/*******************************************************************
 Reads or writes a SAMR_ENC_HASH structure.
********************************************************************/

BOOL samr_io_enc_hash(char *desc, SAMR_ENC_HASH *hsh, prs_struct *ps, int depth)
{
	if (hsh == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_enc_hash");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr ", ps, depth, &hsh->ptr))
		return False;
	if(!prs_uint8s(False, "hash", ps, depth, hsh->hash, sizeof(hsh->hash))) 
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_UNKNOWN_38 structure.
********************************************************************/

void init_samr_q_chgpasswd_user(SAMR_Q_CHGPASSWD_USER *q_u,
				char *dest_host, char *user_name,
				char nt_newpass[516], uchar nt_oldhash[16],
				char lm_newpass[516], uchar lm_oldhash[16])
{
	int len_dest_host = strlen(dest_host);
	int len_user_name = strlen(user_name);

	DEBUG(5,("init_samr_q_chgpasswd_user\n"));

	q_u->ptr_0 = 1;
	init_uni_hdr(&q_u->hdr_dest_host, len_dest_host);
	init_unistr2(&q_u->uni_dest_host, dest_host, len_dest_host);  
	init_uni_hdr(&q_u->hdr_user_name, len_user_name);
	init_unistr2(&q_u->uni_user_name, user_name, len_user_name);  

	init_enc_passwd(&q_u->nt_newpass, nt_newpass);
	init_enc_hash(&q_u->nt_oldhash, nt_oldhash);

	q_u->unknown = 0x01;

	init_enc_passwd(&q_u->lm_newpass, lm_newpass);
	init_enc_hash  (&q_u->lm_oldhash, lm_oldhash);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_q_chgpasswd_user(char *desc, SAMR_Q_CHGPASSWD_USER *q_u, prs_struct *ps, int depth)
{
	if (q_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_q_chgpasswd_user");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_0", ps, depth, &q_u->ptr_0))
		return False;

	if(!smb_io_unihdr ("", &q_u->hdr_dest_host, ps, depth))
		return False;
	if(!smb_io_unistr2("", &q_u->uni_dest_host, q_u->hdr_dest_host.buffer, ps, depth))
		return False;
	if(!smb_io_unihdr ("", &q_u->hdr_user_name, ps, depth))
		return False;
	if(!smb_io_unistr2("", &q_u->uni_user_name, q_u->hdr_user_name.buffer, ps, depth))
		return False;

	if(!samr_io_enc_passwd("nt_newpass", &q_u->nt_newpass, ps, depth))
		return False;
	if(!samr_io_enc_hash  ("nt_oldhash", &q_u->nt_oldhash, ps, depth))
		return False;

	if(!prs_uint32("unknown", ps, depth, &q_u->unknown))
		return False;

	if(!samr_io_enc_passwd("lm_newpass", &q_u->lm_newpass, ps, depth))
		return False;
	if(!samr_io_enc_hash("lm_oldhash", &q_u->lm_oldhash, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SAMR_R_CHGPASSWD_USER structure.
********************************************************************/

void init_samr_r_chgpasswd_user(SAMR_R_CHGPASSWD_USER *r_u, uint32 status)
{
	DEBUG(5,("init_r_chgpasswd_user\n"));

	r_u->status = status;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL samr_io_r_chgpasswd_user(char *desc, SAMR_R_CHGPASSWD_USER *r_u, prs_struct *ps, int depth)
{
	if (r_u == NULL)
		return False;

	prs_debug(ps, depth, desc, "samr_io_r_chgpasswd_user");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_u->status))
		return False;

	return True;
}
