/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Stefan Metzmacher  2004
   Copyright (C) Simo Sorce  2005-2006

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

/*
 *  Name: ldb
 *
 *  Component: ldb header
 *
 *  Description: defines for base ldb API
 *
 *  Author: Andrew Tridgell
 *  Author: Stefan Metzmacher
 */

/**
   \file ldb.h Samba's ldb database

   This header file provides the main API for ldb.
*/

#ifndef _LDB_H_

/*! \cond DOXYGEN_IGNORE */
#define _LDB_H_ 1
/*! \endcond */

/*
  major restrictions as compared to normal LDAP:

     - no async calls.
     - each record must have a unique key field
     - the key must be representable as a NULL terminated C string and may not 
       contain a comma or braces

  major restrictions as compared to tdb:

     - no explicit locking calls
     UPDATE: we have transactions now, better than locking --SSS.

*/

#ifndef ldb_val
/**
   Result value

   An individual lump of data in a result comes in this format. The
   pointer will usually be to a UTF-8 string if the application is
   sensible, but it can be to anything you like, including binary data
   blobs of arbitrary size.

   \note the data is null (0x00) terminated, but the length does not
   include the terminator. 
*/
struct ldb_val {
	uint8_t *data; /*!< result data */
	size_t length; /*!< length of data */
};
#endif

/*! \cond DOXYGEN_IGNORE */
#ifndef PRINTF_ATTRIBUTE
#define PRINTF_ATTRIBUTE(a,b)
#endif
/*! \endcond */

/* opaque ldb_dn structures, see ldb_dn.c for internals */
struct ldb_dn_component;
struct ldb_dn;

/**
 There are a number of flags that are used with ldap_modify() in
 ldb_message_element.flags fields. The LDA_FLAGS_MOD_ADD,
 LDA_FLAGS_MOD_DELETE and LDA_FLAGS_MOD_REPLACE flags are used in
 ldap_modify() calls to specify whether attributes are being added,
 deleted or modified respectively.
*/
#define LDB_FLAG_MOD_MASK  0x3

/**
   Flag value used in ldap_modify() to indicate that attributes are
   being added.

   \sa LDB_FLAG_MOD_MASK
*/
#define LDB_FLAG_MOD_ADD     1

/**
   Flag value used in ldap_modify() to indicate that attributes are
   being replaced.

   \sa LDB_FLAG_MOD_MASK
*/
#define LDB_FLAG_MOD_REPLACE 2

/**
   Flag value used in ldap_modify() to indicate that attributes are
   being deleted.

   \sa LDB_FLAG_MOD_MASK
*/
#define LDB_FLAG_MOD_DELETE  3

/**
  OID for logic AND comaprison.

  This is the well known object ID for a logical AND comparitor.
*/
#define LDB_OID_COMPARATOR_AND  "1.2.840.113556.1.4.803"

/**
  OID for logic OR comparison.

  This is the well known object ID for a logical OR comparitor.
*/
#define LDB_OID_COMPARATOR_OR   "1.2.840.113556.1.4.804"

/**
  results are given back as arrays of ldb_message_element
*/
struct ldb_message_element {
	unsigned int flags;
	const char *name;
	unsigned int num_values;
	struct ldb_val *values;
};


/**
  a ldb_message represents all or part of a record. It can contain an arbitrary
  number of elements. 
*/
struct ldb_message {
	struct ldb_dn *dn;
	unsigned int num_elements;
	struct ldb_message_element *elements;
	void *private_data; /* private to the backend */
};

enum ldb_changetype {
	LDB_CHANGETYPE_NONE=0,
	LDB_CHANGETYPE_ADD,
	LDB_CHANGETYPE_DELETE,
	LDB_CHANGETYPE_MODIFY
};

/**
  LDIF record

  This structure contains a LDIF record, as returned from ldif_read()
  and equivalent functions.
*/
struct ldb_ldif {
	enum ldb_changetype changetype; /*!< The type of change */
	struct ldb_message *msg;  /*!< The changes */
};

enum ldb_scope {LDB_SCOPE_DEFAULT=-1, 
		LDB_SCOPE_BASE=0, 
		LDB_SCOPE_ONELEVEL=1,
		LDB_SCOPE_SUBTREE=2};

struct ldb_context;

/* debugging uses one of the following levels */
enum ldb_debug_level {LDB_DEBUG_FATAL, LDB_DEBUG_ERROR, 
		      LDB_DEBUG_WARNING, LDB_DEBUG_TRACE};

/**
  the user can optionally supply a debug function. The function
  is based on the vfprintf() style of interface, but with the addition
  of a severity level
*/
struct ldb_debug_ops {
	void (*debug)(void *context, enum ldb_debug_level level, 
		      const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3,0);
	void *context;
};

/**
  The user can optionally supply a custom utf8 functions,
  to handle comparisons and casefolding.
*/
struct ldb_utf8_fns {
	void *context;
	char *(*casefold)(void *context, void *mem_ctx, const char *s);
};

/**
   Flag value for database connection mode.

   If LDB_FLG_RDONLY is used in ldb_connect, then the database will be
   opened read-only, if possible.
*/
#define LDB_FLG_RDONLY 1

/**
   Flag value for database connection mode.

   If LDB_FLG_NOSYNC is used in ldb_connect, then the database will be
   opened without synchronous operations, if possible.
*/
#define LDB_FLG_NOSYNC 2

