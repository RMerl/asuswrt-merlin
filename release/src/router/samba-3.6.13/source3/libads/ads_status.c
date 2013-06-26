/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001
   Copyright (C) Andrew Bartlett 2001
   
   
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
#include "smb_krb5.h"
#include "smb_ldap.h"
#include "libads/ads_status.h"

/*
  build a ADS_STATUS structure
*/
ADS_STATUS ads_build_error(enum ads_error_type etype, 
			   int rc, int minor_status)
{
	ADS_STATUS ret;

	if (etype == ENUM_ADS_ERROR_NT) {
		DEBUG(0,("don't use ads_build_error with ENUM_ADS_ERROR_NT!\n"));
		ret.err.rc = -1;
		ret.error_type = ENUM_ADS_ERROR_SYSTEM;
		ret.minor_status = 0;
		return ret;	
	}	
		
	ret.err.rc = rc;
	ret.error_type = etype;		
	ret.minor_status = minor_status;
	return ret;
}

ADS_STATUS ads_build_nt_error(enum ads_error_type etype, 
			   NTSTATUS nt_status)
{
	ADS_STATUS ret;

	if (etype != ENUM_ADS_ERROR_NT) {
		DEBUG(0,("don't use ads_build_nt_error without ENUM_ADS_ERROR_NT!\n"));
		ret.err.rc = -1;
		ret.error_type = ENUM_ADS_ERROR_SYSTEM;
		ret.minor_status = 0;
		return ret;	
	}
	ret.err.nt_status = nt_status;
	ret.error_type = etype;		
	ret.minor_status = 0;
	return ret;
}

/*
  do a rough conversion between ads error codes and NT status codes
  we'll need to fill this in more
*/
NTSTATUS ads_ntstatus(ADS_STATUS status)
{
	switch (status.error_type) {
	case ENUM_ADS_ERROR_NT:
		return status.err.nt_status;	
	case ENUM_ADS_ERROR_SYSTEM:
		return map_nt_error_from_unix(status.err.rc);
#ifdef HAVE_LDAP
	case ENUM_ADS_ERROR_LDAP:
		if (status.err.rc == LDAP_SUCCESS) {
			return NT_STATUS_OK;
		}
		if (status.err.rc == LDAP_TIMELIMIT_EXCEEDED) {
			return NT_STATUS_IO_TIMEOUT;
		}
		return NT_STATUS_LDAP(status.err.rc);
#endif
#ifdef HAVE_KRB5
	case ENUM_ADS_ERROR_KRB5:
		return krb5_to_nt_status(status.err.rc);
#endif
	default:
		break;
	}

	if (ADS_ERR_OK(status)) {
		return NT_STATUS_OK;
	}
	return NT_STATUS_UNSUCCESSFUL;
}

/*
  return a string for an error from a ads routine
*/
const char *ads_errstr(ADS_STATUS status)
{
	switch (status.error_type) {
	case ENUM_ADS_ERROR_SYSTEM:
		return strerror(status.err.rc);
#ifdef HAVE_LDAP
	case ENUM_ADS_ERROR_LDAP:
		return ldap_err2string(status.err.rc);
#endif
#ifdef HAVE_KRB5
	case ENUM_ADS_ERROR_KRB5: 
		return error_message(status.err.rc);
#endif
#ifdef HAVE_GSSAPI
	case ENUM_ADS_ERROR_GSS:
	{
		char *ret;
		uint32 msg_ctx;
		uint32 minor;
		gss_buffer_desc msg1, msg2;

		msg_ctx = 0;
		
		msg1.value = NULL;
		msg2.value = NULL;
		gss_display_status(&minor, status.err.rc, GSS_C_GSS_CODE,
				   GSS_C_NULL_OID, &msg_ctx, &msg1);
		gss_display_status(&minor, status.minor_status, GSS_C_MECH_CODE,
				   GSS_C_NULL_OID, &msg_ctx, &msg2);
		ret = talloc_asprintf(talloc_tos(), "%s : %s",
				      (char *)msg1.value, (char *)msg2.value);
		SMB_ASSERT(ret != NULL);
		gss_release_buffer(&minor, &msg1);
		gss_release_buffer(&minor, &msg2);
		return ret;
	}
#endif
	case ENUM_ADS_ERROR_NT: 
		return get_friendly_nt_error_msg(ads_ntstatus(status));
	default:
		return "Unknown ADS error type!? (not compiled in?)";
	}
}

#ifdef HAVE_GSSAPI
NTSTATUS gss_err_to_ntstatus(uint32 maj, uint32 min)
{
        ADS_STATUS adss = ADS_ERROR_GSS(maj, min);
        DEBUG(10,("gss_err_to_ntstatus: Error %s\n",
                ads_errstr(adss) ));
        return ads_ntstatus(adss);
}
#endif
