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

#include "includes.h"
#include "passdb.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "secrets.h"
#include "memcache.h"
#include "idmap_cache.h"
#include "../libcli/security/security.h"
#include "lib/winbind_util.h"

/*****************************************************************
 Dissect a user-provided name into domain, name, sid and type.

 If an explicit domain name was given in the form domain\user, it
 has to try that. If no explicit domain name was given, we have
 to do guesswork.
*****************************************************************/  

bool lookup_name(TALLOC_CTX *mem_ctx,
		 const char *full_name, int flags,
		 const char **ret_domain, const char **ret_name,
		 struct dom_sid *ret_sid, enum lsa_SidType *ret_type)
{
	char *p;
	const char *tmp;
	const char *domain = NULL;
	const char *name = NULL;
	uint32 rid;
	struct dom_sid sid;
	enum lsa_SidType type;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	if (tmp_ctx == NULL) {
		DEBUG(0, ("talloc_new failed\n"));
		return false;
	}

	p = strchr_m(full_name, '\\');

	if (p != NULL) {
		domain = talloc_strndup(tmp_ctx, full_name,
					PTR_DIFF(p, full_name));
		name = talloc_strdup(tmp_ctx, p+1);
	} else {
		domain = talloc_strdup(tmp_ctx, "");
		name = talloc_strdup(tmp_ctx, full_name);
	}

