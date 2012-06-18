/* 
   Unix SMB/CIFS implementation.

   dcerpc utility functions

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Jelmer Vernooij 2004
   
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
#include "../lib/util/dlinklist.h"
#include "librpc/ndr/libndr.h"
#include "librpc/ndr/ndr_table.h"
#undef strcasecmp

static struct ndr_interface_list *ndr_interfaces;

/*
  register a ndr interface table
*/
NTSTATUS ndr_table_register(const struct ndr_interface_table *table)
{
	struct ndr_interface_list *l;

	for (l = ndr_interfaces; l; l = l->next) {
		if (GUID_equal(&table->syntax_id.uuid, &l->table->syntax_id.uuid)) {
			DEBUG(0, ("Attempt to register interface %s which has the "
				  "same UUID as already registered interface %s\n", 
				  table->name, l->table->name));
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}
	}

	l = talloc(talloc_autofree_context(), struct ndr_interface_list);
	l->table = table;

	DLIST_ADD(ndr_interfaces, l);

  	return NT_STATUS_OK;
}

/*
  find the pipe name for a local IDL interface
*/
const char *ndr_interface_name(const struct GUID *uuid, uint32_t if_version)
{
	const struct ndr_interface_list *l;
	for (l=ndr_table_list();l;l=l->next) {
		if (GUID_equal(&l->table->syntax_id.uuid, uuid) &&
		    l->table->syntax_id.if_version == if_version) {
			return l->table->name;
		}
	}
	return "UNKNOWN";
}

/*
  find the number of calls defined by local IDL
*/
int ndr_interface_num_calls(const struct GUID *uuid, uint32_t if_version)
{
	const struct ndr_interface_list *l;
	for (l=ndr_interfaces;l;l=l->next){
		if (GUID_equal(&l->table->syntax_id.uuid, uuid) &&
		    l->table->syntax_id.if_version == if_version) {
			return l->table->num_calls;
		}
	}
	return -1;
}


/*
  find a dcerpc interface by name
*/
const struct ndr_interface_table *ndr_table_by_name(const char *name)
{
	const struct ndr_interface_list *l;
	for (l=ndr_interfaces;l;l=l->next) {
		if (strcasecmp(l->table->name, name) == 0) {
			return l->table;
		}
	}
	return NULL;
}

/*
  find a dcerpc interface by uuid
*/
const struct ndr_interface_table *ndr_table_by_uuid(const struct GUID *uuid)
{
	const struct ndr_interface_list *l;
	for (l=ndr_interfaces;l;l=l->next) {
		if (GUID_equal(&l->table->syntax_id.uuid, uuid)) {
			return l->table;
		}
	}
	return NULL;
}

/*
  return the list of registered dcerpc_pipes
*/
const struct ndr_interface_list *ndr_table_list(void)
{
	return ndr_interfaces;
}


NTSTATUS ndr_table_init(void)
{
	static bool initialized = false;

	if (initialized) return NT_STATUS_OK;
	initialized = true;

	ndr_table_register_builtin_tables();

	return NT_STATUS_OK;
}
