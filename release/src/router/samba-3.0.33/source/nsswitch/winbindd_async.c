/* 
   Unix SMB/CIFS implementation.

   Async helpers for blocking functions

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Gerald Carter 2006
   
   The helpers always consist of three functions: 

   * A request setup function that takes the necessary parameters together
     with a continuation function that is to be called upon completion

   * A private continuation function that is internal only. This is to be
     called by the lower-level functions in do_async(). Its only task is to
     properly call the continuation function named above.

   * A worker function that is called inside the appropriate child process.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

struct do_async_state {
	TALLOC_CTX *mem_ctx;
	struct winbindd_request request;
	struct winbindd_response response;
	void (*cont)(TALLOC_CTX *mem_ctx,
		     BOOL success,
		     struct winbindd_response *response,
		     void *c, void *private_data);
	void *c, *private_data;
};

static void do_async_recv(void *private_data, BOOL success)
{
	struct do_async_state *state =
		talloc_get_type_abort(private_data, struct do_async_state);

	state->cont(state->mem_ctx, success, &state->response,
		    state->c, state->private_data);
}

static void do_async(TALLOC_CTX *mem_ctx, struct winbindd_child *child,
		     const struct winbindd_request *request,
		     void (*cont)(TALLOC_CTX *mem_ctx, BOOL success,
				  struct winbindd_response *response,
				  void *c, void *private_data),
		     void *c, void *private_data)
{
	struct do_async_state *state;

	state = TALLOC_ZERO_P(mem_ctx, struct do_async_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		cont(mem_ctx, False, NULL, c, private_data);
		return;
	}

	state->mem_ctx = mem_ctx;
	state->request = *request;
	state->request.length = sizeof(state->request);
	state->cont = cont;
	state->c = c;
	state->private_data = private_data;

	async_request(mem_ctx, child, &state->request,
		      &state->response, do_async_recv, state);
}

void do_async_domain(TALLOC_CTX *mem_ctx, struct winbindd_domain *domain,
		     const struct winbindd_request *request,
		     void (*cont)(TALLOC_CTX *mem_ctx, BOOL success,
				  struct winbindd_response *response,
				  void *c, void *private_data),
		     void *c, void *private_data)
{
	struct do_async_state *state;

	state = TALLOC_ZERO_P(mem_ctx, struct do_async_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		cont(mem_ctx, False, NULL, c, private_data);
		return;
	}

	state->mem_ctx = mem_ctx;
	state->request = *request;
	state->request.length = sizeof(state->request);
	state->cont = cont;
	state->c = c;
	state->private_data = private_data;

	async_domain_request(mem_ctx, domain, &state->request,
			     &state->response, do_async_recv, state);
}

static void winbindd_set_mapping_recv(TALLOC_CTX *mem_ctx, BOOL success,
				   struct winbindd_response *response,
				   void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ) = (void (*)(void *, BOOL))c;

	if (!success) {
		DEBUG(5, ("Could not trigger idmap_set_mapping\n"));
		cont(private_data, False);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("idmap_set_mapping returned an error\n"));
		cont(private_data, False);
		return;
	}

	cont(private_data, True);
}

void winbindd_set_mapping_async(TALLOC_CTX *mem_ctx, const struct id_map *map,
			     void (*cont)(void *private_data, BOOL success),
			     void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_SET_MAPPING;
	request.data.dual_idmapset.id = map->xid.id;
	request.data.dual_idmapset.type = map->xid.type;
	sid_to_string(request.data.dual_idmapset.sid, map->sid);

	do_async(mem_ctx, idmap_child(), &request, winbindd_set_mapping_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_set_mapping(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct id_map map;
	DOM_SID sid;
	NTSTATUS result;

	DEBUG(3, ("[%5lu]: dual_idmapset\n", (unsigned long)state->pid));

	if (!string_to_sid(&sid, state->request.data.dual_idmapset.sid))
		return WINBINDD_ERROR;

	map.sid = &sid;
	map.xid.id = state->request.data.dual_idmapset.id;
	map.xid.type = state->request.data.dual_idmapset.type;
	map.status = ID_MAPPED;

	result = idmap_set_mapping(&map);
	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

static void winbindd_set_hwm_recv(TALLOC_CTX *mem_ctx, BOOL success,
				   struct winbindd_response *response,
				   void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ) = (void (*)(void *, BOOL))c;

	if (!success) {
		DEBUG(5, ("Could not trigger idmap_set_hwm\n"));
		cont(private_data, False);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("idmap_set_hwm returned an error\n"));
		cont(private_data, False);
		return;
	}

	cont(private_data, True);
}

void winbindd_set_hwm_async(TALLOC_CTX *mem_ctx, const struct unixid *xid,
			     void (*cont)(void *private_data, BOOL success),
			     void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_SET_HWM;
	request.data.dual_idmapset.id = xid->id;
	request.data.dual_idmapset.type = xid->type;

	do_async(mem_ctx, idmap_child(), &request, winbindd_set_hwm_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_set_hwm(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct unixid xid;
	NTSTATUS result;

	DEBUG(3, ("[%5lu]: dual_set_hwm\n", (unsigned long)state->pid));

	xid.id = state->request.data.dual_idmapset.id;
	xid.type = state->request.data.dual_idmapset.type;

	switch (xid.type) {
	case ID_TYPE_UID:
		result = idmap_set_uid_hwm(&xid);
		break;
	case ID_TYPE_GID:
		result = idmap_set_gid_hwm(&xid);
		break;
	default:
		return WINBINDD_ERROR;
	}
	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

static void winbindd_sids2xids_recv(TALLOC_CTX *mem_ctx, BOOL success,
			       struct winbindd_response *response,
			       void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, void *, int) =
		(void (*)(void *, BOOL, void *, int))c;

	if (!success) {
		DEBUG(5, ("Could not trigger sids2xids\n"));
		cont(private_data, False, NULL, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("sids2xids returned an error\n"));
		cont(private_data, False, NULL, 0);
		return;
	}

	cont(private_data, True, response->extra_data.data, response->length - sizeof(response));
}
			 
void winbindd_sids2xids_async(TALLOC_CTX *mem_ctx, void *sids, int size,
			 void (*cont)(void *private_data, BOOL success, void *data, int len),
			 void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_SIDS2XIDS;
	request.extra_data.data = (char *)sids;
	request.extra_len = size;
	do_async(mem_ctx, idmap_child(), &request, winbindd_sids2xids_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_sids2xids(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DOM_SID *sids;
	struct unixid *xids;
	struct id_map **ids;
	NTSTATUS result;
	int num, i;

	DEBUG(3, ("[%5lu]: sids to unix ids\n", (unsigned long)state->pid));

	if (state->request.extra_len == 0) {
		DEBUG(0, ("Invalid buffer size!\n"));
		return WINBINDD_ERROR;
	}

	sids = (DOM_SID *)state->request.extra_data.data;
	num = state->request.extra_len / sizeof(DOM_SID);

	ids = TALLOC_ZERO_ARRAY(state->mem_ctx, struct id_map *, num + 1);
	if ( ! ids) {
		DEBUG(0, ("Out of memory!\n"));
		return WINBINDD_ERROR;
	}
	for (i = 0; i < num; i++) {
		ids[i] = TALLOC_P(ids, struct id_map);
		if ( ! ids[i]) {
			DEBUG(0, ("Out of memory!\n"));
			talloc_free(ids);
			return WINBINDD_ERROR;
		}
		ids[i]->sid = &sids[i];
	}

	result = idmap_sids_to_unixids(ids);

	if (NT_STATUS_IS_OK(result)) {

		xids = SMB_MALLOC_ARRAY(struct unixid, num);
		if ( ! xids) {
			DEBUG(0, ("Out of memory!\n"));
			talloc_free(ids);
			return WINBINDD_ERROR;
		}
		
		for (i = 0; i < num; i++) {
			if (ids[i]->status == ID_MAPPED) {
				xids[i].type = ids[i]->xid.type;
				xids[i].id = ids[i]->xid.id;
			} else {
				xids[i].type = -1;
			}
		}

		state->response.length = sizeof(state->response) + (sizeof(struct unixid) * num);
		state->response.extra_data.data = xids;

	} else {
		DEBUG (2, ("idmap_sids_to_unixids returned an error: 0x%08x\n", NT_STATUS_V(result)));
		talloc_free(ids);
		return WINBINDD_ERROR;
	}

	talloc_free(ids);
	return WINBINDD_OK;
}

static void winbindd_sid2uid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			       struct winbindd_response *response,
			       void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, uid_t uid) =
		(void (*)(void *, BOOL, uid_t))c;

	if (!success) {
		DEBUG(5, ("Could not trigger sid2uid\n"));
		cont(private_data, False, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("sid2uid returned an error\n"));
		cont(private_data, False, 0);
		return;
	}

	cont(private_data, True, response->data.uid);
}
			 
void winbindd_sid2uid_async(TALLOC_CTX *mem_ctx, const DOM_SID *sid,
			 void (*cont)(void *private_data, BOOL success, uid_t uid),
			 void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_SID2UID;
	sid_to_string(request.data.dual_sid2id.sid, sid);
	do_async(mem_ctx, idmap_child(), &request, winbindd_sid2uid_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_sid2uid(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DOM_SID sid;
	NTSTATUS result;

	DEBUG(3, ("[%5lu]: sid to uid %s\n", (unsigned long)state->pid,
		  state->request.data.dual_sid2id.sid));

	if (!string_to_sid(&sid, state->request.data.dual_sid2id.sid)) {
		DEBUG(1, ("Could not get convert sid %s from string\n",
			  state->request.data.dual_sid2id.sid));
		return WINBINDD_ERROR;
	}

	/* Find uid for this sid and return it, possibly ask the slow remote idmap */

	result = idmap_sid_to_uid(&sid, &(state->response.data.uid));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

