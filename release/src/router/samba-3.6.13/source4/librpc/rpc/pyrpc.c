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

#include <Python.h>
#include "includes.h"
#include <structmember.h>
#include "librpc/rpc/pyrpc.h"
#include "lib/events/events.h"
#include "param/pyparam.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/rpc/pyrpc_util.h"
#include "auth/credentials/pycredentials.h"

void initbase(void);

staticforward PyTypeObject dcerpc_InterfaceType;

static bool PyString_AsGUID(PyObject *object, struct GUID *uuid)
{
	NTSTATUS status;
	status = GUID_from_string(PyString_AsString(object), uuid);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetNTSTATUS(status);
		return false;
	}
	return true;
}

static bool ndr_syntax_from_py_object(PyObject *object, struct ndr_syntax_id *syntax_id)
{
	ZERO_STRUCTP(syntax_id);

	if (PyString_Check(object)) {
		return PyString_AsGUID(object, &syntax_id->uuid);
	} else if (PyTuple_Check(object)) {
		if (PyTuple_Size(object) < 1 || PyTuple_Size(object) > 2) {
			PyErr_SetString(PyExc_ValueError, "Syntax ID tuple has invalid size");
			return false;
		}

		if (!PyString_Check(PyTuple_GetItem(object, 0))) {
			PyErr_SetString(PyExc_ValueError, "Expected GUID as first element in tuple");
			return false;
		}

		if (!PyString_AsGUID(PyTuple_GetItem(object, 0), &syntax_id->uuid)) 
			return false;

		if (!PyInt_Check(PyTuple_GetItem(object, 1))) {
			PyErr_SetString(PyExc_ValueError, "Expected version as second element in tuple");
			return false;
		}

		syntax_id->if_version = PyInt_AsLong(PyTuple_GetItem(object, 1));
		return true;
	}

	PyErr_SetString(PyExc_TypeError, "Expected UUID or syntax id tuple");
	return false;
}

static PyObject *py_iface_server_name(PyObject *obj, void *closure)
{
	const char *server_name;
	dcerpc_InterfaceObject *iface = (dcerpc_InterfaceObject *)obj;

	server_name = dcerpc_server_name(iface->pipe);
	if (server_name == NULL)
		Py_RETURN_NONE;

	return PyString_FromString(server_name);
}

static PyObject *py_ndr_syntax_id(struct ndr_syntax_id *syntax_id)
{
	PyObject *ret;
	char *uuid_str;

	uuid_str = GUID_string(NULL, &syntax_id->uuid);
	if (uuid_str == NULL)
		return NULL;

	ret = Py_BuildValue("(s,i)", uuid_str, syntax_id->if_version);

	talloc_free(uuid_str);

	return ret;
}

static PyObject *py_iface_abstract_syntax(PyObject *obj, void *closure)
{
	dcerpc_InterfaceObject *iface = (dcerpc_InterfaceObject *)obj;

	return py_ndr_syntax_id(&iface->pipe->syntax);
}

static PyObject *py_iface_transfer_syntax(PyObject *obj, void *closure)
{
	dcerpc_InterfaceObject *iface = (dcerpc_InterfaceObject *)obj;

	return py_ndr_syntax_id(&iface->pipe->transfer_syntax);
}

static PyGetSetDef dcerpc_interface_getsetters[] = {
	{ discard_const_p(char, "server_name"), py_iface_server_name, NULL,
	  discard_const_p(char, "name of the server, if connected over SMB") },
	{ discard_const_p(char, "abstract_syntax"), py_iface_abstract_syntax, NULL, 
 	  discard_const_p(char, "syntax id of the abstract syntax") },
	{ discard_const_p(char, "transfer_syntax"), py_iface_transfer_syntax, NULL, 
 	  discard_const_p(char, "syntax id of the transfersyntax") },
	{ NULL }
};

static PyMemberDef dcerpc_interface_members[] = {
	{ discard_const_p(char, "request_timeout"), T_INT, 
	  offsetof(struct dcerpc_pipe, request_timeout), 0,
	  discard_const_p(char, "request timeout, in seconds") },
	{ NULL }
};

static PyObject *py_iface_request(PyObject *self, PyObject *args, PyObject *kwargs)
{
	dcerpc_InterfaceObject *iface = (dcerpc_InterfaceObject *)self;
	int opnum;
	DATA_BLOB data_in, data_out;
	NTSTATUS status;
	char *in_data;
	int in_length;
	PyObject *ret;
	PyObject *object = NULL;
	struct GUID object_guid;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	uint32_t out_flags = 0;
	const char *kwnames[] = { "opnum", "data", "object", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "is#|O:request", 
		discard_const_p(char *, kwnames), &opnum, &in_data, &in_length, &object)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	data_in.data = (uint8_t *)talloc_memdup(mem_ctx, in_data, in_length);
	data_in.length = in_length;

	ZERO_STRUCT(data_out);

	if (object != NULL && !PyString_AsGUID(object, &object_guid)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	status = dcerpc_binding_handle_raw_call(iface->binding_handle,
						object?&object_guid:NULL,
						opnum,
						0, /* in_flags */
						data_in.data,
						data_in.length,
						mem_ctx,
						&data_out.data,
						&data_out.length,
						&out_flags);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetDCERPCStatus(iface->pipe, status);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = PyString_FromStringAndSize((char *)data_out.data, data_out.length);

	talloc_free(mem_ctx);
	return ret;
}

static PyObject *py_iface_alter_context(PyObject *self, PyObject *args, PyObject *kwargs)
{
	dcerpc_InterfaceObject *iface = (dcerpc_InterfaceObject *)self;
	NTSTATUS status;
	const char *kwnames[] = { "abstract_syntax", "transfer_syntax", NULL };
	PyObject *py_abstract_syntax = Py_None, *py_transfer_syntax = Py_None;
	struct ndr_syntax_id abstract_syntax, transfer_syntax;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O:alter_context", 
		discard_const_p(char *, kwnames), &py_abstract_syntax,
		&py_transfer_syntax)) {
		return NULL;
	}

	if (!ndr_syntax_from_py_object(py_abstract_syntax, &abstract_syntax))
		return NULL;

	if (py_transfer_syntax == Py_None) {
		transfer_syntax = ndr_transfer_syntax;
	} else {
		if (!ndr_syntax_from_py_object(py_transfer_syntax, 
					       &transfer_syntax))
			return NULL;
	}

	status = dcerpc_alter_context(iface->pipe, iface->pipe, &abstract_syntax, 
				      &transfer_syntax);

	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetDCERPCStatus(iface->pipe, status);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef dcerpc_interface_methods[] = {
	{ "request", (PyCFunction)py_iface_request, METH_VARARGS|METH_KEYWORDS, "S.request(opnum, data, object=None) -> data\nMake a raw request" },
	{ "alter_context", (PyCFunction)py_iface_alter_context, METH_VARARGS|METH_KEYWORDS, "S.alter_context(syntax)\nChange to a different interface" },
	{ NULL, NULL, 0, NULL },
};

