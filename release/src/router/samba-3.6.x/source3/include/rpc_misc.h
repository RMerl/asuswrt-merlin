/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell                1992-1997
   Copyright (C) Luke Kenneth Casson Leighton   1996-1997
   Copyright (C) Paul Ashton                    1997
   Copyright (C) Gerald (Jerry) Carter          2005
   
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

#ifndef _RPC_MISC_H /* _RPC_MISC_H */
#define _RPC_MISC_H 

/********************************************************************** 
 * RPC policy handle used pretty much everywhere
 **********************************************************************/

#define OUR_HANDLE(hnd) (((hnd)==NULL) ? "NULL" :\
	( IVAL((hnd)->uuid.node,2) == (uint32)sys_getpid() ? "OURS" : \
		"OTHER")), ((unsigned int)IVAL((hnd)->uuid.node,2)),\
		((unsigned int)sys_getpid() )

#endif /* _RPC_MISC_H */
