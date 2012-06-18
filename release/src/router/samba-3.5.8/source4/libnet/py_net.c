/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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
#include <Python.h>
#include "libnet.h"
#include "auth/credentials/pycredentials.h"
#include "libcli/security/security.h"
#include "lib/events/events.h"
#include "param/param.h"

/* FIXME: This prototype should be in param/pyparam.h */
struct loadparm_context *py_default_loadparm_context(TALLOC_CTX *mem_ctx);

static struct libnet_context *py_net_ctx(PyObject *obj, struct tevent_context *ev, struct cli_credentials *creds)
{
/* FIXME: Use obj */
	struct libnet_context *libnet;
	libnet = libnet_context_init(ev, py_default_loadparm_context(NULL));
	if (!libnet) {
		return NULL;
	}
	libnet->cred = creds;
	return libnet;
}

static PyObject *py_net_join(PyObject *cls, PyObject *args, PyObject *kwargs)
{
	struct libnet_Join r;
	NTSTATUS status;
	PyObject *result;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev;
	struct libnet_context *libnet_ctx;
	struct cli_credentials *creds;
	PyObject *py_creds;	
	const char *kwnames[] = { "domain_name", "netbios_name", "join_type", "level", "credentials", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ssiiO:Join", discard_const_p(char *, kwnames), 
					 &r.in.domain_name, &r.in.netbios_name, 
					 &r.in.join_type, &r.in.level, &py_creds))
		return NULL;

	/* FIXME: we really need to get a context from the caller or we may end
	 * up with 2 event contexts */
	ev = s4_event_context_init(NULL);
	mem_ctx = talloc_new(ev);

	creds = cli_credentials_from_py_object(py_creds);
	if (creds == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected credentials object");
		return NULL;
	}

	libnet_ctx = py_net_ctx(cls, ev, creds);

	status = libnet_Join(libnet_ctx, mem_ctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetString(PyExc_RuntimeError, r.out.error_string);
		talloc_free(mem_ctx);
		return NULL;
	}

	result = Py_BuildValue("sss", r.out.join_password, 
			       dom_sid_string(mem_ctx, r.out.domain_sid),
			       r.out.domain_name);

	talloc_free(mem_ctx);

	if (result == NULL)
		return NULL;

	return result;
}

static char py_net_join_doc[] = "join(domain_name, netbios_name, join_type, level) -> (join_password, domain_sid, domain_name)\n\n" \
"Join the domain with the specified name.";

static struct PyMethodDef net_methods[] = {
	{"Join", (PyCFunction)py_net_join, METH_VARARGS|METH_KEYWORDS, py_net_join_doc},
	{NULL }
};

void initnet(void)
{
	Py_InitModule("net", net_methods);
}
