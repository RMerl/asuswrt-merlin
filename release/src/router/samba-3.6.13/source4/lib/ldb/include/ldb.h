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

#include <stdbool.h>
#include <talloc.h>
#include <tevent.h>
#include <ldb_version.h>
#include <ldb_errors.h>

/*
  major restrictions as compared to normal LDAP:

     - each record must have a unique key field
     - the key must be representable as a NULL terminated C string and may not 
       contain a comma or braces

  major restrictions as compared to tdb:

     - no explicit locking calls, but we have transactions when using ldb_tdb

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

#ifndef _DEPRECATED_
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1 )
#define _DEPRECATED_ __attribute__ ((deprecated))
#else
#define _DEPRECATED_
#endif
#endif
/*! \endcond */

/* opaque ldb_dn structures, see ldb_dn.c for internals */
struct ldb_dn_component;
struct ldb_dn;

/**
 There are a number of flags that are used with ldap_modify() in
 ldb_message_element.flags fields. The LDB_FLAGS_MOD_ADD,
 LDB_FLAGS_MOD_DELETE and LDB_FLAGS_MOD_REPLACE flags are used in
 ldap_modify() calls to specify whether attributes are being added,
 deleted or modified respectively.
*/
#define LDB_FLAG_MOD_MASK  0x3

/**
  use this to extract the mod type from the operation
 */
#define LDB_FLAG_MOD_TYPE(flags) ((flags) & LDB_FLAG_MOD_MASK)

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
    flag bits on an element usable only by the internal implementation
*/
#define LDB_FLAG_INTERNAL_MASK 0xFFFFFFF0

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
struct tevent_context;

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
	char *(*casefold)(void *context, TALLOC_CTX *mem_ctx, const char *s, size_t n);
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

/**
   Flag to tell ldif handlers not to force encoding of binary
   structures in base64   
*/
#define LDB_FLG_SHOW_BINARY 16

/**
   Flags to enable ldb tracing
*/
#define LDB_FLG_ENABLE_TRACING 32

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

struct ldb_parse_tree *ldb_parse_tree(TALLOC_CTX *mem_ctx, const char *s);
char *ldb_filter_from_tree(TALLOC_CTX *mem_ctx, const struct ldb_parse_tree *tree);

/**
   Encode a binary blob

   This function encodes a binary blob using the encoding rules in RFC
   2254 (Section 4). This function also escapes any non-printable
   characters.

   \param mem_ctx the memory context to allocate the return string in.
   \param val the (potentially) binary data to be encoded

   \return the encoded data as a null terminated string

   \sa <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>.
*/
char *ldb_binary_encode(TALLOC_CTX *mem_ctx, struct ldb_val val);

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
char *ldb_binary_encode_string(TALLOC_CTX *mem_ctx, const char *string);

/*
  functions for controlling attribute handling
*/
typedef int (*ldb_attr_handler_t)(struct ldb_context *, TALLOC_CTX *mem_ctx, const struct ldb_val *, struct ldb_val *);
typedef int (*ldb_attr_comparison_t)(struct ldb_context *, TALLOC_CTX *mem_ctx, const struct ldb_val *, const struct ldb_val *);
struct ldb_schema_attribute;
typedef int (*ldb_attr_operator_t)(struct ldb_context *, enum ldb_parse_op operation,
				   const struct ldb_schema_attribute *a,
				   const struct ldb_val *, const struct ldb_val *, bool *matched);

/*
  attribute handler structure

  attr			-> The attribute name
  ldif_read_fn		-> convert from ldif to binary format
  ldif_write_fn		-> convert from binary to ldif format
  canonicalise_fn	-> canonicalise a value, for use by indexing and dn construction
  comparison_fn		-> compare two values
*/

struct ldb_schema_syntax {
	const char *name;
	ldb_attr_handler_t ldif_read_fn;
	ldb_attr_handler_t ldif_write_fn;
	ldb_attr_handler_t canonicalise_fn;
	ldb_attr_comparison_t comparison_fn;
	ldb_attr_operator_t operator_fn;
};

struct ldb_schema_attribute {
	const char *name;
	unsigned flags;
	const struct ldb_schema_syntax *syntax;
};

const struct ldb_schema_attribute *ldb_schema_attribute_by_name(struct ldb_context *ldb,
								const char *name);

struct ldb_dn_extended_syntax {
	const char *name;
	ldb_attr_handler_t read_fn;
	ldb_attr_handler_t write_clear_fn;
	ldb_attr_handler_t write_hex_fn;
};

const struct ldb_dn_extended_syntax *ldb_dn_extended_syntax_by_name(struct ldb_context *ldb,
								    const char *name);

/**
   The attribute is not returned by default
*/
#define LDB_ATTR_FLAG_HIDDEN       (1<<0) 

/* the attribute handler name should be freed when released */
#define LDB_ATTR_FLAG_ALLOCATED    (1<<1) 

/**
   The attribute is supplied by the application and should not be removed
*/
#define LDB_ATTR_FLAG_FIXED        (1<<2) 

/*
  when this is set, attempts to create two records which have the same
  value for this attribute will return LDB_ERR_ENTRY_ALREADY_EXISTS
 */
#define LDB_ATTR_FLAG_UNIQUE_INDEX (1<<3)

/*
  when this is set, attempts to create two attribute values for this attribute on a single DN will return LDB_ERR_CONSTRAINT_VIOLATION
 */
#define LDB_ATTR_FLAG_SINGLE_VALUE (1<<4)

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
  LDAP attribute syntax for a boolean

  This is the well-known LDAP attribute syntax for a boolean.

  See <a href="http://www.ietf.org/rfc/rfc2252.txt">RFC 2252</a>, Section 4.3.2 
