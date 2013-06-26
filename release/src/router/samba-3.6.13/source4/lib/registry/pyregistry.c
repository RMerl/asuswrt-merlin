/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   Copyright (C) Wilco Baan Hofman <wilco@baanhofman.nl> 2010
   
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
#include "libcli/util/pyerrors.h"
#include "lib/registry/registry.h"
#include "lib/talloc/pytalloc.h"
#include "lib/events/events.h"
#include "auth/credentials/pycredentials.h"
#include "param/pyparam.h"

extern PyTypeObject PyRegistryKey;
extern PyTypeObject PyRegistry;
extern PyTypeObject PyHiveKey;

/*#define PyRegistryKey_AsRegistryKey(obj) py_talloc_get_type(obj, struct registry_key)*/
#define PyRegistry_AsRegistryContext(obj) ((struct registry_context *)py_talloc_get_ptr(obj))
#define PyHiveKey_AsHiveKey(obj) ((struct hive_key*)py_talloc_get_ptr(obj))


static PyObject *py_get_predefined_key_by_name(PyObject *self, PyObject *args)
{
	char *name;
	WERROR result;
	struct registry_context *ctx = PyRegistry_AsRegistryContext(self);
	struct registry_key *key;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	result = reg_get_predefined_key_by_name(ctx, name, &key);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyRegistryKey, key);
}

static PyObject *py_key_del_abs(PyObject *self, PyObject *args)
{
	char *path;
	WERROR result;
	struct registry_context *ctx = PyRegistry_AsRegistryContext(self);

	if (!PyArg_ParseTuple(args, "s", &path))
		return NULL;

	result = reg_key_del_abs(ctx, path);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE;
}

static PyObject *py_get_predefined_key(PyObject *self, PyObject *args)
{
	uint32_t hkey;
	struct registry_context *ctx = PyRegistry_AsRegistryContext(self);
	WERROR result;
	struct registry_key *key;

	if (!PyArg_ParseTuple(args, "I", &hkey))
		return NULL;

	result = reg_get_predefined_key(ctx, hkey, &key);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyRegistryKey, key);
}

static PyObject *py_diff_apply(PyObject *self, PyObject *args)
{
	char *filename;
	WERROR result;
	struct registry_context *ctx = PyRegistry_AsRegistryContext(self);
	if (!PyArg_ParseTuple(args, "s", &filename))
		return NULL;

	result = reg_diff_apply(ctx, filename);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE; 
}

static PyObject *py_mount_hive(PyObject *self, PyObject *args)
{
	struct registry_context *ctx = PyRegistry_AsRegistryContext(self);
	uint32_t hkey;
	PyObject *py_hivekey, *py_elements = Py_None;
	const char **elements;
	WERROR result;

	if (!PyArg_ParseTuple(args, "OI|O", &py_hivekey, &hkey, &py_elements))
		return NULL;

	if (!PyList_Check(py_elements) && py_elements != Py_None) {
		PyErr_SetString(PyExc_TypeError, "Expected list of elements");
		return NULL;
	}

	if (py_elements == Py_None) {
		elements = NULL;
	} else {
		int i;
		elements = talloc_array(NULL, const char *, PyList_Size(py_elements));
		for (i = 0; i < PyList_Size(py_elements); i++)
			elements[i] = PyString_AsString(PyList_GetItem(py_elements, i));
	}

	SMB_ASSERT(ctx != NULL);

	result = reg_mount_hive(ctx, PyHiveKey_AsHiveKey(py_hivekey), hkey, elements);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE;
}

static PyObject *registry_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	WERROR result;
	struct registry_context *ctx;
	result = reg_open_local(NULL, &ctx);
	PyErr_WERROR_IS_ERR_RAISE(result);
	return py_talloc_steal(&PyRegistry, ctx);
}

static PyMethodDef registry_methods[] = {
	{ "get_predefined_key_by_name", py_get_predefined_key_by_name, METH_VARARGS, 
		"S.get_predefined_key_by_name(name) -> key\n"
		"Find a predefined key by name" },
	{ "key_del_abs", py_key_del_abs, METH_VARARGS, "S.key_del_abs(name) -> None\n"
                "Delete a key by absolute path." },
	{ "get_predefined_key", py_get_predefined_key, METH_VARARGS, "S.get_predefined_key(hkey_id) -> key\n"
		"Find a predefined key by id" },
	{ "diff_apply", py_diff_apply, METH_VARARGS, "S.diff_apply(filename) -> None\n"
        	"Apply the diff from the specified file" },
	{ "mount_hive", py_mount_hive, METH_VARARGS, "S.mount_hive(key, key_id, elements=None) -> None\n"
		"Mount the specified key at the specified path." },
	{ NULL }
};

