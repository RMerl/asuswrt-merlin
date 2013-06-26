/* 
   Unix SMB/CIFS Implementation.
   LDAP protocol helper functions for SAMBA
   Copyright (C) Volker Lendecke 2004
    
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

#ifndef _SMB_LDAP_ERRORS_H_
#define _SMB_LDAP_ERRORS_H_

#ifndef LDAP_SUCCESS
enum ldap_result_code {
	LDAP_SUCCESS				= 0,
	LDAP_OPERATIONS_ERROR			= 1,
	LDAP_PROTOCOL_ERROR			= 2,
	LDAP_TIME_LIMIT_EXCEEDED		= 3,
	LDAP_SIZE_LIMIT_EXCEEDED		= 4,
	LDAP_COMPARE_FALSE			= 5,
	LDAP_COMPARE_TRUE			= 6,
	LDAP_AUTH_METHOD_NOT_SUPPORTED		= 7,
	LDAP_STRONG_AUTH_REQUIRED		= 8,
	LDAP_REFERRAL				= 10,
	LDAP_ADMIN_LIMIT_EXCEEDED		= 11,
	LDAP_UNAVAILABLE_CRITICAL_EXTENSION	= 12,
	LDAP_CONFIDENTIALITY_REQUIRED		= 13,
	LDAP_SASL_BIND_IN_PROGRESS		= 14,
	LDAP_NO_SUCH_ATTRIBUTE			= 16,
	LDAP_UNDEFINED_ATTRIBUTE_TYPE		= 17,
	LDAP_INAPPROPRIATE_MATCHING		= 18,
	LDAP_CONSTRAINT_VIOLATION		= 19,
	LDAP_ATTRIBUTE_OR_VALUE_EXISTS		= 20,
	LDAP_INVALID_ATTRIBUTE_SYNTAX		= 21,
	LDAP_NO_SUCH_OBJECT			= 32,
	LDAP_ALIAS_PROBLEM			= 33,
	LDAP_INVALID_DN_SYNTAX			= 34,
	LDAP_ALIAS_DEREFERENCING_PROBLEM	= 36,
	LDAP_INAPPROPRIATE_AUTHENTICATION	= 48,
	LDAP_INVALID_CREDENTIALS		= 49,
	LDAP_INSUFFICIENT_ACCESS_RIGHTS		= 50,
	LDAP_BUSY				= 51,
	LDAP_UNAVAILABLE			= 52,
	LDAP_UNWILLING_TO_PERFORM		= 53,
	LDAP_LOOP_DETECT			= 54,
	LDAP_NAMING_VIOLATION			= 64,
	LDAP_OBJECT_CLASS_VIOLATION		= 65,
	LDAP_NOT_ALLOWED_ON_NON_LEAF		= 66,
	LDAP_NOT_ALLOWED_ON_RDN			= 67,
	LDAP_ENTRY_ALREADY_EXISTS		= 68,
	LDAP_OBJECT_CLASS_MODS_PROHIBITED	= 69,
	LDAP_AFFECTS_MULTIPLE_DSAS		= 71,
	LDAP_OTHER 				= 80
};
#endif

#endif /* _SMB_LDAP_ERRORS_H_ */