*/
#define LDB_SYNTAX_BOOLEAN              "1.3.6.1.4.1.1466.115.121.1.7"

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

/* Individual controls */

/**
  OID for getting and manipulating attributes from the ldb
  without interception in the operational module.
  It can be used to access attribute that used to be stored in the sam 
  and that are now calculated.
*/
#define LDB_CONTROL_BYPASS_OPERATIONAL_OID "1.3.6.1.4.1.7165.4.3.13"
#define LDB_CONTROL_BYPASS_OPERATIONAL_NAME "bypassoperational"

/**
  OID for recalculate SD control. This control force the
  dsdb code to recalculate the SD of the object as if the
  object was just created.

*/
#define LDB_CONTROL_RECALCULATE_SD_OID "1.3.6.1.4.1.7165.4.3.5"
#define LDB_CONTROL_RECALCULATE_SD_NAME "recalculate_sd"

/**
   REVEAL_INTERNALS is used to reveal internal attributes and DN
   components which are not normally shown to the user
*/
#define LDB_CONTROL_REVEAL_INTERNALS "1.3.6.1.4.1.7165.4.3.6"
#define LDB_CONTROL_REVEAL_INTERNALS_NAME	"reveal_internals"

/**
   LDB_CONTROL_AS_SYSTEM is used to skip access checks on operations
   that are performed by the system, but with a user's credentials, e.g.
   updating prefix map
*/
#define LDB_CONTROL_AS_SYSTEM_OID "1.3.6.1.4.1.7165.4.3.7"

/**
   LDB_CONTROL_PROVISION_OID is used to skip some constraint checks. It's is
   mainly thought to be used for the provisioning.
*/
#define LDB_CONTROL_PROVISION_OID "1.3.6.1.4.1.7165.4.3.16"
#define LDB_CONTROL_PROVISION_NAME	"provision"

/* AD controls */

/**
   OID for the paged results control. This control is included in the
   searchRequest and searchResultDone messages as part of the controls
   field of the LDAPMessage, as defined in Section 4.1.12 of
   LDAP v3. 

   \sa <a href="http://www.ietf.org/rfc/rfc2696.txt">RFC 2696</a>.
*/
#define LDB_CONTROL_PAGED_RESULTS_OID	"1.2.840.113556.1.4.319"
#define LDB_CONTROL_PAGED_RESULTS_NAME	"paged_result"

/**
   OID for specifying the returned elements of the ntSecurityDescriptor

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_sd_flags_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SD_FLAGS_OID	"1.2.840.113556.1.4.801"
#define LDB_CONTROL_SD_FLAGS_NAME	"sd_flags"

/**
   OID for specifying an advanced scope for the search (one partition)

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_domain_scope_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_DOMAIN_SCOPE_OID	"1.2.840.113556.1.4.1339"
#define LDB_CONTROL_DOMAIN_SCOPE_NAME	"domain_scope"

/**
   OID for specifying an advanced scope for a search

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_search_options_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SEARCH_OPTIONS_OID	"1.2.840.113556.1.4.1340"
#define LDB_CONTROL_SEARCH_OPTIONS_NAME	"search_options"

/**
   OID for notification

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_notification_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_NOTIFICATION_OID	"1.2.840.113556.1.4.528"
#define LDB_CONTROL_NOTIFICATION_NAME	"notification"

/**
   OID for performing subtree deletes

   \sa <a href="http://msdn.microsoft.com/en-us/library/aa366991(v=VS.85).aspx">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_TREE_DELETE_OID	"1.2.840.113556.1.4.805"
#define LDB_CONTROL_TREE_DELETE_NAME	"tree_delete"

/**
   OID for getting deleted objects

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_show_deleted_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SHOW_DELETED_OID	"1.2.840.113556.1.4.417"
#define LDB_CONTROL_SHOW_DELETED_NAME	"show_deleted"

/**
   OID for getting recycled objects

   \sa <a href="http://msdn.microsoft.com/en-us/library/dd304621(PROT.13).aspx">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SHOW_RECYCLED_OID         "1.2.840.113556.1.4.2064"
#define LDB_CONTROL_SHOW_RECYCLED_NAME	"show_recycled"

/**
   OID for getting deactivated linked attributes

   \sa <a href="http://msdn.microsoft.com/en-us/library/dd302781(PROT.13).aspx">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SHOW_DEACTIVATED_LINK_OID "1.2.840.113556.1.4.2065"
#define LDB_CONTROL_SHOW_DEACTIVATED_LINK_NAME	"show_deactivated_link"

/**
   OID for extended DN

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_extended_dn_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_EXTENDED_DN_OID	"1.2.840.113556.1.4.529"
#define LDB_CONTROL_EXTENDED_DN_NAME	"extended_dn"

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
#define LDB_CONTROL_SERVER_SORT_NAME	"server_sort"

/**
   OID for LDAP server sort result response extension.

   This control is included in the searchResultDone message as part of
   the controls field of the LDAPMessage, as defined in Section 4.1.12 of
   LDAP v3.

   \sa <a href="http://www.ietf.org/rfc/rfc2891.txt">RFC 2891</a>.
*/
#define LDB_CONTROL_SORT_RESP_OID	"1.2.840.113556.1.4.474"
#define LDB_CONTROL_SORT_RESP_NAME	"server_sort_resp"