PyTypeObject PyRegistry = {
	.tp_name = "Registry",
	.tp_methods = registry_methods,
	.tp_new = registry_new,
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_hive_key_del(PyObject *self, PyObject *args)
{
	char *name;
	struct hive_key *key = PyHiveKey_AsHiveKey(self);
	WERROR result;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	result = hive_key_del(NULL, key, name);

	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE; 
}

static PyObject *py_hive_key_flush(PyObject *self)
{
	WERROR result;
	struct hive_key *key = PyHiveKey_AsHiveKey(self);

	result = hive_key_flush(key);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE;
}

static PyObject *py_hive_key_del_value(PyObject *self, PyObject *args)
{
	char *name;
	WERROR result;
	struct hive_key *key = PyHiveKey_AsHiveKey(self);

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	result = hive_key_del_value(NULL, key, name);

	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE; 
}

static PyObject *py_hive_key_set_value(PyObject *self, PyObject *args)
{
	char *name;
	uint32_t type;
	DATA_BLOB value;
	WERROR result;
	struct hive_key *key = PyHiveKey_AsHiveKey(self);

	if (!PyArg_ParseTuple(args, "siz#", &name, &type, &value.data, &value.length))
		return NULL;

	if (value.data != NULL)
		result = hive_key_set_value(key, name, type, value);
	else
		result = hive_key_del_value(NULL, key, name);

	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE; 
}

static PyMethodDef hive_key_methods[] = {
	{ "del", py_hive_key_del, METH_VARARGS, "S.del(name) -> None\n"
		"Delete a subkey" },
	{ "flush", (PyCFunction)py_hive_key_flush, METH_NOARGS, "S.flush() -> None\n"
                "Flush this key to disk" },
	{ "del_value", py_hive_key_del_value, METH_VARARGS, "S.del_value(name) -> None\n"
                 "Delete a value" },
	{ "set_value", py_hive_key_set_value, METH_VARARGS, "S.set_value(name, type, data) -> None\n"
                 "Set a value" },
	{ NULL }
};

static PyObject *hive_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
	Py_RETURN_NONE;
}

static PyObject *py_open_hive(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "location", "lp_ctx", "session_info", "credentials", NULL };
	WERROR result;
	struct loadparm_context *lp_ctx;
	PyObject *py_lp_ctx, *py_session_info, *py_credentials;
	struct auth_session_info *session_info;
	struct cli_credentials *credentials;
	char *location;
	struct hive_key *hive_key;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OOO",
					 discard_const_p(char *, kwnames),
	                                 &location,
					 &py_lp_ctx, &py_session_info,
					 &py_credentials))
		return NULL;

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
	session_info = NULL;

	result = reg_open_hive(NULL, location, session_info, credentials,
	                       tevent_context_init(NULL),
	                       lp_ctx, &hive_key);
	talloc_free(mem_ctx);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyHiveKey, hive_key);
}

PyTypeObject PyHiveKey = {
	.tp_name = "HiveKey",
	.tp_methods = hive_key_methods,
	.tp_new = hive_new,
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

PyTypeObject PyRegistryKey = {
	.tp_name = "RegistryKey",
	.tp_basicsize = sizeof(py_talloc_Object),
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyObject *py_open_samba(PyObject *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "lp_ctx", "session_info", NULL };
	struct registry_context *reg_ctx;
	WERROR result;
	struct loadparm_context *lp_ctx;
	PyObject *py_lp_ctx, *py_session_info, *py_credentials;
	struct auth_session_info *session_info;
	struct cli_credentials *credentials;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO",
					 discard_const_p(char *, kwnames),
					 &py_lp_ctx, &py_session_info,
					 &py_credentials))
		return NULL;

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

	session_info = NULL; /* FIXME */

	result = reg_open_samba(NULL, &reg_ctx, NULL, 
				lp_ctx, session_info, credentials);
	talloc_free(mem_ctx);
	if (!W_ERROR_IS_OK(result)) {
		PyErr_SetWERROR(result);
		return NULL;
	}

	return py_talloc_steal(&PyRegistry, reg_ctx);
}

static PyObject *py_open_directory(PyObject *self, PyObject *args)
{
	char *location;
	WERROR result;
	struct hive_key *key;

	if (!PyArg_ParseTuple(args, "s", &location))
		return NULL;

	result = reg_open_directory(NULL, location, &key);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyHiveKey, key);
}

static PyObject *py_create_directory(PyObject *self, PyObject *args)
{
	char *location;
	WERROR result;
	struct hive_key *key;

	if (!PyArg_ParseTuple(args, "s", &location))
		return NULL;

	result = reg_create_directory(NULL, location, &key);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyHiveKey, key);
}