#if 0	/* not used */
static void uid2name_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data);

void winbindd_uid2name_async(TALLOC_CTX *mem_ctx, uid_t uid,
			     void (*cont)(void *private_data, BOOL success,
					  const char *name),
			     void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_UID2NAME;
	request.data.uid = uid;
	do_async(mem_ctx, idmap_child(), &request, uid2name_recv,
		 (void *)cont, private_data);
}
#endif	/* not used */

enum winbindd_result winbindd_dual_uid2name(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct passwd *pw;

	DEBUG(3, ("[%5lu]: uid2name %lu\n", (unsigned long)state->pid, 
		  (unsigned long)state->request.data.uid));

	pw = getpwuid(state->request.data.uid);
	if (pw == NULL) {
		DEBUG(5, ("User %lu not found\n",
			  (unsigned long)state->request.data.uid));
		return WINBINDD_ERROR;
	}

	fstrcpy(state->response.data.name.name, pw->pw_name);
	return WINBINDD_OK;
}

#if 0	/* not used */
static void uid2name_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *name) =
		(void (*)(void *, BOOL, const char *))c;

	if (!success) {
		DEBUG(5, ("Could not trigger uid2name\n"));
		cont(private_data, False, NULL);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("uid2name returned an error\n"));
		cont(private_data, False, NULL);
		return;
	}

	cont(private_data, True, response->data.name.name);
}

static void name2uid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data);

