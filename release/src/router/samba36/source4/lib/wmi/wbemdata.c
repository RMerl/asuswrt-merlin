/*
   WMI Implementation
   Copyright (C) 2006 Andrzej Hajda <andrzej.hajda@wp.pl>
   Copyright (C) 2008 Jelmer Vernooij <jelmer@samba.org>

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
#include "librpc/gen_ndr/dcom.h"
#include "librpc/gen_ndr/com_dcom.h"
#include "librpc/ndr/libndr.h"
#include "librpc/ndr/libndr_proto.h"
#include "lib/com/com.h"
#include "lib/com/dcom/dcom.h"
#include "lib/util/dlinklist.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_dcom.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "lib/talloc/talloc.h"
#include "libcli/composite/composite.h"
#include "lib/wmi/wmi.h"
#include "librpc/gen_ndr/ndr_wmi.h"

enum {
	DATATYPE_CLASSOBJECT = 2,
	DATATYPE_OBJECT = 3,
	COFLAG_IS_CLASS = 4,
};

static enum ndr_err_code marshal(TALLOC_CTX *mem_ctx, struct IUnknown *pv, struct OBJREF *o)
{
	struct ndr_push *ndr;
	struct IWbemClassObject *wco;
	struct MInterfacePointer *mp;

	mp = (struct MInterfacePointer *)((char *)o - offsetof(struct MInterfacePointer, obj)); /* FIXME:high remove this Mumbo Jumbo */
	wco = pv->object_data;
	ndr = talloc_zero(mem_ctx, struct ndr_push);
	ndr->flags = 0;
	ndr->alloc_size = 1024;
	ndr->data = talloc_array(mp, uint8_t, ndr->alloc_size);

	if (wco) {
		uint32_t ofs;
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0x12345678));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
		NDR_CHECK(ndr_push_IWbemClassObject(ndr, NDR_SCALARS | NDR_BUFFERS, wco));
		ofs = ndr->offset;
		ndr->offset = 4;
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, ofs - 8));
		ndr->offset = ofs;
	} else {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0));
	}
	o->u_objref.u_custom.pData = talloc_realloc(mp, ndr->data, uint8_t, ndr->offset);
	o->u_objref.u_custom.size = ndr->offset;
	mp->size = sizeof(struct OBJREF) - sizeof(union OBJREF_Types) + sizeof(struct u_custom) + o->u_objref.u_custom.size - 4;
	if (DEBUGLVL(9)) {
		NDR_PRINT_DEBUG(IWbemClassObject, wco);
	}
	return NDR_ERR_SUCCESS;
}

static enum ndr_err_code unmarshal(TALLOC_CTX *mem_ctx, struct OBJREF *o, struct IUnknown **pv)
{
	struct ndr_pull *ndr;
	struct IWbemClassObject *wco;
	enum ndr_err_code ndr_err;
	uint32_t u;

	mem_ctx = talloc_new(0);
	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->current_mem_ctx = mem_ctx;
	ndr->data = o->u_objref.u_custom.pData;
	ndr->data_size = o->u_objref.u_custom.size;

	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	if (!u) {
		talloc_free(*pv);
		*pv = NULL;
		return NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	if (u + 8 > ndr->data_size) {
		DEBUG(1, ("unmarshall_IWbemClassObject: Incorrect data_size"));
		return NDR_ERR_BUFSIZE;
	}
	wco = talloc_zero(*pv, struct IWbemClassObject);
	ndr->current_mem_ctx = wco;
	ndr_err = ndr_pull_IWbemClassObject(ndr, NDR_SCALARS | NDR_BUFFERS, wco);

	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && (DEBUGLVL(9))) {
		NDR_PRINT_DEBUG(IWbemClassObject, wco);
	}

	if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		(*pv)->object_data = wco;
	} else {
		talloc_free(wco);
	}
	return NDR_ERR_SUCCESS;
}

WERROR dcom_IWbemClassObject_from_WbemClassObject(struct com_context *ctx, struct IWbemClassObject **_p, struct IWbemClassObject *wco)
{
	struct IWbemClassObject *p;

	p = talloc_zero(ctx, struct IWbemClassObject);
	p->ctx = ctx;
	p->obj.signature = 0x574f454d;
	p->obj.flags = OBJREF_CUSTOM;
	GUID_from_string("dc12a681-737f-11cf-884d-00aa004b2e24", &p->obj.iid);
	GUID_from_string("4590f812-1d3a-11d0-891f-00aa004b2e24", &p->obj.u_objref.u_custom.clsid);
	p->object_data = (void *)wco;
	talloc_steal(p, p->object_data);
	*_p = p;
	return WERR_OK;
}

