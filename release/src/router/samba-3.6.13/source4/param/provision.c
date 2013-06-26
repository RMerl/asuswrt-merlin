/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008-2009
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include <ldb.h>
#include <pyldb.h>
#include "includes.h"
#include "librpc/ndr/libndr.h"
#include "param/provision.h"
#include "param/secrets.h"
#include "lib/talloc/pytalloc.h"
#include "scripting/python/modules.h"
#include "param/pyparam.h"
#include "dynconfig/dynconfig.h"

static PyObject *provision_module(void)
{
	PyObject *name = PyString_FromString("samba.provision");
	if (name == NULL)
		return NULL;
	return PyImport_Import(name);
}

static PyObject *schema_module(void)
{
	PyObject *name = PyString_FromString("samba.schema");
	if (name == NULL)
		return NULL;
	return PyImport_Import(name);
}

static PyObject *ldb_module(void)
{
	PyObject *name = PyString_FromString("ldb");
	if (name == NULL)
		return NULL;
	return PyImport_Import(name);
}

static PyObject *PyLdb_FromLdbContext(struct ldb_context *ldb_ctx)
{
	PyLdbObject *ret;
	PyObject *ldb_mod = ldb_module();
	PyTypeObject *ldb_ctx_type;
	if (ldb_mod == NULL)
		return NULL;

	ldb_ctx_type = (PyTypeObject *)PyObject_GetAttrString(ldb_mod, "Ldb");

	ret = (PyLdbObject *)ldb_ctx_type->tp_alloc(ldb_ctx_type, 0);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	ret->mem_ctx = talloc_new(NULL);
	ret->ldb_ctx = talloc_reference(ret->mem_ctx, ldb_ctx);
	return (PyObject *)ret;
}

NTSTATUS provision_bare(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
			struct provision_settings *settings, 
			struct provision_result *result)
{
	const char *configfile;
	PyObject *provision_mod, *provision_dict, *provision_fn, *py_result, *parameters, *py_lp_ctx;
	
	DEBUG(0,("Provision for Become-DC test using python\n"));

	Py_Initialize();
	py_update_path(); /* Put the samba path at the start of sys.path */

	provision_mod = provision_module();

	if (provision_mod == NULL) {
		PyErr_Print();
		DEBUG(0, ("Unable to import provision Python module.\n"));
	      	return NT_STATUS_UNSUCCESSFUL;
	}

	provision_dict = PyModule_GetDict(provision_mod);