/**
   OID for LDAP Attribute Scoped Query extension.

   This control is included in SearchRequest or SearchResponse
   messages as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_ASQ_OID		"1.2.840.113556.1.4.1504"
#define LDB_CONTROL_ASQ_NAME	"asq"

/**
   OID for LDAP Directory Sync extension. 

   This control is included in SearchRequest or SearchResponse
   messages as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_DIRSYNC_OID		"1.2.840.113556.1.4.841"
#define LDB_CONTROL_DIRSYNC_NAME	"dirsync"


/**
   OID for LDAP Virtual List View Request extension.

   This control is included in SearchRequest messages
   as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_VLV_REQ_OID		"2.16.840.1.113730.3.4.9"
#define LDB_CONTROL_VLV_REQ_NAME	"vlv"

/**
   OID for LDAP Virtual List View Response extension.

   This control is included in SearchResponse messages
   as part of the controls field of the LDAPMessage.
*/
#define LDB_CONTROL_VLV_RESP_OID	"2.16.840.1.113730.3.4.10"
#define LDB_CONTROL_VLV_RESP_NAME	"vlv_resp"

/**
   OID to let modifies don't give an error when adding an existing
   attribute with the same value or deleting an nonexisting one attribute

   \sa <a href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ldap/ldap/ldap_server_permissive_modify_oid.asp">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_PERMISSIVE_MODIFY_OID	"1.2.840.113556.1.4.1413"
#define LDB_CONTROL_PERMISSIVE_MODIFY_NAME	"permissive_modify"

/** 
    OID to allow the server to be more 'fast and loose' with the data being added.  

    \sa <a href="http://msdn.microsoft.com/en-us/library/aa366982(v=VS.85).aspx">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_SERVER_LAZY_COMMIT   "1.2.840.113556.1.4.619"

/**
   Control for RODC join -see [MS-ADTS] section 3.1.1.3.4.1.23

   \sa <a href="">Microsoft documentation of this OID</a>
*/
#define LDB_CONTROL_RODC_DCPROMO_OID "1.2.840.113556.1.4.1341"
#define LDB_CONTROL_RODC_DCPROMO_NAME	"rodc_join"

/* Other standardised controls */

/**
   OID for the allowing client to request temporary relaxed
   enforcement of constraints of the x.500 model.

   Mainly used for the OpenLDAP backend.

   \sa <a href="http://opends.dev.java.net/public/standards/draft-zeilenga-ldap-managedit.txt">draft managedit</a>.
*/
#define LDB_CONTROL_RELAX_OID "1.3.6.1.4.1.4203.666.5.12"
#define LDB_CONTROL_RELAX_NAME	"relax"

/* Extended operations */

/**
   OID for LDAP Extended Operation SEQUENCE_NUMBER

   This extended operation is used to retrieve the extended sequence number.
*/
#define LDB_EXTENDED_SEQUENCE_NUMBER	"1.3.6.1.4.1.7165.4.4.3"

/**
   OID for LDAP Extended Operation PASSWORD_CHANGE.

   This Extended operation is used to allow user password changes by the user
   itself.
*/
#define LDB_EXTENDED_PASSWORD_CHANGE_OID	"1.3.6.1.4.1.4203.1.11.1"


/**
   OID for LDAP Extended Operation FAST_BIND

   This Extended operations is used to perform a fast bind.
*/
#define LDB_EXTENDED_FAST_BIND_OID	"1.2.840.113556.1.4.1781"

/**
   OID for LDAP Extended Operation START_TLS.

   This Extended operation is used to start a new TLS channel on top of a clear
   text channel.
*/
#define LDB_EXTENDED_START_TLS_OID	"1.3.6.1.4.1.1466.20037"

/**
   OID for LDAP Extended Operation DYNAMIC_REFRESH.

   This Extended operation is used to create and maintain objects which exist
   only a specific time, e.g. when a certain client or a certain person is
   logged in. Data refreshes have to be periodically sent in a specific
   interval. Otherwise the entry is going to be removed.
*/
#define LDB_EXTENDED_DYNAMIC_OID	"1.3.6.1.4.1.1466.101.119.1"

struct ldb_sd_flags_control {
	/*
	 * request the owner	0x00000001
	 * request the group	0x00000002
	 * request the DACL	0x00000004
	 * request the SACL	0x00000008
	 */
	unsigned secinfo_flags;
};

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

#define LDB_SEARCH_OPTION_DOMAIN_SCOPE 0x00000001
#define LDB_SEARCH_OPTION_PHANTOM_ROOT 0x00000002

struct ldb_search_options_control {
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
	const char *attributeName;
	const char *orderingRule;
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
	LDB_REQ_REGISTER_PARTITION
};

enum ldb_reply_type {
	LDB_REPLY_ENTRY,
	LDB_REPLY_REFERRAL,
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

struct ldb_extended {
	const char *oid;
	void *data; /* NULL or a valid talloc pointer! talloc_get_type() will be used on it */
};

enum ldb_sequence_type {
	LDB_SEQ_HIGHEST_SEQ,
	LDB_SEQ_HIGHEST_TIMESTAMP,
	LDB_SEQ_NEXT
};

#define LDB_SEQ_GLOBAL_SEQUENCE    0x01
#define LDB_SEQ_TIMESTAMP_SEQUENCE 0x02

struct ldb_seqnum_request {
	enum ldb_sequence_type type;
};

struct ldb_seqnum_result {
	uint64_t seq_num;
	uint32_t flags;
};

struct ldb_result {
	unsigned int count;
	struct ldb_message **msgs;
	struct ldb_extended *extended;
	struct ldb_control **controls;
	char **refs;
};

struct ldb_reply {
	int error;
	enum ldb_reply_type type;
	struct ldb_message *message;
	struct ldb_extended *response;
	struct ldb_control **controls;
	char *referral;
};

struct ldb_request;
struct ldb_handle;

struct ldb_search {
	struct ldb_dn *base;
	enum ldb_scope scope;
	struct ldb_parse_tree *tree;
	const char * const *attrs;
	struct ldb_result *res;
};

struct ldb_add {
	const struct ldb_message *message;
};

struct ldb_modify {
	const struct ldb_message *message;
};

struct ldb_delete {
	struct ldb_dn *dn;
};

struct ldb_rename {
	struct ldb_dn *olddn;
	struct ldb_dn *newdn;
};

struct ldb_register_control {
	const char *oid;
};

struct ldb_register_partition {
	struct ldb_dn *dn;
};

typedef int (*ldb_request_callback_t)(struct ldb_request *, struct ldb_reply *);

struct ldb_request {

