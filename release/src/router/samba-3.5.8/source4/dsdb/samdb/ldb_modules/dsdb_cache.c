/* 
   Unix SMB/CIFS mplementation.

   The Module that loads some DSDB related things
   into memory. E.g. it loads the dsdb_schema struture
   
   Copyright (C) Stefan Metzmacher 2007
    
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
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb/include/ldb_private.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"

static int dsdb_cache_init(struct ldb_module *module)
{
	/* TODO: load the schema */
	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_dsdb_cache_module_ops = {
	.name		= "dsdb_cache",
	.init_context	= dsdb_cache_init
};
