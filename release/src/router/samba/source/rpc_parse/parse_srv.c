
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison		    1999.
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
 Inits a SH_INFO_1_STR structure
********************************************************************/

void init_srv_share_info1_str(SH_INFO_1_STR *sh1, char *net_name, char *remark)
{
	DEBUG(5,("init_srv_share_info1_str\n"));

	init_unistr2(&sh1->uni_netname, net_name, strlen(net_name)+1);
	init_unistr2(&sh1->uni_remark, remark, strlen(remark)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_share_info1_str(char *desc, SH_INFO_1_STR *sh1, prs_struct *ps, int depth)
{
	if (sh1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_share_info1_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &sh1->uni_netname, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &sh1->uni_remark, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 makes a SH_INFO_1 structure
********************************************************************/

void init_srv_share_info1(SH_INFO_1 *sh1, char *net_name, uint32 type, char *remark)
{
	DEBUG(5,("init_srv_share_info1: %s %8x %s\n", net_name, type, remark));

	sh1->ptr_netname = (net_name != NULL) ? 1 : 0;
	sh1->type        = type;
	sh1->ptr_remark  = (remark != NULL) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_share_info1(char *desc, SH_INFO_1 *sh1, prs_struct *ps, int depth)
{
	if (sh1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_share_info1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_netname", ps, depth, &sh1->ptr_netname))
		return False;
	if(!prs_uint32("type       ", ps, depth, &sh1->type))
		return False;
	if(!prs_uint32("ptr_remark ", ps, depth, &sh1->ptr_remark))
		return False;

	return True;
}

/*******************************************************************
 Inits a SH_INFO_2_STR structure
********************************************************************/

void init_srv_share_info2_str(SH_INFO_2_STR *sh2,
				char *net_name, char *remark,
				char *path, char *passwd)
{
	DEBUG(5,("init_srv_share_info2_str\n"));

	init_unistr2(&sh2->uni_netname, net_name, strlen(net_name)+1);
	init_unistr2(&sh2->uni_remark, remark, strlen(remark)+1);
	init_unistr2(&sh2->uni_path, path, strlen(path)+1);
	init_unistr2(&sh2->uni_passwd, passwd, strlen(passwd)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_share_info2_str(char *desc, SH_INFO_2_STR *sh2, prs_struct *ps, int depth)
{
	if (sh2 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_share_info2_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &sh2->uni_netname, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &sh2->uni_remark, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &sh2->uni_path, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &sh2->uni_passwd, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SH_INFO_2 structure
********************************************************************/

void init_srv_share_info2(SH_INFO_2 *sh2,
				char *net_name, uint32 type, char *remark,
				uint32 perms, uint32 max_uses, uint32 num_uses,
				char *path, char *passwd)
{
	DEBUG(5,("init_srv_share_info2: %s %8x %s\n", net_name, type, remark));

	sh2->ptr_netname = (net_name != NULL) ? 1 : 0;
	sh2->type        = type;
	sh2->ptr_remark  = (remark != NULL) ? 1 : 0;
	sh2->perms       = perms;
	sh2->max_uses    = max_uses;
	sh2->num_uses    = num_uses;
	sh2->type        = type;
	sh2->ptr_path    = (path != NULL) ? 1 : 0;
	sh2->ptr_passwd  = (passwd != NULL) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_share_info2(char *desc, SH_INFO_2 *sh2, prs_struct *ps, int depth)
{
	if (sh2 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_share_info2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_netname", ps, depth, &sh2->ptr_netname))
		return False;
	if(!prs_uint32("type       ", ps, depth, &sh2->type))
		return False;
	if(!prs_uint32("ptr_remark ", ps, depth, &sh2->ptr_remark))
		return False;
	if(!prs_uint32("perms      ", ps, depth, &sh2->perms))
		return False;
	if(!prs_uint32("max_uses   ", ps, depth, &sh2->max_uses))
		return False;
	if(!prs_uint32("num_uses   ", ps, depth, &sh2->num_uses))
		return False;
	if(!prs_uint32("ptr_path   ", ps, depth, &sh2->ptr_path))
		return False;
	if(!prs_uint32("ptr_passwd ", ps, depth, &sh2->ptr_passwd))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_share_ctr(char *desc, SRV_SHARE_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_share_ctr");
	depth++;

	if (UNMARSHALLING(ps)) {
		memset(ctr, '\0', sizeof(SRV_SHARE_INFO_CTR));
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("info_level", ps, depth, &ctr->info_level))
		return False;

	if (ctr->info_level == 0)
		return True;

	if(!prs_uint32("switch_value", ps, depth, &ctr->switch_value))
		return False;
	if(!prs_uint32("ptr_share_info", ps, depth, &ctr->ptr_share_info))
		return False;

	if (ctr->ptr_share_info == 0)
		return True;

	if(!prs_uint32("num_entries", ps, depth, &ctr->num_entries))
		return False;
	if(!prs_uint32("ptr_entries", ps, depth, &ctr->ptr_entries))
		return False;

	if (ctr->ptr_entries == 0) {
		if (ctr->num_entries == 0)
			return True;
		else
			return False;
	}

	if(!prs_uint32("num_entries2", ps, depth, &ctr->num_entries2))
		return False;

	if (ctr->num_entries2 != ctr->num_entries)
		return False;

	switch (ctr->switch_value) {
	case 1:
	{
		SRV_SHARE_INFO_1 *info1 = ctr->share.info1;
		int num_entries = ctr->num_entries;
		int i;

		if (UNMARSHALLING(ps)) {
			if (!(info1 = malloc(num_entries * sizeof(SRV_SHARE_INFO_1))))
				return False;
			memset(info1, '\0', num_entries * sizeof(SRV_SHARE_INFO_1));
			ctr->share.info1 = info1;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_share_info1("", &info1[i].info_1, ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_share_info1_str("", &info1[i].info_1_str, ps, depth))
				return False;
		}

		break;
	}

	case 2:
	{
		SRV_SHARE_INFO_2 *info2 = ctr->share.info2;
		int num_entries = ctr->num_entries;
		int i;

		if (UNMARSHALLING(ps)) {
			if (!(info2 = malloc(num_entries * sizeof(SRV_SHARE_INFO_2))))
				return False;
			memset(info2, '\0', num_entries * sizeof(SRV_SHARE_INFO_2));
			ctr->share.info2 = info2;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_share_info2("", &info2[i].info_2, ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_share_info2_str("", &info2[i].info_2_str, ps, depth))
				return False;
		}

		break;
	}

	default:
		DEBUG(5,("%s no share info at switch_value %d\n",
			 tab_depth(depth), ctr->switch_value));
		break;
	}

	return True;
}

/*******************************************************************
 Frees a SRV_SHARE_INFO_CTR structure.
********************************************************************/

void free_srv_share_info_ctr(SRV_SHARE_INFO_CTR *ctr)
{
	if(!ctr)
		return;
	if(ctr->share.info)
		free(ctr->share.info);
	memset(ctr, '\0', sizeof(SRV_SHARE_INFO_CTR));
}

/*******************************************************************
 Frees a SRV_Q_NET_SHARE_ENUM structure.
********************************************************************/

void free_srv_q_net_share_enum(SRV_Q_NET_SHARE_ENUM *q_n)
{
	if(!q_n)
		return;
	free_srv_share_info_ctr(&q_n->ctr);
	memset(q_n, '\0', sizeof(SRV_Q_NET_SHARE_ENUM));
}

/*******************************************************************
 Frees a SRV_R_NET_SHARE_ENUM structure.
********************************************************************/

void free_srv_r_net_share_enum(SRV_R_NET_SHARE_ENUM *r_n)
{
	if(!r_n)
		return;
	free_srv_share_info_ctr(&r_n->ctr);
	memset(r_n, '\0', sizeof(SRV_R_NET_SHARE_ENUM));
}

/*******************************************************************
 Inits a SRV_Q_NET_SHARE_ENUM structure.
********************************************************************/

void init_srv_q_net_share_enum(SRV_Q_NET_SHARE_ENUM *q_n, 
				char *srv_name, uint32 info_level,
				uint32 preferred_len, ENUM_HND *hnd)
{

	DEBUG(5,("init_q_net_share_enum\n"));

	init_buf_unistr2(&q_n->uni_srv_name, &q_n->ptr_srv_name, srv_name);

	q_n->ctr.info_level = q_n->ctr.switch_value = info_level;
	q_n->ctr.ptr_share_info = 0;
	q_n->preferred_len = preferred_len;

	memcpy(&q_n->enum_hnd, hnd, sizeof(*hnd));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_share_enum(char *desc, SRV_Q_NET_SHARE_ENUM *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_share_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	if(!srv_io_srv_share_ctr("share_ctr", &q_n->ctr, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("preferred_len", ps, depth, &q_n->preferred_len))
		return False;

	if(!smb_io_enum_hnd("enum_hnd", &q_n->enum_hnd, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_r_net_share_enum(char *desc, SRV_R_NET_SHARE_ENUM *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_share_enum");
	depth++;

	if(!srv_io_srv_share_ctr("share_ctr", &r_n->ctr, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("total_entries", ps, depth, &r_n->total_entries))
		return False;
	if(!smb_io_enum_hnd("enum_hnd", &r_n->enum_hnd, ps, depth))
		return False;
	if(!prs_uint32("status     ", ps, depth, &r_n->status))
		return False;

	return True;
}

/*******************************************************************
 Frees a SRV_Q_NET_SHARE_GET_INFO structure.
********************************************************************/

void free_srv_q_net_share_get_info(SRV_Q_NET_SHARE_GET_INFO *q_n)
{
	if(!q_n)
		return;
	memset(q_n, '\0', sizeof(SRV_Q_NET_SHARE_GET_INFO));
}

/*******************************************************************
 Frees a SRV_R_NET_SHARE_GET_INFO structure.
********************************************************************/

void free_srv_r_net_share_get_info(SRV_R_NET_SHARE_GET_INFO *r_n)
{
	if(!r_n)
		return;
	memset(r_n, '\0', sizeof(SRV_R_NET_SHARE_GET_INFO));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_share_get_info(char *desc, SRV_Q_NET_SHARE_GET_INFO *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_share_get_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	if(!smb_io_unistr2("", &q_n->uni_share_name, True, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("info_level", ps, depth, &q_n->info_level))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_r_net_share_get_info(char *desc, SRV_R_NET_SHARE_GET_INFO *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_share_get_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value ", ps, depth, &r_n->switch_value ))
		return False;

	if(!prs_uint32("ptr_share_ctr", ps, depth, &r_n->ptr_share_ctr))
		return False;

	if (r_n->ptr_share_ctr != 0) {
		switch (r_n->switch_value) {
		case 1:
			if(!srv_io_share_info1("", &r_n->share.info1.info_1, ps, depth))
				return False;

			if(!srv_io_share_info1_str("", &r_n->share.info1.info_1_str, ps, depth))
				return False;

			break;
		case 2:
			if(!srv_io_share_info2("", &r_n->share.info2.info_2, ps, depth))
				return False;

			if(!srv_io_share_info2_str("", &r_n->share.info2.info_2_str, ps, depth))
				return False;

			break;
		default:
			DEBUG(5,("%s no share info at switch_value %d\n",
			         tab_depth(depth), r_n->switch_value));
			break;
		}
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("status", ps, depth, &r_n->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SESS_INFO_0_STR structure
********************************************************************/

void init_srv_sess_info0_str(SESS_INFO_0_STR *ss0, char *name)
{
	DEBUG(5,("init_srv_sess_info0_str\n"));

	init_unistr2(&ss0->uni_name, name, strlen(name)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_sess_info0_str(char *desc,  SESS_INFO_0_STR *ss0, prs_struct *ps, int depth)
{
	if (ss0 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_sess_info0_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &ss0->uni_name, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SESS_INFO_0 structure
********************************************************************/

void init_srv_sess_info0(SESS_INFO_0 *ss0, char *name)
{
	DEBUG(5,("init_srv_sess_info0: %s\n", name));

	ss0->ptr_name = (name != NULL) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_sess_info0(char *desc, SESS_INFO_0 *ss0, prs_struct *ps, int depth)
{
	if (ss0 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_sess_info0");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_name", ps, depth, &ss0->ptr_name))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_sess_info_0(char *desc, SRV_SESS_INFO_0 *ss0, prs_struct *ps, int depth)
{
	if (ss0 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_sess_info_0");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries_read", ps, depth, &ss0->num_entries_read))
		return False;
	if(!prs_uint32("ptr_sess_info", ps, depth, &ss0->ptr_sess_info))
		return False;

	if (ss0->ptr_sess_info != 0) {
		int i;
		int num_entries = ss0->num_entries_read;

		if (num_entries > MAX_SESS_ENTRIES) {
			num_entries = MAX_SESS_ENTRIES; /* report this! */
		}

		if(!prs_uint32("num_entries_read2", ps, depth, &ss0->num_entries_read2))
			return False;

		SMB_ASSERT_ARRAY(ss0->info_0, num_entries);

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_sess_info0("", &ss0->info_0[i], ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_sess_info0_str("", &ss0->info_0_str[i], ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a SESS_INFO_1_STR structure
********************************************************************/

void init_srv_sess_info1_str(SESS_INFO_1_STR *ss1, char *name, char *user)
{
	DEBUG(5,("init_srv_sess_info1_str\n"));

	init_unistr2(&ss1->uni_name, name, strlen(name)+1);
	init_unistr2(&ss1->uni_user, name, strlen(user)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_sess_info1_str(char *desc, SESS_INFO_1_STR *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_sess_info1_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &ss1->uni_name, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &(ss1->uni_user), True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SESS_INFO_1 structure
********************************************************************/

void init_srv_sess_info1(SESS_INFO_1 *ss1, 
				char *name, char *user,
				uint32 num_opens, uint32 open_time, uint32 idle_time,
				uint32 user_flags)
{
	DEBUG(5,("init_srv_sess_info1: %s\n", name));

	ss1->ptr_name = (name != NULL) ? 1 : 0;
	ss1->ptr_user = (user != NULL) ? 1 : 0;

	ss1->num_opens  = num_opens;
	ss1->open_time  = open_time;
	ss1->idle_time  = idle_time;
	ss1->user_flags = user_flags;
}

/*******************************************************************
reads or writes a structure.
********************************************************************/

static BOOL srv_io_sess_info1(char *desc, SESS_INFO_1 *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_sess_info1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_name  ", ps, depth, &ss1->ptr_name))
		return False;
	if(!prs_uint32("ptr_user  ", ps, depth, &ss1->ptr_user))
		return False;

	if(!prs_uint32("num_opens ", ps, depth, &ss1->num_opens))
		return False;
	if(!prs_uint32("open_time ", ps, depth, &ss1->open_time))
		return False;
	if(!prs_uint32("idle_time ", ps, depth, &ss1->idle_time))
		return False;
	if(!prs_uint32("user_flags", ps, depth, &ss1->user_flags))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_sess_info_1(char *desc, SRV_SESS_INFO_1 *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_sess_info_1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries_read", ps, depth, &ss1->num_entries_read))
		return False;
	if(!prs_uint32("ptr_sess_info", ps, depth, &ss1->ptr_sess_info))
		return False;

	if (ss1->ptr_sess_info != 0) {
		int i;
		int num_entries = ss1->num_entries_read;

		if (num_entries > MAX_SESS_ENTRIES) {
			num_entries = MAX_SESS_ENTRIES; /* report this! */
		}

		if(!prs_uint32("num_entries_read2", ps, depth, &ss1->num_entries_read2))
			return False;

		SMB_ASSERT_ARRAY(ss1->info_1, num_entries);

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_sess_info1("", &ss1->info_1[i], ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_sess_info1_str("", &ss1->info_1_str[i], ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_sess_ctr(char *desc, SRV_SESS_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_sess_ctr");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value", ps, depth, &ctr->switch_value))
		return False;
	if(!prs_uint32("ptr_sess_ctr", ps, depth, &ctr->ptr_sess_ctr))
		return False;

	if (ctr->ptr_sess_ctr != 0) {
		switch (ctr->switch_value) {
		case 0:
			if(!srv_io_srv_sess_info_0("", &ctr->sess.info0, ps, depth))
				return False;
			break;
		case 1:
			if(!srv_io_srv_sess_info_1("", &ctr->sess.info1, ps, depth))
				return False;
			break;
		default:
			DEBUG(5,("%s no session info at switch_value %d\n",
			         tab_depth(depth), ctr->switch_value));
			break;
		}
	}

	return True;
}

/*******************************************************************
 Inits a SRV_Q_NET_SESS_ENUM structure.
********************************************************************/

void init_srv_q_net_sess_enum(SRV_Q_NET_SESS_ENUM *q_n, 
				char *srv_name, char *qual_name,
				uint32 sess_level, SRV_SESS_INFO_CTR *ctr,
				uint32 preferred_len,
				ENUM_HND *hnd)
{
	q_n->ctr = ctr;

	DEBUG(5,("init_q_net_sess_enum\n"));

	init_buf_unistr2(&q_n->uni_srv_name, &q_n->ptr_srv_name, srv_name);
	init_buf_unistr2(&q_n->uni_qual_name, &q_n->ptr_qual_name, qual_name);

	q_n->sess_level    = sess_level;
	q_n->preferred_len = preferred_len;

	memcpy(&q_n->enum_hnd, hnd, sizeof(*hnd));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_sess_enum(char *desc, SRV_Q_NET_SESS_ENUM *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_sess_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_qual_name", ps, depth, &q_n->ptr_qual_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_qual_name, q_n->ptr_qual_name, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("sess_level", ps, depth, &q_n->sess_level))
		return False;
	
	if (q_n->sess_level != -1) {
		if(!srv_io_srv_sess_ctr("sess_ctr", q_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("preferred_len", ps, depth, &q_n->preferred_len))
		return False;

	if(!smb_io_enum_hnd("enum_hnd", &q_n->enum_hnd, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_r_net_sess_enum(char *desc, SRV_R_NET_SESS_ENUM *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_sess_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("sess_level", ps, depth, &r_n->sess_level))
		return False;

	if (r_n->sess_level != -1) {
		if(!srv_io_srv_sess_ctr("sess_ctr", r_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("total_entries", ps, depth, &r_n->total_entries))
		return False;
	if(!smb_io_enum_hnd("enum_hnd", &r_n->enum_hnd, ps, depth))
		return False;
	if(!prs_uint32("status     ", ps, depth, &r_n->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a CONN_INFO_0 structure
********************************************************************/

void init_srv_conn_info0(CONN_INFO_0 *ss0, uint32 id)
{
	DEBUG(5,("init_srv_conn_info0\n"));

	ss0->id = id;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_conn_info0(char *desc, CONN_INFO_0 *ss0, prs_struct *ps, int depth)
{
	if (ss0 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_conn_info0");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("id", ps, depth, &ss0->id))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_conn_info_0(char *desc, SRV_CONN_INFO_0 *ss0, prs_struct *ps, int depth)
{
	if (ss0 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_conn_info_0");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries_read", ps, depth, &ss0->num_entries_read))
		return False;
	if(!prs_uint32("ptr_conn_info", ps, depth, &ss0->ptr_conn_info))
		return False;

	if (ss0->ptr_conn_info != 0) {
		int i;
		int num_entries = ss0->num_entries_read;

		if (num_entries > MAX_CONN_ENTRIES) {
			num_entries = MAX_CONN_ENTRIES; /* report this! */
		}

		if(!prs_uint32("num_entries_read2", ps, depth, &ss0->num_entries_read2))
			return False;

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_conn_info0("", &ss0->info_0[i], ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a CONN_INFO_1_STR structure
********************************************************************/

void init_srv_conn_info1_str(CONN_INFO_1_STR *ss1, char *usr_name, char *net_name)
{
	DEBUG(5,("init_srv_conn_info1_str\n"));

	init_unistr2(&ss1->uni_usr_name, usr_name, strlen(usr_name)+1);
	init_unistr2(&ss1->uni_net_name, net_name, strlen(net_name)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_conn_info1_str(char *desc, CONN_INFO_1_STR *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_conn_info1_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &ss1->uni_usr_name, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &ss1->uni_net_name, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a CONN_INFO_1 structure
********************************************************************/

void init_srv_conn_info1(CONN_INFO_1 *ss1, 
				uint32 id, uint32 type,
				uint32 num_opens, uint32 num_users, uint32 open_time,
				char *usr_name, char *net_name)
{
	DEBUG(5,("init_srv_conn_info1: %s %s\n", usr_name, net_name));

	ss1->id        = id       ;
	ss1->type      = type     ;
	ss1->num_opens = num_opens ;
	ss1->num_users = num_users;
	ss1->open_time = open_time;

	ss1->ptr_usr_name = (usr_name != NULL) ? 1 : 0;
	ss1->ptr_net_name = (net_name != NULL) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_conn_info1(char *desc, CONN_INFO_1 *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_conn_info1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("id          ", ps, depth, &ss1->id))
		return False;
	if(!prs_uint32("type        ", ps, depth, &ss1->type))
		return False;
	if(!prs_uint32("num_opens   ", ps, depth, &ss1->num_opens))
		return False;
	if(!prs_uint32("num_users   ", ps, depth, &ss1->num_users))
		return False;
	if(!prs_uint32("open_time   ", ps, depth, &ss1->open_time))
		return False;

	if(!prs_uint32("ptr_usr_name", ps, depth, &ss1->ptr_usr_name))
		return False;
	if(!prs_uint32("ptr_net_name", ps, depth, &ss1->ptr_net_name))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_conn_info_1(char *desc, SRV_CONN_INFO_1 *ss1, prs_struct *ps, int depth)
{
	if (ss1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_conn_info_1");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries_read", ps, depth, &ss1->num_entries_read))
		return False;
	if(!prs_uint32("ptr_conn_info", ps, depth, &ss1->ptr_conn_info))
		return False;

	if (ss1->ptr_conn_info != 0) {
		int i;
		int num_entries = ss1->num_entries_read;

		if (num_entries > MAX_CONN_ENTRIES) {
			num_entries = MAX_CONN_ENTRIES; /* report this! */
		}

		if(!prs_uint32("num_entries_read2", ps, depth, &ss1->num_entries_read2))
			return False;

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_conn_info1("", &ss1->info_1[i], ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_conn_info1_str("", &ss1->info_1_str[i], ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_conn_ctr(char *desc, SRV_CONN_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_conn_ctr");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value", ps, depth, &ctr->switch_value))
		return False;
	if(!prs_uint32("ptr_conn_ctr", ps, depth, &ctr->ptr_conn_ctr))
		return False;

	if (ctr->ptr_conn_ctr != 0) {
		switch (ctr->switch_value) {
		case 0:
			if(!srv_io_srv_conn_info_0("", &ctr->conn.info0, ps, depth))
				return False;
			break;
		case 1:
			if(!srv_io_srv_conn_info_1("", &ctr->conn.info1, ps, depth))
				return False;
			break;
		default:
			DEBUG(5,("%s no connection info at switch_value %d\n",
			         tab_depth(depth), ctr->switch_value));
			break;
		}
	}

	return True;
}

/*******************************************************************
  Reads or writes a structure.
********************************************************************/

void init_srv_q_net_conn_enum(SRV_Q_NET_CONN_ENUM *q_n, 
				char *srv_name, char *qual_name,
				uint32 conn_level, SRV_CONN_INFO_CTR *ctr,
				uint32 preferred_len,
				ENUM_HND *hnd)
{
	DEBUG(5,("init_q_net_conn_enum\n"));

	q_n->ctr = ctr;

	init_buf_unistr2(&q_n->uni_srv_name, &q_n->ptr_srv_name, srv_name );
	init_buf_unistr2(&q_n->uni_qual_name, &q_n->ptr_qual_name, qual_name);

	q_n->conn_level    = conn_level;
	q_n->preferred_len = preferred_len;

	memcpy(&q_n->enum_hnd, hnd, sizeof(*hnd));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_conn_enum(char *desc, SRV_Q_NET_CONN_ENUM *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_conn_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name ", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, q_n->ptr_srv_name, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_qual_name", ps, depth, &q_n->ptr_qual_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_qual_name, q_n->ptr_qual_name, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("conn_level", ps, depth, &q_n->conn_level))
		return False;
	
	if (q_n->conn_level != -1) {
		if(!srv_io_srv_conn_ctr("conn_ctr", q_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("preferred_len", ps, depth, &q_n->preferred_len))
		return False;

	if(!smb_io_enum_hnd("enum_hnd", &q_n->enum_hnd, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_r_net_conn_enum(char *desc,  SRV_R_NET_CONN_ENUM *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_conn_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("conn_level", ps, depth, &r_n->conn_level))
		return False;

	if (r_n->conn_level != -1) {
		if(!srv_io_srv_conn_ctr("conn_ctr", r_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("total_entries", ps, depth, &r_n->total_entries))
		return False;
	if(!smb_io_enum_hnd("enum_hnd", &r_n->enum_hnd, ps, depth))
		return False;
	if(!prs_uint32("status     ", ps, depth, &r_n->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a FILE_INFO_3_STR structure
********************************************************************/

void init_srv_file_info3_str(FILE_INFO_3_STR *fi3, char *user_name, char *path_name)
{
	DEBUG(5,("init_srv_file_info3_str\n"));

	init_unistr2(&fi3->uni_path_name, path_name, strlen(path_name)+1);
	init_unistr2(&fi3->uni_user_name, user_name, strlen(user_name)+1);
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_file_info3_str(char *desc, FILE_INFO_3_STR *sh1, prs_struct *ps, int depth)
{
	if (sh1 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_file_info3_str");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("", &sh1->uni_path_name, True, ps, depth))
		return False;
	if(!smb_io_unistr2("", &sh1->uni_user_name, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a FILE_INFO_3 structure
********************************************************************/

void init_srv_file_info3(FILE_INFO_3 *fl3,
				uint32 id, uint32 perms, uint32 num_locks,
				char *path_name, char *user_name)
{
	DEBUG(5,("init_srv_file_info3: %s %s\n", path_name, user_name));

	fl3->id        = id;	
	fl3->perms     = perms;
	fl3->num_locks = num_locks;

	fl3->ptr_path_name = (path_name != NULL) ? 1 : 0;
	fl3->ptr_user_name = (user_name != NULL) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_file_info3(char *desc, FILE_INFO_3 *fl3, prs_struct *ps, int depth)
{
	if (fl3 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_file_info3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("id           ", ps, depth, &fl3->id))
		return False;
	if(!prs_uint32("perms        ", ps, depth, &fl3->perms))
		return False;
	if(!prs_uint32("num_locks    ", ps, depth, &fl3->num_locks))
		return False;
	if(!prs_uint32("ptr_path_name", ps, depth, &fl3->ptr_path_name))
		return False;
	if(!prs_uint32("ptr_user_name", ps, depth, &fl3->ptr_user_name))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_file_info_3(char *desc, SRV_FILE_INFO_3 *fl3, prs_struct *ps, int depth)
{
	if (fl3 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_file_3_fl3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("num_entries_read", ps, depth, &fl3->num_entries_read))
		return False;
	if(!prs_uint32("ptr_file_fl3", ps, depth, &fl3->ptr_file_info))
		return False;

	if (fl3->ptr_file_info != 0) {
		int i;
		int num_entries = fl3->num_entries_read;

		if (num_entries > MAX_FILE_ENTRIES) {
			num_entries = MAX_FILE_ENTRIES; /* report this! */
		}

		if(!prs_uint32("num_entries_read2", ps, depth, &fl3->num_entries_read2))
			return False;

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_file_info3("", &fl3->info_3[i], ps, depth))
				return False;
		}

		for (i = 0; i < num_entries; i++) {
			if(!srv_io_file_info3_str("", &fl3->info_3_str[i], ps, depth))
				return False;
		}

		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL srv_io_srv_file_ctr(char *desc, SRV_FILE_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_srv_file_ctr");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value", ps, depth, &ctr->switch_value))
		return False;
	if(!prs_uint32("ptr_file_ctr", ps, depth, &ctr->ptr_file_ctr))
		return False;

	if (ctr->ptr_file_ctr != 0) {
		switch (ctr->switch_value) {
		case 3:
			if(!srv_io_srv_file_info_3("", &ctr->file.info3, ps, depth))
				return False;
			break;
		default:
			DEBUG(5,("%s no file info at switch_value %d\n",
			         tab_depth(depth), ctr->switch_value));
			break;
		}
	}

	return True;
}

/*******************************************************************
 Inits a SRV_Q_NET_FILE_ENUM structure.
********************************************************************/

void init_srv_q_net_file_enum(SRV_Q_NET_FILE_ENUM *q_n, 
				char *srv_name, char *qual_name,
				uint32 file_level, SRV_FILE_INFO_CTR *ctr,
				uint32 preferred_len,
				ENUM_HND *hnd)
{
	DEBUG(5,("init_q_net_file_enum\n"));

	q_n->ctr = ctr;

	init_buf_unistr2(&q_n->uni_srv_name, &q_n->ptr_srv_name, srv_name);
	init_buf_unistr2(&q_n->uni_qual_name, &q_n->ptr_qual_name, qual_name);

	q_n->file_level    = file_level;
	q_n->preferred_len = preferred_len;

	memcpy(&q_n->enum_hnd, hnd, sizeof(*hnd));
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_file_enum(char *desc, SRV_Q_NET_FILE_ENUM *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_file_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_qual_name", ps, depth, &q_n->ptr_qual_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_qual_name, q_n->ptr_qual_name, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("file_level", ps, depth, &q_n->file_level))
		return False;

	if (q_n->file_level != -1) {
		if(!srv_io_srv_file_ctr("file_ctr", q_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("preferred_len", ps, depth, &q_n->preferred_len))
		return False;

	if(!smb_io_enum_hnd("enum_hnd", &q_n->enum_hnd, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_r_net_file_enum(char *desc, SRV_R_NET_FILE_ENUM *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_file_enum");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("file_level", ps, depth, &r_n->file_level))
		return False;

	if (r_n->file_level != 0) {
		if(!srv_io_srv_file_ctr("file_ctr", r_n->ctr, ps, depth))
			return False;
	}

	if(!prs_uint32("total_entries", ps, depth, &r_n->total_entries))
		return False;
	if(!smb_io_enum_hnd("enum_hnd", &r_n->enum_hnd, ps, depth))
		return False;
	if(!prs_uint32("status     ", ps, depth, &r_n->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a SRV_INFO_101 structure.
 ********************************************************************/

void init_srv_info_101(SRV_INFO_101 *sv101, uint32 platform_id, char *name,
				uint32 ver_major, uint32 ver_minor,
				uint32 srv_type, char *comment)
{
	DEBUG(5,("init_srv_info_101\n"));

	sv101->platform_id  = platform_id;
	init_buf_unistr2(&sv101->uni_name, &sv101->ptr_name, name);
	sv101->ver_major    = ver_major;
	sv101->ver_minor    = ver_minor;
	sv101->srv_type     = srv_type;
	init_buf_unistr2(&sv101->uni_comment, &sv101->ptr_comment, comment);
}

/*******************************************************************
 Reads or writes a SRV_INFO_101 structure.
 ********************************************************************/

static BOOL srv_io_info_101(char *desc, SRV_INFO_101 *sv101, prs_struct *ps, int depth)
{
	if (sv101 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_info_101");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("platform_id ", ps, depth, &sv101->platform_id))
		return False;
	if(!prs_uint32("ptr_name    ", ps, depth, &sv101->ptr_name))
		return False;
	if(!prs_uint32("ver_major   ", ps, depth, &sv101->ver_major))
		return False;
	if(!prs_uint32("ver_minor   ", ps, depth, &sv101->ver_minor))
		return False;
	if(!prs_uint32("srv_type    ", ps, depth, &sv101->srv_type))
		return False;
	if(!prs_uint32("ptr_comment ", ps, depth, &sv101->ptr_comment))
		return False;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unistr2("uni_name    ", &sv101->uni_name, True, ps, depth))
		return False;
	if(!smb_io_unistr2("uni_comment ", &sv101->uni_comment, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a SRV_INFO_102 structure.
 ********************************************************************/

void init_srv_info_102(SRV_INFO_102 *sv102, uint32 platform_id, char *name,
				char *comment, uint32 ver_major, uint32 ver_minor,
				uint32 srv_type, uint32 users, uint32 disc, uint32 hidden,
				uint32 announce, uint32 ann_delta, uint32 licenses,
				char *usr_path)
{
	DEBUG(5,("init_srv_info_102\n"));

	sv102->platform_id  = platform_id;
	init_buf_unistr2(&sv102->uni_name, &sv102->ptr_name, name);
	sv102->ver_major    = ver_major;
	sv102->ver_minor    = ver_minor;
	sv102->srv_type     = srv_type;
	init_buf_unistr2(&sv102->uni_comment, &sv102->ptr_comment, comment);

	/* same as 101 up to here */

	sv102->users        = users;
	sv102->disc         = disc;
	sv102->hidden       = hidden;
	sv102->announce     = announce;
	sv102->ann_delta    =ann_delta;
	sv102->licenses     = licenses;
	init_buf_unistr2(&sv102->uni_usr_path, &sv102->ptr_usr_path, usr_path);
}


/*******************************************************************
 Reads or writes a SRV_INFO_102 structure.
 ********************************************************************/

static BOOL srv_io_info_102(char *desc, SRV_INFO_102 *sv102, prs_struct *ps, int depth)
{
	if (sv102 == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_info102");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("platform_id ", ps, depth, &sv102->platform_id))
		return False;
	if(!prs_uint32("ptr_name    ", ps, depth, &sv102->ptr_name))
		return False;
	if(!prs_uint32("ver_major   ", ps, depth, &sv102->ver_major))
		return False;
	if(!prs_uint32("ver_minor   ", ps, depth, &sv102->ver_minor))
		return False;
	if(!prs_uint32("srv_type    ", ps, depth, &sv102->srv_type))
		return False;
	if(!prs_uint32("ptr_comment ", ps, depth, &sv102->ptr_comment))
		return False;

	/* same as 101 up to here */

	if(!prs_uint32("users       ", ps, depth, &sv102->users))
		return False;
	if(!prs_uint32("disc        ", ps, depth, &sv102->disc))
		return False;
	if(!prs_uint32("hidden      ", ps, depth, &sv102->hidden))
		return False;
	if(!prs_uint32("announce    ", ps, depth, &sv102->announce))
		return False;
	if(!prs_uint32("ann_delta   ", ps, depth, &sv102->ann_delta))
		return False;
	if(!prs_uint32("licenses    ", ps, depth, &sv102->licenses))
		return False;
	if(!prs_uint32("ptr_usr_path", ps, depth, &sv102->ptr_usr_path))
		return False;

	if(!smb_io_unistr2("uni_name    ", &sv102->uni_name, True, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;
	if(!smb_io_unistr2("uni_comment ", &sv102->uni_comment, True, ps, depth))
		return False;
	if(!prs_align(ps))
		return False;
	if(!smb_io_unistr2("uni_usr_path", &sv102->uni_usr_path, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a SRV_INFO_102 structure.
 ********************************************************************/

static BOOL srv_io_info_ctr(char *desc, SRV_INFO_CTR *ctr, prs_struct *ps, int depth)
{
	if (ctr == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_info_ctr");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value", ps, depth, &ctr->switch_value))
		return False;
	if(!prs_uint32("ptr_srv_ctr ", ps, depth, &ctr->ptr_srv_ctr))
		return False;

	if (ctr->ptr_srv_ctr != 0 && ctr->switch_value != 0 && ctr != NULL) {
		switch (ctr->switch_value) {
		case 101:
			if(!srv_io_info_101("sv101", &ctr->srv.sv101, ps, depth))
				return False;
			break;
		case 102:
			if(!srv_io_info_102("sv102", &ctr->srv.sv102, ps, depth))
				return False;
			break;
		default:
			DEBUG(5,("%s no server info at switch_value %d\n",
					 tab_depth(depth), ctr->switch_value));
			break;
		}
		if(!prs_align(ps))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a SRV_Q_NET_SRV_GET_INFO structure.
 ********************************************************************/

void init_srv_q_net_srv_get_info(SRV_Q_NET_SRV_GET_INFO *srv,
				char *server_name, uint32 switch_value)
{
	DEBUG(5,("init_srv_q_net_srv_get_info\n"));

	init_buf_unistr2(&srv->uni_srv_name, &srv->ptr_srv_name, server_name);

	srv->switch_value = switch_value;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL srv_io_q_net_srv_get_info(char *desc, SRV_Q_NET_SRV_GET_INFO *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_srv_get_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name  ", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("switch_value  ", ps, depth, &q_n->switch_value))
		return False;

	return True;
}

/*******************************************************************
 Inits a SRV_R_NET_SRV_GET_INFO structure.
 ********************************************************************/

void init_srv_r_net_srv_get_info(SRV_R_NET_SRV_GET_INFO *srv,
				uint32 switch_value, SRV_INFO_CTR *ctr, uint32 status)
{
	DEBUG(5,("init_srv_r_net_srv_get_info\n"));

	srv->ctr = ctr;

	if (status == 0x0) {
		srv->ctr->switch_value = switch_value;
		srv->ctr->ptr_srv_ctr  = 1;
	} else {
		srv->ctr->switch_value = 0;
		srv->ctr->ptr_srv_ctr  = 0;
	}

	srv->status = status;
}

/*******************************************************************
 Reads or writes a structure.
 ********************************************************************/

BOOL srv_io_r_net_srv_get_info(char *desc, SRV_R_NET_SRV_GET_INFO *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_srv_get_info");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!srv_io_info_ctr("ctr", r_n->ctr, ps, depth))
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_n->status))
		return False;

	return True;
}


/*******************************************************************
 Reads or writes a structure.
 ********************************************************************/

BOOL srv_io_q_net_remote_tod(char *desc, SRV_Q_NET_REMOTE_TOD *q_n, prs_struct *ps, int depth)
{
	if (q_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_q_net_remote_tod");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_srv_name  ", ps, depth, &q_n->ptr_srv_name))
		return False;
	if(!smb_io_unistr2("", &q_n->uni_srv_name, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a TIME_OF_DAY_INFO structure.
 ********************************************************************/

static BOOL srv_io_time_of_day_info(char *desc, TIME_OF_DAY_INFO *tod, prs_struct *ps, int depth)
{
	if (tod == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_time_of_day_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("elapsedt   ", ps, depth, &tod->elapsedt))
		return False;
	if(!prs_uint32("msecs      ", ps, depth, &tod->msecs))
		return False;
	if(!prs_uint32("hours      ", ps, depth, &tod->hours))
		return False;
	if(!prs_uint32("mins       ", ps, depth, &tod->mins))
		return False;
	if(!prs_uint32("secs       ", ps, depth, &tod->secs))
		return False;
	if(!prs_uint32("hunds      ", ps, depth, &tod->hunds))
		return False;
	if(!prs_uint32("timezone   ", ps, depth, &tod->zone))
		return False;
	if(!prs_uint32("tintervals ", ps, depth, &tod->tintervals))
		return False;
	if(!prs_uint32("day        ", ps, depth, &tod->day))
		return False;
	if(!prs_uint32("month      ", ps, depth, &tod->month))
		return False;
	if(!prs_uint32("year       ", ps, depth, &tod->year))
		return False;
	if(!prs_uint32("weekday    ", ps, depth, &tod->weekday))
		return False;

	return True;
}

/*******************************************************************
 Inits a TIME_OF_DAY_INFO structure.
 ********************************************************************/

void init_time_of_day_info(TIME_OF_DAY_INFO *tod, uint32 elapsedt, uint32 msecs,
                           uint32 hours, uint32 mins, uint32 secs, uint32 hunds,
			   uint32 zone, uint32 tintervals, uint32 day,
			   uint32 month, uint32 year, uint32 weekday)
{
	DEBUG(5,("init_time_of_day_info\n"));

	tod->elapsedt	= elapsedt;
	tod->msecs	= msecs;
	tod->hours	= hours;
	tod->mins	= mins;
	tod->secs	= secs;
	tod->hunds	= hunds;
	tod->zone	= zone;
	tod->tintervals	= tintervals;
	tod->day	= day;
	tod->month	= month;
	tod->year	= year;
	tod->weekday	= weekday;
}


/*******************************************************************
 Reads or writes a structure.
 ********************************************************************/

BOOL srv_io_r_net_remote_tod(char *desc, SRV_R_NET_REMOTE_TOD *r_n, prs_struct *ps, int depth)
{
	if (r_n == NULL)
		return False;

	prs_debug(ps, depth, desc, "srv_io_r_net_remote_tod");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_srv_tod ", ps, depth, &r_n->ptr_srv_tod))
		return False;

	if(!srv_io_time_of_day_info("tod", r_n->tod, ps, depth))
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_n->status))
		return False;

	return True;
}