static void winbindd_name2uid_async(TALLOC_CTX *mem_ctx, const char *name,
				    void (*cont)(void *private_data, BOOL success,
						 uid_t uid),
				    void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_NAME2UID;
	fstrcpy(request.data.username, name);
	do_async(mem_ctx, idmap_child(), &request, name2uid_recv,
		 (void *)cont, private_data);
}
#endif	/* not used */

enum winbindd_result winbindd_dual_name2uid(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct passwd *pw;

	/* Ensure null termination */
	state->request.data.username
		[sizeof(state->request.data.username)-1] = '\0';

	DEBUG(3, ("[%5lu]: name2uid %s\n", (unsigned long)state->pid, 
		  state->request.data.username));

	pw = getpwnam(state->request.data.username);
	if (pw == NULL) {
		return WINBINDD_ERROR;
	}

	state->response.data.uid = pw->pw_uid;
	return WINBINDD_OK;
}

#if 0	/* not used */
static void name2uid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, uid_t uid) =
		(void (*)(void *, BOOL, uid_t))c;

	if (!success) {
		DEBUG(5, ("Could not trigger name2uid\n"));
		cont(private_data, False, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("name2uid returned an error\n"));
		cont(private_data, False, 0);
		return;
	}

	cont(private_data, True, response->data.uid);
}
#endif	/* not used */

static void winbindd_sid2gid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			       struct winbindd_response *response,
			       void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, gid_t gid) =
		(void (*)(void *, BOOL, gid_t))c;

	if (!success) {
		DEBUG(5, ("Could not trigger sid2gid\n"));
		cont(private_data, False, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("sid2gid returned an error\n"));
		cont(private_data, False, 0);
		return;
	}

	cont(private_data, True, response->data.gid);
}
			 
void winbindd_sid2gid_async(TALLOC_CTX *mem_ctx, const DOM_SID *sid,
			 void (*cont)(void *private_data, BOOL success, gid_t gid),
			 void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_SID2GID;
	sid_to_string(request.data.dual_sid2id.sid, sid);

	DEBUG(7,("winbindd_sid2gid_async: Resolving %s to a gid\n", 
		request.data.dual_sid2id.sid));

	do_async(mem_ctx, idmap_child(), &request, winbindd_sid2gid_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_sid2gid(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DOM_SID sid;
	NTSTATUS result;

	DEBUG(3, ("[%5lu]: sid to gid %s\n", (unsigned long)state->pid,
		  state->request.data.dual_sid2id.sid));

	if (!string_to_sid(&sid, state->request.data.dual_sid2id.sid)) {
		DEBUG(1, ("Could not get convert sid %s from string\n",
			  state->request.data.dual_sid2id.sid));
		return WINBINDD_ERROR;
	}

	/* Find gid for this sid and return it, possibly ask the slow remote idmap */

	result = idmap_sid_to_gid(&sid, &state->response.data.gid);
	
	DEBUG(10, ("winbindd_dual_sid2gid: 0x%08x - %s - %u\n", NT_STATUS_V(result), sid_string_static(&sid), state->response.data.gid));

	return NT_STATUS_IS_OK(result) ? WINBINDD_OK : WINBINDD_ERROR;
}

static void gid2name_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *name) =
		(void (*)(void *, BOOL, const char *))c;

	if (!success) {
		DEBUG(5, ("Could not trigger gid2name\n"));
		cont(private_data, False, NULL);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("gid2name returned an error\n"));
		cont(private_data, False, NULL);
		return;
	}

	cont(private_data, True, response->data.name.name);
}

void winbindd_gid2name_async(TALLOC_CTX *mem_ctx, gid_t gid,
			     void (*cont)(void *private_data, BOOL success,
					  const char *name),
			     void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_GID2NAME;
	request.data.gid = gid;
	do_async(mem_ctx, idmap_child(), &request, gid2name_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_gid2name(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct group *gr;

	DEBUG(3, ("[%5lu]: gid2name %lu\n", (unsigned long)state->pid, 
		  (unsigned long)state->request.data.gid));

	gr = getgrgid(state->request.data.gid);
	if (gr == NULL)
		return WINBINDD_ERROR;

	fstrcpy(state->response.data.name.name, gr->gr_name);
	return WINBINDD_OK;
}

#if 0	/* not used */
static void name2gid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data);

static void winbindd_name2gid_async(TALLOC_CTX *mem_ctx, const char *name,
				    void (*cont)(void *private_data, BOOL success,
						 gid_t gid),
				    void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_NAME2GID;
	fstrcpy(request.data.groupname, name);
	do_async(mem_ctx, idmap_child(), &request, name2gid_recv,
		 (void *)cont, private_data);
}
#endif	/* not used */

enum winbindd_result winbindd_dual_name2gid(struct winbindd_domain *domain,
					    struct winbindd_cli_state *state)
{
	struct group *gr;

	/* Ensure null termination */
	state->request.data.groupname
		[sizeof(state->request.data.groupname)-1] = '\0';

	DEBUG(3, ("[%5lu]: name2gid %s\n", (unsigned long)state->pid, 
		  state->request.data.groupname));

	gr = getgrnam(state->request.data.groupname);
	if (gr == NULL) {
		return WINBINDD_ERROR;
	}

	state->response.data.gid = gr->gr_gid;
	return WINBINDD_OK;
}

#if 0	/* not used */
static void name2gid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			  struct winbindd_response *response,
			  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, gid_t gid) =
		(void (*)(void *, BOOL, gid_t))c;

	if (!success) {
		DEBUG(5, ("Could not trigger name2gid\n"));
		cont(private_data, False, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("name2gid returned an error\n"));
		cont(private_data, False, 0);
		return;
	}

	cont(private_data, True, response->data.gid);
}
#endif	/* not used */

