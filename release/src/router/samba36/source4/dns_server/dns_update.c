/*
   Unix SMB/CIFS implementation.

   DNS server handler for update requests

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
#include "libcli/util/ntstatus.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_dns.h"
#include "librpc/gen_ndr/ndr_dnsp.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "dns_server/dns_server.h"

static WERROR check_prerequsites(struct dns_server *dns,
				 TALLOC_CTX *mem_ctx,
				 const struct dns_name_packet *in,
				 const struct dns_res_rec *prereqs, uint16_t count)
{
	const struct dns_name_question *zone;
	size_t host_part_len = 0;
	uint16_t i;

	zone = in->questions;

	for (i = 0; i < count; i++) {
		const struct dns_res_rec *r = &prereqs[i];
		bool match;

		if (r->ttl != 0) {
			return DNS_ERR(FORMAT_ERROR);
		}
		match = dns_name_match(zone->name, r->name, &host_part_len);
		if (!match) {
			/* TODO: check if we need to echo all prereqs if the
			 * first fails */
			return DNS_ERR(NOTZONE);
		}
		if (r->rr_class == DNS_QCLASS_ANY) {
			if (r->length != 0) {
				return DNS_ERR(FORMAT_ERROR);
			}
			if (r->rr_type == DNS_QTYPE_ALL) {
				/* TODO: Check if zone has at least one RR */
				return DNS_ERR(NAME_ERROR);
			} else {
				/* TODO: Check if RR exists of the specified type */
				return DNS_ERR(NXRRSET);
			}
		}
		if (r->rr_class == DNS_QCLASS_NONE) {
			if (r->length != 0) {
				return DNS_ERR(FORMAT_ERROR);
			}
			if (r->rr_type == DNS_QTYPE_ALL) {
				/* TODO: Return this error if the given name exits in this zone */
				return DNS_ERR(YXDOMAIN);
			} else {
				/* TODO: Return error if there's an RRset of this type in the zone */
				return DNS_ERR(YXRRSET);
			}
		}
		if (r->rr_class == zone->question_class) {
			/* Check if there's a RR with this */
			return DNS_ERR(NOT_IMPLEMENTED);
		} else {
			return DNS_ERR(FORMAT_ERROR);
		}
	}


	return WERR_OK;
}

static WERROR update_prescan(const struct dns_name_question *zone,
			     const struct dns_res_rec *updates, uint16_t count)
{
	const struct dns_res_rec *r;
	uint16_t i;
	size_t host_part_len;
	bool match;

	for (i = 0; i < count; i++) {
		r = &updates[i];
		match = dns_name_match(zone->name, r->name, &host_part_len);
		if (!match) {
			return DNS_ERR(NOTZONE);
		}
		if (zone->question_class == r->rr_class) {
			/*TODO: also check for AXFR,MAILA,MAILB  */
			if (r->rr_type == DNS_QTYPE_ALL) {
				return DNS_ERR(FORMAT_ERROR);
			}
		} else if (r->rr_class == DNS_QCLASS_ANY) {
			if (r->ttl != 0 || r->length != 0) {
				return DNS_ERR(FORMAT_ERROR);
			}
		} else if (r->rr_class == DNS_QCLASS_NONE) {
			if (r->ttl != 0 || r->rr_type == DNS_QTYPE_ALL) {
				return DNS_ERR(FORMAT_ERROR);
			}
		} else {
			return DNS_ERR(FORMAT_ERROR);
		}
	}
	return WERR_OK;
}

WERROR dns_server_process_update(struct dns_server *dns,
				 TALLOC_CTX *mem_ctx,
				 struct dns_name_packet *in,
				 const struct dns_res_rec *prereqs, uint16_t prereq_count,
				 struct dns_res_rec **updates,      uint16_t *update_count,
				 struct dns_res_rec **additional,   uint16_t *arcount)
{
	struct dns_name_question *zone;
	const struct dns_server_zone *z;
	size_t host_part_len = 0;
	WERROR werror = DNS_ERR(NOT_IMPLEMENTED);
	bool update_allowed = false;

	if (in->qdcount != 1) {
		return DNS_ERR(FORMAT_ERROR);
	}

	zone = in->questions;

	if (zone->question_type != DNS_QTYPE_SOA) {
		return DNS_ERR(FORMAT_ERROR);
	}

	DEBUG(0, ("Got a dns update request.\n"));

	for (z = dns->zones; z != NULL; z = z->next) {
		bool match;

		match = dns_name_match(z->name, zone->name, &host_part_len);
		if (match) {
			break;
		}
	}

	if (z == NULL) {
		return DNS_ERR(NOTAUTH);
	}

	if (host_part_len != 0) {
		/* TODO: We need to delegate this one */
		return DNS_ERR(NOT_IMPLEMENTED);
	}

	werror = check_prerequsites(dns, mem_ctx, in, prereqs, prereq_count);
	W_ERROR_NOT_OK_RETURN(werror);

	/* TODO: Check if update is allowed, we probably want "always",
	 * key-based GSSAPI, key-based bind-style TSIG and "never" as
	 * smb.conf options. */
	if (!update_allowed) {
		return DNS_ERR(REFUSED);
	}

	werror = update_prescan(in->questions, *updates, *update_count);
	W_ERROR_NOT_OK_RETURN(werror);

	return werror;
}
