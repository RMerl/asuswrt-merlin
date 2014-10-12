/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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
#include "librpc/ndr/libndr.h"

static PyObject *uuid_random(PyObject *self, PyObject *args)
{
	struct GUID guid;
	PyObject *pyobj;
	char *str;

	if (!PyArg_ParseTuple(args, ""))
	        return NULL;

	guid = GUID_random();

	str = GUID_string(NULL, &guid);
	if (str == NULL) {
		PyErr_SetString(PyExc_TypeError, "can't convert uuid to string");
		return NULL;
	}

	pyobj = PyString_FromString(str);

	talloc_free(str);

	return pyobj;
}

static PyMethodDef methods[] = {
	{ "uuid4", (PyCFunction)uuid_random, METH_VARARGS, NULL},
	{ NULL, NULL }
};

void inituuid(void)
{
	PyObject *mod = Py_InitModule3("uuid", methods, "UUID helper routines");
	if (mod == NULL)
		return;
}
