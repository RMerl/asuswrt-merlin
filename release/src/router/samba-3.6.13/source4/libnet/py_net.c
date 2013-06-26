/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008-2010
   Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2009

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
#include "libnet.h"
#include "auth/credentials/pycredentials.h"
#include "libcli/security/security.h"
#include "lib/events/events.h"
#include "param/pyparam.h"
#include "auth/gensec/gensec.h"
#include "librpc/rpc/pyrpc_util.h"
#include "libcli/finddc.h"
#include "libcli/resolve/resolve.h"

void initnet(void);

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct libnet_context *libnet_ctx;
	struct tevent_context *ev;
} py_net_Object;

static PyObject *py_net_join(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	struct libnet_Join r;
	NTSTATUS status;
	PyObject *result;
	TALLOC_CTX *mem_ctx;
	const char *kwnames[] = { "domain_name", "netbios_name", "join_type", "level", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ssii:Join", discard_const_p(char *, kwnames), 
					 &r.in.domain_name, &r.in.netbios_name, 
					 &r.in.join_type, &r.in.level))
		return NULL;

	mem_ctx = talloc_new(self->mem_ctx);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_Join(self->libnet_ctx, mem_ctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetString(PyExc_RuntimeError, r.out.error_string?r.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	result = Py_BuildValue("sss", r.out.join_password,
			       dom_sid_string(mem_ctx, r.out.domain_sid),
			       r.out.domain_name);

	talloc_free(mem_ctx);

	return result;
}

static const char py_net_join_doc[] = "join(domain_name, netbios_name, join_type, level) -> (join_password, domain_sid, domain_name)\n\n" \
"Join the domain with the specified name.";

static PyObject *py_net_set_password(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	union libnet_SetPassword r;
	NTSTATUS status;
	PyObject *py_creds;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev;
	const char *kwnames[] = { "account_name", "domain_name", "newpassword", "credentials", NULL };

	r.generic.level = LIBNET_SET_PASSWORD_GENERIC;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sssO:set_password", discard_const_p(char *, kwnames),
					 &r.generic.in.account_name, &r.generic.in.domain_name,
					 &r.generic.in.newpassword, &py_creds)) {
		return NULL;
	}

	/* FIXME: we really need to get a context from the caller or we may end
	 * up with 2 event contexts */
	ev = s4_event_context_init(NULL);

	mem_ctx = talloc_new(ev);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_SetPassword(self->libnet_ctx, mem_ctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetString(PyExc_RuntimeError,
				r.generic.out.error_string?r.generic.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);

	Py_RETURN_NONE;
}

static const char py_net_set_password_doc[] = "set_password(account_name, domain_name, newpassword) -> True\n\n" \
"Set password for a user. You must supply credential with enough rights to do this.\n\n" \
"Sample usage is:\n" \
"net.set_password(account_name=<account_name>,\n" \
"                domain_name=domain_name,\n" \
"                newpassword=new_pass)\n";


static PyObject *py_net_export_keytab(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	struct libnet_export_keytab r;
	TALLOC_CTX *mem_ctx;
	const char *kwnames[] = { "keytab", NULL };
	NTSTATUS status;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s:export_keytab", discard_const_p(char *, kwnames),
					 &r.in.keytab_name)) {
		return NULL;
	}

	mem_ctx = talloc_new(self->mem_ctx);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_export_keytab(self->libnet_ctx, mem_ctx, &r);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetString(PyExc_RuntimeError,
				r.out.error_string?r.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);

	Py_RETURN_NONE;
}

static const char py_net_export_keytab_doc[] = "export_keytab(keytab, name)\n\n"
"Export the DC keytab to a keytab file.";

static PyObject *py_net_time(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "server_name", NULL };
	union libnet_RemoteTOD r;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	char timestr[64];
	PyObject *ret;
	struct tm *tm;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s",
		discard_const_p(char *, kwnames), &r.generic.in.server_name))
		return NULL;

	r.generic.level			= LIBNET_REMOTE_TOD_GENERIC;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_RemoteTOD(self->libnet_ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetString(PyExc_RuntimeError,
				r.generic.out.error_string?r.generic.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	ZERO_STRUCT(timestr);
	tm = localtime(&r.generic.out.time);
	strftime(timestr, sizeof(timestr)-1, "%c %Z",tm);
	
	ret = PyString_FromString(timestr);

	talloc_free(mem_ctx);

	return ret;
}

static const char py_net_time_doc[] = "time(server_name) -> timestr\n"
"Retrieve the remote time on a server";

static PyObject *py_net_user_create(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "username", NULL };
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_CreateUser r;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", discard_const_p(char *, kwnames), 
									 &r.in.user_name))
		return NULL;

	r.in.domain_name = cli_credentials_get_domain(self->libnet_ctx->cred);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_CreateUser(self->libnet_ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetString(PyExc_RuntimeError, r.out.error_string?r.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);
	
	Py_RETURN_NONE;
}

