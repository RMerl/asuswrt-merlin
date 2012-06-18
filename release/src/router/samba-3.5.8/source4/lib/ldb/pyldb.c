/*
   Unix SMB/CIFS implementation.

   Python interface to ldb.

   Copyright (C) 2005,2006 Tim Potter <tpot@samba.org>
   Copyright (C) 2006 Simo Sorce <idra@samba.org>
   Copyright (C) 2007-2009 Jelmer Vernooij <jelmer@samba.org>
   Copyright (C) 2009 Matthias Dieter Wallnöfer

	 ** NOTE! The following LGPL license applies to the ldb
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

#include "replace.h"
#include "ldb_private.h"
#include <Python.h>
#include "pyldb.h"

/* There's no Py_ssize_t in 2.4, apparently */
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5
typedef int Py_ssize_t;
typedef inquiry lenfunc;
typedef intargfunc ssizeargfunc;
#endif

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

static void PyErr_SetLdbError(PyObject *error, int ret, struct ldb_context *ldb_ctx)
{
	if (ret == LDB_ERR_PYTHON_EXCEPTION)
		return; /* Python exception should already be set, just keep that */

	PyErr_SetObject(error, 
					Py_BuildValue(discard_const_p(char, "(i,s)"), ret, 
				  ldb_ctx == NULL?ldb_strerror(ret):ldb_errstring(ldb_ctx)));
}

static PyObject *PyExc_LdbError;

PyAPI_DATA(PyTypeObject) PyLdbMessage;
PyAPI_DATA(PyTypeObject) PyLdbModule;
PyAPI_DATA(PyTypeObject) PyLdbDn;
PyAPI_DATA(PyTypeObject) PyLdb;
PyAPI_DATA(PyTypeObject) PyLdbMessageElement;
PyAPI_DATA(PyTypeObject) PyLdbTree;

static PyObject *PyObject_FromLdbValue(struct ldb_context *ldb_ctx, 
							   struct ldb_message_element *el, 
							   struct ldb_val *val)
{
	struct ldb_val new_val;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	PyObject *ret;

	new_val = *val;

	ret = PyString_FromStringAndSize((const char *)new_val.data, new_val.length);

	talloc_free(mem_ctx);

	return ret;
}

/**
 * Obtain a ldb DN from a Python object.
 *
 * @param mem_ctx Memory context
 * @param object Python object
 * @param ldb_ctx LDB context
 * @return Whether or not the conversion succeeded
 */
bool PyObject_AsDn(TALLOC_CTX *mem_ctx, PyObject *object, 
		   struct ldb_context *ldb_ctx, struct ldb_dn **dn)
{
	struct ldb_dn *odn;

	if (ldb_ctx != NULL && PyString_Check(object)) {
		odn = ldb_dn_new(mem_ctx, ldb_ctx, PyString_AsString(object));
		*dn = odn;
		return true;
	}

	if (PyLdbDn_Check(object)) {
		*dn = PyLdbDn_AsDn(object);
		return true;
	}

	PyErr_SetString(PyExc_TypeError, "Expected DN");
	return false;
}

/**
 * Create a Python object from a ldb_result.
 *
 * @param result LDB result to convert
 * @return Python object with converted result (a list object)
 */
static PyObject *PyLdbResult_FromResult(struct ldb_result *result)
{
	PyObject *ret;
	int i;
	if (result == NULL) {
		Py_RETURN_NONE;
	} 
	ret = PyList_New(result->count);
	for (i = 0; i < result->count; i++) {
		PyList_SetItem(ret, i, PyLdbMessage_FromMessage(result->msgs[i])
		);
	}
	return ret;
}

/**
 * Create a LDB Result from a Python object. 
 * If conversion fails, NULL will be returned and a Python exception set.
 *
 * @param mem_ctx Memory context in which to allocate the LDB Result
 * @param obj Python object to convert
 * @return a ldb_result, or NULL if the conversion failed
 */
static struct ldb_result *PyLdbResult_AsResult(TALLOC_CTX *mem_ctx, 
											   PyObject *obj)
{
	struct ldb_result *res;
	int i;

	if (obj == Py_None)
		return NULL;

	res = talloc_zero(mem_ctx, struct ldb_result);
	res->count = PyList_Size(obj);
	res->msgs = talloc_array(res, struct ldb_message *, res->count);
	for (i = 0; i < res->count; i++) {
		PyObject *item = PyList_GetItem(obj, i);
		res->msgs[i] = PyLdbMessage_AsMessage(item);
	}
	return res;
}

static PyObject *py_ldb_dn_validate(PyLdbDnObject *self)
{
	return PyBool_FromLong(ldb_dn_validate(self->dn));
}

static PyObject *py_ldb_dn_is_valid(PyLdbDnObject *self)
{
	return PyBool_FromLong(ldb_dn_is_valid(self->dn));
}

static PyObject *py_ldb_dn_is_special(PyLdbDnObject *self)
{
	return PyBool_FromLong(ldb_dn_is_special(self->dn));
}

static PyObject *py_ldb_dn_is_null(PyLdbDnObject *self)
{
	return PyBool_FromLong(ldb_dn_is_null(self->dn));
}
 
static PyObject *py_ldb_dn_get_casefold(PyLdbDnObject *self)
{
	return PyString_FromString(ldb_dn_get_casefold(self->dn));
}

static PyObject *py_ldb_dn_get_linearized(PyLdbDnObject *self)
{
	return PyString_FromString(ldb_dn_get_linearized(self->dn));
}

static PyObject *py_ldb_dn_canonical_str(PyLdbDnObject *self)
{
	return PyString_FromString(ldb_dn_canonical_string(self->dn, self->dn));
}

static PyObject *py_ldb_dn_canonical_ex_str(PyLdbDnObject *self)
{
	return PyString_FromString(ldb_dn_canonical_ex_string(self->dn, self->dn));
}

static PyObject *py_ldb_dn_repr(PyLdbDnObject *self)
{
	return PyString_FromFormat("Dn(%s)", PyObject_REPR(PyString_FromString(ldb_dn_get_linearized(self->dn))));
}

static PyObject *py_ldb_dn_check_special(PyLdbDnObject *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	return ldb_dn_check_special(self->dn, name)?Py_True:Py_False;
}

static int py_ldb_dn_compare(PyLdbDnObject *dn1, PyLdbDnObject *dn2)
{
	int ret;
	ret = ldb_dn_compare(dn1->dn, dn2->dn);
	if (ret < 0) ret = -1;
	if (ret > 0) ret = 1;
	return ret;
}

static PyObject *py_ldb_dn_get_parent(PyLdbDnObject *self)
{
	struct ldb_dn *dn = PyLdbDn_AsDn((PyObject *)self);
	struct ldb_dn *parent;
	PyLdbDnObject *py_ret;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	parent = ldb_dn_get_parent(mem_ctx, dn);
	if (parent == NULL) {
		talloc_free(mem_ctx);
		Py_RETURN_NONE;
	}

	py_ret = (PyLdbDnObject *)PyLdbDn.tp_alloc(&PyLdbDn, 0);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}
	py_ret->mem_ctx = mem_ctx;
	py_ret->dn = parent;
	return (PyObject *)py_ret;
}

#define dn_ldb_ctx(dn) ((struct ldb_context *)dn)

static PyObject *py_ldb_dn_add_child(PyLdbDnObject *self, PyObject *args)
{
	PyObject *py_other;
	struct ldb_dn *dn, *other;
	if (!PyArg_ParseTuple(args, "O", &py_other))
		return NULL;

	dn = PyLdbDn_AsDn((PyObject *)self);

	if (!PyObject_AsDn(NULL, py_other, dn_ldb_ctx(dn), &other))
		return NULL;

	return ldb_dn_add_child(dn, other)?Py_True:Py_False;
}

static PyObject *py_ldb_dn_add_base(PyLdbDnObject *self, PyObject *args)
{
	PyObject *py_other;
	struct ldb_dn *other, *dn;
	if (!PyArg_ParseTuple(args, "O", &py_other))
		return NULL;

	dn = PyLdbDn_AsDn((PyObject *)self);

	if (!PyObject_AsDn(NULL, py_other, dn_ldb_ctx(dn), &other))
		return NULL;

	return ldb_dn_add_base(dn, other)?Py_True:Py_False;
}

static PyMethodDef py_ldb_dn_methods[] = {
	{ "validate", (PyCFunction)py_ldb_dn_validate, METH_NOARGS, 
		"S.validate() -> bool\n"
		"Validate DN is correct." },
	{ "is_valid", (PyCFunction)py_ldb_dn_is_valid, METH_NOARGS,
		"S.is_valid() -> bool\n" },
	{ "is_special", (PyCFunction)py_ldb_dn_is_special, METH_NOARGS,
		"S.is_special() -> bool\n"
		"Check whether this is a special LDB DN." },
	{ "is_null", (PyCFunction)py_ldb_dn_is_null, METH_NOARGS,
		"Check whether this is a null DN." },
	{ "get_casefold", (PyCFunction)py_ldb_dn_get_casefold, METH_NOARGS,
		NULL },
	{ "get_linearized", (PyCFunction)py_ldb_dn_get_linearized, METH_NOARGS,
		NULL },
	{ "canonical_str", (PyCFunction)py_ldb_dn_canonical_str, METH_NOARGS,
		"S.canonical_str() -> string\n"
		"Canonical version of this DN (like a posix path)." },
	{ "canonical_ex_str", (PyCFunction)py_ldb_dn_canonical_ex_str, METH_NOARGS,
		"S.canonical_ex_str() -> string\n"
		"Canonical version of this DN (like a posix path, with terminating newline)." },
	{ "check_special", (PyCFunction)py_ldb_dn_is_special, METH_VARARGS, 
		NULL },
	{ "parent", (PyCFunction)py_ldb_dn_get_parent, METH_NOARGS,
   		"S.parent() -> dn\n"
		"Get the parent for this DN." },
	{ "add_child", (PyCFunction)py_ldb_dn_add_child, METH_VARARGS, 
		"S.add_child(dn) -> None\n"
		"Add a child DN to this DN." },
	{ "add_base", (PyCFunction)py_ldb_dn_add_base, METH_VARARGS,
		"S.add_base(dn) -> None\n"
		"Add a base DN to this DN." },
	{ "check_special", (PyCFunction)py_ldb_dn_check_special, METH_VARARGS,
		NULL },
	{ NULL }
};

