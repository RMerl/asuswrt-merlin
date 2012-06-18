/* 
   Unix SMB/CIFS implementation.

   mapping routines for SID <-> unix uid/gid

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

#include "includes.h"
#include "system/passwd.h"
#include "../libds/common/flags.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "libcli/ldap/ldap_ndr.h"
#include "lib/ldb/include/ldb.h"
#include "../lib/util/util_ldb.h"
#include "libcli/security/security.h"
#include "param/param.h"

/*
  these are used for the fallback local uid/gid to sid mapping
  code.
*/
#define SIDMAP_LOCAL_USER_BASE  0x80000000
#define SIDMAP_LOCAL_GROUP_BASE 0xC0000000
#define SIDMAP_MAX_LOCAL_UID    0x3fffffff
#define SIDMAP_MAX_LOCAL_GID    0x3fffffff

/*
  private context for sid mapping routines
*/
struct sidmap_context {
	struct ldb_context *samctx;
};

/*
  open a sidmap context - use talloc_free to close
*/
struct sidmap_context *sidmap_open(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx, 
				   struct loadparm_context *lp_ctx)
{
	struct sidmap_context *sidmap;
	sidmap = talloc(mem_ctx, struct sidmap_context);
	if (sidmap == NULL) {
		return NULL;
	}
	sidmap->samctx = samdb_connect(sidmap, ev_ctx, lp_ctx, system_session(sidmap, lp_ctx));
	if (sidmap->samctx == NULL) {
		talloc_free(sidmap);
		return NULL;
	}

	return sidmap;
}


/*
  check the sAMAccountType field of a search result to see if
  the account is a user account
*/
static bool is_user_account(struct ldb_message *res)
{
	uint_t atype = samdb_result_uint(res, "sAMAccountType", 0);
	if (atype && (!(atype & ATYPE_ACCOUNT))) {
		return false;
	}
	return true;
}

/*
  check the sAMAccountType field of a search result to see if
  the account is a group account
*/
static bool is_group_account(struct ldb_message *res)
{
	uint_t atype = samdb_result_uint(res, "sAMAccountType", 0);
	if (atype && atype == ATYPE_NORMAL_ACCOUNT) {
		return false;
	}
	return true;
}



