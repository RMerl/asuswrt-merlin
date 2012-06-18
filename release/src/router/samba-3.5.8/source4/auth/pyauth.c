/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2008
   
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
#include "param/param.h"
#include "pyauth.h"
#include "auth/system_session_proto.h"
#include "param/pyparam.h"
#include "libcli/security/security.h"


PyTypeObject PyAuthSession = {
	.tp_name = "AuthSession",
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_dealloc = py_talloc_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_repr = py_talloc_default_repr,
};

PyObject *PyAuthSession_FromSession(struct auth_session_info *session)
{
	return py_talloc_reference(&PyAuthSession, session);
}

static PyObject *py_system_session(PyObject *module, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx = NULL;
	struct auth_session_info *session;
	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	lp_ctx = lp_from_py_object(py_lp_ctx);
	if (lp_ctx == NULL)
		return NULL;

	session = system_session(NULL, lp_ctx);

	return PyAuthSession_FromSession(session);
}


static PyObject *py_system_session_anon(PyObject *module, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx;
	struct auth_session_info *session;
	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	lp_ctx = lp_from_py_object(py_lp_ctx);
	if (lp_ctx == NULL)
		return NULL;

	session = system_session_anon(NULL, lp_ctx);

	return PyAuthSession_FromSession(session);
}

static PyObject *py_admin_session(PyObject *module, PyObject *args)
{
	PyObject *py_lp_ctx;
	PyObject *py_sid;
	struct loadparm_context *lp_ctx = NULL;
	struct auth_session_info *session;
	struct dom_sid *domain_sid = NULL;
	if (!PyArg_ParseTuple(args, "OO", &py_lp_ctx, &py_sid))
		return NULL;

	lp_ctx = lp_from_py_object(py_lp_ctx);
	if (lp_ctx == NULL)
		return NULL;

	domain_sid = dom_sid_parse_talloc(NULL, PyString_AsString(py_sid));
	session = admin_session(NULL, lp_ctx, domain_sid);

	return PyAuthSession_FromSession(session);
}

static PyMethodDef py_auth_methods[] = {
	{ "system_session", (PyCFunction)py_system_session, METH_VARARGS, NULL },
	{ "system_session_anonymous", (PyCFunction)py_system_session_anon, METH_VARARGS, NULL },
	{ "admin_session", (PyCFunction)py_admin_session, METH_VARARGS, NULL },
	{ NULL },
};

void initauth(void)
{
	PyObject *m;

	if (PyType_Ready(&PyAuthSession) < 0)
		return;

	m = Py_InitModule3("auth", py_auth_methods, "Authentication and authorization support.");
	if (m == NULL)
		return;

	Py_INCREF(&PyAuthSession);
	PyModule_AddObject(m, "AuthSession", (PyObject *)&PyAuthSession);
}
