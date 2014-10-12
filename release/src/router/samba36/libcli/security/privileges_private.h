/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
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

/*********************************************************************
 Lookup the privilege mask for a privilege name
*********************************************************************/
bool se_priv_from_name( const char *name, uint64_t *privilege_mask );

/***************************************************************************
  return a privilege mask given a privilege id
****************************************************************************/
uint64_t sec_privilege_mask(enum sec_privilege privilege);

/***************************************************************************
 put all privileges into a mask
****************************************************************************/

void se_priv_put_all_privileges(uint64_t *privilege_mask);

/****************************************************************************
 Convert PRIVILEGE_SET to a privilege bitmap and back again
****************************************************************************/

bool se_priv_to_privilege_set( PRIVILEGE_SET *set, uint64_t privilege_mask );
bool privilege_set_to_se_priv( uint64_t *privilege_mask, struct lsa_PrivilegeSet *privset );
