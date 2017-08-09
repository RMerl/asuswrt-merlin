/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   Copyright (C) Simo Sorce 2003
   Copyright (C) Gerald (Jerry) Carter 2005
   Copyright (C) Andrew Bartlett 2010

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

#ifndef PRIVILEGES_H
#define PRIVILEGES_H

#include "../librpc/gen_ndr/lsa.h"
#include "../librpc/gen_ndr/security.h"

/* common privilege bitmask defines */

#define SE_ALL_PRIVS                    (uint64_t)-1

/*
 * These are used in Lsa replies (srv_lsa_nt.c)
 */

typedef struct {
	TALLOC_CTX *mem_ctx;
	bool ext_ctx;
	uint32_t count;
	uint32_t control;
	struct lsa_LUIDAttribute *set;
} PRIVILEGE_SET;

const char* get_privilege_dispname( const char *name );

/*******************************************************************
 return the number of elements in the 'short' privlege array (traditional source3 behaviour)
*******************************************************************/

int num_privileges_in_short_list( void );

/*
  map a privilege id to the wire string constant
*/
const char *sec_privilege_name(enum sec_privilege privilege);

/*
  map a privilege id to a privilege display name. Return NULL if not found

  TODO: this should use language mappings
*/
const char *sec_privilege_display_name(enum sec_privilege privilege, uint16_t *language);

/*
  map a privilege name to a privilege id. Return -1 if not found
*/
enum sec_privilege sec_privilege_id(const char *name);

/*
  map a 'right' name to it's bitmap value. Return 0 if not found
*/
uint32_t sec_right_bit(const char *name);

/*
  assist in walking the table of privileges - return the LUID (low 32 bits) by index
*/
enum sec_privilege sec_privilege_from_index(int idx);

/*
  assist in walking the table of privileges - return the string constant by index
*/
const char *sec_privilege_name_from_index(int idx);

/*
  return true if a security_token has a particular privilege bit set
*/
bool security_token_has_privilege(const struct security_token *token, enum sec_privilege privilege);

/*
  set a bit in the privilege mask
*/
void security_token_set_privilege(struct security_token *token, enum sec_privilege privilege);
/*
  set a bit in the rights mask
*/
void security_token_set_right_bit(struct security_token *token, uint32_t right_bit);

void security_token_debug_privileges(int dbg_class, int dbg_lev, const struct security_token *token);

#endif /* PRIVILEGES_H */
