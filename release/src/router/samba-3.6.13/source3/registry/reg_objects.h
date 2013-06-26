/* 
   Samba's Internal Registry objects
   
   SMB parameters and setup
   Copyright (C) Gerald Carter                   2002-2006.
   Copyright (C) Michael Adam                    2007-2010
   
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

#ifndef _REG_OBJECTS_H /* _REG_OBJECTS_H */
#define _REG_OBJECTS_H

/* low level structure to contain registry values */

struct regval_blob;

/* container for registry values */

struct regval_ctr;

/* container for registry subkey names */

struct regsubkey_ctr;

/* The following definitions come from registry/reg_objects.c  */

WERROR regsubkey_ctr_init(TALLOC_CTX *mem_ctx, struct regsubkey_ctr **ctr);
WERROR regsubkey_ctr_reinit(struct regsubkey_ctr *ctr);
WERROR regsubkey_ctr_set_seqnum(struct regsubkey_ctr *ctr, int seqnum);
int regsubkey_ctr_get_seqnum(struct regsubkey_ctr *ctr);
WERROR regsubkey_ctr_addkey( struct regsubkey_ctr *ctr, const char *keyname );
WERROR regsubkey_ctr_delkey( struct regsubkey_ctr *ctr, const char *keyname );
bool regsubkey_ctr_key_exists( struct regsubkey_ctr *ctr, const char *keyname );
int regsubkey_ctr_numkeys( struct regsubkey_ctr *ctr );
char* regsubkey_ctr_specific_key( struct regsubkey_ctr *ctr, uint32_t key_index );
WERROR regval_ctr_init(TALLOC_CTX *mem_ctx, struct regval_ctr **ctr);
int regval_ctr_numvals(struct regval_ctr *ctr);
struct regval_blob* dup_registry_value(struct regval_blob *val);
void free_registry_value(struct regval_blob *val);
uint8_t* regval_data_p(struct regval_blob *val);
uint32_t regval_size(struct regval_blob *val);
char* regval_name(struct regval_blob *val);
uint32_t regval_type(struct regval_blob *val);
struct regval_blob* regval_ctr_specific_value(struct regval_ctr *ctr,
					      uint32_t idx);
struct regval_blob *regval_ctr_value_byname(struct regval_ctr *ctr,
					    const char *value);
bool regval_ctr_value_exists(struct regval_ctr *ctr, const char *value);
struct regval_blob *regval_compose(TALLOC_CTX *ctx, const char *name,
				   uint32_t type,
				   const uint8_t *data_p, size_t size);
int regval_ctr_addvalue(struct regval_ctr *ctr, const char *name, uint32_t type,
			const uint8_t *data_p, size_t size);
int regval_ctr_addvalue_sz(struct regval_ctr *ctr, const char *name, const char *data);
int regval_ctr_addvalue_multi_sz(struct regval_ctr *ctr, const char *name, const char **data);
int regval_ctr_copyvalue(struct regval_ctr *ctr, struct regval_blob *val);
int regval_ctr_delvalue(struct regval_ctr *ctr, const char *name);
struct regval_blob* regval_ctr_getvalue(struct regval_ctr *ctr,
					const char *name);
int regval_ctr_get_seqnum(struct regval_ctr *ctr);
WERROR regval_ctr_set_seqnum(struct regval_ctr *ctr, int seqnum);
uint32_t regval_dword(struct regval_blob *val);
const char *regval_sz(struct regval_blob *val);


#endif /* _REG_OBJECTS_H */
