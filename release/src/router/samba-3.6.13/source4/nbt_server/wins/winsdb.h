/* 
   Unix SMB/CIFS implementation.

   WINS server structures

   Copyright (C) Andrew Tridgell	2005
   
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

#define WINSDB_FLAG_ALLOC_VERSION	(1<<0)
#define WINSDB_FLAG_TAKE_OWNERSHIP	(1<<1)

struct winsdb_addr {
	const char *address;
	const char *wins_owner;
	time_t expire_time;
};

/*
  each record in the database contains the following information
*/
struct winsdb_record {
	struct nbt_name *name;
	enum wrepl_name_type type;
	enum wrepl_name_state state;
	enum wrepl_name_node node;
	bool is_static;
	time_t expire_time;
	uint64_t version;
	const char *wins_owner;
	struct winsdb_addr **addresses;

	/* only needed for debugging problems */
	const char *registered_by;
};

enum winsdb_handle_caller {
	WINSDB_HANDLE_CALLER_ADMIN	= 0,
	WINSDB_HANDLE_CALLER_NBTD	= 1,
	WINSDB_HANDLE_CALLER_WREPL	= 2
};

struct winsdb_handle {
	/* wins server database handle */
	struct ldb_context *ldb;

	/*
	 * the type of the caller, as we pass this to the
	 * 'wins_ldb' ldb module can decide if it needs to verify the 
	 * the records before they're written to disk
	 */
	enum winsdb_handle_caller caller;

	/* local owner address */
	const char *local_owner;

	/* wins hook script */
	const char *hook_script;
};

enum wins_hook_action {
	WINS_HOOK_ADD		= 0,
	WINS_HOOK_MODIFY	= 1,
	WINS_HOOK_DELETE	= 2
};

struct ldb_message;
struct tevent_context;
#include "nbt_server/wins/winsdb_proto.h"
