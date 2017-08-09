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

#ifndef _NDR_TABLE_PROTO_H_
#define _NDR_TABLE_PROTO_H_

NTSTATUS ndr_table_register(const struct ndr_interface_table *table);
const char *ndr_interface_name(const struct GUID *uuid, uint32_t if_version);
int ndr_interface_num_calls(const struct GUID *uuid, uint32_t if_version);
const struct ndr_interface_table *ndr_table_by_name(const char *name);
const struct ndr_interface_table *ndr_table_by_uuid(const struct GUID *uuid);
const struct ndr_interface_list *ndr_table_list(void);
NTSTATUS ndr_table_init(void);
NTSTATUS ndr_table_register_builtin_tables(void);

#endif /* _NDR_TABLE_PROTO_H_ */

