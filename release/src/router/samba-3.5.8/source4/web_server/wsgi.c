/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright © Jelmer Vernooij <jelmer@samba.org> 2008

   Implementation of the WSGI interface described in PEP0333 
   (http://www.python.org/dev/peps/pep-0333)
   
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
#include "web_server/web_server.h"
#include "../lib/util/dlinklist.h"
#include "../lib/util/data_blob.h"
#include "lib/tls/tls.h"
#include <Python.h>

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

typedef struct {
	PyObject_HEAD
	struct websrv_context *web;
} web_request_Object;

static PyObject *start_response(PyObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *response_header, *exc_info = NULL;
	char *status;
	int i;
	const char *kwnames[] = {
		"status", "response_header", "exc_info", NULL
	};
	web_request_Object *py_web = (web_request_Object *)self;
	struct websrv_context *web = py_web->web;
	struct http_header *headers = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|O:start_response", discard_const_p(char *, kwnames), &status, &response_header, &exc_info)) {
		return NULL;
	}

	/* FIXME: exc_info */

	if (!PyList_Check(response_header)) {
		PyErr_SetString(PyExc_TypeError, "response_header should be list");
		return NULL;
	}

	for (i = 0; i < PyList_Size(response_header); i++) {
		struct http_header *hdr = talloc_zero(web, struct http_header);
		PyObject *item = PyList_GetItem(response_header, i);
		PyObject *py_name, *py_value;

		if (!PyTuple_Check(item)) {
			PyErr_SetString(PyExc_TypeError, "Expected tuple");
			return NULL;
		}

		if (PyTuple_Size(item) != 2) {
			PyErr_SetString(PyExc_TypeError, "header tuple has invalid size, expected 2");
			return NULL;
		}

		py_name = PyTuple_GetItem(item, 0);

		if (!PyString_Check(py_name)) {
			PyErr_SetString(PyExc_TypeError, "header name should be string");
			return NULL;
		}

		py_value = PyTuple_GetItem(item, 1);
		if (!PyString_Check(py_value)) {
			PyErr_SetString(PyExc_TypeError, "header value should be string");
			return NULL;
		}

		hdr->name = talloc_strdup(hdr, PyString_AsString(py_name));
		hdr->value = talloc_strdup(hdr, PyString_AsString(py_value));
		DLIST_ADD(headers, hdr);
	}

	websrv_output_headers(web, status, headers);

	Py_RETURN_NONE;
}

static PyMethodDef web_request_methods[] = {
	{ "start_response", (PyCFunction)start_response, METH_VARARGS|METH_KEYWORDS, NULL },
	{ NULL }
};


PyTypeObject web_request_Type = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "wsgi.Request",
	.tp_methods = web_request_methods,
	.tp_basicsize = sizeof(web_request_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
};

typedef struct {
	PyObject_HEAD
} error_Stream_Object;

static PyObject *py_error_flush(PyObject *self, PyObject *args, PyObject *kwargs)
{
	/* Nothing to do here */
	Py_RETURN_NONE;
}

static PyObject *py_error_write(PyObject *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "str", NULL };
	char *str = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s:write", discard_const_p(char *, kwnames), &str)) {
		return NULL;
	}

	DEBUG(0, ("WSGI App: %s", str));

	Py_RETURN_NONE;
}

static PyObject *py_error_writelines(PyObject *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "seq", NULL };
	PyObject *seq = NULL, *item;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:writelines", discard_const_p(char *, kwnames), &seq)) {
		return NULL;
	}
	
	while ((item = PyIter_Next(seq))) {
		char *str = PyString_AsString(item);

		DEBUG(0, ("WSGI App: %s", str));
	}

	Py_RETURN_NONE;
}

static PyMethodDef error_Stream_methods[] = {
	{ "flush", (PyCFunction)py_error_flush, METH_VARARGS|METH_KEYWORDS, NULL },
	{ "write", (PyCFunction)py_error_write, METH_VARARGS|METH_KEYWORDS, NULL },
	{ "writelines", (PyCFunction)py_error_writelines, METH_VARARGS|METH_KEYWORDS, NULL },
	{ NULL, NULL, 0, NULL }
};

