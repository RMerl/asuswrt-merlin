/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2010
   Copyright (C) Matthias Dieter Wallnöfer          2009
   
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
#include <ldb.h>
#include <pyldb.h>
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "librpc/ndr/libndr.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"
#include "librpc/rpc/pyrpc_util.h"
#include "lib/policy/policy.h"

void initdsdb(void);

/* There's no Py_ssize_t in 2.4, apparently */
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5
typedef int Py_ssize_t;
typedef inquiry lenfunc;
typedef intargfunc ssizeargfunc;
#endif

/* FIXME: These should be in a header file somewhere, once we finish moving
 * away from SWIG .. */
#define PyErr_LDB_OR_RAISE(py_ldb, ldb) \
/*	if (!PyLdb_Check(py_ldb)) { \
		PyErr_SetString(py_ldb_get_exception(), "Ldb connection object required"); \
		return NULL; \
	} */\
	ldb = PyLdb_AsLdbContext(py_ldb);

static PyObject *py_ldb_get_exception(void)
{
	PyObject *mod = PyImport_ImportModule("ldb");
	if (mod == NULL)
		return NULL;

	return PyObject_GetAttrString(mod, "LdbError");
}

static void PyErr_SetLdbError(PyObject *error, int ret, struct ldb_context *ldb_ctx)
{
	if (ret == LDB_ERR_PYTHON_EXCEPTION)
		return; /* Python exception should already be set, just keep that */

	PyErr_SetObject(error, 
			Py_BuildValue(discard_const_p(char, "(i,s)"), ret,
			ldb_ctx == NULL?ldb_strerror(ret):ldb_errstring(ldb_ctx)));
}

static PyObject *py_samdb_server_site_name(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *result;
	struct ldb_context *ldb;
	const char *site;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	site = samdb_server_site_name(ldb, mem_ctx);
	if (site == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to find server site");
		talloc_free(mem_ctx);
		return NULL;
	}

	result = PyString_FromString(site);
	talloc_free(mem_ctx);
	return result;
}

static PyObject *py_dsdb_convert_schema_to_openldap(PyObject *self,
													PyObject *args)
{
	char *target_str, *mapping;
	PyObject *py_ldb;
	struct ldb_context *ldb;
	PyObject *ret;
	char *retstr;

	if (!PyArg_ParseTuple(args, "Oss", &py_ldb, &target_str, &mapping))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	retstr = dsdb_convert_schema_to_openldap(ldb, target_str, mapping);
	if (retstr == NULL) {
		PyErr_SetString(PyExc_RuntimeError,
						"dsdb_convert_schema_to_openldap failed");
		return NULL;
	} 

	ret = PyString_FromString(retstr);
	talloc_free(retstr);
	return ret;
}

