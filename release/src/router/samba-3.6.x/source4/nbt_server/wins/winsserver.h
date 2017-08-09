/* 
   Unix SMB/CIFS implementation.

   wins server WACK processing

   Copyright (C) Stefan Metzmacher	2005
   
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

struct wins_server {
	/* wins server database handle */
	struct winsdb_handle *wins_db;

	/* some configuration */
	struct {
		/* 
		 * the interval (in secs) till an active record will be marked as RELEASED
		 */
		uint32_t min_renew_interval;
		uint32_t max_renew_interval;

		/* 
		 * the interval (in secs) a record remains in RELEASED state,
		 * before it will be marked as TOMBSTONE
		 * (also known as extinction interval)
		 */
		uint32_t tombstone_interval;

		/* 
		 * the interval (in secs) a record remains in TOMBSTONE state,
		 * before it will be removed from the database.
		 * See also 'tombstone_extra_timeout'.
		 * (also known as extinction timeout)
		 */
		uint32_t tombstone_timeout;
	} config;
};

struct wins_challenge_io {
	struct {
		struct nbtd_server *nbtd_server;
		uint16_t nbt_port;
		struct tevent_context *event_ctx;
		struct nbt_name *name;
		uint32_t num_addresses;
		const char **addresses;
	} in;
	struct {
		uint32_t num_addresses;
		const char **addresses;
	} out;
};

struct composite_context;
#include "nbt_server/wins/winsserver_proto.h"
