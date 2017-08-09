/*
   Unix SMB/CIFS implementation.
   Main DCOM functionality
   Copyright (C) 2004 Jelmer Vernooij <jelmer@samba.org>
   Copyright (C) 2006 Andrzej Hajda <andrzej.hajda@wp.pl>

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
#include "system/filesys.h"
#include "librpc/gen_ndr/epmapper.h"
#include "librpc/gen_ndr/ndr_remact_c.h"
#include "librpc/gen_ndr/com_dcom.h"
#include "librpc/gen_ndr/dcom.h"
#include "librpc/rpc/dcerpc.h"
#include "lib/com/dcom/dcom.h"
#include "librpc/ndr/ndr_table.h"
#include "../lib/util/dlinklist.h"
#include "auth/credentials/credentials.h"
#include "libcli/composite/composite.h"

#define DCOM_NEGOTIATED_PROTOCOLS { EPM_PROTOCOL_TCP, EPM_PROTOCOL_SMB, EPM_PROTOCOL_NCALRPC }

static NTSTATUS dcerpc_binding_from_STRINGBINDING(TALLOC_CTX *mem_ctx, struct dcerpc_binding **b_out, struct STRINGBINDING *bd)
{
	char *host, *endpoint;
	struct dcerpc_binding *b;

	b = talloc_zero(mem_ctx, struct dcerpc_binding);
	if (!b) {
		return NT_STATUS_NO_MEMORY;
	}
	
	b->transport = dcerpc_transport_by_endpoint_protocol(bd->wTowerId);

	if (b->transport == -1) {
		DEBUG(1, ("Can't find transport match endpoint protocol %d\n", bd->wTowerId));
		talloc_free(b);
		return NT_STATUS_NOT_SUPPORTED;
	}

	host = talloc_strdup(b, bd->NetworkAddr);
	endpoint = strchr(host, '[');

	if (endpoint) {
		*endpoint = '\0';
		endpoint++;

		endpoint[strlen(endpoint)-1] = '\0';
	}

	b->host = host;
	b->endpoint = talloc_strdup(b, endpoint);

	*b_out = b;
	return NT_STATUS_OK;
}

struct cli_credentials *dcom_get_server_credentials(struct com_context *ctx, const char *server)
{
	struct dcom_server_credentials *c;
	struct cli_credentials *d;

	d = NULL;
	for (c = ctx->dcom->credentials; c; c = c->next) {
		if (c->server == NULL) {
			d = c->credentials;
			continue;
		}
		if (server && !strcmp(c->server, server)) return c->credentials;
	}
	return d;
}

/**
 * Register credentials for a specific server.
 *
 * @param ctx COM context
 * @param server Name of server, can be NULL
 * @param credentials Credentials object
 */
void dcom_add_server_credentials(struct com_context *ctx, const char *server, 
								 struct cli_credentials *credentials)
{
	struct dcom_server_credentials *c;

	/* FIXME: Don't use talloc_find_parent_bytype */
	for (c = ctx->dcom->credentials; c; c = c->next) {
		if ((server == NULL && c->server == NULL) ||
			(server != NULL && c->server != NULL && 
			 !strcmp(c->server, server))) {
			if (c->credentials && c->credentials != credentials) {
				talloc_unlink(c, c->credentials);
				c->credentials = credentials;
				if (talloc_find_parent_bytype(c->credentials, struct dcom_server_credentials))
					(void)talloc_reference(c, c->credentials);
				else
					talloc_steal(c, c->credentials);
			}

			return;
		}
	}

	c = talloc(ctx->event_ctx, struct dcom_server_credentials);
	c->server = talloc_strdup(c, server);
	c->credentials = credentials;
	if (talloc_find_parent_bytype(c->credentials, struct dcom_server_credentials))
		(void)talloc_reference(c, c->credentials);
	else
		talloc_steal(c, c->credentials);

	DLIST_ADD(ctx->dcom->credentials, c);
}

void dcom_update_credentials_for_aliases(struct com_context *ctx, 
										 const char *server, 
										 struct DUALSTRINGARRAY *pds)
{
	struct cli_credentials *cc;
	struct dcerpc_binding *b;
	uint32_t i;
	NTSTATUS status;

	cc = dcom_get_server_credentials(ctx, server);
        for (i = 0; pds->stringbindings[i]; ++i) {
                if (pds->stringbindings[i]->wTowerId != EPM_PROTOCOL_TCP) 
					continue;
                status = dcerpc_binding_from_STRINGBINDING(ctx, &b, pds->stringbindings[i]);
		if (!NT_STATUS_IS_OK(status)) 
			continue;
		dcom_add_server_credentials(ctx, b->host, cc);
		talloc_free(b);
	}
}

