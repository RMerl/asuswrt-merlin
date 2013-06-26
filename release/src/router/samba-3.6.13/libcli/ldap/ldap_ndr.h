/*
   Unix SMB/CIFS mplementation.

   wrap/unwrap NDR encoded elements for ldap calls

   Copyright (C) Andrew Tridgell  2005

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

#ifndef __LIBCLI_LDAP_LDAP_NDR_H__
#define __LIBCLI_LDAP_LDAP_NDR_H__

#include "librpc/gen_ndr/ndr_misc.h"

char *ldap_encode_ndr_uint32(TALLOC_CTX *mem_ctx, uint32_t value);
char *ldap_encode_ndr_dom_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);
char *ldap_encode_ndr_GUID(TALLOC_CTX *mem_ctx, const struct GUID *guid);
NTSTATUS ldap_decode_ndr_GUID(TALLOC_CTX *mem_ctx, struct ldb_val val, struct GUID *guid);

#endif /* __LIBCLI_LDAP_LDAP_NDR_H__ */

