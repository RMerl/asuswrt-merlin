/*
   Unix SMB/CIFS implementation.

   DNS server utils

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
#include "libcli/util/werror.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_dns.h"
#include "librpc/gen_ndr/ndr_dnsp.h"
#include <ldb.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include "dns_server/dns_server.h"

uint8_t werr_to_dns_err(WERROR werr)
{
	if (W_ERROR_EQUAL(WERR_OK, werr)) {
		return DNS_RCODE_OK;
	} else if (W_ERROR_EQUAL(DNS_ERR(FORMAT_ERROR), werr)) {
		return DNS_RCODE_FORMERR;
	} else if (W_ERROR_EQUAL(DNS_ERR(SERVER_FAILURE), werr)) {
		return DNS_RCODE_SERVFAIL;
	} else if (W_ERROR_EQUAL(DNS_ERR(NAME_ERROR), werr)) {
		return DNS_RCODE_NXDOMAIN;
	} else if (W_ERROR_EQUAL(DNS_ERR(NOT_IMPLEMENTED), werr)) {
		return DNS_RCODE_NOTIMP;
	} else if (W_ERROR_EQUAL(DNS_ERR(REFUSED), werr)) {
		return DNS_RCODE_REFUSED;
	} else if (W_ERROR_EQUAL(DNS_ERR(YXDOMAIN), werr)) {
		return DNS_RCODE_YXDOMAIN;
	} else if (W_ERROR_EQUAL(DNS_ERR(YXRRSET), werr)) {
		return DNS_RCODE_YXRRSET;
	} else if (W_ERROR_EQUAL(DNS_ERR(NXRRSET), werr)) {
		return DNS_RCODE_NXRRSET;
	} else if (W_ERROR_EQUAL(DNS_ERR(NOTAUTH), werr)) {
		return DNS_RCODE_NOTAUTH;
	} else if (W_ERROR_EQUAL(DNS_ERR(NOTZONE), werr)) {
		return DNS_RCODE_NOTZONE;
	}
	DEBUG(5, ("No mapping exists for %%s\n"));
	return DNS_RCODE_SERVFAIL;
}

bool dns_name_match(const char *zone, const char *name, size_t *host_part_len)
{
	size_t zl = strlen(zone);
	size_t nl = strlen(name);
	ssize_t zi, ni;
	static const size_t fixup = 'a' - 'A';

	if (zl > nl) {
		return false;
	}

	for (zi = zl, ni = nl; zi >= 0; zi--, ni--) {
		char zc = zone[zi];
		char nc = name[ni];

		/* convert to lower case */
		if (zc >= 'A' && zc <= 'Z') {
			zc += fixup;
		}
		if (nc >= 'A' && nc <= 'Z') {
			nc += fixup;
		}

		if (zc != nc) {
			return false;
		}
	}

	if (ni >= 0) {
		if (name[ni] != '.') {
			return false;
		}

		ni--;
	}

	*host_part_len = ni+1;

	return true;
}

WERROR dns_name2dn(struct dns_server *dns,
		   TALLOC_CTX *mem_ctx,
		   const char *name,
		   struct ldb_dn **_dn)
{
	struct ldb_dn *base;
	struct ldb_dn *dn;
	const struct dns_server_zone *z;
	size_t host_part_len = 0;

	if (name == NULL) {
		return DNS_ERR(FORMAT_ERROR);
	}

	/*TODO: Check if 'name' is a valid DNS name */

	if (strcmp(name, "") == 0) {
		base = ldb_get_default_basedn(dns->samdb);
		dn = ldb_dn_copy(mem_ctx, base);
		ldb_dn_add_child_fmt(dn, "DC=@,DC=RootDNSServers,CN=MicrosoftDNS,CN=System");
		*_dn = dn;
		return WERR_OK;
	}

	for (z = dns->zones; z != NULL; z = z->next) {
		bool match;

		match = dns_name_match(z->name, name, &host_part_len);
		if (match) {
			break;
		}
	}

	if (z == NULL) {
		return DNS_ERR(NAME_ERROR);
	}

	if (host_part_len == 0) {
		dn = ldb_dn_copy(mem_ctx, z->dn);
		ldb_dn_add_child_fmt(dn, "DC=@");
		*_dn = dn;
		return WERR_OK;
	}

	dn = ldb_dn_copy(mem_ctx, z->dn);
	ldb_dn_add_child_fmt(dn, "DC=%*.*s", (int)host_part_len, (int)host_part_len, name);
	*_dn = dn;
	return WERR_OK;
}
