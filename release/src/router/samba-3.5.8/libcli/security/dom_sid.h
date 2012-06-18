/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Stefan (metze) Metzmacher 	2002-2004
   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Jeremy Allison  		1999

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

#ifndef _DOM_SID_H_
#define _DOM_SID_H_

#include "librpc/gen_ndr/security.h"

int dom_sid_compare(const struct dom_sid *sid1, const struct dom_sid *sid2);
bool dom_sid_equal(const struct dom_sid *sid1, const struct dom_sid *sid2);
bool dom_sid_parse(const char *sidstr, struct dom_sid *ret);
struct dom_sid *dom_sid_parse_talloc(TALLOC_CTX *mem_ctx, const char *sidstr);
struct dom_sid *dom_sid_parse_length(TALLOC_CTX *mem_ctx, const DATA_BLOB *sid);
struct dom_sid *dom_sid_dup(TALLOC_CTX *mem_ctx, const struct dom_sid *dom_sid);
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *mem_ctx,
				const struct dom_sid *domain_sid,
				uint32_t rid);
NTSTATUS dom_sid_split_rid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			   struct dom_sid **domain, uint32_t *rid);
bool dom_sid_in_domain(const struct dom_sid *domain_sid,
		       const struct dom_sid *sid);
char *dom_sid_string(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);

#ifndef MAXSUBAUTHS
#define MAXSUBAUTHS 15 /* max sub authorities in a SID */
#endif

#endif /*_DOM_SID_H_*/