/**
   Flag value to specify autoreconnect mode.

   If LDB_FLG_RECONNECT is used in ldb_connect, then the backend will
   be opened in a way that makes it try to auto reconnect if the
   connection is dropped (actually make sense only with ldap).
*/
#define LDB_FLG_RECONNECT 4

/**
   Flag to tell backends not to use mmap
*/
#define LDB_FLG_NOMMAP 8

/*
   structures for ldb_parse_tree handling code
*/
enum ldb_parse_op { LDB_OP_AND=1, LDB_OP_OR=2, LDB_OP_NOT=3,
		    LDB_OP_EQUALITY=4, LDB_OP_SUBSTRING=5,
		    LDB_OP_GREATER=6, LDB_OP_LESS=7, LDB_OP_PRESENT=8,
		    LDB_OP_APPROX=9, LDB_OP_EXTENDED=10 };

struct ldb_parse_tree {
	enum ldb_parse_op operation;
	union {
		struct {
			struct ldb_parse_tree *child;
		} isnot;
		struct {
			const char *attr;
			struct ldb_val value;
		} equality;
		struct {
			const char *attr;
			int start_with_wildcard;
			int end_with_wildcard;
			struct ldb_val **chunks;
		} substring;
		struct {
			const char *attr;
		} present;
		struct {
			const char *attr;
			struct ldb_val value;
		} comparison;
		struct {
			const char *attr;
			int dnAttributes;
			char *rule_id;
			struct ldb_val value;
		} extended;
		struct {
			unsigned int num_elements;
			struct ldb_parse_tree **elements;
		} list;
	} u;
};

struct ldb_parse_tree *ldb_parse_tree(void *mem_ctx, const char *s);
char *ldb_filter_from_tree(void *mem_ctx, struct ldb_parse_tree *tree);

/**
   Encode a binary blob

   This function encodes a binary blob using the encoding rules in RFC
   2254 (Section 4). This function also escapes any non-printable
   characters.

   \param ctx the memory context to allocate the return string in.
   \param val the (potentially) binary data to be encoded

   \return the encoded data as a null terminated string

   \sa <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>.
*/
char *ldb_binary_encode(void *ctx, struct ldb_val val);

/**
   Encode a string

   This function encodes a string using the encoding rules in RFC 2254
   (Section 4). This function also escapes any non-printable
   characters.

   \param mem_ctx the memory context to allocate the return string in.
   \param string the string to be encoded

   \return the encoded data as a null terminated string

   \sa <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>.
*/
char *ldb_binary_encode_string(void *mem_ctx, const char *string);

/*
  functions for controlling attribute handling
*/
typedef int (*ldb_attr_handler_t)(struct ldb_context *, void *mem_ctx, const struct ldb_val *, struct ldb_val *);
typedef int (*ldb_attr_comparison_t)(struct ldb_context *, void *mem_ctx, const struct ldb_val *, const struct ldb_val *);

/*
  attribute handler structure

  attr			-> The attribute name
  flags			-> LDB_ATTR_FLAG_*
  ldif_read_fn		-> convert from ldif to binary format
  ldif_write_fn		-> convert from binary to ldif format
  canonicalise_fn	-> canonicalise a value, for use by indexing and dn construction
  comparison_fn		-> compare two values
*/

struct ldb_attrib_handler {

	const char *attr;
	unsigned flags;

	ldb_attr_handler_t ldif_read_fn;
	ldb_attr_handler_t ldif_write_fn;
	ldb_attr_handler_t canonicalise_fn;
	ldb_attr_comparison_t comparison_fn;
};

/**
   The attribute is not returned by default
*/
#define LDB_ATTR_FLAG_HIDDEN       (1<<0) 

/* the attribute handler name should be freed when released */
#define LDB_ATTR_FLAG_ALLOCATED    (1<<1) 

/**
   The attribute is constructed from other attributes
*/
#define LDB_ATTR_FLAG_CONSTRUCTED  (1<<1) 

/**
  LDAP attribute syntax for a DN

  This is the well-known LDAP attribute syntax for a DN.

  See <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_DN                   "1.3.6.1.4.1.1466.115.121.1.12"

/**
  LDAP attribute syntax for a Directory String

  This is the well-known LDAP attribute syntax for a Directory String.

  \sa <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_DIRECTORY_STRING     "1.3.6.1.4.1.1466.115.121.1.15"

/**
  LDAP attribute syntax for an integer

  This is the well-known LDAP attribute syntax for an integer.

  See <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_INTEGER              "1.3.6.1.4.1.1466.115.121.1.27"

/**
  LDAP attribute syntax for an octet string

  This is the well-known LDAP attribute syntax for an octet string.

  See <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_OCTET_STRING         "1.3.6.1.4.1.1466.115.121.1.40"

/**
  LDAP attribute syntax for UTC time.

  This is the well-known LDAP attribute syntax for a UTC time.

  See <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_UTC_TIME             "1.3.6.1.4.1.1466.115.121.1.53"

#define LDB_SYNTAX_OBJECTCLASS          "LDB_SYNTAX_OBJECTCLASS"

/* sorting helpers */
typedef int (*ldb_qsort_cmp_fn_t) (void *v1, void *v2, void *opaque);