static void dcerpc_interface_dealloc(PyObject* self)
{
	dcerpc_InterfaceObject *interface = (dcerpc_InterfaceObject *)self;
	talloc_free(interface->mem_ctx);
	self->ob_type->tp_free(self);
}

static PyObject *dcerpc_interface_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	dcerpc_InterfaceObject *ret;
	const char *binding_string;
	struct cli_credentials *credentials;
	struct loadparm_context *lp_ctx = NULL;
	PyObject *py_lp_ctx = Py_None, *py_credentials = Py_None;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *event_ctx;
	NTSTATUS status;

	PyObject *syntax, *py_basis = Py_None;
	const char *kwnames[] = {
		"binding", "syntax", "lp_ctx", "credentials", "basis_connection", NULL
	};
	struct ndr_interface_table *table;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|OOO:connect", discard_const_p(char *, kwnames), &binding_string, &syntax, &py_lp_ctx, &py_credentials, &py_basis)) {
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	lp_ctx = lpcfg_from_py_object(mem_ctx, py_lp_ctx);
	if (lp_ctx == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected loadparm context");
		talloc_free(mem_ctx);
		return NULL;
	}

	credentials = cli_credentials_from_py_object(py_credentials);
	if (credentials == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected credentials");
		talloc_free(mem_ctx);
		return NULL;
	}
	ret = PyObject_New(dcerpc_InterfaceObject, type);
	ret->mem_ctx = mem_ctx;

	event_ctx = s4_event_context_init(ret->mem_ctx);

	/* Create a dummy interface table struct. TODO: In the future, we should
	 * rather just allow connecting without requiring an interface table.
	 */

	table = talloc_zero(ret->mem_ctx, struct ndr_interface_table);

	if (table == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Allocating interface table");
		talloc_free(mem_ctx);
		return NULL;
	}

	if (!ndr_syntax_from_py_object(syntax, &table->syntax_id)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	ret->pipe = NULL;
	ret->binding_handle = NULL;

	if (py_basis != Py_None) {
		struct dcerpc_pipe *base_pipe;

		if (!PyObject_TypeCheck(py_basis, &dcerpc_InterfaceType)) {
			PyErr_SetString(PyExc_ValueError, "basis_connection must be a DCE/RPC connection");
			talloc_free(mem_ctx);
			return NULL;
		}

		base_pipe = talloc_reference(ret->mem_ctx, 
					 ((dcerpc_InterfaceObject *)py_basis)->pipe);

		status = dcerpc_secondary_context(base_pipe, &ret->pipe, table);

		ret->pipe = talloc_steal(ret->mem_ctx, ret->pipe);
	} else {
		status = dcerpc_pipe_connect(ret->mem_ctx, &ret->pipe, binding_string, 
			     table, credentials, event_ctx, lp_ctx);
	}

	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetDCERPCStatus(ret->pipe, status);
		talloc_free(ret->mem_ctx);
		return NULL;
	}
	ret->pipe->conn->flags |= DCERPC_NDR_REF_ALLOC;
	ret->binding_handle = ret->pipe->binding_handle;
	return (PyObject *)ret;
}

static PyTypeObject dcerpc_InterfaceType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "dcerpc.ClientConnection",
	.tp_basicsize = sizeof(dcerpc_InterfaceObject),
	.tp_dealloc = dcerpc_interface_dealloc,
	.tp_getset = dcerpc_interface_getsetters,
	.tp_members = dcerpc_interface_members,
	.tp_methods = dcerpc_interface_methods,
	.tp_doc = "ClientConnection(binding, syntax, lp_ctx=None, credentials=None) -> connection\n"
"\n"
"binding should be a DCE/RPC binding string (for example: ncacn_ip_tcp:127.0.0.1)\n"
"syntax should be a tuple with a GUID and version number of an interface\n"
"lp_ctx should be a path to a smb.conf file or a param.LoadParm object\n"
"credentials should be a credentials.Credentials object.\n\n",
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = dcerpc_interface_new,
};

void initbase(void)
{
	PyObject *m;

	if (PyType_Ready(&dcerpc_InterfaceType) < 0)
		return;

	m = Py_InitModule3("base", NULL, "DCE/RPC protocol implementation");
	if (m == NULL)
		return;

	Py_INCREF((PyObject *)&dcerpc_InterfaceType);
	PyModule_AddObject(m, "ClientConnection", (PyObject *)&dcerpc_InterfaceType);
}