static PyObject *py_open_ldb_file(PyObject *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "location", "session_info", "credentials", "lp_ctx", NULL };
	PyObject *py_session_info = Py_None, *py_credentials = Py_None, *py_lp_ctx = Py_None;
	WERROR result;
	char *location;
	struct loadparm_context *lp_ctx;
	struct cli_credentials *credentials;
	struct hive_key *key;
	struct auth_session_info *session_info;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OOO", 
					 discard_const_p(char *, kwnames),
					 &location, &py_session_info,
					 &py_credentials, &py_lp_ctx))
		return NULL;

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

	session_info = NULL; /* FIXME */

	result = reg_open_ldb_file(NULL, location, session_info, credentials,
				   s4_event_context_init(NULL), lp_ctx, &key);
	talloc_free(mem_ctx);
	PyErr_WERROR_IS_ERR_RAISE(result);

	return py_talloc_steal(&PyHiveKey, key);
}

static PyObject *py_str_regtype(PyObject *self, PyObject *args)
{
	int regtype;

	if (!PyArg_ParseTuple(args, "i", &regtype))
		return NULL;
	
	return PyString_FromString(str_regtype(regtype));
}

static PyObject *py_get_predef_name(PyObject *self, PyObject *args)
{
	uint32_t hkey;
	const char *str;

	if (!PyArg_ParseTuple(args, "I", &hkey))
		return NULL;

	str = reg_get_predef_name(hkey);
	if (str == NULL)
		Py_RETURN_NONE;
	return PyString_FromString(str);
}

static PyMethodDef py_registry_methods[] = {
	{ "open_samba", (PyCFunction)py_open_samba, METH_VARARGS|METH_KEYWORDS, "open_samba() -> reg" },
	{ "open_directory", py_open_directory, METH_VARARGS, "open_dir(location) -> key" },
	{ "create_directory", py_create_directory, METH_VARARGS, "create_dir(location) -> key" },
	{ "open_ldb", (PyCFunction)py_open_ldb_file, METH_VARARGS|METH_KEYWORDS, "open_ldb(location, session_info=None, credentials=None, loadparm_context=None) -> key" },
	{ "open_hive", (PyCFunction)py_open_hive, METH_VARARGS|METH_KEYWORDS, "open_hive(location, session_info=None, credentials=None, loadparm_context=None) -> key" },
	{ "str_regtype", py_str_regtype, METH_VARARGS, "str_regtype(int) -> str" },
	{ "get_predef_name", py_get_predef_name, METH_VARARGS, "get_predef_name(hkey) -> str" },
	{ NULL }
};

void initregistry(void)
{
	PyObject *m;
	PyTypeObject *talloc_type = PyTalloc_GetObjectType();

	if (talloc_type == NULL)
		return;

	PyHiveKey.tp_base = talloc_type;
	PyRegistry.tp_base = talloc_type;
	PyRegistryKey.tp_base = talloc_type;

	if (PyType_Ready(&PyHiveKey) < 0)
		return;

	if (PyType_Ready(&PyRegistry) < 0)
		return;

	if (PyType_Ready(&PyRegistryKey) < 0)
		return;

	m = Py_InitModule3("registry", py_registry_methods, "Registry");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "HKEY_CLASSES_ROOT", PyInt_FromLong(HKEY_CLASSES_ROOT));
	PyModule_AddObject(m, "HKEY_CURRENT_USER", PyInt_FromLong(HKEY_CURRENT_USER));
	PyModule_AddObject(m, "HKEY_LOCAL_MACHINE", PyInt_FromLong(HKEY_LOCAL_MACHINE));
	PyModule_AddObject(m, "HKEY_USERS", PyInt_FromLong(HKEY_USERS));
	PyModule_AddObject(m, "HKEY_PERFORMANCE_DATA", PyInt_FromLong(HKEY_PERFORMANCE_DATA));
	PyModule_AddObject(m, "HKEY_CURRENT_CONFIG", PyInt_FromLong(HKEY_CURRENT_CONFIG));
	PyModule_AddObject(m, "HKEY_DYN_DATA", PyInt_FromLong(HKEY_DYN_DATA));
	PyModule_AddObject(m, "HKEY_PERFORMANCE_TEXT", PyInt_FromLong(HKEY_PERFORMANCE_TEXT));
	PyModule_AddObject(m, "HKEY_PERFORMANCE_NLSTEXT", PyInt_FromLong(HKEY_PERFORMANCE_NLSTEXT));

	Py_INCREF(&PyRegistry);
	PyModule_AddObject(m, "Registry", (PyObject *)&PyRegistry);

	Py_INCREF(&PyHiveKey);
	PyModule_AddObject(m, "HiveKey", (PyObject *)&PyHiveKey);

	Py_INCREF(&PyRegistryKey);
	PyModule_AddObject(m, "RegistryKey", (PyObject *)&PyRegistryKey);
}