	if ((domain == NULL) || (name == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	DEBUG(10,("lookup_name: %s => domain=[%s], name=[%s]\n",
		full_name, domain, name));
	DEBUG(10, ("lookup_name: flags = 0x0%x\n", flags));

	if ((flags & LOOKUP_NAME_DOMAIN) &&
	    strequal(domain, get_global_sam_name()))
	{

		/* It's our own domain, lookup the name in passdb */
		if (lookup_global_sam_name(name, flags, &rid, &type)) {
			sid_compose(&sid, get_global_sam_sid(), rid);
			goto ok;
		}
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	if ((flags & LOOKUP_NAME_BUILTIN) &&
	    strequal(domain, builtin_domain_name()))
	{
		if (strlen(name) == 0) {
			/* Swap domain and name */
			tmp = name; name = domain; domain = tmp;
			sid_copy(&sid, &global_sid_Builtin);
			type = SID_NAME_DOMAIN;
			goto ok;
		}

		/* Explicit request for a name in BUILTIN */
		if (lookup_builtin_name(name, &rid)) {
			sid_compose(&sid, &global_sid_Builtin, rid);
			type = SID_NAME_ALIAS;
			goto ok;
		}
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	/* Try the explicit winbind lookup first, don't let it guess the
	 * domain yet at this point yet. This comes later. */

	if ((domain[0] != '\0') &&
	    (flags & ~(LOOKUP_NAME_DOMAIN|LOOKUP_NAME_ISOLATED)) &&
	    (winbind_lookup_name(domain, name, &sid, &type))) {
			goto ok;
	}

	if (((flags & LOOKUP_NAME_NO_NSS) == 0)
	    && strequal(domain, unix_users_domain_name())) {
		if (lookup_unix_user_name(name, &sid)) {
			type = SID_NAME_USER;
			goto ok;
		}
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	if (((flags & LOOKUP_NAME_NO_NSS) == 0)
	    && strequal(domain, unix_groups_domain_name())) {
		if (lookup_unix_group_name(name, &sid)) {
			type = SID_NAME_DOM_GRP;
			goto ok;
		}
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	if ((domain[0] == '\0') && (!(flags & LOOKUP_NAME_ISOLATED))) {
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	/* Now the guesswork begins, we haven't been given an explicit
	 * domain. Try the sequence as documented on
	 * http://msdn.microsoft.com/library/en-us/secmgmt/security/lsalookupnames.asp
	 * November 27, 2005 */

	/* 1. well-known names */

	if ((flags & LOOKUP_NAME_WKN) &&
	    lookup_wellknown_name(tmp_ctx, name, &sid, &domain))
	{
		type = SID_NAME_WKN_GRP;
		goto ok;
	}

	/* 2. Builtin domain as such */

	if ((flags & (LOOKUP_NAME_BUILTIN|LOOKUP_NAME_REMOTE)) &&
	    strequal(name, builtin_domain_name()))
	{
		/* Swap domain and name */
		tmp = name; name = domain; domain = tmp;
		sid_copy(&sid, &global_sid_Builtin);
		type = SID_NAME_DOMAIN;
		goto ok;
	}

	/* 3. Account domain */

	if ((flags & LOOKUP_NAME_DOMAIN) &&
	    strequal(name, get_global_sam_name()))
	{
		if (!secrets_fetch_domain_sid(name, &sid)) {
			DEBUG(3, ("Could not fetch my SID\n"));
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		/* Swap domain and name */
		tmp = name; name = domain; domain = tmp;
		type = SID_NAME_DOMAIN;
		goto ok;
	}

	/* 4. Primary domain */

	if ((flags & LOOKUP_NAME_DOMAIN) && !IS_DC &&
	    strequal(name, lp_workgroup()))
	{
		if (!secrets_fetch_domain_sid(name, &sid)) {
			DEBUG(3, ("Could not fetch the domain SID\n"));
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		/* Swap domain and name */
		tmp = name; name = domain; domain = tmp;
		type = SID_NAME_DOMAIN;
		goto ok;
	}

	/* 5. Trusted domains as such, to me it looks as if members don't do
              this, tested an XP workstation in a NT domain -- vl */

	if ((flags & LOOKUP_NAME_REMOTE) && IS_DC &&
	    (pdb_get_trusteddom_pw(name, NULL, &sid, NULL)))
	{
		/* Swap domain and name */
		tmp = name; name = domain; domain = tmp;
		type = SID_NAME_DOMAIN;
		goto ok;
	}

	/* 6. Builtin aliases */	

	if ((flags & LOOKUP_NAME_BUILTIN) &&
	    lookup_builtin_name(name, &rid))
	{
		domain = talloc_strdup(tmp_ctx, builtin_domain_name());
		sid_compose(&sid, &global_sid_Builtin, rid);
		type = SID_NAME_ALIAS;
		goto ok;
	}

	/* 7. Local systems' SAM (DCs don't have a local SAM) */
	/* 8. Primary SAM (On members, this is the domain) */

	/* Both cases are done by looking at our passdb */

	if ((flags & LOOKUP_NAME_DOMAIN) &&
	    lookup_global_sam_name(name, flags, &rid, &type))
	{
		domain = talloc_strdup(tmp_ctx, get_global_sam_name());
		sid_compose(&sid, get_global_sam_sid(), rid);
		goto ok;
	}

	/* Now our local possibilities are exhausted. */

	if (!(flags & LOOKUP_NAME_REMOTE)) {
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	/* If we are not a DC, we have to ask in our primary domain. Let
	 * winbind do that. */

	if (!IS_DC &&
	    (winbind_lookup_name(lp_workgroup(), name, &sid, &type))) {
		domain = talloc_strdup(tmp_ctx, lp_workgroup());
		goto ok;
	}

	/* 9. Trusted domains */

	/* If we're a DC we have to ask all trusted DC's. Winbind does not do
	 * that (yet), but give it a chance. */

	if (IS_DC && winbind_lookup_name("", name, &sid, &type)) {
		struct dom_sid dom_sid;
		enum lsa_SidType domain_type;

		if (type == SID_NAME_DOMAIN) {
			/* Swap name and type */
			tmp = name; name = domain; domain = tmp;
			goto ok;
		}

		/* Here we have to cope with a little deficiency in the
		 * winbind API: We have to ask it again for the name of the
		 * domain it figured out itself. Maybe fix that later... */

		sid_copy(&dom_sid, &sid);
		sid_split_rid(&dom_sid, NULL);

		if (!winbind_lookup_sid(tmp_ctx, &dom_sid, &domain, NULL,
					&domain_type) ||
		    (domain_type != SID_NAME_DOMAIN)) {
			DEBUG(2, ("winbind could not find the domain's name "
				  "it just looked up for us\n"));
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		goto ok;
	}

	/* 10. Don't translate */

	/* 11. Ok, windows would end here. Samba has two more options:
               Unmapped users and unmapped groups */

	if (((flags & LOOKUP_NAME_NO_NSS) == 0)
	    && lookup_unix_user_name(name, &sid)) {
		domain = talloc_strdup(tmp_ctx, unix_users_domain_name());
		type = SID_NAME_USER;
		goto ok;
	}

	if (((flags & LOOKUP_NAME_NO_NSS) == 0)
	    && lookup_unix_group_name(name, &sid)) {
		domain = talloc_strdup(tmp_ctx, unix_groups_domain_name());
		type = SID_NAME_DOM_GRP;
		goto ok;
	}

	/*
	 * Ok, all possibilities tried. Fail.
	 */

	TALLOC_FREE(tmp_ctx);
	return false;

 ok:
	if ((domain == NULL) || (name == NULL)) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	/*
	 * Hand over the results to the talloc context we've been given.
	 */

	if ((ret_name != NULL) &&
	    !(*ret_name = talloc_strdup(mem_ctx, name))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	if (ret_domain != NULL) {
		char *tmp_dom;
		if (!(tmp_dom = talloc_strdup(mem_ctx, domain))) {
			DEBUG(0, ("talloc failed\n"));
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		strupper_m(tmp_dom);
		*ret_domain = tmp_dom;
	}

	if (ret_sid != NULL) {
		sid_copy(ret_sid, &sid);
	}

	if (ret_type != NULL) {
		*ret_type = type;
	}

	TALLOC_FREE(tmp_ctx);
	return true;
}

/************************************************************************
 Names from smb.conf can be unqualified. eg. valid users = foo
 These names should never map to a remote name. Try global_sam_name()\foo,
 and then "Unix Users"\foo (or "Unix Groups"\foo).
************************************************************************/

bool lookup_name_smbconf(TALLOC_CTX *mem_ctx,
		 const char *full_name, int flags,
		 const char **ret_domain, const char **ret_name,
		 struct dom_sid *ret_sid, enum lsa_SidType *ret_type)
{
	char *qualified_name;
	const char *p;

	/* NB. No winbindd_separator here as lookup_name needs \\' */
	if ((p = strchr_m(full_name, *lp_winbind_separator())) != NULL) {

		/* The name is already qualified with a domain. */

		if (*lp_winbind_separator() != '\\') {
			char *tmp;

			/* lookup_name() needs '\\' as a separator */

			tmp = talloc_strdup(mem_ctx, full_name);
			if (!tmp) {
				return false;
			}
			tmp[p - full_name] = '\\';
			full_name = tmp;
		}

		return lookup_name(mem_ctx, full_name, flags,
				ret_domain, ret_name,
				ret_sid, ret_type);
	}

	/* Try with our own SAM name. */
	qualified_name = talloc_asprintf(mem_ctx, "%s\\%s",
				get_global_sam_name(),
				full_name );
	if (!qualified_name) {
		return false;
	}

	if (lookup_name(mem_ctx, qualified_name, flags,
				ret_domain, ret_name,
				ret_sid, ret_type)) {
		return true;
	}

	/* Finally try with "Unix Users" or "Unix Group" */
	qualified_name = talloc_asprintf(mem_ctx, "%s\\%s",
				flags & LOOKUP_NAME_GROUP ?
					unix_groups_domain_name() :
					unix_users_domain_name(),
				full_name );
	if (!qualified_name) {
		return false;
	}

	return lookup_name(mem_ctx, qualified_name, flags,
				ret_domain, ret_name,
				ret_sid, ret_type);
}

static bool wb_lookup_rids(TALLOC_CTX *mem_ctx,
			   const struct dom_sid *domain_sid,
			   int num_rids, uint32 *rids,
			   const char **domain_name,
			   const char **names, enum lsa_SidType *types)
{
	int i;
	const char **my_names;
	enum lsa_SidType *my_types;
	TALLOC_CTX *tmp_ctx;

	if (!(tmp_ctx = talloc_init("wb_lookup_rids"))) {
		return false;
	}

	if (!winbind_lookup_rids(tmp_ctx, domain_sid, num_rids, rids,
				 domain_name, &my_names, &my_types)) {
		*domain_name = "";
		for (i=0; i<num_rids; i++) {
			names[i] = "";
			types[i] = SID_NAME_UNKNOWN;
		}
		TALLOC_FREE(tmp_ctx);
		return true;
	}

	if (!(*domain_name = talloc_strdup(mem_ctx, *domain_name))) {
		TALLOC_FREE(tmp_ctx);
		return false;
	}

	/*
	 * winbind_lookup_rids allocates its own array. We've been given the
	 * array, so copy it over
	 */

	for (i=0; i<num_rids; i++) {
		if (my_names[i] == NULL) {
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		if (!(names[i] = talloc_strdup(names, my_names[i]))) {
			TALLOC_FREE(tmp_ctx);
			return false;
		}
		types[i] = my_types[i];
	}
	TALLOC_FREE(tmp_ctx);
	return true;
}

static bool lookup_rids(TALLOC_CTX *mem_ctx, const struct dom_sid *domain_sid,
			int num_rids, uint32_t *rids,
			const char **domain_name,
			const char ***names, enum lsa_SidType **types)
{
	int i;

	DEBUG(10, ("lookup_rids called for domain sid '%s'\n",
		   sid_string_dbg(domain_sid)));

	if (num_rids) {
		*names = TALLOC_ZERO_ARRAY(mem_ctx, const char *, num_rids);
		*types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_rids);

		if ((*names == NULL) || (*types == NULL)) {
			return false;
		}

		for (i = 0; i < num_rids; i++)
			(*types)[i] = SID_NAME_UNKNOWN;
	} else {
		*names = NULL;
		*types = NULL;
	}

	if (sid_check_is_domain(domain_sid)) {
		NTSTATUS result;

		if (*domain_name == NULL) {
			*domain_name = talloc_strdup(
				mem_ctx, get_global_sam_name());
		}

		if (*domain_name == NULL) {
			return false;
		}

		become_root();
		result = pdb_lookup_rids(domain_sid, num_rids, rids,
					 *names, *types);
		unbecome_root();

		return (NT_STATUS_IS_OK(result) ||
			NT_STATUS_EQUAL(result, NT_STATUS_NONE_MAPPED) ||
			NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED));
	}

	if (sid_check_is_builtin(domain_sid)) {

		if (*domain_name == NULL) {
			*domain_name = talloc_strdup(
				mem_ctx, builtin_domain_name());
		}

		if (*domain_name == NULL) {
			return false;
		}

		for (i=0; i<num_rids; i++) {
			if (lookup_builtin_rid(*names, rids[i],
					       &(*names)[i])) {
				if ((*names)[i] == NULL) {
					return false;
				}
				(*types)[i] = SID_NAME_ALIAS;
			} else {
				(*types)[i] = SID_NAME_UNKNOWN;
			}
		}
		return true;
	}

	if (sid_check_is_wellknown_domain(domain_sid, NULL)) {
		for (i=0; i<num_rids; i++) {
			struct dom_sid sid;
			sid_compose(&sid, domain_sid, rids[i]);
			if (lookup_wellknown_sid(mem_ctx, &sid,
						 domain_name, &(*names)[i])) {
				if ((*names)[i] == NULL) {
					return false;
				}
				(*types)[i] = SID_NAME_WKN_GRP;
			} else {
				(*types)[i] = SID_NAME_UNKNOWN;
			}
		}
		return true;
	}

	if (sid_check_is_unix_users(domain_sid)) {
		if (*domain_name == NULL) {
			*domain_name = talloc_strdup(
				mem_ctx, unix_users_domain_name());
			if (*domain_name == NULL) {
				return false;
			}
		}
		for (i=0; i<num_rids; i++) {
			(*names)[i] = talloc_strdup(
				(*names), uidtoname(rids[i]));
			if ((*names)[i] == NULL) {
				return false;
			}
			(*types)[i] = SID_NAME_USER;
		}
		return true;
	}

	if (sid_check_is_unix_groups(domain_sid)) {
		if (*domain_name == NULL) {
			*domain_name = talloc_strdup(
				mem_ctx, unix_groups_domain_name());
			if (*domain_name == NULL) {
				return false;
			}
		}
		for (i=0; i<num_rids; i++) {
			(*names)[i] = talloc_strdup(
				(*names), gidtoname(rids[i]));
			if ((*names)[i] == NULL) {
				return false;
			}
			(*types)[i] = SID_NAME_DOM_GRP;
		}
		return true;
	}

	return wb_lookup_rids(mem_ctx, domain_sid, num_rids, rids,
			      domain_name, *names, *types);
}

/*
 * Is the SID a domain as such? If yes, lookup its name.
 */

static bool lookup_as_domain(const struct dom_sid *sid, TALLOC_CTX *mem_ctx,
			     const char **name)
{
	const char *tmp;
	enum lsa_SidType type;

	if (sid_check_is_domain(sid)) {
		*name = talloc_strdup(mem_ctx, get_global_sam_name());
		return true;
	}

	if (sid_check_is_builtin(sid)) {
		*name = talloc_strdup(mem_ctx, builtin_domain_name());
		return true;
	}

	if (sid_check_is_wellknown_domain(sid, &tmp)) {
		*name = talloc_strdup(mem_ctx, tmp);
		return true;
	}

	if (sid_check_is_unix_users(sid)) {
		*name = talloc_strdup(mem_ctx, unix_users_domain_name());
		return true;
	}

	if (sid_check_is_unix_groups(sid)) {
		*name = talloc_strdup(mem_ctx, unix_groups_domain_name());
		return true;
	}

	if (sid->num_auths != 4) {
		/* This can't be a domain */
		return false;
	}

	if (IS_DC) {
		uint32 i, num_domains;
		struct trustdom_info **domains;

		/* This is relatively expensive, but it happens only on DCs
		 * and for SIDs that have 4 sub-authorities and thus look like
		 * domains */

		if (!NT_STATUS_IS_OK(pdb_enum_trusteddoms(mem_ctx,
						          &num_domains,
						          &domains))) {
			return false;
		}

		for (i=0; i<num_domains; i++) {
			if (dom_sid_equal(sid, &domains[i]->sid)) {
				*name = talloc_strdup(mem_ctx,
						      domains[i]->name);
				return true;
			}
		}
		return false;
	}

	if (winbind_lookup_sid(mem_ctx, sid, &tmp, NULL, &type) &&
	    (type == SID_NAME_DOMAIN)) {
		*name = tmp;
		return true;
	}

	return false;
}

/*
 * This tries to implement the rather weird rules for the lsa_lookup level
 * parameter.
 *
 * This is as close as we can get to what W2k3 does. With this we survive the
 * RPC-LSALOOKUP samba4 test as of 2006-01-08. NT4 as a PDC is a bit more
 * different, but I assume that's just being too liberal. For example, W2k3
 * replies to everything else but the levels 1-6 with INVALID_PARAMETER
 * whereas NT4 does the same as level 1 (I think). I did not fully test that
 * with NT4, this is what w2k3 does.
 *
 * Level 1: Ask everywhere
 * Level 2: Ask domain and trusted domains, no builtin and wkn
 * Level 3: Only ask domain
 * Level 4: W2k3ad: Only ask AD trusts
 * Level 5: Only ask transitive forest trusts
 * Level 6: Like 4
 */

static bool check_dom_sid_to_level(const struct dom_sid *sid, int level)
{
	int ret = false;

	switch(level) {
	case 1:
		ret = true;
		break;
	case 2:
		ret = (!sid_check_is_builtin(sid) &&
		       !sid_check_is_wellknown_domain(sid, NULL));
		break;
	case 3:
	case 4:
	case 6:
		ret = sid_check_is_domain(sid);
		break;
	case 5:
		ret = false;
		break;
	}

	DEBUG(10, ("%s SID %s in level %d\n",
		   ret ? "Accepting" : "Rejecting",
		   sid_string_dbg(sid), level));
	return ret;
}

/*
 * Lookup a bunch of SIDs. This is modeled after lsa_lookup_sids with
 * references to domains, it is explicitly made for this.
 *
 * This attempts to be as efficient as possible: It collects all SIDs
 * belonging to a domain and hands them in bulk to the appropriate lookup
 * function. In particular pdb_lookup_rids with ldapsam_trusted benefits
 * *hugely* from this.
 */

NTSTATUS lookup_sids(TALLOC_CTX *mem_ctx, int num_sids,
		     const struct dom_sid **sids, int level,
		     struct lsa_dom_info **ret_domains,
		     struct lsa_name_info **ret_names)
{
	TALLOC_CTX *tmp_ctx;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct lsa_name_info *name_infos;
	struct lsa_dom_info *dom_infos = NULL;

	int i, j;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		DEBUG(0, ("talloc_new failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (num_sids) {
		name_infos = TALLOC_ARRAY(mem_ctx, struct lsa_name_info, num_sids);
		if (name_infos == NULL) {
			result = NT_STATUS_NO_MEMORY;
			goto fail;
		}
	} else {
		name_infos = NULL;
	}

	dom_infos = TALLOC_ZERO_ARRAY(mem_ctx, struct lsa_dom_info,
				      LSA_REF_DOMAIN_LIST_MULTIPLIER);
	if (dom_infos == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	/* First build up the data structures:
	 * 
	 * dom_infos is a list of domains referenced in the list of
	 * SIDs. Later we will walk the list of domains and look up the RIDs
	 * in bulk.
	 *
	 * name_infos is a shadow-copy of the SIDs array to collect the real
	 * data.
	 *
	 * dom_info->idxs is an index into the name_infos array. The
	 * difficulty we have here is that we need to keep the SIDs the client
	 * asked for in the same order for the reply
	 */

	for (i=0; i<num_sids; i++) {
		struct dom_sid sid;
		uint32_t rid = 0;
		const char *domain_name = NULL;

		sid_copy(&sid, sids[i]);
		name_infos[i].type = SID_NAME_USE_NONE;

		if (lookup_as_domain(&sid, name_infos, &domain_name)) {
			/* We can't push that through the normal lookup
			 * process, as this would reference illegal
			 * domains.
			 *
			 * For example S-1-5-32 would end up referencing
			 * domain S-1-5- with RID 32 which is clearly wrong.
			 */
			if (domain_name == NULL) {
				result = NT_STATUS_NO_MEMORY;
				goto fail;
			}

			name_infos[i].rid = 0;
			name_infos[i].type = SID_NAME_DOMAIN;
			name_infos[i].name = NULL;

			if (sid_check_is_builtin(&sid)) {
				/* Yes, W2k3 returns "BUILTIN" both as domain
				 * and name here */
				name_infos[i].name = talloc_strdup(
					name_infos, builtin_domain_name());
				if (name_infos[i].name == NULL) {
					result = NT_STATUS_NO_MEMORY;
					goto fail;
				}
			}
		} else {
			/* This is a normal SID with rid component */
			if (!sid_split_rid(&sid, &rid)) {
				result = NT_STATUS_INVALID_SID;
				goto fail;
			}
		}

		if (!check_dom_sid_to_level(&sid, level)) {
			name_infos[i].rid = 0;
			name_infos[i].type = SID_NAME_UNKNOWN;
			name_infos[i].name = NULL;
			continue;
		}

		for (j=0; j<LSA_REF_DOMAIN_LIST_MULTIPLIER; j++) {
			if (!dom_infos[j].valid) {
				break;
			}
			if (dom_sid_equal(&sid, &dom_infos[j].sid)) {
				break;
			}
		}

		if (j == LSA_REF_DOMAIN_LIST_MULTIPLIER) {
			/* TODO: What's the right error message here? */
			result = NT_STATUS_NONE_MAPPED;
			goto fail;
		}

		if (!dom_infos[j].valid) {
			/* We found a domain not yet referenced, create a new
			 * ref. */
			dom_infos[j].valid = true;
			sid_copy(&dom_infos[j].sid, &sid);

			if (domain_name != NULL) {
				/* This name was being found above in the case
				 * when we found a domain SID */
				dom_infos[j].name =
					talloc_strdup(dom_infos, domain_name);
				if (dom_infos[j].name == NULL) {
					result = NT_STATUS_NO_MEMORY;
					goto fail;
				}
			} else {
				/* lookup_rids will take care of this */
				dom_infos[j].name = NULL;
			}
		}

		name_infos[i].dom_idx = j;

		if (name_infos[i].type == SID_NAME_USE_NONE) {
			name_infos[i].rid = rid;

			ADD_TO_ARRAY(dom_infos, int, i, &dom_infos[j].idxs,
				     &dom_infos[j].num_idxs);

			if (dom_infos[j].idxs == NULL) {
				result = NT_STATUS_NO_MEMORY;
				goto fail;
			}
		}
	}

	/* Iterate over the domains found */

	for (i=0; i<LSA_REF_DOMAIN_LIST_MULTIPLIER; i++) {
		uint32_t *rids;
		const char *domain_name = NULL;
		const char **names;
		enum lsa_SidType *types;
		struct lsa_dom_info *dom = &dom_infos[i];

		if (!dom->valid) {
			/* No domains left, we're done */
			break;
		}

		if (dom->num_idxs) {
			if (!(rids = TALLOC_ARRAY(tmp_ctx, uint32, dom->num_idxs))) {
				result = NT_STATUS_NO_MEMORY;
				goto fail;
			}
		} else {
			rids = NULL;
		}

		for (j=0; j<dom->num_idxs; j++) {
			rids[j] = name_infos[dom->idxs[j]].rid;
		}

		if (!lookup_rids(tmp_ctx, &dom->sid,
				 dom->num_idxs, rids, &domain_name,
				 &names, &types)) {
			result = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		if (!(dom->name = talloc_strdup(dom_infos, domain_name))) {
			result = NT_STATUS_NO_MEMORY;
			goto fail;
		}

		for (j=0; j<dom->num_idxs; j++) {
			int idx = dom->idxs[j];
			name_infos[idx].type = types[j];
			if (types[j] != SID_NAME_UNKNOWN) {
				name_infos[idx].name =
					talloc_strdup(name_infos, names[j]);
				if (name_infos[idx].name == NULL) {
					result = NT_STATUS_NO_MEMORY;
					goto fail;
				}
			} else {
				name_infos[idx].name = NULL;
			}
		}
	}

	*ret_domains = dom_infos;
	*ret_names = name_infos;
	TALLOC_FREE(tmp_ctx);
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(dom_infos);
	TALLOC_FREE(name_infos);
	TALLOC_FREE(tmp_ctx);
	return result;
}

/*****************************************************************
 *THE CANONICAL* convert SID to name function.
*****************************************************************/  

bool lookup_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
		const char **ret_domain, const char **ret_name,
		enum lsa_SidType *ret_type)
{
	struct lsa_dom_info *domain;
	struct lsa_name_info *name;
	TALLOC_CTX *tmp_ctx;
	bool ret = false;

	DEBUG(10, ("lookup_sid called for SID '%s'\n", sid_string_dbg(sid)));

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		DEBUG(0, ("talloc_new failed\n"));
		return false;
	}

	if (!NT_STATUS_IS_OK(lookup_sids(tmp_ctx, 1, &sid, 1,
					 &domain, &name))) {
		goto done;
	}

	if (name->type == SID_NAME_UNKNOWN) {
		goto done;
	}

	if ((ret_domain != NULL) &&
	    !(*ret_domain = talloc_strdup(mem_ctx, domain->name))) {
		goto done;
	}

	if ((ret_name != NULL) && 
	    !(*ret_name = talloc_strdup(mem_ctx, name->name))) {
		goto done;
	}

	if (ret_type != NULL) {
		*ret_type = name->type;
	}

	ret = true;

 done:
	if (ret) {
		DEBUG(10, ("Sid %s -> %s\\%s(%d)\n", sid_string_dbg(sid),
			   domain->name, name->name, name->type));
	} else {
		DEBUG(10, ("failed to lookup sid %s\n", sid_string_dbg(sid)));
	}
	TALLOC_FREE(tmp_ctx);
	return ret;
}

/*****************************************************************
 Id mapping cache.  This is to avoid Winbind mappings already
 seen by smbd to be queried too frequently, keeping winbindd
 busy, and blocking smbd while winbindd is busy with other
 stuff. Written by Michael Steffens <michael.steffens@hp.com>,
 modified to use linked lists by jra.
*****************************************************************/  

/*****************************************************************
  Find a SID given a uid.
*****************************************************************/

static bool fetch_sid_from_uid_cache(struct dom_sid *psid, uid_t uid)
{
	DATA_BLOB cache_value;

	if (!memcache_lookup(NULL, UID_SID_CACHE,
			     data_blob_const(&uid, sizeof(uid)),
			     &cache_value)) {
		return false;
	}

	memcpy(psid, cache_value.data, MIN(sizeof(*psid), cache_value.length));
	SMB_ASSERT(cache_value.length >= offsetof(struct dom_sid, id_auth));
	SMB_ASSERT(cache_value.length == ndr_size_dom_sid(psid, 0));

	return true;
}

/*****************************************************************
  Find a uid given a SID.
*****************************************************************/

static bool fetch_uid_from_cache( uid_t *puid, const struct dom_sid *psid )
{
	DATA_BLOB cache_value;

	if (!memcache_lookup(NULL, SID_UID_CACHE,
			     data_blob_const(psid, ndr_size_dom_sid(psid, 0)),
			     &cache_value)) {
		return false;
	}

	SMB_ASSERT(cache_value.length == sizeof(*puid));
	memcpy(puid, cache_value.data, sizeof(*puid));

	return true;
}

/*****************************************************************
 Store uid to SID mapping in cache.
*****************************************************************/

void store_uid_sid_cache(const struct dom_sid *psid, uid_t uid)
{
	memcache_add(NULL, SID_UID_CACHE,
		     data_blob_const(psid, ndr_size_dom_sid(psid, 0)),
		     data_blob_const(&uid, sizeof(uid)));
	memcache_add(NULL, UID_SID_CACHE,
		     data_blob_const(&uid, sizeof(uid)),
		     data_blob_const(psid, ndr_size_dom_sid(psid, 0)));
}

/*****************************************************************
  Find a SID given a gid.
*****************************************************************/

static bool fetch_sid_from_gid_cache(struct dom_sid *psid, gid_t gid)
{
	DATA_BLOB cache_value;

	if (!memcache_lookup(NULL, GID_SID_CACHE,
			     data_blob_const(&gid, sizeof(gid)),
			     &cache_value)) {
		return false;
	}

	memcpy(psid, cache_value.data, MIN(sizeof(*psid), cache_value.length));
	SMB_ASSERT(cache_value.length >= offsetof(struct dom_sid, id_auth));
	SMB_ASSERT(cache_value.length == ndr_size_dom_sid(psid, 0));

	return true;
}

/*****************************************************************
  Find a gid given a SID.
*****************************************************************/

static bool fetch_gid_from_cache(gid_t *pgid, const struct dom_sid *psid)
{
	DATA_BLOB cache_value;

	if (!memcache_lookup(NULL, SID_GID_CACHE,
			     data_blob_const(psid, ndr_size_dom_sid(psid, 0)),
			     &cache_value)) {
		return false;
	}

	SMB_ASSERT(cache_value.length == sizeof(*pgid));
	memcpy(pgid, cache_value.data, sizeof(*pgid));

	return true;
}

/*****************************************************************
 Store gid to SID mapping in cache.
*****************************************************************/

void store_gid_sid_cache(const struct dom_sid *psid, gid_t gid)
{
	memcache_add(NULL, SID_GID_CACHE,
		     data_blob_const(psid, ndr_size_dom_sid(psid, 0)),
		     data_blob_const(&gid, sizeof(gid)));
	memcache_add(NULL, GID_SID_CACHE,
		     data_blob_const(&gid, sizeof(gid)),
		     data_blob_const(psid, ndr_size_dom_sid(psid, 0)));
}

/*****************************************************************
 *THE LEGACY* convert uid_t to SID function.
*****************************************************************/  

static void legacy_uid_to_sid(struct dom_sid *psid, uid_t uid)
{
	bool ret;

	ZERO_STRUCTP(psid);

	become_root();
	ret = pdb_uid_to_sid(uid, psid);
	unbecome_root();

	if (ret) {
		/* This is a mapped user */
		goto done;
	}

	/* This is an unmapped user */

	uid_to_unix_users_sid(uid, psid);

 done:
	DEBUG(10,("LEGACY: uid %u -> sid %s\n", (unsigned int)uid,
		  sid_string_dbg(psid)));

	store_uid_sid_cache(psid, uid);
	return;
}

/*****************************************************************
 *THE LEGACY* convert gid_t to SID function.
*****************************************************************/  

static void legacy_gid_to_sid(struct dom_sid *psid, gid_t gid)
{
	bool ret;

	ZERO_STRUCTP(psid);

	become_root();
	ret = pdb_gid_to_sid(gid, psid);
	unbecome_root();

	if (ret) {
		/* This is a mapped group */
		goto done;
	}

	/* This is an unmapped group */

	gid_to_unix_groups_sid(gid, psid);

 done:
	DEBUG(10,("LEGACY: gid %u -> sid %s\n", (unsigned int)gid,
		  sid_string_dbg(psid)));

	store_gid_sid_cache(psid, gid);
	return;
}

/*****************************************************************
 *THE LEGACY* convert SID to uid function.
*****************************************************************/  

static bool legacy_sid_to_uid(const struct dom_sid *psid, uid_t *puid)
{
	enum lsa_SidType type;

	if (sid_check_is_in_our_domain(psid)) {
		union unid_t id;
		bool ret;

		become_root();
		ret = pdb_sid_to_id(psid, &id, &type);
		unbecome_root();

		if (ret) {
			if (type != SID_NAME_USER) {
				DEBUG(5, ("sid %s is a %s, expected a user\n",
					  sid_string_dbg(psid),
					  sid_type_lookup(type)));
				return false;
			}
			*puid = id.uid;
			goto done;
		}

		/* This was ours, but it was not mapped.  Fail */
	}

	DEBUG(10,("LEGACY: mapping failed for sid %s\n",
		  sid_string_dbg(psid)));
	return false;

done:
	DEBUG(10,("LEGACY: sid %s -> uid %u\n", sid_string_dbg(psid),
		  (unsigned int)*puid ));

	store_uid_sid_cache(psid, *puid);
	return true;
}

/*****************************************************************
 *THE LEGACY* convert SID to gid function.
 Group mapping is used for gids that maps to Wellknown SIDs
*****************************************************************/  

static bool legacy_sid_to_gid(const struct dom_sid *psid, gid_t *pgid)
{
	GROUP_MAP map;
	union unid_t id;
	enum lsa_SidType type;

	if ((sid_check_is_in_builtin(psid) ||
	     sid_check_is_in_wellknown_domain(psid))) {
		bool ret;

		become_root();
		ret = pdb_getgrsid(&map, *psid);
		unbecome_root();

		if (ret) {
			*pgid = map.gid;
			goto done;
		}
		DEBUG(10,("LEGACY: mapping failed for sid %s\n",
			  sid_string_dbg(psid)));
		return false;
	}

	if (sid_check_is_in_our_domain(psid)) {
		bool ret;

		become_root();
		ret = pdb_sid_to_id(psid, &id, &type);
		unbecome_root();

		if (ret) {
			if ((type != SID_NAME_DOM_GRP) &&
			    (type != SID_NAME_ALIAS)) {
				DEBUG(5, ("LEGACY: sid %s is a %s, expected "
					  "a group\n", sid_string_dbg(psid),
					  sid_type_lookup(type)));
				return false;
			}
			*pgid = id.gid;
			goto done;
		}

		/* This was ours, but it was not mapped.  Fail */
	}

	DEBUG(10,("LEGACY: mapping failed for sid %s\n",
		  sid_string_dbg(psid)));
	return false;

 done:
	DEBUG(10,("LEGACY: sid %s -> gid %u\n", sid_string_dbg(psid),
		  (unsigned int)*pgid ));

	store_gid_sid_cache(psid, *pgid);

	return true;
}

/*****************************************************************
 *THE CANONICAL* convert uid_t to SID function.
*****************************************************************/  

void uid_to_sid(struct dom_sid *psid, uid_t uid)
{
	bool expired = true;
	bool ret;
	ZERO_STRUCTP(psid);

	if (fetch_sid_from_uid_cache(psid, uid))
		return;

	/* Check the winbindd cache directly. */
	ret = idmap_cache_find_uid2sid(uid, psid, &expired);

	if (ret && !expired && is_null_sid(psid)) {
		/*
		 * Negative cache entry, we already asked.
		 * do legacy.
		 */
		legacy_uid_to_sid(psid, uid);
		return;
	}

	if (!ret || expired) {
		/* Not in cache. Ask winbindd. */
		if (!winbind_uid_to_sid(psid, uid)) {
			/*
			 * We shouldn't return the NULL SID
			 * here if winbind was running and
			 * couldn't map, as winbind will have
			 * added a negative entry that will
			 * cause us to go though the
			 * legacy_uid_to_sid()
			 * function anyway in the case above
			 * the next time we ask.
			 */
			DEBUG(5, ("uid_to_sid: winbind failed to find a sid "
				  "for uid %u\n", (unsigned int)uid));

			legacy_uid_to_sid(psid, uid);
			return;
		}
	}

	DEBUG(10,("uid %u -> sid %s\n", (unsigned int)uid,
		  sid_string_dbg(psid)));

	store_uid_sid_cache(psid, uid);
	return;
}

/*****************************************************************
 *THE CANONICAL* convert gid_t to SID function.
*****************************************************************/  

void gid_to_sid(struct dom_sid *psid, gid_t gid)
{
	bool expired = true;
	bool ret;
	ZERO_STRUCTP(psid);

	if (fetch_sid_from_gid_cache(psid, gid))
		return;

	/* Check the winbindd cache directly. */
	ret = idmap_cache_find_gid2sid(gid, psid, &expired);

	if (ret && !expired && is_null_sid(psid)) {
		/*
		 * Negative cache entry, we already asked.
		 * do legacy.
		 */
		legacy_gid_to_sid(psid, gid);
		return;
	}

	if (!ret || expired) {
		/* Not in cache. Ask winbindd. */
		if (!winbind_gid_to_sid(psid, gid)) {
			/*
			 * We shouldn't return the NULL SID
			 * here if winbind was running and
			 * couldn't map, as winbind will have
			 * added a negative entry that will
			 * cause us to go though the
			 * legacy_gid_to_sid()
			 * function anyway in the case above
			 * the next time we ask.
			 */
			DEBUG(5, ("gid_to_sid: winbind failed to find a sid "
				  "for gid %u\n", (unsigned int)gid));

			legacy_gid_to_sid(psid, gid);
			return;
		}
	}

	DEBUG(10,("gid %u -> sid %s\n", (unsigned int)gid,
		  sid_string_dbg(psid)));

	store_gid_sid_cache(psid, gid);
	return;
}

bool sids_to_unix_ids(const struct dom_sid *sids, uint32_t num_sids,
		      struct wbcUnixId *ids)
{
	struct wbcDomainSid *wbc_sids = NULL;
	struct wbcUnixId *wbc_ids = NULL;
	uint32_t i, num_not_cached;
	wbcErr err;
	bool ret = false;

	wbc_sids = TALLOC_ARRAY(talloc_tos(), struct wbcDomainSid, num_sids);
	if (wbc_sids == NULL) {
		return false;
	}

	num_not_cached = 0;

	for (i=0; i<num_sids; i++) {
		bool expired;
		uint32_t rid;

		if (fetch_uid_from_cache(&ids[i].id.uid, &sids[i])) {
			ids[i].type = WBC_ID_TYPE_UID;
			continue;
		}
		if (fetch_gid_from_cache(&ids[i].id.gid, &sids[i])) {
			ids[i].type = WBC_ID_TYPE_GID;
			continue;
		}
		if (sid_peek_check_rid(&global_sid_Unix_Users,
				       &sids[i], &rid)) {
			ids[i].type = WBC_ID_TYPE_UID;
			ids[i].id.uid = rid;
			continue;
		}
		if (sid_peek_check_rid(&global_sid_Unix_Groups,
				       &sids[i], &rid)) {
			ids[i].type = WBC_ID_TYPE_GID;
			ids[i].id.gid = rid;
			continue;
		}
		if (idmap_cache_find_sid2uid(&sids[i], &ids[i].id.uid,
					     &expired)
		    && !expired && ids[i].id.uid != (uid_t)-1) {
			ids[i].type = WBC_ID_TYPE_UID;
			continue;
		}
		if (idmap_cache_find_sid2gid(&sids[i], &ids[i].id.gid,
					     &expired)
		    && !expired && ids[i].id.gid != (gid_t)-1) {
			ids[i].type = WBC_ID_TYPE_GID;
			continue;
		}
		ids[i].type = WBC_ID_TYPE_NOT_SPECIFIED;
		memcpy(&wbc_sids[num_not_cached], &sids[i],
		       ndr_size_dom_sid(&sids[i], 0));
		num_not_cached += 1;
	}
	if (num_not_cached == 0) {
		goto done;
	}
	wbc_ids = TALLOC_ARRAY(talloc_tos(), struct wbcUnixId, num_not_cached);
	if (wbc_ids == NULL) {
		goto fail;
	}
	for (i=0; i<num_not_cached; i++) {
		wbc_ids[i].type = WBC_ID_TYPE_NOT_SPECIFIED;
	}
	err = wbcSidsToUnixIds(wbc_sids, num_not_cached, wbc_ids);
	if (!WBC_ERROR_IS_OK(err)) {
		DEBUG(10, ("wbcSidsToUnixIds returned %s\n",
			   wbcErrorString(err)));
	}

	num_not_cached = 0;

	for (i=0; i<num_sids; i++) {
		if (ids[i].type == WBC_ID_TYPE_NOT_SPECIFIED) {
			ids[i] = wbc_ids[num_not_cached];
			num_not_cached += 1;
		}
	}

	for (i=0; i<num_sids; i++) {
		if (ids[i].type != WBC_ID_TYPE_NOT_SPECIFIED) {
			continue;
		}
		if (legacy_sid_to_gid(&sids[i], &ids[i].id.gid)) {
			ids[i].type = WBC_ID_TYPE_GID;
			continue;
		}
		if (legacy_sid_to_uid(&sids[i], &ids[i].id.uid)) {
			ids[i].type = WBC_ID_TYPE_UID;
			continue;
		}
	}

done:
	for (i=0; i<num_sids; i++) {
		switch(ids[i].type) {
		case WBC_ID_TYPE_GID:
			if (ids[i].id.gid == (gid_t)-1) {
				ids[i].type = WBC_ID_TYPE_NOT_SPECIFIED;
			}
			break;
		case WBC_ID_TYPE_UID:
			if (ids[i].id.uid == (uid_t)-1) {
				ids[i].type = WBC_ID_TYPE_NOT_SPECIFIED;
			}
			break;
		case WBC_ID_TYPE_NOT_SPECIFIED:
			break;
		}
	}
	ret = true;
fail:
	TALLOC_FREE(wbc_ids);
	TALLOC_FREE(wbc_sids);
	return ret;
}

/*****************************************************************
 *THE CANONICAL* convert SID to uid function.
*****************************************************************/  

bool sid_to_uid(const struct dom_sid *psid, uid_t *puid)
{
	bool expired = true;
	bool ret;
	uint32 rid;
	gid_t gid;

	if (fetch_uid_from_cache(puid, psid))
		return true;

	if (fetch_gid_from_cache(&gid, psid)) {
		return false;
	}

	/* Optimize for the Unix Users Domain
	 * as the conversion is straightforward */
	if (sid_peek_check_rid(&global_sid_Unix_Users, psid, &rid)) {
		uid_t uid = rid;
		*puid = uid;

		/* return here, don't cache */
		DEBUG(10,("sid %s -> uid %u\n", sid_string_dbg(psid),
			(unsigned int)*puid ));
		return true;
	}

	/* Check the winbindd cache directly. */
	ret = idmap_cache_find_sid2uid(psid, puid, &expired);

	if (ret && !expired && (*puid == (uid_t)-1)) {
		/*
		 * Negative cache entry, we already asked.
		 * do legacy.
		 */
		return legacy_sid_to_uid(psid, puid);
	}

	if (!ret || expired) {
		/* Not in cache. Ask winbindd. */
		if (!winbind_sid_to_uid(puid, psid)) {
			DEBUG(5, ("winbind failed to find a uid for sid %s\n",
				  sid_string_dbg(psid)));
			/* winbind failed. do legacy */
			return legacy_sid_to_uid(psid, puid);
		}
	}

	/* TODO: Here would be the place to allocate both a gid and a uid for
	 * the SID in question */

	DEBUG(10,("sid %s -> uid %u\n", sid_string_dbg(psid),
		(unsigned int)*puid ));

	store_uid_sid_cache(psid, *puid);
	return true;
}

/*****************************************************************
 *THE CANONICAL* convert SID to gid function.
 Group mapping is used for gids that maps to Wellknown SIDs
*****************************************************************/  

bool sid_to_gid(const struct dom_sid *psid, gid_t *pgid)
{
	bool expired = true;
	bool ret;
	uint32 rid;
	uid_t uid;

	if (fetch_gid_from_cache(pgid, psid))
		return true;

	if (fetch_uid_from_cache(&uid, psid))
		return false;

	/* Optimize for the Unix Groups Domain
	 * as the conversion is straightforward */
	if (sid_peek_check_rid(&global_sid_Unix_Groups, psid, &rid)) {
		gid_t gid = rid;
		*pgid = gid;

		/* return here, don't cache */
		DEBUG(10,("sid %s -> gid %u\n", sid_string_dbg(psid),
			(unsigned int)*pgid ));
		return true;
	}

	/* Check the winbindd cache directly. */
	ret = idmap_cache_find_sid2gid(psid, pgid, &expired);

	if (ret && !expired && (*pgid == (gid_t)-1)) {
		/*
		 * Negative cache entry, we already asked.
		 * do legacy.
		 */
		return legacy_sid_to_gid(psid, pgid);
	}

	if (!ret || expired) {
		/* Not in cache or negative. Ask winbindd. */
		/* Ask winbindd if it can map this sid to a gid.
		 * (Idmap will check it is a valid SID and of the right type) */

		if ( !winbind_sid_to_gid(pgid, psid) ) {

			DEBUG(10,("winbind failed to find a gid for sid %s\n",
				  sid_string_dbg(psid)));
			/* winbind failed. do legacy */
			return legacy_sid_to_gid(psid, pgid);
		}
	}

	DEBUG(10,("sid %s -> gid %u\n", sid_string_dbg(psid),
		  (unsigned int)*pgid ));

	store_gid_sid_cache(psid, *pgid);
	return true;
}

/**
 * @brief This function gets the primary group SID mapping the primary
 *        GID of the user as obtained by an actual getpwnam() call.
 *        This is necessary to avoid issues with arbitrary group SIDs
 *        stored in passdb. We try as hard as we can to get the SID
 *        corresponding to the GID, including trying group mapping.
 *        If nothing else works, we will force "Domain Users" as the
 *        primary group.
 *        This is needed because we must always be able to lookup the
 *        primary group SID, so we cannot settle for an arbitrary SID.
 *
 *        This call can be expensive. Use with moderation.
 *        If you have a "samu" struct around use pdb_get_group_sid()
 *        instead as it does properly cache results.
 *
 * @param mem_ctx[in]     The memory context iused to allocate the result.
 * @param username[in]    The user's name
 * @param _pwd[in|out]    If available, pass in user's passwd struct.
 *                        It will contain a tallocated passwd if NULL was
 *                        passed in.
 * @param _group_sid[out] The user's Primary Group SID
 *
 * @return NTSTATUS error code.
 */
NTSTATUS get_primary_group_sid(TALLOC_CTX *mem_ctx,
				const char *username,
				struct passwd **_pwd,
				struct dom_sid **_group_sid)
{
	TALLOC_CTX *tmp_ctx;
	bool need_lookup_sid = false;
	struct dom_sid *group_sid;
	struct passwd *pwd = *_pwd;

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!pwd) {
		pwd = Get_Pwnam_alloc(mem_ctx, username);
		if (!pwd) {
			DEBUG(0, ("Failed to find a Unix account for %s",
				  username));
			TALLOC_FREE(tmp_ctx);
			return NT_STATUS_NO_SUCH_USER;
		}
	}

	group_sid = talloc_zero(mem_ctx, struct dom_sid);
	if (!group_sid) {
		TALLOC_FREE(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	gid_to_sid(group_sid, pwd->pw_gid);
	if (!is_null_sid(group_sid)) {
		struct dom_sid domain_sid;
		uint32_t rid;

		/* We need a sid within our domain */
		sid_copy(&domain_sid, group_sid);
		sid_split_rid(&domain_sid, &rid);
		if (dom_sid_equal(&domain_sid, get_global_sam_sid())) {
			/*
			 * As shortcut for the expensive lookup_sid call
			 * compare the domain sid part
			 */
			switch (rid) {
			case DOMAIN_RID_ADMINS:
			case DOMAIN_RID_USERS:
				goto done;
			default:
				need_lookup_sid = true;
				break;
			}
		} else {
			/* Try group mapping */
			ZERO_STRUCTP(group_sid);
			if (pdb_gid_to_sid(pwd->pw_gid, group_sid)) {
				need_lookup_sid = true;
			}
		}
	}

	/* We must verify that this is a valid SID that resolves to a
	 * group of the correct type */
	if (need_lookup_sid) {
		enum lsa_SidType type = SID_NAME_UNKNOWN;
		bool lookup_ret;

		DEBUG(10, ("do lookup_sid(%s) for group of user %s\n",
			   sid_string_dbg(group_sid), username));

		/* Now check that it's actually a domain group and
		 * not something else */
		lookup_ret = lookup_sid(tmp_ctx, group_sid,
					NULL, NULL, &type);

		if (lookup_ret && (type == SID_NAME_DOM_GRP)) {
			goto done;
		}

		DEBUG(3, ("Primary group %s for user %s is"
			  " a %s and not a domain group\n",
			  sid_string_dbg(group_sid), username,
			  sid_type_lookup(type)));
	}

	/* Everything else, failed.
	 * Just set it to the 'Domain Users' RID of 513 which will
	   always resolve to a name */
	DEBUG(3, ("Forcing Primary Group to 'Domain Users' for %s\n",
		  username));

	sid_compose(group_sid, get_global_sam_sid(), DOMAIN_RID_USERS);

done:
	*_pwd = talloc_move(mem_ctx, &pwd);
	*_group_sid = talloc_move(mem_ctx, &group_sid);
	TALLOC_FREE(tmp_ctx);
	return NT_STATUS_OK;
}

bool delete_uid_cache(uid_t puid)
{
	DATA_BLOB uid = data_blob_const(&puid, sizeof(puid));
	DATA_BLOB sid;

	if (!memcache_lookup(NULL, UID_SID_CACHE, uid, &sid)) {
		DEBUG(3, ("UID %d is not memcached!\n", (int)puid));
		return false;
	}
	DEBUG(3, ("Delete mapping UID %d <-> %s from memcache\n", (int)puid,
		  sid_string_dbg((struct dom_sid*)sid.data)));
	memcache_delete(NULL, SID_UID_CACHE, sid);
	memcache_delete(NULL, UID_SID_CACHE, uid);
	return true;
}

bool delete_gid_cache(gid_t pgid)
{
	DATA_BLOB gid = data_blob_const(&pgid, sizeof(pgid));
	DATA_BLOB sid;
	if (!memcache_lookup(NULL, GID_SID_CACHE, gid, &sid)) {
		DEBUG(3, ("GID %d is not memcached!\n", (int)pgid));
		return false;
	}
	DEBUG(3, ("Delete mapping GID %d <-> %s from memcache\n", (int)pgid,
		  sid_string_dbg((struct dom_sid*)sid.data)));
	memcache_delete(NULL, SID_GID_CACHE, sid);
	memcache_delete(NULL, GID_SID_CACHE, gid);
	return true;
}

bool delete_sid_cache(const struct dom_sid* psid)
{
	DATA_BLOB sid = data_blob_const(psid, ndr_size_dom_sid(psid, 0));
	DATA_BLOB id;
	if (memcache_lookup(NULL, SID_GID_CACHE, sid, &id)) {
		DEBUG(3, ("Delete mapping %s <-> GID %d from memcache\n",
			  sid_string_dbg(psid), *(int*)id.data));
		memcache_delete(NULL, SID_GID_CACHE, sid);
		memcache_delete(NULL, GID_SID_CACHE, id);
	} else if (memcache_lookup(NULL, SID_UID_CACHE, sid, &id)) {
		DEBUG(3, ("Delete mapping %s <-> UID %d from memcache\n",
			  sid_string_dbg(psid), *(int*)id.data));
		memcache_delete(NULL, SID_UID_CACHE, sid);
		memcache_delete(NULL, UID_SID_CACHE, id);
	} else {
		DEBUG(3, ("SID %s is not memcached!\n", sid_string_dbg(psid)));
		return false;
	}
	return true;
}

void flush_gid_cache(void)
{
	DEBUG(3, ("Flush GID <-> SID memcache\n"));
	memcache_flush(NULL, SID_GID_CACHE);
	memcache_flush(NULL, GID_SID_CACHE);
}

void flush_uid_cache(void)
{
	DEBUG(3, ("Flush UID <-> SID memcache\n"));
	memcache_flush(NULL, SID_UID_CACHE);
	memcache_flush(NULL, UID_SID_CACHE);
}