static Py_ssize_t py_ldb_dn_len(PyLdbDnObject *self)
{
	return ldb_dn_get_comp_num(PyLdbDn_AsDn((PyObject *)self));
}

static PyObject *py_ldb_dn_concat(PyLdbDnObject *self, PyObject *py_other)
{
	struct ldb_dn *dn = PyLdbDn_AsDn((PyObject *)self), 
				  *other;
	PyLdbDnObject *py_ret;
	
	if (!PyObject_AsDn(NULL, py_other, NULL, &other))
		return NULL;

	py_ret = (PyLdbDnObject *)PyLdbDn.tp_alloc(&PyLdbDn, 0);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	py_ret->mem_ctx = talloc_new(NULL);
	py_ret->dn = ldb_dn_copy(py_ret->mem_ctx, dn);
	ldb_dn_add_child(py_ret->dn, other);
	return (PyObject *)py_ret;
}

static PySequenceMethods py_ldb_dn_seq = {
	.sq_length = (lenfunc)py_ldb_dn_len,
	.sq_concat = (binaryfunc)py_ldb_dn_concat,
};

static PyObject *py_ldb_dn_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	struct ldb_dn *ret;
	char *str;
	PyObject *py_ldb;
	struct ldb_context *ldb_ctx;
	TALLOC_CTX *mem_ctx;
	PyLdbDnObject *py_ret;
	const char * const kwnames[] = { "ldb", "dn", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os",
					 discard_const_p(char *, kwnames),
					 &py_ldb, &str))
		return NULL;

	ldb_ctx = PyLdb_AsLdbContext(py_ldb);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ret = ldb_dn_new(mem_ctx, ldb_ctx, str);

	if (ret == NULL || !ldb_dn_validate(ret)) {
		talloc_free(mem_ctx);
		PyErr_SetString(PyExc_ValueError, "unable to parse dn string");
		return NULL;
	}

	py_ret = (PyLdbDnObject *)type->tp_alloc(type, 0);
	if (ret == NULL) {
		talloc_free(mem_ctx);
		PyErr_NoMemory();
		return NULL;
	}
	py_ret->mem_ctx = mem_ctx;
	py_ret->dn = ret;
	return (PyObject *)py_ret;
}

PyObject *PyLdbDn_FromDn(struct ldb_dn *dn)
{
	PyLdbDnObject *py_ret;

	if (dn == NULL) {
		Py_RETURN_NONE;
	}

	py_ret = (PyLdbDnObject *)PyLdbDn.tp_alloc(&PyLdbDn, 0);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	py_ret->mem_ctx = talloc_new(NULL);
	py_ret->dn = talloc_reference(py_ret->mem_ctx, dn);
	return (PyObject *)py_ret;
}

static void py_ldb_dn_dealloc(PyLdbDnObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdbDn = {
	.tp_name = "Dn",
	.tp_methods = py_ldb_dn_methods,
	.tp_str = (reprfunc)py_ldb_dn_get_linearized,
	.tp_repr = (reprfunc)py_ldb_dn_repr,
	.tp_compare = (cmpfunc)py_ldb_dn_compare,
	.tp_as_sequence = &py_ldb_dn_seq,
	.tp_doc = "A LDB distinguished name.",
	.tp_new = py_ldb_dn_new,
	.tp_dealloc = (destructor)py_ldb_dn_dealloc,
	.tp_basicsize = sizeof(PyLdbObject),
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

/* Debug */
static void py_ldb_debug(void *context, enum ldb_debug_level level, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3, 0);
static void py_ldb_debug(void *context, enum ldb_debug_level level, const char *fmt, va_list ap)
{
	PyObject *fn = (PyObject *)context;
	PyObject_CallFunction(fn, discard_const_p(char, "(i,O)"), level, PyString_FromFormatV(fmt, ap));
}

static PyObject *py_ldb_set_debug(PyLdbObject *self, PyObject *args)
{
	PyObject *cb;

	if (!PyArg_ParseTuple(args, "O", &cb))
		return NULL;

	Py_INCREF(cb);
	/* FIXME: Where do we DECREF cb ? */
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ldb_set_debug(self->ldb_ctx, py_ldb_debug, cb), PyLdb_AsLdbContext(self));

	Py_RETURN_NONE;
}

static PyObject *py_ldb_set_create_perms(PyTypeObject *self, PyObject *args)
{
	unsigned int perms;
	if (!PyArg_ParseTuple(args, "I", &perms))
		return NULL;

	ldb_set_create_perms(PyLdb_AsLdbContext(self), perms);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_set_modules_dir(PyTypeObject *self, PyObject *args)
{
	char *modules_dir;
	if (!PyArg_ParseTuple(args, "s", &modules_dir))
		return NULL;

	ldb_set_modules_dir(PyLdb_AsLdbContext(self), modules_dir);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_transaction_start(PyLdbObject *self)
{
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ldb_transaction_start(PyLdb_AsLdbContext(self)), PyLdb_AsLdbContext(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_transaction_commit(PyLdbObject *self)
{
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ldb_transaction_commit(PyLdb_AsLdbContext(self)), PyLdb_AsLdbContext(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_transaction_cancel(PyLdbObject *self)
{
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ldb_transaction_cancel(PyLdb_AsLdbContext(self)), PyLdb_AsLdbContext(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_setup_wellknown_attributes(PyLdbObject *self)
{
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ldb_setup_wellknown_attributes(PyLdb_AsLdbContext(self)), PyLdb_AsLdbContext(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_repr(PyLdbObject *self)
{
	return PyString_FromFormat("<ldb connection>");
}

static PyObject *py_ldb_get_root_basedn(PyLdbObject *self)
{
	struct ldb_dn *dn = ldb_get_root_basedn(PyLdb_AsLdbContext(self));
	if (dn == NULL)
		Py_RETURN_NONE;
	return PyLdbDn_FromDn(dn);
}


static PyObject *py_ldb_get_schema_basedn(PyLdbObject *self)
{
	struct ldb_dn *dn = ldb_get_schema_basedn(PyLdb_AsLdbContext(self));
	if (dn == NULL)
		Py_RETURN_NONE;
	return PyLdbDn_FromDn(dn);
}

static PyObject *py_ldb_get_config_basedn(PyLdbObject *self)
{
	struct ldb_dn *dn = ldb_get_config_basedn(PyLdb_AsLdbContext(self));
	if (dn == NULL)
		Py_RETURN_NONE;
	return PyLdbDn_FromDn(dn);
}

static PyObject *py_ldb_get_default_basedn(PyLdbObject *self)
{
	struct ldb_dn *dn = ldb_get_default_basedn(PyLdb_AsLdbContext(self));
	if (dn == NULL)
		Py_RETURN_NONE;
	return PyLdbDn_FromDn(dn);
}

static const char **PyList_AsStringList(TALLOC_CTX *mem_ctx, PyObject *list, 
										const char *paramname)
{
	const char **ret;
	int i;
	if (!PyList_Check(list)) {
		PyErr_Format(PyExc_TypeError, "%s is not a list", paramname);
		return NULL;
	}
	ret = talloc_array(NULL, const char *, PyList_Size(list)+1);
	for (i = 0; i < PyList_Size(list); i++) {
		PyObject *item = PyList_GetItem(list, i);
		if (!PyString_Check(item)) {
			PyErr_Format(PyExc_TypeError, "%s should be strings", paramname);
			return NULL;
		}
		ret[i] = talloc_strndup(ret, PyString_AsString(item),
							   PyString_Size(item));
	}
	ret[i] = NULL;
	return ret;
}

static int py_ldb_init(PyLdbObject *self, PyObject *args, PyObject *kwargs)
{
	const char * const kwnames[] = { "url", "flags", "options", NULL };
	char *url = NULL;
	PyObject *py_options = Py_None;
	const char **options;
	int flags = 0;
	int ret;
	struct ldb_context *ldb;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ziO:Ldb.__init__",
					 discard_const_p(char *, kwnames),
					 &url, &flags, &py_options))
		return -1;

	ldb = PyLdb_AsLdbContext(self);

	if (py_options == Py_None) {
		options = NULL;
	} else {
		options = PyList_AsStringList(ldb, py_options, "options");
		if (options == NULL)
			return -1;
	}

	if (url != NULL) {
		ret = ldb_connect(ldb, url, flags, options);
		if (ret != LDB_SUCCESS) {
			PyErr_SetLdbError(PyExc_LdbError, ret, ldb);
			return -1;
		}
	}

	talloc_free(options);
	return 0;
}

static PyObject *py_ldb_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	PyLdbObject *ret;
	struct ldb_context *ldb;
	ret = (PyLdbObject *)type->tp_alloc(type, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	ldb = ldb_init(ret->mem_ctx, NULL);

	if (ldb == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ret->ldb_ctx = ldb;
	return (PyObject *)ret;
}

static PyObject *py_ldb_connect(PyLdbObject *self, PyObject *args, PyObject *kwargs)
{
	char *url;
	int flags = 0;
	PyObject *py_options = Py_None;
	int ret;
	const char **options;
	const char * const kwnames[] = { "url", "flags", "options", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ziO",
					 discard_const_p(char *, kwnames),
					 &url, &flags, &py_options))
		return NULL;

	if (py_options == Py_None) {
		options = NULL;
	} else {
		options = PyList_AsStringList(NULL, py_options, "options");
		if (options == NULL)
			return NULL;
	}

	ret = ldb_connect(PyLdb_AsLdbContext(self), url, flags, options);
	talloc_free(options);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));

	Py_RETURN_NONE;
}

static PyObject *py_ldb_modify(PyLdbObject *self, PyObject *args)
{
	PyObject *py_msg;
	int ret;
	if (!PyArg_ParseTuple(args, "O", &py_msg))
		return NULL;

	if (!PyLdbMessage_Check(py_msg)) {
		PyErr_SetString(PyExc_TypeError, "Expected Ldb Message");
		return NULL;
	}

	ret = ldb_modify(PyLdb_AsLdbContext(self), PyLdbMessage_AsMessage(py_msg));
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));

	Py_RETURN_NONE;
}

static PyObject *py_ldb_add(PyLdbObject *self, PyObject *args)
{
	PyObject *py_msg;
	int ret;
	Py_ssize_t dict_pos, msg_pos;
	struct ldb_message_element *msgel;
	struct ldb_message *msg;
	struct ldb_context *ldb_ctx;
	struct ldb_request *req;
	PyObject *key, *value;
	PyObject *py_controls = Py_None;
	TALLOC_CTX *mem_ctx;
	struct ldb_control **parsed_controls;

	if (!PyArg_ParseTuple(args, "O|O", &py_msg, &py_controls ))
		return NULL;
	ldb_ctx = PyLdb_AsLdbContext(self);

	mem_ctx = talloc_new(NULL);
	if (py_controls == Py_None) {
		parsed_controls = NULL;
	} else {
		const char **controls = PyList_AsStringList(ldb_ctx, py_controls, "controls");
		parsed_controls = ldb_parse_control_strings(ldb_ctx, ldb_ctx, controls);
		talloc_free(controls);
	}
	if (PyDict_Check(py_msg)) {
		PyObject *dn_value = PyDict_GetItemString(py_msg, "dn");
		msg = ldb_msg_new(mem_ctx);
		msg->elements = talloc_zero_array(msg, struct ldb_message_element, PyDict_Size(py_msg));
		msg_pos = dict_pos = 0;
		if (dn_value) {
		   	if (!PyObject_AsDn(msg, dn_value, ldb_ctx, &msg->dn)) {
		   		PyErr_SetString(PyExc_TypeError, "unable to import dn object");
				talloc_free(mem_ctx);
				return NULL;
			}
			if (msg->dn == NULL) {
				PyErr_SetString(PyExc_TypeError, "dn set but not found");
				talloc_free(mem_ctx);
				return NULL;
			}
		}

		while (PyDict_Next(py_msg, &dict_pos, &key, &value)) {
			char *key_str = PyString_AsString(key);
			if (strcmp(key_str, "dn") != 0) {
				msgel = PyObject_AsMessageElement(msg->elements, value, 0, key_str);
				if (msgel == NULL) {
					PyErr_SetString(PyExc_TypeError, "unable to import element");
					talloc_free(mem_ctx);
					return NULL;
				}
				memcpy(&msg->elements[msg_pos], msgel, sizeof(*msgel));
				msg_pos++;
			}
		}

		if (msg->dn == NULL) {
			PyErr_SetString(PyExc_TypeError, "no dn set");
			talloc_free(mem_ctx);
			return NULL;
		}

		msg->num_elements = msg_pos;
	} else {
		msg = PyLdbMessage_AsMessage(py_msg);
	}
        
	ret = ldb_msg_sanity_check(ldb_ctx, msg);
        if (ret != LDB_SUCCESS) {
		PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));
		talloc_free(mem_ctx);
		return NULL;
        }

        ret = ldb_build_add_req(&req, ldb_ctx, ldb_ctx,
                                        msg,
                                        parsed_controls,
                                        NULL,
                                        ldb_op_default_callback,
                                        NULL);

        if (ret != LDB_SUCCESS) {
		PyErr_SetString(PyExc_TypeError, "failed to build request");
		talloc_free(mem_ctx);
		return NULL;
	}

        /* do request and autostart a transaction */
	/* Then let's LDB handle the message error in case of pb as they are meaningful */

        ret = ldb_transaction_start(ldb_ctx);
        if (ret != LDB_SUCCESS) {
		talloc_free(req);
		talloc_free(mem_ctx);
		PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));
        }

        ret = ldb_request(ldb_ctx, req);
        if (ret == LDB_SUCCESS) {
                ret = ldb_wait(req->handle, LDB_WAIT_ALL);
        } 

	if (ret == LDB_SUCCESS) {
                ret = ldb_transaction_commit(ldb_ctx);
        } else {
        	ldb_transaction_cancel(ldb_ctx);
		if (ldb_ctx->err_string == NULL) {
			/* no error string was setup by the backend */
			ldb_asprintf_errstring(ldb_ctx, "%s (%d)", ldb_strerror(ret), ret);
		}
	}
	talloc_free(req);
	talloc_free(mem_ctx);
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));

	Py_RETURN_NONE;
}

