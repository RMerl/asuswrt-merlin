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
#include "libcli/security/security.h"

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

static void PyType_AddMethods(PyTypeObject *type, PyMethodDef *methods)
{
	PyObject *dict;
	int i;
	if (type->tp_dict == NULL)
		type->tp_dict = PyDict_New();
	dict = type->tp_dict;
	for (i = 0; methods[i].ml_name; i++) {
		PyObject *descr;
		if (methods[i].ml_flags & METH_CLASS) 
			descr = PyCFunction_New(&methods[i], (PyObject *)type);
		else 
			descr = PyDescr_NewMethod(type, &methods[i]);
		PyDict_SetItemString(dict, methods[i].ml_name, 
				     descr);
	}
}

static PyObject *py_dom_sid_split(PyObject *py_self, PyObject *args)
{
	struct dom_sid *self = py_talloc_get_ptr(py_self);
	struct dom_sid *domain_sid;
	TALLOC_CTX *mem_ctx;
	uint32_t rid;
	NTSTATUS status;
	PyObject *py_domain_sid;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = dom_sid_split_rid(mem_ctx, self, &domain_sid, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetString(PyExc_RuntimeError, "dom_sid_split_rid failed");
		talloc_free(mem_ctx);
		return NULL;
	}

	py_domain_sid = py_talloc_steal(&dom_sid_Type, domain_sid);
	talloc_free(mem_ctx);
	return Py_BuildValue("(OI)", py_domain_sid, rid);
}

static int py_dom_sid_cmp(PyObject *py_self, PyObject *py_other)
{
	struct dom_sid *self = py_talloc_get_ptr(py_self), *other;
	other = py_talloc_get_ptr(py_other);
	if (other == NULL)
		return -1;

	return dom_sid_compare(self, other);
}

static PyObject *py_dom_sid_str(PyObject *py_self)
{
	struct dom_sid *self = py_talloc_get_ptr(py_self);
	char *str = dom_sid_string(NULL, self);
	PyObject *ret = PyString_FromString(str);
	talloc_free(str);
	return ret;
}

static PyObject *py_dom_sid_repr(PyObject *py_self)
{
	struct dom_sid *self = py_talloc_get_ptr(py_self);
	char *str = dom_sid_string(NULL, self);
	PyObject *ret = PyString_FromFormat("dom_sid('%s')", str);
	talloc_free(str);
	return ret;
}

