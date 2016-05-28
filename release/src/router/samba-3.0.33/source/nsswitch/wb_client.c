/* 
   Unix SMB/CIFS implementation.

   winbind client code

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Tridgell 2000
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA  02111-1307, USA.   
*/

#include "includes.h"
#include "nsswitch/winbind_nss.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

NSS_STATUS winbindd_request_response(int req_type,
                                 struct winbindd_request *request,
                                 struct winbindd_response *response);

/* Call winbindd to convert a name to a sid */

BOOL winbind_lookup_name(const char *dom_name, const char *name, DOM_SID *sid, 
                         enum lsa_SidType *name_type)
{
	struct winbindd_request request;
	struct winbindd_response response;
	NSS_STATUS result;
	
	if (!sid || !name_type)
		return False;

	/* Send off request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.data.name.dom_name, dom_name);
	fstrcpy(request.data.name.name, name);

	if ((result = winbindd_request_response(WINBINDD_LOOKUPNAME, &request, 
				       &response)) == NSS_STATUS_SUCCESS) {
		if (!string_to_sid(sid, response.data.sid.sid))
			return False;
		*name_type = (enum lsa_SidType)response.data.sid.type;
	}

	return result == NSS_STATUS_SUCCESS;
}

/* Call winbindd to convert sid to name */

BOOL winbind_lookup_sid(TALLOC_CTX *mem_ctx, const DOM_SID *sid, 
			const char **domain, const char **name,
                        enum lsa_SidType *name_type)
{
	struct winbindd_request request;
	struct winbindd_response response;
	NSS_STATUS result;
	
	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.data.sid, sid_string_static(sid));
	
	/* Make request */

	result = winbindd_request_response(WINBINDD_LOOKUPSID, &request,
					   &response);

	if (result != NSS_STATUS_SUCCESS) {
		return False;
	}

	/* Copy out result */

	if (domain != NULL) {
		*domain = talloc_strdup(mem_ctx, response.data.name.dom_name);
		if (*domain == NULL) {
			DEBUG(0, ("talloc failed\n"));
			return False;
		}
	}
	if (name != NULL) {
		*name = talloc_strdup(mem_ctx, response.data.name.name);
		if (*name == NULL) {
			DEBUG(0, ("talloc failed\n"));
			return False;
		}
	}

	*name_type = (enum lsa_SidType)response.data.name.type;

	DEBUG(10, ("winbind_lookup_sid: SUCCESS: SID %s -> %s %s\n", 
		   sid_string_static(sid), response.data.name.dom_name,
		   response.data.name.name));
	return True;
}

BOOL winbind_lookup_rids(TALLOC_CTX *mem_ctx,
			 const DOM_SID *domain_sid,
			 int num_rids, uint32 *rids,
			 const char **domain_name,
			 const char ***names, enum lsa_SidType **types)
{
	size_t i, buflen;
	ssize_t len;
	char *ridlist;
	char *p;
	struct winbindd_request request;
	struct winbindd_response response;
	NSS_STATUS result;

	if (num_rids == 0) {
		return False;
	}

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.data.sid, sid_string_static(domain_sid));
	
	len = 0;
	buflen = 0;
	ridlist = NULL;

	for (i=0; i<num_rids; i++) {
		sprintf_append(mem_ctx, &ridlist, &len, &buflen,
			       "%ld\n", rids[i]);
	}

	if ((num_rids != 0) && (ridlist == NULL)) {
		return False;
	}

	request.extra_data.data = ridlist;
	request.extra_len = strlen(ridlist)+1;

	result = winbindd_request_response(WINBINDD_LOOKUPRIDS,
					   &request, &response);

	TALLOC_FREE(ridlist);

	if (result != NSS_STATUS_SUCCESS) {
		return False;
	}

	*domain_name = talloc_strdup(mem_ctx, response.data.domain_name);

	if (num_rids) {
		*names = TALLOC_ARRAY(mem_ctx, const char *, num_rids);
		*types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_rids);

		if ((*names == NULL) || (*types == NULL)) {
			goto fail;
		}
	} else {
		*names = NULL;
		*types = NULL;
	}

	p = (char *)response.extra_data.data;

	for (i=0; i<num_rids; i++) {
		char *q;

		if (*p == '\0') {
			DEBUG(10, ("Got invalid reply: %s\n",
				   (char *)response.extra_data.data));
			goto fail;
		}
			
		(*types)[i] = (enum lsa_SidType)strtoul(p, &q, 10);

		if (*q != ' ') {
			DEBUG(10, ("Got invalid reply: %s\n",
				   (char *)response.extra_data.data));
			goto fail;
		}

		p = q+1;

		q = strchr(p, '\n');
		if (q == NULL) {
			DEBUG(10, ("Got invalid reply: %s\n",
				   (char *)response.extra_data.data));
			goto fail;
		}

		*q = '\0';

		(*names)[i] = talloc_strdup(*names, p);

		p = q+1;
	}

	if (*p != '\0') {
		DEBUG(10, ("Got invalid reply: %s\n",
			   (char *)response.extra_data.data));
		goto fail;
	}

	SAFE_FREE(response.extra_data.data);

	return True;

 fail:
	TALLOC_FREE(*names);
	TALLOC_FREE(*types);
	return False;
}

