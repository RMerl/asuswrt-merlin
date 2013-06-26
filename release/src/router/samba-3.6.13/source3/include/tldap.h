/*
   Unix SMB/CIFS implementation.
   Infrastructure for async ldap client requests
   Copyright (C) Volker Lendecke 2009

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

#ifndef __TLDAP_H__
#define __TLDAP_H__

#include <talloc.h>
#include <tevent.h>

struct tldap_context;
struct tldap_message;

struct tldap_control {
	const char *oid;
	DATA_BLOB value;
	bool critical;
};

struct tldap_attribute {
	char *name;
	int num_values;
	DATA_BLOB *values;
};

struct tldap_mod {
	int mod_op;
	char *attribute;
	int num_values;
	DATA_BLOB *values;
};

bool tevent_req_is_ldap_error(struct tevent_req *req, int *perr);

struct tldap_context *tldap_context_create(TALLOC_CTX *mem_ctx, int fd);
bool tldap_connection_ok(struct tldap_context *ld);
bool tldap_context_setattr(struct tldap_context *ld,
			   const char *name, const void *pptr);
void *tldap_context_getattr(struct tldap_context *ld, const char *name);

struct tevent_req *tldap_sasl_bind_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tldap_context *ld,
					const char *dn,
					const char *mechanism,
					DATA_BLOB *creds,
					struct tldap_control *sctrls,
					int num_sctrls,
					struct tldap_control *cctrls,
					int num_cctrls);
int tldap_sasl_bind_recv(struct tevent_req *req);
int tldap_sasl_bind(struct tldap_context *ldap,
		    const char *dn,
		    const char *mechanism,
		    DATA_BLOB *creds,
		    struct tldap_control *sctrls,
		    int num_sctrls,
		    struct tldap_control *cctrls,
		    int num_ctrls);

struct tevent_req *tldap_simple_bind_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tldap_context *ldap,
					  const char *dn,
					  const char *passwd);
int tldap_simple_bind_recv(struct tevent_req *req);
int tldap_simple_bind(struct tldap_context *ldap, const char *dn,
		      const char *passwd);

struct tevent_req *tldap_search_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *base, int scope,
				     const char *filter,
				     const char **attrs,
				     int num_attrs,
				     int attrsonly,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls,
				     int timelimit,
				     int sizelimit,
				     int deref);
int tldap_search_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
		      struct tldap_message **pmsg);
int tldap_search(struct tldap_context *ld,
		 const char *base, int scope, const char *filter,
		 const char **attrs, int num_attrs, int attrsonly,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls,
		 int timelimit, int sizelimit, int deref,
		 TALLOC_CTX *mem_ctx, struct tldap_message ***entries,
		 struct tldap_message ***refs);
bool tldap_entry_dn(struct tldap_message *msg, char **dn);
bool tldap_entry_attributes(struct tldap_message *msg,
			    struct tldap_attribute **attributes,
			    int *num_attributes);

struct tevent_req *tldap_add_send(TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  struct tldap_context *ld,
				  const char *dn,
				  struct tldap_mod *attributes,
				  int num_attributes,
				  struct tldap_control *sctrls,
				  int num_sctrls,
				  struct tldap_control *cctrls,
				  int num_cctrls);
int tldap_add_recv(struct tevent_req *req);
int tldap_add(struct tldap_context *ld, const char *dn,
	      struct tldap_mod *attributes, int num_attributes,
	      struct tldap_control *sctrls, int num_sctrls,
	      struct tldap_control *cctrls, int num_cctrls);

struct tevent_req *tldap_modify_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *dn,
				     struct tldap_mod *mods, int num_mods,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls);
int tldap_modify_recv(struct tevent_req *req);
int tldap_modify(struct tldap_context *ld, const char *dn,
		 struct tldap_mod *mods, int num_mods,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls);

struct tevent_req *tldap_delete_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct tldap_context *ld,
				     const char *dn,
				     struct tldap_control *sctrls,
				     int num_sctrls,
				     struct tldap_control *cctrls,
				     int num_cctrls);
int tldap_delete_recv(struct tevent_req *req);
int tldap_delete(struct tldap_context *ld, const char *dn,
		 struct tldap_control *sctrls, int num_sctrls,
		 struct tldap_control *cctrls, int num_cctrls);

int tldap_msg_id(const struct tldap_message *msg);
int tldap_msg_type(const struct tldap_message *msg);
const char *tldap_msg_matcheddn(struct tldap_message *msg);
const char *tldap_msg_diagnosticmessage(struct tldap_message *msg);
const char *tldap_msg_referral(struct tldap_message *msg);
void tldap_msg_sctrls(struct tldap_message *msg, int *num_sctrls,
		      struct tldap_control **sctrls);
struct tldap_message *tldap_ctx_lastmsg(struct tldap_context *ld);
const char *tldap_err2string(int rc);

/* DEBUG */
enum tldap_debug_level {
	TLDAP_DEBUG_FATAL,
	TLDAP_DEBUG_ERROR,
	TLDAP_DEBUG_WARNING,
	TLDAP_DEBUG_TRACE
};

void tldap_set_debug(struct tldap_context *ld,
		     void (*log_fn)(void *log_private,
				    enum tldap_debug_level level,
				    const char *fmt,
				    va_list ap) PRINTF_ATTRIBUTE(3,0),
		     void *log_private);

/*
 * "+ 0x60" is from ASN1_APPLICATION
 */
