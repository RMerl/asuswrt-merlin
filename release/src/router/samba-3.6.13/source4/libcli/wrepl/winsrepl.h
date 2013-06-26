/*
   Unix SMB/CIFS implementation.

   structures for WINS replication client library

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2005-2010

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

#include "librpc/gen_ndr/nbt.h"
#include "librpc/gen_ndr/winsrepl.h"

struct wrepl_request;
struct wrepl_socket;

struct wrepl_send_ctrl {
	bool send_only;
	bool disconnect_after_send;
};

/*
  setup an association
*/
struct wrepl_associate {
	struct {
		uint32_t assoc_ctx;
		uint16_t major_version;
	} out;
};

/*
  setup an association
*/
struct wrepl_associate_stop {
	struct {
		uint32_t assoc_ctx;
		uint32_t reason;
	} in;
};

/*
  pull the partner table
*/
struct wrepl_pull_table {
	struct {
		uint32_t assoc_ctx;
	} in;
	struct {
		uint32_t num_partners;
		struct wrepl_wins_owner *partners;
	} out;
};

#define WREPL_NAME_TYPE(flags) (flags & WREPL_FLAGS_RECORD_TYPE)
#define WREPL_NAME_STATE(flags) ((flags & WREPL_FLAGS_RECORD_STATE)>>2)
#define WREPL_NAME_NODE(flags) ((flags & WREPL_FLAGS_NODE_TYPE)>>5)
#define WREPL_NAME_IS_STATIC(flags) ((flags & WREPL_FLAGS_IS_STATIC)?true:false)

#define WREPL_NAME_FLAGS(type, state, node, is_static) \
	(type | (state << 2) | (node << 5) | \
	 (is_static ? WREPL_FLAGS_IS_STATIC : 0))

struct wrepl_address {
	const char *owner;
	const char *address;
};

struct wrepl_name {
	struct nbt_name name;
	enum wrepl_name_type type;
	enum wrepl_name_state state;
	enum wrepl_name_node node;
	bool is_static;
	uint32_t raw_flags;
	uint64_t version_id;
	const char *owner;
	uint32_t num_addresses;
	struct wrepl_address *addresses;
};

/*
  a full pull replication
*/
struct wrepl_pull_names {
	struct {
		uint32_t assoc_ctx;
		struct wrepl_wins_owner partner;
	} in;
	struct {
		uint32_t num_names;
		struct wrepl_name *names;
	} out;
};

struct tstream_context;

#include "libcli/wrepl/winsrepl_proto.h"
