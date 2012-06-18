/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008
   
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
#include "libsmbclient.h"
#include "libsmb_internal.h"


/* 
 * check if an element is part of the list. 
 */
int
SMBC_dlist_contains(SMBCFILE * list, SMBCFILE *p)
{
	if (!p || !list) return False;
	do {
		if (p == list) return True;
		list = list->next;
	} while (list);
	return False;
}


/*
 * Convert an SMB error into a UNIX error ...
 */
int
SMBC_errno(SMBCCTX *context,
           struct cli_state *c)
{
	int ret = cli_errno(c);
        
        if (cli_is_dos_error(c)) {
                uint8 eclass;
                uint32 ecode;
                
                cli_dos_error(c, &eclass, &ecode);
                
                DEBUG(3,("smbc_error %d %d (0x%x) -> %d\n", 
                         (int)eclass, (int)ecode, (int)ecode, ret));
        } else {
                NTSTATUS status;
                
                status = cli_nt_error(c);
                
                DEBUG(3,("smbc errno %s -> %d\n",
                         nt_errstr(status), ret));
        }
        
	return ret;
}

