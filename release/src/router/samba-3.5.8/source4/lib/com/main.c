/*
   Unix SMB/CIFS implementation.
   Main COM functionality
   Copyright (C) 2004 Jelmer Vernooij <jelmer@samba.org>

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
#include "lib/com/com.h"
#include "lib/events/events.h"
#include "librpc/gen_ndr/com_dcom.h"

WERROR com_init_ctx(struct com_context **ctx, struct tevent_context *event_ctx)
{
	*ctx = talloc(NULL, struct com_context);
	if (event_ctx == NULL) {
		event_ctx = event_context_init(*ctx);
	}
	(*ctx)->event_ctx = event_ctx;
	return WERR_OK;
}

WERROR com_create_object(struct com_context *ctx, struct GUID *clsid, int num_ifaces, struct GUID *iid, struct IUnknown **ip, WERROR *results)
{
	struct IUnknown *iunk = NULL;
	struct IClassFactory *factory;
	WERROR error;
	int i;
	struct GUID classfact_iid;

	GUID_from_string(NDR_ICLASSFACTORY_UUID, &classfact_iid);

	/* Obtain class object */
	error = com_get_class_object(ctx, clsid, &classfact_iid, (struct IUnknown **)&factory);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(3, ("Unable to obtain class object for %s\n", GUID_string(NULL, clsid)));
		return error;
	}

	/* Run IClassFactory::CreateInstance() */
	error = IClassFactory_CreateInstance(factory, ctx, NULL, &classfact_iid, &iunk);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(3, ("Error while calling IClassFactory::CreateInstance : %s\n", win_errstr(error)));
		return error;
	}

	if (!iunk) {
		DEBUG(0, ("IClassFactory_CreateInstance returned success but result pointer is still NULL!\n"));
		return WERR_GENERAL_FAILURE;
	}
	
	/* Release class object */
	IUnknown_Release(factory, ctx);
	
	error = WERR_OK;
	
	/* Do one or more QueryInterface calls */
	for (i = 0; i < num_ifaces; i++) {
		results[i] = IUnknown_QueryInterface(iunk, ctx, &iid[i], &ip[i]);
		if (!W_ERROR_IS_OK(results[i])) error = results[i];
	}

	return error;
}

WERROR com_get_class_object(struct com_context *ctx, struct GUID *clsid, struct GUID *iid, struct IUnknown **ip)
{
	struct IUnknown *iu;
	
	iu = com_class_by_clsid(ctx, clsid);
	if (!iu) {
		return WERR_CLASS_NOT_REGISTERED;
	}
	
	return IUnknown_QueryInterface(iu, ctx, iid, ip);
}