static PyObject *py_samdb_set_domain_sid(PyLdbObject *self, PyObject *args)
{ 
	PyObject *py_ldb, *py_sid;
	struct ldb_context *ldb;
	struct dom_sid *sid;
	bool ret;

	if (!PyArg_ParseTuple(args, "OO", &py_ldb, &py_sid))
		return NULL;
	
	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	sid = dom_sid_parse_talloc(NULL, PyString_AsString(py_sid));
	if (sid == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	ret = samdb_set_domain_sid(ldb, sid);
	talloc_free(sid);
	if (!ret) {
		PyErr_SetString(PyExc_RuntimeError, "set_domain_sid failed");
		return NULL;
	} 
	Py_RETURN_NONE;
}

static PyObject *py_samdb_set_ntds_settings_dn(PyLdbObject *self, PyObject *args)
{ 
	PyObject *py_ldb, *py_ntds_settings_dn;
	struct ldb_context *ldb;
	struct ldb_dn *ntds_settings_dn;
	TALLOC_CTX *tmp_ctx;
	bool ret;

	if (!PyArg_ParseTuple(args, "OO", &py_ldb, &py_ntds_settings_dn))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	tmp_ctx = talloc_new(NULL);
	if (tmp_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	if (!PyObject_AsDn(tmp_ctx, py_ntds_settings_dn, ldb, &ntds_settings_dn)) {
		/* exception thrown by "PyObject_AsDn" */
		talloc_free(tmp_ctx);
		return NULL;
	}

	ret = samdb_set_ntds_settings_dn(ldb, ntds_settings_dn);
	talloc_free(tmp_ctx);
	if (!ret) {
		PyErr_SetString(PyExc_RuntimeError, "set_ntds_settings_dn failed");
		return NULL;
	} 
	Py_RETURN_NONE;
}

static PyObject *py_samdb_get_domain_sid(PyLdbObject *self, PyObject *args)
{ 
	PyObject *py_ldb;
	struct ldb_context *ldb;
	const struct dom_sid *sid;
	PyObject *ret;
	char *retstr;

	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	sid = samdb_domain_sid(ldb);
	if (!sid) {
		PyErr_SetString(PyExc_RuntimeError, "samdb_domain_sid failed");
		return NULL;
	}

	retstr = dom_sid_string(NULL, sid);
	if (retstr == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret = PyString_FromString(retstr);
	talloc_free(retstr);
	return ret;
}

static PyObject *py_samdb_ntds_invocation_id(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *result;
	struct ldb_context *ldb;
	const struct GUID *guid;
	char *retstr;

	if (!PyArg_ParseTuple(args, "O", &py_ldb)) {
		return NULL;
	}

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	guid = samdb_ntds_invocation_id(ldb);
	if (guid == NULL) {
		PyErr_SetString(PyExc_RuntimeError,
						"Failed to find NTDS invocation ID");
		return NULL;
	}

	retstr = GUID_string(NULL, guid);
	if (retstr == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	result = PyString_FromString(retstr);
	talloc_free(retstr);
	return result;
}

static PyObject *py_dsdb_get_oid_from_attid(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	struct ldb_context *ldb;
	uint32_t attid;
	struct dsdb_schema *schema;
	const char *oid;
	PyObject *ret;
	WERROR status;
	TALLOC_CTX *mem_ctx;

	if (!PyArg_ParseTuple(args, "Oi", &py_ldb, &attid))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) {
		PyErr_NoMemory();
		return NULL;
	}

	schema = dsdb_get_schema(ldb, mem_ctx);
	if (!schema) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to find a schema from ldb \n");
		talloc_free(mem_ctx);
		return NULL;
	}
	
	status = dsdb_schema_pfm_oid_from_attid(schema->prefixmap, attid,
	                                        mem_ctx, &oid);
	if (!W_ERROR_IS_OK(status)) {
		PyErr_SetWERROR(status);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = PyString_FromString(oid);

	talloc_free(mem_ctx);

	return ret;
}


static PyObject *py_dsdb_get_attid_from_lDAPDisplayName(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *is_schema_nc;
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
	const char *ldap_display_name;
	bool schema_nc = false;
	const struct dsdb_attribute *a;
	uint32_t attid;

	if (!PyArg_ParseTuple(args, "OsO", &py_ldb, &ldap_display_name, &is_schema_nc))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	if (is_schema_nc) {
		if (!PyBool_Check(is_schema_nc)) {
			PyErr_SetString(PyExc_TypeError, "Expected boolean is_schema_nc");
			return NULL;
		}
		if (is_schema_nc == Py_True) {
			schema_nc = true;
		}
	}

	schema = dsdb_get_schema(ldb, NULL);

	if (!schema) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to find a schema from ldb");
		return NULL;
	}

	a = dsdb_attribute_by_lDAPDisplayName(schema, ldap_display_name);
	if (a == NULL) {
		PyErr_Format(PyExc_RuntimeError, "Failed to find attribute '%s'", ldap_display_name);
		return NULL;
	}

	attid = dsdb_attribute_get_attid(a, schema_nc);

	return PyLong_FromUnsignedLong(attid);
}

/*
  convert a python string to a DRSUAPI drsuapi_DsReplicaAttribute attribute
 */
static PyObject *py_dsdb_DsReplicaAttribute(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *el_list, *ret;
	struct ldb_context *ldb;
	char *ldap_display_name;
	const struct dsdb_attribute *a;
	struct dsdb_schema *schema;
	struct dsdb_syntax_ctx syntax_ctx;
	struct ldb_message_element *el;
	struct drsuapi_DsReplicaAttribute *attr;
	TALLOC_CTX *tmp_ctx;
	WERROR werr;
	Py_ssize_t i;

	if (!PyArg_ParseTuple(args, "OsO", &py_ldb, &ldap_display_name, &el_list)) {
		return NULL;
	}

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	if (!PyList_Check(el_list)) {
		PyErr_Format(PyExc_TypeError, "ldif_elements must be a list");
		return NULL;
	}

	schema = dsdb_get_schema(ldb, NULL);
	if (!schema) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to find a schema from ldb");
		return NULL;
	}

	a = dsdb_attribute_by_lDAPDisplayName(schema, ldap_display_name);
	if (a == NULL) {
		PyErr_Format(PyExc_RuntimeError, "Failed to find attribute '%s'", ldap_display_name);
		return NULL;
	}

	dsdb_syntax_ctx_init(&syntax_ctx, ldb, schema);
	syntax_ctx.is_schema_nc = false;

	tmp_ctx = talloc_new(ldb);
	if (tmp_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	el = talloc_zero(tmp_ctx, struct ldb_message_element);
	if (el == NULL) {
		PyErr_NoMemory();
		talloc_free(tmp_ctx);
		return NULL;
	}

	el->name = ldap_display_name;
	el->num_values = PyList_Size(el_list);

	el->values = talloc_array(el, struct ldb_val, el->num_values);
	if (el->values == NULL) {
		PyErr_NoMemory();
		talloc_free(tmp_ctx);
		return NULL;
	}

	for (i = 0; i < el->num_values; i++) {
		PyObject *item = PyList_GetItem(el_list, i);
		if (!PyString_Check(item)) {
			PyErr_Format(PyExc_TypeError, "ldif_elements should be strings");
			return NULL;
		}
		el->values[i].data = (uint8_t *)PyString_AsString(item);
		el->values[i].length = PyString_Size(item);
	}

	attr = talloc_zero(tmp_ctx, struct drsuapi_DsReplicaAttribute);
	if (attr == NULL) {
		PyErr_NoMemory();
		talloc_free(tmp_ctx);
		return NULL;
	}

	werr = a->syntax->ldb_to_drsuapi(&syntax_ctx, a, el, attr, attr);
	PyErr_WERROR_IS_ERR_RAISE(werr);

	ret = py_return_ndr_struct("samba.dcerpc.drsuapi", "DsReplicaAttribute", attr, attr);

	talloc_free(tmp_ctx);

	return ret;
}

static PyObject *py_dsdb_set_ntds_invocation_id(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *py_guid;
	bool ret;
	struct GUID guid;
	struct ldb_context *ldb;
	if (!PyArg_ParseTuple(args, "OO", &py_ldb, &py_guid))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);
	GUID_from_string(PyString_AsString(py_guid), &guid);

	ret = samdb_set_ntds_invocation_id(ldb, &guid);
	if (!ret) {
		PyErr_SetString(PyExc_RuntimeError, "set_ntds_invocation_id failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *py_samdb_ntds_objectGUID(PyObject *self, PyObject *args)
{
	PyObject *py_ldb, *result;
	struct ldb_context *ldb;
	const struct GUID *guid;
	char *retstr;

	if (!PyArg_ParseTuple(args, "O", &py_ldb)) {
		return NULL;
	}

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	guid = samdb_ntds_objectGUID(ldb);
	if (guid == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to find NTDS GUID");
		return NULL;
	}

	retstr = GUID_string(NULL, guid);
	if (retstr == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	result = PyString_FromString(retstr);
	talloc_free(retstr);
	return result;
}

static PyObject *py_dsdb_set_global_schema(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	struct ldb_context *ldb;
	int ret;
	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	ret = dsdb_set_global_schema(ldb);
	PyErr_LDB_ERROR_IS_ERR_RAISE(py_ldb_get_exception(), ret, ldb);

	Py_RETURN_NONE;
}

static PyObject *py_dsdb_load_partition_usn(PyObject *self, PyObject *args)
{
	PyObject *py_dn, *py_ldb, *result;
	struct ldb_dn *dn;
	uint64_t highest_uSN, urgent_uSN;
	struct ldb_context *ldb;
	TALLOC_CTX *mem_ctx;
	int ret;

	if (!PyArg_ParseTuple(args, "OO", &py_ldb, &py_dn)) {
		return NULL;
	}

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
	   PyErr_NoMemory();
	   return NULL;
	}

	if (!PyObject_AsDn(mem_ctx, py_dn, ldb, &dn)) {
	   talloc_free(mem_ctx);
	   return NULL;
	}

	ret = dsdb_load_partition_usn(ldb, dn, &highest_uSN, &urgent_uSN);
	if (ret != LDB_SUCCESS) {
	   PyErr_Format(PyExc_RuntimeError,
			"Failed to load partition [%s] uSN - %s",
			ldb_dn_get_linearized(dn),
			ldb_errstring(ldb));
	   talloc_free(mem_ctx);
	   return NULL;
	}

	talloc_free(mem_ctx);

	result = PyDict_New();

	PyDict_SetItemString(result, "uSNHighest", PyInt_FromLong((uint64_t)highest_uSN));
	PyDict_SetItemString(result, "uSNUrgent", PyInt_FromLong((uint64_t)urgent_uSN));


	return result;
}

static PyObject *py_dsdb_set_am_rodc(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	bool ret;
	struct ldb_context *ldb;
	int py_val;

	if (!PyArg_ParseTuple(args, "Oi", &py_ldb, &py_val))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);
	ret = samdb_set_am_rodc(ldb, (bool)py_val);
	if (!ret) {
		PyErr_SetString(PyExc_RuntimeError, "set_am_rodc failed");
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *py_dsdb_set_schema_from_ldif(PyObject *self, PyObject *args)
{
	WERROR result;
	char *pf, *df;
	PyObject *py_ldb;
	struct ldb_context *ldb;

	if (!PyArg_ParseTuple(args, "Oss", &py_ldb, &pf, &df))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	result = dsdb_set_schema_from_ldif(ldb, pf, df);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE;
}

static PyObject *py_dsdb_set_schema_from_ldb(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	struct ldb_context *ldb;
	PyObject *py_from_ldb;
	struct ldb_context *from_ldb;
	struct dsdb_schema *schema;
	int ret;
	if (!PyArg_ParseTuple(args, "OO", &py_ldb, &py_from_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	PyErr_LDB_OR_RAISE(py_from_ldb, from_ldb);

	schema = dsdb_get_schema(from_ldb, NULL);
	if (!schema) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to set find a schema on 'from' ldb!\n");
		return NULL;
	}

	ret = dsdb_reference_schema(ldb, schema, true);
	PyErr_LDB_ERROR_IS_ERR_RAISE(py_ldb_get_exception(), ret, ldb);

	Py_RETURN_NONE;
}

static PyObject *py_dsdb_write_prefixes_from_schema_to_ldb(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	struct ldb_context *ldb;
	WERROR result;
	struct dsdb_schema *schema;

	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	schema = dsdb_get_schema(ldb, NULL);
	if (!schema) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to set find a schema on ldb!\n");
		return NULL;
	}

	result = dsdb_write_prefixes_from_schema_to_ldb(NULL, ldb, schema);
	PyErr_WERROR_IS_ERR_RAISE(result);

	Py_RETURN_NONE;
}


static PyObject *py_dsdb_get_partitions_dn(PyObject *self, PyObject *args)
{
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	PyObject *py_ldb, *ret;
	PyObject *mod;

	mod = PyImport_ImportModule("ldb");

	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	dn = samdb_partitions_dn(ldb, NULL);
	if (dn == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret = PyLdbDn_FromDn(dn);
	talloc_free(dn);
	return ret;
}


/*
  call into samdb_rodc()
 */
static PyObject *py_dsdb_am_rodc(PyObject *self, PyObject *args)
{
	PyObject *py_ldb;
	struct ldb_context *ldb;
	int ret;
	bool am_rodc;

	if (!PyArg_ParseTuple(args, "O", &py_ldb))
		return NULL;

	PyErr_LDB_OR_RAISE(py_ldb, ldb);

	ret = samdb_rodc(ldb, &am_rodc);
	if (ret != LDB_SUCCESS) {
		PyErr_SetString(PyExc_RuntimeError, ldb_errstring(ldb));
		return NULL;
	}

	return PyBool_FromLong(am_rodc);
}


static PyMethodDef py_dsdb_methods[] = {
	{ "_samdb_server_site_name", (PyCFunction)py_samdb_server_site_name,
		METH_VARARGS, "Get the server site name as a string"},
	{ "_dsdb_convert_schema_to_openldap",
		(PyCFunction)py_dsdb_convert_schema_to_openldap, METH_VARARGS, 
		"dsdb_convert_schema_to_openldap(ldb, target_str, mapping) -> str\n"
		"Create an OpenLDAP schema from a schema." },
	{ "_samdb_set_domain_sid", (PyCFunction)py_samdb_set_domain_sid,
		METH_VARARGS,
		"samdb_set_domain_sid(samdb, sid)\n"
		"Set SID of domain to use." },
	{ "_samdb_get_domain_sid", (PyCFunction)py_samdb_get_domain_sid,
		METH_VARARGS,
		"samdb_get_domain_sid(samdb)\n"
		"Get SID of domain in use." },
	{ "_samdb_ntds_invocation_id", (PyCFunction)py_samdb_ntds_invocation_id,
		METH_VARARGS, "get the NTDS invocation ID GUID as a string"},
	{ "_samdb_set_ntds_settings_dn", (PyCFunction)py_samdb_set_ntds_settings_dn,
		METH_VARARGS,
		"samdb_set_ntds_settings_dn(samdb, ntds_settings_dn)\n"
		"Set NTDS Settings DN for this LDB (allows it to be set before the DB fully exists)." },
	{ "_dsdb_get_oid_from_attid", (PyCFunction)py_dsdb_get_oid_from_attid,
		METH_VARARGS, NULL },
	{ "_dsdb_get_attid_from_lDAPDisplayName", (PyCFunction)py_dsdb_get_attid_from_lDAPDisplayName,
		METH_VARARGS, NULL },
	{ "_dsdb_set_ntds_invocation_id",
		(PyCFunction)py_dsdb_set_ntds_invocation_id, METH_VARARGS,
		NULL },
	{ "_samdb_ntds_objectGUID", (PyCFunction)py_samdb_ntds_objectGUID,
		METH_VARARGS, "get the NTDS objectGUID as a string"},
	{ "_dsdb_set_global_schema", (PyCFunction)py_dsdb_set_global_schema,
		METH_VARARGS, NULL },
	{ "_dsdb_load_partition_usn", (PyCFunction)py_dsdb_load_partition_usn,
		METH_VARARGS,
		"get uSNHighest and uSNUrgent from the partition @REPLCHANGED"},
	{ "_dsdb_set_am_rodc",
		(PyCFunction)py_dsdb_set_am_rodc, METH_VARARGS,
		NULL },
	{ "_am_rodc",
		(PyCFunction)py_dsdb_am_rodc, METH_VARARGS,
		NULL },
	{ "_dsdb_set_schema_from_ldif", (PyCFunction)py_dsdb_set_schema_from_ldif, METH_VARARGS,
		NULL },
	{ "_dsdb_set_schema_from_ldb", (PyCFunction)py_dsdb_set_schema_from_ldb, METH_VARARGS,
		NULL },
	{ "_dsdb_write_prefixes_from_schema_to_ldb", (PyCFunction)py_dsdb_write_prefixes_from_schema_to_ldb, METH_VARARGS,
		NULL },
	{ "_dsdb_get_partitions_dn", (PyCFunction)py_dsdb_get_partitions_dn, METH_VARARGS, NULL },
	{ "_dsdb_DsReplicaAttribute", (PyCFunction)py_dsdb_DsReplicaAttribute, METH_VARARGS, NULL },
	{ NULL }
};

void initdsdb(void)
{
	PyObject *m;

	m = Py_InitModule3("dsdb", py_dsdb_methods, 
			   "Python bindings for the directory service databases.");
	if (m == NULL)
		return;

#define ADD_DSDB_FLAG(val)  PyModule_AddObject(m, #val, PyInt_FromLong(val))

	/* "userAccountControl" flags */
	ADD_DSDB_FLAG(UF_NORMAL_ACCOUNT);
	ADD_DSDB_FLAG(UF_TEMP_DUPLICATE_ACCOUNT);
	ADD_DSDB_FLAG(UF_SERVER_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_WORKSTATION_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_INTERDOMAIN_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_PASSWD_NOTREQD);
	ADD_DSDB_FLAG(UF_ACCOUNTDISABLE);

	ADD_DSDB_FLAG(UF_SCRIPT);
	ADD_DSDB_FLAG(UF_ACCOUNTDISABLE);
	ADD_DSDB_FLAG(UF_00000004);
	ADD_DSDB_FLAG(UF_HOMEDIR_REQUIRED);
	ADD_DSDB_FLAG(UF_LOCKOUT);
	ADD_DSDB_FLAG(UF_PASSWD_NOTREQD);
	ADD_DSDB_FLAG(UF_PASSWD_CANT_CHANGE);
	ADD_DSDB_FLAG(UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED);
	ADD_DSDB_FLAG(UF_TEMP_DUPLICATE_ACCOUNT);
	ADD_DSDB_FLAG(UF_NORMAL_ACCOUNT);
	ADD_DSDB_FLAG(UF_00000400);
	ADD_DSDB_FLAG(UF_INTERDOMAIN_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_WORKSTATION_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_SERVER_TRUST_ACCOUNT);
	ADD_DSDB_FLAG(UF_00004000);
	ADD_DSDB_FLAG(UF_00008000);
	ADD_DSDB_FLAG(UF_DONT_EXPIRE_PASSWD);
	ADD_DSDB_FLAG(UF_MNS_LOGON_ACCOUNT);
	ADD_DSDB_FLAG(UF_SMARTCARD_REQUIRED);
	ADD_DSDB_FLAG(UF_TRUSTED_FOR_DELEGATION);
	ADD_DSDB_FLAG(UF_NOT_DELEGATED);
	ADD_DSDB_FLAG(UF_USE_DES_KEY_ONLY);
	ADD_DSDB_FLAG(UF_DONT_REQUIRE_PREAUTH);
	ADD_DSDB_FLAG(UF_PASSWORD_EXPIRED);
	ADD_DSDB_FLAG(UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION);
	ADD_DSDB_FLAG(UF_NO_AUTH_DATA_REQUIRED);
	ADD_DSDB_FLAG(UF_PARTIAL_SECRETS_ACCOUNT);

	/* groupType flags */
	ADD_DSDB_FLAG(GTYPE_SECURITY_BUILTIN_LOCAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_SECURITY_GLOBAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_SECURITY_DOMAIN_LOCAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_SECURITY_UNIVERSAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_DISTRIBUTION_GLOBAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP);
	ADD_DSDB_FLAG(GTYPE_DISTRIBUTION_UNIVERSAL_GROUP);

	/* "sAMAccountType" flags */
	ADD_DSDB_FLAG(ATYPE_NORMAL_ACCOUNT);
	ADD_DSDB_FLAG(ATYPE_WORKSTATION_TRUST);
	ADD_DSDB_FLAG(ATYPE_INTERDOMAIN_TRUST);
	ADD_DSDB_FLAG(ATYPE_SECURITY_GLOBAL_GROUP);
	ADD_DSDB_FLAG(ATYPE_SECURITY_LOCAL_GROUP);
	ADD_DSDB_FLAG(ATYPE_SECURITY_UNIVERSAL_GROUP);
	ADD_DSDB_FLAG(ATYPE_DISTRIBUTION_GLOBAL_GROUP);
	ADD_DSDB_FLAG(ATYPE_DISTRIBUTION_LOCAL_GROUP);
	ADD_DSDB_FLAG(ATYPE_DISTRIBUTION_UNIVERSAL_GROUP);

	/* "domainFunctionality", "forestFunctionality" flags in the rootDSE */
	ADD_DSDB_FLAG(DS_DOMAIN_FUNCTION_2000);
	ADD_DSDB_FLAG(DS_DOMAIN_FUNCTION_2003_MIXED);
	ADD_DSDB_FLAG(DS_DOMAIN_FUNCTION_2003);
	ADD_DSDB_FLAG(DS_DOMAIN_FUNCTION_2008);
	ADD_DSDB_FLAG(DS_DOMAIN_FUNCTION_2008_R2);

	/* "systemFlags" */
	ADD_DSDB_FLAG(SYSTEM_FLAG_CR_NTDS_NC);
	ADD_DSDB_FLAG(SYSTEM_FLAG_CR_NTDS_DOMAIN);
	ADD_DSDB_FLAG(SYSTEM_FLAG_CR_NTDS_NOT_GC_REPLICATED);
	ADD_DSDB_FLAG(SYSTEM_FLAG_SCHEMA_BASE_OBJECT);
	ADD_DSDB_FLAG(SYSTEM_FLAG_ATTR_IS_RDN);
	ADD_DSDB_FLAG(SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE);
	ADD_DSDB_FLAG(SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE);
	ADD_DSDB_FLAG(SYSTEM_FLAG_DOMAIN_DISALLOW_RENAME);
	ADD_DSDB_FLAG(SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE);
	ADD_DSDB_FLAG(SYSTEM_FLAG_CONFIG_ALLOW_MOVE);
	ADD_DSDB_FLAG(SYSTEM_FLAG_CONFIG_ALLOW_RENAME);
	ADD_DSDB_FLAG(SYSTEM_FLAG_DISALLOW_DELETE);

	/* Kerberos encryption type constants */
	ADD_DSDB_FLAG(ENC_ALL_TYPES);
	ADD_DSDB_FLAG(ENC_CRC32);
	ADD_DSDB_FLAG(ENC_RSA_MD5);
	ADD_DSDB_FLAG(ENC_RC4_HMAC_MD5);
	ADD_DSDB_FLAG(ENC_HMAC_SHA1_96_AES128);
	ADD_DSDB_FLAG(ENC_HMAC_SHA1_96_AES256);

	ADD_DSDB_FLAG(SEARCH_FLAG_ATTINDEX);
	ADD_DSDB_FLAG(SEARCH_FLAG_PDNTATTINDEX);
	ADD_DSDB_FLAG(SEARCH_FLAG_ANR);
	ADD_DSDB_FLAG(SEARCH_FLAG_PRESERVEONDELETE);
	ADD_DSDB_FLAG(SEARCH_FLAG_COPY);
	ADD_DSDB_FLAG(SEARCH_FLAG_TUPLEINDEX);
	ADD_DSDB_FLAG(SEARCH_FLAG_SUBTREEATTRINDEX);
	ADD_DSDB_FLAG(SEARCH_FLAG_CONFIDENTIAL);
	ADD_DSDB_FLAG(SEARCH_FLAG_NEVERVALUEAUDIT);
	ADD_DSDB_FLAG(SEARCH_FLAG_RODC_ATTRIBUTE);

	ADD_DSDB_FLAG(DS_FLAG_ATTR_NOT_REPLICATED);
	ADD_DSDB_FLAG(DS_FLAG_ATTR_REQ_PARTIAL_SET_MEMBER);
	ADD_DSDB_FLAG(DS_FLAG_ATTR_IS_CONSTRUCTED);

	ADD_DSDB_FLAG(DS_NTDSDSA_OPT_IS_GC);
	ADD_DSDB_FLAG(DS_NTDSDSA_OPT_DISABLE_INBOUND_REPL);
	ADD_DSDB_FLAG(DS_NTDSDSA_OPT_DISABLE_OUTBOUND_REPL);
	ADD_DSDB_FLAG(DS_NTDSDSA_OPT_DISABLE_NTDSCONN_XLATE);
	ADD_DSDB_FLAG(DS_NTDSDSA_OPT_DISABLE_SPN_REGISTRATION);

	ADD_DSDB_FLAG(NTDSCONN_KCC_GC_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_RING_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_MINIMIZE_HOPS_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_STALE_SERVERS_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_OSCILLATING_CONNECTION_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_INTERSITE_GC_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_INTERSITE_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_SERVER_FAILOVER_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_SITE_FAILOVER_TOPOLOGY);
	ADD_DSDB_FLAG(NTDSCONN_KCC_REDUNDANT_SERVER_TOPOLOGY);

	/* GPO policy flags */
	ADD_DSDB_FLAG(GPLINK_OPT_DISABLE);
	ADD_DSDB_FLAG(GPLINK_OPT_ENFORCE);
	ADD_DSDB_FLAG(GPO_FLAG_USER_DISABLE);
	ADD_DSDB_FLAG(GPO_FLAG_MACHINE_DISABLE);
	ADD_DSDB_FLAG(GPO_INHERIT);
	ADD_DSDB_FLAG(GPO_BLOCK_INHERITANCE);
}
