/*
   Unix SMB/CIFS implementation.

   Python interface to ldb.

   Copyright (C) 2007-2008 Jelmer Vernooij <jelmer@samba.org>

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

#ifndef _PYLDB_H_
#define _PYLDB_H_

#include <talloc.h>

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_context *ldb_ctx;
} PyLdbObject;

#define PyLdb_AsLdbContext(pyobj) ((PyLdbObject *)pyobj)->ldb_ctx
#define PyLdb_Check(ob) PyObject_TypeCheck(ob, &PyLdb)

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *dn;
} PyLdbDnObject;

PyObject *PyLdbDn_FromDn(struct ldb_dn *);
bool PyObject_AsDn(TALLOC_CTX *mem_ctx, PyObject *object, struct ldb_context *ldb_ctx, struct ldb_dn **dn);
#define PyLdbDn_AsDn(pyobj) ((PyLdbDnObject *)pyobj)->dn
#define PyLdbDn_Check(ob) PyObject_TypeCheck(ob, &PyLdbDn)

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_message *msg;
} PyLdbMessageObject;
#define PyLdbMessage_Check(ob) PyObject_TypeCheck(ob, &PyLdbMessage)
#define PyLdbMessage_AsMessage(pyobj) ((PyLdbMessageObject *)pyobj)->msg

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_module *mod;
} PyLdbModuleObject;
#define PyLdbModule_AsModule(pyobj) ((PyLdbModuleObject *)pyobj)->mod

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_message_element *el;
} PyLdbMessageElementObject;
#define PyLdbMessageElement_AsMessageElement(pyobj) ((PyLdbMessageElementObject *)pyobj)->el
#define PyLdbMessageElement_Check(ob) PyObject_TypeCheck(ob, &PyLdbMessageElement)

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_parse_tree *tree;
} PyLdbTreeObject;
#define PyLdbTree_AsTree(pyobj) ((PyLdbTreeObject *)pyobj)->tree

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	PyObject *msgs;
	PyObject *referals;
	PyObject *controls;
} PyLdbResultObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX *mem_ctx;
	struct ldb_control *data;
} PyLdbControlObject;

#define PyErr_LDB_ERROR_IS_ERR_RAISE(err,ret,ldb) \
	if (ret != LDB_SUCCESS) { \
		PyErr_SetLdbError(err, ret, ldb); \
		return NULL; \
	}

/* Picked out of thin air. To do this properly, we should probably have some part of the 
 * errors in LDB be allocated to bindings ? */
#define LDB_ERR_PYTHON_EXCEPTION	142

#endif /* _PYLDB_H_ */
