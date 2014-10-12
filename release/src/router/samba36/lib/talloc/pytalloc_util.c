/* 
   Unix SMB/CIFS implementation.
   Python/Talloc glue
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

#include <Python.h>
#include "replace.h"
#include <talloc.h>
#include "pytalloc.h"
#include <assert.h>

_PUBLIC_ PyTypeObject *PyTalloc_GetObjectType(void)
{
	static PyTypeObject *type = NULL;
	PyObject *mod;

	if (type != NULL) {
		return type;
	}

	mod = PyImport_ImportModule("talloc");
	if (mod == NULL) {
		return NULL;
	}

	type = (PyTypeObject *)PyObject_GetAttrString(mod, "Object");
	Py_DECREF(mod);

	return type;
}

/**
 * Import an existing talloc pointer into a Python object.
 */
_PUBLIC_ PyObject *py_talloc_steal_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx,
						   void *ptr)
{
	py_talloc_Object *ret = (py_talloc_Object *)py_type->tp_alloc(py_type, 0);
	ret->talloc_ctx = talloc_new(NULL);
	if (ret->talloc_ctx == NULL) {
		return NULL;
	}
	if (talloc_steal(ret->talloc_ctx, mem_ctx) == NULL) {
		return NULL;
	}
	talloc_set_name_const(ret->talloc_ctx, py_type->tp_name);
	ret->ptr = ptr;
	return (PyObject *)ret;
}

/**
 * Import an existing talloc pointer into a Python object.
 */
_PUBLIC_ PyObject *py_talloc_steal(PyTypeObject *py_type, void *ptr)
{
	return py_talloc_steal_ex(py_type, ptr, ptr);
}


/**
 * Import an existing talloc pointer into a Python object, leaving the
 * original parent, and creating a reference to the object in the python
 * object
 */
_PUBLIC_ PyObject *py_talloc_reference_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx, void *ptr)
{
	py_talloc_Object *ret;

	if (ptr == NULL) {
		Py_RETURN_NONE;
	}

	ret = (py_talloc_Object *)py_type->tp_alloc(py_type, 0);
	ret->talloc_ctx = talloc_new(NULL);
	if (ret->talloc_ctx == NULL) {
		return NULL;
	}
	if (talloc_reference(ret->talloc_ctx, mem_ctx) == NULL) {
		return NULL;
	}
	talloc_set_name_const(ret->talloc_ctx, py_type->tp_name);
	ret->ptr = ptr;
	return (PyObject *)ret;
}

static void py_cobject_talloc_free(void *ptr)
{
	talloc_free(ptr);
}

_PUBLIC_ PyObject *PyCObject_FromTallocPtr(void *ptr)
{
	if (ptr == NULL) {
		Py_RETURN_NONE;
	}
	return PyCObject_FromVoidPtr(ptr, py_cobject_talloc_free);
}

_PUBLIC_ int PyTalloc_Check(PyObject *obj)
{
	PyTypeObject *tp = PyTalloc_GetObjectType();

	return PyObject_TypeCheck(obj, tp);
}