static int py_dom_sid_init(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *str = NULL;
	struct dom_sid *sid = py_talloc_get_ptr(self);
	const char *kwnames[] = { "str", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", discard_const_p(char *, kwnames), &str))
		return -1;

	if (str != NULL && !dom_sid_parse(str, sid)) {
		PyErr_SetString(PyExc_TypeError, "Unable to parse string");
		return -1;
	}

	return 0;
}

static PyMethodDef py_dom_sid_extra_methods[] = {
	{ "split", (PyCFunction)py_dom_sid_split, METH_NOARGS,
		"S.split() -> (domain_sid, rid)\n"
		"Split a domain sid" },
	{ NULL }
};


static void py_dom_sid_patch(PyTypeObject *type)
{
	type->tp_init = py_dom_sid_init;
	type->tp_str = py_dom_sid_str;
	type->tp_repr = py_dom_sid_repr;
	type->tp_compare = py_dom_sid_cmp;
	PyType_AddMethods(type, py_dom_sid_extra_methods);
}

#define PY_DOM_SID_PATCH py_dom_sid_patch

static PyObject *py_descriptor_sacl_add(PyObject *self, PyObject *args)
{
	struct security_descriptor *desc = py_talloc_get_ptr(self);
	NTSTATUS status;
	struct security_ace *ace;
	PyObject *py_ace;

	if (!PyArg_ParseTuple(args, "O", &py_ace))
		return NULL;

	ace = py_talloc_get_ptr(py_ace);
	status = security_descriptor_sacl_add(desc, ace);
	PyErr_NTSTATUS_IS_ERR_RAISE(status);
	Py_RETURN_NONE;
}

static PyObject *py_descriptor_dacl_add(PyObject *self, PyObject *args)
{
	struct security_descriptor *desc = py_talloc_get_ptr(self);
	NTSTATUS status;
	struct security_ace *ace;
	PyObject *py_ace;

	if (!PyArg_ParseTuple(args, "O", &py_ace))
		return NULL;

	ace = py_talloc_get_ptr(py_ace);

	status = security_descriptor_dacl_add(desc, ace);
	PyErr_NTSTATUS_IS_ERR_RAISE(status);
	Py_RETURN_NONE;
}

static PyObject *py_descriptor_dacl_del(PyObject *self, PyObject *args)
{
	struct security_descriptor *desc = py_talloc_get_ptr(self);
	NTSTATUS status;
	struct dom_sid *sid;
	PyObject *py_sid;

	if (!PyArg_ParseTuple(args, "O", &py_sid))
		return NULL;

	sid = py_talloc_get_ptr(py_sid);
	status = security_descriptor_dacl_del(desc, sid);
	PyErr_NTSTATUS_IS_ERR_RAISE(status);
	Py_RETURN_NONE;
}

static PyObject *py_descriptor_sacl_del(PyObject *self, PyObject *args)
{
	struct security_descriptor *desc = py_talloc_get_ptr(self);
	NTSTATUS status;
	struct dom_sid *sid;
	PyObject *py_sid;

	if (!PyArg_ParseTuple(args, "O", &py_sid))
		return NULL;

	sid = py_talloc_get_ptr(py_sid);
	status = security_descriptor_sacl_del(desc, sid);
	PyErr_NTSTATUS_IS_ERR_RAISE(status);
	Py_RETURN_NONE;
}

static PyObject *py_descriptor_new(PyTypeObject *self, PyObject *args, PyObject *kwargs)
{
	return py_talloc_steal(self, security_descriptor_initialise(NULL));
}

static PyObject *py_descriptor_from_sddl(PyObject *self, PyObject *args)
{
	struct security_descriptor *secdesc;
	char *sddl;
	PyObject *py_sid;
	struct dom_sid *sid;

	if (!PyArg_ParseTuple(args, "sO!", &sddl, &dom_sid_Type, &py_sid))
		return NULL;

	sid = py_talloc_get_ptr(py_sid);

	secdesc = sddl_decode(NULL, sddl, sid);
	if (secdesc == NULL) {
		PyErr_SetString(PyExc_TypeError, "Unable to parse SDDL");
		return NULL;
	}

	return py_talloc_steal((PyTypeObject *)self, secdesc);
}

static PyObject *py_descriptor_as_sddl(PyObject *self, PyObject *args)
{
	struct dom_sid *sid;
	PyObject *py_sid = Py_None;
	struct security_descriptor *desc = py_talloc_get_ptr(self);
	char *text;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "|O!", &dom_sid_Type, &py_sid))
		return NULL;

	if (py_sid != Py_None)
		sid = py_talloc_get_ptr(py_sid);
	else
		sid = NULL;

	text = sddl_encode(NULL, desc, sid);

	ret = PyString_FromString(text);

	talloc_free(text);

	return ret;
}

static PyMethodDef py_descriptor_extra_methods[] = {
	{ "sacl_add", (PyCFunction)py_descriptor_sacl_add, METH_VARARGS,
		"S.sacl_add(ace) -> None\n"
		"Add a security ace to this security descriptor" },
	{ "dacl_add", (PyCFunction)py_descriptor_dacl_add, METH_VARARGS,
		NULL },
	{ "dacl_del", (PyCFunction)py_descriptor_dacl_del, METH_VARARGS,
		NULL },
	{ "sacl_del", (PyCFunction)py_descriptor_sacl_del, METH_VARARGS,
		NULL },
	{ "from_sddl", (PyCFunction)py_descriptor_from_sddl, METH_VARARGS|METH_CLASS, 
		NULL },
	{ "as_sddl", (PyCFunction)py_descriptor_as_sddl, METH_VARARGS,
		NULL },
	{ NULL }
};

static void py_descriptor_patch(PyTypeObject *type)
{
	type->tp_new = py_descriptor_new;
	PyType_AddMethods(type, py_descriptor_extra_methods);
}

#define PY_DESCRIPTOR_PATCH py_descriptor_patch

static PyObject *py_token_is_sid(PyObject *self, PyObject *args)
{
	PyObject *py_sid;
	struct dom_sid *sid;
	struct security_token *token = py_talloc_get_ptr(self);
	if (!PyArg_ParseTuple(args, "O", &py_sid))
		return NULL;

	sid = py_talloc_get_ptr(py_sid);

	return PyBool_FromLong(security_token_is_sid(token, sid));
}

static PyObject *py_token_has_sid(PyObject *self, PyObject *args)
{
	PyObject *py_sid;
	struct dom_sid *sid;
	struct security_token *token = py_talloc_get_ptr(self);
	if (!PyArg_ParseTuple(args, "O", &py_sid))
		return NULL;

	sid = py_talloc_get_ptr(py_sid);

	return PyBool_FromLong(security_token_has_sid(token, sid));
}