	enum ldb_request_type operation;

	union {
		struct ldb_search search;
		struct ldb_add    add;
		struct ldb_modify mod;
		struct ldb_delete del;
		struct ldb_rename rename;
		struct ldb_extended extended;
		struct ldb_register_control reg_control;
		struct ldb_register_partition reg_partition;
	} op;

	struct ldb_control **controls;

	void *context;
	ldb_request_callback_t callback;

	int timeout;
	time_t starttime;
	struct ldb_handle *handle;
};

int ldb_request(struct ldb_context *ldb, struct ldb_request *request);
int ldb_request_done(struct ldb_request *req, int status);
bool ldb_request_is_done(struct ldb_request *req);

int ldb_modules_wait(struct ldb_handle *handle);
int ldb_wait(struct ldb_handle *handle, enum ldb_wait_type type);

int ldb_set_timeout(struct ldb_context *ldb, struct ldb_request *req, int timeout);
int ldb_set_timeout_from_prev_req(struct ldb_context *ldb, struct ldb_request *oldreq, struct ldb_request *newreq);
void ldb_set_create_perms(struct ldb_context *ldb, unsigned int perms);
void ldb_set_modules_dir(struct ldb_context *ldb, const char *path);
struct tevent_context;
void ldb_set_event_context(struct ldb_context *ldb, struct tevent_context *ev);
struct tevent_context * ldb_get_event_context(struct ldb_context *ldb);

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

