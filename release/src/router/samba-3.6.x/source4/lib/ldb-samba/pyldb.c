/*
   Unix SMB/CIFS implementation.

   Python interface to ldb, Samba-specific functions

   Copyright (C) 2007-2010 Jelmer Vernooij <jelmer@samba.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>
#include "includes.h"
#include <ldb.h>
#include <pyldb.h>
#include "param/pyparam.h"
#include "auth/credentials/pycredentials.h"
#include "ldb_wrap.h"
#include "lib/ldb-samba/ldif_handlers.h"
#include "auth/pyauth.h"

void init_ldb(void);

static PyObject *pyldb_module;
static PyObject *py_ldb_error;
staticforward PyTypeObject PySambaLdb;

static void PyErr_SetLdbError(PyObject *error, int ret, struct ldb_context *ldb_ctx)
{
	if (ret == LDB_ERR_PYTHON_EXCEPTION)
		return; /* Python exception should already be set, just keep that */

	PyErr_SetObject(error, 
			Py_BuildValue(discard_const_p(char, "(i,s)"), ret,
			ldb_ctx == NULL?ldb_strerror(ret):ldb_errstring(ldb_ctx)));
}

static PyObject *py_ldb_set_loadparm(PyObject *self, PyObject *args)
{
	PyObject *py_lp_ctx;
	struct loadparm_context *lp_ctx;
	struct ldb_context *ldb;

	if (!PyArg_ParseTuple(args, "O", &py_lp_ctx))
		return NULL;

	ldb = PyLdb_AsLdbContext(self);

	lp_ctx = lpcfg_from_py_object(ldb, py_lp_ctx);
	if (lp_ctx == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected loadparm object");
		return NULL;
	}

	ldb_set_opaque(ldb, "loadparm", lp_ctx);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_set_credentials(PyObject *self, PyObject *args)
{
	PyObject *py_creds;
	struct cli_credentials *creds;
	struct ldb_context *ldb;

	if (!PyArg_ParseTuple(args, "O", &py_creds))
		return NULL;

	creds = cli_credentials_from_py_object(py_creds);
	if (creds == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected credentials object");
		return NULL;
	}

	ldb = PyLdb_AsLdbContext(self);

	ldb_set_opaque(ldb, "credentials", creds);

	Py_RETURN_NONE;
}

/* XXX: This function really should be in libldb's pyldb.c */
static PyObject *py_ldb_set_opaque_integer(PyObject *self, PyObject *args)
{
	int value;
	int *old_val, *new_val;
	char *py_opaque_name, *opaque_name_talloc;
	struct ldb_context *ldb;
	int ret;
	TALLOC_CTX *tmp_ctx;

	if (!PyArg_ParseTuple(args, "si", &py_opaque_name, &value))
		return NULL;

	ldb = PyLdb_AsLdbContext(self);

	/* see if we have a cached copy */
	old_val = (int *)ldb_get_opaque(ldb, py_opaque_name);
	/* XXX: We shouldn't just blindly assume that the value that is 
	 * already present has the size of an int and is not shared 
	 * with other code that may rely on it not changing. 
	 * JRV 20100403 */

	if (old_val) {
		*old_val = value;
		Py_RETURN_NONE;
	}

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	new_val = talloc(tmp_ctx, int);
	if (new_val == NULL) {
		talloc_free(tmp_ctx);
		PyErr_NoMemory();
		return NULL;
	}

	opaque_name_talloc = talloc_strdup(tmp_ctx, py_opaque_name);
	if (opaque_name_talloc == NULL) {
		talloc_free(tmp_ctx);
		PyErr_NoMemory();
		return NULL;
	}

	*new_val = value;

	/* cache the domain_sid in the ldb */
	ret = ldb_set_opaque(ldb, opaque_name_talloc, new_val);

	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		PyErr_SetLdbError(py_ldb_error, ret, ldb);
		return NULL;
	}

	talloc_steal(ldb, new_val);
	talloc_steal(ldb, opaque_name_talloc);
	talloc_free(tmp_ctx);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_set_utf8_casefold(PyObject *self)
{
	struct ldb_context *ldb;

	ldb = PyLdb_AsLdbContext(self);

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_set_session_info(PyObject *self, PyObject *args)
{
	PyObject *py_session_info;
	struct auth_session_info *info;
	struct ldb_context *ldb;
	PyObject *mod_samba_auth;
	PyObject *PyAuthSession_Type;
	bool ret;

	mod_samba_auth = PyImport_ImportModule("samba.auth");
	if (mod_samba_auth == NULL)
		return NULL;

	PyAuthSession_Type = PyObject_GetAttrString(mod_samba_auth, "AuthSession");
	if (PyAuthSession_Type == NULL)
		return NULL;

	ret = PyArg_ParseTuple(args, "O!", PyAuthSession_Type, &py_session_info);

	Py_DECREF(PyAuthSession_Type);
	Py_DECREF(mod_samba_auth);

	if (!ret)
		return NULL;

	ldb = PyLdb_AsLdbContext(self);

	info = PyAuthSession_AsSession(py_session_info);

	ldb_set_opaque(ldb, "sessionInfo", info);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_register_samba_handlers(PyObject *self)
{
	struct ldb_context *ldb;
	int ret;

	/* XXX: Perhaps call this from PySambaLdb's init function ? */

	ldb = PyLdb_AsLdbContext(self);
	ret = ldb_register_samba_handlers(ldb);

	PyErr_LDB_ERROR_IS_ERR_RAISE(py_ldb_error, ret, ldb);

	Py_RETURN_NONE;
}

static PyMethodDef py_samba_ldb_methods[] = {
	{ "set_loadparm", (PyCFunction)py_ldb_set_loadparm, METH_VARARGS, 
		"ldb_set_loadparm(session_info)\n"
		"Set loadparm context to use when connecting." },
	{ "set_credentials", (PyCFunction)py_ldb_set_credentials, METH_VARARGS,
		"ldb_set_credentials(credentials)\n"
		"Set credentials to use when connecting." },
	{ "set_opaque_integer", (PyCFunction)py_ldb_set_opaque_integer,
		METH_VARARGS, NULL },
	{ "set_utf8_casefold", (PyCFunction)py_ldb_set_utf8_casefold, 
		METH_NOARGS,
		"ldb_set_utf8_casefold()\n"
		"Set the right Samba casefolding function for UTF8 charset." },
	{ "register_samba_handlers", (PyCFunction)py_ldb_register_samba_handlers,
		METH_NOARGS,
		"register_samba_handlers()\n"
		"Register Samba-specific LDB modules and schemas." },
	{ "set_session_info", (PyCFunction)py_ldb_set_session_info, METH_VARARGS,
		"set_session_info(session_info)\n"
		"Set session info to use when connecting." },
	{ NULL },
};

static PyTypeObject PySambaLdb = {
	.tp_name = "samba._ldb.Ldb",
	.tp_doc = "Connection to a LDB database.",
	.tp_methods = py_samba_ldb_methods,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};

void init_ldb(void)
{
	PyObject *m;

	pyldb_module = PyImport_ImportModule("ldb");
	if (pyldb_module == NULL)
		return;

	PySambaLdb.tp_base = (PyTypeObject *)PyObject_GetAttrString(pyldb_module, "Ldb");
	if (PySambaLdb.tp_base == NULL)
		return;

	py_ldb_error = PyObject_GetAttrString(pyldb_module, "LdbError");

	if (PyType_Ready(&PySambaLdb) < 0)
		return;

	m = Py_InitModule3("_ldb", NULL, "Samba-specific LDB python bindings");
	if (m == NULL)
		return;

	Py_INCREF(&PySambaLdb);
	PyModule_AddObject(m, "Ldb", (PyObject *)&PySambaLdb);
}