static PyObject *py_ldb_delete(PyLdbObject *self, PyObject *args)
{
	PyObject *py_dn;
	struct ldb_dn *dn;
	int ret;
	struct ldb_context *ldb;
	if (!PyArg_ParseTuple(args, "O", &py_dn))
		return NULL;

	ldb = PyLdb_AsLdbContext(self);

	if (!PyObject_AsDn(NULL, py_dn, ldb, &dn))
		return NULL;

	ret = ldb_delete(ldb, dn);
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, ldb);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_rename(PyLdbObject *self, PyObject *args)
{
	PyObject *py_dn1, *py_dn2;
	struct ldb_dn *dn1, *dn2;
	int ret;
	struct ldb_context *ldb;
	TALLOC_CTX *mem_ctx;
	if (!PyArg_ParseTuple(args, "OO", &py_dn1, &py_dn2))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ldb = PyLdb_AsLdbContext(self);
	if (!PyObject_AsDn(mem_ctx, py_dn1, ldb, &dn1)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	if (!PyObject_AsDn(mem_ctx, py_dn2, ldb, &dn2)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_rename(ldb, dn1, dn2);
	talloc_free(mem_ctx);
	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, ldb);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_schema_attribute_remove(PyLdbObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	ldb_schema_attribute_remove(PyLdb_AsLdbContext(self), name);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_schema_attribute_add(PyLdbObject *self, PyObject *args)
{
	char *attribute, *syntax;
	unsigned int flags;
	int ret;
	if (!PyArg_ParseTuple(args, "sIs", &attribute, &flags, &syntax))
		return NULL;

	ret = ldb_schema_attribute_add(PyLdb_AsLdbContext(self), attribute, flags, syntax);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, PyLdb_AsLdbContext(self));

	Py_RETURN_NONE;
}

static PyObject *ldb_ldif_to_pyobject(struct ldb_ldif *ldif)
{
	if (ldif == NULL) {
		Py_RETURN_NONE;
	} else {
	/* We don't want this attached to the 'ldb' any more */
		return Py_BuildValue(discard_const_p(char, "(iO)"),
				     ldif->changetype,
				     PyLdbMessage_FromMessage(ldif->msg));
	}
}


static PyObject *py_ldb_write_ldif(PyLdbMessageObject *self, PyObject *args)
{
	int changetype;
	PyObject *py_msg;
	struct ldb_ldif ldif;
	PyObject *ret;
	char *string;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTuple(args, "Oi", &py_msg, &changetype))
		return NULL;

	if (!PyLdbMessage_Check(py_msg)) {
		PyErr_SetString(PyExc_TypeError, "Expected Ldb Message for msg");
		return NULL;
	}

	ldif.msg = PyLdbMessage_AsMessage(py_msg);
	ldif.changetype = changetype;

	mem_ctx = talloc_new(NULL);

	string = ldb_ldif_write_string(PyLdb_AsLdbContext(self), mem_ctx, &ldif);
	if (!string) {
		PyErr_SetString(PyExc_KeyError, "Failed to generate LDIF");
		return NULL;
	}

	ret = PyString_FromString(string);

	talloc_free(mem_ctx);

	return ret;
}

static PyObject *py_ldb_parse_ldif(PyLdbObject *self, PyObject *args)
{
	PyObject *list;
	struct ldb_ldif *ldif;
	const char *s;

	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTuple(args, "s", &s))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) {
		Py_RETURN_NONE;
	}

	list = PyList_New(0);
	while (s && *s != '\0') {
		ldif = ldb_ldif_read_string(self->ldb_ctx, &s);
		talloc_steal(mem_ctx, ldif);
		if (ldif) {
			PyList_Append(list, ldb_ldif_to_pyobject(ldif));
		} else {
			PyErr_SetString(PyExc_ValueError, "unable to parse ldif string");
			talloc_free(mem_ctx);
			return NULL;
		}
	}
	talloc_free(mem_ctx); /* The pyobject already has a reference to the things it needs */
	return PyObject_GetIter(list);
}

static PyObject *py_ldb_msg_diff(PyLdbObject *self, PyObject *args)
{
	PyObject *py_msg_old;
	PyObject *py_msg_new;
	struct ldb_message *diff;
	PyObject *py_ret;

	if (!PyArg_ParseTuple(args, "OO", &py_msg_old, &py_msg_new))
		return NULL;

	if (!PyLdbMessage_Check(py_msg_old)) {
		PyErr_SetString(PyExc_TypeError, "Expected Ldb Message for old message");
		return NULL;
	}

	if (!PyLdbMessage_Check(py_msg_new)) {
		PyErr_SetString(PyExc_TypeError, "Expected Ldb Message for new message");
		return NULL;
	}

	diff = ldb_msg_diff(PyLdb_AsLdbContext(self), PyLdbMessage_AsMessage(py_msg_old), PyLdbMessage_AsMessage(py_msg_new));
	if (diff == NULL) 
		return NULL;

	py_ret = PyLdbMessage_FromMessage(diff);

	return py_ret;
}

static PyObject *py_ldb_schema_format_value(PyLdbObject *self, PyObject *args)
{
	const struct ldb_schema_attribute *a;
	struct ldb_val old_val;
	struct ldb_val new_val;
	TALLOC_CTX *mem_ctx;
	PyObject *ret;
	char *element_name;
	PyObject *val;

	if (!PyArg_ParseTuple(args, "sO", &element_name, &val))
		return NULL;

	mem_ctx = talloc_new(NULL);

	old_val.data = (uint8_t *)PyString_AsString(val);
	old_val.length = PyString_Size(val);

	a = ldb_schema_attribute_by_name(PyLdb_AsLdbContext(self), element_name);

	if (a == NULL) {
		Py_RETURN_NONE;
	}

	if (a->syntax->ldif_write_fn(PyLdb_AsLdbContext(self), mem_ctx, &old_val, &new_val) != 0) {
		talloc_free(mem_ctx);
		Py_RETURN_NONE;
	}

	ret = PyString_FromStringAndSize((const char *)new_val.data, new_val.length);

	talloc_free(mem_ctx);

	return ret;
}