  \return pointer to ldb_context that should be free'd (using talloc_free())
  at the end of the program.
*/
struct ldb_context *ldb_init(TALLOC_CTX *mem_ctx, struct tevent_context *ev_ctx);

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

typedef void (*ldb_async_timeout_fn) (void *);
typedef bool (*ldb_async_callback_fn) (void *);
typedef int (*ldb_async_ctx_add_op_fn)(void *, time_t, void *, ldb_async_timeout_fn, ldb_async_callback_fn);
typedef int (*ldb_async_ctx_wait_op_fn)(void *);

void ldb_async_ctx_set_private_data(struct ldb_context *ldb,
					void *private_data);
void ldb_async_ctx_set_add_op(struct ldb_context *ldb,
				ldb_async_ctx_add_op_fn add_op);
void ldb_async_ctx_set_wait_op(struct ldb_context *ldb,
				ldb_async_ctx_wait_op_fn wait_op);

int ldb_connect(struct ldb_context *ldb, const char *url, unsigned int flags, const char *options[]);

/*
  return an automatic basedn from the rootDomainNamingContext of the rootDSE
  This value have been set in an opaque pointer at connection time
*/
struct ldb_dn *ldb_get_root_basedn(struct ldb_context *ldb);

/*
  return an automatic basedn from the configurationNamingContext of the rootDSE
  This value have been set in an opaque pointer at connection time
*/
struct ldb_dn *ldb_get_config_basedn(struct ldb_context *ldb);

/*
  return an automatic basedn from the schemaNamingContext of the rootDSE
  This value have been set in an opaque pointer at connection time
*/
struct ldb_dn *ldb_get_schema_basedn(struct ldb_context *ldb);

/*
  return an automatic baseDN from the defaultNamingContext of the rootDSE
  This value have been set in an opaque pointer at connection time
*/
struct ldb_dn *ldb_get_default_basedn(struct ldb_context *ldb);

/**
  The default async search callback function 

  \param req the request we are callback of
  \param ares a single reply from the async core

  \return result code (LDB_SUCCESS on success, or a failure code)

  \note this function expects req->context to always be an struct ldb_result pointer
  AND a talloc context, this function will steal on the context each message
  from the ares reply passed on by the async core so that in the end all the
  messages will be in the context (ldb_result)  memory tree.
  Freeing the passed context (ldb_result tree) will free all the resources
  (the request need to be freed separately and the result doe not depend on the
  request that can be freed as sson as the search request is finished)
*/

int ldb_search_default_callback(struct ldb_request *req, struct ldb_reply *ares);

/**
  The default async extended operation callback function

  \param req the request we are callback of
  \param ares a single reply from the async core

  \return result code (LDB_SUCCESS on success, or a failure code)
*/
int ldb_op_default_callback(struct ldb_request *req, struct ldb_reply *ares);

int ldb_modify_default_callback(struct ldb_request *req, struct ldb_reply *ares);

/**
  Helper function to build a search request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param base the Base Distinguished Name for the query (use ldb_dn_new() for an empty one)
  \param scope the search scope for the query
  \param expression the search expression to use for this query
  \param attrs the search attributes for the query (pass NULL if none required)
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_search_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *base,
	       		enum ldb_scope scope,
			const char *expression,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

int ldb_build_search_req_ex(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *base,
			enum ldb_scope scope,
			struct ldb_parse_tree *tree,
			const char * const *attrs,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

/**
  Helper function to build an add request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param message contains the entry to be added 
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_add_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

/**
  Helper function to build a modify request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param message contains the entry to be modified
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_mod_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			const struct ldb_message *message,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

/**
  Helper function to build a delete request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param dn the DN to be deleted
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_del_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *dn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

/**
  Helper function to build a rename request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param olddn the old DN
  \param newdn the new DN
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/

int ldb_build_rename_req(struct ldb_request **ret_req,
			struct ldb_context *ldb,
			TALLOC_CTX *mem_ctx,
			struct ldb_dn *olddn,
			struct ldb_dn *newdn,
			struct ldb_control **controls,
			void *context,
			ldb_request_callback_t callback,
			struct ldb_request *parent);

/**
  Add a ldb_control to a ldb_request

  \param req the request struct where to add the control
  \param oid the object identifier of the control as string
  \param critical whether the control should be critical or not
  \param data a talloc pointer to the control specific data

  \return result code (LDB_SUCCESS on success, or a failure code)
*/
int ldb_request_add_control(struct ldb_request *req, const char *oid, bool critical, void *data);

/**
  replace a ldb_control in a ldb_request

  \param req the request struct where to add the control
  \param oid the object identifier of the control as string
  \param critical whether the control should be critical or not
  \param data a talloc pointer to the control specific data

  \return result code (LDB_SUCCESS on success, or a failure code)
*/
int ldb_request_replace_control(struct ldb_request *req, const char *oid, bool critical, void *data);

/**
   check if a control with the specified "oid" exist and return it 
  \param req the request struct where to add the control
  \param oid the object identifier of the control as string

  \return the control, NULL if not found 
*/
struct ldb_control *ldb_request_get_control(struct ldb_request *req, const char *oid);

/**
   check if a control with the specified "oid" exist and return it 
  \param rep the reply struct where to add the control
  \param oid the object identifier of the control as string

  \return the control, NULL if not found 
*/
struct ldb_control *ldb_reply_get_control(struct ldb_reply *rep, const char *oid);

/**
  Search the database

  This function searches the database, and returns 
  records that match an LDAP-like search expression

  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx the memory context to use for the request and the results
  \param result the return result
  \param base the Base Distinguished Name for the query (use ldb_dn_new() for an empty one)
  \param scope the search scope for the query
  \param attrs the search attributes for the query (pass NULL if none required)
  \param exp_fmt the search expression to use for this query (printf like)

  \return result code (LDB_SUCCESS on success, or a failure code)

  \note use talloc_free() to free the ldb_result returned
*/
int ldb_search(struct ldb_context *ldb, TALLOC_CTX *mem_ctx,
	       struct ldb_result **result, struct ldb_dn *base,
	       enum ldb_scope scope, const char * const *attrs,
	       const char *exp_fmt, ...) PRINTF_ATTRIBUTE(7,8);

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
int ldb_rename(struct ldb_context *ldb, struct ldb_dn *olddn, struct ldb_dn *newdn);

/**
  Delete a record from the database

  This function deletes a record from the database.

  \param ldb the context associated with the database (from
  ldb_init())
  \param dn the DN for the record to be deleted.

  \return result code (LDB_SUCCESS if the record was deleted,
  otherwise a failure code)
*/
int ldb_delete(struct ldb_context *ldb, struct ldb_dn *dn);

/**
  The default async extended operation callback function 

  \param req the request we are callback of
  \param ares a single reply from the async core

  \return result code (LDB_SUCCESS on success, or a failure code)

  \note this function expects req->context to always be an struct ldb_result pointer
  AND a talloc context, this function will steal on the context each message
  from the ares reply passed on by the async core so that in the end all the
  messages will be in the context (ldb_result)  memory tree.
  Freeing the passed context (ldb_result tree) will free all the resources
  (the request need to be freed separately and the result doe not depend on the
  request that can be freed as sson as the search request is finished)
*/

int ldb_extended_default_callback(struct ldb_request *req, struct ldb_reply *ares);


/**
  Helper function to build a extended request

  \param ret_req the request structure is returned here (talloced on mem_ctx)
  \param ldb the context associated with the database (from ldb_init())
  \param mem_ctx a talloc memory context (used as parent of ret_req)
  \param oid the OID of the extended operation.
  \param data a void pointer a the extended operation specific parameters,
  it needs to be NULL or a valid talloc pointer! talloc_get_type() will be used on it  
  \param controls an array of controls
  \param context the callback function context
  \param the callback function to handle the async replies
  \param the parent request if any

  \return result code (LDB_SUCCESS on success, or a failure code)
*/
int ldb_build_extended_req(struct ldb_request **ret_req,
			   struct ldb_context *ldb,
			   TALLOC_CTX *mem_ctx,
			   const char *oid,
			   void *data,/* NULL or a valid talloc pointer! talloc_get_type() will be used on it */
			   struct ldb_control **controls,
			   void *context,
			   ldb_request_callback_t callback,
			   struct ldb_request *parent);

/**
  call an extended operation

  This function deletes a record from the database.

  \param ldb the context associated with the database (from ldb_init())
  \param oid the OID of the extended operation.
  \param data a void pointer a the extended operation specific parameters,
  it needs to be NULL or a valid talloc pointer! talloc_get_type() will be used on it  
  \param res the result of the extended operation

  \return result code (LDB_SUCCESS if the extended operation returned fine,
  otherwise a failure code)
*/
int ldb_extended(struct ldb_context *ldb, 
		 const char *oid,
		 void *data,/* NULL or a valid talloc pointer! talloc_get_type() will be used on it */
		 struct ldb_result **res);

/**
  Obtain current/next database sequence number
*/
int ldb_sequence_number(struct ldb_context *ldb, enum ldb_sequence_type type, uint64_t *seq_num);

/**
  start a transaction
*/
int ldb_transaction_start(struct ldb_context *ldb);

/**
   first phase of two phase commit
 */
int ldb_transaction_prepare_commit(struct ldb_context *ldb);

/**
  commit a transaction
*/
int ldb_transaction_commit(struct ldb_context *ldb);

/**
  cancel a transaction
*/
int ldb_transaction_cancel(struct ldb_context *ldb);

/*
  cancel a transaction with no error if no transaction is pending
  used when we fork() to clear any parent transactions
*/
int ldb_transaction_cancel_noerr(struct ldb_context *ldb);


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
char *ldb_casefold(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *s, size_t n);

/**
   Check the attribute name is valid according to rfc2251
   \param s the string to check

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
   Write an LDIF message to a string

   \param ldb the ldb context (from ldb_init())
   \param mem_ctx the talloc context on which to attach the string)
   \param msg the message to write out

   \return the string containing the LDIF, or NULL on error

   \sa ldb_ldif_read_string for the reader equivalent to this function.
*/
char * ldb_ldif_write_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, 
			  const struct ldb_ldif *msg);


