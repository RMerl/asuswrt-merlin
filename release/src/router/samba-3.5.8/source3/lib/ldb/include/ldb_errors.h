/* 
   ldb database library

   Copyright (C) Simo Sorce  2005

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
 *  Description: defines error codes following RFC 2251 ldap error codes
 *
 *  Author: Simo Sorce
 */

#ifndef _LDB_ERRORS_H_

/*! \cond DOXYGEN_IGNORE */
#define _LDB_ERRORS_H_ 1
/*! \endcond */

/** 
   \file ldb_errors.h

   This header provides a set of result codes for LDB function calls.

   Many LDB function calls return an integer value (int). As shown in
   the function documentation, those return values may indicate
   whether the function call worked correctly (in which case it
   returns LDB_SUCCESS) or some problem occurred (in which case some
   other value will be returned). As a special case,
   LDB_ERR_COMPARE_FALSE or LDB_ERR_COMPARE_TRUE may be returned,
   which does not indicate an error.

   \note Not all error codes make sense for LDB, however they are
   based on the LDAP error codes, and are kept for reference and to
   avoid overlap.

   \note Some of this documentation is based on information in
   the OpenLDAP documentation, as developed and maintained by the
   <a href="http://www.openldap.org/">The OpenLDAP Project</a>.
 */

/**
   The function call succeeded.

   If a function returns LDB_SUCCESS, then that function, and the
   underlying transactions that may have been required, completed
   successfully.
*/
#define LDB_SUCCESS				0

/**
   The function call failed for some non-specific reason.
*/
#define LDB_ERR_OPERATIONS_ERROR		1

/**
   The function call failed because of a protocol violation.
*/
#define LDB_ERR_PROTOCOL_ERROR			2

/**
   The function call failed because a time limit was exceeded.
*/
#define LDB_ERR_TIME_LIMIT_EXCEEDED		3

/**
   The function call failed because a size limit was exceeded.
*/
#define LDB_ERR_SIZE_LIMIT_EXCEEDED		4

/**
   The function was for value comparison, and the comparison operation
   returned false.

   \note This is a status value, and doesn't normally indicate an
   error.
*/
#define LDB_ERR_COMPARE_FALSE			5

/**
   The function was for value comparison, and the comparison operation
   returned true.

   \note This is a status value, and doesn't normally indicate an
   error.
*/
#define LDB_ERR_COMPARE_TRUE			6

/**
   The function used an authentication method that is not supported by
   the database.
*/
#define LDB_ERR_AUTH_METHOD_NOT_SUPPORTED	7

/**
   The function call required a underlying operation that required
   strong authentication.

   This will normally only occur if you are using LDB with a LDAP
   backend.
*/
#define LDB_ERR_STRONG_AUTH_REQUIRED		8
/* 9 RESERVED */

/**
   The function resulted in a referral to another server.
*/
#define LDB_ERR_REFERRAL			10

/**
   The function failed because an administrative / policy limit was
   exceeded.
*/
#define LDB_ERR_ADMIN_LIMIT_EXCEEDED		11

/**
   The function required an extension or capability that the
   database cannot provide.
*/
#define LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION	12

/**
   The function involved a transaction or database operation that
   could not be performed without a secure link.
*/
#define LDB_ERR_CONFIDENTIALITY_REQUIRED	13

/**
   This is an intermediate result code for SASL bind operations that
   have more than one step.

   \note This is a result code that does not normally indicate an
   error has occurred.
*/
#define LDB_ERR_SASL_BIND_IN_PROGRESS		14

/**
   The function referred to an attribute type that is not present in
   the entry.
*/
#define LDB_ERR_NO_SUCH_ATTRIBUTE		16

/**
   The function referred to an attribute type that is invalid
*/
#define LDB_ERR_UNDEFINED_ATTRIBUTE_TYPE	17

/**
   The function required a filter type that is not available for the
   specified attribute.
*/
#define LDB_ERR_INAPPROPRIATE_MATCHING		18

/**
   The function would have violated an attribute constraint.
*/
#define LDB_ERR_CONSTRAINT_VIOLATION		19

/**
   The function involved an attribute type or attribute value that
   already exists in the entry.
*/
#define LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS	20
/**
   The function used an invalid (incorrect syntax) attribute value.
*/
#define LDB_ERR_INVALID_ATTRIBUTE_SYNTAX	21

/* 22-31 unused */

/**
   The function referred to an object that does not exist in the
   database.
*/
#define LDB_ERR_NO_SUCH_OBJECT			32

/**
   The function referred to an alias which points to a non-existant
   object in the database.
*/
#define LDB_ERR_ALIAS_PROBLEM			33

/**
   The function used a DN which was invalid (incorrect syntax).
*/
#define LDB_ERR_INVALID_DN_SYNTAX		34

/* 35 RESERVED */

/**
   The function required dereferencing of an alias, and something went
   wrong during the dereferencing process.
*/
#define LDB_ERR_ALIAS_DEREFERENCING_PROBLEM	36

/* 37-47 unused */

/**
   The function passed in the wrong authentication method.
*/
#define LDB_ERR_INAPPROPRIATE_AUTHENTICATION	48

/**
   The function passed in or referenced incorrect credentials during
   authentication. 
*/
#define LDB_ERR_INVALID_CREDENTIALS		49

/**
   The function required access permissions that the user does not
   possess. 
*/
#define LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS	50

/**
   The function required a transaction or call that the database could
   not perform because it is busy.
*/
#define LDB_ERR_BUSY				51

/**
   The function required a transaction or call to a database that is
   not available.
*/
#define LDB_ERR_UNAVAILABLE			52

/**
   The function required a transaction or call to a database that the
   database declined to perform.
*/
#define LDB_ERR_UNWILLING_TO_PERFORM		53

/**
   The function failed because it resulted in a loop being detected.
*/
#define LDB_ERR_LOOP_DETECT			54

/* 55-63 unused */

/**
   The function failed because it would have violated a naming rule.
*/
#define LDB_ERR_NAMING_VIOLATION		64

/**
   The function failed because it would have violated the schema.
*/
#define LDB_ERR_OBJECT_CLASS_VIOLATION		65

/**
   The function required an operation that is only allowed on leaf
   objects, but the object is not a leaf.
*/
#define LDB_ERR_NOT_ALLOWED_ON_NON_LEAF		66

/**
   The function required an operation that cannot be performed on a
   Relative DN, but the object is a Relative DN.
*/
#define LDB_ERR_NOT_ALLOWED_ON_RDN		67

/**
   The function failed because the entry already exists.
*/
#define LDB_ERR_ENTRY_ALREADY_EXISTS		68

/**
   The function failed because modifications to an object class are
   not allowable.
*/
#define LDB_ERR_OBJECT_CLASS_MODS_PROHIBITED	69

/* 70 RESERVED FOR CLDAP */

/**
   The function failed because it needed to be applied to multiple
   databases.
*/
#define LDB_ERR_AFFECTS_MULTIPLE_DSAS		71

/* 72-79 unused */

/**
   The function failed for unknown reasons.
*/
#define LDB_ERR_OTHER				80

/* 81-90 RESERVED for APIs */

#endif /* _LDB_ERRORS_H_ */
