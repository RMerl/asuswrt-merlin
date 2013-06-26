/* 
   Unix SMB/CIFS implementation.
   Winbind Utility functions

   Copyright (C) Gerald (Jerry) Carter   2007

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
#include "../libcli/security/security.h"
#include "../lib/util/util_pw.h"
#include "nsswitch/libwbclient/wbclient.h"

#if defined(WITH_WINBIND)

#include "lib/winbind_util.h"

struct passwd * winbind_getpwnam(const char * name)
{
	wbcErr result;
	struct passwd * tmp_pwd = NULL;
	struct passwd * pwd = NULL;

	result = wbcGetpwnam(name, &tmp_pwd);
	if (result != WBC_ERR_SUCCESS)
		return pwd;

	pwd = tcopy_passwd(talloc_tos(), tmp_pwd);

	wbcFreeMemory(tmp_pwd);

	return pwd;
}

struct passwd * winbind_getpwsid(const struct dom_sid *sid)
{
	wbcErr result;
	struct passwd * tmp_pwd = NULL;
	struct passwd * pwd = NULL;
	struct wbcDomainSid dom_sid;

	memcpy(&dom_sid, sid, sizeof(dom_sid));

	result = wbcGetpwsid(&dom_sid, &tmp_pwd);
	if (result != WBC_ERR_SUCCESS)
		return pwd;

	pwd = tcopy_passwd(talloc_tos(), tmp_pwd);

	wbcFreeMemory(tmp_pwd);

	return pwd;
}

/* Call winbindd to convert a name to a sid */

bool winbind_lookup_name(const char *dom_name, const char *name, struct dom_sid *sid,
                         enum lsa_SidType *name_type)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;
	enum wbcSidType type;	

	result = wbcLookupName(dom_name, name, &dom_sid, &type);
	if (result != WBC_ERR_SUCCESS)
		return false;

	memcpy(sid, &dom_sid, sizeof(struct dom_sid));
	*name_type = (enum lsa_SidType)type;	

	return true;	
}

/* Call winbindd to convert sid to name */

bool winbind_lookup_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			const char **domain, const char **name,
                        enum lsa_SidType *name_type)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;
	enum wbcSidType type;
	char *domain_name = NULL;
	char *account_name = NULL;

	memcpy(&dom_sid, sid, sizeof(dom_sid));	

	result = wbcLookupSid(&dom_sid, &domain_name, &account_name, &type);
	if (result != WBC_ERR_SUCCESS)
		return false;

	/* Copy out result */

	if (domain) {		
		*domain = talloc_strdup(mem_ctx, domain_name);
	}
	if (name) {
		*name = talloc_strdup(mem_ctx, account_name);
	}
	*name_type = (enum lsa_SidType)type;

	DEBUG(10, ("winbind_lookup_sid: SUCCESS: SID %s -> %s %s\n", 
		   sid_string_dbg(sid), domain_name, account_name));

	wbcFreeMemory(domain_name);
	wbcFreeMemory(account_name);

	if ((domain && !*domain) || (name && !*name)) {		
		DEBUG(0,("winbind_lookup_sid: talloc() failed!\n"));
		return false;
	}	


	return true;
}

/* Ping winbindd to see it is alive */

bool winbind_ping(void)
{
	wbcErr result = wbcPing();

	return (result == WBC_ERR_SUCCESS);
}

/* Call winbindd to convert SID to uid */

bool winbind_sid_to_uid(uid_t *puid, const struct dom_sid *sid)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;

	memcpy(&dom_sid, sid, sizeof(dom_sid));	

	result = wbcSidToUid(&dom_sid, puid);	

	return (result == WBC_ERR_SUCCESS);	
}

/* Call winbindd to convert uid to sid */

bool winbind_uid_to_sid(struct dom_sid *sid, uid_t uid)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;

	result = wbcUidToSid(uid, &dom_sid);
	if (result == WBC_ERR_SUCCESS) {
		memcpy(sid, &dom_sid, sizeof(struct dom_sid));
	} else {
		sid_copy(sid, &global_sid_NULL);
	}

	return (result == WBC_ERR_SUCCESS);
}