/*
   Produce a string form of an ldb message

   convenient function to turn a ldb_message into a string. Useful for
   debugging
 */
char *ldb_ldif_message_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, 
			      enum ldb_changetype changetype,
			      const struct ldb_message *msg);


/**
   Base64 encode a buffer

   \param mem_ctx the memory context that the result is allocated
   from. 
   \param buf pointer to the array that is to be encoded
   \param len the number of elements in the array to be encoded

   \return pointer to an array containing the encoded data

   \note The caller is responsible for freeing the result
*/
char *ldb_base64_encode(TALLOC_CTX *mem_ctx, const char *buf, int len);

/**
   Base64 decode a buffer

   This function decodes a base64 encoded string in place.

   \param s the string to decode.

   \return the length of the returned (decoded) string.

   \note the string is null terminated, but the null terminator is not
   included in the length.
*/
int ldb_base64_decode(char *s);

/* The following definitions come from lib/ldb/common/ldb_dn.c  */

/**
  Get the linear form of a DN (without any extended components)
  
  \param dn The DN to linearize
*/

const char *ldb_dn_get_linearized(struct ldb_dn *dn);

/**
  Allocate a copy of the linear form of a DN (without any extended components) onto the supplied memory context 
  
  \param dn The DN to linearize
  \param mem_ctx TALLOC context to return result on
*/

char *ldb_dn_alloc_linearized(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);

/**
  Get the linear form of a DN (with any extended components)
  
  \param mem_ctx TALLOC context to return result on
  \param dn The DN to linearize
  \param mode Style of extended DN to return (0 is HEX representation of binary form, 1 is a string form)
*/
char *ldb_dn_get_extended_linearized(TALLOC_CTX *mem_ctx, struct ldb_dn *dn, int mode);
const struct ldb_val *ldb_dn_get_extended_component(struct ldb_dn *dn, const char *name);
int ldb_dn_set_extended_component(struct ldb_dn *dn, const char *name, const struct ldb_val *val);
void ldb_dn_extended_filter(struct ldb_dn *dn, const char * const *accept_list);
void ldb_dn_remove_extended_components(struct ldb_dn *dn);
bool ldb_dn_has_extended(struct ldb_dn *dn);

int ldb_dn_extended_add_syntax(struct ldb_context *ldb, 
			       unsigned flags,
			       const struct ldb_dn_extended_syntax *syntax);

/** 
  Allocate a new DN from a string

  \param mem_ctx TALLOC context to return resulting ldb_dn structure on
  \param dn The new DN 

  \note The DN will not be parsed at this time.  Use ldb_dn_validate to tell if the DN is syntacticly correct
*/

struct ldb_dn *ldb_dn_new(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, const char *dn);
/** 
  Allocate a new DN from a printf style format string and arguments

  \param mem_ctx TALLOC context to return resulting ldb_dn structure on
  \param new_fms The new DN as a format string (plus arguments)

  \note The DN will not be parsed at this time.  Use ldb_dn_validate to tell if the DN is syntacticly correct
*/

struct ldb_dn *ldb_dn_new_fmt(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, const char *new_fmt, ...) PRINTF_ATTRIBUTE(3,4);
/** 
  Allocate a new DN from a struct ldb_val (useful to avoid buffer overrun)

  \param mem_ctx TALLOC context to return resulting ldb_dn structure on
  \param dn The new DN 

  \note The DN will not be parsed at this time.  Use ldb_dn_validate to tell if the DN is syntacticly correct
*/

struct ldb_dn *ldb_dn_from_ldb_val(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, const struct ldb_val *strdn);

/**
  Determine if this DN is syntactically valid 

  \param dn The DN to validate
*/

bool ldb_dn_validate(struct ldb_dn *dn);

char *ldb_dn_escape_value(TALLOC_CTX *mem_ctx, struct ldb_val value);
const char *ldb_dn_get_casefold(struct ldb_dn *dn);
char *ldb_dn_alloc_casefold(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);

int ldb_dn_compare_base(struct ldb_dn *base, struct ldb_dn *dn);
int ldb_dn_compare(struct ldb_dn *edn0, struct ldb_dn *edn1);

bool ldb_dn_add_base(struct ldb_dn *dn, struct ldb_dn *base);
bool ldb_dn_add_base_fmt(struct ldb_dn *dn, const char *base_fmt, ...) PRINTF_ATTRIBUTE(2,3);
bool ldb_dn_add_child(struct ldb_dn *dn, struct ldb_dn *child);
bool ldb_dn_add_child_fmt(struct ldb_dn *dn, const char *child_fmt, ...) PRINTF_ATTRIBUTE(2,3);
bool ldb_dn_remove_base_components(struct ldb_dn *dn, unsigned int num);
bool ldb_dn_remove_child_components(struct ldb_dn *dn, unsigned int num);

