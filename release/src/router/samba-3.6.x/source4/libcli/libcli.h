/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 2004
   Copyright (C) James Myers 2003 <myersjj@samba.org>

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

#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#include "librpc/gen_ndr/nbt.h"

struct substitute_context;

/* 
   smbcli_state: internal state used in libcli library for single-threaded callers, 
   i.e. a single session on a single socket. 
 */
struct smbcli_state {
	struct smbcli_transport *transport;
	struct smbcli_session *session;
	struct smbcli_tree *tree;
	struct substitute_context *substitute;
	struct smblsa_state *lsa;
};

struct clilist_file_info {
	uint64_t size;
	uint16_t attrib;
	time_t mtime;
	const char *name;
	const char *short_name;
};

struct nbt_dc_name {
	const char *address;
	const char *name;
};

struct cli_credentials;
struct tevent_context;

/* passed to br lock code. */
enum brl_type {
	READ_LOCK,
	WRITE_LOCK,
	PENDING_READ_LOCK,
	PENDING_WRITE_LOCK
};



#include "libcli/raw/libcliraw.h"
struct gensec_settings;
#include "libcli/libcli_proto.h"

#endif /* __LIBCLI_H__ */