WERROR IWbemClassObject_GetMethod(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, struct IWbemClassObject **in, struct IWbemClassObject **out)
{
	uint32_t i;
	struct IWbemClassObject *wco;

	wco = (struct IWbemClassObject *)d->object_data;
	for (i = 0; i < wco->obj_methods->count; ++i)
		if (!strcmp(wco->obj_methods->method[i].name, name)) {
			if (in) dcom_IWbemClassObject_from_WbemClassObject(d->ctx, in, wco->obj_methods->method[i].in);
			if (out) dcom_IWbemClassObject_from_WbemClassObject(d->ctx, out, wco->obj_methods->method[i].out);
			return WERR_OK;
		}
	return WERR_NOT_FOUND;
}

void IWbemClassObject_CreateInstance(struct IWbemClassObject *wco)
{
	uint32_t i;

	wco->instance = talloc_zero(wco, struct WbemInstance);
	wco->instance->default_flags = talloc_array(wco->instance, uint8_t, wco->obj_class->__PROPERTY_COUNT);
	wco->instance->data = talloc_array(wco->instance, union CIMVAR, wco->obj_class->__PROPERTY_COUNT);
	memset(wco->instance->data, 0, sizeof(union CIMVAR) * wco->obj_class->__PROPERTY_COUNT);
	for (i = 0; i < wco->obj_class->__PROPERTY_COUNT; ++i) {
		wco->instance->default_flags[i] = 1; /* FIXME:high resolve this magic */
	}
	wco->instance->__CLASS = wco->obj_class->__CLASS;
	wco->instance->u2_4 = 4;
	wco->instance->u3_1 = 1;
}

WERROR IWbemClassObject_Clone(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject **copy)
{
	return WERR_NOT_SUPPORTED;
}

WERROR IWbemClassObject_SpawnInstance(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, uint32_t flags, struct IWbemClassObject **instance)
{
	struct IWbemClassObject *wco, *nwco;

	wco = (struct IWbemClassObject *)d->object_data;
	nwco = talloc_zero(mem_ctx, struct IWbemClassObject);
	nwco->flags = WCF_INSTANCE;
	nwco->obj_class = wco->obj_class;
	IWbemClassObject_CreateInstance(nwco);
	dcom_IWbemClassObject_from_WbemClassObject(d->ctx, instance, nwco);
	return WERR_OK;
}

WERROR IWbemClassObject_Get(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, union CIMVAR *val, enum CIMTYPE_ENUMERATION *cimtype, uint32_t *flavor)
{
	uint32_t i;
	for (i = 0; i < d->obj_class->__PROPERTY_COUNT; ++i) {
		if (!strcmp(d->obj_class->properties[i].property.name, name)) {
			duplicate_CIMVAR(mem_ctx, &d->instance->data[i], val, d->obj_class->properties[i].property.desc->cimtype);
			if (cimtype != NULL) 
				*cimtype = d->obj_class->properties[i].property.desc->cimtype;
			if (flavor != NULL) 
				*flavor = 0; /* FIXME:avg implement flavor */
			return WERR_OK;
		}
	}
	return WERR_NOT_FOUND;
}

WERROR IWbemClassObject_Put(struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, const char *name, uint32_t flags, union CIMVAR *val, enum CIMTYPE_ENUMERATION cimtype)
{
	struct IWbemClassObject *wco;
	uint32_t i;

	wco = (struct IWbemClassObject *)d->object_data;
	for (i = 0; i < wco->obj_class->__PROPERTY_COUNT; ++i) {
		if (!strcmp(wco->obj_class->properties[i].property.name, name)) {
			if (cimtype && cimtype != wco->obj_class->properties[i].property.desc->cimtype) return WERR_INVALID_PARAM;
			wco->instance->default_flags[i] = 0;
			duplicate_CIMVAR(wco->instance, val, &wco->instance->data[i], wco->obj_class->properties[i].property.desc->cimtype);
			return WERR_OK;
		}
	}
	return WERR_NOT_FOUND;
}

#define WERR_CHECK(msg) if (!W_ERROR_IS_OK(result)) { \
			      DEBUG(1, ("ERROR: %s - %s\n", msg, wmi_errstr(result))); \
	return result; \
			  } else { \
			      DEBUG(1, ("OK   : %s\n", msg)); \
			  }

struct pair_guid_ptr {
	struct GUID guid;
	void *ptr;
	struct pair_guid_ptr *next, *prev;
};

