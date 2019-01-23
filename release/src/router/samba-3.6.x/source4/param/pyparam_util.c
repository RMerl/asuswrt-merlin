/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
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

#include <Python.h>
#include "includes.h"
#include "param/param.h"
#include "param/loadparm.h"
#include "lib/talloc/pytalloc.h"

#define PyLoadparmContext_AsLoadparmContext(obj) py_talloc_get_type(obj, struct loadparm_context)

_PUBLIC_ struct loadparm_context *lpcfg_from_py_object(TALLOC_CTX *mem_ctx, PyObject *py_obj)
{
	struct loadparm_context *lp_ctx;
	PyObject *param_mod;
	PyTypeObject *lp_type;
	bool is_lpobj;

	if (PyString_Check(py_obj)) {
		lp_ctx = loadparm_init_global(false);
		if (!lpcfg_load(lp_ctx, PyString_AsString(py_obj))) {
			PyErr_Format(PyExc_RuntimeError, "Unable to load %s", 
				     PyString_AsString(py_obj));
			return NULL;
		}
		return lp_ctx;
	}

	if (py_obj == Py_None) {
		return loadparm_init_global(true);
	}

	param_mod = PyImport_ImportModule("samba.param");
	if (param_mod == NULL) {
		return NULL;
	}

	lp_type = (PyTypeObject *)PyObject_GetAttrString(param_mod, "LoadParm");
	Py_DECREF(param_mod);
	if (lp_type == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to import LoadParm");
		return NULL;
	}

	is_lpobj = PyObject_TypeCheck(py_obj, lp_type);
	Py_DECREF(lp_type);
	if (is_lpobj) {
		return talloc_reference(mem_ctx, PyLoadparmContext_AsLoadparmContext(py_obj));
	}

	PyErr_SetNone(PyExc_TypeError);
	return NULL;
}

struct loadparm_context *py_default_loadparm_context(TALLOC_CTX *mem_ctx)
{
	return loadparm_init_global(true);
}


