/*
 *  Unix SMB/CIFS implementation.
 *
 *  Virtual Windows Registry Layer
 *
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007-2010
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

#ifndef _REG_API_H
#define _REG_API_H

WERROR reg_openhive(TALLOC_CTX *mem_ctx, const char *hive,
		    uint32 desired_access,
		    const struct security_token *token,
		    struct registry_key **pkey);
WERROR reg_openkey(TALLOC_CTX *mem_ctx, struct registry_key *parent,
		   const char *name, uint32 desired_access,
		   struct registry_key **pkey);
WERROR reg_enumkey(TALLOC_CTX *mem_ctx, struct registry_key *key,
		   uint32 idx, char **name, NTTIME *last_write_time);
WERROR reg_enumvalue(TALLOC_CTX *mem_ctx, struct registry_key *key,
		     uint32 idx, char **pname, struct registry_value **pval);
WERROR reg_queryvalue(TALLOC_CTX *mem_ctx, struct registry_key *key,
		      const char *name, struct registry_value **pval);
WERROR reg_querymultiplevalues(TALLOC_CTX *mem_ctx,
			       struct registry_key *key,
			       uint32_t num_names,
			       const char **names,
			       uint32_t *pnum_vals,
			       struct registry_value **pvals);
WERROR reg_queryinfokey(struct registry_key *key, uint32_t *num_subkeys,
			uint32_t *max_subkeylen, uint32_t *max_subkeysize,
			uint32_t *num_values, uint32_t *max_valnamelen,
			uint32_t *max_valbufsize, uint32_t *secdescsize,
			NTTIME *last_changed_time);
WERROR reg_createkey(TALLOC_CTX *ctx, struct registry_key *parent,
		     const char *subkeypath, uint32 desired_access,
		     struct registry_key **pkey,
		     enum winreg_CreateAction *paction);
WERROR reg_deletekey(struct registry_key *parent, const char *path);
WERROR reg_setvalue(struct registry_key *key, const char *name,
		    const struct registry_value *val);
WERROR reg_deletevalue(struct registry_key *key, const char *name);
WERROR reg_getkeysecurity(TALLOC_CTX *mem_ctx, struct registry_key *key,
			  struct security_descriptor **psecdesc);
WERROR reg_setkeysecurity(struct registry_key *key,
			  struct security_descriptor *psecdesc);
WERROR reg_getversion(uint32_t *version);
WERROR reg_deleteallvalues(struct registry_key *key);
WERROR reg_deletekey_recursive(struct registry_key *parent,
			       const char *path);
WERROR reg_deletesubkeys_recursive(struct registry_key *parent,
				   const char *path);


#endif /* _REG_API_H */
