#ifndef _system_kerberos_h
#define _system_kerberos_h

/* 
   Unix SMB/CIFS implementation.

   kerberos system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_KRB5
/* Whether the krb5_address struct has a addrtype property */
/* #undef HAVE_ADDRTYPE_IN_KRB5_ADDRESS */
/* Whether the krb5_address struct has a addr_type property */
#define HAVE_ADDR_TYPE_IN_KRB5_ADDRESS 1
/* Define to 1 if you have the `gsskrb5_extract_authz_data_from_sec_context' */
#define HAVE_GSSKRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT 1
/* Define to 1 if you have the `gsskrb5_get_initiator_subkey' function. */
#define HAVE_GSSKRB5_GET_INITIATOR_SUBKEY 1
/* Define to 1 if you have the `gsskrb5_register_acceptor_identity' function. */
#define HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY 1
/* Define to 1 if you have the `gss_krb5_ccache_name' function. */
#define HAVE_GSS_KRB5_CCACHE_NAME 1
/* Define to 1 if you have the `krb5_addlog_func' function. */
#define HAVE_KRB5_ADDLOG_FUNC 1
/* Define to 1 if you have the `krb5_auth_con_setkey' function. */
#define HAVE_KRB5_AUTH_CON_SETKEY 1
/* Define to 1 if you have the `krb5_auth_con_setuseruserkey' function. */
/* #undef HAVE_KRB5_AUTH_CON_SETUSERUSERKEY */
/* Define to 1 if you have the `krb5_c_enctype_compare' function. */
#define HAVE_KRB5_C_ENCTYPE_COMPARE 1
/* Define to 1 if you have the `krb5_c_verify_checksum' function. */
#define HAVE_KRB5_C_VERIFY_CHECKSUM 1
/* Whether the type krb5_encrypt_block exists */
/* #undef HAVE_KRB5_ENCRYPT_BLOCK */
/* Define to 1 if you have the `krb5_encrypt_data' function. */
/* #undef HAVE_KRB5_ENCRYPT_DATA */
/* Define to 1 if you have the `krb5_enctypes_compatible_keys' function. */
#define HAVE_KRB5_ENCTYPES_COMPATIBLE_KEYS 1
/* Define to 1 if you have the `krb5_free_data_contents' function. */
#define HAVE_KRB5_FREE_DATA_CONTENTS 1
/* Define to 1 if you have the `krb5_free_error_string' function. */
#define HAVE_KRB5_FREE_ERROR_STRING 1
/* Define to 1 if you have the `krb5_free_keytab_entry_contents' function. */
/* #undef HAVE_KRB5_FREE_KEYTAB_ENTRY_CONTENTS */
/* Define to 1 if you have the `krb5_free_ktypes' function. */
/* #undef HAVE_KRB5_FREE_KTYPES */
/* Define to 1 if you have the `krb5_free_unparsed_name' function. */
/* #undef HAVE_KRB5_FREE_UNPARSED_NAME */
/* Define to 1 if you have the `krb5_get_default_in_tkt_etypes' function. */
#define HAVE_KRB5_GET_DEFAULT_IN_TKT_ETYPES 1
/* Define to 1 if you have the `krb5_get_error_string' function. */
#define HAVE_KRB5_GET_ERROR_STRING 1
/* Define to 1 if you have the `krb5_get_permitted_enctypes' function. */
/* #undef HAVE_KRB5_GET_PERMITTED_ENCTYPES */
/* Define to 1 if you have the `krb5_get_pw_salt' function. */
#define HAVE_KRB5_GET_PW_SALT 1
/* Define to 1 if you have the <krb5.h> header file. */
#define HAVE_KRB5_H 1
/* Define to 1 if you have the `krb5_initlog' function. */
#define HAVE_KRB5_INITLOG 1
/* Define to 1 if you have the `krb5_kdc_default_config' function. */
#define HAVE_KRB5_KDC_DEFAULT_CONFIG 1
/* Whether the krb5_creds struct has a keyblock property */
/* #undef HAVE_KRB5_KEYBLOCK_IN_CREDS */
/* Whether the krb5_keyblock struct has a keyvalue property */
#define HAVE_KRB5_KEYBLOCK_KEYVALUE 1
/* Whether krb5_keytab_entry has key member */
/* #undef HAVE_KRB5_KEYTAB_ENTRY_KEY */
/* Whether krb5_keytab_entry has keyblock member */
#define HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK 1
/* Define to 1 if you have the `krb5_krbhst_get_addrinfo' function. */
#define HAVE_KRB5_KRBHST_GET_ADDRINFO 1
/* Define to 1 if you have the `krb5_kt_compare' function. */
#define HAVE_KRB5_KT_COMPARE 1
/* Define to 1 if you have the `krb5_kt_free_entry' function. */
#define HAVE_KRB5_KT_FREE_ENTRY 1
/* Whether the type krb5_log_facility exists */
#define HAVE_KRB5_LOG_FACILITY 1
/* Define to 1 if you have the `krb5_mk_req_extended' function. */
#define HAVE_KRB5_MK_REQ_EXTENDED 1
/* Define to 1 if you have the `krb5_principal2salt' function. */
/* #undef HAVE_KRB5_PRINCIPAL2SALT */
/* Define to 1 if you have the `krb5_principal_get_comp_string' function. */
#define HAVE_KRB5_PRINCIPAL_GET_COMP_STRING 1
/* Whether krb5_princ_component is available */
/* #undef HAVE_KRB5_PRINC_COMPONENT */
/* Whether the krb5_creds struct has a session property */
#define HAVE_KRB5_SESSION_IN_CREDS 1
/* Define to 1 if you have the `krb5_set_default_in_tkt_etypes' function. */
#define HAVE_KRB5_SET_DEFAULT_IN_TKT_ETYPES 1
/* Define to 1 if you have the `krb5_set_default_tgs_ktypes' function. */
/* #undef HAVE_KRB5_SET_DEFAULT_TGS_KTYPES */
/* Define to 1 if you have the `krb5_set_real_time' function. */
#define HAVE_KRB5_SET_REAL_TIME 1
/* Define to 1 if you have the `krb5_set_warn_dest' function. */
#define HAVE_KRB5_SET_WARN_DEST 1
/* Define to 1 if you have the `krb5_string_to_key' function. */
#define HAVE_KRB5_STRING_TO_KEY 1
/* Define to 1 if you have the `krb5_string_to_key_salt' function. */
#define HAVE_KRB5_STRING_TO_KEY_SALT 1
/* Define to 1 if you have the `krb5_ticket_get_authorization_data_type' */
#define HAVE_KRB5_TICKET_GET_AUTHORIZATION_DATA_TYPE 1
/* Whether the krb5_ticket struct has a enc_part2 property */
/* #undef HAVE_KRB5_TKT_ENC_PART2 */
/* Define to 1 if you have the `krb5_use_enctype' function. */
/* #undef HAVE_KRB5_USE_ENCTYPE */
/* Define to 1 if you have the `krb5_verify_checksum' function. */
#define HAVE_KRB5_VERIFY_CHECKSUM 1
/* Whether krb5_princ_realm returns krb5_realm or krb5_data */
#define KRB5_PRINC_REALM_RETURNS_REALM 1

#include "heimdal/lib/krb5/krb5.h"
#include "heimdal/lib/com_err/com_err.h"
#endif

#endif
