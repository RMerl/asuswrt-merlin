/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009

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

/*
   flags for dsdb_request_add_controls(). For the module functions,
   the upper 16 bits are in dsdb/samdb/ldb_modules/util.h
*/
#define DSDB_SEARCH_SEARCH_ALL_PARTITIONS     0x0001
#define DSDB_SEARCH_SHOW_DELETED              0x0002
#define DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT 0x0004
#define DSDB_SEARCH_REVEAL_INTERNALS          0x0008
#define DSDB_SEARCH_SHOW_EXTENDED_DN          0x0010
#define DSDB_MODIFY_RELAX		      0x0020
#define DSDB_MODIFY_PERMISSIVE		      0x0040
#define DSDB_FLAG_AS_SYSTEM		      0x0080
#define DSDB_TREE_DELETE		      0x0100
#define DSDB_SEARCH_ONE_ONLY		      0x0200 /* give an error unless 1 record */
#define DSDB_SEARCH_SHOW_RECYCLED	      0x0400
#define DSDB_PROVISION			      0x0800

bool is_attr_in_list(const char * const * attrs, const char *attr);

#define DSDB_SECRET_ATTRIBUTES_EX(sep) \
	"currentValue" sep \
	"dBCSPwd" sep \
	"initialAuthIncoming" sep \
	"initialAuthOutgoing" sep \
	"lmPwdHistory" sep \
	"ntPwdHistory" sep \
	"priorValue" sep \
	"supplementalCredentials" sep \
	"trustAuthIncoming" sep \
	"trustAuthOutgoing" sep \
	"unicodePwd"

#define DSDB_SECRET_ATTRIBUTES_COMMA ,
#define DSDB_SECRET_ATTRIBUTES DSDB_SECRET_ATTRIBUTES_EX(DSDB_SECRET_ATTRIBUTES_COMMA)
