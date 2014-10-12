/*
   Unix SMB/CIFS implementation.
   COM class tables
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
#include "librpc/gen_ndr/ndr_misc.h"

/* Specific implementation of one or more interfaces */
struct com_class
{
	const char *progid;
	struct GUID clsid;

	struct IUnknown *class_object;
	struct com_class *prev, *next;
} * running_classes = NULL;

static struct IUnknown *get_com_class_running(const struct GUID *clsid)
{
	struct com_class *c = running_classes;

	while(c) {

		if (GUID_equal(clsid, &c->clsid)) {
			return c->class_object;
		}

		c = c->next;
	}

	return NULL;
}

static struct IUnknown *get_com_class_so(TALLOC_CTX *mem_ctx, const struct GUID *clsid)
{
	char *module_name;
	char *clsid_str;
	void *mod;
	get_class_object_function f;

	clsid_str = GUID_string(mem_ctx, clsid);
	module_name = talloc_asprintf(mem_ctx, "%s.so", clsid_str);
	talloc_free(clsid_str);

	mod = dlopen(module_name, 0);

	if (!mod) {
		return NULL;
	}
	
	f = dlsym(mod, "get_class_object");

	if (!f) {
		return NULL;
	}

	return f(clsid);
}

struct IUnknown *com_class_by_clsid(struct com_context *ctx, const struct GUID *clsid)
{
	struct IUnknown *c;
	
	/* Check list of running COM classes first */
	c = get_com_class_running(clsid);

	if (c != NULL) {
		return c;
	}

	c = get_com_class_so(ctx, clsid);

	if (c != NULL) {
		return c;
	}
	
	return NULL;
}

NTSTATUS com_register_running_class(struct GUID *clsid, const char *progid, struct IUnknown *p)
{
	struct com_class *l = talloc_zero(running_classes?running_classes:talloc_autofree_context(), struct com_class);

	l->clsid = *clsid;
	l->progid = talloc_strdup(l, progid);
	l->class_object = p;

	DLIST_ADD(running_classes, l);
	
	return NT_STATUS_OK;
}