struct dcom_client_context *dcom_client_init(struct com_context *ctx, struct cli_credentials *credentials)
{
	ctx->dcom = talloc_zero(ctx, struct dcom_client_context);
	if (!credentials) {
                credentials = cli_credentials_init(ctx);
                cli_credentials_set_conf(credentials, ctx->lp_ctx);
                cli_credentials_parse_string(credentials, "%", CRED_SPECIFIED);
	}
	dcom_add_server_credentials(ctx, NULL, credentials);
	return ctx->dcom;
}

static NTSTATUS dcom_connect_host(struct com_context *ctx, 
								  struct dcerpc_pipe **p, const char *server)
{
	struct dcerpc_binding *bd;
	const char * available_transports[] = { "ncacn_ip_tcp", "ncacn_np" };
	int i;
	NTSTATUS status;
	TALLOC_CTX *loc_ctx;

	if (server == NULL) { 
		return dcerpc_pipe_connect(ctx->event_ctx, p, "ncalrpc", 
								   &ndr_table_IRemoteActivation,
								   dcom_get_server_credentials(ctx, NULL), ctx->event_ctx, ctx->lp_ctx);
	}
	loc_ctx = talloc_new(ctx);

	/* Allow server name to contain a binding string */
	if (strchr(server, ':') && 
		NT_STATUS_IS_OK(dcerpc_parse_binding(loc_ctx, server, &bd))) {
		if (DEBUGLVL(11))
			bd->flags |= DCERPC_DEBUG_PRINT_BOTH;
		status = dcerpc_pipe_connect_b(ctx->event_ctx, p, bd, 
									   &ndr_table_IRemoteActivation,
								   dcom_get_server_credentials(ctx, bd->host), ctx->event_ctx, ctx->lp_ctx);
		goto end;
	}

	for (i = 0; i < ARRAY_SIZE(available_transports); i++)
	{
		char *binding = talloc_asprintf(loc_ctx, "%s:%s", available_transports[i], server);
		if (!binding) {
			status = NT_STATUS_NO_MEMORY;
			goto end;
		}
		status = dcerpc_pipe_connect(ctx->event_ctx, p, binding, 
									 &ndr_table_IRemoteActivation, 
									 dcom_get_server_credentials(ctx, server), 
									 ctx->event_ctx, ctx->lp_ctx);

		if (NT_STATUS_IS_OK(status)) {
			if (DEBUGLVL(11))
				(*p)->conn->flags |= DCERPC_DEBUG_PRINT_BOTH;
			goto end;
		} else {
			DEBUG(1,(__location__": dcom_connect_host : %s\n", get_friendly_nt_error_msg(status)));
		}
	}

end:
	talloc_free(loc_ctx);
	return status;
}

struct dcom_object_exporter *object_exporter_by_oxid(struct com_context *ctx, 
													 uint64_t oxid)
{
	struct dcom_object_exporter *ox;
	for (ox = ctx->dcom->object_exporters; ox; ox = ox->next) {
		if (ox->oxid == oxid) {
			return ox;
		}
	}

	return NULL; 
}

struct dcom_object_exporter *object_exporter_update_oxid(struct com_context *ctx, uint64_t oxid, struct DUALSTRINGARRAY *bindings)
{
	struct dcom_object_exporter *ox;
	ox = object_exporter_by_oxid(ctx, oxid);
	if (!ox) {
		ox = talloc_zero(ctx, struct dcom_object_exporter);
		DLIST_ADD(ctx->dcom->object_exporters, ox);
		ox->oxid = oxid;
	} else {
		talloc_free(ox->bindings);
	}
	ox->bindings = bindings;
	talloc_steal(ox, bindings);
	return ox;
}

struct dcom_object_exporter *object_exporter_by_ip(struct com_context *ctx, struct IUnknown *ip)
{
	return object_exporter_by_oxid(ctx, ip->obj.u_objref.u_standard.std.oxid);
}

