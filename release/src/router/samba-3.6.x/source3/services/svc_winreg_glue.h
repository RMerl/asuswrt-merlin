/*
 *  Unix SMB/CIFS implementation.
 *
 *  SVC winreg glue
 *
 *  Copyright (c) 2005      Marcin Krzysztof Porwit
 *  Copyright (c) 2005      Gerald (Jerry) Carter
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SVC_WINREG_GLUE_H
#define SVC_WINREG_GLUE_H

struct auth_serversupplied_info;

struct security_descriptor* svcctl_gen_service_sd(TALLOC_CTX *mem_ctx);

struct security_descriptor *svcctl_get_secdesc(TALLOC_CTX *mem_ctx,
					       struct messaging_context *msg_ctx,
					       const struct auth_serversupplied_info *session_info,
					       const char *name);

bool svcctl_set_secdesc(struct messaging_context *msg_ctx,
			const struct auth_serversupplied_info *session_info,
			const char *name,
			struct security_descriptor *sd);

const char *svcctl_get_string_value(TALLOC_CTX *mem_ctx,
				    struct messaging_context *msg_ctx,
				    const struct auth_serversupplied_info *session_info,
				    const char *key_name,
				    const char *value_name);

const char *svcctl_lookup_dispname(TALLOC_CTX *mem_ctx,
				   struct messaging_context *msg_ctx,
				   const struct auth_serversupplied_info *session_info,
				   const char *name);

const char *svcctl_lookup_description(TALLOC_CTX *mem_ctx,
				      struct messaging_context *msg_ctx,
				      const struct auth_serversupplied_info *session_info,
				      const char *name);

#endif /* SVC_WINREG_GLUE_H */

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
