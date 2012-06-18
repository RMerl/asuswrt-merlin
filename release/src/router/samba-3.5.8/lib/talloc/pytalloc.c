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

#include "replace.h"
#include <talloc.h>
#include <pytalloc.h>

/**
 * Simple dealloc for talloc-wrapping PyObjects
 */
void py_talloc_dealloc(PyObject* self)
{
	py_talloc_Object *obj = (py_talloc_Object *)self;
	talloc_free(obj->talloc_ctx);
	obj->talloc_ctx = NULL;
	self->ob_type->tp_free(self);
}

/**
 * Import an existing talloc pointer into a Python object.
 */
PyObject *py_talloc_steal_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx, 
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
	ret->ptr = ptr;
	return (PyObject *)ret;
}


/**
 * Import an existing talloc pointer into a Python object, leaving the
 * original parent, and creating a reference to the object in the python
 * object
 */
PyObject *py_talloc_reference_ex(PyTypeObject *py_type, TALLOC_CTX *mem_ctx, void *ptr)
{
	py_talloc_Object *ret = (py_talloc_Object *)py_type->tp_alloc(py_type, 0);
	ret->talloc_ctx = talloc_new(NULL);
	if (ret->talloc_ctx == NULL) {
		return NULL;
	}
	if (talloc_reference(ret->talloc_ctx, mem_ctx) == NULL) {
		return NULL;
	}
	ret->ptr = ptr;
	return (PyObject *)ret;
}

/**
 * Default (but slightly more useful than the default) implementation of Repr().
 */
PyObject *py_talloc_default_repr(PyObject *obj)
{
	py_talloc_Object *talloc_obj = (py_talloc_Object *)obj;
	PyTypeObject *type = (PyTypeObject*)PyObject_Type(obj);

	return PyString_FromFormat("<%s talloc object at 0x%p>", 
				   type->tp_name, talloc_obj->ptr);
}

static void py_cobject_talloc_free(void *ptr)
{
	talloc_free(ptr);
}

PyObject *PyCObject_FromTallocPtr(void *ptr)
{
	return PyCObject_FromVoidPtr(ptr, py_cobject_talloc_free);
}
