/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002-2005
 *  Copyright (C) Michael Adam                      2006-2008
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

#ifndef _REG_DISPATCHER_H
#define _REG_DISPATCHER_H

bool store_reg_keys(struct registry_key_handle *key,
		    struct regsubkey_ctr *subkeys);
bool store_reg_values(struct registry_key_handle *key, struct regval_ctr *val);
WERROR create_reg_subkey(struct registry_key_handle *key, const char *subkey);
WERROR delete_reg_subkey(struct registry_key_handle *key, const char *subkey);
int fetch_reg_keys(struct registry_key_handle *key,
		   struct regsubkey_ctr *subkey_ctr);
int fetch_reg_values(struct registry_key_handle *key, struct regval_ctr *val);
bool regkey_access_check(struct registry_key_handle *key, uint32 requested,
			 uint32 *granted,
			 const struct security_token *token);
WERROR regkey_get_secdesc(TALLOC_CTX *mem_ctx, struct registry_key_handle *key,
			  struct security_descriptor **psecdesc);
WERROR regkey_set_secdesc(struct registry_key_handle *key,
			  struct security_descriptor *psecdesc);
bool reg_subkeys_need_update(struct registry_key_handle *key,
			     struct regsubkey_ctr *subkeys);
bool reg_values_need_update(struct registry_key_handle *key,
			    struct regval_ctr *values);

#endif /* _REG_DISPATCHER_H */