PyTypeObject error_Stream_Type = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "wsgi.ErrorStream",
	.tp_basicsize = sizeof(error_Stream_Object),
	.tp_methods = error_Stream_methods,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
};

typedef struct {
	PyObject_HEAD
	struct websrv_context *web;
	size_t offset;
} input_Stream_Object;

static PyObject *py_input_read(PyObject *_self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "size", NULL };
	PyObject *ret;
	input_Stream_Object *self = (input_Stream_Object *)_self;
	int size = -1;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", discard_const_p(char *, kwnames), &size))
		return NULL;
	
	/* Don't read beyond buffer boundaries */
	if (size == -1)
		size = self->web->input.partial.length-self->offset;
	else
		size = MIN(size, self->web->input.partial.length-self->offset);

	ret = PyString_FromStringAndSize((char *)self->web->input.partial.data+self->offset, size);
	self->offset += size;
	
	return ret;
}

static PyObject *py_input_readline(PyObject *_self)
{
	/* FIXME */
	PyErr_SetString(PyExc_NotImplementedError, 
			"readline() not yet implemented");
	return NULL;
}

static PyObject *py_input_readlines(PyObject *_self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "hint", NULL };
	int hint;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", discard_const_p(char *, kwnames), &hint))
		return NULL;
	
	/* FIXME */
	PyErr_SetString(PyExc_NotImplementedError, 
			"readlines() not yet implemented");
	return NULL;
}

static PyObject *py_input___iter__(PyObject *_self)
{
	/* FIXME */
	PyErr_SetString(PyExc_NotImplementedError, 
			"__iter__() not yet implemented");
	return NULL;
}

static PyMethodDef input_Stream_methods[] = {
	{ "read", (PyCFunction)py_input_read, METH_VARARGS|METH_KEYWORDS, NULL },
	{ "readline", (PyCFunction)py_input_readline, METH_NOARGS, NULL },
	{ "readlines", (PyCFunction)py_input_readlines, METH_VARARGS|METH_KEYWORDS, NULL },
	{ "__iter__", (PyCFunction)py_input___iter__, METH_NOARGS, NULL },
	{ NULL, NULL, 0, NULL }
};

PyTypeObject input_Stream_Type = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "wsgi.InputStream",
	.tp_basicsize = sizeof(input_Stream_Object),
	.tp_methods = input_Stream_methods,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
};

static PyObject *Py_InputHttpStream(struct websrv_context *web)
{
	input_Stream_Object *ret = PyObject_New(input_Stream_Object, &input_Stream_Type);
	ret->web = web;
	ret->offset = 0;
	return (PyObject *)ret;
}

static PyObject *Py_ErrorHttpStream(void)
{
	error_Stream_Object *ret = PyObject_New(error_Stream_Object, &error_Stream_Type);
	return (PyObject *)ret;
}