static void lookupsid_recv(TALLOC_CTX *mem_ctx, BOOL success,
			   struct winbindd_response *response,
			   void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *dom_name,
		     const char *name, enum lsa_SidType type) =
		(void (*)(void *, BOOL, const char *, const char *,
			  enum lsa_SidType))c;

	if (!success) {
		DEBUG(5, ("Could not trigger lookupsid\n"));
		cont(private_data, False, NULL, NULL, SID_NAME_UNKNOWN);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("lookupsid returned an error\n"));
		cont(private_data, False, NULL, NULL, SID_NAME_UNKNOWN);
		return;
	}

	cont(private_data, True, response->data.name.dom_name,
	     response->data.name.name,
	     (enum lsa_SidType)response->data.name.type);
}

void winbindd_lookupsid_async(TALLOC_CTX *mem_ctx, const DOM_SID *sid,
			      void (*cont)(void *private_data, BOOL success,
					   const char *dom_name,
					   const char *name,
					   enum lsa_SidType type),
			      void *private_data)
{
	struct winbindd_domain *domain;
	struct winbindd_request request;

	domain = find_lookup_domain_from_sid(sid);
	if (domain == NULL) {
		DEBUG(5, ("Could not find domain for sid %s\n",
			  sid_string_static(sid)));
		cont(private_data, False, NULL, NULL, SID_NAME_UNKNOWN);
		return;
	}

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_LOOKUPSID;
	fstrcpy(request.data.sid, sid_string_static(sid));

	do_async_domain(mem_ctx, domain, &request, lookupsid_recv,
			(void *)cont, private_data);
}

enum winbindd_result winbindd_dual_lookupsid(struct winbindd_domain *domain,
					     struct winbindd_cli_state *state)
{
	enum lsa_SidType type;
	DOM_SID sid;
	char *name;
	char *dom_name;

	/* Ensure null termination */
	state->request.data.sid[sizeof(state->request.data.sid)-1]='\0';

	DEBUG(3, ("[%5lu]: lookupsid %s\n", (unsigned long)state->pid, 
		  state->request.data.sid));

	/* Lookup sid from PDC using lsa_lookup_sids() */

	if (!string_to_sid(&sid, state->request.data.sid)) {
		DEBUG(5, ("%s not a SID\n", state->request.data.sid));
		return WINBINDD_ERROR;
	}

	/* Lookup the sid */

	if (!winbindd_lookup_name_by_sid(state->mem_ctx, &sid, &dom_name, &name,
					 &type)) {
		TALLOC_FREE(dom_name);
		TALLOC_FREE(name);
		return WINBINDD_ERROR;
	}

	fstrcpy(state->response.data.name.dom_name, dom_name);
	fstrcpy(state->response.data.name.name, name);
	state->response.data.name.type = type;

	TALLOC_FREE(dom_name);
	TALLOC_FREE(name);
	return WINBINDD_OK;
}

/********************************************************************
 This is the second callback after contacting the forest root
********************************************************************/

struct lookupname_state {
	char *dom_name;
	char *name;
	void *caller_private_data;
};

static void lookupname_recv2(TALLOC_CTX *mem_ctx, BOOL success,
			    struct winbindd_response *response,
			    void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const DOM_SID *sid,
		     enum lsa_SidType type) =
		(void (*)(void *, BOOL, const DOM_SID *, enum lsa_SidType))c;
	DOM_SID sid;
	struct lookupname_state *s = talloc_get_type_abort(private_data, struct lookupname_state);

	if (!success) {
		DEBUG(5, ("Could not trigger lookup_name\n"));
		cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("lookup_name returned an error\n"));
		cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	if (!string_to_sid(&sid, response->data.sid.sid)) {
		DEBUG(0, ("Could not convert string %s to sid\n",
			  response->data.sid.sid));
		cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	cont(s->caller_private_data, True, &sid,
	     (enum lsa_SidType)response->data.sid.type);
}

/********************************************************************
 This is the first callback after contacting our own domain 
********************************************************************/

