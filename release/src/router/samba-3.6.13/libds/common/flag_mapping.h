/*
   Unix SMB/CIFS implementation.
   helper mapping functions for the UF and ACB flags

   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Matthias Dieter Wallnöfer 2010

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

#ifndef __LIBDS_COMMON_FLAG_MAPPING_H__
#define __LIBDS_COMMON_FLAG_MAPPING_H__

/* The following definitions come from flag_mapping.c  */

uint32_t ds_acb2uf(uint32_t acb);
uint32_t ds_uf2acb(uint32_t uf);
uint32_t ds_uf2atype(uint32_t uf);
uint32_t ds_gtype2atype(uint32_t gtype);
enum lsa_SidType ds_atype_map(uint32_t atype);
uint32_t ds_uf2prim_group_rid(uint32_t uf);

#endif /* __LIBDS_COMMON_FLAG_MAPPING_H__ */
