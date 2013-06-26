/*
   Unix SMB/CIFS implementation.

   Map SIDs to uids/gids and back

   Copyright (C) Kai Blin 2008

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

#ifndef _WINBIND_IDMAP_H_
#define _WINBIND_IDMAP_H_

#include "librpc/gen_ndr/idmap.h"

struct idmap_context {
	struct loadparm_context *lp_ctx;
	struct ldb_context *ldb_ctx;
	struct dom_sid *unix_groups_sid;
	struct dom_sid *unix_users_sid;
};

struct tevent_context;

#include "winbind/idmap_proto.h"

#endif

