/*
   Unix SMB/CIFS implementation.
   Python bindings for tevent

   Copyright (C) Jelmer Vernooij 2010

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>
#include <tevent.h>

typedef struct {
	PyObject_HEAD
	struct tevent_context *ev;
} TeventContext_Object;

typedef struct {
	PyObject_HEAD
	struct tevent_queue *queue;
} TeventQueue_Object;

typedef struct {
	PyObject_HEAD
	struct tevent_req *req;
} TeventReq_Object;

typedef struct {
	PyObject_HEAD
	struct tevent_signal *signal;
} TeventSignal_Object;

typedef struct {
	PyObject_HEAD
	struct tevent_timer *timer;
} TeventTimer_Object;

typedef struct {
	PyObject_HEAD
	struct tevent_fd *fd;
} TeventFd_Object;

staticforward PyTypeObject TeventContext_Type;
staticforward PyTypeObject TeventReq_Type;
staticforward PyTypeObject TeventQueue_Type;
staticforward PyTypeObject TeventSignal_Type;
staticforward PyTypeObject TeventTimer_Type;
staticforward PyTypeObject TeventFd_Type;

static int py_context_init(struct tevent_context *ev)
{
	/* FIXME */
	return 0;
}

static struct tevent_fd *py_add_fd(struct tevent_context *ev,
				    TALLOC_CTX *mem_ctx,
				    int fd, uint16_t flags,
				    tevent_fd_handler_t handler,
				    void *private_data,
				    const char *handler_name,
				    const char *location)
{
	/* FIXME */
	return NULL;
}

static void py_set_fd_close_fn(struct tevent_fd *fde,
				tevent_fd_close_fn_t close_fn)
{
	/* FIXME */
}

uint16_t py_get_fd_flags(struct tevent_fd *fde)
{
	/* FIXME */
	return 0;
}

static void py_set_fd_flags(struct tevent_fd *fde, uint16_t flags)
{
	/* FIXME */
}

/* timed_event functions */
static struct tevent_timer *py_add_timer(struct tevent_context *ev,
					  TALLOC_CTX *mem_ctx,
					  struct timeval next_event,
					  tevent_timer_handler_t handler,
					  void *private_data,
					  const char *handler_name,
					  const char *location)
{
	/* FIXME */
	return NULL;
}

/* immediate event functions */
static void py_schedule_immediate(struct tevent_immediate *im,
				   struct tevent_context *ev,
				   tevent_immediate_handler_t handler,
				   void *private_data,
				   const char *handler_name,
				   const char *location)
{
	/* FIXME */
}

/* signal functions */
static struct tevent_signal *py_add_signal(struct tevent_context *ev,
					    TALLOC_CTX *mem_ctx,
					    int signum, int sa_flags,
					    tevent_signal_handler_t handler,
					    void *private_data,
					    const char *handler_name,
					    const char *location)
{
	/* FIXME */
	return NULL;
}

/* loop functions */
static int py_loop_once(struct tevent_context *ev, const char *location)
{
	/* FIXME */
	return 0;
}

static int py_loop_wait(struct tevent_context *ev, const char *location)
{
	/* FIXME */
	return 0;
}

const static struct tevent_ops py_tevent_ops = {
	.context_init = py_context_init,
	.add_fd = py_add_fd,
	.set_fd_close_fn = py_set_fd_close_fn,
	.get_fd_flags = py_get_fd_flags,
	.set_fd_flags = py_set_fd_flags,
	.add_timer = py_add_timer,
	.schedule_immediate = py_schedule_immediate,
	.add_signal = py_add_signal,
	.loop_wait = py_loop_wait,
	.loop_once = py_loop_once,
};

