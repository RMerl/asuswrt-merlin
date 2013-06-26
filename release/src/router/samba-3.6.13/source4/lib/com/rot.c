/* 
   Unix SMB/CIFS implementation.

   Running object table functions

   Copyright (C) Jelmer Vernooij 2004-2005
   
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
#include "lib/com/com.h"

struct dcom_interface_p *dcom_get_local_iface_p(struct GUID *ipid)
{
	/* FIXME: Call the local ROT and do a 
	 * rot_get_interface_pointer call */

	/* FIXME: Perhaps have a local (thread-local) table with 
	 * local DCOM objects so that not every DCOM call requires a lookup 
	 * to the ROT? */
	return NULL; 
}
