/* 
   Unix SMB/CIFS implementation.
   Copyright © Jelmer Vernooij <jelmer@samba.org> 2008

   Based on the equivalent for EJS:
   Copyright © Andrew Tridgell <tridge@samba.org> 2005
   
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

#include "includes.h"
#include <Python.h>
#include "scripting/python/modules.h"
#include "libcli/util/pyerrors.h"
#include "librpc/rpc/pyrpc.h"
#include "lib/messaging/irpc.h"
#include "lib/messaging/messaging.h"
#include "lib/events/events.h"
#include "cluster/cluster.h"
#include "param/param.h"
#include "param/pyparam.h"

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

PyAPI_DATA(PyTypeObject) messaging_Type;
PyAPI_DATA(PyTypeObject) irpc_ClientConnectionType;

/* FIXME: This prototype should be in py_irpc.h, or shared otherwise */
extern const struct PyNdrRpcMethodDef py_ndr_irpc_methods[];

static bool server_id_from_py(PyObject *object, struct server_id *server_id)
{
	if (!PyTuple_Check(object)) {
		PyErr_SetString(PyExc_ValueError, "Expected tuple");
		return false;
	}

	if (PyTuple_Size(object) == 3) {
		return PyArg_ParseTuple(object, "iii", &server_id->id, &server_id->id2, &server_id->node);
	} else {
		int id, id2;
		if (!PyArg_ParseTuple(object, "ii", &id, &id2))
			return false;
		*server_id = cluster_id(id, id2);
		return true;
	}
}

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct messaging_context *msg_ctx;
} messaging_Object;

PyObject *py_messaging_connect(PyTypeObject *self, PyObject *args, PyObject *kwargs)
{
	struct tevent_context *ev;
	const char *kwnames[] = { "own_id", "messaging_path", NULL };
	PyObject *own_id = Py_None;
	const char *messaging_path = NULL;
	messaging_Object *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Oz:connect", 
		discard_const_p(char *, kwnames), &own_id, &messaging_path)) {
		return NULL;
	}

	ret = PyObject_New(messaging_Object, &messaging_Type);
	if (ret == NULL)
		return NULL;

	ret->mem_ctx = talloc_new(NULL);

	ev = s4_event_context_init(ret->mem_ctx);

	if (messaging_path == NULL) {
		messaging_path = lp_messaging_path(ret->mem_ctx, 
								   py_default_loadparm_context(ret->mem_ctx));
	} else {
		messaging_path = talloc_strdup(ret->mem_ctx, messaging_path);
	}

	if (own_id != Py_None) {
		struct server_id server_id;

		if (!server_id_from_py(own_id, &server_id)) 
			return NULL;

		ret->msg_ctx = messaging_init(ret->mem_ctx, 
					    messaging_path,
					    server_id,
				        py_iconv_convenience(ret->mem_ctx),
					    ev);
	} else {
		ret->msg_ctx = messaging_client_init(ret->mem_ctx, 
					    messaging_path,
				        py_iconv_convenience(ret->mem_ctx),
					    ev);
	}

	if (ret->msg_ctx == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "messaging_connect unable to create a messaging context");
		talloc_free(ret->mem_ctx);
		return NULL;
	}

	return (PyObject *)ret;
}

static void py_messaging_dealloc(PyObject *self)
{
	messaging_Object *iface = (messaging_Object *)self;
	talloc_free(iface->msg_ctx);
	PyObject_Del(self);
}

static PyObject *py_messaging_send(PyObject *self, PyObject *args, PyObject *kwargs)
{
	messaging_Object *iface = (messaging_Object *)self;
	uint32_t msg_type;
	DATA_BLOB data;
	PyObject *target;
	NTSTATUS status;
	struct server_id server;
	const char *kwnames[] = { "target", "msg_type", "data", NULL };
	int length;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Ois#|:send", 
		discard_const_p(char *, kwnames), &target, &msg_type, &data.data, &length)) {
		return NULL;
	}

	data.length = length;

	if (!server_id_from_py(target, &server)) 
		return NULL;

	status = messaging_send(iface->msg_ctx, server, msg_type, &data);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	Py_RETURN_NONE;
}

static void py_msg_callback_wrapper(struct messaging_context *msg, void *private_data,
			       uint32_t msg_type, 
			       struct server_id server_id, DATA_BLOB *data)
{
	PyObject *callback = (PyObject *)private_data;