static void lookupname_recv(TALLOC_CTX *mem_ctx, BOOL success,
			    struct winbindd_response *response,
			    void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const DOM_SID *sid,
		     enum lsa_SidType type) =
		(void (*)(void *, BOOL, const DOM_SID *, enum lsa_SidType))c;
	DOM_SID sid;
	struct lookupname_state *s = talloc_get_type_abort(private_data, struct lookupname_state);

	if (!success) {
		DEBUG(5, ("lookupname_recv: lookup_name() failed!\n"));
		cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	if (response->result != WINBINDD_OK) {
		/* Try again using the forest root */
		struct winbindd_domain *root_domain = find_root_domain();
		struct winbindd_request request;

		if ( !root_domain ) {
			DEBUG(5,("lookupname_recv: unable to determine forest root\n"));
			cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
			return;
		}

		ZERO_STRUCT(request);
		request.cmd = WINBINDD_LOOKUPNAME;
		fstrcpy(request.data.name.dom_name, s->dom_name);
		fstrcpy(request.data.name.name, s->name);

		do_async_domain(mem_ctx, root_domain, &request, lookupname_recv2,
				(void *)cont, s);

		return;
	}

	if (!string_to_sid(&sid, response->data.sid.sid)) {
		DEBUG(0, ("Could not convert string %s to sid\n",
			  response->data.sid.sid));
		cont(s->caller_private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	cont(s->caller_private_data, True, &sid,
	     (enum lsa_SidType)response->data.sid.type);
}

/********************************************************************
 The lookup name call first contacts a DC in its own domain
 and fallbacks to contact a DC in the forest in our domain doesn't
 know the name.
********************************************************************/

void winbindd_lookupname_async(TALLOC_CTX *mem_ctx,
			       const char *dom_name, const char *name,
			       void (*cont)(void *private_data, BOOL success,
					    const DOM_SID *sid,
					    enum lsa_SidType type),
			       void *private_data)
{
	struct winbindd_request request;
	struct winbindd_domain *domain;
	struct lookupname_state *s;

	if ( (domain = find_lookup_domain_from_name(dom_name)) == NULL ) {
		DEBUG(5, ("Could not find domain for name %s\n", dom_name));
		cont(private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_LOOKUPNAME;
	fstrcpy(request.data.name.dom_name, dom_name);
	fstrcpy(request.data.name.name, name);

	if ( (s = TALLOC_ZERO_P(mem_ctx, struct lookupname_state)) == NULL ) {
		DEBUG(0, ("winbindd_lookupname_async: talloc failed\n"));
		cont(private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	s->dom_name = talloc_strdup( s, dom_name );
	s->name     = talloc_strdup( s, name );
	if (!s->dom_name || !s->name) {
		cont(private_data, False, NULL, SID_NAME_UNKNOWN);
		return;
	}

	s->caller_private_data = private_data;

	do_async_domain(mem_ctx, domain, &request, lookupname_recv,
			(void *)cont, s);
}

enum winbindd_result winbindd_dual_lookupname(struct winbindd_domain *domain,
					      struct winbindd_cli_state *state)
{
	enum lsa_SidType type;
	char *name_domain, *name_user;
	DOM_SID sid;
	char *p;

	/* Ensure null termination */
	state->request.data.name.dom_name[sizeof(state->request.data.name.dom_name)-1]='\0';

	/* Ensure null termination */
	state->request.data.name.name[sizeof(state->request.data.name.name)-1]='\0';

	/* cope with the name being a fully qualified name */
	p = strstr(state->request.data.name.name, lp_winbind_separator());
	if (p) {
		*p = 0;
		name_domain = state->request.data.name.name;
		name_user = p+1;
	} else {
		name_domain = state->request.data.name.dom_name;
		name_user = state->request.data.name.name;
	}

	DEBUG(3, ("[%5lu]: lookupname %s%s%s\n", (unsigned long)state->pid,
		  name_domain, lp_winbind_separator(), name_user));

	/* Lookup name from DC using lsa_lookup_names() */
	if (!winbindd_lookup_sid_by_name(state->mem_ctx, domain, name_domain,
					 name_user, &sid, &type)) {
		return WINBINDD_ERROR;
	}

	sid_to_string(state->response.data.sid.sid, &sid);
	state->response.data.sid.type = type;

	return WINBINDD_OK;
}

BOOL print_sidlist(TALLOC_CTX *mem_ctx, const DOM_SID *sids,
		   size_t num_sids, char **result, ssize_t *len)
{
	size_t i;
	size_t buflen = 0;

	*len = 0;
	*result = NULL;
	for (i=0; i<num_sids; i++) {
		sprintf_append(mem_ctx, result, len, &buflen,
			       "%s\n", sid_string_static(&sids[i]));
	}

	if ((num_sids != 0) && (*result == NULL)) {
		return False;
	}

	return True;
}

static BOOL parse_sidlist(TALLOC_CTX *mem_ctx, char *sidstr,
			  DOM_SID **sids, size_t *num_sids)
{
	char *p, *q;

	p = sidstr;
	if (p == NULL)
		return False;

	while (p[0] != '\0') {
		DOM_SID sid;
		q = strchr(p, '\n');
		if (q == NULL) {
			DEBUG(0, ("Got invalid sidstr: %s\n", p));
			return False;
		}
		*q = '\0';
		q += 1;
		if (!string_to_sid(&sid, p)) {
			DEBUG(0, ("Could not parse sid %s\n", p));
			return False;
		}
		if (!add_sid_to_array(mem_ctx, &sid, sids, num_sids)) {
			return False;
		}
		p = q;
	}
	return True;
}

static BOOL parse_ridlist(TALLOC_CTX *mem_ctx, char *ridstr,
			  uint32 **rids, size_t *num_rids)
{
	char *p;

	p = ridstr;
	if (p == NULL)
		return False;

	while (p[0] != '\0') {
		uint32 rid;
		char *q;
		rid = strtoul(p, &q, 10);
		if (*q != '\n') {
			DEBUG(0, ("Got invalid ridstr: %s\n", p));
			return False;
		}
		p = q+1;
		ADD_TO_ARRAY(mem_ctx, uint32, rid, rids, num_rids);
	}
	return True;
}

enum winbindd_result winbindd_dual_lookuprids(struct winbindd_domain *domain,
					      struct winbindd_cli_state *state)
{
	uint32 *rids = NULL;
	size_t i, buflen, num_rids = 0;
	ssize_t len;
	DOM_SID domain_sid;
	char *domain_name;
	char **names;
	enum lsa_SidType *types;
	NTSTATUS status;
	char *result;

	DEBUG(10, ("Looking up RIDs for domain %s (%s)\n",
		   state->request.domain_name,
		   state->request.data.sid));

	if (!parse_ridlist(state->mem_ctx, state->request.extra_data.data,
			   &rids, &num_rids)) {
		DEBUG(5, ("Could not parse ridlist\n"));
		return WINBINDD_ERROR;
	}

	if (!string_to_sid(&domain_sid, state->request.data.sid)) {
		DEBUG(5, ("Could not parse domain sid %s\n",
			  state->request.data.sid));
		return WINBINDD_ERROR;
	}

	status = domain->methods->rids_to_names(domain, state->mem_ctx,
						&domain_sid, rids, num_rids,
						&domain_name,
						&names, &types);

	if (!NT_STATUS_IS_OK(status) &&
	    !NT_STATUS_EQUAL(status, STATUS_SOME_UNMAPPED)) {
		return WINBINDD_ERROR;
	}

	len = 0;
	buflen = 0;
	result = NULL;

	for (i=0; i<num_rids; i++) {
		sprintf_append(state->mem_ctx, &result, &len, &buflen,
			       "%d %s\n", types[i], names[i]);
	}

	fstrcpy(state->response.data.domain_name, domain_name);

	if (result != NULL) {
		state->response.extra_data.data = SMB_STRDUP(result);
		if (!state->response.extra_data.data) {
			return WINBINDD_ERROR;
		}
		state->response.length += len+1;
	}

	return WINBINDD_OK;
}

static void getsidaliases_recv(TALLOC_CTX *mem_ctx, BOOL success,
			       struct winbindd_response *response,
			       void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ,
		     DOM_SID *aliases, size_t num_aliases) =
		(void (*)(void *, BOOL, DOM_SID *, size_t))c;
	char *aliases_str;
	DOM_SID *sids = NULL;
	size_t num_sids = 0;

	if (!success) {
		DEBUG(5, ("Could not trigger getsidaliases\n"));
		cont(private_data, success, NULL, 0);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("getsidaliases returned an error\n"));
		cont(private_data, False, NULL, 0);
		return;
	}

	aliases_str = (char *)response->extra_data.data;

	if (aliases_str == NULL) {
		DEBUG(10, ("getsidaliases return 0 SIDs\n"));
		cont(private_data, True, NULL, 0);
		return;
	}

	if (!parse_sidlist(mem_ctx, aliases_str, &sids, &num_sids)) {
		DEBUG(0, ("Could not parse sids\n"));
		cont(private_data, False, NULL, 0);
		return;
	}

	SAFE_FREE(response->extra_data.data);

	cont(private_data, True, sids, num_sids);
}

void winbindd_getsidaliases_async(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const DOM_SID *sids, size_t num_sids,
			 	  void (*cont)(void *private_data,
				 	       BOOL success,
					       const DOM_SID *aliases,
					       size_t num_aliases),
				  void *private_data)
{
	struct winbindd_request request;
	char *sidstr = NULL;
	ssize_t len;

	if (num_sids == 0) {
		cont(private_data, True, NULL, 0);
		return;
	}

	if (!print_sidlist(mem_ctx, sids, num_sids, &sidstr, &len)) {
		cont(private_data, False, NULL, 0);
		return;
	}

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_GETSIDALIASES;
	request.extra_len = len;
	request.extra_data.data = sidstr;

	do_async_domain(mem_ctx, domain, &request, getsidaliases_recv,
			(void *)cont, private_data);
}

enum winbindd_result winbindd_dual_getsidaliases(struct winbindd_domain *domain,
						 struct winbindd_cli_state *state)
{
	DOM_SID *sids = NULL;
	size_t num_sids = 0;
	char *sidstr = NULL;
	ssize_t len;
	size_t i;
	uint32 num_aliases;
	uint32 *alias_rids;
	NTSTATUS result;

	DEBUG(3, ("[%5lu]: getsidaliases\n", (unsigned long)state->pid));

	sidstr = state->request.extra_data.data;
	if (sidstr == NULL) {
		sidstr = talloc_strdup(state->mem_ctx, "\n"); /* No SID */
		if (!sidstr) {
			DEBUG(0, ("Out of memory\n"));
			return WINBINDD_ERROR;
		}
	}

	DEBUG(10, ("Sidlist: %s\n", sidstr));

	if (!parse_sidlist(state->mem_ctx, sidstr, &sids, &num_sids)) {
		DEBUG(0, ("Could not parse SID list: %s\n", sidstr));
		return WINBINDD_ERROR;
	}

	num_aliases = 0;
	alias_rids = NULL;

	result = domain->methods->lookup_useraliases(domain,
						     state->mem_ctx,
						     num_sids, sids,
						     &num_aliases,
						     &alias_rids);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(3, ("Could not lookup_useraliases: %s\n",
			  nt_errstr(result)));
		return WINBINDD_ERROR;
	}

	num_sids = 0;
	sids = NULL;
	sidstr = NULL;

	DEBUG(10, ("Got %d aliases\n", num_aliases));

	for (i=0; i<num_aliases; i++) {
		DOM_SID sid;
		DEBUGADD(10, (" rid %d\n", alias_rids[i]));
		sid_copy(&sid, &domain->sid);
		sid_append_rid(&sid, alias_rids[i]);
		if (!add_sid_to_array(state->mem_ctx, &sid, &sids, &num_sids)) {
			return WINBINDD_ERROR;
		}
	}


	if (!print_sidlist(state->mem_ctx, sids, num_sids, &sidstr, &len)) {
		DEBUG(0, ("Could not print_sidlist\n"));
		state->response.extra_data.data = NULL;
		return WINBINDD_ERROR;
	}

	state->response.extra_data.data = NULL;

	if (sidstr) {
		state->response.extra_data.data = SMB_STRDUP(sidstr);
		if (!state->response.extra_data.data) {
			DEBUG(0, ("Out of memory\n"));
			return WINBINDD_ERROR;
		}
		DEBUG(10, ("aliases_list: %s\n",
			   (char *)state->response.extra_data.data));
		state->response.length += len+1;
	}
	
	return WINBINDD_OK;
}

struct gettoken_state {
	TALLOC_CTX *mem_ctx;
	DOM_SID user_sid;
	struct winbindd_domain *alias_domain;
	struct winbindd_domain *local_alias_domain;
	struct winbindd_domain *builtin_domain;
	DOM_SID *sids;
	size_t num_sids;
	void (*cont)(void *private_data, BOOL success, DOM_SID *sids, size_t num_sids);
	void *private_data;
};

static void gettoken_recvdomgroups(TALLOC_CTX *mem_ctx, BOOL success,
				   struct winbindd_response *response,
				   void *c, void *private_data);
static void gettoken_recvaliases(void *private_data, BOOL success,
				 const DOM_SID *aliases,
				 size_t num_aliases);
				 

void winbindd_gettoken_async(TALLOC_CTX *mem_ctx, const DOM_SID *user_sid,
			     void (*cont)(void *private_data, BOOL success,
					  DOM_SID *sids, size_t num_sids),
			     void *private_data)
{
	struct winbindd_domain *domain;
	struct winbindd_request request;
	struct gettoken_state *state;

	state = TALLOC_ZERO_P(mem_ctx, struct gettoken_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		cont(private_data, False, NULL, 0);
		return;
	}

	state->mem_ctx = mem_ctx;
	sid_copy(&state->user_sid, user_sid);
	state->alias_domain = find_our_domain();
	state->local_alias_domain = find_domain_from_name( get_global_sam_name() );
	state->builtin_domain = find_builtin_domain();
	state->cont = cont;
	state->private_data = private_data;

	domain = find_domain_from_sid_noinit(user_sid);
	if (domain == NULL) {
		DEBUG(5, ("Could not find domain from SID %s\n",
			  sid_string_static(user_sid)));
		cont(private_data, False, NULL, 0);
		return;
	}

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_GETUSERDOMGROUPS;
	fstrcpy(request.data.sid, sid_string_static(user_sid));

	do_async_domain(mem_ctx, domain, &request, gettoken_recvdomgroups,
			NULL, state);
}

static void gettoken_recvdomgroups(TALLOC_CTX *mem_ctx, BOOL success,
				   struct winbindd_response *response,
				   void *c, void *private_data)
{
	struct gettoken_state *state =
		talloc_get_type_abort(private_data, struct gettoken_state);
	char *sids_str;
	
	if (!success) {
		DEBUG(10, ("Could not get domain groups\n"));
		state->cont(state->private_data, False, NULL, 0);
		return;
	}

	sids_str = (char *)response->extra_data.data;

	if (sids_str == NULL) {
		/* This could be normal if we are dealing with a
		   local user and local groups */

		if ( !sid_check_is_in_our_domain( &state->user_sid ) ) {
			DEBUG(10, ("Received no domain groups\n"));
			state->cont(state->private_data, True, NULL, 0);
			return;
		}
	}

	state->sids = NULL;
	state->num_sids = 0;

	if (!add_sid_to_array(mem_ctx, &state->user_sid, &state->sids,
			 &state->num_sids)) {
		DEBUG(0, ("Out of memory\n"));
		state->cont(state->private_data, False, NULL, 0);
		return;
	}

	if (sids_str && !parse_sidlist(mem_ctx, sids_str, &state->sids,
			   &state->num_sids)) {
		DEBUG(0, ("Could not parse sids\n"));
		state->cont(state->private_data, False, NULL, 0);
		return;
	}

	SAFE_FREE(response->extra_data.data);

	if (state->alias_domain == NULL) {
		DEBUG(10, ("Don't expand domain local groups\n"));
		state->cont(state->private_data, True, state->sids,
			    state->num_sids);
		return;
	}

	winbindd_getsidaliases_async(state->alias_domain, mem_ctx,
				     state->sids, state->num_sids,
				     gettoken_recvaliases, state);
}

static void gettoken_recvaliases(void *private_data, BOOL success,
				 const DOM_SID *aliases,
				 size_t num_aliases)
{
	struct gettoken_state *state = (struct gettoken_state *)private_data;
	size_t i;

	if (!success) {
		DEBUG(10, ("Could not receive domain local groups\n"));
		state->cont(state->private_data, False, NULL, 0);
		return;
	}

	for (i=0; i<num_aliases; i++) {
		if (!add_sid_to_array(state->mem_ctx, &aliases[i],
				 &state->sids, &state->num_sids)) {
			DEBUG(0, ("Out of memory\n"));
			state->cont(state->private_data, False, NULL, 0);
			return;
		}
	}

	if (state->local_alias_domain != NULL) {
		struct winbindd_domain *local_domain = state->local_alias_domain;
		DEBUG(10, ("Expanding our own local groups\n"));
		state->local_alias_domain = NULL;
		winbindd_getsidaliases_async(local_domain, state->mem_ctx,
					     state->sids, state->num_sids,
					     gettoken_recvaliases, state);
		return;
	}

	if (state->builtin_domain != NULL) {
		struct winbindd_domain *builtin_domain = state->builtin_domain;
		DEBUG(10, ("Expanding our own BUILTIN groups\n"));
		state->builtin_domain = NULL;
		winbindd_getsidaliases_async(builtin_domain, state->mem_ctx,
					     state->sids, state->num_sids,
					     gettoken_recvaliases, state);
		return;
	}

	state->cont(state->private_data, True, state->sids, state->num_sids);
}

static void query_user_recv(TALLOC_CTX *mem_ctx, BOOL success,
			    struct winbindd_response *response,
			    void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *acct_name,
		     const char *full_name, const char *homedir, 
		     const char *shell, uint32 gid, uint32 group_rid) =
		(void (*)(void *, BOOL, const char *, const char *,
			  const char *, const char *, uint32, uint32))c;

	if (!success) {
		DEBUG(5, ("Could not trigger query_user\n"));
		cont(private_data, False, NULL, NULL, NULL, NULL, -1, -1);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("query_user returned an error\n"));
		cont(private_data, False, NULL, NULL, NULL, NULL, -1, -1);
		return;
	}

	cont(private_data, True, response->data.user_info.acct_name,
	     response->data.user_info.full_name,
	     response->data.user_info.homedir,
	     response->data.user_info.shell,
	     response->data.user_info.primary_gid,
	     response->data.user_info.group_rid);
}

void query_user_async(TALLOC_CTX *mem_ctx, struct winbindd_domain *domain,
		      const DOM_SID *sid,
		      void (*cont)(void *private_data, BOOL success,
				   const char *acct_name,
				   const char *full_name,
				   const char *homedir,
				   const char *shell,
				   gid_t gid,
				   uint32 group_rid),
		      void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_USERINFO;
	sid_to_string(request.data.sid, sid);
	do_async_domain(mem_ctx, domain, &request, query_user_recv,
			(void *)cont, private_data);
}

/* The following uid2sid/gid2sid functions has been contributed by
 * Keith Reynolds <Keith.Reynolds@centrify.com> */

static void winbindd_uid2sid_recv(TALLOC_CTX *mem_ctx, BOOL success,
				  struct winbindd_response *response,
				  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *sid) =
		(void (*)(void *, BOOL, const char *))c;

	if (!success) {
		DEBUG(5, ("Could not trigger uid2sid\n"));
		cont(private_data, False, NULL);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("uid2sid returned an error\n"));
		cont(private_data, False, NULL);
		return;
	}

	cont(private_data, True, response->data.sid.sid);
}