struct ldb_dn *ldb_dn_copy(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);
struct ldb_dn *ldb_dn_get_parent(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);
char *ldb_dn_canonical_string(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);
char *ldb_dn_canonical_ex_string(TALLOC_CTX *mem_ctx, struct ldb_dn *dn);
int ldb_dn_get_comp_num(struct ldb_dn *dn);
int ldb_dn_get_extended_comp_num(struct ldb_dn *dn);
const char *ldb_dn_get_component_name(struct ldb_dn *dn, unsigned int num);
const struct ldb_val *ldb_dn_get_component_val(struct ldb_dn *dn, unsigned int num);
const char *ldb_dn_get_rdn_name(struct ldb_dn *dn);
const struct ldb_val *ldb_dn_get_rdn_val(struct ldb_dn *dn);
int ldb_dn_set_component(struct ldb_dn *dn, int num, const char *name, const struct ldb_val val);

bool ldb_dn_is_valid(struct ldb_dn *dn);
bool ldb_dn_is_special(struct ldb_dn *dn);
bool ldb_dn_check_special(struct ldb_dn *dn, const char *check);
bool ldb_dn_is_null(struct ldb_dn *dn);
int ldb_dn_update_components(struct ldb_dn *dn, const struct ldb_dn *ref_dn);


/**
   Compare two attributes

   This function compares to attribute names. Note that this is a
   case-insensitive comparison.

   \param a the first attribute name to compare
   \param b the second attribute name to compare

   \return 0 if the attribute names are the same, or only differ in
   case; non-zero if there are any differences

  attribute names are restricted by rfc2251 so using
  strcasecmp and toupper here is ok.
  return 0 for match
*/
#define ldb_attr_cmp(a, b) strcasecmp(a, b)
char *ldb_attr_casefold(TALLOC_CTX *mem_ctx, const char *s);
int ldb_attr_dn(const char *attr);

/**
   Create an empty message

   \param mem_ctx the memory context to create in. You can pass NULL
   to get the top level context, however the ldb context (from
   ldb_init()) may be a better choice
*/
struct ldb_message *ldb_msg_new(TALLOC_CTX *mem_ctx);

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
int ldb_msg_add_linearized_dn(struct ldb_message *msg, const char *attr_name,
			      struct ldb_dn *dn);
