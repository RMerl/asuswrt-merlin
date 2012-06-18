
/*
   Unix SMB/CIFS implementation.
   Registry hive interface
   Copyright (C) Jelmer Vernooij				  2003-2007.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "registry.h"
#include "system/filesys.h"
#include "param/param.h"

/** Open a registry file/host/etc */
_PUBLIC_ WERROR reg_open_hive(TALLOC_CTX *parent_ctx, const char *location,
			      struct auth_session_info *session_info,
			      struct cli_credentials *credentials,
			      struct tevent_context *ev_ctx,
			      struct loadparm_context *lp_ctx,
			      struct hive_key **root)
{
	int fd, num;
	char peek[20];

	/* Check for directory */
	if (directory_exist(location)) {
		return reg_open_directory(parent_ctx, location, root);
	}

	fd = open(location, O_RDWR);
	if (fd == -1) {
		if (errno == ENOENT)
			return WERR_BADFILE;
		return WERR_BADFILE;
	}

	num = read(fd, peek, 20);
	if (num == -1) {
		return WERR_BADFILE;
	}

	if (!strncmp(peek, "regf", 4)) {
		close(fd);
		return reg_open_regf_file(parent_ctx, location, lp_iconv_convenience(lp_ctx), root);
	} else if (!strncmp(peek, "TDB file", 8)) {
		close(fd);
		return reg_open_ldb_file(parent_ctx, location, session_info,
					 credentials, ev_ctx, lp_ctx, root);
	}

	return WERR_BADFILE;
}

_PUBLIC_ WERROR hive_key_get_info(TALLOC_CTX *mem_ctx,
				  const struct hive_key *key,
				  const char **classname, uint32_t *num_subkeys,
				  uint32_t *num_values,
				  NTTIME *last_change_time,
				  uint32_t *max_subkeynamelen,
				  uint32_t *max_valnamelen,
				  uint32_t *max_valbufsize)
{
	return key->ops->get_key_info(mem_ctx, key, classname, num_subkeys,
				      num_values, last_change_time,
				      max_subkeynamelen,
				      max_valnamelen, max_valbufsize);
}

_PUBLIC_ WERROR hive_key_add_name(TALLOC_CTX *ctx,
				  const struct hive_key *parent_key,
				  const char *name, const char *classname,
				  struct security_descriptor *desc,
				  struct hive_key **key)
{
	SMB_ASSERT(strchr(name, '\\') == NULL);

	return parent_key->ops->add_key(ctx, parent_key, name, classname,
					desc, key);
}

_PUBLIC_ WERROR hive_key_del(const struct hive_key *key, const char *name)
{
	return key->ops->del_key(key, name);
}

_PUBLIC_ WERROR hive_get_key_by_name(TALLOC_CTX *mem_ctx,
				     const struct hive_key *key,
				     const char *name,
				     struct hive_key **subkey)
{
	return key->ops->get_key_by_name(mem_ctx, key, name, subkey);
}

WERROR hive_enum_key(TALLOC_CTX *mem_ctx,
		     const struct hive_key *key, uint32_t idx,
		     const char **name,
		     const char **classname,
		     NTTIME *last_mod_time)
{
	return key->ops->enum_key(mem_ctx, key, idx, name, classname,
				  last_mod_time);
}

WERROR hive_key_set_value(struct hive_key *key, const char *name, uint32_t type,
					  const DATA_BLOB data)
{
	if (key->ops->set_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->set_value(key, name, type, data);
}

WERROR hive_get_value(TALLOC_CTX *mem_ctx,
		      struct hive_key *key, const char *name,
		      uint32_t *type, DATA_BLOB *data)
{
	if (key->ops->get_value_by_name == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->get_value_by_name(mem_ctx, key, name, type, data);
}

WERROR hive_get_value_by_index(TALLOC_CTX *mem_ctx,
			       struct hive_key *key, uint32_t idx,
			       const char **name,
			       uint32_t *type, DATA_BLOB *data)
{
	if (key->ops->enum_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->enum_value(mem_ctx, key, idx, name, type, data);
}

WERROR hive_get_sec_desc(TALLOC_CTX *mem_ctx,
			 struct hive_key *key, 
			 struct security_descriptor **security)
{
	if (key->ops->get_sec_desc == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->get_sec_desc(mem_ctx, key, security);
}

WERROR hive_set_sec_desc(struct hive_key *key, 
			 const struct security_descriptor *security)
{
	if (key->ops->set_sec_desc == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->set_sec_desc(key, security);
}

WERROR hive_key_del_value(struct hive_key *key, const char *name)
{
	if (key->ops->delete_value == NULL)
		return WERR_NOT_SUPPORTED;

	return key->ops->delete_value(key, name);
}

WERROR hive_key_flush(struct hive_key *key)
{
	if (key->ops->flush_key == NULL)
		return WERR_OK;

	return key->ops->flush_key(key);
}