/* Call winbindd to convert SID to uid */

BOOL winbind_sid_to_uid(uid_t *puid, const DOM_SID *sid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;
	fstring sid_str;

	if (!puid)
		return False;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	sid_to_string(sid_str, sid);
	fstrcpy(request.data.sid, sid_str);
	
	/* Make request */

	result = winbindd_request_response(WINBINDD_SID_TO_UID, &request, &response);

	/* Copy out result */

	if (result == NSS_STATUS_SUCCESS) {
		*puid = response.data.uid;
	}

	return (result == NSS_STATUS_SUCCESS);
}

/* Call winbindd to convert uid to sid */

BOOL winbind_uid_to_sid(DOM_SID *sid, uid_t uid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	if (!sid)
		return False;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.uid = uid;

	/* Make request */

	result = winbindd_request_response(WINBINDD_UID_TO_SID, &request, &response);

	/* Copy out result */

	if (result == NSS_STATUS_SUCCESS) {
		if (!string_to_sid(sid, response.data.sid.sid))
			return False;
	} else {
		sid_copy(sid, &global_sid_NULL);
	}

	return (result == NSS_STATUS_SUCCESS);
}

/* Call winbindd to convert SID to gid */

BOOL winbind_sid_to_gid(gid_t *pgid, const DOM_SID *sid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;
	fstring sid_str;

	if (!pgid)
		return False;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	sid_to_string(sid_str, sid);
	fstrcpy(request.data.sid, sid_str);
	
	/* Make request */

	result = winbindd_request_response(WINBINDD_SID_TO_GID, &request, &response);

	/* Copy out result */

	if (result == NSS_STATUS_SUCCESS) {
		*pgid = response.data.gid;
	}

	return (result == NSS_STATUS_SUCCESS);
}

/* Call winbindd to convert gid to sid */

BOOL winbind_gid_to_sid(DOM_SID *sid, gid_t gid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	if (!sid)
		return False;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.data.gid = gid;

	/* Make request */

	result = winbindd_request_response(WINBINDD_GID_TO_SID, &request, &response);

	/* Copy out result */

	if (result == NSS_STATUS_SUCCESS) {
		if (!string_to_sid(sid, response.data.sid.sid))
			return False;
	} else {
		sid_copy(sid, &global_sid_NULL);
	}

	return (result == NSS_STATUS_SUCCESS);
}

/* Call winbindd to convert SID to uid */

