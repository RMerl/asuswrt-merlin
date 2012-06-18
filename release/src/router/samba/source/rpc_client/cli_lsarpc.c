
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison                    1999.
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


#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"

extern int DEBUGLEVEL;


/****************************************************************************
do a LSA Open Policy
****************************************************************************/

BOOL do_lsa_open_policy(struct cli_state *cli,
			char *server_name, POLICY_HND *hnd,
			BOOL sec_qos)
{
	prs_struct rbuf;
	prs_struct buf; 
	LSA_Q_OPEN_POL q_o;
	LSA_SEC_QOS qos;
	LSA_R_OPEN_POL r_o;

	if (hnd == NULL)
		return False;

	prs_init(&buf , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rbuf, 0, 4, UNMARSHALL );

	/* create and send a MSRPC command with api LSA_OPENPOLICY */

	DEBUG(4,("LSA Open Policy\n"));

	/* store the parameters */
	if (sec_qos) {
		init_lsa_sec_qos(&qos, 2, 1, 0, 0x20000000);
		init_q_open_pol(&q_o, 0x5c, 0, 0, &qos);
	} else {
		init_q_open_pol(&q_o, 0x5c, 0, 0x1, NULL);
	}

	/* turn parameters into data stream */
	if(!lsa_io_q_open_pol("", &q_o, &buf, 0)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, LSA_OPENPOLICY, &buf, &rbuf)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&buf);

	if(!lsa_io_r_open_pol("", &r_o, &rbuf, 0)) {
		DEBUG(0,("do_lsa_open_policy: Failed to unmarshall LSA_R_OPEN_POL\n"));
		prs_mem_free(&rbuf);
		return False;
	}

	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("LSA_OPENPOLICY: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rbuf);
		return False;
	} else {
		/* ok, at last: we're happy. return the policy handle */
		memcpy(hnd, r_o.pol.data, sizeof(hnd->data));
	}

	prs_mem_free(&rbuf);

	return True;
}

/****************************************************************************
do a LSA Lookup SIDs
****************************************************************************/

BOOL do_lsa_lookup_sids(struct cli_state *cli,
			POLICY_HND *hnd,
			int num_sids,
			DOM_SID **sids,
			char ***names,
			int *num_names)
{
	prs_struct rbuf;
	prs_struct buf; 
	LSA_Q_LOOKUP_SIDS q_l;
	LSA_R_LOOKUP_SIDS r_l;
	DOM_R_REF ref;
	LSA_TRANS_NAME_ENUM t_names;
	int i;
	BOOL valid_response = False;

	if (hnd == NULL || num_sids == 0 || sids == NULL)
		return False;

	prs_init(&buf , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rbuf, 0, 4, UNMARSHALL );

	/* create and send a MSRPC command with api LSA_LOOKUP_SIDS */

	DEBUG(4,("LSA Lookup SIDs\n"));

	/* store the parameters */
	init_q_lookup_sids(&q_l, hnd, num_sids, sids, 1);

	/* turn parameters into data stream */
	if(!lsa_io_q_lookup_sids("", &q_l, &buf, 0)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, LSA_LOOKUPSIDS, &buf, &rbuf)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&buf);

	r_l.dom_ref = &ref;
	r_l.names   = &t_names;

	if(!lsa_io_r_lookup_sids("", &r_l, &rbuf, 0)) {
		DEBUG(0,("do_lsa_lookup_sids: Failed to unmarshall LSA_R_LOOKUP_SIDS\n"));
		prs_mem_free(&rbuf);
		return False;
	}

		
	if (r_l.status != 0) {
		/* report error code */
		DEBUG(0,("LSA_LOOKUP_SIDS: %s\n", get_nt_error_msg(r_l.status)));
	} else {
		if (t_names.ptr_trans_names != 0)
			valid_response = True;
	}

	if(!valid_response) {
		prs_mem_free(&rbuf);
		return False;
	}

	if (num_names != NULL)
		(*num_names) = t_names.num_entries;

	for (i = 0; i < t_names.num_entries; i++) {
		if (t_names.name[i].domain_idx >= ref.num_ref_doms_1) {
			DEBUG(0,("LSA_LOOKUP_SIDS: domain index out of bounds\n"));
			prs_mem_free(&rbuf);
			return False;
		}
	}

	if (names != NULL && t_names.num_entries != 0)
		(*names) = (char**)malloc((*num_names) * sizeof(char*));

	if (names != NULL && (*names) != NULL) {
		/* take each name, construct a \DOMAIN\name string */
		for (i = 0; i < (*num_names); i++) {
			fstring name;
			fstring dom_name;
			fstring full_name;
			uint32 dom_idx = t_names.name[i].domain_idx;
			fstrcpy(dom_name, dos_unistr2(ref.ref_dom[dom_idx].uni_dom_name.buffer));
			fstrcpy(name, dos_unistr2(t_names.uni_name[i].buffer));
			
			slprintf(full_name, sizeof(full_name)-1, "\\%s\\%s",
			         dom_name, name);

			(*names)[i] = strdup(full_name);
		}
	}

	prs_mem_free(&rbuf);

	return valid_response;
}

