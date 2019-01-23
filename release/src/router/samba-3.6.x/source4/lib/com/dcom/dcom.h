/* 
   Unix SMB/CIFS implementation.
   COM standard objects
   Copyright (C) Jelmer Vernooij					  2004-2005.
   
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

#ifndef _DCOM_H /* _DCOM_H */
#define _DCOM_H 

struct cli_credentials;
struct dcerpc_pipe;

#include "lib/com/com.h"
#include "librpc/gen_ndr/orpc.h"

struct dcom_client_context {
	struct dcom_server_credentials {
		const char *server;
		struct cli_credentials *credentials;
		struct dcom_server_credentials *prev, *next;
	} *credentials;
	struct dcom_object_exporter {
		uint64_t oxid;
		char *host;
		struct IRemUnknown *rem_unknown;
		struct DUALSTRINGARRAY *bindings;
		struct dcerpc_pipe *pipe;
		struct dcom_object_exporter *prev, *next;
	} *object_exporters;
};

typedef enum ndr_err_code (*marshal_fn)(TALLOC_CTX *mem_ctx, struct IUnknown *pv, struct OBJREF *o);
typedef enum ndr_err_code (*unmarshal_fn)(TALLOC_CTX *mem_ctx, struct OBJREF *o, struct IUnknown **pv);


struct dcom_client_context *dcom_client_init(struct com_context *ctx, struct cli_credentials *credentials);
struct dcom_object_exporter *object_exporter_by_oxid(struct com_context *ctx, uint64_t oxid);
struct dcom_object_exporter *object_exporter_by_ip(struct com_context *ctx, struct IUnknown *ip);
WERROR dcom_create_object(struct com_context *ctx, struct GUID *clsid, const char *server, int num_ifaces, struct GUID *iid, struct IUnknown ***ip, WERROR *results);
WERROR dcom_get_class_object(struct com_context *ctx, struct GUID *clsid, const char *server, struct GUID *iid, struct IUnknown **ip);
NTSTATUS dcom_get_pipe(struct IUnknown *iface, struct dcerpc_pipe **pp);
NTSTATUS dcom_OBJREF_from_IUnknown(struct OBJREF *o, struct IUnknown *p);
NTSTATUS dcom_IUnknown_from_OBJREF(TALLOC_CTX *mem_ctx, struct com_context *ctx, struct IUnknown **_p, struct OBJREF *o);
uint64_t dcom_get_current_oxid(void);
void dcom_add_server_credentials(struct com_context *ctx, const char *server, struct cli_credentials *credentials);
WERROR dcom_query_interface(struct IUnknown *d, uint32_t cRefs, uint16_t cIids, struct GUID *iids, struct IUnknown **ip, WERROR *results);

#include "librpc/gen_ndr/com_dcom.h"

NTSTATUS dcom_register_proxy(struct IUnknown_vtable *proxy_vtable);
struct IUnknown_vtable *dcom_proxy_vtable_by_iid(struct GUID *iid);
NTSTATUS dcom_register_marshal(struct GUID *clsid, marshal_fn marshal, unmarshal_fn unmarshal);

#include "libcli/composite/composite.h"
void dcom_release_continue(struct composite_context *cr);
#define IUnknown_ipid(d) ((d)->obj.u_objref.u_standard.std.ipid)
struct composite_context *dcom_release_send(struct IUnknown *d, TALLOC_CTX *mem_ctx);
marshal_fn dcom_marshal_by_clsid(struct GUID *clsid);
unmarshal_fn dcom_unmarshal_by_clsid(struct GUID *clsid);

struct dcom_proxy_async_call_state {
	struct IUnknown *d;
	const struct ndr_interface_table *table;
	uint32_t opnum;
	void (*continuation)(struct rpc_request *);
	TALLOC_CTX *mem_ctx;
	void *r;
};


#endif /* _DCOM_H */
