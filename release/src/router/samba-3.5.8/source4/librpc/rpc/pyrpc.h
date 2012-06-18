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

#ifndef _PYRPC_H_
#define _PYRPC_H_

#include "libcli/util/pyerrors.h"
#include "librpc/rpc/dcerpc.h"

#define PY_CHECK_TYPE(type, var, fail) \
	if (!PyObject_TypeCheck(var, type)) {\
		PyErr_Format(PyExc_TypeError, "Expected type %s", (type)->tp_name); \
		fail; \
	}

#define dom_sid0_Type dom_sid_Type
#define dom_sid2_Type dom_sid_Type
#define dom_sid28_Type dom_sid_Type
#define dom_sid0_Check dom_sid_Check
#define dom_sid2_Check dom_sid_Check
#define dom_sid28_Check dom_sid_Check

/* This macro is only provided by Python >= 2.3 */
#ifndef PyAPI_DATA
#   define PyAPI_DATA(RTYPE) extern RTYPE
#endif

typedef struct {
	PyObject_HEAD
	struct dcerpc_pipe *pipe;
} dcerpc_InterfaceObject;

PyAPI_DATA(PyTypeObject) dcerpc_InterfaceType;

#define PyErr_FromNdrError(err) Py_BuildValue("(is)", err, ndr_map_error2string(err))

#define PyErr_SetNdrError(err) \
		PyErr_SetObject(PyExc_RuntimeError, PyErr_FromNdrError(err))

void PyErr_SetDCERPCStatus(struct dcerpc_pipe *pipe, NTSTATUS status);

typedef bool (*py_data_pack_fn) (PyObject *args, PyObject *kwargs, void *r);
typedef PyObject *(*py_data_unpack_fn) (void *r);

struct PyNdrRpcMethodDef {
	const char *name;
	const char *doc;
	dcerpc_call_fn call;
	py_data_pack_fn pack_in_data;
	py_data_unpack_fn unpack_out_data;
	uint32_t opnum;
	const struct ndr_interface_table *table;
};

bool PyInterface_AddNdrRpcMethods(PyTypeObject *object, const struct PyNdrRpcMethodDef *mds);
PyObject *py_dcerpc_interface_init_helper(PyTypeObject *type, PyObject *args, PyObject *kwargs, const struct ndr_interface_table *table);

#endif /* _PYRPC_H_ */
