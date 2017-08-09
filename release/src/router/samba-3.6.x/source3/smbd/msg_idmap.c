/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief  Notify smbd about idmap changes
 * @file   msg_idmap.c
 * @author Gregor Beck <gb@sernet.de>
 * @date   Feb 2011
 *
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "globals.h"
#include "../libcli/security/dom_sid.h"
#include "idmap_cache.h"
#include "passdb/lookup_sid.h"
#include "auth.h"
#include "messages.h"

struct id {
	union {
		uid_t uid;
		gid_t gid;
		struct dom_sid sid;
	} id;
	enum {UID, GID, SID} type;
};

static bool parse_id(const char* str, struct id* id)
{
	struct dom_sid sid;
	unsigned long ul;
	char c, trash;

	if (sscanf(str, "%cID %lu%c", &c, &ul, &trash) == 2) {
		switch(c) {
		case 'G':
			id->id.gid = ul;
			id->type = GID;
			return true;
		case 'U':
			id->id.uid = ul;
			id->type = UID;
			return true;
		default:
			break;
		}
	} else if (string_to_sid(&sid, str)) {
		id->id.sid = sid;
		id->type = SID;
		return true;
	}
	return false;
}

static bool uid_in_use(const struct user_struct* user, uid_t uid)
{
	while (user) {
		if (user->session_info && (user->session_info->utok.uid == uid)) {
			return true;
		}
		user = user->next;
	}
	return false;
}

static bool gid_in_use(const struct user_struct* user, gid_t gid)
{
	while (user) {
		if (user->session_info != NULL) {
			int i;
			struct security_unix_token utok = user->session_info->utok;
			if (utok.gid == gid) {
				return true;
			}
			for(i=0; i<utok.ngroups; i++) {
				if (utok.groups[i] == gid) {
					return true;
				}
			}
		}
		user = user->next;
	}
	return false;
}

static bool sid_in_use(const struct user_struct* user, const struct dom_sid* psid)
{
	uid_t uid;
	gid_t gid;
	if (sid_to_gid(psid, &gid)) {
		return gid_in_use(user, gid);
	} else if (sid_to_uid(psid, &uid)) {
		return uid_in_use(user, uid);
	}
	return false;
}


static bool id_in_use(const struct user_struct* user, const struct id* id)
{
	switch(id->type) {
	case UID:
		return uid_in_use(user, id->id.uid);
	case GID:
		return gid_in_use(user, id->id.gid);
	case SID:
		return sid_in_use(user, &id->id.sid);
	default:
		break;
	}
	return false;
}

static void delete_from_cache(const struct id* id)
{
	switch(id->type) {
	case UID:
		delete_uid_cache(id->id.uid);
		idmap_cache_del_uid(id->id.uid);
		break;
	case GID:
		delete_gid_cache(id->id.gid);
		idmap_cache_del_gid(id->id.gid);
		break;
	case SID:
		delete_sid_cache(&id->id.sid);
		idmap_cache_del_sid(&id->id.sid);
		break;
	default:
		break;
	}
}


static void message_idmap_flush(struct messaging_context *msg_ctx,
				void* private_data,
				uint32_t msg_type,
				struct server_id server_id,
				DATA_BLOB* data)
{
	const char* msg = data ? (const char*)data->data : NULL;

	if ((msg == NULL) || (msg[0] == '\0')) {
		flush_gid_cache();
		flush_uid_cache();
	} else if (strncmp(msg, "GID", 3)) {
		flush_gid_cache();
	} else if (strncmp(msg, "UID", 3)) {
		flush_uid_cache();
	} else {
		DEBUG(0, ("Invalid argument: %s\n", msg));
	}
}


static void message_idmap_delete(struct messaging_context *msg_ctx,
				 void* private_data,
				 uint32_t msg_type,
				 struct server_id server_id,
				 DATA_BLOB* data)
{
	const char* msg = (data && data->data) ? (const char*)data->data : "<NULL>";
	bool do_kill = (msg_type == MSG_IDMAP_KILL);
	struct user_struct* validated_users = smbd_server_conn->smb1.sessions.validated_users;
	struct id id;

	if (!parse_id(msg, &id)) {
		DEBUG(0, ("Invalid ?ID: %s\n", msg));
		return;
	}

	if( do_kill && id_in_use(validated_users, &id) ) {
		exit_server_cleanly(msg);
	} else {
		delete_from_cache(&id);
	}
}

void msg_idmap_register_msgs(struct messaging_context *ctx)
{
	messaging_register(ctx, NULL, MSG_IDMAP_FLUSH,  message_idmap_flush);
	messaging_register(ctx, NULL, MSG_IDMAP_DELETE, message_idmap_delete);
	messaging_register(ctx, NULL, MSG_IDMAP_KILL,   message_idmap_delete);
}