/**
   OID for the paged results control. This control is included in the
   searchRequest and searchResultDone messages as part of the controls
   field of the LDAPMessage, as defined in Section 4.1.12 of
   LDAP v3. 

   \sa <a href="http://www.ietf.org/rfc/rfc2696.txt">RFC 2696</a>.
*/
#define LDB_CONTROL_PAGED_RESULTS_OID	"1.2.840.113556.1.4.319"

/**
   OID for specifying the returned elements of the ntSecurityDescriptor

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_sd_flags_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SD_FLAGS_OID	"1.2.840.113556.1.4.801"

/**
   OID for specifying an advanced scope for the search (one partition)

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_domain_scope_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_DOMAIN_SCOPE_OID	"1.2.840.113556.1.4.1339"

/**
   OID for specifying an advanced scope for a search

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_search_options_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SEARCH_OPTIONS_OID	"1.2.840.113556.1.4.1340"

/**
   OID for notification

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_notification_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_NOTIFICATION_OID	"1.2.840.113556.1.4.528"

/**
   OID for getting deleted objects

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_show_deleted_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SHOW_DELETED_OID	"1.2.840.113556.1.4.417"

/**
   OID for extended DN

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_extended_dn_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_EXTENDED_DN_OID	"1.2.840.113556.1.4.529"

/**
   OID for LDAP server sort result extension.

   This control is included in the searchRequest message as part of
   the controls field of the LDAPMessage, as defined in Section 4.1.12
   of LDAP v3. The controlType is set to
   "1.2.840.113556.1.4.473". The criticality MAY be either TRUE or
   FALSE (where absent is also equivalent to FALSE) at the client's
   option.

   \sa <a href="http://www.ietf.org/rfc/rfc2891.txt">RFC 2891</a>.
*/
#define LDB_CONTROL_SERVER_SORT_OID	"1.2.840.113556.1.4.473"

/**
   OID for LDAP server sort result response extension.

   This control is included in the searchResultDone message as part of
   the controls field of the LDAPMessage, as defined in Section 4.1.12 of
   LDAP v3.

   \sa <a href="http://www.ietf.org/rfc/rfc2891.txt">RFC 2891</a>.
*/
#define LDB_CONTROL_SORT_RESP_OID	"1.2.840.113556.1.4.474"

/**
   OID for LDAP Attribute Scoped Query extension.

   This control is included in SearchRequest or SearchResponse
   messages as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_ASQ_OID		"1.2.840.113556.1.4.1504"

/**
   OID for LDAP Directory Sync extension. 

   This control is included in SearchRequest or SearchResponse
   messages as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_DIRSYNC_OID		"1.2.840.113556.1.4.841"


/**
   OID for LDAP Virtual List View Request extension.

   This control is included in SearchRequest messages
   as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_VLV_REQ_OID		"2.16.840.1.113730.3.4.9"

/**
   OID for LDAP Virtual List View Response extension.

   This control is included in SearchResponse messages
   as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_VLV_RESP_OID	"2.16.840.1.113730.3.4.10"

/**
   OID to let modifies don't give an error when adding an existing
   attribute with the same value or deleting an nonexisting one attribute

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_permissive_modify_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_PERMISSIVE_MODIFY_OID	"1.2.840.113556.1.4.1413"

/**
   OID for LDAP Extended Operation START_TLS.

   This Extended operation is used to start a new TLS
   channel on top of a clear text channel.
*/
#define LDB_EXTENDED_START_TLS_OID	"1.3.6.1.4.1.1466.20037"

/**
   OID for LDAP Extended Operation START_TLS.

   This Extended operation is used to start a new TLS
   channel on top of a clear text channel.
*/
#define LDB_EXTENDED_DYNAMIC_OID	"1.3.6.1.4.1.1466.101.119.1"

/**
   OID for LDAP Extended Operation START_TLS.

   This Extended operation is used to start a new TLS
   channel on top of a clear text channel.
*/
#define LDB_EXTENDED_FAST_BIND_OID	"1.2.840.113556.1.4.1781"

struct ldb_sd_flags_control {
	/*
	 * request the owner	0x00000001
	 * request the group	0x00000002
	 * request the DACL	0x00000004
	 * request the SACL	0x00000008
	 */
	unsigned secinfo_flags;
};

struct ldb_search_options_control {
	/*
	 * DOMAIN_SCOPE		0x00000001
	 * this limits the search to one partition,
	 * and no referrals will be returned.
	 * (Note this doesn't limit the entries by there
	 *  objectSid belonging to a domain! Builtin and Foreign Sids
	 *  are still returned)
	 *
	 * PHANTOM_ROOT		0x00000002
	 * this search on the whole tree on a domain controller
	 * over multiple partitions without referrals.
	 * (This is the default behavior on the Global Catalog Port)
	 */
	unsigned search_options;
};

struct ldb_paged_control {
	int size;
	int cookie_len;
	char *cookie;
};

struct ldb_extended_dn_control {
	int type;
};

struct ldb_server_sort_control {
	char *attributeName;
	char *orderingRule;
	int reverse;
};

struct ldb_sort_resp_control {
	int result;
	char *attr_desc;
};

struct ldb_asq_control {
	int request;
	char *source_attribute;
	int src_attr_len;
	int result;
};

struct ldb_dirsync_control {
	int flags;
	int max_attributes;
	int cookie_len;
	char *cookie;
};