#define TLDAP_REQ_BIND (0 + 0x60)
#define TLDAP_RES_BIND (1 + 0x60)
#define TLDAP_REQ_UNBIND (2 + 0x60)
#define TLDAP_REQ_SEARCH (3 + 0x60)
#define TLDAP_RES_SEARCH_ENTRY (4 + 0x60)
#define TLDAP_RES_SEARCH_RESULT (5 + 0x60)
#define TLDAP_REQ_MODIFY (6 + 0x60)
#define TLDAP_RES_MODIFY (7 + 0x60)
#define TLDAP_REQ_ADD (8 + 0x60)
#define TLDAP_RES_ADD (9 + 0x60)
/* ASN1_APPLICATION_SIMPLE instead of ASN1_APPLICATION */
#define TLDAP_REQ_DELETE (10 + 0x40)
#define TLDAP_RES_DELETE (11 + 0x60)
#define TLDAP_REQ_MODDN (12 + 0x60)
#define TLDAP_RES_MODDN (13 + 0x60)
#define TLDAP_REQ_COMPARE (14 + 0x60)
#define TLDAP_RES_COMPARE (15 + 0x60)
/* ASN1_APPLICATION_SIMPLE instead of ASN1_APPLICATION */
#define TLDAP_REQ_ABANDON (16 + 0x40)
#define TLDAP_RES_SEARCH_REFERENCE (19 + 0x60)
#define TLDAP_REQ_EXTENDED (23 + 0x60)
#define TLDAP_RES_EXTENDED (24 + 0x60)
#define TLDAP_RES_INTERMEDIATE (25 + 0x60)

#define TLDAP_SUCCESS (0x00)
#define TLDAP_OPERATIONS_ERROR (0x01)
#define TLDAP_PROTOCOL_ERROR (0x02)
#define TLDAP_TIMELIMIT_EXCEEDED (0x03)
#define TLDAP_SIZELIMIT_EXCEEDED (0x04)
#define TLDAP_COMPARE_FALSE (0x05)
#define TLDAP_COMPARE_TRUE (0x06)
#define TLDAP_STRONG_AUTH_NOT_SUPPORTED (0x07)
#define TLDAP_STRONG_AUTH_REQUIRED (0x08)
#define TLDAP_REFERRAL (0x0a)
#define TLDAP_ADMINLIMIT_EXCEEDED (0x0b)
#define TLDAP_UNAVAILABLE_CRITICAL_EXTENSION (0x0c)
#define TLDAP_CONFIDENTIALITY_REQUIRED (0x0d)
#define TLDAP_SASL_BIND_IN_PROGRESS (0x0e)
#define TLDAP_NO_SUCH_ATTRIBUTE (0x10)
#define TLDAP_UNDEFINED_TYPE (0x11)
#define TLDAP_INAPPROPRIATE_MATCHING (0x12)
#define TLDAP_CONSTRAINT_VIOLATION (0x13)
#define TLDAP_TYPE_OR_VALUE_EXISTS (0x14)
#define TLDAP_INVALID_SYNTAX (0x15)
#define TLDAP_NO_SUCH_OBJECT (0x20)
#define TLDAP_ALIAS_PROBLEM (0x21)
#define TLDAP_INVALID_DN_SYNTAX (0x22)
#define TLDAP_IS_LEAF (0x23)
#define TLDAP_ALIAS_DEREF_PROBLEM (0x24)
#define TLDAP_INAPPROPRIATE_AUTH (0x30)
#define TLDAP_INVALID_CREDENTIALS (0x31)
#define TLDAP_INSUFFICIENT_ACCESS (0x32)
#define TLDAP_BUSY (0x33)
#define TLDAP_UNAVAILABLE (0x34)
#define TLDAP_UNWILLING_TO_PERFORM (0x35)
#define TLDAP_LOOP_DETECT (0x36)
#define TLDAP_NAMING_VIOLATION (0x40)
#define TLDAP_OBJECT_CLASS_VIOLATION (0x41)
#define TLDAP_NOT_ALLOWED_ON_NONLEAF (0x42)
#define TLDAP_NOT_ALLOWED_ON_RDN (0x43)
#define TLDAP_ALREADY_EXISTS (0x44)
#define TLDAP_NO_OBJECT_CLASS_MODS (0x45)
#define TLDAP_RESULTS_TOO_LARGE (0x46)
#define TLDAP_AFFECTS_MULTIPLE_DSAS (0x47)
#define TLDAP_OTHER (0x50)
#define TLDAP_SERVER_DOWN (0x51)
#define TLDAP_LOCAL_ERROR (0x52)
#define TLDAP_ENCODING_ERROR (0x53)
#define TLDAP_DECODING_ERROR (0x54)
#define TLDAP_TIMEOUT (0x55)
#define TLDAP_AUTH_UNKNOWN (0x56)
#define TLDAP_FILTER_ERROR (0x57)
#define TLDAP_USER_CANCELLED (0x58)
#define TLDAP_PARAM_ERROR (0x59)
#define TLDAP_NO_MEMORY (0x5a)
#define TLDAP_CONNECT_ERROR (0x5b)
#define TLDAP_NOT_SUPPORTED (0x5c)
#define TLDAP_CONTROL_NOT_FOUND (0x5d)
#define TLDAP_NO_RESULTS_RETURNED (0x5e)
#define TLDAP_MORE_RESULTS_TO_RETURN (0x5f)
#define TLDAP_CLIENT_LOOP (0x60)
#define TLDAP_REFERRAL_LIMIT_EXCEEDED (0x61)

#define TLDAP_MOD_ADD (0)
#define TLDAP_MOD_DELETE (1)
#define TLDAP_MOD_REPLACE (2)

#define TLDAP_SCOPE_BASE (0)
#define TLDAP_SCOPE_ONE (1)
#define TLDAP_SCOPE_SUB (2)

#define TLDAP_CONTROL_PAGEDRESULTS "1.2.840.113556.1.4.319"

#endif
