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

#ifndef _PY_TALLOC_H_
#define _PY_TALLOC_H_

#include <Python.h>
#include <talloc.h>

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *talloc_ctx;
	void *ptr;
} py_talloc_Object;

/* Deallocate a py_talloc_Object */
void py_talloc_dealloc(PyObject* self);

/* Retrieve the pointer for a py_talloc_object. Like talloc_get_type() 
 * but for py_talloc_Objects. */

/* FIXME: Call PyErr_SetString(PyExc_TypeError, "expected " __STR(type) ") 
 * when talloc_get_type() returns NULL. */
#define py_talloc_get_type(py_obj, type) (talloc_get_type(py_talloc_get_ptr(py_obj), type))

#define py_talloc_get_ptr(py_obj) (((py_talloc_Object *)py_obj)->ptr)
#define py_talloc_get_mem_ctx(py_obj)  ((py_talloc_Object *)py_obj)->talloc_ctx

PyObject *py_talloc_steal_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx, void *ptr);
PyObject *py_talloc_reference_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx, void *ptr);
#define py_talloc_steal(py_type, talloc_ptr) py_talloc_steal_ex(py_type, talloc_ptr, talloc_ptr)
#define py_talloc_reference(py_type, talloc_ptr) py_talloc_reference_ex(py_type, talloc_ptr, talloc_ptr)

/* Sane default implementation of reprfunc. */
PyObject *py_talloc_default_repr(PyObject *py_obj);

#define py_talloc_new(type, typeobj) py_talloc_steal(typeobj, talloc_zero(NULL, type))

PyObject *PyCObject_FromTallocPtr(void *);

#endif /* _PY_TALLOC_H_ */