struct ldb_vlv_req_control {
	int beforeCount;
	int afterCount;
	int type;
	union {
		struct {
			int offset;
			int contentCount;
		} byOffset;
		struct {
			int value_len;
			char *value;
		} gtOrEq;
	} match;
	int ctxid_len;
	char *contextId;
};

struct ldb_vlv_resp_control {
	int targetPosition;
	int contentCount;
	int vlv_result;
	int ctxid_len;
	char *contextId;
};

struct ldb_control {
	const char *oid;
	int critical;
	void *data;
};

enum ldb_request_type {
	LDB_SEARCH,
	LDB_ADD,
	LDB_MODIFY,
	LDB_DELETE,
	LDB_RENAME,
	LDB_EXTENDED,
	LDB_REQ_REGISTER_CONTROL,
	LDB_REQ_REGISTER_PARTITION,
	LDB_SEQUENCE_NUMBER
};

enum ldb_reply_type {
	LDB_REPLY_ENTRY,
	LDB_REPLY_REFERRAL,
	LDB_REPLY_EXTENDED,
	LDB_REPLY_DONE
};

enum ldb_wait_type {
	LDB_WAIT_ALL,
	LDB_WAIT_NONE
};

enum ldb_state {
	LDB_ASYNC_INIT,
	LDB_ASYNC_PENDING,
	LDB_ASYNC_DONE
};

struct ldb_result {
	unsigned int count;
	struct ldb_message **msgs;
	char **refs;
	struct ldb_control **controls;
};

struct ldb_extended {
	const char *oid;
	const char *value;
	int value_len;
};

struct ldb_reply {
	enum ldb_reply_type type;
	struct ldb_message *message;
	struct ldb_extended *response;
	char *referral;
	struct ldb_control **controls;
};

struct ldb_handle {
	int status;
	enum ldb_state state;
	void *private_data;
	struct ldb_module *module;
};

struct ldb_search {
	const struct ldb_dn *base;
	enum ldb_scope scope;
	const struct ldb_parse_tree *tree;
	const char * const *attrs;
	struct ldb_result *res;
};

struct ldb_add {
	const struct ldb_message *message;
};

struct  ldb_modify {
	const struct ldb_message *message;
};

struct ldb_delete {
	const struct ldb_dn *dn;
};

struct ldb_rename {
	const struct ldb_dn *olddn;
	const struct ldb_dn *newdn;
};

struct ldb_register_control {
	const char *oid;
};

struct ldb_register_partition {
	const struct ldb_dn *dn;
};

struct ldb_sequence_number {
	enum ldb_sequence_type {
		LDB_SEQ_HIGHEST_SEQ,
		LDB_SEQ_HIGHEST_TIMESTAMP,
		LDB_SEQ_NEXT
	} type;
	uint64_t seq_num;
	uint32_t flags;
};

typedef int (*ldb_request_callback_t)(struct ldb_context *, void *, struct ldb_reply *);
struct ldb_request {

	enum ldb_request_type operation;

	union {
		struct ldb_search search;
		struct ldb_add    add;
		struct ldb_modify mod;
		struct ldb_delete del;
		struct ldb_rename rename;
		struct ldb_register_control reg_control;
		struct ldb_register_partition reg_partition;
		struct ldb_sequence_number seq_num;
	} op;

	struct ldb_control **controls;

	void *context;
	ldb_request_callback_t callback;

	int timeout;
	time_t starttime;
	struct ldb_handle *handle;
};

int ldb_request(struct ldb_context *ldb, struct ldb_request *request);

int ldb_wait(struct ldb_handle *handle, enum ldb_wait_type type);

int ldb_set_timeout(struct ldb_context *ldb, struct ldb_request *req, int timeout);
int ldb_set_timeout_from_prev_req(struct ldb_context *ldb, struct ldb_request *oldreq, struct ldb_request *newreq);
void ldb_set_create_perms(struct ldb_context *ldb, unsigned int perms);

/**
  Initialise ldbs' global information

  This is required before any other LDB call

  \return 0 if initialisation succeeded, -1 otherwise
*/
int ldb_global_init(void);

/**
  Initialise an ldb context

  This is required before any other LDB call.

  \param mem_ctx pointer to a talloc memory context. Pass NULL if there is
  no suitable context available.

  \param ev_ctx Event context. This is here for API compatibility 
  with the Samba 4 version of LDB and ignored in this version of LDB.

  \return pointer to ldb_context that should be free'd (using talloc_free())
  at the end of the program.
*/
struct ldb_context *ldb_init(void *mem_ctx, struct tevent_context *ev_ctx);

/**
   Connect to a database.

   This is typically called soon after ldb_init(), and is required prior to
   any search or database modification operations.

   The URL can be one of the following forms:
    - tdb://path
    - ldapi://path
    - ldap://host
    - sqlite://path

   \param ldb the context associated with the database (from ldb_init())
   \param url the URL of the database to connect to, as noted above
   \param flags a combination of LDB_FLG_* to modify the connection behaviour
   \param options backend specific options - passed uninterpreted to the backend

   \return result code (LDB_SUCCESS on success, or a failure code)

   \note It is an error to connect to a database that does not exist in readonly mode
   (that is, with LDB_FLG_RDONLY). However in read-write mode, the database will be
   created if it does not exist.
*/
int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[]);

/*
  return an automatic baseDN from the defaultNamingContext of the rootDSE
  This value have been set in an opaque pointer at connection time
*/
const struct ldb_dn *ldb_get_default_basedn(struct ldb_context *ldb);