	PyObject_CallFunction(callback, discard_const_p(char, "i(iii)s#"), msg_type, 
			      server_id.id, server_id.id2, server_id.node, 
			      data->data, data->length);
}

static PyObject *py_messaging_register(PyObject *self, PyObject *args, PyObject *kwargs)
{
	messaging_Object *iface = (messaging_Object *)self;
	int msg_type = -1;
	PyObject *callback;
	NTSTATUS status;
	const char *kwnames[] = { "callback", "msg_type", NULL };
	
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i:send", 
		discard_const_p(char *, kwnames), &callback, &msg_type)) {
		return NULL;
	}

	Py_INCREF(callback);

	if (msg_type == -1) {
		uint32_t msg_type32 = msg_type;
		status = messaging_register_tmp(iface->msg_ctx, callback,
						py_msg_callback_wrapper, &msg_type32);
		msg_type = msg_type32;
	} else {
		status = messaging_register(iface->msg_ctx, callback,
				    msg_type, py_msg_callback_wrapper);
	}
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	return PyLong_FromLong(msg_type);
}

static PyObject *py_messaging_deregister(PyObject *self, PyObject *args, PyObject *kwargs)
{
	messaging_Object *iface = (messaging_Object *)self;
	int msg_type = -1;
	PyObject *callback;
	const char *kwnames[] = { "callback", "msg_type", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i:send", 
		discard_const_p(char *, kwnames), &callback, &msg_type)) {
		return NULL;
	}

	messaging_deregister(iface->msg_ctx, msg_type, callback);

	Py_DECREF(callback);

	Py_RETURN_NONE;
}

static PyObject *py_messaging_add_name(PyObject *self, PyObject *args, PyObject *kwargs)
{
	messaging_Object *iface = (messaging_Object *)self;
	NTSTATUS status;
	char *name;
	const char *kwnames[] = { "name", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|:send", 
		discard_const_p(char *, kwnames), &name)) {
		return NULL;
	}

	status = irpc_add_name(iface->msg_ctx, name);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	Py_RETURN_NONE;
}


static PyObject *py_messaging_remove_name(PyObject *self, PyObject *args, PyObject *kwargs)
{
	messaging_Object *iface = (messaging_Object *)self;
	char *name;
	const char *kwnames[] = { "name", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|:send", 
		discard_const_p(char *, kwnames), &name)) {
		return NULL;
	}

	irpc_remove_name(iface->msg_ctx, name);

	Py_RETURN_NONE;
}

static PyMethodDef py_messaging_methods[] = {
	{ "send", (PyCFunction)py_messaging_send, METH_VARARGS|METH_KEYWORDS, 
		"S.send(target, msg_type, data) -> None\nSend a message" },
	{ "register", (PyCFunction)py_messaging_register, METH_VARARGS|METH_KEYWORDS,
		"S.register(callback, msg_type=None) -> msg_type\nRegister a message handler" },
	{ "deregister", (PyCFunction)py_messaging_deregister, METH_VARARGS|METH_KEYWORDS,
		"S.deregister(callback, msg_type) -> None\nDeregister a message handler" },
	{ "add_name", (PyCFunction)py_messaging_add_name, METH_VARARGS|METH_KEYWORDS, "S.add_name(name) -> None\nListen on another name" },
	{ "remove_name", (PyCFunction)py_messaging_remove_name, METH_VARARGS|METH_KEYWORDS, "S.remove_name(name) -> None\nStop listening on a name" },
	{ NULL, NULL, 0, NULL }
};

static PyObject *py_messaging_server_id(PyObject *obj, void *closure)
{
	messaging_Object *iface = (messaging_Object *)obj;
	struct server_id server_id = messaging_get_server_id(iface->msg_ctx);

	return Py_BuildValue("(iii)", server_id.id, server_id.id2, 
			     server_id.node);
}

static PyGetSetDef py_messaging_getset[] = {
	{ discard_const_p(char, "server_id"), py_messaging_server_id, NULL, 
	  discard_const_p(char, "local server id") },
	{ NULL },
};


PyTypeObject messaging_Type = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "irpc.Messaging",
	.tp_basicsize = sizeof(messaging_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	.tp_new = py_messaging_connect,
	.tp_dealloc = py_messaging_dealloc,
	.tp_methods = py_messaging_methods,
	.tp_getset = py_messaging_getset,
	.tp_doc = "Messaging(own_id=None, messaging_path=None)\n" \
		  "Create a new object that can be used to communicate with the peers in the specified messaging path.\n" \
		  "If no path is specified, the default path from smb.conf will be used."
};


