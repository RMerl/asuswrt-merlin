/* 
   Unix SMB/CIFS implementation.
   client lpq operations
   Copyright (C) Tim Potter 2005
   
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

/****************************************************************************
 lpq - async send
****************************************************************************/
struct smbcli_request *smb_raw_lpq_send(struct smbcli_tree *tree,
					union smb_lpq *parms)
{
	return NULL;
}

/****************************************************************************
 lpq - async receive
****************************************************************************/
NTSTATUS smb_raw_lpq_recv(struct smbcli_request *req, union smb_lpq *parms)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/*
  lpq - sync interface
*/
NTSTATUS smb_raw_lpq(struct smbcli_tree *tree, union smb_lpq *parms)
{
	struct smbcli_request *req = smb_raw_lpq_send(tree, parms);
	return smb_raw_lpq_recv(req, parms);
}
