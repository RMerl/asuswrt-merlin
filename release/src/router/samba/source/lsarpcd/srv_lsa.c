
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison                    1998.
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
extern DOM_SID global_sam_sid;
extern fstring global_myworkgroup;
extern pstring global_myname;

/***************************************************************************
 lsa_reply_open_policy2
 ***************************************************************************/

static BOOL lsa_reply_open_policy2(prs_struct *rdata)
{
	int i;
	LSA_R_OPEN_POL2 r_o;

	ZERO_STRUCT(r_o);

	/* set up the LSA QUERY INFO response */

	for (i = 4; i < POL_HND_SIZE; i++)
		r_o.pol.data[i] = i;
	r_o.status = 0x0;

	/* store the response in the SMB stream */
	if(!lsa_io_r_open_pol2("", &r_o, rdata, 0)) {
		DEBUG(0,("lsa_reply_open_policy2: unable to marshall LSA_R_OPEN_POL2.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
lsa_reply_open_policy
 ***************************************************************************/

static BOOL lsa_reply_open_policy(prs_struct *rdata)
{
	int i;
	LSA_R_OPEN_POL r_o;

	ZERO_STRUCT(r_o);

	/* set up the LSA QUERY INFO response */

	for (i = 4; i < POL_HND_SIZE; i++)
		r_o.pol.data[i] = i;
	r_o.status = 0x0;

	/* store the response in the SMB stream */
	if(!lsa_io_r_open_pol("", &r_o, rdata, 0)) {
		DEBUG(0,("lsa_reply_open_policy: unable to marshall LSA_R_OPEN_POL.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
Init dom_query
 ***************************************************************************/

static void init_dom_query(DOM_QUERY *d_q, char *dom_name, DOM_SID *dom_sid)
{
	fstring sid_str;
	int domlen = strlen(dom_name);

	d_q->uni_dom_max_len = domlen * 2;
	d_q->uni_dom_str_len = domlen * 2;

	d_q->buffer_dom_name = domlen  != 0    ? 1 : 0; /* domain buffer pointer */
	d_q->buffer_dom_sid  = dom_sid != NULL ? 1 : 0; /* domain sid pointer */

	/* this string is supposed to be character short */
	init_unistr2(&d_q->uni_domain_name, dom_name, domlen);

	sid_to_string(sid_str, dom_sid);
	init_dom_sid2(&d_q->dom_sid, dom_sid);
}

/***************************************************************************
 lsa_reply_enum_trust_dom
 ***************************************************************************/

static void lsa_reply_enum_trust_dom(LSA_Q_ENUM_TRUST_DOM *q_e,
				prs_struct *rdata,
				uint32 enum_context, char *dom_name, DOM_SID *dom_sid)
{
	LSA_R_ENUM_TRUST_DOM r_e;

	ZERO_STRUCT(r_e);

	/* set up the LSA QUERY INFO response */
	init_r_enum_trust_dom(&r_e, enum_context, dom_name, dom_sid,
	      dom_name != NULL ? 0x0 : 0x80000000 | NT_STATUS_UNABLE_TO_FREE_VM);

	/* store the response in the SMB stream */
	lsa_io_r_enum_trust_dom("", &r_e, rdata, 0);
}

/***************************************************************************
lsa_reply_query_info
 ***************************************************************************/

static BOOL lsa_reply_query_info(LSA_Q_QUERY_INFO *q_q, prs_struct *rdata,
				char *dom_name, DOM_SID *dom_sid)
{
	LSA_R_QUERY_INFO r_q;

	ZERO_STRUCT(r_q);

	/* set up the LSA QUERY INFO response */

	r_q.undoc_buffer = 0x22000000; /* bizarre */
	r_q.info_class = q_q->info_class;

	init_dom_query(&r_q.dom.id5, dom_name, dom_sid);

	r_q.status = 0x0;

	/* store the response in the SMB stream */
	if(!lsa_io_r_query("", &r_q, rdata, 0)) {
		DEBUG(0,("lsa_reply_query_info: failed to marshall LSA_R_QUERY_INFO.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
 init_dom_ref - adds a domain if it's not already in, returns the index.
***************************************************************************/

static int init_dom_ref(DOM_R_REF *ref, char *dom_name, DOM_SID *dom_sid)
{
	int num = 0;
	int len;

	if (dom_name != NULL) {
		for (num = 0; num < ref->num_ref_doms_1; num++) {
			fstring domname;
			fstrcpy(domname, dos_unistr2_to_str(&ref->ref_dom[num].uni_dom_name));
			if (strequal(domname, dom_name))
				return num;
		}
	} else {
		num = ref->num_ref_doms_1;
	}

	if (num >= MAX_REF_DOMAINS) {
		/* index not found, already at maximum domain limit */
		return -1;
	}

	ref->num_ref_doms_1 = num+1;
	ref->ptr_ref_dom  = 1;
	ref->max_entries = MAX_REF_DOMAINS;
	ref->num_ref_doms_2 = num+1;

	len = (dom_name != NULL) ? strlen(dom_name) : 0;
	if(dom_name != NULL && len == 0)
		len = 1;

	init_uni_hdr(&ref->hdr_ref_dom[num].hdr_dom_name, len);
	ref->hdr_ref_dom[num].ptr_dom_sid = dom_sid != NULL ? 1 : 0;

	init_unistr2(&ref->ref_dom[num].uni_dom_name, dom_name, len);
	init_dom_sid2(&ref->ref_dom[num].ref_dom, dom_sid );

	return num;
}

/***************************************************************************
 init_lsa_rid2s
 ***************************************************************************/

static void init_lsa_rid2s(DOM_R_REF *ref, DOM_RID2 *rid2,
				int num_entries, UNISTR2 name[MAX_LOOKUP_SIDS],
				uint32 *mapped_count)
{
	int i;
	int total = 0;
	*mapped_count = 0;

	SMB_ASSERT(num_entries <= MAX_LOOKUP_SIDS);

	for (i = 0; i < num_entries; i++) {
		BOOL status = False;
		DOM_SID dom_sid;
		DOM_SID sid;
		uint32 rid = 0xffffffff;
		int dom_idx = -1;
		pstring full_name;
		fstring dom_name;
		fstring user;
		uint8 sid_name_use = SID_NAME_UNKNOWN;

		pstrcpy(full_name, dos_unistr2_to_str(&name[i]));

		/*
		 * Try and split the name into a DOMAIN and
		 * user component.
		 */

		split_domain_name(full_name, dom_name, user);

		/*
		 * We only do anything with this name if we
		 * can map the Domain into a SID we know.
		 */

		if (map_domain_name_to_sid(&dom_sid, dom_name)) {
			dom_idx = init_dom_ref(ref, dom_name, &dom_sid);

			if (lookup_local_name(dom_name, user, &sid, &sid_name_use) && sid_split_rid(&sid, &rid)) 
				status = True;
		}

		if (status)
			(*mapped_count)++;
		else {
			dom_idx = -1;
			rid = 0xffffffff;
			sid_name_use = SID_NAME_UNKNOWN;
		}

		init_dom_rid2(&rid2[total], rid, sid_name_use, dom_idx);
		total++;
	}
}

/***************************************************************************
 init_reply_lookup_names
 ***************************************************************************/

static void init_reply_lookup_names(LSA_R_LOOKUP_NAMES *r_l,
                DOM_R_REF *ref, uint32 num_entries,
                DOM_RID2 *rid2, uint32 mapped_count)
{
	r_l->ptr_dom_ref  = 1;
	r_l->dom_ref      = ref;

	r_l->num_entries  = num_entries;
	r_l->ptr_entries  = 1;
	r_l->num_entries2 = num_entries;
	r_l->dom_rid      = rid2;

	r_l->mapped_count = mapped_count;

	if (mapped_count == 0)
		r_l->status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	else
		r_l->status = 0x0;
}

/***************************************************************************
 Init lsa_trans_names.
 ***************************************************************************/

static void init_lsa_trans_names(DOM_R_REF *ref, LSA_TRANS_NAME_ENUM *trn,
				int num_entries, DOM_SID2 sid[MAX_LOOKUP_SIDS], uint32 *mapped_count)
{
	extern DOM_SID global_sid_S_1_5_0x20; /* BUILTIN sid. */
	int i;
	int total = 0;
	*mapped_count = 0;

	SMB_ASSERT(num_entries <= MAX_LOOKUP_SIDS);

	for (i = 0; i < num_entries; i++) {
		BOOL status = False;
		DOM_SID find_sid = sid[i].sid;
		uint32 rid = 0xffffffff;
		int dom_idx = -1;
		fstring name;
		fstring dom_name;
		uint8 sid_name_use = 0;

		memset(dom_name, '\0', sizeof(dom_name));
		memset(name, '\0', sizeof(name));

		/*
		 * First, check to see if the SID is one of the well
		 * known ones (this includes our own domain SID).
		 * Next, check if the domain prefix is one of the
		 * well known ones. If so and the domain prefix was
		 * either BUILTIN or our own global sid, then lookup
		 * the RID as a user or group id and translate to
		 * a name.
		 */

		if (map_domain_sid_to_name(&find_sid, dom_name)) {
			sid_name_use = SID_NAME_DOMAIN;
		} else if (sid_split_rid(&find_sid, &rid) && map_domain_sid_to_name(&find_sid, dom_name)) {
			if (sid_equal(&find_sid, &global_sam_sid) ||
				sid_equal(&find_sid, &global_sid_S_1_5_0x20)) {
				status = lookup_local_rid(rid, name, &sid_name_use);
			} else  {
				status = lookup_known_rid(&find_sid, rid, name, &sid_name_use);
			}
		}

		DEBUG(10,("init_lsa_trans_names: adding domain '%s' sid %s to referenced list.\n",
				dom_name, name ));

		dom_idx = init_dom_ref(ref, dom_name, &find_sid);

		if(!status) {
			slprintf(name, sizeof(name)-1, "unix.%08x", rid);
			sid_name_use = SID_NAME_UNKNOWN;
		}

		DEBUG(10,("init_lsa_trans_names: added user '%s\\%s' to referenced list.\n", dom_name, name ));

		(*mapped_count)++;

		init_lsa_trans_name(&trn->name[total], &trn->uni_name[total],
					sid_name_use, name, dom_idx);
		total++;
	}

	trn->num_entries = total;
	trn->ptr_trans_names = 1;
	trn->num_entries2 = total;
}

/***************************************************************************
 Init_reply_lookup_sids.
 ***************************************************************************/

static void init_reply_lookup_sids(LSA_R_LOOKUP_SIDS *r_l,
                DOM_R_REF *ref, LSA_TRANS_NAME_ENUM *names,
                uint32 mapped_count)
{
	r_l->ptr_dom_ref  = 1;
	r_l->dom_ref      = ref;
	r_l->names        = names;
	r_l->mapped_count = mapped_count;

	if (mapped_count == 0)
		r_l->status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	else
		r_l->status = 0x0;
}

/***************************************************************************
lsa_reply_lookup_sids
 ***************************************************************************/

static BOOL lsa_reply_lookup_sids(prs_struct *rdata, DOM_SID2 *sid, int num_entries)
{
	LSA_R_LOOKUP_SIDS r_l;
	DOM_R_REF ref;
	LSA_TRANS_NAME_ENUM names;
	uint32 mapped_count = 0;

	ZERO_STRUCT(r_l);
	ZERO_STRUCT(ref);
	ZERO_STRUCT(names);

	/* set up the LSA Lookup SIDs response */
	init_lsa_trans_names(&ref, &names, num_entries, sid, &mapped_count);
	init_reply_lookup_sids(&r_l, &ref, &names, mapped_count);

	/* store the response in the SMB stream */
	if(!lsa_io_r_lookup_sids("", &r_l, rdata, 0)) {
		DEBUG(0,("lsa_reply_lookup_sids: Failed to marshall LSA_R_LOOKUP_SIDS.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
lsa_reply_lookup_names
 ***************************************************************************/

static BOOL lsa_reply_lookup_names(prs_struct *rdata,
                UNISTR2 names[MAX_LOOKUP_SIDS], int num_entries)
{
	LSA_R_LOOKUP_NAMES r_l;
	DOM_R_REF ref;
	DOM_RID2 rids[MAX_LOOKUP_SIDS];
	uint32 mapped_count = 0;

	ZERO_STRUCT(r_l);
	ZERO_STRUCT(ref);
	ZERO_ARRAY(rids);

	/* set up the LSA Lookup RIDs response */
	init_lsa_rid2s(&ref, rids, num_entries, names, &mapped_count);
	init_reply_lookup_names(&r_l, &ref, num_entries, rids, mapped_count);

	/* store the response in the SMB stream */
	if(!lsa_io_r_lookup_names("", &r_l, rdata, 0)) {
		DEBUG(0,("lsa_reply_lookup_names: Failed to marshall LSA_R_LOOKUP_NAMES.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
 api_lsa_open_policy2
 ***************************************************************************/

static BOOL api_lsa_open_policy2( uint16 vuid, prs_struct *data,
                             prs_struct *rdata )
{
	LSA_Q_OPEN_POL2 q_o;

	ZERO_STRUCT(q_o);

	/* grab the server, object attributes and desired access flag...*/
	if(!lsa_io_q_open_pol2("", &q_o, data, 0)) {
		DEBUG(0,("api_lsa_open_policy2: unable to unmarshall LSA_Q_OPEN_POL2.\n"));
		return False;
	}

	/* lkclXXXX having decoded it, ignore all fields in the open policy! */

	/* return a 20 byte policy handle */
	if(!lsa_reply_open_policy2(rdata))
		return False;

	return True;
}

/***************************************************************************
api_lsa_open_policy
 ***************************************************************************/
static BOOL api_lsa_open_policy( uint16 vuid, prs_struct *data,
                             prs_struct *rdata )
{
	LSA_Q_OPEN_POL q_o;

	ZERO_STRUCT(q_o);

	/* grab the server, object attributes and desired access flag...*/
	if(!lsa_io_q_open_pol("", &q_o, data, 0)) {
		DEBUG(0,("api_lsa_open_policy: unable to unmarshall LSA_Q_OPEN_POL.\n"));
		return False;
	}

	/* lkclXXXX having decoded it, ignore all fields in the open policy! */

	/* return a 20 byte policy handle */
	if(!lsa_reply_open_policy(rdata))
		return False;

	return True;
}

/***************************************************************************
api_lsa_enum_trust_dom
 ***************************************************************************/
static BOOL api_lsa_enum_trust_dom( uint16 vuid, prs_struct *data,
                                    prs_struct *rdata )
{
	LSA_Q_ENUM_TRUST_DOM q_e;

	ZERO_STRUCT(q_e);

	/* grab the enum trust domain context etc. */
	lsa_io_q_enum_trust_dom("", &q_e, data, 0);

	/* construct reply.  return status is always 0x0 */
	lsa_reply_enum_trust_dom(&q_e, rdata, 0, NULL, NULL);

	return True;
}

/***************************************************************************
api_lsa_query_info
 ***************************************************************************/
static BOOL api_lsa_query_info( uint16 vuid, prs_struct *data,
                                prs_struct *rdata )
{
	LSA_Q_QUERY_INFO q_i;
	fstring name;
	DOM_SID *sid = NULL;
	memset(name, 0, sizeof(name));

	ZERO_STRUCT(q_i);

	/* grab the info class and policy handle */
	if(!lsa_io_q_query("", &q_i, data, 0)) {
		DEBUG(0,("api_lsa_query_info: failed to unmarshall LSA_Q_QUERY_INFO.\n"));
		return False;
	}

	switch (q_i.info_class) {
	case 0x03:
		fstrcpy(name, global_myworkgroup);
		sid = &global_sam_sid;
		break;
	case 0x05:
		fstrcpy(name, global_myname);
		sid = &global_sam_sid;
		break;
	default:
		DEBUG(0,("api_lsa_query_info: unknown info level in Lsa Query: %d\n", q_i.info_class));
		break;
	}

	/* construct reply.  return status is always 0x0 */
	if(!lsa_reply_query_info(&q_i, rdata, name, sid))
		return False;

	return True;
}

/***************************************************************************
 api_lsa_lookup_sids
 ***************************************************************************/

static BOOL api_lsa_lookup_sids( uint16 vuid, prs_struct *data, prs_struct *rdata )
{
	LSA_Q_LOOKUP_SIDS q_l;
	ZERO_STRUCT(q_l);

	/* grab the info class and policy handle */
	if(!lsa_io_q_lookup_sids("", &q_l, data, 0)) {
		DEBUG(0,("api_lsa_lookup_sids: failed to unmarshall LSA_Q_LOOKUP_SIDS.\n"));
		return False;
	}

	/* construct reply.  return status is always 0x0 */
	if(!lsa_reply_lookup_sids(rdata, q_l.sids.sid, q_l.sids.num_entries))
		return False;

	return True;
}

/***************************************************************************
 api_lsa_lookup_names
 ***************************************************************************/

static BOOL api_lsa_lookup_names( uint16 vuid, prs_struct *data, prs_struct *rdata )
{
	LSA_Q_LOOKUP_NAMES q_l;
	ZERO_STRUCT(q_l);

	/* grab the info class and policy handle */
	if(!lsa_io_q_lookup_names("", &q_l, data, 0)) {
		DEBUG(0,("api_lsa_lookup_names: failed to unmarshall LSA_Q_LOOKUP_NAMES.\n"));
		return False;
	}

	SMB_ASSERT_ARRAY(q_l.uni_name, q_l.num_entries);

	return lsa_reply_lookup_names(rdata, q_l.uni_name, q_l.num_entries);
}

/***************************************************************************
 api_lsa_close
 ***************************************************************************/
static BOOL api_lsa_close( uint16 vuid, prs_struct *data,
                                  prs_struct *rdata)
{
	LSA_R_CLOSE r_c;

	ZERO_STRUCT(r_c);

	/* store the response in the SMB stream */
	if (!lsa_io_r_close("", &r_c, rdata, 0)) {
		DEBUG(0,("api_lsa_close: lsa_io_r_close failed.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
 api_lsa_open_secret
 ***************************************************************************/
static BOOL api_lsa_open_secret( uint16 vuid, prs_struct *data,
                                  prs_struct *rdata)
{
	/* XXXX this is NOT good */
	size_t i;
	uint32 dummy = 0;

	for(i =0; i < 4; i++) {
		if(!prs_uint32("api_lsa_close", rdata, 1, &dummy)) {
			DEBUG(0,("api_lsa_open_secret: prs_uint32 %d failed.\n",
				(int)i ));
			return False;
		}
	}

	dummy = 0xC0000000 | NT_STATUS_OBJECT_NAME_NOT_FOUND;
	if(!prs_uint32("api_lsa_close", rdata, 1, &dummy)) {
		DEBUG(0,("api_lsa_open_secret: prs_uint32 status failed.\n"));
		return False;
	}

	return True;
}

/***************************************************************************
 \PIPE\ntlsa commands
 ***************************************************************************/
static struct api_struct api_lsa_cmds[] =
{
	{ "LSA_OPENPOLICY2"     , LSA_OPENPOLICY2     , api_lsa_open_policy2   },
	{ "LSA_OPENPOLICY"      , LSA_OPENPOLICY      , api_lsa_open_policy    },
	{ "LSA_QUERYINFOPOLICY" , LSA_QUERYINFOPOLICY , api_lsa_query_info     },
	{ "LSA_ENUMTRUSTDOM"    , LSA_ENUMTRUSTDOM    , api_lsa_enum_trust_dom },
	{ "LSA_CLOSE"           , LSA_CLOSE           , api_lsa_close          },
	{ "LSA_OPENSECRET"      , LSA_OPENSECRET      , api_lsa_open_secret    },
	{ "LSA_LOOKUPSIDS"      , LSA_LOOKUPSIDS      , api_lsa_lookup_sids    },
	{ "LSA_LOOKUPNAMES"     , LSA_LOOKUPNAMES     , api_lsa_lookup_names   },
	{ NULL                  , 0                   , NULL                   }
};

/***************************************************************************
 api_ntLsarpcTNP
 ***************************************************************************/
BOOL api_ntlsa_rpc(pipes_struct *p, prs_struct *data)
{
	return api_rpcTNP(p, "api_ntlsa_rpc", api_lsa_cmds, data);
}
