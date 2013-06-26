/* 
   Partitions ldb module

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007

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
#include <ldb.h>
#include <ldb_errors.h>
#include <ldb_module.h>
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "system/locale.h"
#include "param/param.h"

struct dsdb_partition {
	struct ldb_module *module;
	struct dsdb_control_current_partition *ctrl;
	const char *backend_url;
	DATA_BLOB orig_record;
};

struct partition_module {
	const char **modules;
	struct ldb_dn *dn;
};

struct partition_private_data {
	struct dsdb_partition **partitions;
	struct ldb_dn **replicate;
	
	struct partition_module **modules;
	const char *ldapBackend;

	uint64_t metadata_seq;
	uint32_t in_transaction;

	struct ldb_message *forced_module_msg;
};

#include "dsdb/samdb/ldb_modules/partition_proto.h"
