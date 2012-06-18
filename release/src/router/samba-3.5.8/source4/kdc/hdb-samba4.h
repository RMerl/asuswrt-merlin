/* 
   Unix SMB/CIFS implementation.

   KDC structures

   Copyright (C) Andrew Tridgell	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   
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

struct hdb_samba4_context {
	struct tevent_context *ev_ctx;
	struct loadparm_context *lp_ctx;
};

extern struct hdb_method hdb_samba4;

struct hdb_samba4_private {
	struct ldb_context *samdb;
	struct smb_iconv_convenience *iconv_convenience;
	struct loadparm_context *lp_ctx;
	struct ldb_message *msg;
	struct ldb_dn *realm_dn;
	hdb_entry_ex *entry_ex;
};
