/*
   Unix SMB/CIFS implementation.
   uid/user handling
   Copyright (C) Andrew Tridgell         1992-1998
   Copyright (C) Gerald (Jerry) Carter   2003
   Copyright (C) Volker Lendecke	 2005

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


#ifndef _PASSDB_LOOKUP_SID_H_
#define _PASSDB_LOOKUP_SID_H_

#include "../librpc/gen_ndr/lsa.h"
#include "nsswitch/libwbclient/wbclient.h"

#define LOOKUP_NAME_NONE		0x00000000
#define LOOKUP_NAME_ISOLATED             0x00000001  /* Look up unqualified names */
#define LOOKUP_NAME_REMOTE               0x00000002  /* Ask others */
#define LOOKUP_NAME_GROUP                0x00000004  /* (unused) This is a NASTY hack for
							valid users = @foo where foo also
							exists in as user. */
#define LOOKUP_NAME_NO_NSS		 0x00000008  /* no NSS calls to avoid
							winbind recursions */
#define LOOKUP_NAME_BUILTIN		0x00000010 /* builtin names */
#define LOOKUP_NAME_WKN			0x00000020 /* well known names */
#define LOOKUP_NAME_DOMAIN		0x00000040 /* only lookup own domain */
#define LOOKUP_NAME_LOCAL		(LOOKUP_NAME_ISOLATED\
					|LOOKUP_NAME_BUILTIN\
					|LOOKUP_NAME_WKN\
					|LOOKUP_NAME_DOMAIN)
#define LOOKUP_NAME_ALL			(LOOKUP_NAME_ISOLATED\
					|LOOKUP_NAME_REMOTE\
					|LOOKUP_NAME_BUILTIN\
					|LOOKUP_NAME_WKN\
					|LOOKUP_NAME_DOMAIN)

struct lsa_dom_info {
	bool valid;
	struct dom_sid sid;
	const char *name;
	int num_idxs;
	int *idxs;
};

struct lsa_name_info {
	uint32 rid;
	enum lsa_SidType type;
	const char *name;
	int dom_idx;
};

/* The following definitions come from passdb/lookup_sid.c  */

bool lookup_name(TALLOC_CTX *mem_ctx,
		 const char *full_name, int flags,
		 const char **ret_domain, const char **ret_name,
		 struct dom_sid *ret_sid, enum lsa_SidType *ret_type);
bool lookup_name_smbconf(TALLOC_CTX *mem_ctx,
		 const char *full_name, int flags,
		 const char **ret_domain, const char **ret_name,
		 struct dom_sid *ret_sid, enum lsa_SidType *ret_type);
NTSTATUS lookup_sids(TALLOC_CTX *mem_ctx, int num_sids,
		     const struct dom_sid **sids, int level,
		     struct lsa_dom_info **ret_domains,
		     struct lsa_name_info **ret_names);
bool lookup_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
		const char **ret_domain, const char **ret_name,
		enum lsa_SidType *ret_type);
void store_uid_sid_cache(const struct dom_sid *psid, uid_t uid);
void store_gid_sid_cache(const struct dom_sid *psid, gid_t gid);
void uid_to_sid(struct dom_sid *psid, uid_t uid);
void gid_to_sid(struct dom_sid *psid, gid_t gid);
bool sids_to_unix_ids(const struct dom_sid *sids, uint32_t num_sids,
		      struct wbcUnixId *ids);
bool sid_to_uid(const struct dom_sid *psid, uid_t *puid);
bool sid_to_gid(const struct dom_sid *psid, gid_t *pgid);
NTSTATUS get_primary_group_sid(TALLOC_CTX *mem_ctx,
				const char *username,
				struct passwd **_pwd,
				struct dom_sid **_group_sid);
bool delete_uid_cache(uid_t uid);
bool delete_gid_cache(gid_t gid);
bool delete_sid_cache(const struct dom_sid* psid);
void flush_uid_cache(void);
void flush_gid_cache(void);

#endif /* _PASSDB_LOOKUP_SID_H_ */
