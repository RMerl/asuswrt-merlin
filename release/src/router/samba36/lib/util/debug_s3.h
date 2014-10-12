/*
   Unix SMB/CIFS implementation.
   SMB debug stuff
   Copyright (C) Andrew Tridgell 2002

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

#include "librpc/gen_ndr/server_id.h"

struct messaging_context;
struct server_id;
void debug_message(struct messaging_context *msg_ctx, void *private_data, uint32_t msg_type, struct server_id src, DATA_BLOB *data);
void debug_register_msgs(struct messaging_context *msg_ctx);
bool reopen_logs( void );