static void *get_ptr_by_guid(struct pair_guid_ptr *list, struct GUID *uuid)
{
	for (; list; list = list->next) {
	    	if (GUID_equal(&list->guid, uuid))
				return list->ptr;
	}
	return NULL;
}

static void add_pair_guid_ptr(TALLOC_CTX *mem_ctx, struct pair_guid_ptr **list, struct GUID *uuid, void *ptr)
{
	struct pair_guid_ptr *e;

	e = talloc(mem_ctx, struct pair_guid_ptr);
	e->guid = *uuid;
	e->ptr = ptr;
	talloc_steal(e, ptr);
	DLIST_ADD(*list, e);
}

struct IEnumWbemClassObject_data {
	struct GUID guid;
	struct IWbemFetchSmartEnum *pFSE;
	struct IWbemWCOSmartEnum *pSE;
	struct pair_guid_ptr *cache;
};
#define NDR_CHECK_EXPR(expr) do { if (!(expr)) {\
					DEBUG(0, ("%s(%d): WBEMDATA_ERR(0x%08X): Error parsing(%s)\n", __FILE__, __LINE__, ndr->offset, #expr)); \
					return NDR_ERR_VALIDATE; \
					} \
				    } while(0)

#define NDR_CHECK_CONST(val, exp) NDR_CHECK_EXPR((val) == (exp))


static enum ndr_err_code WBEMDATA_Parse(TALLOC_CTX *mem_ctx, uint8_t *data, uint32_t size, struct IEnumWbemClassObject *d, uint32_t uCount, struct IWbemClassObject **apObjects)
{
	struct ndr_pull *ndr;
	uint32_t u, i, ofs_next;
	uint8_t u8, datatype;
	struct GUID guid;
	struct IEnumWbemClassObject_data *ecod;

	if (!uCount) 
		return NDR_ERR_BAD_SWITCH;

	ecod = d->object_data;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->current_mem_ctx = d->ctx;
	ndr->data = data;
	ndr->data_size = size;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);

	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, 0x0);
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, *(const uint32_t *)"WBEM");
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, *(const uint32_t *)"DATA");
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, 0x1A); /* Length of header */
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_PULL_NEED_BYTES(ndr, u + 6);
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, 0x0);
	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &u8));
	NDR_CHECK_CONST(u8, 0x01); /* Major Version */
	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &u8));
	NDR_CHECK_EXPR(u8 <= 1); /* Minor Version 0 - Win2000, 1 - XP/2003 */
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, 0x8); /* Length of header */
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_PULL_NEED_BYTES(ndr, u);
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, 0xC); /* Length of header */
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_PULL_NEED_BYTES(ndr, u + 4);
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
	NDR_CHECK_CONST(u, uCount);
	for (i = 0; i < uCount; ++i) {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
		NDR_CHECK_CONST(u, 0x9); /* Length of header */
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
		NDR_PULL_NEED_BYTES(ndr, u + 1);
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &datatype));
		ofs_next = ndr->offset + u;
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
		NDR_CHECK_CONST(u, 0x18); /* Length of header */
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &u));
		NDR_PULL_NEED_BYTES(ndr, u + 16);
		NDR_CHECK(ndr_pull_GUID(ndr, NDR_SCALARS, &guid));
		switch (datatype) {
		case DATATYPE_CLASSOBJECT:
			apObjects[i] = talloc_zero(d->ctx, struct IWbemClassObject);
			ndr->current_mem_ctx = apObjects[i];
			NDR_CHECK(ndr_pull_WbemClassObject(ndr, NDR_SCALARS|NDR_BUFFERS, apObjects[i]));
			ndr->current_mem_ctx = d->ctx;
			add_pair_guid_ptr(ecod, &ecod->cache, &guid, apObjects[i]->obj_class);
			break;
		case DATATYPE_OBJECT:
			apObjects[i] = talloc_zero(d->ctx, struct IWbemClassObject);
			apObjects[i]->obj_class = get_ptr_by_guid(ecod->cache, &guid);
			(void)talloc_reference(apObjects[i], apObjects[i]->obj_class);
			ndr->current_mem_ctx = apObjects[i];
			NDR_CHECK(ndr_pull_WbemClassObject_Object(ndr, NDR_SCALARS|NDR_BUFFERS, apObjects[i]));
			ndr->current_mem_ctx = d->ctx;
			break;
		default:
			DEBUG(0, ("WBEMDATA_Parse: Data type %d not supported\n", datatype));
			return NDR_ERR_BAD_SWITCH;
		}
		ndr->offset = ofs_next;
    		if (DEBUGLVL(9)) {
			NDR_PRINT_DEBUG(IWbemClassObject, apObjects[i]);
		}
	}
	return NDR_ERR_SUCCESS;
}