/**
  The Default iasync search callback function 

  \param ldb the context associated with the database (from ldb_init())
  \param context the callback context
  \param ares a single reply from the async core

  \return result code (LDB_SUCCESS on success, or a failure code)

  \note this function expects the context to always be an struct ldb_result pointer
  AND a talloc context, this function will steal on the context each message
  from the ares reply passed on by the async core so that in the end all the
  messages will be in the context (ldb_result)  memory tree.
  Freeing the passed context (ldb_result tree) will free all the resources
  (the request need to be freed separately and the result doe not depend on the
  request that can be freed as sson as the search request is finished)
*/

int ldb_search_default_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares);

/**
  Helper function to build a search request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc emmory context (used as parent of ret_req)
  \param base the Base Distinguished Name for the query (use ldb_dn_new() for an empty one)
  \param scope the search scope for the query
  \param expression the search expression to use for this query
  \param attrs the search attributes for the query (pass NULL if none required)
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_search_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *base,
	       		enum ldb_scope scope,
			const char *expression,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback);

/**
  Helper function to build an add request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc emmory context (used as parent of ret_req)
  \param message contains the entry to be added 
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_add_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback);

/**
  Helper function to build a modify request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc emmory context (used as parent of ret_req)
  \param message contains the entry to be modified
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_mod_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback);

/**
  Helper function to build a delete request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc emmory context (used as parent of ret_req)
  \param dn the DN to be deleted
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_del_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *dn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback);

/**
  Helper function to build a rename request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc emmory context (used as parent of ret_req)
  \param olddn the old DN
  \param newdn the new DN
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_rename_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			void *mem_ctx,
			const struct ldb_dn *olddn,
			const struct ldb_dn *newdn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback);

/**
  Search the database

  This function searches the database, and returns 
  records that match an LDAP-like search expression

  \param ldb the context associated with the database (from ldb_init())
  \param base the Base Distinguished Name for the query (use ldb_dn_new() for an empty one)
  \param scope the search scope for the query
  \param expression the search expression to use for this query
  \param attrs the search attributes for the query (pass NULL if none required)
  \param res the return result

  \return result code (LDB_SUCCESS on success, or a failure code)

  \note use talloc_free() to free the ldb_result returned
*/
int ldb_search(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
		       struct ldb_result **result, struct ldb_dn *base,
		       enum ldb_scope scope, const char * const *attrs,
		       const char *exp_fmt, ...);

/*
  like ldb_search() but takes a parse tree
*/
int ldb_search_bytree(struct ldb_context *ldb, 
		      const struct ldb_dn *base,
		      enum ldb_scope scope,
		      struct ldb_parse_tree *tree,
		      const char * const *attrs, struct ldb_result **res);

/**
  Add a record to the database.

  This function adds a record to the database. This function will fail
  if a record with the specified class and key already exists in the
  database. 

  \param ldb the context associated with the database (from
  ldb_init())
  \param message the message containing the record to add.

  \return result code (LDB_SUCCESS if the record was added, otherwise
  a failure code)
*/
int ldb_add(struct ldb_context *ldb, 
	    const struct ldb_message *message);

/**
  Modify the specified attributes of a record

  This function modifies a record that is in the database.

  \param ldb the context associated with the database (from
  ldb_init())
  \param message the message containing the changes required.

  \return result code (LDB_SUCCESS if the record was modified as
  requested, otherwise a failure code)
*/
int ldb_modify(struct ldb_context *ldb, 
	       const struct ldb_message *message);

/**
  Rename a record in the database

  This function renames a record in the database.

  \param ldb the context associated with the database (from
  ldb_init())
  \param olddn the DN for the record to be renamed.
  \param newdn the new DN 

  \return result code (LDB_SUCCESS if the record was renamed as
  requested, otherwise a failure code)
*/
int ldb_rename(struct ldb_context *ldb, const struct ldb_dn *olddn, const struct ldb_dn *newdn);

/**
  Delete a record from the database

  This function deletes a record from the database.

  \param ldb the context associated with the database (from
  ldb_init())
  \param dn the DN for the record to be deleted.

  \return result code (LDB_SUCCESS if the record was deleted,
  otherwise a failure code)
*/
int ldb_delete(struct ldb_context *ldb, const struct ldb_dn *dn);

/**
  start a transaction
*/
int ldb_transaction_start(struct ldb_context *ldb);

/**
  commit a transaction
*/
int ldb_transaction_commit(struct ldb_context *ldb);

/**
  cancel a transaction
*/
int ldb_transaction_cancel(struct ldb_context *ldb);


/**
  return extended error information from the last call
*/
const char *ldb_errstring(struct ldb_context *ldb);

/**
  return a string explaining what a ldb error constant meancs
*/
const char *ldb_strerror(int ldb_err);

/**
  setup the default utf8 functions
  FIXME: these functions do not yet handle utf8
*/
void ldb_set_utf8_default(struct ldb_context *ldb);

/**
   Casefold a string

   \param ldb the ldb context
   \param mem_ctx the memory context to allocate the result string
   memory from. 
   \param s the string that is to be folded
   \return a copy of the string, converted to upper case

   \note The default function is not yet UTF8 aware. Provide your own
         set of functions through ldb_set_utf8_fns()
*/
char *ldb_casefold(struct ldb_context *ldb, void *mem_ctx, const char *s);

