/*
 *  Unix SMB/CIFS implementation.
 *  Python bindings for libpolicy
 *  Copyright (C) Jelmer Vernooij 2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include "includes.h"
#include "policy.h"
#include "libcli/util/pyerrors.h"

static PyObject *py_get_gpo_flags(PyObject *self, PyObject *args)
{
	int flags;
	PyObject *py_ret;
	const char **ret;
	TALLOC_CTX *mem_ctx;
	int i;
	NTSTATUS status;

	if (!PyArg_ParseTuple(args, "i", &flags))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = gp_get_gpo_flags(mem_ctx, flags, &ret);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetNTSTATUS(status);
		talloc_free(mem_ctx);
		return NULL;
	}

	py_ret = PyList_New(0);
	for (i = 0; ret[i]; i++) {
		PyObject *item = PyString_FromString(ret[i]);
		if (item == NULL) {
			talloc_free(mem_ctx);
			Py_DECREF(py_ret);
			PyErr_NoMemory();
			return NULL;
		}
		PyList_Append(py_ret, item);
	}

	talloc_free(mem_ctx);

	return py_ret;
}

static PyObject *py_get_gplink_options(PyObject *self, PyObject *args)
{
	int flags;
	PyObject *py_ret;
	const char **ret;
	TALLOC_CTX *mem_ctx;
	int i;
	NTSTATUS status;

	if (!PyArg_ParseTuple(args, "i", &flags))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = gp_get_gplink_options(mem_ctx, flags, &ret);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetNTSTATUS(status);
		talloc_free(mem_ctx);
		return NULL;
	}

	py_ret = PyList_New(0);
	for (i = 0; ret[i]; i++) {
		PyObject *item = PyString_FromString(ret[i]);
		if (item == NULL) {
			talloc_free(mem_ctx);
			Py_DECREF(py_ret);
			PyErr_NoMemory();
			return NULL;
		}
		PyList_Append(py_ret, item);
	}

	talloc_free(mem_ctx);

	return py_ret;
}

static PyMethodDef py_policy_methods[] = {
	{ "get_gpo_flags", (PyCFunction)py_get_gpo_flags, METH_VARARGS,
		"get_gpo_flags(flags) -> list" },
    { "get_gplink_options", (PyCFunction)py_get_gplink_options, METH_VARARGS,
        "get_gplink_options(options) -> list" },
	{ NULL }
};

void initpolicy(void)
{
	PyObject *m;

	m = Py_InitModule3("policy", py_policy_methods, "(Group) Policy manipulation");
	if (!m)
		return;

	PyModule_AddObject(m, "GPO_FLAG_USER_DISABLE",
					   PyInt_FromLong(GPO_FLAG_USER_DISABLE));
	PyModule_AddObject(m, "GPO_MACHINE_USER_DISABLE",
					   PyInt_FromLong(GPO_FLAG_MACHINE_DISABLE));
	PyModule_AddObject(m, "GPLINK_OPT_DISABLE",
					   PyInt_FromLong(GPLINK_OPT_DISABLE ));
	PyModule_AddObject(m, "GPLINK_OPT_ENFORCE ",
					   PyInt_FromLong(GPLINK_OPT_ENFORCE ));
}