/* Call winbindd to convert SID to gid */

bool winbind_sid_to_gid(gid_t *pgid, const struct dom_sid *sid)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;

	memcpy(&dom_sid, sid, sizeof(dom_sid));	

	result = wbcSidToGid(&dom_sid, pgid);	

	return (result == WBC_ERR_SUCCESS);	
}

/* Call winbindd to convert gid to sid */

bool winbind_gid_to_sid(struct dom_sid *sid, gid_t gid)
{
	struct wbcDomainSid dom_sid;
	wbcErr result;

	result = wbcGidToSid(gid, &dom_sid);
	if (result == WBC_ERR_SUCCESS) {
		memcpy(sid, &dom_sid, sizeof(struct dom_sid));
	} else {
		sid_copy(sid, &global_sid_NULL);
	}

	return (result == WBC_ERR_SUCCESS);
}

/* Check for a trusted domain */

wbcErr wb_is_trusted_domain(const char *domain)
{
	wbcErr result;
	struct wbcDomainInfo *info = NULL;

	result = wbcDomainInfo(domain, &info);

	if (WBC_ERROR_IS_OK(result)) {
		wbcFreeMemory(info);
	}

	return result;	
}

/* Lookup a set of rids in a given domain */

bool winbind_lookup_rids(TALLOC_CTX *mem_ctx,
			 const struct dom_sid *domain_sid,
			 int num_rids, uint32 *rids,
			 const char **domain_name,
			 const char ***names, enum lsa_SidType **types)
{
	const char *dom_name = NULL;
	const char **namelist = NULL;
	enum wbcSidType *name_types = NULL;
	struct wbcDomainSid dom_sid;
	wbcErr ret;
	int i;	

	memcpy(&dom_sid, domain_sid, sizeof(struct wbcDomainSid));

	ret = wbcLookupRids(&dom_sid, num_rids, rids,
			    &dom_name, &namelist, &name_types);
	if (ret != WBC_ERR_SUCCESS) {		
		return false;
	}	

	*domain_name = talloc_strdup(mem_ctx, dom_name);
	*names       = TALLOC_ARRAY(mem_ctx, const char*, num_rids);
	*types       = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_rids);

	for(i=0; i<num_rids; i++) {
		(*names)[i] = talloc_strdup(*names, namelist[i]);
		(*types)[i] = (enum lsa_SidType)name_types[i];
	}

	wbcFreeMemory(CONST_DISCARD(char*, dom_name));
	wbcFreeMemory(namelist);
	wbcFreeMemory(name_types);

	return true;	
}

/* Ask Winbind to allocate a new uid for us */

bool winbind_allocate_uid(uid_t *uid)
{
	wbcErr ret;

	ret = wbcAllocateUid(uid);

	return (ret == WBC_ERR_SUCCESS);
}

/* Ask Winbind to allocate a new gid for us */

bool winbind_allocate_gid(gid_t *gid)
{
	wbcErr ret;

	ret = wbcAllocateGid(gid);

	return (ret == WBC_ERR_SUCCESS);
}

bool winbind_get_groups(TALLOC_CTX * mem_ctx, const char *account, uint32_t *num_groups, gid_t **_groups)
{
	wbcErr ret;
	uint32_t ngroups;
	gid_t *group_list = NULL;

	ret = wbcGetGroups(account, &ngroups, &group_list);
	if (ret != WBC_ERR_SUCCESS)
		return false;

	*_groups = TALLOC_ARRAY(mem_ctx, gid_t, ngroups);
	if (*_groups == NULL) {
	    wbcFreeMemory(group_list);
	    return false;
	}

	memcpy(*_groups, group_list, ngroups* sizeof(gid_t));
	*num_groups = ngroups;

	wbcFreeMemory(group_list);
	return true;
}