WERROR dcom_create_object(struct com_context *ctx, struct GUID *clsid, const char *server, int num_ifaces, struct GUID *iid, struct IUnknown ***ip, WERROR *results)
{
	uint16_t protseq[] = DCOM_NEGOTIATED_PROTOCOLS;
	struct dcerpc_pipe *p;
	struct dcom_object_exporter *m;
	NTSTATUS status;
	struct RemoteActivation r;
	struct DUALSTRINGARRAY *pds;
	int i;
	WERROR hr;
	uint64_t oxid;
	struct GUID ipidRemUnknown;
	struct IUnknown *ru_template;
	struct ORPCTHAT that;
	uint32_t AuthnHint;
	struct COMVERSION ServerVersion;
	struct MInterfacePointer **ifaces;
	TALLOC_CTX *loc_ctx;

	status = dcom_connect_host(ctx, &p, server);
	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(1, ("Unable to connect to %s - %s\n", server, get_friendly_nt_error_msg(status)));
		return ntstatus_to_werror(status);
	}
	loc_ctx = talloc_new(ctx);

	ifaces = talloc_array(loc_ctx, struct MInterfacePointer *, num_ifaces);

	ZERO_STRUCT(r.in);
	r.in.this.version.MajorVersion = COM_MAJOR_VERSION;
	r.in.this.version.MinorVersion = COM_MINOR_VERSION;
	r.in.this.cid = GUID_random();
	r.in.Clsid = *clsid;
	r.in.ClientImpLevel = RPC_C_IMP_LEVEL_IDENTIFY;
	r.in.num_protseqs = ARRAY_SIZE(protseq);
	r.in.protseq = protseq;
	r.in.Interfaces = num_ifaces;
	r.in.pIIDs = iid;
	r.out.that = &that;
	r.out.pOxid = &oxid;
	r.out.pdsaOxidBindings = &pds;
	r.out.ipidRemUnknown = &ipidRemUnknown;
	r.out.AuthnHint = &AuthnHint;
	r.out.ServerVersion = &ServerVersion;
	r.out.hr = &hr;
	r.out.ifaces = ifaces;
	r.out.results = results;

	status = dcerpc_RemoteActivation(p, loc_ctx, &r);
	talloc_free(p);

	if(NT_STATUS_IS_ERR(status)) {
		DEBUG(1, ("Error while running RemoteActivation %s\n", nt_errstr(status)));
		hr = ntstatus_to_werror(status);
		goto end;
	}

	if(!W_ERROR_IS_OK(r.out.result)) {
		hr = r.out.result;
		goto end;
	}
	
	if(!W_ERROR_IS_OK(hr)) {
		goto end;
	}

	m = object_exporter_update_oxid(ctx, oxid, pds);

	ru_template = NULL;
	*ip = talloc_array(ctx, struct IUnknown *, num_ifaces);
	for (i = 0; i < num_ifaces; i++) {
		(*ip)[i] = NULL;
		if (W_ERROR_IS_OK(results[i])) {
			status = dcom_IUnknown_from_OBJREF(ctx, &(*ip)[i], &r.out.ifaces[i]->obj);
			if (!NT_STATUS_IS_OK(status)) {
				results[i] = ntstatus_to_werror(status);
			} else if (!ru_template)
				ru_template = (*ip)[i];
		}
	}

	/* TODO:avg check when exactly oxid should be updated,its lifetime etc */
	if (m->rem_unknown && memcmp(&m->rem_unknown->obj.u_objref.u_standard.std.ipid, &ipidRemUnknown, sizeof(ipidRemUnknown))) {
		talloc_free(m->rem_unknown);
		m->rem_unknown = NULL;
	}
	if (!m->rem_unknown) {
		if (!ru_template) {
			DEBUG(1,("dcom_create_object: Cannot Create IRemUnknown - template interface not available\n"));
			hr = WERR_GENERAL_FAILURE;
		}
		m->rem_unknown = talloc_zero(m, struct IRemUnknown);
		memcpy(m->rem_unknown, ru_template, sizeof(struct IUnknown));
		GUID_from_string(COM_IREMUNKNOWN_UUID, &m->rem_unknown->obj.iid);
		m->rem_unknown->obj.u_objref.u_standard.std.ipid = ipidRemUnknown;
		m->rem_unknown->vtable = (struct IRemUnknown_vtable *)dcom_proxy_vtable_by_iid(&m->rem_unknown->obj.iid);
		/* TODO:avg copy stringbindigs?? */
	}

	dcom_update_credentials_for_aliases(ctx, server, pds);
	{ 
		char *c; 
		c = strchr(server, '['); 
		if (m->host) talloc_free(m->host); 
		m->host = c ? talloc_strndup(m, server, c - server) : talloc_strdup(m, server); 
	}
	hr = WERR_OK;