WERROR IEnumWbemClassObject_SmartNext(struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, uint32_t uCount, struct IWbemClassObject **apObjects, uint32_t *puReturned)
{
	WERROR result;
	NTSTATUS status;
	struct IEnumWbemClassObject_data *ecod;
	TALLOC_CTX *loc_ctx;
	uint32_t size;
	uint8_t *data;

	loc_ctx = talloc_new(0);
	ecod = d->object_data;
	if (!ecod) {
		struct GUID iid;
		WERROR coresult;

		d->object_data = ecod = talloc_zero(d, struct IEnumWbemClassObject_data);
		GUID_from_string(COM_IWBEMFETCHSMARTENUM_UUID, &iid);
		result = dcom_query_interface((struct IUnknown *)d, 5, 1, &iid, (struct IUnknown **)&ecod->pFSE, &coresult);
		WERR_CHECK("dcom_query_interface.");
		result = coresult;
		WERR_CHECK("Retrieve enumerator of result(IWbemFetchSmartEnum).");

		result = IWbemFetchSmartEnum_Fetch(ecod->pFSE, mem_ctx, &ecod->pSE);
		WERR_CHECK("Retrieve enumerator of result(IWbemWCOSmartEnum).");

		ecod->guid = GUID_random();
		d->vtable->Release_send = dcom_proxy_IEnumWbemClassObject_Release_send;
	}

	result = IWbemWCOSmartEnum_Next(ecod->pSE, loc_ctx, &ecod->guid, lTimeout, uCount, puReturned, &size, &data);
	if (!W_ERROR_EQUAL(result, WERR_BADFUNC)) {
		WERR_CHECK("IWbemWCOSmartEnum_Next.");
	}

	if (data) {
		NDR_CHECK(WBEMDATA_Parse(mem_ctx, data, size, d, *puReturned, apObjects));
	}
	if (!W_ERROR_IS_OK(result)) {
		status = werror_to_ntstatus(result);
		DEBUG(9, ("dcom_proxy_IEnumWbemClassObject_Next: %s - %s\n", nt_errstr(status), get_friendly_nt_error_msg(status)));
	}
	talloc_free(loc_ctx);
	return result;
}

struct composite_context *dcom_proxy_IEnumWbemClassObject_Release_send(struct IUnknown *d, TALLOC_CTX *mem_ctx)
{
	struct composite_context *c, *cr;
	struct REMINTERFACEREF iref[3];
	struct dcom_object_exporter *ox;
	struct IEnumWbemClassObject_data *ecod;
	int n;

	c = composite_create(d->ctx, d->ctx->event_ctx);
	if (c == NULL) return NULL;
	c->private_data = d;

	ox = object_exporter_by_ip(d->ctx, d);
	iref[0].ipid = IUnknown_ipid(d);
	iref[0].cPublicRefs = 5;
	iref[0].cPrivateRefs = 0;
	n = 1;

	ecod = d->object_data;
	if (ecod) {
		if (ecod->pFSE) {
			talloc_steal(d, ecod->pFSE);
			iref[n].ipid = IUnknown_ipid(ecod->pFSE);
			iref[n].cPublicRefs = 5;
			iref[n].cPrivateRefs = 0;
			++n;
		}
		if (ecod->pSE) {
			talloc_steal(d, ecod->pSE);
			iref[n].ipid = IUnknown_ipid(ecod->pSE);
			iref[n].cPublicRefs = 5;
			iref[n].cPrivateRefs = 0;
			++n;
		}
	}
	cr = IRemUnknown_RemRelease_send(ox->rem_unknown, mem_ctx, n, iref);

	composite_continue(c, cr, dcom_release_continue, c);
	return c;
}

NTSTATUS dcom_proxy_IWbemClassObject_init(void)
{
	struct GUID clsid;
	GUID_from_string("4590f812-1d3a-11d0-891f-00aa004b2e24", &clsid);
	dcom_register_marshal(&clsid, marshal, unmarshal);

#if 0
	struct IEnumWbemClassObject_vtable *proxy_vtable;
	proxy_vtable = (struct IEnumWbemClassObject_vtable *)dcom_proxy_vtable_by_iid((struct GUID *)&dcerpc_table_IEnumWbemClassObject.syntax_id.uuid);
	if (proxy_vtable)
		proxy_vtable->Release_send = dcom_proxy_IEnumWbemClassObject_Release_send;
	else
		DEBUG(0, ("WARNING: IEnumWbemClassObject should be initialized before IWbemClassObject."));
#endif

	return NT_STATUS_OK;
}