bool winbind_get_sid_aliases(TALLOC_CTX *mem_ctx,
			     const struct dom_sid *dom_sid,
			     const struct dom_sid *members,
			     size_t num_members,
			     uint32_t **pp_alias_rids,
			     size_t *p_num_alias_rids)
{
	wbcErr ret;
	struct wbcDomainSid domain_sid;
	struct wbcDomainSid *sid_list = NULL;
	size_t i;
	uint32_t * rids;
	uint32_t num_rids;

	memcpy(&domain_sid, dom_sid, sizeof(*dom_sid));

	sid_list = TALLOC_ARRAY(mem_ctx, struct wbcDomainSid, num_members);

	for (i=0; i < num_members; i++) {
	    memcpy(&sid_list[i], &members[i], sizeof(sid_list[i]));
	}

	ret = wbcGetSidAliases(&domain_sid,
			       sid_list,
			       num_members,
			       &rids,
			       &num_rids);
	if (ret != WBC_ERR_SUCCESS) {
		return false;
	}

	*pp_alias_rids = TALLOC_ARRAY(mem_ctx, uint32_t, num_rids);
	if (*pp_alias_rids == NULL) {
		wbcFreeMemory(rids);
		return false;
	}

	memcpy(*pp_alias_rids, rids, sizeof(uint32_t) * num_rids);

	*p_num_alias_rids = num_rids;
	wbcFreeMemory(rids);

	return true;
}

#else      /* WITH_WINBIND */

struct passwd * winbind_getpwnam(const char * name)
{
	return NULL;
}

struct passwd * winbind_getpwsid(const struct dom_sid *sid)
{
	return NULL;
}

bool winbind_lookup_name(const char *dom_name, const char *name, struct dom_sid *sid,
                         enum lsa_SidType *name_type)
{
	return false;
}

/* Call winbindd to convert sid to name */

bool winbind_lookup_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			const char **domain, const char **name,
                        enum lsa_SidType *name_type)
{
	return false;
}

/* Ping winbindd to see it is alive */

bool winbind_ping(void)
{
	return false;
}

/* Call winbindd to convert SID to uid */

bool winbind_sid_to_uid(uid_t *puid, const struct dom_sid *sid)
{
	return false;
}

/* Call winbindd to convert uid to sid */

bool winbind_uid_to_sid(struct dom_sid *sid, uid_t uid)
{
	return false;
}

/* Call winbindd to convert SID to gid */

bool winbind_sid_to_gid(gid_t *pgid, const struct dom_sid *sid)
{
	return false;	
}

/* Call winbindd to convert gid to sid */

bool winbind_gid_to_sid(struct dom_sid *sid, gid_t gid)
{
	return false;
}

/* Check for a trusted domain */

wbcErr wb_is_trusted_domain(const char *domain)
{
	return WBC_ERR_UNKNOWN_FAILURE;
}

/* Lookup a set of rids in a given domain */

bool winbind_lookup_rids(TALLOC_CTX *mem_ctx,
			 const struct dom_sid *domain_sid,
			 int num_rids, uint32 *rids,
			 const char **domain_name,
			 const char ***names, enum lsa_SidType **types)
{
	return false;
}

/* Ask Winbind to allocate a new uid for us */

bool winbind_allocate_uid(uid_t *uid)
{
	return false;
}

/* Ask Winbind to allocate a new gid for us */

bool winbind_allocate_gid(gid_t *gid)
{
	return false;
}

bool winbind_get_groups(TALLOC_CTX *mem_ctx, const char *account, uint32_t *num_groups, gid_t **_groups)
{
	return false;
}

bool winbind_get_sid_aliases(TALLOC_CTX *mem_ctx,
			     const struct dom_sid *dom_sid,
			     const struct dom_sid *members,
			     size_t num_members,
			     uint32_t **pp_alias_rids,
			     size_t *p_num_alias_rids)
{
	return false;
}

#endif     /* WITH_WINBIND */