/*
  return the dom_sid of our primary domain
*/
static NTSTATUS sidmap_primary_domain_sid(struct sidmap_context *sidmap, 
					  TALLOC_CTX *mem_ctx, struct dom_sid **sid)
{
	const char *attrs[] = { "objectSid", NULL };
	int ret;
	struct ldb_message **res = NULL;

	ret = gendb_search_dn(sidmap->samctx, mem_ctx, NULL, &res, attrs);
	if (ret != 1) {
		talloc_free(res);
		return NT_STATUS_NO_SUCH_DOMAIN;
	}
	
	*sid = samdb_result_dom_sid(mem_ctx, res[0], "objectSid");
	talloc_free(res);
	if (*sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}


/*
  map a sid to a unix uid
*/
NTSTATUS sidmap_sid_to_unixuid(struct sidmap_context *sidmap,
					const struct dom_sid *sid, uid_t *uid)
{
	const char *attrs[] = { "sAMAccountName", "uidNumber",
				"sAMAccountType", "unixName", NULL };
	int ret;
	const char *s;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	struct dom_sid *domain_sid;
	NTSTATUS status;

	tmp_ctx = talloc_new(sidmap);

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs,
			   "objectSid=%s",
			   ldap_encode_ndr_dom_sid(tmp_ctx, sid));

	if (ret != 1) {
		goto allocated_sid;
	}

	/* make sure its a user, not a group */
	if (!is_user_account(res[0])) {
		DEBUG(0,("sid_to_unixuid: sid %s is not an account!\n",
			 dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_SID;
	}

	/* first try to get the uid directly */
	s = samdb_result_string(res[0], "uidNumber", NULL);
	if (s != NULL) {
		*uid = strtoul(s, NULL, 0);
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* next try via the UnixName attribute */
	s = samdb_result_string(res[0], "unixName", NULL);
	if (s != NULL) {
		struct passwd *pwd = getpwnam(s);
		if (!pwd) {
			DEBUG(0,("unixName %s for sid %s does not exist as a local user\n", s, 
				 dom_sid_string(tmp_ctx, sid)));
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_SUCH_USER;
		}
		*uid = pwd->pw_uid;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* finally try via the sAMAccountName attribute */
	s = samdb_result_string(res[0], "sAMAccountName", NULL);
	if (s != NULL) {
		struct passwd *pwd = getpwnam(s);
		if (!pwd) {
			DEBUG(0,("sAMAccountName '%s' for sid %s does not exist as a local user\n", 
				 s, dom_sid_string(tmp_ctx, sid)));
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_SUCH_USER;
		}
		*uid = pwd->pw_uid;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}


allocated_sid:
	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (dom_sid_in_domain(domain_sid, sid)) {
		uint32_t rid = sid->sub_auths[sid->num_auths-1];
		if (rid >= SIDMAP_LOCAL_USER_BASE && 
		    rid <  SIDMAP_LOCAL_GROUP_BASE) {
			*uid = rid - SIDMAP_LOCAL_USER_BASE;
			talloc_free(tmp_ctx);
			return NT_STATUS_OK;
		}
	}
	

	DEBUG(0,("sid_to_unixuid: no uidNumber, unixName or sAMAccountName for sid %s\n", 
		 dom_sid_string(tmp_ctx, sid)));

	talloc_free(tmp_ctx);
	return NT_STATUS_NONE_MAPPED;
}


/*
  see if a sid is a group - very inefficient!
*/
bool sidmap_sid_is_group(struct sidmap_context *sidmap, struct dom_sid *sid)
{
	const char *attrs[] = { "sAMAccountType", NULL };
	int ret;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	NTSTATUS status;
	struct dom_sid *domain_sid;
	bool is_group;

	tmp_ctx = talloc_new(sidmap);

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "objectSid=%s", ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret == 1) {
		is_group = is_group_account(res[0]);
		talloc_free(tmp_ctx);
		return is_group;
	}

	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return false;
	}

	if (dom_sid_in_domain(domain_sid, sid)) {
		uint32_t rid = sid->sub_auths[sid->num_auths-1];
		if (rid >= SIDMAP_LOCAL_GROUP_BASE) {
			talloc_free(tmp_ctx);
			return true;
		}
	}

	talloc_free(tmp_ctx);
	return false;
}

/*
  map a sid to a unix gid
*/
NTSTATUS sidmap_sid_to_unixgid(struct sidmap_context *sidmap,
					const struct dom_sid *sid, gid_t *gid)
{
	const char *attrs[] = { "sAMAccountName", "gidNumber",
				"unixName", "sAMAccountType", NULL };
	int ret;
	const char *s;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	NTSTATUS status;
	struct dom_sid *domain_sid;

	tmp_ctx = talloc_new(sidmap);

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "objectSid=%s", ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret != 1) {
		goto allocated_sid;
	}

	/* make sure its not a user */
	if (!is_group_account(res[0])) {
		DEBUG(0,("sid_to_unixgid: sid %s is a ATYPE_NORMAL_ACCOUNT\n", 
			 dom_sid_string(tmp_ctx, sid)));
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_SID;
	}

	/* first try to get the gid directly */
	s = samdb_result_string(res[0], "gidNumber", NULL);
	if (s != NULL) {
		*gid = strtoul(s, NULL, 0);
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* next try via the UnixName attribute */
	s = samdb_result_string(res[0], "unixName", NULL);
	if (s != NULL) {
		struct group *grp = getgrnam(s);
		if (!grp) {
			DEBUG(0,("unixName '%s' for sid %s does not exist as a local group\n", 
				 s, dom_sid_string(tmp_ctx, sid)));
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_SUCH_GROUP;
		}
		*gid = grp->gr_gid;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

	/* finally try via the sAMAccountName attribute */
	s = samdb_result_string(res[0], "sAMAccountName", NULL);
	if (s != NULL) {
		struct group *grp = getgrnam(s);
		if (!grp) {
			DEBUG(0,("sAMAccountName '%s' for sid %s does not exist as a local group\n", s, dom_sid_string(tmp_ctx, sid)));
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_SUCH_GROUP;
		}
		*gid = grp->gr_gid;
		talloc_free(tmp_ctx);
		return NT_STATUS_OK;
	}

allocated_sid:
	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (dom_sid_in_domain(domain_sid, sid)) {
		uint32_t rid = sid->sub_auths[sid->num_auths-1];
		if (rid >= SIDMAP_LOCAL_GROUP_BASE) {
			*gid = rid - SIDMAP_LOCAL_GROUP_BASE;
			talloc_free(tmp_ctx);
			return NT_STATUS_OK;
		}
	}

	DEBUG(0,("sid_to_unixgid: no gidNumber, unixName or sAMAccountName for sid %s\n", 
		 dom_sid_string(tmp_ctx, sid)));

	talloc_free(tmp_ctx);
	return NT_STATUS_NONE_MAPPED;
}


/*
  map a unix uid to a dom_sid
  the returned sid is allocated in the supplied mem_ctx
*/
NTSTATUS sidmap_uid_to_sid(struct sidmap_context *sidmap,
				    TALLOC_CTX *mem_ctx,
				    const uid_t uid, struct dom_sid **sid)
{
	const char *attrs[] = { "sAMAccountName", "objectSid", "sAMAccountType", NULL };
	int ret, i;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	struct passwd *pwd;
	struct dom_sid *domain_sid;
	NTSTATUS status;

	/*
	  we search for the mapping in the following order:

	    - check if the uid is in the dynamic uid range assigned for winbindd
	      use. If it is, then look in winbindd sid mapping
	      database (not implemented yet)
	    - look for a user account in samdb that has uidNumber set to the
	      given uid
	    - look for a user account in samdb that has unixName or
	      sAMAccountName set to the name given by getpwuid()
	    - assign a SID by adding the uid to SIDMAP_LOCAL_USER_BASE in the local
	      domain
	*/


	tmp_ctx = talloc_new(mem_ctx);


	/*
	  step 2: look for a user account in samdb that has uidNumber set to the
                  given uid
	*/

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "uidNumber=%u", (unsigned int)uid);
	for (i=0;i<ret;i++) {
		if (!is_user_account(res[i])) continue;

		*sid = samdb_result_dom_sid(mem_ctx, res[i], "objectSid");
		talloc_free(tmp_ctx);
		NT_STATUS_HAVE_NO_MEMORY(*sid);
		return NT_STATUS_OK;
	}

	/*
	  step 3: look for a user account in samdb that has unixName
	          or sAMAccountName set to the name given by getpwuid()
	*/
	pwd = getpwuid(uid);
	if (pwd == NULL) {
		goto allocate_sid;
	}

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "(|(unixName=%s)(sAMAccountName=%s))", 
			   pwd->pw_name, pwd->pw_name);
	for (i=0;i<ret;i++) {
		if (!is_user_account(res[i])) continue;

		*sid = samdb_result_dom_sid(mem_ctx, res[i], "objectSid");
		talloc_free(tmp_ctx);
		NT_STATUS_HAVE_NO_MEMORY(*sid);
		return NT_STATUS_OK;
	}


	/*
	    step 4: assign a SID by adding the uid to
	            SIDMAP_LOCAL_USER_BASE in the local domain
	*/
allocate_sid:
	if (uid > SIDMAP_MAX_LOCAL_UID) {
		return NT_STATUS_NONE_MAPPED;
	}

	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	*sid = dom_sid_add_rid(mem_ctx, domain_sid, SIDMAP_LOCAL_USER_BASE + uid);
	talloc_free(tmp_ctx);

	if (*sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}


/*
  map a unix gid to a dom_sid
  the returned sid is allocated in the supplied mem_ctx
*/
NTSTATUS sidmap_gid_to_sid(struct sidmap_context *sidmap,
				    TALLOC_CTX *mem_ctx,
				    const gid_t gid, struct dom_sid **sid)
{
	const char *attrs[] = { "sAMAccountName", "objectSid", "sAMAccountType", NULL };
	int ret, i;
	TALLOC_CTX *tmp_ctx;
	struct ldb_message **res;
	struct group *grp;
	struct dom_sid *domain_sid;
	NTSTATUS status;

	/*
	  we search for the mapping in the following order:

	    - check if the gid is in the dynamic gid range assigned for winbindd
	      use. If it is, then look in winbindd sid mapping
	      database (not implemented yet)
	    - look for a group account in samdb that has gidNumber set to the
	      given gid
	    - look for a group account in samdb that has unixName or
	      sAMAccountName set to the name given by getgrgid()
	    - assign a SID by adding the gid to SIDMAP_LOCAL_GROUP_BASE in the local
	      domain
	*/


	tmp_ctx = talloc_new(sidmap);


	/*
	  step 2: look for a group account in samdb that has gidNumber set to the
                  given gid
	*/

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "gidNumber=%u", (unsigned int)gid);
	for (i=0;i<ret;i++) {
		if (!is_group_account(res[i])) continue;

		*sid = samdb_result_dom_sid(mem_ctx, res[i], "objectSid");
		talloc_free(tmp_ctx);
		NT_STATUS_HAVE_NO_MEMORY(*sid);
		return NT_STATUS_OK;
	}

	/*
	  step 3: look for a group account in samdb that has unixName
	          or sAMAccountName set to the name given by getgrgid()
	*/
	grp = getgrgid(gid);
	if (grp == NULL) {
		goto allocate_sid;
	}

	ret = gendb_search(sidmap->samctx, tmp_ctx, NULL, &res, attrs, 
			   "(|(unixName=%s)(sAMAccountName=%s))", 
			   grp->gr_name, grp->gr_name);
	for (i=0;i<ret;i++) {
		if (!is_group_account(res[i])) continue;

		*sid = samdb_result_dom_sid(mem_ctx, res[i], "objectSid");
		talloc_free(tmp_ctx);
		NT_STATUS_HAVE_NO_MEMORY(*sid);
		return NT_STATUS_OK;
	}


	/*
	    step 4: assign a SID by adding the gid to
	            SIDMAP_LOCAL_GROUP_BASE in the local domain
	*/
allocate_sid:
	if (gid > SIDMAP_MAX_LOCAL_GID) {
		return NT_STATUS_NONE_MAPPED;
	}

	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	*sid = dom_sid_add_rid(mem_ctx, domain_sid, SIDMAP_LOCAL_GROUP_BASE + gid);
	talloc_free(tmp_ctx);

	if (*sid == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/*
  check if a sid is in the range of auto-allocated SIDs from our primary domain,
  and if it is, then return the name and atype
*/
NTSTATUS sidmap_allocated_sid_lookup(struct sidmap_context *sidmap, 
					      TALLOC_CTX *mem_ctx, 
					      const struct dom_sid *sid,
					      const char **name,
					      enum lsa_SidType *rtype)
{
	NTSTATUS status;
	struct dom_sid *domain_sid;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	uint32_t rid, atype;
	
	status = sidmap_primary_domain_sid(sidmap, tmp_ctx, &domain_sid);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_SUCH_DOMAIN;
	}

	if (!dom_sid_in_domain(domain_sid, sid)) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NONE_MAPPED;
	}

	talloc_free(tmp_ctx);

	rid = sid->sub_auths[sid->num_auths-1];
	if (rid < SIDMAP_LOCAL_USER_BASE) {
		return NT_STATUS_NONE_MAPPED;
	}

	if (rid < SIDMAP_LOCAL_GROUP_BASE) {
		struct passwd *pwd;
		uid_t uid = rid - SIDMAP_LOCAL_USER_BASE;
		atype = ATYPE_NORMAL_ACCOUNT;
		*rtype = ds_atype_map(atype);

		pwd = getpwuid(uid);
		if (pwd == NULL) {
			*name = talloc_asprintf(mem_ctx, "uid%u", uid);
		} else {
			*name = talloc_strdup(mem_ctx, pwd->pw_name);
		}
	} else {
		struct group *grp;
		gid_t gid = rid - SIDMAP_LOCAL_GROUP_BASE;
		atype = ATYPE_LOCAL_GROUP;
		*rtype = ds_atype_map(atype);
		grp = getgrgid(gid);
		if (grp == NULL) {
			*name = talloc_asprintf(mem_ctx, "gid%u", gid);
		} else {
			*name = talloc_strdup(mem_ctx, grp->gr_name);
		}
	}

	if (*name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}
