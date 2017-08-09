/*
   Unix SMB/CIFS implementation.

   endpoint server for the samr pipe - definitions

   Copyright (C) Andrew Tridgell 2004

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

#include "param/param.h"

/*
  this type allows us to distinguish handle types
*/
enum samr_handle {
	SAMR_HANDLE_CONNECT,
	SAMR_HANDLE_DOMAIN,
	SAMR_HANDLE_USER,
	SAMR_HANDLE_GROUP,
	SAMR_HANDLE_ALIAS
};


/*
  state asscoiated with a samr_Connect*() operation
*/
struct samr_connect_state {
	struct ldb_context *sam_ctx;
	uint32_t access_mask;
};

/*
  state associated with a samr_OpenDomain() operation
*/
struct samr_domain_state {
	struct samr_connect_state *connect_state;
	void *sam_ctx;
	uint32_t access_mask;
	struct dom_sid *domain_sid;
	const char *domain_name;
	struct ldb_dn *domain_dn;
	enum server_role role;
	bool builtin;
	struct loadparm_context *lp_ctx;
};

/*
  state associated with a open account handle
*/
struct samr_account_state {
	struct samr_domain_state *domain_state;
	void *sam_ctx;
	uint32_t access_mask;
	struct dom_sid *account_sid;
	const char *account_name;
	struct ldb_dn *account_dn;
};
