/*
   Unix SMB/CIFS implementation.

   DNS structures

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

#ifndef __DNS_SERVER_H__
#define __DNS_SERVER_H__

#include "librpc/gen_ndr/dns.h"

struct tsocket_address;

struct dns_server_zone {
	struct dns_server_zone *prev, *next;
	const char *name;
	struct ldb_dn *dn;
};

struct dns_server {
	struct task_server *task;
	struct ldb_context *samdb;
	struct dns_server_zone *zones;
};


WERROR dns_server_process_query(struct dns_server *dns,
				TALLOC_CTX *mem_ctx,
				struct dns_name_packet *in,
				struct dns_res_rec **answers,    uint16_t *ancount,
				struct dns_res_rec **nsrecs,     uint16_t *nscount,
				struct dns_res_rec **additional, uint16_t *arcount);

WERROR dns_server_process_update(struct dns_server *dns,
				 TALLOC_CTX *mem_ctx,
				 struct dns_name_packet *in,
				 const struct dns_res_rec *prereqs, uint16_t prereq_count,
				 struct dns_res_rec **updates,      uint16_t *update_count,
				 struct dns_res_rec **additional,   uint16_t *arcount);

uint8_t werr_to_dns_err(WERROR werror);
bool dns_name_match(const char *zone, const char *name, size_t *host_part_len);
WERROR dns_name2dn(struct dns_server *dns,
		   TALLOC_CTX *mem_ctx,
		   const char *name,
		   struct ldb_dn **_dn);

#define DNS_ERR(err_str) WERR_DNS_ERROR_RCODE_##err_str
#endif /* __DNS_SERVER_H__ */
