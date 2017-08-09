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

#include <Python.h>
#include "includes.h"
#include "pycredentials.h"
#include "param/param.h"
#include "lib/cmdline/credentials.h"
#include "librpc/gen_ndr/samr.h" /* for struct samr_Password */
#include "libcli/util/pyerrors.h"
#include "param/pyparam.h"
#include <tevent.h>

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

static PyObject *py_creds_set_krb_forwardable(py_talloc_Object *self, PyObject *args)
{
	int state;
	if (!PyArg_ParseTuple(args, "i", &state))
		return NULL;

	cli_credentials_set_krb_forwardable(PyCredentials_AsCliCredentials(self), state);
	Py_RETURN_NONE;
}

static PyObject *py_creds_guess(py_talloc_Object *self, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx;
	TALLOC_CTX *mem_ctx;
	struct cli_credentials *creds;

	creds = PyCredentials_AsCliCredentials(self);

	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	lp_ctx = lpcfg_from_py_object(mem_ctx, py_lp_ctx);
	if (lp_ctx == NULL) {
		talloc_free(mem_ctx);
		return NULL;
	}

	cli_credentials_guess(creds, lp_ctx);

	talloc_free(mem_ctx);

	Py_RETURN_NONE;
}

static PyObject *py_creds_set_machine_account(py_talloc_Object *self, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	struct loadparm_context *lp_ctx;
	NTSTATUS status;
	struct cli_credentials *creds;
	TALLOC_CTX *mem_ctx;

	creds = PyCredentials_AsCliCredentials(self);

	if (!PyArg_ParseTuple(args, "|O", &py_lp_ctx))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	lp_ctx = lpcfg_from_py_object(mem_ctx, py_lp_ctx);
	if (lp_ctx == NULL) {
		talloc_free(mem_ctx);
		return NULL;
	}

	status = cli_credentials_set_machine_account(creds, lp_ctx);
	talloc_free(mem_ctx);

	PyErr_NTSTATUS_IS_ERR_RAISE(status);

	Py_RETURN_NONE;
}

PyObject *PyCredentialCacheContainer_from_ccache_container(struct ccache_container *ccc)
{
	PyCredentialCacheContainerObject *py_ret;

	if (ccc == NULL) {
		Py_RETURN_NONE;
	}

	py_ret = (PyCredentialCacheContainerObject *)PyCredentialCacheContainer.tp_alloc(&PyCredentialCacheContainer, 0);
	if (py_ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	py_ret->mem_ctx = talloc_new(NULL);
	py_ret->ccc = talloc_reference(py_ret->mem_ctx, ccc);
	return (PyObject *)py_ret;
}


static PyObject *py_creds_get_named_ccache(py_talloc_Object *self, PyObject *args)
{
	PyObject *py_lp_ctx = Py_None;
	char *ccache_name;
	struct loadparm_context *lp_ctx;
	struct ccache_container *ccc;
	struct tevent_context *event_ctx;
	int ret;
	const char *error_string;
	struct cli_credentials *creds;
	TALLOC_CTX *mem_ctx;

	creds = PyCredentials_AsCliCredentials(self);

	if (!PyArg_ParseTuple(args, "|Os", &py_lp_ctx, &ccache_name))
		return NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	lp_ctx = lpcfg_from_py_object(mem_ctx, py_lp_ctx);
	if (lp_ctx == NULL) {
		talloc_free(mem_ctx);
		return NULL;
	}

	event_ctx = tevent_context_init(mem_ctx);

	ret = cli_credentials_get_named_ccache(creds, event_ctx, lp_ctx,
					       ccache_name, &ccc, &error_string);
	talloc_unlink(mem_ctx, lp_ctx);
	if (ret == 0) {
		talloc_steal(ccc, event_ctx);
		talloc_free(mem_ctx);
		return PyCredentialCacheContainer_from_ccache_container(ccc);
	}

	PyErr_SetString(PyExc_RuntimeError, error_string?error_string:"NULL");

	talloc_free(mem_ctx);
	return NULL;
}

static PyObject *py_creds_set_gensec_features(py_talloc_Object *self, PyObject *args)
{
	unsigned int gensec_features;

	if (!PyArg_ParseTuple(args, "I", &gensec_features))
		return NULL;

	cli_credentials_set_gensec_features(PyCredentials_AsCliCredentials(self), gensec_features);

	Py_RETURN_NONE;
}

static PyObject *py_creds_get_gensec_features(py_talloc_Object *self, PyObject *args)
{
	unsigned int gensec_features;

	gensec_features = cli_credentials_get_gensec_features(PyCredentials_AsCliCredentials(self));
	return PyInt_FromLong(gensec_features);
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
	{ "set_krb_forwardable", (PyCFunction)py_creds_set_krb_forwardable, METH_VARARGS,
		NULL },
	{ "guess", (PyCFunction)py_creds_guess, METH_VARARGS, NULL },
	{ "set_machine_account", (PyCFunction)py_creds_set_machine_account, METH_VARARGS, NULL },
	{ "get_named_ccache", (PyCFunction)py_creds_get_named_ccache, METH_VARARGS, NULL },
	{ "set_gensec_features", (PyCFunction)py_creds_set_gensec_features, METH_VARARGS, NULL },
	{ "get_gensec_features", (PyCFunction)py_creds_get_gensec_features, METH_NOARGS, NULL },
	{ NULL }
};

PyTypeObject PyCredentials = {
	.tp_name = "Credentials",
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_new = py_creds_new,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_methods = py_creds_methods,
};


PyTypeObject PyCredentialCacheContainer = {
	.tp_name = "CredentialCacheContainer",
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initcredentials(void)
{
	PyObject *m;
	PyTypeObject *talloc_type = PyTalloc_GetObjectType();
	if (talloc_type == NULL)
		return;

	PyCredentials.tp_base = PyCredentialCacheContainer.tp_base = talloc_type;

	if (PyType_Ready(&PyCredentials) < 0)
		return;

	if (PyType_Ready(&PyCredentialCacheContainer) < 0)
		return;

	m = Py_InitModule3("credentials", NULL, "Credentials management.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "AUTO_USE_KERBEROS", PyInt_FromLong(CRED_AUTO_USE_KERBEROS));
	PyModule_AddObject(m, "DONT_USE_KERBEROS", PyInt_FromLong(CRED_DONT_USE_KERBEROS));
	PyModule_AddObject(m, "MUST_USE_KERBEROS", PyInt_FromLong(CRED_MUST_USE_KERBEROS));

	PyModule_AddObject(m, "AUTO_KRB_FORWARDABLE",  PyInt_FromLong(CRED_AUTO_KRB_FORWARDABLE));
	PyModule_AddObject(m, "NO_KRB_FORWARDABLE",    PyInt_FromLong(CRED_NO_KRB_FORWARDABLE));
	PyModule_AddObject(m, "FORCE_KRB_FORWARDABLE", PyInt_FromLong(CRED_FORCE_KRB_FORWARDABLE));

	Py_INCREF(&PyCredentials);
	PyModule_AddObject(m, "Credentials", (PyObject *)&PyCredentials);
	Py_INCREF(&PyCredentialCacheContainer);
	PyModule_AddObject(m, "CredentialCacheContainer", (PyObject *)&PyCredentialCacheContainer);
}
