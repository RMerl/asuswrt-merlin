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

static BOOL lsa_io_trans_names(char *desc, LSA_TRANS_NAME_ENUM *trn, prs_struct *ps, int depth);

/*******************************************************************
 Inits a LSA_TRANS_NAME structure.
********************************************************************/

void init_lsa_trans_name(LSA_TRANS_NAME *trn, UNISTR2 *uni_name,
			uint32 sid_name_use, char *name, uint32 idx)
{
	int len_name = strlen(name);

	if(len_name == 0)
		len_name = 1;

	trn->sid_name_use = sid_name_use;
	init_uni_hdr(&trn->hdr_name, len_name);
	init_unistr2(uni_name, name, len_name);
	trn->domain_idx = idx;
}

/*******************************************************************
 Reads or writes a LSA_TRANS_NAME structure.
********************************************************************/

static BOOL lsa_io_trans_name(char *desc, LSA_TRANS_NAME *trn, prs_struct *ps, int depth)
{
	if (trn == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_trans_name");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("sid_name_use", ps, depth, &trn->sid_name_use))
		return False;
	if(!smb_io_unihdr ("hdr_name", &trn->hdr_name, ps, depth))
		return False;
	if(!prs_uint32("domain_idx  ", ps, depth, &trn->domain_idx))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a DOM_R_REF structure.
********************************************************************/

static BOOL lsa_io_dom_r_ref(char *desc, DOM_R_REF *r_r, prs_struct *ps, int depth)
{
	int i, s, n;

	prs_debug(ps, depth, desc, "lsa_io_dom_r_ref");
	depth++;

	if (r_r == NULL)
		return False;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("num_ref_doms_1", ps, depth, &r_r->num_ref_doms_1)) /* num referenced domains? */
		return False;
	if(!prs_uint32("ptr_ref_dom   ", ps, depth, &r_r->ptr_ref_dom)) /* undocumented buffer pointer. */
		return False;
	if(!prs_uint32("max_entries   ", ps, depth, &r_r->max_entries)) /* 32 - max number of entries */
		return False;

	SMB_ASSERT_ARRAY(r_r->hdr_ref_dom, r_r->num_ref_doms_1);

	if (r_r->ptr_ref_dom != 0) {
		if(!prs_uint32("num_ref_doms_2", ps, depth, &r_r->num_ref_doms_2)) /* 4 - num referenced domains? */
			return False;

		SMB_ASSERT_ARRAY(r_r->ref_dom, r_r->num_ref_doms_2);

		for (i = 0; i < r_r->num_ref_doms_1; i++) {
			fstring t;

			slprintf(t, sizeof(t) - 1, "dom_ref[%d] ", i);
			if(!smb_io_unihdr(t, &r_r->hdr_ref_dom[i].hdr_dom_name, ps, depth))
				return False;

			slprintf(t, sizeof(t) - 1, "sid_ptr[%d] ", i);
			if(!prs_uint32(t, ps, depth, &r_r->hdr_ref_dom[i].ptr_dom_sid))
				return False;
		}

		for (i = 0, n = 0, s = 0; i < r_r->num_ref_doms_2; i++) {
			fstring t;

			if (r_r->hdr_ref_dom[i].hdr_dom_name.buffer != 0) {
				slprintf(t, sizeof(t) - 1, "dom_ref[%d] ", i);
				if(!smb_io_unistr2(t, &r_r->ref_dom[n].uni_dom_name, True, ps, depth)) /* domain name unicode string */
					return False;
				n++;
			}

			if (r_r->hdr_ref_dom[i].ptr_dom_sid != 0) {
				slprintf(t, sizeof(t) - 1, "sid_ptr[%d] ", i);
				if(!smb_io_dom_sid2("", &r_r->ref_dom[s].ref_dom, ps, depth)) /* referenced domain SIDs */
					return False;
				s++;
			}
		}
	}

	return True;
}

/*******************************************************************
 Inits an LSA_SEC_QOS structure.
********************************************************************/

void init_lsa_sec_qos(LSA_SEC_QOS *qos, uint16 imp_lev, uint8 ctxt, uint8 eff,
				uint32 unknown)
{
	DEBUG(5,("init_lsa_sec_qos\n"));

	qos->len = 0x0c; /* length of quality of service block, in bytes */
	qos->sec_imp_level = imp_lev;
	qos->sec_ctxt_mode = ctxt;
	qos->effective_only = eff;
	qos->unknown = unknown;
}

/*******************************************************************
 Reads or writes an LSA_SEC_QOS structure.
********************************************************************/

static BOOL lsa_io_sec_qos(char *desc,  LSA_SEC_QOS *qos, prs_struct *ps, int depth)
{
	uint32 start;

	if (qos == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_obj_qos");
	depth++;

	if(!prs_align(ps))
		return False;
	
	start = prs_offset(ps);

	/* these pointers had _better_ be zero, because we don't know
	   what they point to!
	 */
	if(!prs_uint32("len           ", ps, depth, &qos->len)) /* 0x18 - length (in bytes) inc. the length field. */
		return False;
	if(!prs_uint16("sec_imp_level ", ps, depth, &qos->sec_imp_level ))
		return False;
	if(!prs_uint8 ("sec_ctxt_mode ", ps, depth, &qos->sec_ctxt_mode ))
		return False;
	if(!prs_uint8 ("effective_only", ps, depth, &qos->effective_only))
		return False;
	if(!prs_uint32("unknown       ", ps, depth, &qos->unknown))
		return False;

	if (qos->len != prs_offset(ps) - start) {
		DEBUG(3,("lsa_io_sec_qos: length %x does not match size %x\n",
		         qos->len, prs_offset(ps) - start));
	}

	return True;
}


/*******************************************************************
 Inits an LSA_OBJ_ATTR structure.
********************************************************************/

void init_lsa_obj_attr(LSA_OBJ_ATTR *attr, uint32 attributes, LSA_SEC_QOS *qos)
{
	DEBUG(5,("make_lsa_obj_attr\n"));

	attr->len = 0x18; /* length of object attribute block, in bytes */
	attr->ptr_root_dir = 0;
	attr->ptr_obj_name = 0;
	attr->attributes = attributes;
	attr->ptr_sec_desc = 0;
	
	if (qos != NULL) {
		attr->ptr_sec_qos = 1;
		attr->sec_qos = qos;
	} else {
		attr->ptr_sec_qos = 0;
		attr->sec_qos = NULL;
	}
}

/*******************************************************************
 Reads or writes an LSA_OBJ_ATTR structure.
********************************************************************/

static BOOL lsa_io_obj_attr(char *desc, LSA_OBJ_ATTR *attr, prs_struct *ps, int depth)
{
	uint32 start;

	if (attr == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_obj_attr");
	depth++;

	if(!prs_align(ps))
		return False;
	
	start = prs_offset(ps);

	/* these pointers had _better_ be zero, because we don't know
	   what they point to!
	 */
	if(!prs_uint32("len         ", ps, depth, &attr->len)) /* 0x18 - length (in bytes) inc. the length field. */
		return False;
	if(!prs_uint32("ptr_root_dir", ps, depth, &attr->ptr_root_dir)) /* 0 - root directory (pointer) */
		return False;
	if(!prs_uint32("ptr_obj_name", ps, depth, &attr->ptr_obj_name)) /* 0 - object name (pointer) */
		return False;
	if(!prs_uint32("attributes  ", ps, depth, &attr->attributes)) /* 0 - attributes (undocumented) */
		return False;
	if(!prs_uint32("ptr_sec_desc", ps, depth, &attr->ptr_sec_desc)) /* 0 - security descriptior (pointer) */
		return False;
	if(!prs_uint32("ptr_sec_qos ", ps, depth, &attr->ptr_sec_qos )) /* security quality of service (pointer) */
		return False;

	if (attr->len != prs_offset(ps) - start) {
		DEBUG(3,("lsa_io_obj_attr: length %x does not match size %x\n",
		         attr->len, prs_offset(ps) - start));
	}

	if (attr->ptr_sec_qos != 0 && attr->sec_qos != NULL) {
		if(!lsa_io_sec_qos("sec_qos", attr->sec_qos, ps, depth))
			return False;
	}

	return True;
}


/*******************************************************************
 Inits an LSA_Q_OPEN_POL structure.
********************************************************************/

void init_q_open_pol(LSA_Q_OPEN_POL *r_q, uint16 system_name,
			uint32 attributes,
			uint32 desired_access,
			LSA_SEC_QOS *qos)
{
	DEBUG(5,("make_open_pol: attr:%d da:%d\n", attributes, desired_access));

	r_q->ptr = 1; /* undocumented pointer */

	if (qos == NULL)
		r_q->des_access = desired_access;

	r_q->system_name = system_name;
	init_lsa_obj_attr(&r_q->attr, attributes, qos);
}

/*******************************************************************
 Reads or writes an LSA_Q_OPEN_POL structure.
********************************************************************/

BOOL lsa_io_q_open_pol(char *desc, LSA_Q_OPEN_POL *r_q, prs_struct *ps, int depth)
{
	if (r_q == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_open_pol");
	depth++;

	if(!prs_uint32("ptr       ", ps, depth, &r_q->ptr))
		return False;
	if(!prs_uint16("system_name", ps, depth, &r_q->system_name))
		return False;
	if(!prs_align( ps ))
		return False;

	if(!lsa_io_obj_attr("", &r_q->attr, ps, depth))
		return False;

	if (r_q->attr.ptr_sec_qos == 0) {
		if(!prs_uint32("des_access", ps, depth, &r_q->des_access))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes an LSA_R_OPEN_POL structure.
********************************************************************/

BOOL lsa_io_r_open_pol(char *desc, LSA_R_OPEN_POL *r_p, prs_struct *ps, int depth)
{
	if (r_p == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_open_pol");
	depth++;

	if(!smb_io_pol_hnd("", &r_p->pol, ps, depth))
		return False;

	if(!prs_uint32("status", ps, depth, &r_p->status))
		return False;

	return True;
}

/*******************************************************************
 Inits an LSA_Q_OPEN_POL2 structure.
********************************************************************/

void init_q_open_pol2(LSA_Q_OPEN_POL2 *r_q, char *server_name,
			uint32 attributes,
			uint32 desired_access,
			LSA_SEC_QOS *qos)
{
	DEBUG(5,("make_open_pol2: attr:%d da:%d\n", attributes, desired_access));

	r_q->ptr = 1; /* undocumented pointer */

	if (qos == NULL)
		r_q->des_access = desired_access;

	init_unistr2(&r_q->uni_server_name, server_name, strlen(server_name));
	init_lsa_obj_attr(&r_q->attr, attributes, qos);
}

/*******************************************************************
 Reads or writes an LSA_Q_OPEN_POL2 structure.
********************************************************************/

BOOL lsa_io_q_open_pol2(char *desc, LSA_Q_OPEN_POL2 *r_q, prs_struct *ps, int depth)
{
	if (r_q == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_open_pol2");
	depth++;

	if(!prs_uint32("ptr       ", ps, depth, &r_q->ptr))
		return False;

	if(!smb_io_unistr2 ("", &r_q->uni_server_name, r_q->ptr, ps, depth))
		return False;
	if(!lsa_io_obj_attr("", &r_q->attr, ps, depth))
		return False;

	if (r_q->attr.ptr_sec_qos == 0) {
		if(!prs_uint32("des_access", ps, depth, &r_q->des_access))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes an LSA_R_OPEN_POL2 structure.
********************************************************************/

BOOL lsa_io_r_open_pol2(char *desc, LSA_R_OPEN_POL2 *r_p, prs_struct *ps, int depth)
{
	if (r_p == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_open_pol2");
	depth++;

	if(!smb_io_pol_hnd("", &r_p->pol, ps, depth))
		return False;

	if(!prs_uint32("status", ps, depth, &r_p->status))
		return False;

	return True;
}

/*******************************************************************
 Inits an LSA_Q_QUERY_INFO structure.
********************************************************************/

void init_q_query(LSA_Q_QUERY_INFO *q_q, POLICY_HND *hnd, uint16 info_class)
{
	DEBUG(5,("make_q_query\n"));

	memcpy(&q_q->pol, hnd, sizeof(q_q->pol));

	q_q->info_class = info_class;
}

/*******************************************************************
 Reads or writes an LSA_Q_QUERY_INFO structure.
********************************************************************/

BOOL lsa_io_q_query(char *desc, LSA_Q_QUERY_INFO *q_q, prs_struct *ps, int depth)
{
	if (q_q == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_query");
	depth++;

	if(!smb_io_pol_hnd("", &q_q->pol, ps, depth))
		return False;

	if(!prs_uint16("info_class", ps, depth, &q_q->info_class))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an LSA_Q_ENUM_TRUST_DOM structure.
********************************************************************/

BOOL lsa_io_q_enum_trust_dom(char *desc, LSA_Q_ENUM_TRUST_DOM *q_e, prs_struct *ps, int depth)
{
	if (q_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_enum_trust_dom");
	depth++;


	if(!smb_io_pol_hnd("", &q_e->pol, ps, depth))
		return False;

	if(!prs_uint32("enum_context ", ps, depth, &q_e->enum_context))
		return False;
	if(!prs_uint32("preferred_len", ps, depth, &q_e->preferred_len))
		return False;

	return True;
}

/*******************************************************************
 Inits an LSA_R_ENUM_TRUST_DOM structure.
********************************************************************/

void init_r_enum_trust_dom(LSA_R_ENUM_TRUST_DOM *r_e,
                           uint32 enum_context, char *domain_name, DOM_SID *domain_sid,
                           uint32 status)
{
	DEBUG(5,("make_r_enum_trust_dom\n"));

	r_e->enum_context = enum_context;

	if (status == 0) {
		int len_domain_name = strlen(domain_name);

		r_e->num_domains  = 1;
		r_e->ptr_enum_domains = 1;
		r_e->num_domains2 = 1;

		init_uni_hdr2(&r_e->hdr_domain_name, len_domain_name);
		init_unistr2 (&r_e->uni_domain_name, domain_name, len_domain_name);
		init_dom_sid2(&r_e->other_domain_sid, domain_sid);
	} else {
		r_e->num_domains = 0;
		r_e->ptr_enum_domains = 0;
	}

	r_e->status = status;
}

/*******************************************************************
 Reads or writes an LSA_R_ENUM_TRUST_DOM structure.
********************************************************************/

BOOL lsa_io_r_enum_trust_dom(char *desc,  LSA_R_ENUM_TRUST_DOM *r_e, prs_struct *ps, int depth)
{
	if (r_e == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_enum_trust_dom");
	depth++;

	if(!prs_uint32("enum_context    ", ps, depth, &r_e->enum_context))
		return False;
	if(!prs_uint32("num_domains     ", ps, depth, &r_e->num_domains))
		return False;
	if(!prs_uint32("ptr_enum_domains", ps, depth, &r_e->ptr_enum_domains))
		return False;

	if (r_e->ptr_enum_domains != 0) {
		if(!prs_uint32("num_domains2", ps, depth, &r_e->num_domains2))
			return False;
		if(!smb_io_unihdr2 ("", &r_e->hdr_domain_name, ps, depth))
			return False;
		if(!smb_io_unistr2 ("", &r_e->uni_domain_name, r_e->hdr_domain_name.buffer, ps, depth))
			return False;
		if(!smb_io_dom_sid2("", &r_e->other_domain_sid, ps, depth))
			return False;
	}

	if(!prs_uint32("status", ps, depth, &r_e->status))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an LSA_Q_QUERY_INFO structure.
********************************************************************/

BOOL lsa_io_r_query(char *desc, LSA_R_QUERY_INFO *r_q, prs_struct *ps, int depth)
{
	if (r_q == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_query");
	depth++;

	if(!prs_uint32("undoc_buffer", ps, depth, &r_q->undoc_buffer))
		return False;

	if (r_q->undoc_buffer != 0) {
		if(!prs_uint16("info_class", ps, depth, &r_q->info_class))
			return False;

		switch (r_q->info_class) {
		case 3:
			if(!smb_io_dom_query_3("", &r_q->dom.id3, ps, depth))
				return False;
			break;
		case 5:
			if(!smb_io_dom_query_5("", &r_q->dom.id3, ps, depth))
				return False;
			break;
		default:
			/* PANIC! */
			break;
		}
	}

	if(!prs_uint32("status", ps, depth, &r_q->status))
		return False;

	return True;
}

/*******************************************************************
 Inits a LSA_SID_ENUM structure.
********************************************************************/

void init_lsa_sid_enum(LSA_SID_ENUM *sen, int num_entries, DOM_SID **sids)
{
	int i, i2;

	DEBUG(5,("make_lsa_sid_enum\n"));

	sen->num_entries  = num_entries;
	sen->ptr_sid_enum = (num_entries != 0) ? 1 : 0;
	sen->num_entries2 = num_entries;

	SMB_ASSERT_ARRAY(sen->sid, sen->num_entries);

	for (i = 0, i2 = 0; i < num_entries; i++) {
		if (sids[i] != NULL) {
			sen->ptr_sid[i] = 1;
			init_dom_sid2(&sen->sid[i2], sids[i]);
			i2++;
		} else {
			sen->ptr_sid[i] = 0;
		}
	}
}

/*******************************************************************
 Reads or writes a LSA_SID_ENUM structure.
********************************************************************/

static BOOL lsa_io_sid_enum(char *desc, LSA_SID_ENUM *sen,
				prs_struct *ps, int depth)
{
	int i;

	if (sen == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_sid_enum");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("num_entries ", ps, depth, &sen->num_entries))
		return False;
	if(!prs_uint32("ptr_sid_enum", ps, depth, &sen->ptr_sid_enum))
		return False;
	if(!prs_uint32("num_entries2", ps, depth, &sen->num_entries2))
		return False;

	SMB_ASSERT_ARRAY(sen->ptr_sid, sen->num_entries);

	for (i = 0; i < sen->num_entries; i++) {	
		fstring temp;
		slprintf(temp, sizeof(temp) - 1, "ptr_sid[%d]", i);
		if(!prs_uint32(temp, ps, depth, &sen->ptr_sid[i])) /* domain SID pointers to be looked up. */
			return False;
	}

	SMB_ASSERT_ARRAY(sen->sid, sen->num_entries);

	for (i = 0; i < sen->num_entries; i++) {
		fstring temp;
		slprintf(temp, sizeof(temp) - 1, "sid[%d]", i);
		if(!smb_io_dom_sid2(temp, &sen->sid[i], ps, depth)) /* domain SIDs to be looked up. */
			return False;
	}

	return True;
}

/*******************************************************************
 Inits an LSA_R_ENUM_TRUST_DOM structure.
********************************************************************/

void init_q_lookup_sids(LSA_Q_LOOKUP_SIDS *q_l, POLICY_HND *hnd,
				int num_sids, DOM_SID **sids,
				uint16 level)
{
	DEBUG(5,("make_r_enum_trust_dom\n"));

	memcpy(&q_l->pol, hnd, sizeof(q_l->pol));
	init_lsa_sid_enum(&q_l->sids, num_sids, sids);

	q_l->names.num_entries     = 0;
	q_l->names.ptr_trans_names = 0;
	q_l->names.num_entries2    = 0;

	q_l->level.value = level;
}

/*******************************************************************
 Reads or writes a LSA_Q_LOOKUP_SIDS structure.
********************************************************************/

BOOL lsa_io_q_lookup_sids(char *desc, LSA_Q_LOOKUP_SIDS *q_s, prs_struct *ps, int depth)
{
	if (q_s == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_lookup_sids");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_pol_hnd("pol_hnd", &q_s->pol, ps, depth)) /* policy handle */
		return False;
	if(!lsa_io_sid_enum("sids   ", &q_s->sids, ps, depth)) /* sids to be looked up */
		return False;
	if(!lsa_io_trans_names("names  ", &q_s->names, ps, depth)) /* translated names */
		return False;
	if(!smb_io_lookup_level("switch ", &q_s->level, ps, depth)) /* lookup level */
		return False;

	if(!prs_uint32("mapped_count", ps, depth, &q_s->mapped_count))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

static BOOL lsa_io_trans_names(char *desc, LSA_TRANS_NAME_ENUM *trn,
                prs_struct *ps, int depth)
{
	int i;

	if (trn == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_trans_names");
	depth++;

	if(!prs_align(ps))
		return False;
   
	if(!prs_uint32("num_entries    ", ps, depth, &trn->num_entries))
		return False;
	if(!prs_uint32("ptr_trans_names", ps, depth, &trn->ptr_trans_names))
		return False;

	if (trn->ptr_trans_names != 0) {
		if(!prs_uint32("num_entries2   ", ps, depth, &trn->num_entries2))
			return False;
		SMB_ASSERT_ARRAY(trn->name, trn->num_entries);

		for (i = 0; i < trn->num_entries2; i++) {
			fstring t;
			slprintf(t, sizeof(t) - 1, "name[%d] ", i);

			if(!lsa_io_trans_name(t, &trn->name[i], ps, depth)) /* translated name */
				return False;
		}

		for (i = 0; i < trn->num_entries2; i++) {
			fstring t;
			slprintf(t, sizeof(t) - 1, "name[%d] ", i);

			if(!smb_io_unistr2(t, &trn->uni_name[i], trn->name[i].hdr_name.buffer, ps, depth))
				return False;
			if(!prs_align(ps))
				return False;
		}
	}

	return True;
}

/*******************************************************************
 Reads or writes a structure.
********************************************************************/

BOOL lsa_io_r_lookup_sids(char *desc, LSA_R_LOOKUP_SIDS *r_s, prs_struct *ps, int depth)
{
	if (r_s == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_lookup_sids");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_dom_ref", ps, depth, &r_s->ptr_dom_ref))
		return False;

	if (r_s->ptr_dom_ref != 0)
		if(!lsa_io_dom_r_ref ("dom_ref", r_s->dom_ref, ps, depth)) /* domain reference info */
			return False;

	if(!lsa_io_trans_names("names  ", r_s->names, ps, depth)) /* translated names */
		return False;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("mapped_count", ps, depth, &r_s->mapped_count))
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_s->status))
		return False;

	return True;
}

/*******************************************************************
makes a structure.
********************************************************************/

void init_q_lookup_names(LSA_Q_LOOKUP_NAMES *q_l, POLICY_HND *hnd,
                int num_names, char **names)
{
	int i;

	DEBUG(5,("init_q_lookup_names\n"));

	memcpy(&q_l->pol, hnd, sizeof(q_l->pol));

	q_l->num_entries = num_names;
	q_l->num_entries2 = num_names;

	SMB_ASSERT_ARRAY(q_l->uni_name, q_l->num_entries);

	for (i = 0; i < num_names; i++) {
		char* name = names[i];
		int len = strlen(name);
		init_uni_hdr(&q_l->hdr_name[i], len);
		init_unistr2(&q_l->uni_name[i], name, len);
	}

	q_l->num_trans_entries  = 0;
	q_l->ptr_trans_sids  = 0;
	q_l->lookup_level = 1;
	q_l->mapped_count = 0;
}

/*******************************************************************
reads or writes a structure.
********************************************************************/

BOOL lsa_io_q_lookup_names(char *desc, LSA_Q_LOOKUP_NAMES *q_r, prs_struct *ps, int depth)
{
	int i;

	if (q_r == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_lookup_names");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_pol_hnd("", &q_r->pol, ps, depth)) /* policy handle */
		return False;

	if(!prs_uint32("num_entries    ", ps, depth, &q_r->num_entries))
		return False;
	if(!prs_uint32("num_entries2   ", ps, depth, &q_r->num_entries2))
		return False;

	SMB_ASSERT_ARRAY(q_r->uni_name, q_r->num_entries);

	for (i = 0; i < q_r->num_entries; i++) {
		if(!smb_io_unihdr("hdr_name", &q_r->hdr_name[i], ps, depth)) /* pointer names */
			return False;
	}

	for (i = 0; i < q_r->num_entries; i++) {
		if(!smb_io_unistr2("dom_name", &q_r->uni_name[i], q_r->hdr_name[i].buffer, ps, depth)) /* names to be looked up */
			return False;
		if(!prs_align(ps))
			return False;
	}

	if(!prs_uint32("num_trans_entries ", ps, depth, &q_r->num_trans_entries))
		return False;
	if(!prs_uint32("ptr_trans_sids ", ps, depth, &q_r->ptr_trans_sids))
		return False;
	if(!prs_uint32("lookup_level   ", ps, depth, &q_r->lookup_level))
		return False;
	if(!prs_uint32("mapped_count   ", ps, depth, &q_r->mapped_count))
		return False;

	return True;
}

/*******************************************************************
reads or writes a structure.
********************************************************************/

BOOL lsa_io_r_lookup_names(char *desc, LSA_R_LOOKUP_NAMES *r_r, prs_struct *ps, int depth)
{
	int i;

	if (r_r == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_lookup_names");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ptr_dom_ref", ps, depth, &r_r->ptr_dom_ref))
		return False;

	if (r_r->ptr_dom_ref != 0)
		if(!lsa_io_dom_r_ref("", r_r->dom_ref, ps, depth))
			return False;

	if(!prs_uint32("num_entries", ps, depth, &r_r->num_entries))
		return False;
	if(!prs_uint32("ptr_entries", ps, depth, &r_r->ptr_entries))
		return False;

	if (r_r->ptr_entries != 0) {
		if(!prs_uint32("num_entries2", ps, depth, &r_r->num_entries2))
			return False;

		if (r_r->num_entries2 != r_r->num_entries) {
			/* RPC fault */
			return False;
		}

		for (i = 0; i < r_r->num_entries2; i++)
			if(!smb_io_dom_rid2("", &r_r->dom_rid[i], ps, depth)) /* domain RIDs being looked up */
				return False;
	}

	if(!prs_uint32("mapped_count", ps, depth, &r_r->mapped_count))
		return False;

	if(!prs_uint32("status      ", ps, depth, &r_r->status))
		return False;

	return True;
}


/*******************************************************************
 Inits an LSA_Q_CLOSE structure.
********************************************************************/

void init_lsa_q_close(LSA_Q_CLOSE *q_c, POLICY_HND *hnd)
{
	DEBUG(5,("make_lsa_q_close\n"));

	memcpy(&q_c->pol, hnd, sizeof(q_c->pol));
}

/*******************************************************************
 Reads or writes an LSA_Q_CLOSE structure.
********************************************************************/

BOOL lsa_io_q_close(char *desc, LSA_Q_CLOSE *q_c, prs_struct *ps, int depth)
{
	if (q_c == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_q_close");
	depth++;

	if(!smb_io_pol_hnd("", &q_c->pol, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an LSA_R_CLOSE structure.
********************************************************************/

BOOL lsa_io_r_close(char *desc,  LSA_R_CLOSE *r_c, prs_struct *ps, int depth)
{
	if (r_c == NULL)
		return False;

	prs_debug(ps, depth, desc, "lsa_io_r_close");
	depth++;

	if(!smb_io_pol_hnd("", &r_c->pol, ps, depth))
		return False;

	if(!prs_uint32("status", ps, depth, &r_c->status))
		return False;

	return True;
}