int ldb_msg_add_fmt(struct ldb_message *msg, 
		    const char *attr_name, const char *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
   compare two message elements - return 0 on match
*/
int ldb_msg_element_compare(struct ldb_message_element *el1, 
			    struct ldb_message_element *el2);
int ldb_msg_element_compare_name(struct ldb_message_element *el1, 
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

struct ldb_dn *ldb_msg_find_attr_as_dn(struct ldb_context *ldb,
				       TALLOC_CTX *mem_ctx,
				       const struct ldb_message *msg,
				       const char *attr_name);

void ldb_msg_sort_elements(struct ldb_message *msg);

struct ldb_message *ldb_msg_copy_shallow(TALLOC_CTX *mem_ctx, 
					 const struct ldb_message *msg);
struct ldb_message *ldb_msg_copy(TALLOC_CTX *mem_ctx, 
				 const struct ldb_message *msg);

/*
 * ldb_msg_canonicalize() is now depreciated
 * Please use ldb_msg_normalize() instead
 *
 * NOTE: Returned ldb_message object is allocated
 * into *ldb's context. Callers are recommended
 * to steal the returned object into a TALLOC_CTX
 * with short lifetime.
 */
struct ldb_message *ldb_msg_canonicalize(struct ldb_context *ldb, 
					 const struct ldb_message *msg) _DEPRECATED_;

int ldb_msg_normalize(struct ldb_context *ldb,
		      TALLOC_CTX *mem_ctx,
		      const struct ldb_message *msg,
		      struct ldb_message **_msg_out);


/*
 * ldb_msg_diff() is now depreciated
 * Please use ldb_msg_difference() instead
 *
 * NOTE: Returned ldb_message object is allocated
 * into *ldb's context. Callers are recommended
 * to steal the returned object into a TALLOC_CTX
 * with short lifetime.
 */
struct ldb_message *ldb_msg_diff(struct ldb_context *ldb, 
				 struct ldb_message *msg1,
				 struct ldb_message *msg2) _DEPRECATED_;

/**
 * return a ldb_message representing the differences between msg1 and msg2.
 * If you then use this in a ldb_modify() call,
 * it can be used to save edits to a message
 *
 * Result message is constructed as follows:
 * - LDB_FLAG_MOD_ADD     - elements found only in msg2
 * - LDB_FLAG_MOD_REPLACE - elements in msg2 that have
 * 			    different value in msg1
 *                          Value for msg2 element is used
 * - LDB_FLAG_MOD_DELETE  - elements found only in msg2
 *
 * @return LDB_SUCCESS or LDB_ERR_OPERATIONS_ERROR
 */
int ldb_msg_difference(struct ldb_context *ldb,
		       TALLOC_CTX *mem_ctx,
		       struct ldb_message *msg1,
		       struct ldb_message *msg2,
		       struct ldb_message **_msg_out);

/**
   Tries to find a certain string attribute in a message

   \param msg the message to check
   \param name attribute name
   \param value attribute value

   \return 1 on match and 0 otherwise.
*/
int ldb_msg_check_string_attribute(const struct ldb_message *msg,
				   const char *name,
				   const char *value);

/**
   Integrity check an ldb_message

   This function performs basic sanity / integrity checks on an
   ldb_message.

   \param ldb context in which to perform the checks
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
struct ldb_val ldb_val_dup(TALLOC_CTX *mem_ctx, const struct ldb_val *v);

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
		      char *(*casefold)(void *, void *, const char *, size_t n));

/**
   this sets up debug to print messages on stderr
*/
int ldb_set_debug_stderr(struct ldb_context *ldb);

/* control backend specific opaque values */
int ldb_set_opaque(struct ldb_context *ldb, const char *name, void *value);
void *ldb_get_opaque(struct ldb_context *ldb, const char *name);

const char **ldb_attr_list_copy(TALLOC_CTX *mem_ctx, const char * const *attrs);
const char **ldb_attr_list_copy_add(TALLOC_CTX *mem_ctx, const char * const *attrs, const char *new_attr);
int ldb_attr_in_list(const char * const *attrs, const char *attr);

int ldb_msg_rename_attr(struct ldb_message *msg, const char *attr, const char *replace);
int ldb_msg_copy_attr(struct ldb_message *msg, const char *attr, const char *replace);
void ldb_msg_remove_attr(struct ldb_message *msg, const char *attr);
void ldb_msg_remove_element(struct ldb_message *msg, struct ldb_message_element *el);


void ldb_parse_tree_attr_replace(struct ldb_parse_tree *tree, 
				 const char *attr, 
				 const char *replace);

/*
  shallow copy a tree - copying only the elements array so that the caller
  can safely add new elements without changing the message
*/
struct ldb_parse_tree *ldb_parse_tree_copy_shallow(TALLOC_CTX *mem_ctx,
						   const struct ldb_parse_tree *ot);

/**
   Convert a time structure to a string

   This function converts a time_t structure to an LDAP formatted
   GeneralizedTime string.
		
   \param mem_ctx the memory context to allocate the return string in
   \param t the time structure to convert

   \return the formatted string, or NULL if the time structure could
   not be converted
*/
char *ldb_timestring(TALLOC_CTX *mem_ctx, time_t t);

/**
   Convert a string to a time structure

   This function converts an LDAP formatted GeneralizedTime string
   to a time_t structure.

   \param s the string to convert

   \return the time structure, or 0 if the string cannot be converted
*/
time_t ldb_string_to_time(const char *s);

/**
  convert a LDAP GeneralizedTime string in ldb_val format to a
  time_t.
*/
int ldb_val_to_time(const struct ldb_val *v, time_t *t);

/**
   Convert a time structure to a string

   This function converts a time_t structure to an LDAP formatted
   UTCTime string.
		
   \param mem_ctx the memory context to allocate the return string in
   \param t the time structure to convert

   \return the formatted string, or NULL if the time structure could
   not be converted
*/
char *ldb_timestring_utc(TALLOC_CTX *mem_ctx, time_t t);

/**
   Convert a string to a time structure

   This function converts an LDAP formatted UTCTime string
   to a time_t structure.

   \param s the string to convert

   \return the time structure, or 0 if the string cannot be converted
*/
time_t ldb_string_utc_to_time(const char *s);


void ldb_qsort (void *const pbase, size_t total_elems, size_t size, void *opaque, ldb_qsort_cmp_fn_t cmp);

#ifndef discard_const
#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))
#endif

/*
  a wrapper around ldb_qsort() that ensures the comparison function is
  type safe. This will produce a compilation warning if the types
  don't match
 */
#define LDB_TYPESAFE_QSORT(base, numel, opaque, comparison)	\
do { \
	if (numel > 1) { \
		ldb_qsort(base, numel, sizeof((base)[0]), discard_const(opaque), (ldb_qsort_cmp_fn_t)comparison); \
		comparison(&((base)[0]), &((base)[1]), opaque);		\
	} \
} while (0)

/* allow ldb to also call TYPESAFE_QSORT() */
#ifndef TYPESAFE_QSORT
#define TYPESAFE_QSORT(base, numel, comparison) \
do { \
	if (numel > 1) { \
		qsort(base, numel, sizeof((base)[0]), (int (*)(const void *, const void *))comparison); \
		comparison(&((base)[0]), &((base)[1])); \
	} \
} while (0)
#endif



/**
   Convert a control into its string representation.
   
   \param mem_ctx TALLOC context to return result on, and to allocate error_string on
   \param control A struct ldb_control to convert

   \return string representation of the control 
*/
char* ldb_control_to_string(TALLOC_CTX *mem_ctx, const struct ldb_control *control);
/**
   Convert a string representing a control into a ldb_control structure 
   
   \param ldb LDB context
   \param mem_ctx TALLOC context to return result on, and to allocate error_string on
   \param control_strings A string-formatted control

   \return a ldb_control element
*/
struct ldb_control *ldb_parse_control_from_string(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char *control_strings);
/**
   Convert an array of string represention of a control into an array of ldb_control structures 
   
   \param ldb LDB context
   \param mem_ctx TALLOC context to return result on, and to allocate error_string on
   \param control_strings Array of string-formatted controls

   \return array of ldb_control elements
*/
struct ldb_control **ldb_parse_control_strings(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, const char **control_strings);

/**
   return the ldb flags 
*/
unsigned int ldb_get_flags(struct ldb_context *ldb);

/* set the ldb flags */
void ldb_set_flags(struct ldb_context *ldb, unsigned flags);


struct ldb_dn *ldb_dn_binary_from_ldb_val(TALLOC_CTX *mem_ctx,
					  struct ldb_context *ldb,
					  const struct ldb_val *strdn);

int ldb_dn_get_binary(struct ldb_dn *dn, struct ldb_val *val);
int ldb_dn_set_binary(struct ldb_dn *dn, struct ldb_val *val);

/* debugging functions for ldb requests */
void ldb_req_set_location(struct ldb_request *req, const char *location);
const char *ldb_req_location(struct ldb_request *req);

/* set the location marker on a request handle - used for debugging */
#define LDB_REQ_SET_LOCATION(req) ldb_req_set_location(req, __location__)

/*
  minimise a DN. The caller must pass in a validated DN.

  If the DN has an extended component then only the first extended
  component is kept, the DN string is stripped.

  The existing dn is modified
 */
bool ldb_dn_minimise(struct ldb_dn *dn);

#endif