	if (provision_dict == NULL) {
		DEBUG(0, ("Unable to get dictionary for provision module\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	provision_fn = PyDict_GetItemString(provision_dict, "provision_become_dc");
	if (provision_fn == NULL) {
		PyErr_Print();
		DEBUG(0, ("Unable to get provision_become_dc function\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	DEBUG(0,("New Server in Site[%s]\n", 
		 settings->site_name));

	DEBUG(0,("DSA Instance [%s]\n"
		"\tinvocationId[%s]\n",
		settings->ntds_dn_str,
		settings->invocation_id == NULL?"None":GUID_string(mem_ctx, settings->invocation_id)));

	DEBUG(0,("Paths under targetdir[%s]\n",
		 settings->targetdir));
	parameters = PyDict_New();

	configfile = lpcfg_configfile(lp_ctx);
	if (configfile != NULL) {
		PyDict_SetItemString(parameters, "smbconf", 
				     PyString_FromString(configfile));
	}

	PyDict_SetItemString(parameters, "rootdn", 
						 PyString_FromString(settings->root_dn_str));
	if (settings->targetdir != NULL)
		PyDict_SetItemString(parameters, "targetdir", 
							 PyString_FromString(settings->targetdir));
	PyDict_SetItemString(parameters, "hostname", 
						 PyString_FromString(settings->netbios_name));
	PyDict_SetItemString(parameters, "domain", 
						 PyString_FromString(settings->domain));
	PyDict_SetItemString(parameters, "realm", 
						 PyString_FromString(settings->realm));
	if (settings->root_dn_str)
		PyDict_SetItemString(parameters, "rootdn", 
				     PyString_FromString(settings->root_dn_str));

	if (settings->domain_dn_str) 
		PyDict_SetItemString(parameters, "domaindn", 
				     PyString_FromString(settings->domain_dn_str));

	if (settings->schema_dn_str) 
		PyDict_SetItemString(parameters, "schemadn", 
				     PyString_FromString(settings->schema_dn_str));
	
	if (settings->config_dn_str) 
		PyDict_SetItemString(parameters, "configdn", 
				     PyString_FromString(settings->config_dn_str));
	
	if (settings->server_dn_str) 
		PyDict_SetItemString(parameters, "serverdn", 
				     PyString_FromString(settings->server_dn_str));
	
	if (settings->site_name) 
		PyDict_SetItemString(parameters, "sitename", 
				     PyString_FromString(settings->site_name));

	PyDict_SetItemString(parameters, "machinepass", 
			     PyString_FromString(settings->machine_password));

	
	PyDict_SetItemString(parameters, "debuglevel", PyInt_FromLong(DEBUGLEVEL));

	py_result = PyEval_CallObjectWithKeywords(provision_fn, NULL, parameters);

	Py_DECREF(parameters);

	if (py_result == NULL) {
		PyErr_Print();
		PyErr_Clear();
		return NT_STATUS_UNSUCCESSFUL;
	}

	result->domaindn = talloc_strdup(mem_ctx, PyString_AsString(PyObject_GetAttrString(py_result, "domaindn")));

	/* FIXME paths */
	py_lp_ctx = PyObject_GetAttrString(py_result, "lp");
	if (py_lp_ctx == NULL) {
		DEBUG(0, ("Missing 'lp' attribute"));
		return NT_STATUS_UNSUCCESSFUL;
	}
	result->lp_ctx = lpcfg_from_py_object(mem_ctx, py_lp_ctx);
	result->samdb = PyLdb_AsLdbContext(PyObject_GetAttrString(py_result, "samdb"));

	return NT_STATUS_OK;
}

static PyObject *py_dom_sid_FromSid(struct dom_sid *sid)
{
	PyObject *mod_security, *dom_sid_Type;

	mod_security = PyImport_ImportModule("samba.dcerpc.security");
	if (mod_security == NULL)
		return NULL;

	dom_sid_Type = PyObject_GetAttrString(mod_security, "dom_sid");
	if (dom_sid_Type == NULL)
		return NULL;

	return py_talloc_reference((PyTypeObject *)dom_sid_Type, sid);
}

NTSTATUS provision_store_self_join(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
				   struct tevent_context *event_ctx,
				   struct provision_store_self_join_settings *settings,
				   const char **error_string)
{
	int ret;
	PyObject *provision_mod, *provision_dict, *provision_fn, *py_result, *parameters, *py_sid;
	struct ldb_context *ldb;
	TALLOC_CTX *tmp_mem = talloc_new(mem_ctx);
	if (!tmp_mem) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Open the secrets database */
	ldb = secrets_db_connect(tmp_mem, lp_ctx);
	if (!ldb) {
		*error_string
			= talloc_asprintf(mem_ctx, 
					  "Could not open secrets database");
		talloc_free(tmp_mem);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	ret = ldb_transaction_start(ldb);

	if (ret != LDB_SUCCESS) {
		*error_string
			= talloc_asprintf(mem_ctx, 
					  "Could not start transaction on secrets database: %s", ldb_errstring(ldb));
		talloc_free(tmp_mem);
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	Py_Initialize();
	py_update_path(); /* Put the samba path at the start of sys.path */
	provision_mod = provision_module();

	if (provision_mod == NULL) {
		PyErr_Print();
		*error_string
			= talloc_asprintf(mem_ctx, "Unable to import provision Python module.");
		talloc_free(tmp_mem);
	      	return NT_STATUS_UNSUCCESSFUL;
	}

	provision_dict = PyModule_GetDict(provision_mod);

	if (provision_dict == NULL) {
		*error_string
			= talloc_asprintf(mem_ctx, "Unable to get dictionary for provision module");
		talloc_free(tmp_mem);
		return NT_STATUS_UNSUCCESSFUL;
	}

	provision_fn = PyDict_GetItemString(provision_dict, "secretsdb_self_join");
	if (provision_fn == NULL) {
		PyErr_Print();
		*error_string
			= talloc_asprintf(mem_ctx, "Unable to get provision_become_dc function");
		talloc_free(tmp_mem);
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	parameters = PyDict_New();

	PyDict_SetItemString(parameters, "secretsdb", 
			     PyLdb_FromLdbContext(ldb));
	PyDict_SetItemString(parameters, "domain", 
			     PyString_FromString(settings->domain_name));
	if (settings->realm != NULL) {
		PyDict_SetItemString(parameters, "realm",
				     PyString_FromString(settings->realm));
	}
	PyDict_SetItemString(parameters, "machinepass", 
			     PyString_FromString(settings->machine_password));
	PyDict_SetItemString(parameters, "netbiosname", 
			     PyString_FromString(settings->netbios_name));

	py_sid = py_dom_sid_FromSid(settings->domain_sid);
	if (py_sid == NULL) {
		Py_DECREF(parameters);
		goto failure;
	}

	PyDict_SetItemString(parameters, "domainsid", 
			     py_sid);

	PyDict_SetItemString(parameters, "secure_channel_type", 
		       PyInt_FromLong(settings->secure_channel_type));

	PyDict_SetItemString(parameters, "key_version_number", 
		       PyInt_FromLong(settings->key_version_number));

	py_result = PyEval_CallObjectWithKeywords(provision_fn, NULL, parameters);

	Py_DECREF(parameters);

	if (py_result == NULL) {
		goto failure;
	}

	ret = ldb_transaction_commit(ldb);
	if (ret != LDB_SUCCESS) {
		*error_string
			= talloc_asprintf(mem_ctx, 
					  "Could not commit transaction on secrets database: %s", ldb_errstring(ldb));
		talloc_free(tmp_mem);
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	talloc_free(tmp_mem);

	return NT_STATUS_OK;

failure:
	ldb_transaction_cancel(ldb);
	talloc_free(tmp_mem);

	PyErr_Print();
	PyErr_Clear();
	return NT_STATUS_UNSUCCESSFUL;
}


struct ldb_context *provision_get_schema(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
					 DATA_BLOB *override_prefixmap)
{
	PyObject *schema_mod, *schema_dict, *schema_fn, *py_result, *parameters;
	
	Py_Initialize();
	py_update_path(); /* Put the samba path at the start of sys.path */

	schema_mod = schema_module();

	if (schema_mod == NULL) {
		PyErr_Print();
		DEBUG(0, ("Unable to import schema Python module.\n"));
	      	return NULL;
	}

	schema_dict = PyModule_GetDict(schema_mod);

	if (schema_dict == NULL) {
		DEBUG(0, ("Unable to get dictionary for schema module\n"));
		return NULL;
	}

	schema_fn = PyDict_GetItemString(schema_dict, "ldb_with_schema");
	if (schema_fn == NULL) {
		PyErr_Print();
		DEBUG(0, ("Unable to get schema_get_ldb function\n"));
		return NULL;
	}
	
	parameters = PyDict_New();

	if (override_prefixmap) {
		PyDict_SetItemString(parameters, "override_prefixmap",
				     PyString_FromStringAndSize((const char *)override_prefixmap->data,
								override_prefixmap->length));
	}

	py_result = PyEval_CallObjectWithKeywords(schema_fn, NULL, parameters);

	Py_DECREF(parameters);

	if (py_result == NULL) {
		PyErr_Print();
		PyErr_Clear();
		return NULL;
	}

	return PyLdb_AsLdbContext(PyObject_GetAttrString(py_result, "ldb"));
}