static PyObject *py_token_is_anonymous(PyObject *self)
{
	struct security_token *token = py_talloc_get_ptr(self);
	
	return PyBool_FromLong(security_token_is_anonymous(token));
}

static PyObject *py_token_is_system(PyObject *self)
{
	struct security_token *token = py_talloc_get_ptr(self);
	
	return PyBool_FromLong(security_token_is_system(token));
}

static PyObject *py_token_has_builtin_administrators(PyObject *self)
{
	struct security_token *token = py_talloc_get_ptr(self);
	
	return PyBool_FromLong(security_token_has_builtin_administrators(token));
}

static PyObject *py_token_has_nt_authenticated_users(PyObject *self)
{
	struct security_token *token = py_talloc_get_ptr(self);
	
	return PyBool_FromLong(security_token_has_nt_authenticated_users(token));
}

static PyObject *py_token_has_privilege(PyObject *self, PyObject *args)
{
	int priv;
	struct security_token *token = py_talloc_get_ptr(self);

	if (!PyArg_ParseTuple(args, "i", &priv))
		return NULL;

	return PyBool_FromLong(security_token_has_privilege(token, priv));
}

static PyObject *py_token_set_privilege(PyObject *self, PyObject *args)
{
	int priv;
	struct security_token *token = py_talloc_get_ptr(self);

	if (!PyArg_ParseTuple(args, "i", &priv))
		return NULL;

	security_token_set_privilege(token, priv);
	Py_RETURN_NONE;
}

static PyObject *py_token_new(PyTypeObject *self, PyObject *args, PyObject *kwargs)
{
	return py_talloc_steal(self, security_token_initialise(NULL));
}	

static PyMethodDef py_token_extra_methods[] = {
	{ "is_sid", (PyCFunction)py_token_is_sid, METH_VARARGS,
		"S.is_sid(sid) -> bool\n"
		"Check whether this token is of the specified SID." },
	{ "has_sid", (PyCFunction)py_token_has_sid, METH_VARARGS,
		NULL },
	{ "is_anonymous", (PyCFunction)py_token_is_anonymous, METH_NOARGS,
		"S.is_anonymus() -> bool\n"
		"Check whether this is an anonymous token." },
	{ "is_system", (PyCFunction)py_token_is_system, METH_NOARGS,
		NULL },
	{ "has_builtin_administrators", (PyCFunction)py_token_has_builtin_administrators, METH_NOARGS,
		NULL },
	{ "has_nt_authenticated_users", (PyCFunction)py_token_has_nt_authenticated_users, METH_NOARGS,
		NULL },
	{ "has_privilege", (PyCFunction)py_token_has_privilege, METH_VARARGS,
		NULL },
	{ "set_privilege", (PyCFunction)py_token_set_privilege, METH_VARARGS,
		NULL },
	{ NULL }
};

#define PY_TOKEN_PATCH py_token_patch
static void py_token_patch(PyTypeObject *type)
{
	type->tp_new = py_token_new;
	PyType_AddMethods(type, py_token_extra_methods);
}

static PyObject *py_privilege_name(PyObject *self, PyObject *args)
{
	int priv;
	if (!PyArg_ParseTuple(args, "i", &priv))
		return NULL;

	return PyString_FromString(sec_privilege_name(priv));
}

static PyObject *py_privilege_id(PyObject *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	return PyInt_FromLong(sec_privilege_id(name));
}

static PyObject *py_random_sid(PyObject *self)
{
	struct dom_sid *sid;
	PyObject *ret;
    	char *str = talloc_asprintf(NULL, "S-1-5-21-%u-%u-%u", 
			(unsigned)generate_random(), 
			(unsigned)generate_random(), 
			(unsigned)generate_random());

        sid = dom_sid_parse_talloc(NULL, str);
	talloc_free(str);
	ret = py_talloc_steal(&dom_sid_Type, sid);
	return ret;
}

static PyMethodDef py_mod_security_extra_methods[] = {
	{ "random_sid", (PyCFunction)py_random_sid, METH_NOARGS, NULL },
	{ "privilege_id", (PyCFunction)py_privilege_id, METH_VARARGS, NULL },
	{ "privilege_name", (PyCFunction)py_privilege_name, METH_VARARGS, NULL },
	{ NULL }
};

static void py_mod_security_patch(PyObject *m)
{
	int i;
	for (i = 0; py_mod_security_extra_methods[i].ml_name; i++) {
		PyObject *descr = PyCFunction_New(&py_mod_security_extra_methods[i], NULL);
		PyModule_AddObject(m, py_mod_security_extra_methods[i].ml_name,
				   descr);
	}
}

#define PY_MOD_SECURITY_PATCH py_mod_security_patch
