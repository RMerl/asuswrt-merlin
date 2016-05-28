/* 
   Unix SMB/CIFS implementation.
   krb5 set password implementation
   Copyright (C) Remus Koos 2001 (remuskoos@yahoo.com)
   
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

#include "includes.h"

#ifdef HAVE_KRB5

ADS_STATUS ads_change_trust_account_password(ADS_STRUCT *ads, char *host_principal)
{
	char *password;
	char *new_password;
	ADS_STATUS ret;
	uint32 sec_channel_type;
    
	if ((password = secrets_fetch_machine_password(lp_workgroup(), NULL, &sec_channel_type)) == NULL) {
		DEBUG(1,("Failed to retrieve password for principal %s\n", host_principal));
		return ADS_ERROR_SYSTEM(ENOENT);
	}

	new_password = generate_random_str(DEFAULT_TRUST_ACCOUNT_PASSWORD_LENGTH);
    
	ret = kerberos_set_password(ads->auth.kdc_server, host_principal, password, host_principal, new_password, ads->auth.time_offset);

	if (!ADS_ERR_OK(ret)) {
		goto failed;
	}

	if (!secrets_store_machine_password(new_password, lp_workgroup(), sec_channel_type)) {
		DEBUG(1,("Failed to save machine password\n"));
		ret = ADS_ERROR_SYSTEM(EACCES);
		goto failed;
	}

failed:
	SAFE_FREE(password);
	return ret;
}

ADS_STATUS ads_guess_service_principal(ADS_STRUCT *ads,
				       char **returned_principal)
{
	char *princ = NULL;

	if (ads->server.realm && ads->server.ldap_server) {
		char *server, *server_realm;

		server = SMB_STRDUP(ads->server.ldap_server);
		server_realm = SMB_STRDUP(ads->server.realm);

		if (!server || !server_realm) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		strlower_m(server);
		strupper_m(server_realm);
		asprintf(&princ, "ldap/%s@%s", server, server_realm);

		SAFE_FREE(server);
		SAFE_FREE(server_realm);

		if (!princ) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	} else if (ads->config.realm && ads->config.ldap_server_name) {
		char *server, *server_realm;

		server = SMB_STRDUP(ads->config.ldap_server_name);
		server_realm = SMB_STRDUP(ads->config.realm);

		if (!server || !server_realm) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}

		strlower_m(server);
		strupper_m(server_realm);
		asprintf(&princ, "ldap/%s@%s", server, server_realm);

		SAFE_FREE(server);
		SAFE_FREE(server_realm);

		if (!princ) {
			return ADS_ERROR(LDAP_NO_MEMORY);
		}
	}

	if (!princ) {
		return ADS_ERROR(LDAP_PARAM_ERROR);
	}

	*returned_principal = princ;

	return ADS_SUCCESS;
}

#endif
