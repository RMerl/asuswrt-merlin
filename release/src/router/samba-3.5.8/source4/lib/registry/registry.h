/*
   Unix SMB/CIFS implementation.
   Registry interface
   Copyright (C) Gerald Carter                        2002.
   Copyright (C) Jelmer Vernooij					  2003-2007.

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

#ifndef _REGISTRY_H /* _REGISTRY_H */
#define _REGISTRY_H

struct registry_context;
struct loadparm_context;
struct smb_iconv_convenience;

#include <talloc.h>
#include "libcli/util/werror.h"
#include "librpc/gen_ndr/security.h"
#include "libcli/util/ntstatus.h"
#include "../lib/util/time.h"
#include "../lib/util/data_blob.h"

/**
 * The hive API. This API is generally used for
 * reading a specific file that contains just one hive.
 *
 * Good examples are .DAT (NTUSER.DAT) files.
 *
 * This API does not have any notification support (that
 * should be provided by the registry implementation), nor
 * does it understand what predefined keys are.
 */

struct hive_key {
	const struct hive_operations *ops;
};

struct hive_operations {
	const char *name;

	/**
	 * Open a specific subkey
	 */
	WERROR (*enum_key) (TALLOC_CTX *mem_ctx,
			    const struct hive_key *key, uint32_t idx,
			    const char **name,
			    const char **classname,
			    NTTIME *last_mod_time);

	/**
	 * Open a subkey by name
	 */
	WERROR (*get_key_by_name) (TALLOC_CTX *mem_ctx,
				   const struct hive_key *key, const char *name,
				   struct hive_key **subkey);

	/**
	 * Add a new key.
	 */
	WERROR (*add_key) (TALLOC_CTX *ctx,
			   const struct hive_key *parent_key, const char *name,
			   const char *classname,
			   struct security_descriptor *desc,
			   struct hive_key **key);
	/**
	 * Remove an existing key.
	 */
	WERROR (*del_key) (const struct hive_key *key, const char *name);

	/**
	 * Force write of a key to disk.
	 */
	WERROR (*flush_key) (struct hive_key *key);

	/**
	 * Retrieve a registry value with a specific index.
	 */
	WERROR (*enum_value) (TALLOC_CTX *mem_ctx,
			      struct hive_key *key, int idx,
			      const char **name, uint32_t *type,
			      DATA_BLOB *data);

	/**
	 * Retrieve a registry value with the specified name
	 */
	WERROR (*get_value_by_name) (TALLOC_CTX *mem_ctx,
				     struct hive_key *key, const char *name,
				     uint32_t *type, DATA_BLOB *data);

	/**
	 * Set a value on the specified registry key.
	 */
	WERROR (*set_value) (struct hive_key *key, const char *name,
			     uint32_t type, const DATA_BLOB data);

	/**
	 * Remove a value.
	 */
	WERROR (*delete_value) (struct hive_key *key, const char *name);

	/* Security Descriptors */

	/**
	 * Change the security descriptor on a registry key.
	 *
	 * This should return WERR_NOT_SUPPORTED if the underlying
	 * format does not have a mechanism for storing
	 * security descriptors.
	 */
	WERROR (*set_sec_desc) (struct hive_key *key,
				const struct security_descriptor *desc);

	/**
	 * Retrieve the security descriptor on a registry key.
	 *
	 * This should return WERR_NOT_SUPPORTED if the underlying
	 * format does not have a mechanism for storing
	 * security descriptors.
	 */
	WERROR (*get_sec_desc) (TALLOC_CTX *ctx,
				const struct hive_key *key,
				struct security_descriptor **desc);

	/**
	 * Retrieve general information about a key.
	 */
	WERROR (*get_key_info) (TALLOC_CTX *mem_ctx,
				const struct hive_key *key,
				const char **classname,
				uint32_t *num_subkeys,
				uint32_t *num_values,
				NTTIME *last_change_time,
				uint32_t *max_subkeynamelen,
				uint32_t *max_valnamelen,
				uint32_t *max_valbufsize);
};

struct cli_credentials;
struct auth_session_info;
struct tevent_context;

WERROR reg_open_hive(TALLOC_CTX *parent_ctx, const char *location,
		     struct auth_session_info *session_info,
		     struct cli_credentials *credentials,
		     struct tevent_context *ev_ctx,
		     struct loadparm_context *lp_ctx,
		     struct hive_key **root);
WERROR hive_key_get_info(TALLOC_CTX *mem_ctx, const struct hive_key *key,
			 const char **classname, uint32_t *num_subkeys,
			 uint32_t *num_values, NTTIME *last_change_time,
			 uint32_t *max_subkeynamelen,
			 uint32_t *max_valnamelen, uint32_t *max_valbufsize);