end:
	talloc_free(loc_ctx);
	return hr;
}

int find_similar_binding(struct STRINGBINDING **sb, const char *host)
{ 
	int i, l;
	l = strlen(host);
	for (i = 0; sb[i]; ++i) {
		if ((sb[i]->wTowerId == EPM_PROTOCOL_TCP) && !strncasecmp(host, sb[i]->NetworkAddr, l) && (sb[i]->NetworkAddr[l] == '['))
		break;
	}
	return i;
}

WERROR dcom_query_interface(struct IUnknown *d, uint32_t cRefs, uint16_t cIids, struct GUID *iids, struct IUnknown **ip, WERROR *results)
{
	struct dcom_object_exporter *ox;
	struct REMQIRESULT *rqir;
	WERROR result;
	NTSTATUS status;
	int i;
	TALLOC_CTX *loc_ctx;
	struct IUnknown ru;

	loc_ctx = talloc_new(d);
	ox = object_exporter_by_ip(d->ctx, d);

	result = IRemUnknown_RemQueryInterface(ox->rem_unknown, loc_ctx, &IUnknown_ipid(d), cRefs, cIids, iids, &rqir);
	if (!W_ERROR_IS_OK(result)) {
		DEBUG(1, ("dcom_query_interface failed: %08X\n", W_ERROR_V(result)));
		talloc_free(loc_ctx);
		return result;
	}
	ru = *(struct IUnknown *)ox->rem_unknown;
	for (i = 0; i < cIids; ++i) {
		ip[i] = NULL;
		results[i] = rqir[i].hResult;
		if (W_ERROR_IS_OK(results[i])) {
			ru.obj.iid = iids[i];
			ru.obj.u_objref.u_standard.std = rqir[i].std;
			status = dcom_IUnknown_from_OBJREF(d->ctx, &ip[i], &ru.obj);
			if (!NT_STATUS_IS_OK(status)) {
				results[i] = ntstatus_to_werror(status);
			}
		}
	}

	talloc_free(loc_ctx);
	return WERR_OK;
}

int is_ip_binding(const char* s)
{
	while (*s && (*s != '[')) {
		if (((*s >= '0') && (*s <= '9')) || *s == '.')
			++s;
		else
		    return 0;
	}
	return 1;
}

NTSTATUS dcom_get_pipe(struct IUnknown *iface, struct dcerpc_pipe **pp)
{
	struct dcerpc_binding *binding;
	struct GUID iid;
	uint64_t oxid;
	NTSTATUS status;
	int i, j, isimilar;
	struct dcerpc_pipe *p;
	struct dcom_object_exporter *ox;
	const struct ndr_interface_table *table;

	ox = object_exporter_by_oxid(iface->ctx, iface->obj.u_objref.u_standard.std.oxid);
	if (!ox) {
		DEBUG(0, ("dcom_get_pipe: OXID not found\n"));
		return NT_STATUS_NOT_SUPPORTED;
	}

	p = ox->pipe;
	
	iid = iface->vtable->iid;
	table = ndr_table_by_uuid(&iid);
	if (table == NULL) {
		char *guid_str;
		guid_str = GUID_string(NULL, &iid);
		DEBUG(0,(__location__": dcom_get_pipe - unrecognized interface{%s}\n", guid_str));
		talloc_free(guid_str);
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (p && p->last_fault_code) {
		talloc_free(p);
		ox->pipe = p = NULL;
	}

	if (p) {
		if (!GUID_equal(&p->syntax.uuid, &iid)) {
			ox->pipe->syntax.uuid = iid;

			/* interface will always be present, so 
			 * idl_iface_by_uuid can't return NULL */
			/* status = dcerpc_secondary_context(p, &p2, idl_iface_by_uuid(&iid)); */
			status = dcerpc_alter_context(p, p, &ndr_table_by_uuid(&iid)->syntax_id, &p->transfer_syntax);
		} else
			status = NT_STATUS_OK;
		*pp = p;
		return status;
	}

	status = NT_STATUS_NO_MORE_ENTRIES;

	/* To avoid delays whe connecting nonroutable bindings we 1st check binding starting with hostname */
	/* FIX:low create concurrent connections to all bindings, fastest wins - Win2k and newer does this way???? */
	isimilar = find_similar_binding(ox->bindings->stringbindings, ox->host);
	DEBUG(1, (__location__": dcom_get_pipe: host=%s, similar=%s\n", ox->host, ox->bindings->stringbindings[isimilar] ? ox->bindings->stringbindings[isimilar]->NetworkAddr : "None"));
	j = isimilar - 1;
	for (i = 0; ox->bindings->stringbindings[i]; ++i) {
		if (!ox->bindings->stringbindings[++j]) j = 0;
		/* FIXME:LOW Use also other transports if possible */
		if ((j != isimilar) && (ox->bindings->stringbindings[j]->wTowerId != EPM_PROTOCOL_TCP || !is_ip_binding(ox->bindings->stringbindings[j]->NetworkAddr))) {
			DEBUG(9, ("dcom_get_pipe: Skipping stringbinding %24.24s\n", ox->bindings->stringbindings[j]->NetworkAddr));
			continue;
		}
		DEBUG(9, ("dcom_get_pipe: Trying stringbinding %s\n", ox->bindings->stringbindings[j]->NetworkAddr));
		status = dcerpc_binding_from_STRINGBINDING(iface->ctx, &binding, 
							   ox->bindings->stringbindings[j]);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Error parsing string binding"));
		} else {
			/* FIXME:LOW Make flags more flexible */
			binding->flags |= DCERPC_AUTH_NTLM | DCERPC_SIGN;
			if (DEBUGLVL(11))
				binding->flags |= DCERPC_DEBUG_PRINT_BOTH;
			status = dcerpc_pipe_connect_b(iface->ctx->event_ctx, &p, binding, 
						       ndr_table_by_uuid(&iid),
						       dcom_get_server_credentials(iface->ctx, binding->host),
							   iface->ctx->event_ctx, iface->ctx->lp_ctx);
			talloc_unlink(iface->ctx, binding);
		}
		if (NT_STATUS_IS_OK(status)) break;
	}

	if (NT_STATUS_IS_ERR(status)) {
		DEBUG(0, ("Unable to connect to remote host - %s\n", nt_errstr(status)));
		return status;
	}

	DEBUG(2, ("Successfully connected to OXID %llx\n", (long long)oxid));
	
	ox->pipe = *pp = p;

	return NT_STATUS_OK;
}