static PyObject *py_ldb_search(PyLdbObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_base = Py_None;
	enum ldb_scope scope = LDB_SCOPE_DEFAULT;
	char *expr = NULL;
	PyObject *py_attrs = Py_None;
	PyObject *py_controls = Py_None;
	const char * const kwnames[] = { "base", "scope", "expression", "attrs", "controls", NULL };
	int ret;
	struct ldb_result *res;
	struct ldb_request *req;
	const char **attrs;
	struct ldb_context *ldb_ctx;
	struct ldb_control **parsed_controls;
	struct ldb_dn *base;
	PyObject *py_ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OizOO",
					 discard_const_p(char *, kwnames),
					 &py_base, &scope, &expr, &py_attrs, &py_controls))
		return NULL;

	ldb_ctx = PyLdb_AsLdbContext(self);

	if (py_attrs == Py_None) {
		attrs = NULL;
	} else {
		attrs = PyList_AsStringList(NULL, py_attrs, "attrs");
		if (attrs == NULL)
			return NULL;
	}

	if (py_base == Py_None) {
		base = ldb_get_default_basedn(ldb_ctx);
	} else {
		if (!PyObject_AsDn(ldb_ctx, py_base, ldb_ctx, &base)) {
			talloc_free(attrs);
			return NULL;
		}
	}

	if (py_controls == Py_None) {
		parsed_controls = NULL;
	} else {
		const char **controls = PyList_AsStringList(ldb_ctx, py_controls, "controls");
		parsed_controls = ldb_parse_control_strings(ldb_ctx, ldb_ctx, controls);
		talloc_free(controls);
	}

	res = talloc_zero(ldb_ctx, struct ldb_result);
	if (res == NULL) {
		PyErr_NoMemory();
		talloc_free(attrs);
		return NULL;
	}

	ret = ldb_build_search_req(&req, ldb_ctx, ldb_ctx,
				   base,
				   scope,
				   expr,
				   attrs,
				   parsed_controls,
				   res,
				   ldb_search_default_callback,
				   NULL);

	talloc_steal(req, attrs);

	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, ldb_ctx);
		return NULL;
	}

	ret = ldb_request(ldb_ctx, req);

	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);

	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, ldb_ctx);
		return NULL;
	}

	py_ret = PyLdbResult_FromResult(res);

	talloc_free(res);

	return py_ret;
}

static PyObject *py_ldb_get_opaque(PyLdbObject *self, PyObject *args)
{
	char *name;
	void *data;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	data = ldb_get_opaque(PyLdb_AsLdbContext(self), name);

	if (data == NULL)
		Py_RETURN_NONE;

	/* FIXME: More interpretation */

	return Py_True;
}

