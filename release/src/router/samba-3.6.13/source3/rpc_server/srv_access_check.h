/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell                   1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton      1996-1997,
 *  Copyright (C) Paul Ashton                       1997,
 *  Copyright (C) Marc Jacobsen			    1999,
 *  Copyright (C) Jeremy Allison                    2001-2008,
 *  Copyright (C) Jean François Micouleau           1998-2001,
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com>   2002,
 *  Copyright (C) Gerald (Jerry) Carter             2003-2004,
 *  Copyright (C) Simo Sorce                        2003.
 *  Copyright (C) Volker Lendecke		    2005.
 *  Copyright (C) Guenther Deschner		    2008.
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

#ifndef _RPC_SERVER_SRV_ACCESS_CHECK_H_
#define _RPC_SERVER_SRV_ACCESS_CHECK_H_

/* The following definitions come from rpc_server/srv_access_check.c */

NTSTATUS access_check_object( struct security_descriptor *psd, struct security_token *token,
			      enum sec_privilege needed_priv_1, enum sec_privilege needed_priv_2,
			      uint32 rights_mask,
			      uint32 des_access, uint32 *acc_granted,
			      const char *debug );
void map_max_allowed_access(const struct security_token *nt_token,
			    const struct security_unix_token *unix_token,
			    uint32_t *pacc_requested);

#endif /* _RPC_SERVER_SRV_ACCESS_CHECK_H_ */