/**
   Check the attribute name is valid according to rfc2251
   \param s tthe string to check

   \return 1 if the name is ok
*/
int ldb_valid_attr_name(const char *s);

/*
  ldif manipulation functions
*/
/**
   Write an LDIF message

   This function writes an LDIF message using a caller supplied  write
   function.

   \param ldb the ldb context (from ldb_init())
   \param fprintf_fn a function pointer for the write function. This must take
   a private data pointer, followed by a format string, and then a variable argument
   list. 
   \param private_data pointer that will be provided back to the write
   function. This is useful for maintaining state or context.
   \param ldif the message to write out

   \return the total number of bytes written, or an error code as returned
   from the write function.

   \sa ldb_ldif_write_file for a more convenient way to write to a
   file stream.

   \sa ldb_ldif_read for the reader equivalent to this function.
*/
int ldb_ldif_write(struct ldb_context *ldb,
		   int (*fprintf_fn)(void *, const char *, ...) PRINTF_ATTRIBUTE(2,3), 
		   void *private_data,
		   const struct ldb_ldif *ldif);

/**
   Clean up an LDIF message

   This function cleans up a LDIF message read using ldb_ldif_read()
   or related functions (such as ldb_ldif_read_string() and
   ldb_ldif_read_file().

   \param ldb the ldb context (from ldb_init())
   \param msg the message to clean up and free

*/
void ldb_ldif_read_free(struct ldb_context *ldb, struct ldb_ldif *msg);

/**
   Read an LDIF message

   This function creates an LDIF message using a caller supplied read
   function. 

   \param ldb the ldb context (from ldb_init())
   \param fgetc_fn a function pointer for the read function. This must
   take a private data pointer, and must return a pointer to an
   integer corresponding to the next byte read (or EOF if there is no
   more data to be read).
   \param private_data pointer that will be provided back to the read
   function. This is udeful for maintaining state or context.

   \return the LDIF message that has been read in

   \note You must free the LDIF message when no longer required, using
   ldb_ldif_read_free().

   \sa ldb_ldif_read_file for a more convenient way to read from a
   file stream.

   \sa ldb_ldif_read_string for a more convenient way to read from a
   string (char array).

   \sa ldb_ldif_write for the writer equivalent to this function.
*/
struct ldb_ldif *ldb_ldif_read(struct ldb_context *ldb, 
			       int (*fgetc_fn)(void *), void *private_data);

/**
   Read an LDIF message from a file

   This function reads the next LDIF message from the contents of a
   file stream. If you want to get all of the LDIF messages, you will
   need to repeatedly call this function, until it returns NULL.

   \param ldb the ldb context (from ldb_init())
   \param f the file stream to read from (typically from fdopen())

   \sa ldb_ldif_read_string for an equivalent function that will read
   from a string (char array).

   \sa ldb_ldif_write_file for the writer equivalent to this function.

*/
struct ldb_ldif *ldb_ldif_read_file(struct ldb_context *ldb, FILE *f);

/**
   Read an LDIF message from a string

   This function reads the next LDIF message from the contents of a char
   array. If you want to get all of the LDIF messages, you will need
   to repeatedly call this function, until it returns NULL.

   \param ldb the ldb context (from ldb_init())
   \param s pointer to the char array to read from

   \sa ldb_ldif_read_file for an equivalent function that will read
   from a file stream.

   \sa ldb_ldif_write for a more general (arbitrary read function)
   version of this function.
*/
struct ldb_ldif *ldb_ldif_read_string(struct ldb_context *ldb, const char **s);

/**
   Write an LDIF message to a file

   \param ldb the ldb context (from ldb_init())
   \param f the file stream to write to (typically from fdopen())
   \param msg the message to write out

   \return the total number of bytes written, or a negative error code

   \sa ldb_ldif_read_file for the reader equivalent to this function.
*/
int ldb_ldif_write_file(struct ldb_context *ldb, FILE *f, const struct ldb_ldif *msg);

/**
   Base64 encode a buffer

   \param mem_ctx the memory context that the result is allocated
   from. 
   \param buf pointer to the array that is to be encoded
   \param len the number of elements in the array to be encoded

   \return pointer to an array containing the encoded data

   \note The caller is responsible for freeing the result
*/
char *ldb_base64_encode(void *mem_ctx, const char *buf, int len);

/**
   Base64 decode a buffer

   This function decodes a base64 encoded string in place.

   \param s the string to decode.

   \return the length of the returned (decoded) string.

   \note the string is null terminated, but the null terminator is not
   included in the length.
*/
int ldb_base64_decode(char *s);

int ldb_attrib_add_handlers(struct ldb_context *ldb, 
			    const struct ldb_attrib_handler *handlers, 
			    unsigned num_handlers);

/* The following definitions come from lib/ldb/common/ldb_dn.c  */

