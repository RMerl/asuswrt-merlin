/*
   Unix SMB/CIFS implementation.
   DCOM proxy tables functionality
   Copyright (C) 2005 Jelmer Vernooij <jelmer@samba.org>
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
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/com_dcom.h"
#include "lib/com/dcom/dcom.h"

static struct dcom_proxy {
	struct IUnknown_vtable *vtable;
	struct dcom_proxy *prev, *next;
}  *proxies = NULL;

NTSTATUS dcom_register_proxy(struct IUnknown_vtable *proxy_vtable)
{
	struct dcom_proxy *proxy = talloc(talloc_autofree_context(), struct dcom_proxy);

	proxy->vtable = proxy_vtable;
	DLIST_ADD(proxies, proxy);

	return NT_STATUS_OK;
}

struct IUnknown_vtable *dcom_proxy_vtable_by_iid(struct GUID *iid)
{
	struct dcom_proxy *p;
	for (p = proxies; p; p = p->next) {
		if (GUID_equal(&p->vtable->iid, iid)) {
			return p->vtable;
		}
	}
	return NULL;
}

static struct dcom_marshal {
	struct GUID clsid;
	marshal_fn marshal;
	unmarshal_fn unmarshal;
	struct dcom_marshal *prev, *next;
}  *marshals = NULL;

NTSTATUS dcom_register_marshal(struct GUID *clsid, marshal_fn marshal, unmarshal_fn unmarshal)
{
	struct dcom_marshal *p = talloc(talloc_autofree_context(), struct dcom_marshal);

	p->clsid = *clsid;
	p->marshal = marshal;
	p->unmarshal = unmarshal;
	DLIST_ADD(marshals, p);
	return NT_STATUS_OK;
}

_PUBLIC_ marshal_fn dcom_marshal_by_clsid(struct GUID *clsid)
{
	struct dcom_marshal *p;
	for (p = marshals; p; p = p->next) {
		if (GUID_equal(&p->clsid, clsid)) {
			return p->marshal;
		}
	}
	return NULL;
}

_PUBLIC_ unmarshal_fn dcom_unmarshal_by_clsid(struct GUID *clsid)
{
	struct dcom_marshal *p;
	for (p = marshals; p; p = p->next) {
		if (GUID_equal(&p->clsid, clsid)) {
			return p->unmarshal;
		}
	}
	return NULL;
}