static const char py_net_create_user_doc[] = "create_user(username)\n"
"Create a new user.";

static PyObject *py_net_user_delete(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "username", NULL };
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_DeleteUser r;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", discard_const_p(char *, kwnames), 
									 &r.in.user_name))
		return NULL;

	r.in.domain_name = cli_credentials_get_domain(self->libnet_ctx->cred);

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_DeleteUser(self->libnet_ctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetString(PyExc_RuntimeError, r.out.error_string?r.out.error_string:nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);
	
	Py_RETURN_NONE;
}

static const char py_net_delete_user_doc[] = "delete_user(username)\n"
"Delete a user.";

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

static PyObject *py_net_vampire(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "domain", "target_dir", NULL };
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	PyObject *ret;
	struct libnet_Vampire r;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|z", discard_const_p(char *, kwnames),
	                                 &r.in.domain_name, &r.in.targetdir)) {
		return NULL;
	}

	r.in.netbios_name  = lpcfg_netbios_name(self->libnet_ctx->lp_ctx);
	r.out.error_string = NULL;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = libnet_Vampire(self->libnet_ctx, mem_ctx, &r);

	if (!NT_STATUS_IS_OK(status)) {
		PyErr_SetString(PyExc_RuntimeError,
		                r.out.error_string ? r.out.error_string : nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = Py_BuildValue("(sO)", r.out.domain_name, py_dom_sid_FromSid(r.out.domain_sid));

	talloc_free(mem_ctx);

	return ret;
}

struct replicate_state {
	void *vampire_state;
	dcerpc_InterfaceObject *drs_pipe;
	struct libnet_BecomeDC_StoreChunk chunk;
	DATA_BLOB gensec_skey;
	struct libnet_BecomeDC_Partition partition;
	struct libnet_BecomeDC_Forest forest;
	struct libnet_BecomeDC_DestDSA dest_dsa;
};

/*
  setup for replicate_chunk() calls
 */
static PyObject *py_net_replicate_init(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "samdb", "lp", "drspipe", NULL };
	PyObject *py_ldb, *py_lp, *py_drspipe;
	struct ldb_context *samdb;
	struct loadparm_context *lp;
	struct replicate_state *s;
	NTSTATUS status;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO",
					 discard_const_p(char *, kwnames),
	                                 &py_ldb, &py_lp, &py_drspipe)) {
		return NULL;
	}

	s = talloc_zero(NULL, struct replicate_state);
	if (!s) return NULL;

	lp = lpcfg_from_py_object(s, py_lp);
	if (lp == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected lp object");
		talloc_free(s);
		return NULL;
	}

	samdb = PyLdb_AsLdbContext(py_ldb);
	if (samdb == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected ldb object");
		talloc_free(s);
		return NULL;
	}

	s->drs_pipe = (dcerpc_InterfaceObject *)(py_drspipe);

	s->vampire_state = libnet_vampire_replicate_init(s, samdb, lp);
	if (s->vampire_state == NULL) {
		PyErr_SetString(PyExc_TypeError, "Failed to initialise vampire_state");
		talloc_free(s);
		return NULL;
	}

	status = gensec_session_key(s->drs_pipe->pipe->conn->security_state.generic_state,
				    &s->gensec_skey);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_Format(PyExc_RuntimeError, "Unable to get session key from drspipe: %s",
			     nt_errstr(status));
		talloc_free(s);
		return NULL;
	}

	s->forest.dns_name = lpcfg_dnsdomain(lp);

	s->chunk.gensec_skey = &s->gensec_skey;
	s->chunk.partition = &s->partition;
	s->chunk.forest = &s->forest;
	s->chunk.dest_dsa = &s->dest_dsa;

	return PyCObject_FromTallocPtr(s);
}


/*
  process one replication chunk
 */
