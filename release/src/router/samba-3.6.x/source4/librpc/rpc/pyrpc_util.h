/*
   Unix SMB/CIFS implementation.

   Python interface to DCE/RPC library - utility functions.

   Copyright (C) 2010 Jelmer Vernooij <jelmer@samba.org>
   Copyright (C) 2010 Andrew Tridgell <tridge@samba.org>

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

#ifndef __PYRPC_UTIL_H__
#define __PYRPC_UTIL_H__

#include "librpc/rpc/pyrpc.h"

#define PyErr_FromNdrError(err) Py_BuildValue("(is)", err, ndr_map_error2string(err))

#define PyErr_SetNdrError(err) \
		PyErr_SetObject(PyExc_RuntimeError, PyErr_FromNdrError(err))

void PyErr_SetDCERPCStatus(struct dcerpc_pipe *p, NTSTATUS status);

typedef NTSTATUS (*py_dcerpc_call_fn) (struct dcerpc_binding_handle *, TALLOC_CTX *, void *);
typedef bool (*py_data_pack_fn) (PyObject *args, PyObject *kwargs, void *r);
typedef PyObject *(*py_data_unpack_fn) (void *r);

struct PyNdrRpcMethodDef {
	const char *name;
	const char *doc;
	py_dcerpc_call_fn call;
	py_data_pack_fn pack_in_data;
	py_data_unpack_fn unpack_out_data;
	uint32_t opnum;
	const struct ndr_interface_table *table;
};

bool py_check_dcerpc_type(PyObject *obj, const char *module, const char *type_name);
bool PyInterface_AddNdrRpcMethods(PyTypeObject *object, const struct PyNdrRpcMethodDef *mds);
PyObject *py_dcerpc_interface_init_helper(PyTypeObject *type, PyObject *args, PyObject *kwargs, const struct ndr_interface_table *table);

PyObject *py_return_ndr_struct(const char *module_name, const char *type_name,
			       TALLOC_CTX *r_ctx, void *r);

PyObject *PyString_FromStringOrNULL(const char *str);

#endif /* __PYRPC_UTIL_H__ */