static PyObject *py_register_backend(PyObject *self, PyObject *args)
{
	PyObject *name, *py_backend;

	if (!PyArg_ParseTuple(args, "O", &py_backend))
		return NULL;

	name = PyObject_GetAttrString(py_backend, "name");
	if (name == NULL) {
		PyErr_SetNone(PyExc_AttributeError);
		return NULL;
	}

	if (!PyString_Check(name)) {
		PyErr_SetNone(PyExc_TypeError);
		return NULL;
	}

	if (!tevent_register_backend(PyString_AsString(name), &py_tevent_ops)) { /* FIXME: What to do with backend */
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *py_tevent_context_reinitialise(TeventContext_Object *self)
{
	int ret = tevent_re_initialise(self->ev);
	if (ret != 0) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *py_tevent_queue_stop(TeventQueue_Object *self)
{
	tevent_queue_stop(self->queue);
	Py_RETURN_NONE;
}

static PyObject *py_tevent_queue_start(TeventQueue_Object *self)
{
	tevent_queue_start(self->queue);
	Py_RETURN_NONE;
}

static void py_queue_trigger(struct tevent_req *req, void *private_data)
{
	PyObject *callback = private_data, *ret;

	ret = PyObject_CallFunction(callback, "");
	Py_XDECREF(ret);
}

static PyObject *py_tevent_queue_add(TeventQueue_Object *self, PyObject *args)
{
	TeventContext_Object *py_ev;
	TeventReq_Object *py_req;
	PyObject *trigger;
	bool ret;

	if (!PyArg_ParseTuple(args, "O!O!O", 
						  &TeventContext_Type, &py_ev,
						  &TeventReq_Type, &py_req,
						  &trigger))
		return NULL;

	Py_INCREF(trigger);

	ret = tevent_queue_add(self->queue, py_ev->ev, py_req->req,
						   py_queue_trigger, trigger);
	if (!ret) {
		PyErr_SetString(PyExc_RuntimeError, "queue add failed");
		Py_DECREF(trigger);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef py_tevent_queue_methods[] = {
	{ "stop", (PyCFunction)py_tevent_queue_stop, METH_NOARGS,
		"S.stop()" },
	{ "start", (PyCFunction)py_tevent_queue_start, METH_NOARGS,
		"S.start()" },
	{ "add", (PyCFunction)py_tevent_queue_add, METH_VARARGS,
		"S.add(ctx, req, trigger, baton)" },
	{ NULL },
};

static PyObject *py_tevent_context_wakeup_send(PyObject *self, PyObject *args)
{
	/* FIXME */

	Py_RETURN_NONE;
}

static PyObject *py_tevent_context_loop_wait(TeventContext_Object *self)
{
	if (tevent_loop_wait(self->ev) != 0) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *py_tevent_context_loop_once(TeventContext_Object *self)
{
	if (tevent_loop_once(self->ev) != 0) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}
	Py_RETURN_NONE;
}

#ifdef TEVENT_DEPRECATED
static bool py_tevent_finished(PyObject *callback)
{
	PyObject *py_ret;
	bool ret;

	py_ret = PyObject_CallFunction(callback, "");
	if (py_ret == NULL)
		return true;
	ret = PyObject_IsTrue(py_ret);
	Py_DECREF(py_ret);
	return ret;
}

static PyObject *py_tevent_context_loop_until(TeventContext_Object *self, PyObject *args)
{
	PyObject *callback;
	if (!PyArg_ParseTuple(args, "O", &callback))
		return NULL;

	if (tevent_loop_until(self->ev, py_tevent_finished, callback) != 0) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}

	if (PyErr_Occurred())
		return NULL;

	Py_RETURN_NONE;
}
#endif

static void py_tevent_signal_handler(struct tevent_context *ev,
					struct tevent_signal *se,
					int signum,
					int count,
					void *siginfo,
					void *private_data)
{
	PyObject *callback = (PyObject *)private_data, *ret;

	ret = PyObject_CallFunction(callback, "ii", signum, count);
	Py_XDECREF(ret);
}

static void py_tevent_signal_dealloc(TeventSignal_Object *self)
{
	talloc_free(self->signal);
	PyObject_Del(self);
}

static PyTypeObject TeventSignal_Type = {
	.tp_name = "Signal",
	.tp_basicsize = sizeof(TeventSignal_Object),
	.tp_dealloc = (destructor)py_tevent_signal_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_tevent_context_add_signal(TeventContext_Object *self, PyObject *args)
{
	int signum, sa_flags;
	PyObject *handler;
	struct tevent_signal *sig;
	TeventSignal_Object *ret;

	if (!PyArg_ParseTuple(args, "iiO", &signum, &sa_flags, &handler))
		return NULL;

	Py_INCREF(handler);
	sig = tevent_add_signal(self->ev, NULL, signum, sa_flags,
							py_tevent_signal_handler, handler);

	ret = PyObject_New(TeventSignal_Object, &TeventSignal_Type);
	if (ret == NULL) {
		PyErr_NoMemory();
		talloc_free(sig);
		return NULL;
	}

	ret->signal = sig;

	return (PyObject *)ret;
}

static void py_timer_handler(struct tevent_context *ev,
				       struct tevent_timer *te,
				       struct timeval current_time,
				       void *private_data)
{
	PyObject *callback = private_data, *ret;
	ret = PyObject_CallFunction(callback, "l", te);
	Py_XDECREF(ret);
}

static PyObject *py_tevent_context_add_timer(TeventContext_Object *self, PyObject *args)
{
	TeventTimer_Object *ret;
	struct timeval next_event;
	struct tevent_timer *timer;
	PyObject *handler;
	if (!PyArg_ParseTuple(args, "lO", &next_event, &handler))
		return NULL;

	timer = tevent_add_timer(self->ev, NULL, next_event, py_timer_handler,
							 handler);
	if (timer == NULL) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}

	ret = PyObject_New(TeventTimer_Object, &TeventTimer_Type);
	if (ret == NULL) {
		PyErr_NoMemory();
		talloc_free(timer);
		return NULL;
	}
	ret->timer = timer;

	return (PyObject *)ret;
}

static void py_fd_handler(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    uint16_t flags,
				    void *private_data)
{
	PyObject *callback = private_data, *ret;

	ret = PyObject_CallFunction(callback, "i", flags);
	Py_XDECREF(ret);
}

static PyObject *py_tevent_context_add_fd(TeventContext_Object *self, PyObject *args)
{
	int fd, flags;
	PyObject *handler;
	struct tevent_fd *tfd;
	TeventFd_Object *ret;

	if (!PyArg_ParseTuple(args, "iiO", &fd, &flags, &handler))
		return NULL;

	tfd = tevent_add_fd(self->ev, NULL, fd, flags, py_fd_handler, handler);
	if (tfd == NULL) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}

	ret = PyObject_New(TeventFd_Object, &TeventFd_Type);
	if (ret == NULL) {
		talloc_free(tfd);
		return NULL;
	}
	ret->fd = tfd;

	return (PyObject *)ret;
}

#ifdef TEVENT_DEPRECATED
static PyObject *py_tevent_context_set_allow_nesting(TeventContext_Object *self)
{
	tevent_loop_allow_nesting(self->ev);
	Py_RETURN_NONE;
}
#endif

static PyMethodDef py_tevent_context_methods[] = {
	{ "reinitialise", (PyCFunction)py_tevent_context_reinitialise, METH_NOARGS,
		"S.reinitialise()" },
	{ "wakeup_send", (PyCFunction)py_tevent_context_wakeup_send, 
		METH_VARARGS, "S.wakeup_send(wakeup_time) -> req" },
	{ "loop_wait", (PyCFunction)py_tevent_context_loop_wait,
		METH_NOARGS, "S.loop_wait()" },
	{ "loop_once", (PyCFunction)py_tevent_context_loop_once,
		METH_NOARGS, "S.loop_once()" },
#ifdef TEVENT_DEPRECATED
	{ "loop_until", (PyCFunction)py_tevent_context_loop_until,
		METH_VARARGS, "S.loop_until(callback)" },
#endif
	{ "add_signal", (PyCFunction)py_tevent_context_add_signal,
		METH_VARARGS, "S.add_signal(signum, sa_flags, handler) -> signal" },
	{ "add_timer", (PyCFunction)py_tevent_context_add_timer,
		METH_VARARGS, "S.add_timer(next_event, handler) -> timer" },
	{ "add_fd", (PyCFunction)py_tevent_context_add_fd, 
		METH_VARARGS, "S.add_fd(fd, flags, handler) -> fd" },
#ifdef TEVENT_DEPRECATED
	{ "allow_nesting", (PyCFunction)py_tevent_context_set_allow_nesting, 
		METH_NOARGS, "Whether to allow nested tevent loops." },
#endif
	{ NULL },
};

static PyObject *py_tevent_req_wakeup_recv(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_received(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_is_error(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_poll(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_is_in_progress(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyGetSetDef py_tevent_req_getsetters[] = {
	{ "in_progress", (getter)py_tevent_req_is_in_progress, NULL,
		"Whether the request is in progress" },
	{ NULL }
};

static PyObject *py_tevent_req_post(PyObject *self, PyObject *args)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_set_error(PyObject *self, PyObject *args)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_done(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_notify_callback(PyObject *self)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_set_endtime(PyObject *self, PyObject *args)
{
	/* FIXME */
	Py_RETURN_NONE;
}

static PyObject *py_tevent_req_cancel(TeventReq_Object *self)
{
	if (!tevent_req_cancel(self->req)) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyMethodDef py_tevent_req_methods[] = {
	{ "wakeup_recv", (PyCFunction)py_tevent_req_wakeup_recv, METH_NOARGS,
		"Wakeup received" },
	{ "received", (PyCFunction)py_tevent_req_received, METH_NOARGS,
		"Receive finished" },
	{ "is_error", (PyCFunction)py_tevent_req_is_error, METH_NOARGS,
		"is_error() -> (error, state)" },
	{ "poll", (PyCFunction)py_tevent_req_poll, METH_VARARGS,
		"poll(ctx)" },
	{ "post", (PyCFunction)py_tevent_req_post, METH_VARARGS,
		"post(ctx) -> req" },
	{ "set_error", (PyCFunction)py_tevent_req_set_error, METH_VARARGS,
		"set_error(error)" },
	{ "done", (PyCFunction)py_tevent_req_done, METH_NOARGS,
		"done()" },
	{ "notify_callback", (PyCFunction)py_tevent_req_notify_callback,
		METH_NOARGS, "notify_callback()" },
	{ "set_endtime", (PyCFunction)py_tevent_req_set_endtime,
		METH_VARARGS, "set_endtime(ctx, endtime)" },
	{ "cancel", (PyCFunction)py_tevent_req_cancel,
		METH_NOARGS, "cancel()" },
	{ NULL }
};

static void py_tevent_req_dealloc(TeventReq_Object *self)
{
	talloc_free(self->req);
	PyObject_DEL(self);
}

static PyTypeObject TeventReq_Type = {
	.tp_name = "tevent.Request",
	.tp_basicsize = sizeof(TeventReq_Object),
	.tp_methods = py_tevent_req_methods,
	.tp_dealloc = (destructor)py_tevent_req_dealloc,
	.tp_getset = py_tevent_req_getsetters,
	/* FIXME: .tp_new = py_tevent_req_new, */
};

static PyObject *py_tevent_queue_get_length(TeventQueue_Object *self)
{
	return PyInt_FromLong(tevent_queue_length(self->queue));
}

static PyGetSetDef py_tevent_queue_getsetters[] = {
	{ "length", (getter)py_tevent_queue_get_length,
		NULL, "The number of elements in the queue." },
	{ NULL },
};

static void py_tevent_queue_dealloc(TeventQueue_Object *self)
{
	talloc_free(self->queue);
	PyObject_Del(self);
}

static PyTypeObject TeventQueue_Type = {
	.tp_name = "tevent.Queue",
	.tp_basicsize = sizeof(TeventQueue_Object),
	.tp_dealloc = (destructor)py_tevent_queue_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_getset = py_tevent_queue_getsetters,
	.tp_methods = py_tevent_queue_methods,
};

static PyObject *py_tevent_context_signal_support(PyObject *_self)
{
	TeventContext_Object *self = (TeventContext_Object *)_self;
	return PyBool_FromLong(tevent_signal_support(self->ev));
}

static PyGetSetDef py_tevent_context_getsetters[] = {
	{ "signal_support", (getter)py_tevent_context_signal_support,
		NULL, "if this platform and tevent context support signal handling" },
	{ NULL }
};

static void py_tevent_context_dealloc(TeventContext_Object *self)
{
	talloc_free(self->ev);
	PyObject_Del(self);
}

static PyObject *py_tevent_context_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	const char * const kwnames[] = { "name", NULL };
	char *name = NULL;
	struct tevent_context *ev;
	TeventContext_Object *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwnames, &name))
		return NULL;

	if (name == NULL) {
		ev = tevent_context_init(NULL);
	} else {
		ev = tevent_context_init_byname(NULL, name);
	}

	if (ev == NULL) {
		PyErr_SetNone(PyExc_RuntimeError);
		return NULL;
	}

	ret = PyObject_New(TeventContext_Object, type);
	if (ret == NULL) {
		PyErr_NoMemory();
		talloc_free(ev);
		return NULL;
	}

	ret->ev = ev;
	return (PyObject *)ret;
}

static PyTypeObject TeventContext_Type = {
	.tp_name = "_tevent.Context",
	.tp_new = py_tevent_context_new,
	.tp_basicsize = sizeof(TeventContext_Object),
	.tp_dealloc = (destructor)py_tevent_context_dealloc,
	.tp_methods = py_tevent_context_methods,
	.tp_getset = py_tevent_context_getsetters,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_set_default_backend(PyObject *self, PyObject *args)
{
	char *backend_name;
	if (!PyArg_ParseTuple(args, "s", &backend_name))
		return NULL;

	tevent_set_default_backend(backend_name);

	Py_RETURN_NONE;
}

static PyObject *py_backend_list(PyObject *self)
{
	PyObject *ret;
	int i;
	const char **backends;

	ret = PyList_New(0);
	if (ret == NULL) {
		return NULL;
	}

	backends = tevent_backend_list(NULL);
	if (backends == NULL) {
		PyErr_SetNone(PyExc_RuntimeError);
		Py_DECREF(ret);
		return NULL;
	}
	for (i = 0; backends[i]; i++) {
		PyList_Append(ret, PyString_FromString(backends[i]));
	}

	talloc_free(backends);

	return ret;
}

static PyMethodDef tevent_methods[] = {
	{ "register_backend", (PyCFunction)py_register_backend, METH_VARARGS,
		"register_backend(backend)" },
	{ "set_default_backend", (PyCFunction)py_set_default_backend, 
		METH_VARARGS, "set_default_backend(backend)" },
	{ "backend_list", (PyCFunction)py_backend_list, 
		METH_NOARGS, "backend_list() -> list" },
	{ NULL },
};

void init_tevent(void)
{
	PyObject *m;

	if (PyType_Ready(&TeventContext_Type) < 0)
		return;

	if (PyType_Ready(&TeventQueue_Type) < 0)
		return;

	if (PyType_Ready(&TeventReq_Type) < 0)
		return;

	if (PyType_Ready(&TeventSignal_Type) < 0)
		return;

	if (PyType_Ready(&TeventTimer_Type) < 0)
		return;

	if (PyType_Ready(&TeventFd_Type) < 0)
		return;

	m = Py_InitModule3("_tevent", tevent_methods, "Tevent integration for twisted.");
	if (m == NULL)
		return;

	Py_INCREF(&TeventContext_Type);
	PyModule_AddObject(m, "Context", (PyObject *)&TeventContext_Type);

	Py_INCREF(&TeventQueue_Type);
	PyModule_AddObject(m, "Queue", (PyObject *)&TeventQueue_Type);

	Py_INCREF(&TeventReq_Type);
	PyModule_AddObject(m, "Request", (PyObject *)&TeventReq_Type);

	Py_INCREF(&TeventSignal_Type);
	PyModule_AddObject(m, "Signal", (PyObject *)&TeventSignal_Type);

	Py_INCREF(&TeventTimer_Type);
	PyModule_AddObject(m, "Timer", (PyObject *)&TeventTimer_Type);

	Py_INCREF(&TeventFd_Type);
	PyModule_AddObject(m, "Fd", (PyObject *)&TeventFd_Type);
}