static PyObject *py_net_replicate_chunk(py_net_Object *self, PyObject *args, PyObject *kwargs)
{
	const char *kwnames[] = { "state", "level", "ctr", "schema", NULL };
	PyObject *py_state, *py_ctr, *py_schema;
	struct replicate_state *s;
	unsigned level;
	NTSTATUS (*chunk_handler)(void *private_data, const struct libnet_BecomeDC_StoreChunk *c);
	NTSTATUS status;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OIO|O",
					 discard_const_p(char *, kwnames),
	                                 &py_state, &level, &py_ctr, &py_schema)) {
		return NULL;
	}

	s = talloc_get_type(PyCObject_AsVoidPtr(py_state), struct replicate_state);
	if (!s) {
		PyErr_SetString(PyExc_TypeError, "Expected replication_state");
		return NULL;
	}

	switch (level) {
	case 1:
		if (!py_check_dcerpc_type(py_ctr, "samba.dcerpc.drsuapi", "DsGetNCChangesCtr1")) {
			return NULL;
		}
		s->chunk.ctr1                         = py_talloc_get_ptr(py_ctr);
		s->partition.nc                       = *s->chunk.ctr1->naming_context;
		s->partition.more_data                = s->chunk.ctr1->more_data;
		s->partition.source_dsa_guid          = s->chunk.ctr1->source_dsa_guid;
		s->partition.source_dsa_invocation_id = s->chunk.ctr1->source_dsa_invocation_id;
		s->partition.highwatermark            = s->chunk.ctr1->new_highwatermark;
		break;
	case 6:
		if (!py_check_dcerpc_type(py_ctr, "samba.dcerpc.drsuapi", "DsGetNCChangesCtr6")) {
			return NULL;
		}
		s->chunk.ctr6                         = py_talloc_get_ptr(py_ctr);
		s->partition.nc                       = *s->chunk.ctr6->naming_context;
		s->partition.more_data                = s->chunk.ctr6->more_data;
		s->partition.source_dsa_guid          = s->chunk.ctr6->source_dsa_guid;
		s->partition.source_dsa_invocation_id = s->chunk.ctr6->source_dsa_invocation_id;
		s->partition.highwatermark            = s->chunk.ctr6->new_highwatermark;
		break;
	default:
		PyErr_Format(PyExc_TypeError, "Bad level %u in replicate_chunk", level);
		return NULL;
	}

	chunk_handler = libnet_vampire_cb_store_chunk;
	if (py_schema) {
		if (!PyBool_Check(py_schema)) {
			PyErr_SetString(PyExc_TypeError, "Expected boolean schema");
			return NULL;
		}
		if (py_schema == Py_True) {
			chunk_handler = libnet_vampire_cb_schema_chunk;
		}
	}

	s->chunk.ctr_level = level;

	status = chunk_handler(s->vampire_state, &s->chunk);
	if (!NT_STATUS_IS_OK(status)) {
		PyErr_Format(PyExc_TypeError, "Failed to process chunk: %s", nt_errstr(status));
		return NULL;
	}

	Py_RETURN_NONE;
}


/*
  find a DC given a domain name and server type
 */
static PyObject *py_net_finddc(py_net_Object *self, PyObject *args)
{
	const char *domain_name;
	unsigned server_type;
	NTSTATUS status;
	struct finddcs *io;
	TALLOC_CTX *mem_ctx;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "sI", &domain_name, &server_type)) {
		return NULL;
	}

	mem_ctx = talloc_new(self->mem_ctx);

	io = talloc_zero(mem_ctx, struct finddcs);
	io->in.domain_name = domain_name;
	io->in.minimum_dc_flags = server_type;

	status = finddcs_cldap(io, io,
			       lpcfg_resolve_context(self->libnet_ctx->lp_ctx), self->ev);
	if (NT_STATUS_IS_ERR(status)) {
		PyErr_SetString(PyExc_RuntimeError, nt_errstr(status));
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = py_return_ndr_struct("samba.dcerpc.nbt", "NETLOGON_SAM_LOGON_RESPONSE_EX",
				   io, &io->out.netlogon.data.nt5_ex);
	talloc_free(mem_ctx);

	return ret;
}


static const char py_net_vampire_doc[] = "vampire(domain, target_dir=None)\n"
					 "Vampire a domain.";

static const char py_net_replicate_init_doc[] = "replicate_init(samdb, lp, drspipe)\n"
					 "Setup for replicate_chunk calls.";

static const char py_net_replicate_chunk_doc[] = "replicate_chunk(state, level, ctr, schema)\n"
					 "Process replication for one chunk";

