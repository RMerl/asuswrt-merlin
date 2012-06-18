/* 
 *  Unix SMB/CIFS implementation.
 *  PAM error mapping functions
 *  Copyright (C) Andrew Bartlett 2002
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

#ifndef __AUTH_NTLM_PAM_ERRORS_H__
#define __AUTH_NTLM_PAM_ERRORS_H__

/*****************************************************************************
convert a PAM error to a NT status32 code
 *****************************************************************************/
NTSTATUS pam_to_nt_status(int pam_error);

/*****************************************************************************
convert an NT status32 code to a PAM error
 *****************************************************************************/
int nt_status_to_pam(NTSTATUS nt_status);

#endif /* __AUTH_NTLM_PAM_ERRORS_H__ */