void winbindd_uid2sid_async(TALLOC_CTX *mem_ctx, uid_t uid,
			    void (*cont)(void *private_data, BOOL success, const char *sid),
			    void *private_data)
{
	struct winbindd_request request;

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_UID2SID;
	request.data.uid = uid;
	do_async(mem_ctx, idmap_child(), &request, winbindd_uid2sid_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_uid2sid(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DOM_SID sid;
	NTSTATUS result;

	DEBUG(3,("[%5lu]: uid to sid %lu\n",
		 (unsigned long)state->pid,
		 (unsigned long) state->request.data.uid));

	/* Find sid for this uid and return it, possibly ask the slow remote idmap */
	result = idmap_uid_to_sid(&sid, state->request.data.uid);

	if (NT_STATUS_IS_OK(result)) {
		sid_to_string(state->response.data.sid.sid, &sid);
		state->response.data.sid.type = SID_NAME_USER;
		return WINBINDD_OK;
	}

	return WINBINDD_ERROR;
}

static void winbindd_gid2sid_recv(TALLOC_CTX *mem_ctx, BOOL success,
				  struct winbindd_response *response,
				  void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ, const char *sid) =
		(void (*)(void *, BOOL, const char *))c;

	if (!success) {
		DEBUG(5, ("Could not trigger gid2sid\n"));
		cont(private_data, False, NULL);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("gid2sid returned an error\n"));
		cont(private_data, False, NULL);
		return;
	}

	cont(private_data, True, response->data.sid.sid);
}