WERROR hive_key_add_name(TALLOC_CTX *ctx, const struct hive_key *parent_key,
			 const char *name, const char *classname,
			 struct security_descriptor *desc,
			 struct hive_key **key);
WERROR hive_key_del(const struct hive_key *key, const char *name);
WERROR hive_get_key_by_name(TALLOC_CTX *mem_ctx,
			    const struct hive_key *key, const char *name,
			    struct hive_key **subkey);
WERROR hive_enum_key(TALLOC_CTX *mem_ctx,
		     const struct hive_key *key, uint32_t idx,
		     const char **name,
		     const char **classname,
		     NTTIME *last_mod_time);

WERROR hive_key_set_value(struct hive_key *key, const char *name,
		      uint32_t type, const DATA_BLOB data);

WERROR hive_get_value(TALLOC_CTX *mem_ctx,
		      struct hive_key *key, const char *name,
		      uint32_t *type, DATA_BLOB *data);
WERROR hive_get_value_by_index(TALLOC_CTX *mem_ctx,
			       struct hive_key *key, uint32_t idx,
			       const char **name,
			       uint32_t *type, DATA_BLOB *data);
WERROR hive_get_sec_desc(TALLOC_CTX *mem_ctx,
			 struct hive_key *key,
			 struct security_descriptor **security);

WERROR hive_set_sec_desc(struct hive_key *key, 
			 const struct security_descriptor *security);

WERROR hive_key_del_value(struct hive_key *key, const char *name);

WERROR hive_key_flush(struct hive_key *key);


/* Individual backends */
WERROR reg_open_directory(TALLOC_CTX *parent_ctx,
			  const char *location, struct hive_key **key);
WERROR reg_open_regf_file(TALLOC_CTX *parent_ctx,
			  const char *location, struct smb_iconv_convenience *iconv_convenience,
			  struct hive_key **key);
WERROR reg_open_ldb_file(TALLOC_CTX *parent_ctx, const char *location,
			 struct auth_session_info *session_info,
			 struct cli_credentials *credentials,
			 struct tevent_context *ev_ctx,
			 struct loadparm_context *lp_ctx,
			 struct hive_key **k);


WERROR reg_create_directory(TALLOC_CTX *parent_ctx,
			    const char *location, struct hive_key **key);
WERROR reg_create_regf_file(TALLOC_CTX *parent_ctx,
			    struct smb_iconv_convenience *iconv_convenience,
			    const char *location,
			    int major_version,
			    struct hive_key **key);



/* Handles for the predefined keys */
#define HKEY_CLASSES_ROOT		0x80000000
#define HKEY_CURRENT_USER		0x80000001
#define HKEY_LOCAL_MACHINE		0x80000002
#define HKEY_USERS			0x80000003
#define HKEY_PERFORMANCE_DATA		0x80000004
#define HKEY_CURRENT_CONFIG		0x80000005
#define HKEY_DYN_DATA			0x80000006
#define HKEY_PERFORMANCE_TEXT		0x80000050
#define HKEY_PERFORMANCE_NLSTEXT	0x80000060

#define HKEY_FIRST		HKEY_CLASSES_ROOT
#define HKEY_LAST		HKEY_PERFORMANCE_NLSTEXT

struct reg_predefined_key {
	uint32_t handle;
	const char *name;
};

extern const struct reg_predefined_key reg_predefined_keys[];

#define	REG_DELETE		-1

/*
 * The general idea here is that every backend provides a 'hive'. Combining
 * various hives gives you a complete registry like windows has
 */

#define REGISTRY_INTERFACE_VERSION 1

struct reg_key_operations;

/* structure to store the registry handles */
struct registry_key
{
	struct registry_context *context;
};

struct registry_value
{
	const char *name;
	unsigned int data_type;
	DATA_BLOB data;
};

/* FIXME */
typedef void (*reg_key_notification_function) (void);
typedef void (*reg_value_notification_function) (void);

struct cli_credentials;

struct registry_operations {
	const char *name;

	WERROR (*get_key_info) (TALLOC_CTX *mem_ctx,
				const struct registry_key *key,
				const char **classname,
				uint32_t *numsubkeys,
				uint32_t *numvalues,
				NTTIME *last_change_time,
				uint32_t *max_subkeynamelen,
				uint32_t *max_valnamelen,
				uint32_t *max_valbufsize);

	WERROR (*flush_key) (struct registry_key *key);

	WERROR (*get_predefined_key) (struct registry_context *ctx,
				      uint32_t key_id,
				      struct registry_key **key);