static const char py_net_finddc_doc[] = "finddc(domain, server_type)\n"
					 "find a DC with the specified server_type bits. Return the DNS name";

static PyMethodDef net_obj_methods[] = {
	{"join", (PyCFunction)py_net_join, METH_VARARGS|METH_KEYWORDS, py_net_join_doc},
	{"set_password", (PyCFunction)py_net_set_password, METH_VARARGS|METH_KEYWORDS, py_net_set_password_doc},
	{"export_keytab", (PyCFunction)py_net_export_keytab, METH_VARARGS|METH_KEYWORDS, py_net_export_keytab_doc},
	{"time", (PyCFunction)py_net_time, METH_VARARGS|METH_KEYWORDS, py_net_time_doc},
	{"create_user", (PyCFunction)py_net_user_create, METH_VARARGS|METH_KEYWORDS, py_net_create_user_doc},
	{"delete_user", (PyCFunction)py_net_user_delete, METH_VARARGS|METH_KEYWORDS, py_net_delete_user_doc},
	{"vampire", (PyCFunction)py_net_vampire, METH_VARARGS|METH_KEYWORDS, py_net_vampire_doc},
	{"replicate_init", (PyCFunction)py_net_replicate_init, METH_VARARGS|METH_KEYWORDS, py_net_replicate_init_doc},
	{"replicate_chunk", (PyCFunction)py_net_replicate_chunk, METH_VARARGS|METH_KEYWORDS, py_net_replicate_chunk_doc},
	{"finddc", (PyCFunction)py_net_finddc, METH_VARARGS, py_net_finddc_doc},
	{ NULL }
};

static void py_net_dealloc(py_net_Object *self)
{
	talloc_free(self->mem_ctx);
	PyObject_Del(self);
}

static PyObject *net_obj_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	PyObject *py_creds, *py_lp = Py_None;
	const char *kwnames[] = { "creds", "lp", "server", NULL };
	py_net_Object *ret;
	struct loadparm_context *lp;
	const char *server_address = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Oz",
					 discard_const_p(char *, kwnames), &py_creds, &py_lp,
					 &server_address))
		return NULL;

	ret = PyObject_New(py_net_Object, type);
	if (ret == NULL) {
		return NULL;
	}

	/* FIXME: we really need to get a context from the caller or we may end
	 * up with 2 event contexts */
	ret->ev = s4_event_context_init(NULL);
	ret->mem_ctx = talloc_new(ret->ev);

	lp = lpcfg_from_py_object(ret->mem_ctx, py_lp);
	if (lp == NULL) {
		Py_DECREF(ret);
		return NULL;
	}

	ret->libnet_ctx = libnet_context_init(ret->ev, lp);
	if (ret->libnet_ctx == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to initialize net");
		Py_DECREF(ret);
		return NULL;
	}

	ret->libnet_ctx->server_address = server_address;

	ret->libnet_ctx->cred = cli_credentials_from_py_object(py_creds);
	if (ret->libnet_ctx->cred == NULL) {
		PyErr_SetString(PyExc_TypeError, "Expected credentials object");
		Py_DECREF(ret);
		return NULL;
	}

	return (PyObject *)ret;
}


PyTypeObject py_net_Type = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "net.Net",
	.tp_basicsize = sizeof(py_net_Object),
	.tp_dealloc = (destructor)py_net_dealloc,
	.tp_methods = net_obj_methods,
	.tp_new = net_obj_new,
};

void initnet(void)
{
	PyObject *m;

	if (PyType_Ready(&py_net_Type) < 0)
		return;

	m = Py_InitModule3("net", NULL, NULL);
	if (m == NULL)
		return;

	Py_INCREF(&py_net_Type);
	PyModule_AddObject(m, "Net", (PyObject *)&py_net_Type);
	PyModule_AddObject(m, "LIBNET_JOINDOMAIN_AUTOMATIC", PyInt_FromLong(LIBNET_JOINDOMAIN_AUTOMATIC));
	PyModule_AddObject(m, "LIBNET_JOINDOMAIN_SPECIFIED", PyInt_FromLong(LIBNET_JOINDOMAIN_SPECIFIED));
	PyModule_AddObject(m, "LIBNET_JOIN_AUTOMATIC", PyInt_FromLong(LIBNET_JOIN_AUTOMATIC));
	PyModule_AddObject(m, "LIBNET_JOIN_SPECIFIED", PyInt_FromLong(LIBNET_JOIN_SPECIFIED));
}
