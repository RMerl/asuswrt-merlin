/* 
   Unix SMB/CIFS implementation.   
   Error handling code
   
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

#ifndef _SAMBA_ERROR_H_
#define _SAMBA_ERROR_H_

#include "libcli/util/werror.h"
#include "libcli/util/doserr.h"
#include "libcli/util/ntstatus.h"

/** NT error on DOS connection! (NT_STATUS_OK) */
bool ntstatus_dos_equal(NTSTATUS status1, NTSTATUS status2);

/*****************************************************************************
convert a NT status code to a dos class/code
 *****************************************************************************/
void ntstatus_to_dos(NTSTATUS ntstatus, uint8_t *eclass, uint32_t *ecode);

/*****************************************************************************
convert a WERROR to a NT status32 code
 *****************************************************************************/
NTSTATUS werror_to_ntstatus(WERROR error);

/*****************************************************************************
convert a NTSTATUS to a WERROR
 *****************************************************************************/
WERROR ntstatus_to_werror(NTSTATUS error);

/*********************************************************************
 Map an NT error code from a Unix error code.
*********************************************************************/
NTSTATUS map_nt_error_from_unix(int unix_error);

#endif /* _SAMBA_ERROR_H */
