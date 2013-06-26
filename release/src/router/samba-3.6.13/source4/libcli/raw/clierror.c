/* 
   Unix SMB/CIFS implementation.
   client error handling routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"


/***************************************************************************
 Return an error message from the last response
****************************************************************************/
_PUBLIC_ const char *smbcli_errstr(struct smbcli_tree *tree)
{   
	switch (tree->session->transport->error.etype) {
	case ETYPE_SMB:
		return nt_errstr(tree->session->transport->error.e.nt_status);

	case ETYPE_SOCKET:
		return "socket_error";

	case ETYPE_NBT:
		return "nbt_error";

	case ETYPE_NONE:
		return "no_error";
	}
	return NULL;
}


/* Return the 32-bit NT status code from the last packet */
_PUBLIC_ NTSTATUS smbcli_nt_error(struct smbcli_tree *tree)
{
	switch (tree->session->transport->error.etype) {
	case ETYPE_SMB:
		return tree->session->transport->error.e.nt_status;

	case ETYPE_SOCKET:
		return NT_STATUS_UNSUCCESSFUL;

	case ETYPE_NBT:
		return NT_STATUS_UNSUCCESSFUL;

	case ETYPE_NONE:
		return NT_STATUS_OK;
	}

	return NT_STATUS_UNSUCCESSFUL;
}


/* Return true if the last packet was an error */
bool smbcli_is_error(struct smbcli_tree *tree)
{
	return NT_STATUS_IS_ERR(smbcli_nt_error(tree));
}
