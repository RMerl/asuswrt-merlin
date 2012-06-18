/* 
   Unix SMB/CIFS implementation.
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

#include "includes.h"
#include <Python.h>
#include "pycredentials.h"
#include "param/param.h"
#include "lib/cmdline/credentials.h"
#include "librpc/gen_ndr/samr.h" /* for struct samr_Password */
#include "libcli/util/pyerrors.h"
#include "param/pyparam.h"

#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

static PyObject *PyString_FromStringOrNULL(const char *str)
{
	if (str == NULL)
		Py_RETURN_NONE;
	return PyString_FromString(str);
}

static PyObject *py_creds_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	py_talloc_Object *ret = (py_talloc_Object *)type->tp_alloc(type, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->talloc_ctx = talloc_new(NULL);
	if (ret->talloc_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->ptr = cli_credentials_init(ret->talloc_ctx);
	return (PyObject *)ret;
}

static PyObject *py_creds_get_username(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_username(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_username(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_username(PyCredentials_AsCliCredentials(self), newval, obt));
}

static PyObject *py_creds_get_password(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_password(PyCredentials_AsCliCredentials(self)));
}


static PyObject *py_creds_set_password(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_password(PyCredentials_AsCliCredentials(self), newval, obt));
}

static PyObject *py_creds_get_domain(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_domain(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_domain(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_domain(PyCredentials_AsCliCredentials(self), newval, obt));
}

static PyObject *py_creds_get_realm(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_realm(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_realm(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_realm(PyCredentials_AsCliCredentials(self), newval, obt));
}

static PyObject *py_creds_get_bind_dn(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_bind_dn(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_bind_dn(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	if (!PyArg_ParseTuple(args, "s", &newval))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_bind_dn(PyCredentials_AsCliCredentials(self), newval));
}

static PyObject *py_creds_get_workstation(py_talloc_Object *self)
{
	return PyString_FromStringOrNULL(cli_credentials_get_workstation(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_workstation(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	return PyBool_FromLong(cli_credentials_set_workstation(PyCredentials_AsCliCredentials(self), newval, obt));
}

static PyObject *py_creds_is_anonymous(py_talloc_Object *self)
{
	return PyBool_FromLong(cli_credentials_is_anonymous(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_anonymous(py_talloc_Object *self)
{
	cli_credentials_set_anonymous(PyCredentials_AsCliCredentials(self));
	Py_RETURN_NONE;
}

static PyObject *py_creds_authentication_requested(py_talloc_Object *self)
{
        return PyBool_FromLong(cli_credentials_authentication_requested(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_wrong_password(py_talloc_Object *self)
{
        return PyBool_FromLong(cli_credentials_wrong_password(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_set_cmdline_callbacks(py_talloc_Object *self)
{
        return PyBool_FromLong(cli_credentials_set_cmdline_callbacks(PyCredentials_AsCliCredentials(self)));
}

static PyObject *py_creds_parse_string(py_talloc_Object *self, PyObject *args)
{
	char *newval;
	enum credentials_obtained obt = CRED_SPECIFIED;
	if (!PyArg_ParseTuple(args, "s|i", &newval, &obt))
		return NULL;

	cli_credentials_parse_string(PyCredentials_AsCliCredentials(self), newval, obt);
	Py_RETURN_NONE;
}

static PyObject *py_creds_get_nt_hash(py_talloc_Object *self)
{
	const struct samr_Password *ntpw = cli_credentials_get_nt_hash(PyCredentials_AsCliCredentials(self), self->ptr);

	return PyString_FromStringAndSize(discard_const_p(char, ntpw->hash), 16);
}

static PyObject *py_creds_set_kerberos_state(py_talloc_Object *self, PyObject *args)
{
	int state;
	if (!PyArg_ParseTuple(args, "i", &state))
		return NULL;

	cli_credentials_set_kerberos_state(PyCredentials_AsCliCredentials(self), state);
	Py_RETURN_NONE;
}

static PyObject *py_creds_guess(py_talloc_Object *self, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx;
	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	lp_ctx = lp_from_py_object(py_lp_ctx);
	if (lp_ctx == NULL) 
		return NULL;

	cli_credentials_guess(PyCredentials_AsCliCredentials(self), lp_ctx);

	Py_RETURN_NONE;
}

static PyObject *py_creds_set_machine_account(py_talloc_Object *self, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx;
	NTSTATUS status;
	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	lp_ctx = lp_from_py_object(py_lp_ctx);
	if (lp_ctx == NULL) 
		return NULL;

	status = cli_credentials_set_machine_account(PyCredentials_AsCliCredentials(self), lp_ctx);
	PyErr_NTSTATUS_IS_ERR_RAISE(status);

	Py_RETURN_NONE;
}

static PyMethodDef py_creds_methods[] = {
	{ "get_username", (PyCFunction)py_creds_get_username, METH_NOARGS,
		"S.get_username() -> username\nObtain username." },
	{ "set_username", (PyCFunction)py_creds_set_username, METH_VARARGS,
		"S.set_username(name, obtained=CRED_SPECIFIED) -> None\n"
		"Change username." },
	{ "get_password", (PyCFunction)py_creds_get_password, METH_NOARGS,
		"S.get_password() -> password\n"
		"Obtain password." },
	{ "set_password", (PyCFunction)py_creds_set_password, METH_VARARGS,
		"S.set_password(password, obtained=CRED_SPECIFIED) -> None\n"
		"Change password." },
	{ "get_domain", (PyCFunction)py_creds_get_domain, METH_NOARGS,
		"S.get_domain() -> domain\n"
		"Obtain domain name." },
	{ "set_domain", (PyCFunction)py_creds_set_domain, METH_VARARGS,
		"S.set_domain(domain, obtained=CRED_SPECIFIED) -> None\n"
		"Change domain name." },
	{ "get_realm", (PyCFunction)py_creds_get_realm, METH_NOARGS,
		"S.get_realm() -> realm\n"
		"Obtain realm name." },
	{ "set_realm", (PyCFunction)py_creds_set_realm, METH_VARARGS,
		"S.set_realm(realm, obtained=CRED_SPECIFIED) -> None\n"
		"Change realm name." },
	{ "get_bind_dn", (PyCFunction)py_creds_get_bind_dn, METH_NOARGS,
		"S.get_bind_dn() -> bind dn\n"
		"Obtain bind DN." },
	{ "set_bind_dn", (PyCFunction)py_creds_set_bind_dn, METH_VARARGS,
		"S.set_bind_dn(bind_dn) -> None\n"
		"Change bind DN." },
	{ "is_anonymous", (PyCFunction)py_creds_is_anonymous, METH_NOARGS,
		NULL },
	{ "set_anonymous", (PyCFunction)py_creds_set_anonymous, METH_NOARGS,
        	"S.set_anonymous() -> None\n"
		"Use anonymous credentials." },
	{ "get_workstation", (PyCFunction)py_creds_get_workstation, METH_NOARGS,
		NULL },
	{ "set_workstation", (PyCFunction)py_creds_set_workstation, METH_VARARGS,
		NULL },
	{ "authentication_requested", (PyCFunction)py_creds_authentication_requested, METH_NOARGS,
		NULL },
	{ "wrong_password", (PyCFunction)py_creds_wrong_password, METH_NOARGS,
		"S.wrong_password() -> bool\n"
		"Indicate the returned password was incorrect." },
	{ "set_cmdline_callbacks", (PyCFunction)py_creds_set_cmdline_callbacks, METH_NOARGS,
		"S.set_cmdline_callbacks() -> bool\n"
		"Use command-line to obtain credentials not explicitly set." },
	{ "parse_string", (PyCFunction)py_creds_parse_string, METH_VARARGS,
		"S.parse_string(text, obtained=CRED_SPECIFIED) -> None\n"
		"Parse credentials string." },
	{ "get_nt_hash", (PyCFunction)py_creds_get_nt_hash, METH_NOARGS,
		NULL },
	{ "set_kerberos_state", (PyCFunction)py_creds_set_kerberos_state, METH_VARARGS,
		NULL },
	{ "guess", (PyCFunction)py_creds_guess, METH_VARARGS, NULL },
	{ "set_machine_account", (PyCFunction)py_creds_set_machine_account, METH_VARARGS, NULL },
	{ NULL }
};

PyTypeObject PyCredentials = {
	.tp_name = "Credentials",
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_dealloc = py_talloc_dealloc,
	.tp_new = py_creds_new,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = py_creds_methods,
};

void initcredentials(void)
{
	PyObject *m;

	if (PyType_Ready(&PyCredentials) < 0)
		return;

	m = Py_InitModule3("credentials", NULL, "Credentials management.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "AUTO_USE_KERBEROS", PyInt_FromLong(CRED_AUTO_USE_KERBEROS));
	PyModule_AddObject(m, "DONT_USE_KERBEROS", PyInt_FromLong(CRED_DONT_USE_KERBEROS));
	PyModule_AddObject(m, "MUST_USE_KERBEROS", PyInt_FromLong(CRED_MUST_USE_KERBEROS));

	Py_INCREF(&PyCredentials);
	PyModule_AddObject(m, "Credentials", (PyObject *)&PyCredentials);
}