/*
  state of a irpc 'connection'
*/
typedef struct {
	PyObject_HEAD
	const char *server_name;
	struct server_id *dest_ids;
	struct messaging_context *msg_ctx;
	TALLOC_CTX *mem_ctx;
} irpc_ClientConnectionObject;

/*
  setup a context for talking to a irpc server
     example: 
        status = irpc.connect("smb_server");
*/

PyObject *py_irpc_connect(PyTypeObject *self, PyObject *args, PyObject *kwargs)
{
	struct tevent_context *ev;
	const char *kwnames[] = { "server", "own_id", "messaging_path", NULL };
	char *server;
	const char *messaging_path = NULL;
	PyObject *own_id = Py_None;
	irpc_ClientConnectionObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Oz:connect", 
		discard_const_p(char *, kwnames), &server, &own_id, &messaging_path)) {
		return NULL;
	}

	ret = PyObject_New(irpc_ClientConnectionObject, &irpc_ClientConnectionType);
	if (ret == NULL)
		return NULL;

	ret->mem_ctx = talloc_new(NULL);

	ret->server_name = server;

	ev = s4_event_context_init(ret->mem_ctx);

	if (messaging_path == NULL) {
		messaging_path = lp_messaging_path(ret->mem_ctx, 
								   py_default_loadparm_context(ret->mem_ctx));
	} else {
		messaging_path = talloc_strdup(ret->mem_ctx, messaging_path);
	}

	if (own_id != Py_None) {
		struct server_id server_id;

		if (!server_id_from_py(own_id, &server_id)) 
			return NULL;

		ret->msg_ctx = messaging_init(ret->mem_ctx, 
					    messaging_path,
					    server_id,
				        py_iconv_convenience(ret->mem_ctx),
					    ev);
	} else {
		ret->msg_ctx = messaging_client_init(ret->mem_ctx, 
					    messaging_path,
				        py_iconv_convenience(ret->mem_ctx),
					    ev);
	}

	if (ret->msg_ctx == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "irpc_connect unable to create a messaging context");
		talloc_free(ret->mem_ctx);
		return NULL;
	}

	ret->dest_ids = irpc_servers_byname(ret->msg_ctx, ret->mem_ctx, ret->server_name);
	if (ret->dest_ids == NULL || ret->dest_ids[0].id == 0) {
		talloc_free(ret->mem_ctx);
		PyErr_SetNTSTATUS(NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return NULL;
	} else {
		return (PyObject *)ret;
	}
}

typedef struct {
	PyObject_HEAD
	struct irpc_request **reqs;
	int count;
	int current;
	TALLOC_CTX *mem_ctx;
	py_data_unpack_fn unpack_fn;
} irpc_ResultObject;

	
static PyObject *irpc_result_next(irpc_ResultObject *iterator)
{
	NTSTATUS status;

	if (iterator->current >= iterator->count) {
		PyErr_SetString(PyExc_StopIteration, "No more results");
		return NULL;
	}

	status = irpc_call_recv(iterator->reqs[iterator->current]);
	iterator->current++;
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	return iterator->unpack_fn(iterator->reqs[iterator->current-1]->r);
}

static PyObject *irpc_result_len(irpc_ResultObject *self)
{
	return PyLong_FromLong(self->count);
}

static PyMethodDef irpc_result_methods[] = {
	{ "__len__", (PyCFunction)irpc_result_len, METH_NOARGS, 
		"Number of elements returned"},
	{ NULL }
};

static void irpc_result_dealloc(PyObject *self)
{
	talloc_free(((irpc_ResultObject *)self)->mem_ctx);
	PyObject_Del(self);
}

PyTypeObject irpc_ResultIteratorType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "irpc.ResultIterator",
	.tp_basicsize = sizeof(irpc_ResultObject),
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	.tp_iternext = (iternextfunc)irpc_result_next,
	.tp_iter = PyObject_SelfIter,
	.tp_methods = irpc_result_methods,
	.tp_dealloc = irpc_result_dealloc,
};

