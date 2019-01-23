/*
 *  Unix SMB/CIFS implementation.
 *  libnet Support
 *  Copyright (C) Guenther Deschner 2008
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

#include "../librpc/gen_ndr/netlogon.h"

enum net_samsync_mode {
	NET_SAMSYNC_MODE_FETCH_PASSDB = 0,
	NET_SAMSYNC_MODE_FETCH_LDIF = 1,
	NET_SAMSYNC_MODE_FETCH_KEYTAB = 2,
	NET_SAMSYNC_MODE_DUMP = 3
};

struct samsync_context;

struct samsync_ops {
	NTSTATUS (*startup)(TALLOC_CTX *mem_ctx,
			    struct samsync_context *ctx,
			    enum netr_SamDatabaseID id,
			    uint64_t *sequence_num);
	NTSTATUS (*process_objects)(TALLOC_CTX *mem_ctx,
				    enum netr_SamDatabaseID id,
				    struct netr_DELTA_ENUM_ARRAY *array,
				    uint64_t *sequence_num,
				    struct samsync_context *ctx);
	NTSTATUS (*finish)(TALLOC_CTX *mem_ctx,
			   struct samsync_context *ctx,
			   enum netr_SamDatabaseID id,
			   uint64_t sequence_num);
};

struct samsync_object {
	uint16_t database_id;
	uint16_t object_type;
	union {
		uint32_t rid;
		const char *name;
		struct dom_sid sid;
	} object_identifier;
};

struct samsync_context {
	enum net_samsync_mode mode;
	const struct dom_sid *domain_sid;
	const char *domain_sid_str;
	const char *domain_name;
	const char *output_filename;

	const char *username;
	const char *password;

	char *result_message;
	char *error_message;

	bool single_object_replication;
	bool force_full_replication;
	bool clean_old_entries;

	uint32_t num_objects;
	struct samsync_object *objects;

	struct rpc_pipe_client *cli;
	struct messaging_context *msg_ctx;

	const struct samsync_ops *ops;

	void *private_data;
};

extern const struct samsync_ops libnet_samsync_ldif_ops;
extern const struct samsync_ops libnet_samsync_keytab_ops;
extern const struct samsync_ops libnet_samsync_display_ops;
extern const struct samsync_ops libnet_samsync_passdb_ops;

/* The following definitions come from libnet/libnet_samsync.c  */

NTSTATUS libnet_samsync_init_context(TALLOC_CTX *mem_ctx,
				     const struct dom_sid *domain_sid,
				     struct samsync_context **ctx_p);
NTSTATUS libnet_samsync(enum netr_SamDatabaseID database_id,
			struct samsync_context *ctx);
NTSTATUS pull_netr_AcctLockStr(TALLOC_CTX *mem_ctx,
			       struct lsa_BinaryString *r,
			       struct netr_AcctLockStr **str_p);