	WERROR (*open_key) (TALLOC_CTX *mem_ctx,
			    struct registry_key *parent,
			    const char *path,
			    struct registry_key **key);

	WERROR (*create_key) (TALLOC_CTX *mem_ctx,
			      struct registry_key *parent,
			      const char *name,
			      const char *key_class,
			      struct security_descriptor *security,
			      struct registry_key **key);

	WERROR (*delete_key) (struct registry_key *key, const char *name);

	WERROR (*delete_value) (struct registry_key *key, const char *name);

	WERROR (*enum_key) (TALLOC_CTX *mem_ctx,
			    const struct registry_key *key, uint32_t idx,
			    const char **name,
			    const char **keyclass,
			    NTTIME *last_changed_time);

	WERROR (*enum_value) (TALLOC_CTX *mem_ctx,
			      const struct registry_key *key, uint32_t idx,
			      const char **name,
			      uint32_t *type,
			      DATA_BLOB *data);

	WERROR (*get_sec_desc) (TALLOC_CTX *mem_ctx,
				const struct registry_key *key,
				struct security_descriptor **security);

	WERROR (*set_sec_desc) (struct registry_key *key,
				const struct security_descriptor *security);

	WERROR (*load_key) (struct registry_key *key,
			    const char *key_name,
			    const char *path);

	WERROR (*unload_key) (struct registry_key *key, const char *name);

	WERROR (*notify_value_change) (struct registry_key *key,
				       reg_value_notification_function fn);

	WERROR (*get_value) (TALLOC_CTX *mem_ctx,
			     const struct registry_key *key,
			     const char *name,
			     uint32_t *type,
			     DATA_BLOB *data);

	WERROR (*set_value) (struct registry_key *key,
			     const char *name,
			     uint32_t type,
			     const DATA_BLOB data);
};

/**
 * Handle to a full registry
 * contains zero or more hives
 */
struct registry_context {
	const struct registry_operations *ops;
};

struct auth_session_info;
struct tevent_context;
struct loadparm_context;

/**
 * Open the locally defined registry.
 */
WERROR reg_open_local(TALLOC_CTX *mem_ctx,
		      struct registry_context **ctx);

WERROR reg_open_samba(TALLOC_CTX *mem_ctx,
		      struct registry_context **ctx,
		      struct tevent_context *ev_ctx,
		      struct loadparm_context *lp_ctx,
		      struct auth_session_info *session_info,
		      struct cli_credentials *credentials);

/**
 * Open the registry on a remote machine.
 */
WERROR reg_open_remote(struct registry_context **ctx,
		       struct auth_session_info *session_info,
		       struct cli_credentials *credentials,
		       struct loadparm_context *lp_ctx,
		       const char *location, struct tevent_context *ev);

WERROR reg_open_wine(struct registry_context **ctx, const char *path);

const char *reg_get_predef_name(uint32_t hkey);
WERROR reg_get_predefined_key_by_name(struct registry_context *ctx,
				      const char *name,
				      struct registry_key **key);
WERROR reg_get_predefined_key(struct registry_context *ctx,
			      uint32_t hkey,
			      struct registry_key **key);

WERROR reg_open_key(TALLOC_CTX *mem_ctx, struct registry_key *parent,
		    const char *name, struct registry_key **result);

WERROR reg_key_get_value_by_index(TALLOC_CTX *mem_ctx,
				  const struct registry_key *key, uint32_t idx,
				  const char **name,
				  uint32_t *type,
				  DATA_BLOB *data);
WERROR reg_key_get_info(TALLOC_CTX *mem_ctx,
			const struct registry_key *key,
			const char **class_name,
			uint32_t *num_subkeys,
			uint32_t *num_values,
			NTTIME *last_change_time,
			uint32_t *max_subkeynamelen,
			uint32_t *max_valnamelen,
			uint32_t *max_valbufsize);
WERROR reg_key_get_subkey_by_index(TALLOC_CTX *mem_ctx,
				   const struct registry_key *key,
				   int idx,
				   const char **name,
				   const char **classname,
				   NTTIME *last_mod_time);
WERROR reg_key_get_subkey_by_name(TALLOC_CTX *mem_ctx,
				  const struct registry_key *key,
				  const char *name,
				  struct registry_key **subkey);
WERROR reg_key_get_value_by_name(TALLOC_CTX *mem_ctx,
				 const struct registry_key *key,
				 const char *name,
				 uint32_t *type,
				 DATA_BLOB *data);