static PyObject *py_irpc_call(irpc_ClientConnectionObject *p, struct PyNdrRpcMethodDef *method_def, PyObject *args, PyObject *kwargs)
{
	void *ptr;
	struct irpc_request **reqs;
	int i, count;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	irpc_ResultObject *ret;

	/* allocate the C structure */
	ptr = talloc_zero_size(mem_ctx, method_def->table->calls[method_def->opnum].struct_size);
	if (ptr == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* convert the mpr object into a C structure */
	if (!method_def->pack_in_data(args, kwargs, ptr)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	for (count=0;p->dest_ids[count].id;count++) /* noop */ ;

	/* we need to make a call per server */
	reqs = talloc_array(mem_ctx, struct irpc_request *, count);
	if (reqs == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* make the actual calls */
	for (i=0;i<count;i++) {
		reqs[i] = irpc_call_send(p->msg_ctx, p->dest_ids[i], 
					 method_def->table, method_def->opnum, ptr, ptr);
		if (reqs[i] == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
		talloc_steal(reqs, reqs[i]);
	}

	ret = PyObject_New(irpc_ResultObject, &irpc_ResultIteratorType);
	ret->mem_ctx = mem_ctx;
	ret->reqs = reqs;
	ret->count = count;
	ret->current = 0;
	ret->unpack_fn = method_def->unpack_out_data;

	return (PyObject *)ret;
done:
	talloc_free(mem_ctx);
	PyErr_SetNTSTATUS(status);
	return NULL;
}

static PyObject *py_irpc_call_wrapper(PyObject *self, PyObject *args, void *wrapped, PyObject *kwargs)
{	
	irpc_ClientConnectionObject *iface = (irpc_ClientConnectionObject *)self;
	struct PyNdrRpcMethodDef *md = wrapped;

	return py_irpc_call(iface, md, args, kwargs);
}

static void py_irpc_dealloc(PyObject *self)
{
	irpc_ClientConnectionObject *iface = (irpc_ClientConnectionObject *)self;
	talloc_free(iface->mem_ctx);
	PyObject_Del(self);
}

PyTypeObject irpc_ClientConnectionType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "irpc.ClientConnection",
	.tp_basicsize = sizeof(irpc_ClientConnectionObject),
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	.tp_new = py_irpc_connect,
	.tp_dealloc = py_irpc_dealloc,
	.tp_doc = "ClientConnection(server, own_id=None, messaging_path=None)\n" \
		  "Create a new IRPC client connection to communicate with the servers in the specified path.\n" \
		  "If no path is specified, the default path from smb.conf will be used."
};

static bool irpc_AddNdrRpcMethods(PyTypeObject *ifacetype, const struct PyNdrRpcMethodDef *mds)
{
	int i;
	for (i = 0; mds[i].name; i++) {
		PyObject *ret;
		struct wrapperbase *wb = calloc(sizeof(struct wrapperbase), 1);

		wb->name = discard_const_p(char, mds[i].name);
		wb->flags = PyWrapperFlag_KEYWORDS;
		wb->wrapper = (wrapperfunc)py_irpc_call_wrapper;
		wb->doc = discard_const_p(char, mds[i].doc);
		
		ret = PyDescr_NewWrapper(ifacetype, wb, discard_const_p(void, &mds[i]));

		PyDict_SetItemString(ifacetype->tp_dict, mds[i].name, 
				     (PyObject *)ret);
	}

	return true;
}

void initmessaging(void)
{
	PyObject *mod;
	PyObject *dep_irpc;

	dep_irpc = PyImport_ImportModule("samba.dcerpc.irpc");
	if (dep_irpc == NULL)
		return;

	if (PyType_Ready(&irpc_ClientConnectionType) < 0)
		return;

	if (PyType_Ready(&messaging_Type) < 0)
		return;

	if (PyType_Ready(&irpc_ResultIteratorType) < 0) 
		return;

	if (!irpc_AddNdrRpcMethods(&irpc_ClientConnectionType, py_ndr_irpc_methods))
		return;

	mod = Py_InitModule3("messaging", NULL, "Internal RPC");
	if (mod == NULL)
		return;

	Py_INCREF((PyObject *)&irpc_ClientConnectionType);
	PyModule_AddObject(mod, "ClientConnection", (PyObject *)&irpc_ClientConnectionType);

	Py_INCREF((PyObject *)&messaging_Type);
	PyModule_AddObject(mod, "Messaging", (PyObject *)&messaging_Type);
}