void winbindd_gid2sid_async(TALLOC_CTX *mem_ctx, gid_t gid,
			    void (*cont)(void *private_data, BOOL success, const char *sid),
			    void *private_data)
{
	struct winbindd_request request;

	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_GID2SID;
	request.data.gid = gid;
	do_async(mem_ctx, idmap_child(), &request, winbindd_gid2sid_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_gid2sid(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DOM_SID sid;
	NTSTATUS result;

	DEBUG(3,("[%5lu]: gid %lu to sid\n",
		(unsigned long)state->pid,
		(unsigned long) state->request.data.gid));

	/* Find sid for this gid and return it, possibly ask the slow remote idmap */
	result = idmap_gid_to_sid(&sid, state->request.data.gid);

	if (NT_STATUS_IS_OK(result)) {
		sid_to_string(state->response.data.sid.sid, &sid);
		DEBUG(10, ("[%5lu]: retrieved sid: %s\n",
			   (unsigned long)state->pid,
			   state->response.data.sid.sid));
		state->response.data.sid.type = SID_NAME_DOM_GRP;
		return WINBINDD_OK;
	}

	return WINBINDD_ERROR;
}

static void winbindd_dump_id_maps_recv(TALLOC_CTX *mem_ctx, BOOL success,
			       struct winbindd_response *response,
			       void *c, void *private_data)
{
	void (*cont)(void *priv, BOOL succ) =
		(void (*)(void *, BOOL))c;

	if (!success) {
		DEBUG(5, ("Could not trigger a map dump\n"));
		cont(private_data, False);
		return;
	}

	if (response->result != WINBINDD_OK) {
		DEBUG(5, ("idmap dump maps returned an error\n"));
		cont(private_data, False);
		return;
	}

	cont(private_data, True);
}
			 
void winbindd_dump_maps_async(TALLOC_CTX *mem_ctx, void *data, int size,
			 void (*cont)(void *private_data, BOOL success),
			 void *private_data)
{
	struct winbindd_request request;
	ZERO_STRUCT(request);
	request.cmd = WINBINDD_DUAL_DUMP_MAPS;
	request.extra_data.data = (char *)data;
	request.extra_len = size;
	do_async(mem_ctx, idmap_child(), &request, winbindd_dump_id_maps_recv,
		 (void *)cont, private_data);
}

enum winbindd_result winbindd_dual_dump_maps(struct winbindd_domain *domain,
					   struct winbindd_cli_state *state)
{
	DEBUG(3, ("[%5lu]: dual dump maps\n", (unsigned long)state->pid));

	idmap_dump_maps((char *)state->request.extra_data.data);

	return WINBINDD_OK;
}