WERROR reg_key_del(struct registry_key *parent, const char *name);
WERROR reg_key_add_name(TALLOC_CTX *mem_ctx,
			struct registry_key *parent, const char *name,
			const char *classname,
			struct security_descriptor *desc,
			struct registry_key **newkey);
WERROR reg_val_set(struct registry_key *key, const char *value,
		   uint32_t type, DATA_BLOB data);
WERROR reg_get_sec_desc(TALLOC_CTX *ctx, const struct registry_key *key,
			struct security_descriptor **secdesc);
WERROR reg_del_value(struct registry_key *key, const char *valname);
WERROR reg_key_flush(struct registry_key *key);
WERROR reg_create_key(TALLOC_CTX *mem_ctx,
		      struct registry_key *parent,
		      const char *name,
		      const char *key_class,
		      struct security_descriptor *security,
		      struct registry_key **key);

/* Utility functions */
const char *str_regtype(int type);
char *reg_val_data_string(TALLOC_CTX *mem_ctx, struct smb_iconv_convenience *iconv_convenience, uint32_t type, const DATA_BLOB data);
char *reg_val_description(TALLOC_CTX *mem_ctx, struct smb_iconv_convenience *iconv_convenience, const char *name,
			  uint32_t type, const DATA_BLOB data);
bool reg_string_to_val(TALLOC_CTX *mem_ctx, struct smb_iconv_convenience *iconv_convenience, const char *type_str,
		       const char *data_str, uint32_t *type, DATA_BLOB *data);
WERROR reg_open_key_abs(TALLOC_CTX *mem_ctx, struct registry_context *handle,
			const char *name, struct registry_key **result);
WERROR reg_key_del_abs(struct registry_context *ctx, const char *path);
WERROR reg_key_add_abs(TALLOC_CTX *mem_ctx, struct registry_context *ctx,
		       const char *path, uint32_t access_mask,
		       struct security_descriptor *sec_desc,
		       struct registry_key **result);
WERROR reg_load_key(struct registry_context *ctx, struct registry_key *key,
		    const char *name, const char *filename);

WERROR reg_mount_hive(struct registry_context *rctx,
		      struct hive_key *hive_key,
		      uint32_t key_id,
		      const char **elements);

struct registry_key *reg_import_hive_key(struct registry_context *ctx,
					 struct hive_key *hive,
					 uint32_t predef_key,
					 const char **elements);
WERROR reg_set_sec_desc(struct registry_key *key,
			const struct security_descriptor *security);

struct reg_diff_callbacks {
	WERROR (*add_key) (void *callback_data, const char *key_name);
	WERROR (*set_value) (void *callback_data, const char *key_name,
			     const char *value_name, uint32_t value_type,
			     DATA_BLOB value);
	WERROR (*del_value) (void *callback_data, const char *key_name,
			     const char *value_name);
	WERROR (*del_key) (void *callback_data, const char *key_name);
	WERROR (*del_all_values) (void *callback_data, const char *key_name);
	WERROR (*done) (void *callback_data);
};

WERROR reg_diff_apply(struct registry_context *ctx, 
					  struct smb_iconv_convenience *ic, const char *filename);

WERROR reg_generate_diff(struct registry_context *ctx1,
			 struct registry_context *ctx2,
			 const struct reg_diff_callbacks *callbacks,
			 void *callback_data);
WERROR reg_dotreg_diff_save(TALLOC_CTX *ctx, const char *filename,
			    struct smb_iconv_convenience *iconv_convenience,
			    struct reg_diff_callbacks **callbacks,
			    void **callback_data);
WERROR reg_preg_diff_save(TALLOC_CTX *ctx, const char *filename,
			  struct smb_iconv_convenience *ic,
			  struct reg_diff_callbacks **callbacks,
			  void **callback_data);
WERROR reg_generate_diff_key(struct registry_key *oldkey,
			     struct registry_key *newkey,
			     const char *path,
			     const struct reg_diff_callbacks *callbacks,
			     void *callback_data);
WERROR reg_diff_load(const char *filename,
	             struct smb_iconv_convenience *iconv_convenience,
		     const struct reg_diff_callbacks *callbacks,
		     void *callback_data);

WERROR reg_dotreg_diff_load(int fd,
				     struct smb_iconv_convenience *iconv_convenience,
				     const struct reg_diff_callbacks *callbacks,
				     void *callback_data);

WERROR reg_preg_diff_load(int fd,
		   struct smb_iconv_convenience *iconv_convenience, 
				   const struct reg_diff_callbacks *callbacks,
				   void *callback_data);

WERROR local_get_predefined_key(struct registry_context *ctx,
				uint32_t key_id, struct registry_key **key);


#endif /* _REGISTRY_H */
