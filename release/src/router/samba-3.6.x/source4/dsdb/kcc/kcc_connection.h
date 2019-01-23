/*
   Unix SMB/CIFS mplementation.

   KCC service

   Copyright (C) Crístian Deives 2009
   based on drepl service code

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

#ifndef _DSDB_REPL_KCC_CONNECTION_H_
#define _DSDB_REPL_KCC_CONNECTION_H_

struct kcc_connection {
	struct GUID obj_guid;
	struct GUID dsa_guid;
	uint8_t schedule[84];
};

struct kcc_connection_list {
	unsigned count;
	struct kcc_connection *servers;
};

#endif /* _DSDB_REPL_KCC_CONNECTION_H_ */
