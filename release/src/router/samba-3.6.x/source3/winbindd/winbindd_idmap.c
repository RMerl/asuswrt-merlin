/*
   Unix SMB/CIFS implementation.

   Async helpers for blocking functions

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Gerald Carter 2006
   Copyright (C) Simo Sorce 2007

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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static struct winbindd_child static_idmap_child;

struct winbindd_child *idmap_child(void)
{
	return &static_idmap_child;
}

static const struct winbindd_child_dispatch_table idmap_dispatch_table[] = {
	{
		.name		= "PING",
		.struct_cmd	= WINBINDD_PING,
		.struct_fn	= winbindd_dual_ping,
	},{
		.name		= "NDRCMD",
		.struct_cmd	= WINBINDD_DUAL_NDRCMD,
		.struct_fn	= winbindd_dual_ndrcmd,
	},{
		.name		= NULL,
	}
};

void init_idmap_child(void)
{
	setup_child(NULL, &static_idmap_child,
		    idmap_dispatch_table,
		    "log.winbindd", "idmap");
}