BOOL winbind_sids_to_unixids(struct id_map *ids, int num_ids)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;
	DOM_SID *sids;
	int i;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.extra_len = num_ids * sizeof(DOM_SID);

	sids = (DOM_SID *)SMB_MALLOC(request.extra_len);
	for (i = 0; i < num_ids; i++) {
		sid_copy(&sids[i], ids[i].sid);
	}

	request.extra_data.data = (char *)sids;
	
	/* Make request */

	result = winbindd_request_response(WINBINDD_SIDS_TO_XIDS, &request, &response);

	/* Copy out result */

	if (result == NSS_STATUS_SUCCESS) {
		struct unixid *wid = (struct unixid *)response.extra_data.data;
		
		for (i = 0; i < num_ids; i++) {
			if (wid[i].type == -1) {
				ids[i].status = ID_UNMAPPED;
			} else {
				ids[i].status = ID_MAPPED;
				ids[i].xid.type = wid[i].type;
				ids[i].xid.id = wid[i].id;
			}
		}
	}

	SAFE_FREE(request.extra_data.data);
	SAFE_FREE(response.extra_data.data);

	return (result == NSS_STATUS_SUCCESS);
}

BOOL winbind_idmap_dump_maps(TALLOC_CTX *memctx, const char *file)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	request.extra_data.data = SMB_STRDUP(file);
	request.extra_len = strlen(request.extra_data.data) + 1;

	result = winbindd_request_response(WINBINDD_DUMP_MAPS, &request, &response);

	SAFE_FREE(request.extra_data.data);
	return (result == NSS_STATUS_SUCCESS);
}

BOOL winbind_allocate_uid(uid_t *uid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	result = winbindd_request_response(WINBINDD_ALLOCATE_UID,
					   &request, &response);

	if (result != NSS_STATUS_SUCCESS)
		return False;

	/* Copy out result */
	*uid = response.data.uid;

	return True;
}

BOOL winbind_allocate_gid(gid_t *gid)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	result = winbindd_request_response(WINBINDD_ALLOCATE_GID,
					   &request, &response);

	if (result != NSS_STATUS_SUCCESS)
		return False;

	/* Copy out result */
	*gid = response.data.gid;

	return True;
}

BOOL winbind_set_mapping(const struct id_map *map)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	request.data.dual_idmapset.id = map->xid.id;
	request.data.dual_idmapset.type = map->xid.type;
	sid_to_string(request.data.dual_idmapset.sid, map->sid);

	result = winbindd_request_response(WINBINDD_SET_MAPPING, &request, &response);

	return (result == NSS_STATUS_SUCCESS);
}

BOOL winbind_set_uid_hwm(unsigned long id)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	request.data.dual_idmapset.id = id;
	request.data.dual_idmapset.type = ID_TYPE_UID;

	result = winbindd_request_response(WINBINDD_SET_HWM, &request, &response);

	return (result == NSS_STATUS_SUCCESS);
}

BOOL winbind_set_gid_hwm(unsigned long id)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int result;

	/* Initialise request */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	/* Make request */

	request.data.dual_idmapset.id = id;
	request.data.dual_idmapset.type = ID_TYPE_GID;

	result = winbindd_request_response(WINBINDD_SET_HWM, &request, &response);

	return (result == NSS_STATUS_SUCCESS);
}

/**********************************************************************
 simple wrapper function to see if winbindd is alive
**********************************************************************/

BOOL winbind_ping( void )
{
	NSS_STATUS result;

	result = winbindd_request_response(WINBINDD_PING, NULL, NULL);

	return result == NSS_STATUS_SUCCESS;
}

/**********************************************************************
 Is a domain trusted?

 result == NSS_STATUS_UNAVAIL: winbind not around
 result == NSS_STATUS_NOTFOUND: winbind around, but domain missing

 Due to a bad API NSS_STATUS_NOTFOUND is returned both when winbind_off and
 when winbind return WINBINDD_ERROR. So the semantics of this routine depends
 on winbind_on. Grepping for winbind_off I just found 3 places where winbind
 is turned off, and this does not conflict (as far as I have seen) with the
 callers of is_trusted_domains.

 I *hate* global variables....

 Volker

**********************************************************************/

NSS_STATUS wb_is_trusted_domain(const char *domain)
{
	struct winbindd_request request;
	struct winbindd_response response;

	/* Call winbindd */

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	fstrcpy(request.domain_name, domain);

	return winbindd_request_response(WINBINDD_DOMAIN_INFO, &request, &response);
}