/****************************************************************************
do a LSA Query Info Policy
****************************************************************************/
BOOL do_lsa_query_info_pol(struct cli_state *cli,
			POLICY_HND *hnd, uint16 info_class,
			fstring domain_name, DOM_SID *domain_sid)
{
	prs_struct rbuf;
	prs_struct buf; 
	LSA_Q_QUERY_INFO q_q;
	LSA_R_QUERY_INFO r_q;
	fstring sid_str;

	ZERO_STRUCTP(domain_sid);
	domain_name[0] = 0;

	if (hnd == NULL || domain_name == NULL || domain_sid == NULL)
		return False;

	prs_init(&buf , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rbuf, 0, 4, UNMARSHALL );

	/* create and send a MSRPC command with api LSA_QUERYINFOPOLICY */

	DEBUG(4,("LSA Query Info Policy\n"));

	/* store the parameters */
	init_q_query(&q_q, hnd, info_class);

	/* turn parameters into data stream */
	if(!lsa_io_q_query("", &q_q, &buf, 0)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, LSA_QUERYINFOPOLICY, &buf, &rbuf)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&buf);

	if(!lsa_io_r_query("", &r_q, &rbuf, 0)) {
		prs_mem_free(&rbuf);
		return False;
	}

	if (r_q.status != 0) {
		/* report error code */
		DEBUG(0,("LSA_QUERYINFOPOLICY: %s\n", get_nt_error_msg(r_q.status)));
		prs_mem_free(&rbuf);
		return False;
	}

	if (r_q.info_class != q_q.info_class) {
		/* report different info classes */
		DEBUG(0,("LSA_QUERYINFOPOLICY: error info_class (q,r) differ - (%x,%x)\n",
				q_q.info_class, r_q.info_class));
		prs_mem_free(&rbuf);
		return False;
	}

	/* ok, at last: we're happy. */
	switch (r_q.info_class) {
	case 3:
		if (r_q.dom.id3.buffer_dom_name != 0) {
			char *dom_name = dos_unistrn2(r_q.dom.id3.uni_domain_name.buffer,
						  r_q.dom.id3.uni_domain_name.uni_str_len);
			fstrcpy(domain_name, dom_name);
		}
		if (r_q.dom.id3.buffer_dom_sid != 0)
			*domain_sid = r_q.dom.id3.dom_sid.sid;
		break;
	case 5:
		if (r_q.dom.id5.buffer_dom_name != 0) {
			char *dom_name = dos_unistrn2(r_q.dom.id5.uni_domain_name.buffer,
						  r_q.dom.id5.uni_domain_name.uni_str_len);
			fstrcpy(domain_name, dom_name);
		}
		if (r_q.dom.id5.buffer_dom_sid != 0)
			*domain_sid = r_q.dom.id5.dom_sid.sid;
		break;
	default:
		DEBUG(3,("LSA_QUERYINFOPOLICY: unknown info class\n"));
		domain_name[0] = 0;

		prs_mem_free(&rbuf);
		return False;
	}
		
	sid_to_string(sid_str, domain_sid);
	DEBUG(3,("LSA_QUERYINFOPOLICY (level %x): domain:%s  domain sid:%s\n",
	          r_q.info_class, domain_name, sid_str));

	prs_mem_free(&rbuf);

	return True;
}

/****************************************************************************
do a LSA Close
****************************************************************************/

BOOL do_lsa_close(struct cli_state *cli, POLICY_HND *hnd)
{
	prs_struct rbuf;
	prs_struct buf; 
	LSA_Q_CLOSE q_c;
	LSA_R_CLOSE r_c;
	int i;

	if (hnd == NULL)
		return False;

	/* create and send a MSRPC command with api LSA_OPENPOLICY */

	prs_init(&buf , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rbuf, 0, 4, UNMARSHALL );

	DEBUG(4,("LSA Close\n"));

	/* store the parameters */
	init_lsa_q_close(&q_c, hnd);

	/* turn parameters into data stream */
	if(!lsa_io_q_close("", &q_c, &buf, 0)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, LSA_CLOSE, &buf, &rbuf)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&buf);

	if(!lsa_io_r_close("", &r_c, &rbuf, 0)) {
		prs_mem_free(&rbuf);
		return False;
	}

	if (r_c.status != 0) {
		/* report error code */
		DEBUG(0,("LSA_CLOSE: %s\n", get_nt_error_msg(r_c.status)));
		prs_mem_free(&rbuf);
		return False;
	}

	/* check that the returned policy handle is all zeros */

	for (i = 0; i < sizeof(r_c.pol.data); i++) {
		if (r_c.pol.data[i] != 0) {
			DEBUG(0,("LSA_CLOSE: non-zero handle returned\n"));
			prs_mem_free(&rbuf);
			return False;
		}
	}

	prs_mem_free(&rbuf);

	return True;
}