static PyObject *create_environ(bool tls, int content_length, struct http_header *headers, const char *request_method, const char *servername, int serverport, PyObject *inputstream, const char *request_string)
{
	PyObject *env;
	PyObject *errorstream;
	PyObject *py_scheme;
	struct http_header *hdr;
	char *questionmark;
	
	env = PyDict_New();
	if (env == NULL) {
		return NULL;
	}

	errorstream = Py_ErrorHttpStream();
	if (errorstream == NULL) {
		Py_DECREF(env);
		Py_DECREF(inputstream);
		return NULL;
	}

	PyDict_SetItemString(env, "wsgi.input", inputstream);
	PyDict_SetItemString(env, "wsgi.errors", errorstream);
	PyDict_SetItemString(env, "wsgi.version", Py_BuildValue("(i,i)", 1, 0));
	PyDict_SetItemString(env, "wsgi.multithread", Py_False);
	PyDict_SetItemString(env, "wsgi.multiprocess", Py_True);
	PyDict_SetItemString(env, "wsgi.run_once", Py_False);
	PyDict_SetItemString(env, "SERVER_PROTOCOL", PyString_FromString("HTTP/1.0"));
	if (content_length > 0) {
		PyDict_SetItemString(env, "CONTENT_LENGTH", PyLong_FromLong(content_length));
	}
	PyDict_SetItemString(env, "REQUEST_METHOD", PyString_FromString(request_method));

	questionmark = strchr(request_string, '?');
	if (questionmark == NULL) {
		PyDict_SetItemString(env, "SCRIPT_NAME", PyString_FromString(request_string));
	} else {
		PyDict_SetItemString(env, "QUERY_STRING", PyString_FromString(questionmark+1));
		PyDict_SetItemString(env, "SCRIPT_NAME", PyString_FromStringAndSize(request_string, questionmark-request_string));
	}
	
	PyDict_SetItemString(env, "SERVER_NAME", PyString_FromString(servername));
	PyDict_SetItemString(env, "SERVER_PORT", PyInt_FromLong(serverport));
	for (hdr = headers; hdr; hdr = hdr->next) {
		char *name;
		if (!strcasecmp(hdr->name, "Content-Type")) {
			PyDict_SetItemString(env, "CONTENT_TYPE", PyString_FromString(hdr->value));
		} else { 
			asprintf(&name, "HTTP_%s", hdr->name);
			PyDict_SetItemString(env, name, PyString_FromString(hdr->value));
			free(name);
		}
	}

	if (tls) {
		py_scheme = PyString_FromString("https");
	} else {
		py_scheme = PyString_FromString("http");
	}
	PyDict_SetItemString(env, "wsgi.url_scheme", py_scheme);

	return env;
}

static void wsgi_process_http_input(struct web_server_data *wdata,
				    struct websrv_context *web)
{
	PyObject *py_environ, *result, *item, *iter;
	PyObject *request_handler = (PyObject *)wdata->private_data;
	struct socket_address *socket_address;

	web_request_Object *py_web = PyObject_New(web_request_Object, &web_request_Type);
	py_web->web = web;

	socket_address = socket_get_my_addr(web->conn->socket, web);
	py_environ = create_environ(tls_enabled(web->conn->socket),
				    web->input.content_length, 
				    web->input.headers, 
				    web->input.post_request?"POST":"GET",
				    socket_address->addr,
				    socket_address->port, 
				    Py_InputHttpStream(web),
				    web->input.url
				    );
	if (py_environ == NULL) {
		DEBUG(0, ("Unable to create WSGI environment object\n"));
		return;
	}

	result = PyObject_CallMethod(request_handler, discard_const_p(char, "__call__"), discard_const_p(char, "OO"),
				       py_environ, PyObject_GetAttrString((PyObject *)py_web, "start_response"));

	if (result == NULL) {
		DEBUG(0, ("error while running WSGI code\n"));
		return;
	}

	iter = PyObject_GetIter(result);
	Py_DECREF(result);

	/* Now, iter over all the data returned */

	while ((item = PyIter_Next(iter))) {
		websrv_output(web, PyString_AsString(item), PyString_Size(item));
		Py_DECREF(item);
	}

	Py_DECREF(iter);
}

bool wsgi_initialize(struct web_server_data *wdata)
{
	PyObject *py_swat;

	Py_Initialize();

	if (PyType_Ready(&web_request_Type) < 0)
		return false;

	if (PyType_Ready(&input_Stream_Type) < 0)
		return false;

	if (PyType_Ready(&error_Stream_Type) < 0)
		return false;

	wdata->http_process_input = wsgi_process_http_input;
	py_swat = PyImport_Import(PyString_FromString("swat"));
	if (py_swat == NULL) {
		DEBUG(0, ("Unable to find SWAT\n"));
		return false;
	}
	wdata->private_data = py_swat;
	return true;
}