NTSTATUS dcom_OBJREF_from_IUnknown(TALLLOC_CTX *mem_ctx, struct OBJREF *o, struct IUnknown *p)
{
	/* FIXME: Cache generated objref objects? */
	ZERO_STRUCTP(o);
	
	if (!p) {
		o->signature = OBJREF_SIGNATURE;
		o->flags = OBJREF_NULL;
	} else {
		*o = p->obj;
		switch(o->flags) {
		case OBJREF_CUSTOM: {
			marshal_fn marshal;

			marshal = dcom_marshal_by_clsid(&o->u_objref.u_custom.clsid);
			if (marshal) {
				return marshal(mem_ctx, p, o);
			} else {
				return NT_STATUS_NOT_SUPPORTED;
			}
		}
		}
	}

	return NT_STATUS_OK;
}

enum ndr_err_code dcom_IUnknown_from_OBJREF(struct com_context *ctx, struct IUnknown **_p, struct OBJREF *o)
{
	struct IUnknown *p;
	struct dcom_object_exporter *ox;
	unmarshal_fn unmarshal;

	switch(o->flags) {
	case OBJREF_NULL: 
		*_p = NULL;
		return NDR_ERR_SUCCESS;

	case OBJREF_STANDARD:
		p = talloc_zero(ctx, struct IUnknown);
		p->ctx = ctx;
		p->obj = *o;
		p->vtable = dcom_proxy_vtable_by_iid(&o->iid);

		if (!p->vtable) {
			DEBUG(0, ("Unable to find proxy class for interface with IID %s\n", GUID_string(ctx, &o->iid)));
			return NDR_ERR_INVALID_POINTER;
		}

		p->vtable->Release_send = dcom_release_send;

		ox = object_exporter_by_oxid(ctx, o->u_objref.u_standard.std.oxid);
		/* FIXME: Add object to list of objects to ping */
		*_p = p;
		return NDR_ERR_SUCCESS;
		
	case OBJREF_HANDLER:
		p = talloc_zero(ctx, struct IUnknown);
		p->ctx = ctx;	
		p->obj = *o;
		ox = object_exporter_by_oxid(ctx, o->u_objref.u_handler.std.oxid );
		/* FIXME: Add object to list of objects to ping */
/*FIXME		p->vtable = dcom_vtable_by_clsid(&o->u_objref.u_handler.clsid);*/
		/* FIXME: Do the custom unmarshaling call */
	
		*_p = p;
		return NDR_ERR_BAD_SWITCH;
		
	case OBJREF_CUSTOM:
		p = talloc_zero(ctx, struct IUnknown);
		p->ctx = ctx;	
		p->vtable = NULL;
		p->obj = *o;
		unmarshal = dcom_unmarshal_by_clsid(&o->u_objref.u_custom.clsid);
		*_p = p;
		if (unmarshal) {
		    return unmarshal(ctx, o, _p);
		} else {
		    return NDR_ERR_BAD_SWITCH;
		}
	}

