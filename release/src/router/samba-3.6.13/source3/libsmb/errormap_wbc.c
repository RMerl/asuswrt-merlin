/* 
 *  Unix SMB/CIFS implementation.
 *  error mapping functions
 *  Copyright (C) Andrew Tridgell 2001
 *  Copyright (C) Andrew Bartlett 2001
 *  Copyright (C) Tim Potter 2000
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "nsswitch/libwbclient/wbclient.h"

/*******************************************************************************
 Map between wbcErr and NT status.
*******************************************************************************/

static const struct {
	wbcErr wbc_err;
	NTSTATUS nt_status;
} wbcErr_ntstatus_map[] = {
	{ WBC_ERR_SUCCESS,		 NT_STATUS_OK },
	{ WBC_ERR_NOT_IMPLEMENTED,	 NT_STATUS_NOT_IMPLEMENTED },
	{ WBC_ERR_UNKNOWN_FAILURE,	 NT_STATUS_UNSUCCESSFUL },
	{ WBC_ERR_NO_MEMORY,		 NT_STATUS_NO_MEMORY },
	{ WBC_ERR_INVALID_SID,		 NT_STATUS_INVALID_SID },
	{ WBC_ERR_INVALID_PARAM,	 NT_STATUS_INVALID_PARAMETER },
	{ WBC_ERR_WINBIND_NOT_AVAILABLE, NT_STATUS_SERVER_DISABLED },
	{ WBC_ERR_DOMAIN_NOT_FOUND,	 NT_STATUS_NO_SUCH_DOMAIN },
	{ WBC_ERR_INVALID_RESPONSE,	 NT_STATUS_INVALID_NETWORK_RESPONSE },
	{ WBC_ERR_NSS_ERROR,		 NT_STATUS_INTERNAL_ERROR },
	{ WBC_ERR_AUTH_ERROR,		 NT_STATUS_LOGON_FAILURE },
	{ WBC_ERR_UNKNOWN_USER,		 NT_STATUS_NO_SUCH_USER },
	{ WBC_ERR_UNKNOWN_GROUP,	 NT_STATUS_NO_SUCH_GROUP },
	{ WBC_ERR_PWD_CHANGE_FAILED,	 NT_STATUS_PASSWORD_RESTRICTION }
};

NTSTATUS map_nt_error_from_wbcErr(wbcErr wbc_err)
{
	int i;

	/* Look through list */
	for (i=0;i<ARRAY_SIZE(wbcErr_ntstatus_map);i++) {
		if (wbcErr_ntstatus_map[i].wbc_err == wbc_err) {
			return wbcErr_ntstatus_map[i].nt_status;
		}
	}

	/* Default return */
	return NT_STATUS_UNSUCCESSFUL;
}

