/* 
   Unix SMB/CIFS implementation.

   wins hook feature, we run a specified script
   which can then do some custom actions

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

#include "includes.h"
#include "nbt_server/nbt_server.h"
#include "nbt_server/wins/winsdb.h"
#include "system/filesys.h"

static const char *wins_hook_action_string(enum wins_hook_action action)
{
	switch (action) {
	case WINS_HOOK_ADD:	return "add";
	case WINS_HOOK_MODIFY:	return "refresh";
	case WINS_HOOK_DELETE:	return "delete";
	}

	return "unknown";
}

void wins_hook(struct winsdb_handle *h, const struct winsdb_record *rec, 
	       enum wins_hook_action action, const char *wins_hook_script)
{
	uint32_t i, length;
	int child;
	char *cmd = NULL;
	TALLOC_CTX *tmp_mem = NULL;

	if (!wins_hook_script || !wins_hook_script[0]) return;

	tmp_mem = talloc_new(h);
	if (!tmp_mem) goto failed;

	length = winsdb_addr_list_length(rec->addresses);

	if (action == WINS_HOOK_MODIFY && length < 1) {
		action = WINS_HOOK_DELETE;
	}

	cmd = talloc_asprintf(tmp_mem,
			      "%s %s %s %02x %ld",
			      wins_hook_script,
			      wins_hook_action_string(action),
			      rec->name->name,
			      rec->name->type,
			      (long int) rec->expire_time);
	if (!cmd) goto failed;

	for (i=0; rec->addresses[i]; i++) {
		cmd = talloc_asprintf_append_buffer(cmd, " %s", rec->addresses[i]->address);
		if (!cmd) goto failed;
	}

	DEBUG(10,("call wins hook '%s'\n", cmd));

	/* signal handling in posix really sucks - doing this in a library
	   affects the whole app, but what else to do?? */
	signal(SIGCHLD, SIG_IGN);

	child = fork();
	if (child == (pid_t)-1) {
		goto failed;
	}

	if (child == 0) {
/* TODO: close file handles */
		execl("/bin/sh", "sh", "-c", cmd, NULL);
		_exit(0);
	}

	talloc_free(tmp_mem);
	return;
failed:
	talloc_free(tmp_mem);
	DEBUG(0,("FAILED: calling wins hook '%s'\n", wins_hook_script));
}