	return NDR_ERR_BAD_SWITCH;
}

uint64_t dcom_get_current_oxid(void)
{
	return getpid();
}

/* FIXME:Fake async dcom_get_pipe_* */
struct composite_context *dcom_get_pipe_send(struct IUnknown *d, TALLOC_CTX *mem_ctx)
{
        struct composite_context *c;

        c = composite_create(0, d->ctx->event_ctx);
        if (c == NULL) return NULL;
        c->private_data = d;
        /* composite_done(c); bugged - callback is triggered twice by composite_continue and composite_done */
        c->state = COMPOSITE_STATE_DONE; /* this is workaround */

        return c;
}

NTSTATUS dcom_get_pipe_recv(struct composite_context *c, struct dcerpc_pipe **pp)
{
        NTSTATUS status;

        status = dcom_get_pipe((struct IUnknown *)c->private_data, pp);
        talloc_free(c);

        return status;
}

/* FIXME:avg put IUnknown_Release_out into header */
struct IUnknown_Release_out {
        uint32_t result;
};

void dcom_release_continue(struct composite_context *cr)
{
	struct composite_context *c;
	struct IUnknown *d;
	struct IUnknown_Release_out *out;
	WERROR r;

	c = talloc_get_type(cr->async.private_data, struct composite_context);
	d = c->private_data;
	r = IRemUnknown_RemRelease_recv(cr);
	talloc_free(d);
	out = talloc_zero(c, struct IUnknown_Release_out);
	out->result = W_ERROR_V(r);
	c->private_data = out;
	composite_done(c);
}

struct composite_context *dcom_release_send(struct IUnknown *d, TALLOC_CTX *mem_ctx)
{
        struct composite_context *c, *cr;
	struct REMINTERFACEREF iref;
	struct dcom_object_exporter *ox;

        c = composite_create(d->ctx, d->ctx->event_ctx);
        if (c == NULL) return NULL;
        c->private_data = d;

	ox = object_exporter_by_ip(d->ctx, d);
	iref.ipid = IUnknown_ipid(d);
	iref.cPublicRefs = 5;
	iref.cPrivateRefs = 0;
	cr = IRemUnknown_RemRelease_send(ox->rem_unknown, mem_ctx, 1, &iref);

	composite_continue(c, cr, dcom_release_continue, c);
	return c;
}

uint32_t dcom_release_recv(struct composite_context *c)
{
	NTSTATUS status;
	WERROR r;

	status = composite_wait(c);
	if (!NT_STATUS_IS_OK(status))
		r = ntstatus_to_werror(status);
	else
		W_ERROR_V(r) = ((struct IUnknown_Release_out *)c->private_data)->result;
	talloc_free(c);
	return W_ERROR_IS_OK(r) ? 0 : W_ERROR_V(r);
}

uint32_t dcom_release(void *interface, TALLOC_CTX *mem_ctx)
{
	struct composite_context *c;

	c = dcom_release_send(interface, mem_ctx);
	return dcom_release_recv(c);
}

void dcom_proxy_async_call_recv_pipe_send_rpc(struct composite_context *c_pipe)
{
        struct composite_context *c;
        struct dcom_proxy_async_call_state *s;
        struct dcerpc_pipe *p;
        struct rpc_request *req;
        NTSTATUS status;

        c = c_pipe->async.private_data;
        s = talloc_get_type(c->private_data, struct dcom_proxy_async_call_state);

        status = dcom_get_pipe_recv(c_pipe, &p);
        if (!NT_STATUS_IS_OK(status)) {
                composite_error(c, NT_STATUS_RPC_NT_CALL_FAILED);
                return;
        }
/*TODO: FIXME - for now this unused anyway */
        req = dcerpc_ndr_request_send(p, &s->d->obj.u_objref.u_standard.std.ipid, s->table, s->opnum, s, s->r);
        composite_continue_rpc(c, req, s->continuation, c);
}
