/* 
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Andrew Tridgell 2009
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009

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

/* predeclare some structures used by utility functions */
struct dsdb_schema;
struct dsdb_attribute;
struct dsdb_fsmo_extended_op;
struct security_descriptor;
struct dom_sid;

#include "librpc/gen_ndr/misc.h"
#include "dsdb/samdb/ldb_modules/util_proto.h"
#include "dsdb/common/util.h"

/* extend the dsdb_request_add_controls() flags for module
   specific functions */
#define DSDB_FLAG_NEXT_MODULE		      0x00100000
#define DSDB_FLAG_OWN_MODULE		      0x00400000
#define DSDB_FLAG_TOP_MODULE		      0x00800000
#define DSDB_FLAG_TRUSTED		      0x01000000
