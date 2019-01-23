/*
   Unix SMB/CIFS implementation.

   DNS server handler for queries

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "libcli/util/werror.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_dns.h"
#include "librpc/gen_ndr/ndr_dnsp.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "dns_server/dns_server.h"

static WERROR handle_question(struct dns_server *dns,
			      TALLOC_CTX *mem_ctx,
			      const struct dns_name_question *question,
			      struct dns_res_rec **answers, uint16_t *ancount)
{
	struct dns_res_rec *ans;
	struct ldb_dn *dn = NULL;
	WERROR werror;
	static const char * const attrs[] = { "dnsRecord", NULL};
	int ret;
	uint16_t ai = *ancount;
	unsigned int ri;
	struct ldb_message *msg = NULL;
	struct dnsp_DnssrvRpcRecord *recs;
	struct ldb_message_element *el;

	werror = dns_name2dn(dns, mem_ctx, question->name, &dn);
	W_ERROR_NOT_OK_RETURN(werror);

	ret = dsdb_search_one(dns->samdb, mem_ctx, &msg, dn,
			      LDB_SCOPE_BASE, attrs, 0, "%s", "(objectClass=dnsNode)");
	if (ret != LDB_SUCCESS) {
		return DNS_ERR(NAME_ERROR);
	}

	el = ldb_msg_find_element(msg, attrs[0]);
	if (el == NULL) {
		return DNS_ERR(NAME_ERROR);
	}

	recs = talloc_array(mem_ctx, struct dnsp_DnssrvRpcRecord, el->num_values);
	for (ri = 0; ri < el->num_values; ri++) {
		struct ldb_val *v = &el->values[ri];
		enum ndr_err_code ndr_err;

		ndr_err = ndr_pull_struct_blob(v, recs, &recs[ri],
				(ndr_pull_flags_fn_t)ndr_pull_dnsp_DnssrvRpcRecord);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0, ("Failed to grab dnsp_DnssrvRpcRecord\n"));
			return DNS_ERR(SERVER_FAILURE);
		}
	}

	ans = talloc_realloc(mem_ctx, *answers, struct dns_res_rec,
			     ai + el->num_values);
	W_ERROR_HAVE_NO_MEMORY(ans);

	switch (question->question_type) {
	case DNS_QTYPE_CNAME:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = talloc_strdup(ans, question->name);
			ans[ai].rr_type = DNS_QTYPE_CNAME;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.cname_record = talloc_strdup(ans, recs[ri].data.cname);
			ai++;
		}
		break;
	case DNS_QTYPE_A:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			/* TODO: if the record actually is a DNS_QTYPE_A */

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = talloc_strdup(ans, question->name);
			ans[ai].rr_type = DNS_QTYPE_A;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.ipv4_record = talloc_strdup(ans, recs[ri].data.ipv4);
			ai++;
		}
		break;
	case DNS_QTYPE_AAAA:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = talloc_strdup(ans, question->name);
			ans[ai].rr_type = DNS_QTYPE_AAAA;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.ipv6_record = recs[ri].data.ipv6;
			ai++;
		}
		break;
	case DNS_QTYPE_NS:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = question->name;
			ans[ai].rr_type = DNS_QTYPE_NS;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.ns_record = recs[ri].data.ns;
			ai++;
		}
		break;
	case DNS_QTYPE_SRV:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = question->name;
			ans[ai].rr_type = DNS_QTYPE_SRV;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.srv_record.priority = recs[ri].data.srv.wPriority;
			ans[ai].rdata.srv_record.weight = recs[ri].data.srv.wWeight;
			ans[ai].rdata.srv_record.port = recs[ri].data.srv.wPort;
			ans[ai].rdata.srv_record.target = recs[ri].data.srv.nameTarget;
			ai++;
		}
		break;
	case DNS_QTYPE_SOA:
		for (ri = 0; ri < el->num_values; ri++) {
			if (recs[ri].wType != question->question_type) {
				continue;
			}

			ZERO_STRUCT(ans[ai]);
			ans[ai].name = question->name;
			ans[ai].rr_type = DNS_QTYPE_SOA;
			ans[ai].rr_class = DNS_QCLASS_IP;
			ans[ai].ttl = recs[ri].dwTtlSeconds;
			ans[ai].length = UINT16_MAX;
			ans[ai].rdata.soa_record.mname	= recs[ri].data.soa.mname;
			ans[ai].rdata.soa_record.rname	= recs[ri].data.soa.rname;
			ans[ai].rdata.soa_record.serial	= recs[ri].data.soa.serial;
			ans[ai].rdata.soa_record.refresh= recs[ri].data.soa.refresh;
			ans[ai].rdata.soa_record.retry	= recs[ri].data.soa.retry;
			ans[ai].rdata.soa_record.expire	= recs[ri].data.soa.expire;
			ans[ai].rdata.soa_record.minimum= recs[ri].data.soa.minimum;
			ai++;
		}
		break;
	default:
		return DNS_ERR(NOT_IMPLEMENTED);
	}

	if (*ancount == ai) {
		return DNS_ERR(NAME_ERROR);
	}

	*ancount = ai;
	*answers = ans;

	return WERR_OK;

}

WERROR dns_server_process_query(struct dns_server *dns,
				TALLOC_CTX *mem_ctx,
				struct dns_name_packet *in,
				struct dns_res_rec **answers,    uint16_t *ancount,
				struct dns_res_rec **nsrecs,     uint16_t *nscount,
				struct dns_res_rec **additional, uint16_t *arcount)
{
	uint16_t i, num_answers=0;
	struct dns_res_rec *ans=NULL;
	WERROR werror;

	ans = talloc_array(mem_ctx, struct dns_res_rec, 0);
	W_ERROR_HAVE_NO_MEMORY(ans);

	for (i = 0; i < in->qdcount; ++i) {
		werror = handle_question(dns, mem_ctx, &in->questions[i], &ans, &num_answers);
		W_ERROR_NOT_OK_RETURN(werror);
	}

	*answers = ans;
	*ancount = num_answers;

	/*FIXME: Do something for these */
	*nsrecs  = NULL;
	*nscount = 0;

	*additional = NULL;
	*arcount    = 0;

	return WERR_OK;
}
