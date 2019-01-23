/*
   Unix SMB/CIFS implementation.
   simple kerberos5 routines for active directory
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Guenther Deschner 2005-2009

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

#ifndef _HEADER_smb_krb5_h
#define _HEADER_smb_krb5_h

#define KRB5_PRIVATE    1       /* this file uses PRIVATE interfaces! */
/* this file uses DEPRECATED interfaces! */

#if defined(HAVE_KRB5_DEPRECATED_WITH_IDENTIFIER)
#define KRB5_DEPRECATED 1
#else
#define KRB5_DEPRECATED
#endif

#if HAVE_KRB5_H
#include <krb5.h>
#endif

#if HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#elif HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#elif HAVE_GSSAPI_H
#include <gssapi.h>
#endif

#if HAVE_COM_ERR_H
#include <com_err.h>
#endif

#ifndef KRB5_ADDR_NETBIOS
#define KRB5_ADDR_NETBIOS 0x14
#endif

#ifndef KRB5KRB_ERR_RESPONSE_TOO_BIG
#define KRB5KRB_ERR_RESPONSE_TOO_BIG (-1765328332L)
#endif

/* Heimdal uses a slightly different name */
#if defined(HAVE_ENCTYPE_ARCFOUR_HMAC_MD5) && !defined(HAVE_ENCTYPE_ARCFOUR_HMAC)
#define ENCTYPE_ARCFOUR_HMAC ENCTYPE_ARCFOUR_HMAC_MD5
#endif

/* The older versions of heimdal that don't have this
   define don't seem to use it anyway.  I'm told they
   always use a subkey */
#ifndef HAVE_AP_OPTS_USE_SUBKEY
#define AP_OPTS_USE_SUBKEY 0
#endif

#ifdef HAVE_KRB5
typedef struct {
#if defined(HAVE_MAGIC_IN_KRB5_ADDRESS) && defined(HAVE_ADDRTYPE_IN_KRB5_ADDRESS) /* MIT */
	krb5_address **addrs;
#elif defined(HAVE_KRB5_ADDRESSES) /* Heimdal */
	krb5_addresses *addrs;
#else
#error UNKNOWN_KRB5_ADDRESS_TYPE
#endif /* defined(HAVE_MAGIC_IN_KRB5_ADDRESS) && defined(HAVE_ADDRTYPE_IN_KRB5_ADDRESS) */
} smb_krb5_addresses;

#ifdef HAVE_KRB5_KEYBLOCK_KEYVALUE /* Heimdal */
#define KRB5_KEY_TYPE(k)	((k)->keytype)
#define KRB5_KEY_LENGTH(k)	((k)->keyvalue.length)
#define KRB5_KEY_DATA(k)	((k)->keyvalue.data)
#define KRB5_KEY_DATA_CAST	void
#else /* MIT */
#define KRB5_KEY_TYPE(k)	((k)->enctype)
#define KRB5_KEY_LENGTH(k)	((k)->length)
#define KRB5_KEY_DATA(k)	((k)->contents)
#define KRB5_KEY_DATA_CAST	krb5_octet
#endif /* HAVE_KRB5_KEYBLOCK_KEYVALUE */

#ifdef HAVE_KRB5_KEYTAB_ENTRY_KEY               /* MIT */
#define KRB5_KT_KEY(k)		(&(k)->key)
#elif HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK          /* Heimdal */
#define KRB5_KT_KEY(k)		(&(k)->keyblock)
#else
#error krb5_keytab_entry has no key or keyblock member
#endif /* HAVE_KRB5_KEYTAB_ENTRY_KEY */

#endif /* HAVE_KRB5 */

#include "krb5_protos.h"

#endif /* _HEADER_smb_krb5_h */