static PyObject *py_ldb_set_opaque(PyLdbObject *self, PyObject *args)
{
	char *name;
	PyObject *data;

	if (!PyArg_ParseTuple(args, "sO", &name, &data))
		return NULL;

	/* FIXME: More interpretation */

	ldb_set_opaque(PyLdb_AsLdbContext(self), name, data);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_modules(PyLdbObject *self)
{
	struct ldb_context *ldb = PyLdb_AsLdbContext(self);
	PyObject *ret = PyList_New(0);
	struct ldb_module *mod;

	for (mod = ldb->modules; mod; mod = mod->next) {
		PyList_Append(ret, PyLdbModule_FromModule(mod));
	}

	return ret;
}

static PyMethodDef py_ldb_methods[] = {
	{ "set_debug", (PyCFunction)py_ldb_set_debug, METH_VARARGS, 
		"S.set_debug(callback) -> None\n"
		"Set callback for LDB debug messages.\n"
		"The callback should accept a debug level and debug text." },
	{ "set_create_perms", (PyCFunction)py_ldb_set_create_perms, METH_VARARGS, 
		"S.set_create_perms(mode) -> None\n"
		"Set mode to use when creating new LDB files." },
	{ "set_modules_dir", (PyCFunction)py_ldb_set_modules_dir, METH_VARARGS,
		"S.set_modules_dir(path) -> None\n"
		"Set path LDB should search for modules" },
	{ "transaction_start", (PyCFunction)py_ldb_transaction_start, METH_NOARGS, 
		"S.transaction_start() -> None\n"
		"Start a new transaction." },
	{ "transaction_commit", (PyCFunction)py_ldb_transaction_commit, METH_NOARGS, 
		"S.transaction_commit() -> None\n"
		"commit a new transaction." },
	{ "transaction_cancel", (PyCFunction)py_ldb_transaction_cancel, METH_NOARGS, 
		"S.transaction_cancel() -> None\n"
		"cancel a new transaction." },
	{ "setup_wellknown_attributes", (PyCFunction)py_ldb_setup_wellknown_attributes, METH_NOARGS, 
		NULL },
	{ "get_root_basedn", (PyCFunction)py_ldb_get_root_basedn, METH_NOARGS,
		NULL },
	{ "get_schema_basedn", (PyCFunction)py_ldb_get_schema_basedn, METH_NOARGS,
		NULL },
	{ "get_default_basedn", (PyCFunction)py_ldb_get_default_basedn, METH_NOARGS,
		NULL },
	{ "get_config_basedn", (PyCFunction)py_ldb_get_config_basedn, METH_NOARGS,
		NULL },
	{ "connect", (PyCFunction)py_ldb_connect, METH_VARARGS|METH_KEYWORDS, 
		"S.connect(url, flags=0, options=None) -> None\n"
		"Connect to a LDB URL." },
	{ "modify", (PyCFunction)py_ldb_modify, METH_VARARGS, 
		"S.modify(message) -> None\n"
		"Modify an entry." },
	{ "add", (PyCFunction)py_ldb_add, METH_VARARGS, 
		"S.add(message) -> None\n"
		"Add an entry." },
	{ "delete", (PyCFunction)py_ldb_delete, METH_VARARGS,
		"S.delete(dn) -> None\n"
		"Remove an entry." },
	{ "rename", (PyCFunction)py_ldb_rename, METH_VARARGS,
		"S.rename(old_dn, new_dn) -> None\n"
		"Rename an entry." },
	{ "search", (PyCFunction)py_ldb_search, METH_VARARGS|METH_KEYWORDS,
		"S.search(base=None, scope=None, expression=None, attrs=None, controls=None) -> msgs\n"
		"Search in a database.\n"
		"\n"
		":param base: Optional base DN to search\n"
		":param scope: Search scope (SCOPE_BASE, SCOPE_ONELEVEL or SCOPE_SUBTREE)\n"
		":param expression: Optional search expression\n"
		":param attrs: Attributes to return (defaults to all)\n"
		":param controls: Optional list of controls\n"
		":return: Iterator over Message objects\n"
	},
	{ "schema_attribute_remove", (PyCFunction)py_ldb_schema_attribute_remove, METH_VARARGS,
		NULL },
	{ "schema_attribute_add", (PyCFunction)py_ldb_schema_attribute_add, METH_VARARGS,
		NULL },
	{ "schema_format_value", (PyCFunction)py_ldb_schema_format_value, METH_VARARGS,
		NULL },
	{ "parse_ldif", (PyCFunction)py_ldb_parse_ldif, METH_VARARGS,
		"S.parse_ldif(ldif) -> iter(messages)\n"
		"Parse a string formatted using LDIF." },
	{ "write_ldif", (PyCFunction)py_ldb_write_ldif, METH_VARARGS,
		"S.write_ldif(message, changetype) -> ldif\n"
		"Print the message as a string formatted using LDIF." },
	{ "msg_diff", (PyCFunction)py_ldb_msg_diff, METH_VARARGS,
		"S.msg_diff(Message) -> Message\n"
		"Return an LDB Message of the difference between two Message objects." },
	{ "get_opaque", (PyCFunction)py_ldb_get_opaque, METH_VARARGS,
		"S.get_opaque(name) -> value\n"
		"Get an opaque value set on this LDB connection. \n"
		":note: The returned value may not be useful in Python."
	},
	{ "set_opaque", (PyCFunction)py_ldb_set_opaque, METH_VARARGS,
		"S.set_opaque(name, value) -> None\n"
		"Set an opaque value on this LDB connection. \n"
		":note: Passing incorrect values may cause crashes." },
	{ "modules", (PyCFunction)py_ldb_modules, METH_NOARGS,
		"S.modules() -> list\n"
		"Return the list of modules on this LDB connection " },
	{ NULL },
};

PyObject *PyLdbModule_FromModule(struct ldb_module *mod)
{
	PyLdbModuleObject *ret;

	ret = (PyLdbModuleObject *)PyLdbModule.tp_alloc(&PyLdbModule, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	ret->mod = talloc_reference(ret->mem_ctx, mod);
	return (PyObject *)ret;
}

static PyObject *py_ldb_get_firstmodule(PyLdbObject *self, void *closure)
{
	return PyLdbModule_FromModule(PyLdb_AsLdbContext(self)->modules);
}

static PyGetSetDef py_ldb_getset[] = {
	{ discard_const_p(char, "firstmodule"), (getter)py_ldb_get_firstmodule, NULL, NULL },
	{ NULL }
};

static int py_ldb_contains(PyLdbObject *self, PyObject *obj)
{
	struct ldb_context *ldb_ctx = PyLdb_AsLdbContext(self);
	struct ldb_dn *dn;
	struct ldb_result *result;
	int ret;
	int count;

	if (!PyObject_AsDn(ldb_ctx, obj, ldb_ctx, &dn))
		return -1;

	ret = ldb_search(ldb_ctx, ldb_ctx, &result, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) {
		PyErr_SetLdbError(PyExc_LdbError, ret, ldb_ctx);
		return -1;
	}

	count = result->count;

	talloc_free(result);

	return count;
}

static PySequenceMethods py_ldb_seq = {
	.sq_contains = (objobjproc)py_ldb_contains,
};

PyObject *PyLdb_FromLdbContext(struct ldb_context *ldb_ctx)
{
	PyLdbObject *ret;

	ret = (PyLdbObject *)PyLdb.tp_alloc(&PyLdb, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	ret->ldb_ctx = talloc_reference(ret->mem_ctx, ldb_ctx);
	return (PyObject *)ret;
}

static void py_ldb_dealloc(PyLdbObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdb = {
	.tp_name = "Ldb",
	.tp_methods = py_ldb_methods,
	.tp_repr = (reprfunc)py_ldb_repr,
	.tp_new = py_ldb_new,
	.tp_init = (initproc)py_ldb_init,
	.tp_dealloc = (destructor)py_ldb_dealloc,
	.tp_getset = py_ldb_getset,
	.tp_getattro = PyObject_GenericGetAttr,
	.tp_basicsize = sizeof(PyLdbObject),
	.tp_doc = "Connection to a LDB database.",
	.tp_as_sequence = &py_ldb_seq,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};

static PyObject *py_ldb_module_repr(PyLdbModuleObject *self)
{
	return PyString_FromFormat("<ldb module '%s'>", PyLdbModule_AsModule(self)->ops->name);
}

static PyObject *py_ldb_module_str(PyLdbModuleObject *self)
{
	return PyString_FromString(PyLdbModule_AsModule(self)->ops->name);
}

static PyObject *py_ldb_module_start_transaction(PyLdbModuleObject *self)
{
	PyLdbModule_AsModule(self)->ops->start_transaction(PyLdbModule_AsModule(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_end_transaction(PyLdbModuleObject *self)
{
	PyLdbModule_AsModule(self)->ops->end_transaction(PyLdbModule_AsModule(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_del_transaction(PyLdbModuleObject *self)
{
	PyLdbModule_AsModule(self)->ops->del_transaction(PyLdbModule_AsModule(self));
	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_search(PyLdbModuleObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *py_base, *py_tree, *py_attrs, *py_ret;
	int ret, scope;
	struct ldb_request *req;
	const char * const kwnames[] = { "base", "scope", "tree", "attrs", NULL };
	struct ldb_module *mod;
	const char * const*attrs;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OiOO",
					 discard_const_p(char *, kwnames),
					 &py_base, &scope, &py_tree, &py_attrs))
		return NULL;

	mod = self->mod;

	if (py_attrs == Py_None) {
		attrs = NULL;
	} else {
		attrs = PyList_AsStringList(NULL, py_attrs, "attrs");
		if (attrs == NULL)
			return NULL;
	}

	ret = ldb_build_search_req(&req, mod->ldb, NULL, PyLdbDn_AsDn(py_base), 
			     scope, NULL /* expr */, attrs,
			     NULL /* controls */, NULL, NULL, NULL);

	talloc_steal(req, attrs);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, mod->ldb);

	req->op.search.res = NULL;

	ret = mod->ops->search(mod, req);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, mod->ldb);

	py_ret = PyLdbResult_FromResult(req->op.search.res);

	talloc_free(req);

	return py_ret;	
}


static PyObject *py_ldb_module_add(PyLdbModuleObject *self, PyObject *args)
{
	struct ldb_request *req;
	PyObject *py_message;
	int ret;
	struct ldb_module *mod;

	if (!PyArg_ParseTuple(args, "O", &py_message))
		return NULL;

	req = talloc_zero(NULL, struct ldb_request);
	req->operation = LDB_ADD;
	req->op.add.message = PyLdbMessage_AsMessage(py_message);

	mod = PyLdbModule_AsModule(self);
	ret = mod->ops->add(mod, req);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, mod->ldb);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_modify(PyLdbModuleObject *self, PyObject *args) 
{
	int ret;
	struct ldb_request *req;
	PyObject *py_message;
	struct ldb_module *mod;

	if (!PyArg_ParseTuple(args, "O", &py_message))
		return NULL;

	req = talloc_zero(NULL, struct ldb_request);
	req->operation = LDB_MODIFY;
	req->op.mod.message = PyLdbMessage_AsMessage(py_message);

	mod = PyLdbModule_AsModule(self);
	ret = mod->ops->modify(mod, req);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, mod->ldb);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_delete(PyLdbModuleObject *self, PyObject *args) 
{
	int ret;
	struct ldb_request *req;
	PyObject *py_dn;

	if (!PyArg_ParseTuple(args, "O", &py_dn))
		return NULL;

	req = talloc_zero(NULL, struct ldb_request);
	req->operation = LDB_DELETE;
	req->op.del.dn = PyLdbDn_AsDn(py_dn);

	ret = PyLdbModule_AsModule(self)->ops->del(PyLdbModule_AsModule(self), req);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, NULL);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_module_rename(PyLdbModuleObject *self, PyObject *args)
{
	int ret;
	struct ldb_request *req;
	PyObject *py_dn1, *py_dn2;

	if (!PyArg_ParseTuple(args, "OO", &py_dn1, &py_dn2))
		return NULL;

	req = talloc_zero(NULL, struct ldb_request);

	req->operation = LDB_RENAME;
	req->op.rename.olddn = PyLdbDn_AsDn(py_dn1);
	req->op.rename.newdn = PyLdbDn_AsDn(py_dn2);

	ret = PyLdbModule_AsModule(self)->ops->rename(PyLdbModule_AsModule(self), req);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, NULL);

	Py_RETURN_NONE;
}

static PyMethodDef py_ldb_module_methods[] = {
	{ "search", (PyCFunction)py_ldb_module_search, METH_VARARGS|METH_KEYWORDS, NULL },
	{ "add", (PyCFunction)py_ldb_module_add, METH_VARARGS, NULL },
	{ "modify", (PyCFunction)py_ldb_module_modify, METH_VARARGS, NULL },
	{ "rename", (PyCFunction)py_ldb_module_rename, METH_VARARGS, NULL },
	{ "delete", (PyCFunction)py_ldb_module_delete, METH_VARARGS, NULL },
	{ "start_transaction", (PyCFunction)py_ldb_module_start_transaction, METH_NOARGS, NULL },
	{ "end_transaction", (PyCFunction)py_ldb_module_end_transaction, METH_NOARGS, NULL },
	{ "del_transaction", (PyCFunction)py_ldb_module_del_transaction, METH_NOARGS, NULL },
	{ NULL },
};

static void py_ldb_module_dealloc(PyLdbModuleObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdbModule = {
	.tp_name = "LdbModule",
	.tp_methods = py_ldb_module_methods,
	.tp_repr = (reprfunc)py_ldb_module_repr,
	.tp_str = (reprfunc)py_ldb_module_str,
	.tp_basicsize = sizeof(PyLdbModuleObject),
	.tp_dealloc = (destructor)py_ldb_module_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};


/**
 * Create a ldb_message_element from a Python object.
 *
 * This will accept any sequence objects that contains strings, or 
 * a string object.
 *
 * A reference to set_obj will be borrowed. 
 *
 * @param mem_ctx Memory context
 * @param set_obj Python object to convert
 * @param flags ldb_message_element flags to set
 * @param attr_name Name of the attribute
 * @return New ldb_message_element, allocated as child of mem_ctx
 */
struct ldb_message_element *PyObject_AsMessageElement(TALLOC_CTX *mem_ctx,
											   PyObject *set_obj, int flags,
											   const char *attr_name)
{
	struct ldb_message_element *me;

	if (PyLdbMessageElement_Check(set_obj))
		return talloc_reference(mem_ctx, 
								PyLdbMessageElement_AsMessageElement(set_obj));

	me = talloc(mem_ctx, struct ldb_message_element);

	me->name = talloc_strdup(me, attr_name);
	me->flags = flags;
	if (PyString_Check(set_obj)) {
		me->num_values = 1;
		me->values = talloc_array(me, struct ldb_val, me->num_values);
		me->values[0].length = PyString_Size(set_obj);
		me->values[0].data = talloc_memdup(me, 
			(uint8_t *)PyString_AsString(set_obj), me->values[0].length);
	} else if (PySequence_Check(set_obj)) {
		int i;
		me->num_values = PySequence_Size(set_obj);
		me->values = talloc_array(me, struct ldb_val, me->num_values);
		for (i = 0; i < me->num_values; i++) {
			PyObject *obj = PySequence_GetItem(set_obj, i);

			me->values[i].length = PyString_Size(obj);
			me->values[i].data = talloc_memdup(me, 
				(uint8_t *)PyString_AsString(obj), me->values[i].length);
		}
	} else {
		talloc_free(me);
		me = NULL;
	}

	return me;
}


static PyObject *ldb_msg_element_to_set(struct ldb_context *ldb_ctx, 
								 struct ldb_message_element *me)
{
	int i;
	PyObject *result;

	/* Python << 2.5 doesn't have PySet_New and PySet_Add. */
	result = PyList_New(me->num_values);

	for (i = 0; i < me->num_values; i++) {
		PyList_SetItem(result, i,
			PyObject_FromLdbValue(ldb_ctx, me, &me->values[i]));
	}

	return result;
}

static PyObject *py_ldb_msg_element_get(PyLdbMessageElementObject *self, PyObject *args)
{
	int i;
	if (!PyArg_ParseTuple(args, "i", &i))
		return NULL;
	if (i < 0 || i >= PyLdbMessageElement_AsMessageElement(self)->num_values)
		Py_RETURN_NONE;

	return PyObject_FromLdbValue(NULL, PyLdbMessageElement_AsMessageElement(self), 
								 &(PyLdbMessageElement_AsMessageElement(self)->values[i]));
}

static PyObject *py_ldb_msg_element_flags(PyLdbMessageElementObject *self, PyObject *args)
{
	struct ldb_message_element *el;

	el = PyLdbMessageElement_AsMessageElement(self);
	return PyInt_FromLong(el->flags);
}

static PyObject *py_ldb_msg_element_set_flags(PyLdbMessageElementObject *self, PyObject *args)
{
	int flags;
	struct ldb_message_element *el;
	if (!PyArg_ParseTuple(args, "i", &flags))
		return NULL;

	el = PyLdbMessageElement_AsMessageElement(self);
	el->flags = flags;
	Py_RETURN_NONE;
}

static PyMethodDef py_ldb_msg_element_methods[] = {
	{ "get", (PyCFunction)py_ldb_msg_element_get, METH_VARARGS, NULL },
	{ "set_flags", (PyCFunction)py_ldb_msg_element_set_flags, METH_VARARGS, NULL },
	{ "flags", (PyCFunction)py_ldb_msg_element_flags, METH_NOARGS, NULL },
	{ NULL },
};

static Py_ssize_t py_ldb_msg_element_len(PyLdbMessageElementObject *self)
{
	return PyLdbMessageElement_AsMessageElement(self)->num_values;
}

static PyObject *py_ldb_msg_element_find(PyLdbMessageElementObject *self, Py_ssize_t idx)
{
	struct ldb_message_element *el = PyLdbMessageElement_AsMessageElement(self);
	if (idx < 0 || idx >= el->num_values) {
		PyErr_SetString(PyExc_IndexError, "Out of range");
		return NULL;
	}
	return PyString_FromStringAndSize((char *)el->values[idx].data, el->values[idx].length);
}

static PySequenceMethods py_ldb_msg_element_seq = {
	.sq_length = (lenfunc)py_ldb_msg_element_len,
	.sq_item = (ssizeargfunc)py_ldb_msg_element_find,
};

static int py_ldb_msg_element_cmp(PyLdbMessageElementObject *self, PyLdbMessageElementObject *other)
{
	return ldb_msg_element_compare(PyLdbMessageElement_AsMessageElement(self), 
								   PyLdbMessageElement_AsMessageElement(other));
}

static PyObject *py_ldb_msg_element_iter(PyLdbMessageElementObject *self)
{
	return PyObject_GetIter(ldb_msg_element_to_set(NULL, PyLdbMessageElement_AsMessageElement(self)));
}

PyObject *PyLdbMessageElement_FromMessageElement(struct ldb_message_element *el, TALLOC_CTX *mem_ctx)
{
	PyLdbMessageElementObject *ret;
	ret = (PyLdbMessageElementObject *)PyLdbMessageElement.tp_alloc(&PyLdbMessageElement, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	if (talloc_reference(ret->mem_ctx, mem_ctx) == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->el = el;
	return (PyObject *)ret;
}

static PyObject *py_ldb_msg_element_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	PyObject *py_elements = NULL;
	struct ldb_message_element *el;
	int flags = 0;
	char *name = NULL;
	const char * const kwnames[] = { "elements", "flags", "name", NULL };
	PyLdbMessageElementObject *ret;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Ois",
					 discard_const_p(char *, kwnames),
					 &py_elements, &flags, &name))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	el = talloc_zero(mem_ctx, struct ldb_message_element);

	if (py_elements != NULL) {
		int i;
		if (PyString_Check(py_elements)) {
			el->num_values = 1;
			el->values = talloc_array(el, struct ldb_val, 1);
			el->values[0].length = PyString_Size(py_elements);
			el->values[0].data = talloc_memdup(el, 
				(uint8_t *)PyString_AsString(py_elements), el->values[0].length);
		} else if (PySequence_Check(py_elements)) {
			el->num_values = PySequence_Size(py_elements);
			el->values = talloc_array(el, struct ldb_val, el->num_values);
			for (i = 0; i < el->num_values; i++) {
				PyObject *item = PySequence_GetItem(py_elements, i);
				if (!PyString_Check(item)) {
					PyErr_Format(PyExc_TypeError, 
							"Expected string as element %d in list", 
							i);
					talloc_free(mem_ctx);
					return NULL;
				}
				el->values[i].length = PyString_Size(item);
				el->values[i].data = talloc_memdup(el, 
					(uint8_t *)PyString_AsString(item), el->values[i].length);
			}
		} else {
			PyErr_SetString(PyExc_TypeError, 
					"Expected string or list");
			talloc_free(mem_ctx);
			return NULL;
		}
	}

	el->flags = flags;
	el->name = talloc_strdup(el, name);

	ret = (PyLdbMessageElementObject *)PyLdbMessageElement.tp_alloc(&PyLdbMessageElement, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}

	ret->mem_ctx = mem_ctx;
	ret->el = el;
	return (PyObject *)ret;
}

static PyObject *py_ldb_msg_element_repr(PyLdbMessageElementObject *self)
{
	char *element_str = NULL;
	int i;
	struct ldb_message_element *el = PyLdbMessageElement_AsMessageElement(self);
	PyObject *ret;

	for (i = 0; i < el->num_values; i++) {
		PyObject *o = py_ldb_msg_element_find(self, i);
		if (element_str == NULL)
			element_str = talloc_strdup(NULL, PyObject_REPR(o));
		else
			element_str = talloc_asprintf_append(element_str, ",%s", PyObject_REPR(o));
	}

	ret = PyString_FromFormat("MessageElement([%s])", element_str);

	talloc_free(element_str);

	return ret;
}

static PyObject *py_ldb_msg_element_str(PyLdbMessageElementObject *self)
{
	struct ldb_message_element *el = PyLdbMessageElement_AsMessageElement(self);

	if (el->num_values == 1)
		return PyString_FromStringAndSize((char *)el->values[0].data, el->values[0].length);
	else 
		Py_RETURN_NONE;
}

static void py_ldb_msg_element_dealloc(PyLdbMessageElementObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdbMessageElement = {
	.tp_name = "MessageElement",
	.tp_basicsize = sizeof(PyLdbMessageElementObject),
	.tp_dealloc = (destructor)py_ldb_msg_element_dealloc,
	.tp_repr = (reprfunc)py_ldb_msg_element_repr,
	.tp_str = (reprfunc)py_ldb_msg_element_str,
	.tp_methods = py_ldb_msg_element_methods,
	.tp_compare = (cmpfunc)py_ldb_msg_element_cmp,
	.tp_iter = (getiterfunc)py_ldb_msg_element_iter,
	.tp_as_sequence = &py_ldb_msg_element_seq,
	.tp_new = py_ldb_msg_element_new,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_ldb_msg_remove_attr(PyLdbMessageObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	ldb_msg_remove_attr(self->msg, name);

	Py_RETURN_NONE;
}

static PyObject *py_ldb_msg_keys(PyLdbMessageObject *self)
{
	struct ldb_message *msg = PyLdbMessage_AsMessage(self);
	int i, j = 0;
	PyObject *obj = PyList_New(msg->num_elements+(msg->dn != NULL?1:0));
	if (msg->dn != NULL) {
		PyList_SetItem(obj, j, PyString_FromString("dn"));
		j++;
	}
	for (i = 0; i < msg->num_elements; i++) {
		PyList_SetItem(obj, j, PyString_FromString(msg->elements[i].name));
		j++;
	}
	return obj;
}

static PyObject *py_ldb_msg_getitem_helper(PyLdbMessageObject *self, PyObject *py_name)
{
	struct ldb_message_element *el;
	char *name;
	struct ldb_message *msg = PyLdbMessage_AsMessage(self);
	if (!PyString_Check(py_name)) {
		PyErr_SetNone(PyExc_TypeError);
		return NULL;
	}
	name = PyString_AsString(py_name);
	if (!strcmp(name, "dn"))
		return PyLdbDn_FromDn(msg->dn);
	el = ldb_msg_find_element(msg, name);
	if (el == NULL) {
		return NULL;
	}
	return (PyObject *)PyLdbMessageElement_FromMessageElement(el, msg);
}

static PyObject *py_ldb_msg_getitem(PyLdbMessageObject *self, PyObject *py_name)
{
	PyObject *ret = py_ldb_msg_getitem_helper(self, py_name);
	if (ret == NULL) {
		PyErr_SetString(PyExc_KeyError, "No such element");
		return NULL;
	}
	return ret;
}

static PyObject *py_ldb_msg_get(PyLdbMessageObject *self, PyObject *args)
{
	PyObject *name, *ret;
	if (!PyArg_ParseTuple(args, "O", &name))
		return NULL;

	ret = py_ldb_msg_getitem_helper(self, name);
	if (ret == NULL) {
		if (PyErr_Occurred())
			return NULL;
		Py_RETURN_NONE;
	}
	return ret;
}

static PyObject *py_ldb_msg_items(PyLdbMessageObject *self)
{
	struct ldb_message *msg = PyLdbMessage_AsMessage(self);
	int i, j;
	PyObject *l = PyList_New(msg->num_elements + (msg->dn == NULL?0:1));
	j = 0;
	if (msg->dn != NULL) {
		PyList_SetItem(l, 0, Py_BuildValue("(sO)", "dn", PyLdbDn_FromDn(msg->dn)));
		j++;
	}
	for (i = 0; i < msg->num_elements; i++, j++) {
		PyList_SetItem(l, j, Py_BuildValue("(sO)", msg->elements[i].name, PyLdbMessageElement_FromMessageElement(&msg->elements[i], self->msg)));
	}
	return l;
}

static PyMethodDef py_ldb_msg_methods[] = { 
	{ "keys", (PyCFunction)py_ldb_msg_keys, METH_NOARGS, NULL },
	{ "remove", (PyCFunction)py_ldb_msg_remove_attr, METH_VARARGS, NULL },
	{ "get", (PyCFunction)py_ldb_msg_get, METH_VARARGS, NULL },
	{ "items", (PyCFunction)py_ldb_msg_items, METH_NOARGS, NULL },
	{ NULL },
};

static PyObject *py_ldb_msg_iter(PyLdbMessageObject *self)
{
	PyObject *list, *iter;

	list = py_ldb_msg_keys(self);
	iter = PyObject_GetIter(list);
	Py_DECREF(list);
	return iter;
}

static int py_ldb_msg_setitem(PyLdbMessageObject *self, PyObject *name, PyObject *value)
{
	char *attr_name;

	if (!PyString_Check(name)) {
		PyErr_SetNone(PyExc_TypeError);
		return -1;
	}
	
	attr_name = PyString_AsString(name);
	if (value == NULL) {
		/* delitem */
		ldb_msg_remove_attr(self->msg, attr_name);
	} else {
		struct ldb_message_element *el = PyObject_AsMessageElement(self->msg,
											value, 0, attr_name);
		if (el == NULL)
			return -1;
		ldb_msg_remove_attr(PyLdbMessage_AsMessage(self), attr_name);
		ldb_msg_add(PyLdbMessage_AsMessage(self), el, el->flags);
	}
	return 0;
}

static Py_ssize_t py_ldb_msg_length(PyLdbMessageObject *self)
{
	return PyLdbMessage_AsMessage(self)->num_elements;
}

static PyMappingMethods py_ldb_msg_mapping = {
	.mp_length = (lenfunc)py_ldb_msg_length,
	.mp_subscript = (binaryfunc)py_ldb_msg_getitem,
	.mp_ass_subscript = (objobjargproc)py_ldb_msg_setitem,
};

static PyObject *py_ldb_msg_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	const char * const kwnames[] = { "dn", NULL };
	struct ldb_message *ret;
	TALLOC_CTX *mem_ctx;
	PyObject *pydn = NULL;
	PyLdbMessageObject *py_ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O",
					 discard_const_p(char *, kwnames),
					 &pydn))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ret = ldb_msg_new(mem_ctx);
	if (ret == NULL) {
		talloc_free(mem_ctx);
		PyErr_NoMemory();
		return NULL;
	}

	if (pydn != NULL) {
		struct ldb_dn *dn;
		if (!PyObject_AsDn(NULL, pydn, NULL, &dn)) {
			talloc_free(mem_ctx);
			return NULL;
		}
		ret->dn = talloc_reference(ret, dn);
	}

	py_ret = (PyLdbMessageObject *)type->tp_alloc(type, 0);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		talloc_free(mem_ctx);
		return NULL;
	}

	py_ret->mem_ctx = mem_ctx;
	py_ret->msg = ret;
	return (PyObject *)py_ret;
}

PyObject *PyLdbMessage_FromMessage(struct ldb_message *msg)
{
	PyLdbMessageObject *ret;

	ret = (PyLdbMessageObject *)PyLdbMessage.tp_alloc(&PyLdbMessage, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	ret->msg = talloc_reference(ret->mem_ctx, msg);
	return (PyObject *)ret;
}

static PyObject *py_ldb_msg_get_dn(PyLdbMessageObject *self, void *closure)
{
	struct ldb_message *msg = PyLdbMessage_AsMessage(self);
	return PyLdbDn_FromDn(msg->dn);
}

static int py_ldb_msg_set_dn(PyLdbMessageObject *self, PyObject *value, void *closure)
{
	struct ldb_message *msg = PyLdbMessage_AsMessage(self);
	if (!PyLdbDn_Check(value)) {
		PyErr_SetNone(PyExc_TypeError);
		return -1;
	}

	msg->dn = talloc_reference(msg, PyLdbDn_AsDn(value));
	return 0;
}

static PyGetSetDef py_ldb_msg_getset[] = {
	{ discard_const_p(char, "dn"), (getter)py_ldb_msg_get_dn, (setter)py_ldb_msg_set_dn, NULL },
	{ NULL }
};

static PyObject *py_ldb_msg_repr(PyLdbMessageObject *self)
{
	PyObject *dict = PyDict_New(), *ret;
	if (PyDict_Update(dict, (PyObject *)self) != 0)
		return NULL;
	ret = PyString_FromFormat("Message(%s)", PyObject_REPR(dict));
	Py_DECREF(dict);
	return ret;
}

static void py_ldb_msg_dealloc(PyLdbMessageObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdbMessage = {
	.tp_name = "Message",
	.tp_methods = py_ldb_msg_methods,
	.tp_getset = py_ldb_msg_getset,
	.tp_as_mapping = &py_ldb_msg_mapping,
	.tp_basicsize = sizeof(PyLdbMessageObject),
	.tp_dealloc = (destructor)py_ldb_msg_dealloc,
	.tp_new = py_ldb_msg_new,
	.tp_repr = (reprfunc)py_ldb_msg_repr,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_iter = (getiterfunc)py_ldb_msg_iter,
};

PyObject *PyLdbTree_FromTree(struct ldb_parse_tree *tree)
{
	PyLdbTreeObject *ret;

	ret = (PyLdbTreeObject *)PyLdbTree.tp_alloc(&PyLdbTree, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ret->mem_ctx = talloc_new(NULL);
	ret->tree = talloc_reference(ret->mem_ctx, tree);
	return (PyObject *)ret;
}

static void py_ldb_tree_dealloc(PyLdbTreeObject *self)
{
	talloc_free(self->mem_ctx);
	self->ob_type->tp_free(self);
}

PyTypeObject PyLdbTree = {
	.tp_name = "Tree",
	.tp_basicsize = sizeof(PyLdbTreeObject),
	.tp_dealloc = (destructor)py_ldb_tree_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

/* Ldb_module */
static int py_module_search(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result, *py_base, *py_attrs, *py_tree;

	py_base = PyLdbDn_FromDn(req->op.search.base);

	if (py_base == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_tree = PyLdbTree_FromTree(req->op.search.tree);

	if (py_tree == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	if (req->op.search.attrs == NULL) {
		py_attrs = Py_None;
	} else {
		int i, len;
		for (len = 0; req->op.search.attrs[len]; len++);
		py_attrs = PyList_New(len);
		for (i = 0; i < len; i++)
			PyList_SetItem(py_attrs, i, PyString_FromString(req->op.search.attrs[i]));
	}

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "search"),
					discard_const_p(char, "OiOO"),
					py_base, req->op.search.scope, py_tree, py_attrs);

	Py_DECREF(py_attrs);
	Py_DECREF(py_tree);
	Py_DECREF(py_base);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	req->op.search.res = PyLdbResult_AsResult(NULL, py_result);
	if (req->op.search.res == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_add(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result, *py_msg;

	py_msg = PyLdbMessage_FromMessage(discard_const_p(struct ldb_message, req->op.add.message));

	if (py_msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "add"),
					discard_const_p(char, "O"),
					py_msg);

	Py_DECREF(py_msg);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_modify(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result, *py_msg;

	py_msg = PyLdbMessage_FromMessage(discard_const_p(struct ldb_message, req->op.mod.message));

	if (py_msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "modify"),
					discard_const_p(char, "O"),
					py_msg);

	Py_DECREF(py_msg);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_del(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result, *py_dn;

	py_dn = PyLdbDn_FromDn(req->op.del.dn);

	if (py_dn == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "delete"),
					discard_const_p(char, "O"),
					py_dn);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_rename(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result, *py_olddn, *py_newdn;

	py_olddn = PyLdbDn_FromDn(req->op.rename.olddn);

	if (py_olddn == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_newdn = PyLdbDn_FromDn(req->op.rename.newdn);

	if (py_newdn == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "rename"),
					discard_const_p(char, "OO"),
					py_olddn, py_newdn);

	Py_DECREF(py_olddn);
	Py_DECREF(py_newdn);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_request(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "request"),
					discard_const_p(char, ""));

	return LDB_ERR_OPERATIONS_ERROR;
}

static int py_module_extended(struct ldb_module *mod, struct ldb_request *req)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "extended"),
					discard_const_p(char, ""));

	return LDB_ERR_OPERATIONS_ERROR;
}

static int py_module_start_transaction(struct ldb_module *mod)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "start_transaction"),
					discard_const_p(char, ""));

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_end_transaction(struct ldb_module *mod)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "end_transaction"),
					discard_const_p(char, ""));

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_del_transaction(struct ldb_module *mod)
{
	PyObject *py_ldb = (PyObject *)mod->private_data;
	PyObject *py_result;

	py_result = PyObject_CallMethod(py_ldb, discard_const_p(char, "del_transaction"),
					discard_const_p(char, ""));

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	Py_DECREF(py_result);

	return LDB_SUCCESS;
}

static int py_module_destructor(struct ldb_module *mod)
{
	Py_DECREF((PyObject *)mod->private_data);
	return 0;
}

static int py_module_init(struct ldb_module *mod)
{
	PyObject *py_class = (PyObject *)mod->ops->private_data;
	PyObject *py_result, *py_next, *py_ldb;

	py_ldb = PyLdb_FromLdbContext(mod->ldb);

	if (py_ldb == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_next = PyLdbModule_FromModule(mod->next);

	if (py_next == NULL)
		return LDB_ERR_OPERATIONS_ERROR;

	py_result = PyObject_CallFunction(py_class, discard_const_p(char, "OO"),
					  py_ldb, py_next);

	if (py_result == NULL) {
		return LDB_ERR_PYTHON_EXCEPTION;
	}

	mod->private_data = py_result;

	talloc_set_destructor(mod, py_module_destructor);

	return ldb_next_init(mod);
}

static PyObject *py_register_module(PyObject *module, PyObject *args)
{
	int ret;
	struct ldb_module_ops *ops;
	PyObject *input;

	if (!PyArg_ParseTuple(args, "O", &input))
		return NULL;

	ops = talloc_zero(talloc_autofree_context(), struct ldb_module_ops);
	if (ops == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ops->name = talloc_strdup(ops, PyString_AsString(PyObject_GetAttrString(input, discard_const_p(char, "name"))));

	Py_INCREF(input);
	ops->private_data = input;
	ops->init_context = py_module_init;
	ops->search = py_module_search;
	ops->add = py_module_add;
	ops->modify = py_module_modify;
	ops->del = py_module_del;
	ops->rename = py_module_rename;
	ops->request = py_module_request;
	ops->extended = py_module_extended;
	ops->start_transaction = py_module_start_transaction;
	ops->end_transaction = py_module_end_transaction;
	ops->del_transaction = py_module_del_transaction;

	ret = ldb_register_module(ops);

	PyErr_LDB_ERROR_IS_ERR_RAISE(PyExc_LdbError, ret, NULL);

	Py_RETURN_NONE;
}

static PyObject *py_timestring(PyObject *module, PyObject *args)
{
	time_t t;
	char *tresult;
	PyObject *ret;
	if (!PyArg_ParseTuple(args, "L", &t))
		return NULL;
	tresult = ldb_timestring(NULL, t);
	ret = PyString_FromString(tresult);
	talloc_free(tresult);
	return ret;
}

static PyObject *py_string_to_time(PyObject *module, PyObject *args)
{
	char *str;
	if (!PyArg_ParseTuple(args, "s", &str))
		return NULL;

	return PyInt_FromLong(ldb_string_to_time(str));
}

static PyObject *py_valid_attr_name(PyObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;
	return PyBool_FromLong(ldb_valid_attr_name(name));
}

static PyMethodDef py_ldb_global_methods[] = {
	{ "register_module", py_register_module, METH_VARARGS, 
		"S.register_module(module) -> None\n"
		"Register a LDB module."},
	{ "timestring", py_timestring, METH_VARARGS, 
		"S.timestring(int) -> string\n"
		"Generate a LDAP time string from a UNIX timestamp" },
	{ "string_to_time", py_string_to_time, METH_VARARGS,
		"S.string_to_time(string) -> int\n"
		"Parse a LDAP time string into a UNIX timestamp." },
	{ "valid_attr_name", py_valid_attr_name, METH_VARARGS,
		"S.valid_attr_name(name) -> bool\n"
		"Check whether the supplied name is a valid attribute name." },
	{ "open", (PyCFunction)py_ldb_new, METH_VARARGS|METH_KEYWORDS,
		NULL },
	{ NULL }
};

void initldb(void)
{
	PyObject *m;

	if (PyType_Ready(&PyLdbDn) < 0)
		return;

	if (PyType_Ready(&PyLdbMessage) < 0)
		return;

	if (PyType_Ready(&PyLdbMessageElement) < 0)
		return;

	if (PyType_Ready(&PyLdb) < 0)
		return;

	if (PyType_Ready(&PyLdbModule) < 0)
		return;

	if (PyType_Ready(&PyLdbTree) < 0)
		return;

	m = Py_InitModule3("ldb", py_ldb_global_methods, 
		"An interface to LDB, a LDAP-like API that can either to talk an embedded database (TDB-based) or a standards-compliant LDAP server.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "SCOPE_DEFAULT", PyInt_FromLong(LDB_SCOPE_DEFAULT));
	PyModule_AddObject(m, "SCOPE_BASE", PyInt_FromLong(LDB_SCOPE_BASE));
	PyModule_AddObject(m, "SCOPE_ONELEVEL", PyInt_FromLong(LDB_SCOPE_ONELEVEL));
	PyModule_AddObject(m, "SCOPE_SUBTREE", PyInt_FromLong(LDB_SCOPE_SUBTREE));

	PyModule_AddObject(m, "CHANGETYPE_NONE", PyInt_FromLong(LDB_CHANGETYPE_NONE));
	PyModule_AddObject(m, "CHANGETYPE_ADD", PyInt_FromLong(LDB_CHANGETYPE_ADD));
	PyModule_AddObject(m, "CHANGETYPE_DELETE", PyInt_FromLong(LDB_CHANGETYPE_DELETE));
	PyModule_AddObject(m, "CHANGETYPE_MODIFY", PyInt_FromLong(LDB_CHANGETYPE_MODIFY));

	PyModule_AddObject(m, "FLAG_MOD_ADD", PyInt_FromLong(LDB_FLAG_MOD_ADD));
	PyModule_AddObject(m, "FLAG_MOD_REPLACE", PyInt_FromLong(LDB_FLAG_MOD_REPLACE));
	PyModule_AddObject(m, "FLAG_MOD_DELETE", PyInt_FromLong(LDB_FLAG_MOD_DELETE));

	PyModule_AddObject(m, "SUCCESS", PyInt_FromLong(LDB_SUCCESS));
	PyModule_AddObject(m, "ERR_OPERATIONS_ERROR", PyInt_FromLong(LDB_ERR_OPERATIONS_ERROR));
	PyModule_AddObject(m, "ERR_PROTOCOL_ERROR", PyInt_FromLong(LDB_ERR_PROTOCOL_ERROR));
	PyModule_AddObject(m, "ERR_TIME_LIMIT_EXCEEDED", PyInt_FromLong(LDB_ERR_TIME_LIMIT_EXCEEDED));
	PyModule_AddObject(m, "ERR_SIZE_LIMIT_EXCEEDED", PyInt_FromLong(LDB_ERR_SIZE_LIMIT_EXCEEDED));
	PyModule_AddObject(m, "ERR_COMPARE_FALSE", PyInt_FromLong(LDB_ERR_COMPARE_FALSE));
	PyModule_AddObject(m, "ERR_COMPARE_TRUE", PyInt_FromLong(LDB_ERR_COMPARE_TRUE));
	PyModule_AddObject(m, "ERR_AUTH_METHOD_NOT_SUPPORTED", PyInt_FromLong(LDB_ERR_AUTH_METHOD_NOT_SUPPORTED));
	PyModule_AddObject(m, "ERR_STRONG_AUTH_REQUIRED", PyInt_FromLong(LDB_ERR_STRONG_AUTH_REQUIRED));
	PyModule_AddObject(m, "ERR_REFERRAL", PyInt_FromLong(LDB_ERR_REFERRAL));
	PyModule_AddObject(m, "ERR_ADMIN_LIMIT_EXCEEDED", PyInt_FromLong(LDB_ERR_ADMIN_LIMIT_EXCEEDED));
	PyModule_AddObject(m, "ERR_UNSUPPORTED_CRITICAL_EXTENSION", PyInt_FromLong(LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION));
	PyModule_AddObject(m, "ERR_CONFIDENTIALITY_REQUIRED", PyInt_FromLong(LDB_ERR_CONFIDENTIALITY_REQUIRED));
	PyModule_AddObject(m, "ERR_SASL_BIND_IN_PROGRESS", PyInt_FromLong(LDB_ERR_SASL_BIND_IN_PROGRESS));
	PyModule_AddObject(m, "ERR_NO_SUCH_ATTRIBUTE", PyInt_FromLong(LDB_ERR_NO_SUCH_ATTRIBUTE));
	PyModule_AddObject(m, "ERR_UNDEFINED_ATTRIBUTE_TYPE", PyInt_FromLong(LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE));
	PyModule_AddObject(m, "ERR_INAPPROPRIATE_MATCHING", PyInt_FromLong(LDB_ERR_INAPPROPRIATE_MATCHING));
	PyModule_AddObject(m, "ERR_CONSTRAINT_VIOLATION", PyInt_FromLong(LDB_ERR_CONSTRAINT_VIOLATION));
	PyModule_AddObject(m, "ERR_ATTRIBUTE_OR_VALUE_EXISTS", PyInt_FromLong(LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS));
	PyModule_AddObject(m, "ERR_INVALID_ATTRIBUTE_SYNTAX", PyInt_FromLong(LDB_ERR_INVALID_ATTRIBUTE_SYNTAX));
	PyModule_AddObject(m, "ERR_NO_SUCH_OBJECT", PyInt_FromLong(LDB_ERR_NO_SUCH_OBJECT));
	PyModule_AddObject(m, "ERR_ALIAS_PROBLEM", PyInt_FromLong(LDB_ERR_ALIAS_PROBLEM));
	PyModule_AddObject(m, "ERR_INVALID_DN_SYNTAX", PyInt_FromLong(LDB_ERR_INVALID_DN_SYNTAX));
	PyModule_AddObject(m, "ERR_ALIAS_DEREFERINCING_PROBLEM", PyInt_FromLong(LDB_ERR_ALIAS_DEREFERENCING_PROBLEM));
	PyModule_AddObject(m, "ERR_INAPPROPRIATE_AUTHENTICATION", PyInt_FromLong(LDB_ERR_INAPPROPRIATE_AUTHENTICATION));
	PyModule_AddObject(m, "ERR_INVALID_CREDENTIALS", PyInt_FromLong(LDB_ERR_INVALID_CREDENTIALS));
	PyModule_AddObject(m, "ERR_INSUFFICIENT_ACCESS_RIGHTS", PyInt_FromLong(LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS));
	PyModule_AddObject(m, "ERR_BUSY", PyInt_FromLong(LDB_ERR_BUSY));
	PyModule_AddObject(m, "ERR_UNAVAILABLE", PyInt_FromLong(LDB_ERR_UNAVAILABLE));
	PyModule_AddObject(m, "ERR_UNWILLING_TO_PERFORM", PyInt_FromLong(LDB_ERR_UNWILLING_TO_PERFORM));
	PyModule_AddObject(m, "ERR_LOOP_DETECT", PyInt_FromLong(LDB_ERR_LOOP_DETECT));
	PyModule_AddObject(m, "ERR_NAMING_VIOLATION", PyInt_FromLong(LDB_ERR_NAMING_VIOLATION));
	PyModule_AddObject(m, "ERR_OBJECT_CLASS_VIOLATION", PyInt_FromLong(LDB_ERR_OBJECT_CLASS_VIOLATION));
	PyModule_AddObject(m, "ERR_NOT_ALLOWED_ON_NON_LEAF", PyInt_FromLong(LDB_ERR_NOT_ALLOWED_ON_NON_LEAF));
	PyModule_AddObject(m, "ERR_NOT_ALLOWED_ON_RDN", PyInt_FromLong(LDB_ERR_NOT_ALLOWED_ON_RDN));
	PyModule_AddObject(m, "ERR_ENTRY_ALREADY_EXISTS", PyInt_FromLong(LDB_ERR_ENTRY_ALREADY_EXISTS));
	PyModule_AddObject(m, "ERR_OBJECT_CLASS_MODS_PROHIBITED", PyInt_FromLong(LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED));
	PyModule_AddObject(m, "ERR_AFFECTS_MULTIPLE_DSAS", PyInt_FromLong(LDB_ERR_AFFECTS_MULTIPLE_DSAS));
	PyModule_AddObject(m, "ERR_OTHER", PyInt_FromLong(LDB_ERR_OTHER));

        PyModule_AddObject(m, "FLG_RDONLY", PyInt_FromLong(LDB_FLG_RDONLY));
        PyModule_AddObject(m, "FLG_NOSYNC", PyInt_FromLong(LDB_FLG_NOSYNC));
        PyModule_AddObject(m, "FLG_RECONNECT", PyInt_FromLong(LDB_FLG_RECONNECT));
        PyModule_AddObject(m, "FLG_NOMMAP", PyInt_FromLong(LDB_FLG_NOMMAP));


	PyModule_AddObject(m, "__docformat__", PyString_FromString("restructuredText"));

	PyExc_LdbError = PyErr_NewException(discard_const_p(char, "_ldb.LdbError"), NULL, NULL);
	PyModule_AddObject(m, "LdbError", PyExc_LdbError);

	Py_INCREF(&PyLdb);
	Py_INCREF(&PyLdbDn);
	Py_INCREF(&PyLdbModule);
	Py_INCREF(&PyLdbMessage);
	Py_INCREF(&PyLdbMessageElement);
	Py_INCREF(&PyLdbTree);

	PyModule_AddObject(m, "Ldb", (PyObject *)&PyLdb);
	PyModule_AddObject(m, "Dn", (PyObject *)&PyLdbDn);
	PyModule_AddObject(m, "Message", (PyObject *)&PyLdbMessage);
	PyModule_AddObject(m, "MessageElement", (PyObject *)&PyLdbMessageElement);
	PyModule_AddObject(m, "Module", (PyObject *)&PyLdbModule);
	PyModule_AddObject(m, "Tree", (PyObject *)&PyLdbTree);
}