int ldb_dn_is_special(const struct ldb_dn *dn);
int ldb_dn_check_special(const struct ldb_dn *dn, const char *check);
char *ldb_dn_escape_value(void *mem_ctx, struct ldb_val value);
struct ldb_dn *ldb_dn_new(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, const char *dn);
bool ldb_dn_validate(struct ldb_dn *dn);
struct ldb_dn *ldb_dn_new_fmt(void *mem_ctx, struct ldb_context *ldb, const char *new_fmt, ...);
struct ldb_dn *ldb_dn_explode(void *mem_ctx, const char *dn);
struct ldb_dn *ldb_dn_explode_or_special(void *mem_ctx, const char *dn);
char *ldb_dn_linearize(void *mem_ctx, const struct ldb_dn *edn);
char *ldb_dn_linearize_casefold(struct ldb_context *ldb, void *mem_ctx, const struct ldb_dn *edn);
int ldb_dn_compare_base(struct ldb_context *ldb, const struct ldb_dn *base, const struct ldb_dn *dn);
int ldb_dn_compare(struct ldb_context *ldb, const struct ldb_dn *edn0, const struct ldb_dn *edn1);
struct ldb_dn *ldb_dn_casefold(struct ldb_context *ldb, void *mem_ctx, const struct ldb_dn *edn);
struct ldb_dn *ldb_dn_explode_casefold(struct ldb_context *ldb, void *mem_ctx, const char *dn);
struct ldb_dn *ldb_dn_copy_partial(void *mem_ctx, const struct ldb_dn *dn, int num_el);
struct ldb_dn *ldb_dn_copy(void *mem_ctx, const struct ldb_dn *dn);
struct ldb_dn *ldb_dn_copy_rebase(void *mem_ctx, const struct ldb_dn *old, const struct ldb_dn *old_base, const struct ldb_dn *new_base);
struct ldb_dn *ldb_dn_get_parent(void *mem_ctx, const struct ldb_dn *dn);
struct ldb_dn_component *ldb_dn_build_component(void *mem_ctx, const char *attr,
							       const char *val);
struct ldb_dn *ldb_dn_build_child(void *mem_ctx, const char *attr,
						 const char * value,
						 const struct ldb_dn *base);
struct ldb_dn *ldb_dn_compose(void *mem_ctx, const struct ldb_dn *dn1, const struct ldb_dn *dn2);
struct ldb_dn *ldb_dn_string_compose(void *mem_ctx, const struct ldb_dn *base, const char *child_fmt, ...) PRINTF_ATTRIBUTE(3,4);
char *ldb_dn_canonical_string(void *mem_ctx, const struct ldb_dn *dn);
char *ldb_dn_canonical_ex_string(void *mem_ctx, const struct ldb_dn *dn);
int ldb_dn_get_comp_num(const struct ldb_dn *dn);
const char *ldb_dn_get_component_name(const struct ldb_dn *dn, unsigned int num);
const struct ldb_val *ldb_dn_get_component_val(const struct ldb_dn *dn, unsigned int num);
const char *ldb_dn_get_rdn_name(const struct ldb_dn *dn);
const struct ldb_val *ldb_dn_get_rdn_val(const struct ldb_dn *dn);
int ldb_dn_set_component(struct ldb_dn *dn, int num, const char *name, const struct ldb_val val);



/* useful functions for ldb_message structure manipulation */
int ldb_dn_cmp(struct ldb_context *ldb, const char *dn1, const char *dn2);

/**
   Compare two attributes

   This function compares to attribute names. Note that this is a
   case-insensitive comparison.

   \param attr1 the first attribute name to compare
   \param attr2 the second attribute name to compare

   \return 0 if the attribute names are the same, or only differ in
   case; non-zero if there are any differences
*/
int ldb_attr_cmp(const char *attr1, const char *attr2);
char *ldb_attr_casefold(void *mem_ctx, const char *s);
int ldb_attr_dn(const char *attr);

/**
   Create an empty message

   \param mem_ctx the memory context to create in. You can pass NULL
   to get the top level context, however the ldb context (from
   ldb_init()) may be a better choice
*/
struct ldb_message *ldb_msg_new(void *mem_ctx);

/**
   Find an element within an message
*/
struct ldb_message_element *ldb_msg_find_element(const struct ldb_message *msg, 
						 const char *attr_name);

/**
   Compare two ldb_val values

   \param v1 first ldb_val structure to be tested
   \param v2 second ldb_val structure to be tested

   \return 1 for a match, 0 if there is any difference
*/
int ldb_val_equal_exact(const struct ldb_val *v1, const struct ldb_val *v2);

/**
   find a value within an ldb_message_element

   \param el the element to search
   \param val the value to search for

   \note This search is case sensitive
*/
struct ldb_val *ldb_msg_find_val(const struct ldb_message_element *el, 
				 struct ldb_val *val);

/**
   add a new empty element to a ldb_message
*/
int ldb_msg_add_empty(struct ldb_message *msg,
		const char *attr_name,
		int flags,
		struct ldb_message_element **return_el);

/**
   add a element to a ldb_message
*/
int ldb_msg_add(struct ldb_message *msg, 
		const struct ldb_message_element *el, 
		int flags);
int ldb_msg_add_value(struct ldb_message *msg, 
		const char *attr_name,
		const struct ldb_val *val,
		struct ldb_message_element **return_el);
int ldb_msg_add_steal_value(struct ldb_message *msg, 
		      const char *attr_name,
		      struct ldb_val *val);
int ldb_msg_add_steal_string(struct ldb_message *msg, 
			     const char *attr_name, char *str);
int ldb_msg_add_string(struct ldb_message *msg, 
		       const char *attr_name, const char *str);
