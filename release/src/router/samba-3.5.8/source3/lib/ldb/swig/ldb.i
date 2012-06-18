/* 
   Unix SMB/CIFS implementation.

   Swig interface to ldb.

   Copyright (C) 2005,2006 Tim Potter <tpot@samba.org>
   Copyright (C) 2006 Simo Sorce <idra@samba.org>

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

%module ldb

%{

/* Some typedefs to help swig along */

typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

/* Include headers */

#include "lib/ldb/include/ldb.h"
#include "lib/talloc/talloc.h"

%}

%include "carrays.i"
%include "exception.i"

/*
 * Constants
 */

#define LDB_SUCCESS				0
#define LDB_ERR_OPERATIONS_ERROR		1
#define LDB_ERR_PROTOCOL_ERROR			2
#define LDB_ERR_TIME_LIMIT_EXCEEDED		3
#define LDB_ERR_SIZE_LIMIT_EXCEEDED		4
#define LDB_ERR_COMPARE_FALSE			5
#define LDB_ERR_COMPARE_TRUE			6
#define LDB_ERR_AUTH_METHOD_NOT_SUPPORTED	7
#define LDB_ERR_STRONG_AUTH_REQUIRED		8
/* 9 RESERVED */
#define LDB_ERR_REFERRAL			10
#define LDB_ERR_ADMIN_LIMIT_EXCEEDED		11
#define LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION	12
#define LDB_ERR_CONFIDENTIALITY_REQUIRED	13
#define LDB_ERR_SASL_BIND_IN_PROGRESS		14
#define LDB_ERR_NO_SUCH_ATTRIBUTE		16
#define LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE	17
#define LDB_ERR_INAPPROPRIATE_MATCHING		18
#define LDB_ERR_CONSTRAINT_VIOLATION		19
#define LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS	20
#define LDB_ERR_INVALID_ATTRIBUTE_SYNTAX	21
/* 22-31 unused */
#define LDB_ERR_NO_SUCH_OBJECT			32
#define LDB_ERR_ALIAS_PROBLEM			33
#define LDB_ERR_INVALID_DN_SYNTAX		34
/* 35 RESERVED */
#define LDB_ERR_ALIAS_DEREFERENCING_PROBLEM	36
/* 37-47 unused */
#define LDB_ERR_INAPPROPRIATE_AUTHENTICATION	48
#define LDB_ERR_INVALID_CREDENTIALS		49
#define LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS	50
#define LDB_ERR_BUSY				51
#define LDB_ERR_UNAVAILABLE			52
#define LDB_ERR_UNWILLING_TO_PERFORM		53
#define LDB_ERR_LOOP_DETECT			54
/* 55-63 unused */
#define LDB_ERR_NAMING_VIOLATION		64
#define LDB_ERR_OBJECT_CLASS_VIOLATION		65
#define LDB_ERR_NOT_ALLOWED_ON_NON_LEAF		66
#define LDB_ERR_NOT_ALLOWED_ON_RDN		67
#define LDB_ERR_ENTRY_ALREADY_EXISTS		68
#define LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED	69
/* 70 RESERVED FOR CLDAP */
#define LDB_ERR_AFFECTS_MULTIPLE_DSAS		71
/* 72-79 unused */
#define LDB_ERR_OTHER				80

enum ldb_scope {LDB_SCOPE_DEFAULT=-1, 
		LDB_SCOPE_BASE=0, 
		LDB_SCOPE_ONELEVEL=1,
		LDB_SCOPE_SUBTREE=2};

/* 
 * Wrap struct ldb_context
 */

/* The ldb functions will crash if a NULL ldb context is passed so
   catch this before it happens. */

%typemap(check) struct ldb_context* {
	if ($1 == NULL)
		SWIG_exception(SWIG_ValueError, 
			"ldb context must be non-NULL");
}

/* 
 * Wrap a small bit of talloc
 */

/* Use talloc_init() to create a parameter to pass to ldb_init().  Don't
   forget to free it using talloc_free() afterwards. */

TALLOC_CTX *talloc_init(char *name);
int talloc_free(TALLOC_CTX *ptr);

/*
 * Wrap struct ldb_val
 */

%typemap(in) struct ldb_val *INPUT (struct ldb_val temp) {
	$1 = &temp;
	if (!PyString_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "string arg expected");
		return NULL;
	}
	$1->length = PyString_Size($input);
	$1->data = PyString_AsString($input);
}

%typemap(out) struct ldb_val {
	$result = PyString_FromStringAndSize($1.data, $1.length);
}

/*
 * Wrap struct ldb_result
 */

%typemap(in, numinputs=0) struct ldb_result **OUT (struct ldb_result *temp_ldb_result) {
	$1 = &temp_ldb_result;
}

%typemap(argout) struct ldb_result ** {
	resultobj = SWIG_NewPointerObj(*$1, SWIGTYPE_p_ldb_result, 0);
}	

%types(struct ldb_result *);

/*
 * Wrap struct ldb_message_element
 */

%array_functions(struct ldb_val, ldb_val_array);

struct ldb_message_element {
	unsigned int flags;
	const char *name;
	unsigned int num_values;
	struct ldb_val *values;
};

/*
 * Wrap struct ldb_message
 */

%array_functions(struct ldb_message_element, ldb_message_element_array);

struct ldb_message {
	struct ldb_dn *dn;
	unsigned int num_elements;
	struct ldb_message_element *elements;
	void *private_data;
};

/*
 * Wrap struct ldb_result
 */

%array_functions(struct ldb_message *, ldb_message_ptr_array);

struct ldb_result {
	unsigned int count;
	struct ldb_message **msgs;
	char **refs;
	struct ldb_control **controls;
};

/*
 * Wrap ldb functions 
 */

/* Initialisation */

int ldb_global_init(void);
struct ldb_context *ldb_init(TALLOC_CTX *mem_ctx);

/* Error handling */

const char *ldb_errstring(struct ldb_context *ldb);
const char *ldb_strerror(int ldb_err);

/* Top-level ldb operations */

int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[]);

int ldb_search(struct ldb_context *ldb, const struct ldb_dn *base, enum ldb_scope scope, const char *expression, const char * const *attrs, struct ldb_result **OUT);

int ldb_delete(struct ldb_context *ldb, const struct ldb_dn *dn);

int ldb_rename(struct ldb_context *ldb, const struct ldb_dn *olddn, const struct ldb_dn *newdn);

int ldb_add(struct ldb_context *ldb, const struct ldb_message *message);

/* Ldb message operations */

struct ldb_message *ldb_msg_new(void *mem_ctx);

struct ldb_message_element *ldb_msg_find_element(const struct ldb_message *msg, const char *attr_name);

int ldb_msg_add_value(struct ldb_message *msg, const char *attr_name, const struct ldb_val *INPUT);

void ldb_msg_remove_attr(struct ldb_message *msg, const char *attr);

int ldb_msg_sanity_check(struct ldb_message *msg);

/* DN operations */

struct ldb_dn *ldb_dn_explode(void *mem_ctx, const char *dn);

char *ldb_dn_linearize(void *mem_ctx, const struct ldb_dn *dn);
