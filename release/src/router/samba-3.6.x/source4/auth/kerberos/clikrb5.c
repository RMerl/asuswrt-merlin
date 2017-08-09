/* 
   Unix SMB/CIFS implementation.
   simple kerberos5 routines for active directory
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
  
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

#include "includes.h"
#include "system/network.h"
#include "system/kerberos.h"
#include "system/time.h"
#include "auth/kerberos/kerberos.h"

#ifdef HAVE_KRB5

#if defined(HAVE_KRB5_PRINCIPAL2SALT) && defined(HAVE_KRB5_USE_ENCTYPE) && defined(HAVE_KRB5_STRING_TO_KEY) && defined(HAVE_KRB5_ENCRYPT_BLOCK)
 int create_kerberos_key_from_string(krb5_context context,
					krb5_principal host_princ,
					krb5_data *password,
					krb5_keyblock *key,
					krb5_enctype enctype)
{
	int ret;
	krb5_data salt;
	krb5_encrypt_block eblock;

	ret = krb5_principal2salt(context, host_princ, &salt);
	if (ret) {
		DEBUG(1,("krb5_principal2salt failed (%s)\n", error_message(ret)));
		return ret;
	}
	krb5_use_enctype(context, &eblock, enctype);
	ret = krb5_string_to_key(context, &eblock, key, password, &salt);
	SAFE_FREE(salt.data);
	return ret;
}
#elif defined(HAVE_KRB5_GET_PW_SALT) && defined(HAVE_KRB5_STRING_TO_KEY_SALT)
 int create_kerberos_key_from_string(krb5_context context,
					krb5_principal host_princ,
					krb5_data *password,
					krb5_keyblock *key,
					krb5_enctype enctype)
{
	int ret;
	krb5_salt salt;

	ret = krb5_get_pw_salt(context, host_princ, &salt);
	if (ret) {
		DEBUG(1,("krb5_get_pw_salt failed (%s)\n", error_message(ret)));
		return ret;
	}
	ret = krb5_string_to_key_salt(context, enctype, password->data,
				      salt, key);
	krb5_free_salt(context, salt);
	return ret;
}
#else
#error UNKNOWN_CREATE_KEY_FUNCTIONS
#endif

 void kerberos_free_data_contents(krb5_context context, krb5_data *pdata)
{
	if (pdata->data) {
		krb5_data_free(pdata);
	}
}

 krb5_error_code smb_krb5_kt_free_entry(krb5_context context, krb5_keytab_entry *kt_entry)
{
#if defined(HAVE_KRB5_KT_FREE_ENTRY)
	return krb5_kt_free_entry(context, kt_entry);
#elif defined(HAVE_KRB5_FREE_KEYTAB_ENTRY_CONTENTS)
	return krb5_free_keytab_entry_contents(context, kt_entry);
#else
#error UNKNOWN_KT_FREE_FUNCTION
#endif
}

 char *smb_get_krb5_error_message(krb5_context context, krb5_error_code code, TALLOC_CTX *mem_ctx) 
{
	char *ret;
	
#if defined(HAVE_KRB5_GET_ERROR_MESSAGE) && defined(HAVE_KRB5_FREE_ERROR_MESSAGE) 	
	const char *context_error = krb5_get_error_message(context, code);
	if (context_error) {
		ret = talloc_asprintf(mem_ctx, "%s: %s", error_message(code), context_error);
		krb5_free_error_message(context, context_error);
		return ret;
	}
#endif
	ret = talloc_strdup(mem_ctx, error_message(code));
	return ret;
}

#endif