int ldb_msg_add_fmt(struct ldb_message *msg, 
		    const char *attr_name, const char *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
   compare two message elements - return 0 on match
*/
int ldb_msg_element_compare(struct ldb_message_element *el1, 
			    struct ldb_message_element *el2);

/**
   Find elements in a message.

   This function finds elements and converts to a specific type, with
   a give default value if not found. Assumes that elements are
   single valued.
*/
const struct ldb_val *ldb_msg_find_ldb_val(const struct ldb_message *msg, const char *attr_name);
int ldb_msg_find_attr_as_int(const struct ldb_message *msg, 
			     const char *attr_name,
			     int default_value);
unsigned int ldb_msg_find_attr_as_uint(const struct ldb_message *msg, 
				       const char *attr_name,
				       unsigned int default_value);
int64_t ldb_msg_find_attr_as_int64(const struct ldb_message *msg, 
				   const char *attr_name,
				   int64_t default_value);
uint64_t ldb_msg_find_attr_as_uint64(const struct ldb_message *msg, 
				     const char *attr_name,
				     uint64_t default_value);
double ldb_msg_find_attr_as_double(const struct ldb_message *msg, 
				   const char *attr_name,
				   double default_value);
int ldb_msg_find_attr_as_bool(const struct ldb_message *msg, 
			      const char *attr_name,
			      int default_value);
const char *ldb_msg_find_attr_as_string(const struct ldb_message *msg, 
					const char *attr_name,
					const char *default_value);

struct ldb_dn *ldb_msg_find_attr_as_dn(void *mem_ctx,
				       const struct ldb_message *msg,
				        const char *attr_name);

void ldb_msg_sort_elements(struct ldb_message *msg);

struct ldb_message *ldb_msg_copy_shallow(void *mem_ctx, 
					 const struct ldb_message *msg);
struct ldb_message *ldb_msg_copy(void *mem_ctx, 
				 const struct ldb_message *msg);

struct ldb_message *ldb_msg_canonicalize(struct ldb_context *ldb, 
					 const struct ldb_message *msg);


struct ldb_message *ldb_msg_diff(struct ldb_context *ldb, 
				 struct ldb_message *msg1,
				 struct ldb_message *msg2);

int ldb_msg_check_string_attribute(const struct ldb_message *msg,
				   const char *name,
				   const char *value);

/**
   Integrity check an ldb_message

   This function performs basic sanity / integrity checks on an
   ldb_message.

   \param msg the message to check

   \return LDB_SUCCESS if the message is OK, or a non-zero error code
   (one of LDB_ERR_INVALID_DN_SYNTAX, LDB_ERR_ENTRY_ALREADY_EXISTS or
   LDB_ERR_INVALID_ATTRIBUTE_SYNTAX) if there is a problem with a
   message.
*/
int ldb_msg_sanity_check(struct ldb_context *ldb,
			 const struct ldb_message *msg);

/**
   Duplicate an ldb_val structure

   This function copies an ldb value structure. 

   \param mem_ctx the memory context that the duplicated value will be
   allocated from
   \param v the ldb_val to be duplicated.

   \return the duplicated ldb_val structure.
*/
struct ldb_val ldb_val_dup(void *mem_ctx, const struct ldb_val *v);

/**
  this allows the user to set a debug function for error reporting
*/
int ldb_set_debug(struct ldb_context *ldb,
		  void (*debug)(void *context, enum ldb_debug_level level, 
				const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3,0),
		  void *context);

/**
  this allows the user to set custom utf8 function for error reporting
*/
void ldb_set_utf8_fns(struct ldb_context *ldb,
			void *context,
			char *(*casefold)(void *, void *, const char *));

/**
   this sets up debug to print messages on stderr
*/
int ldb_set_debug_stderr(struct ldb_context *ldb);

/* control backend specific opaque values */
int ldb_set_opaque(struct ldb_context *ldb, const char *name, void *value);
void *ldb_get_opaque(struct ldb_context *ldb, const char *name);

const struct ldb_attrib_handler *ldb_attrib_handler(struct ldb_context *ldb,
						    const char *attrib);


const char **ldb_attr_list_copy(void *mem_ctx, const char * const *attrs);
const char **ldb_attr_list_copy_add(void *mem_ctx, const char * const *attrs, const char *new_attr);
int ldb_attr_in_list(const char * const *attrs, const char *attr);


void ldb_parse_tree_attr_replace(struct ldb_parse_tree *tree, 
				 const char *attr, 
				 const char *replace);

int ldb_msg_rename_attr(struct ldb_message *msg, const char *attr, const char *replace);
int ldb_msg_copy_attr(struct ldb_message *msg, const char *attr, const char *replace);
void ldb_msg_remove_attr(struct ldb_message *msg, const char *attr);

/**
   Convert a time structure to a string

   This function converts a time_t structure to an LDAP formatted time
   string.

   \param mem_ctx the memory context to allocate the return string in
   \param t the time structure to convert

   \return the formatted string, or NULL if the time structure could
   not be converted
*/
char *ldb_timestring(void *mem_ctx, time_t t);

/**
   Convert a string to a time structure

   This function converts an LDAP formatted time string to a time_t
   structure.

   \param s the string to convert

   \return the time structure, or 0 if the string cannot be converted
*/
time_t ldb_string_to_time(const char *s);


void ldb_qsort (void *const pbase, size_t total_elems, size_t size, void *opaque, ldb_qsort_cmp_fn_t cmp);
#endif
