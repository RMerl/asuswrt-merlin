/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2009
   
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
#include "param/param.h"
#include "auth/gensec/gensec.h"
#include "libcli/util/pyerrors.h"
#include "pytalloc.h"
#include <tevent.h>

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

static PyObject *py_get_name_by_authtype(PyObject *self, PyObject *args)
{
	int type;
	const char *name;
	struct gensec_security *security;

	if (!PyArg_ParseTuple(args, "i", &type))
		return NULL;

	security = (struct gensec_security *)py_talloc_get_ptr(self);

	name = gensec_get_name_by_authtype(security, type);
	if (name == NULL)
		Py_RETURN_NONE;

	return PyString_FromString(name);
}

static struct gensec_settings *settings_from_object(PyObject *object)
{
	return NULL; /* FIXME */
}

static PyObject *py_gensec_start_client(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	NTSTATUS status;
	py_talloc_Object *self;
	struct gensec_settings *settings;
	const char *kwnames[] = { "settings", NULL };
	PyObject *py_settings;
	struct tevent_context *ev;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &py_settings))
		return NULL;

	settings = settings_from_object(py_settings);
	if (settings == NULL)
		return NULL;
	
	self = (py_talloc_Object*)type->tp_alloc(type, 0);
	if (self == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	self->talloc_ctx = talloc_new(NULL);
	if (self->talloc_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ev = tevent_context_init(self->talloc_ctx);
	if (ev == NULL) {
		PyErr_NoMemory();
		PyObject_Del(self);
		return NULL;
	}
	status = gensec_client_start(self->talloc_ctx, 
		(struct gensec_security **)&self->ptr, ev, settings);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetNTSTATUS(status);
		PyObject_DEL(self);
		return NULL;
	}
	return (PyObject *)self;
}

static PyObject *py_gensec_session_info(PyObject *self)
{
	NTSTATUS status;
	struct gensec_security *security = (struct gensec_security *)py_talloc_get_ptr(self);
	struct auth_session_info *info;
	status = gensec_session_info(security, &info);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	/* FIXME */
	Py_RETURN_NONE;
}

static PyMethodDef py_gensec_security_methods[] = {
	{ "start_client", (PyCFunction)py_gensec_start_client, METH_VARARGS|METH_KEYWORDS|METH_CLASS, 
		"S.start_client(settings) -> gensec" },
/*	{ "start_server", (PyCFunction)py_gensec_start_server, METH_VARARGS|METH_KEYWORDS|METH_CLASS, 
		"S.start_server(auth_ctx, settings) -> gensec" },*/
	{ "session_info", (PyCFunction)py_gensec_session_info, METH_NOARGS,
		"S.session_info() -> info" },
	{ "get_name_by_authtype", (PyCFunction)py_get_name_by_authtype, METH_VARARGS,
		"S.get_name_by_authtype(authtype) -> name\nLookup an auth type." },
	{ NULL }
};

static PyTypeObject Py_Security = {
	.tp_name = "Security",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = py_gensec_security_methods,
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_dealloc = py_talloc_dealloc,
};

void initgensec(void)
{
	PyObject *m;

	if (PyType_Ready(&Py_Security) < 0)
		return;

	m = Py_InitModule3("gensec", NULL, "Generic Security Interface.");
	if (m == NULL)
		return;

	Py_INCREF(&Py_Security);
	PyModule_AddObject(m, "Security", (PyObject *)&Py_Security);
}
