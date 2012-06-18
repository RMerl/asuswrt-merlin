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

#include <stdint.h>
#include <stdbool.h>

#include "includes.h"
#include "param/param.h"
#include "param/loadparm.h"
#include <Python.h>
#include "pytalloc.h"

#define PyLoadparmContext_AsLoadparmContext(obj) py_talloc_get_type(obj, struct loadparm_context)

_PUBLIC_ struct loadparm_context *lp_from_py_object(PyObject *py_obj)
{
    struct loadparm_context *lp_ctx;

    if (PyString_Check(py_obj)) {
        lp_ctx = loadparm_init(NULL);
        if (!lp_load(lp_ctx, PyString_AsString(py_obj))) {
            talloc_free(lp_ctx);
			PyErr_Format(PyExc_RuntimeError, "Unable to load %s", 
						 PyString_AsString(py_obj));
            return NULL;
        }
        return lp_ctx;
    }

    if (py_obj == Py_None) {
        lp_ctx = loadparm_init(NULL);
		/* We're not checking that loading the file succeeded *on purpose */
        lp_load_default(lp_ctx);
        return lp_ctx;
    }

    return PyLoadparmContext_AsLoadparmContext(py_obj);
}

struct loadparm_context *py_default_loadparm_context(TALLOC_CTX *mem_ctx)
{
    struct loadparm_context *ret;
    ret = loadparm_init(mem_ctx);
    if (!lp_load_default(ret))
        return NULL;
    return ret;
}


