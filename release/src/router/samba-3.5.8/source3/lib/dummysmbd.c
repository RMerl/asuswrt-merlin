/*
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Gerald (Jerry) Carter          2004.

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

/* Stupid dummy functions required due to the horrible dependency mess
   in Samba. */

#include "includes.h"

int get_client_fd(void)
{
	return -1;
}

int find_service(fstring service)
{
	return -1;
}

bool conn_snum_used(int snum)
{
	return False;
}

void cancel_pending_lock_requests_by_fid(files_struct *fsp, struct byte_range_lock *br_lck)
{
}

void send_stat_cache_delete_message(const char *name)
{
}

NTSTATUS can_delete_directory(struct connection_struct *conn,
				const char *dirname)
{
	return NT_STATUS_OK;
}

bool change_to_root_user(void)
{
	return false;
}

struct event_context *smbd_event_context(void)
{
	return NULL;
}

struct messaging_context *smbd_messaging_context(void)
{
	return NULL;
}

/**
 * The following two functions need to be called from inside the low-level BRL
 * code for oplocks correctness in smbd.  Since other utility binaries also
 * link in some of the brl code directly, these dummy functions are necessary
 * to avoid needing to link in the oplocks code and its dependencies to all of
 * the utility binaries.
 */
void contend_level2_oplocks_begin(files_struct *fsp,
				  enum level2_contention_type type)
{
	return;
}

void contend_level2_oplocks_end(files_struct *fsp,
				enum level2_contention_type type)
{
	return;
}
