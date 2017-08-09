/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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

struct wreplsrv_pull_cycle_io {
	struct {
		struct wreplsrv_partner *partner;
		uint32_t num_owners;
		struct wrepl_wins_owner *owners;
		struct wreplsrv_out_connection *wreplconn;
	} in;
};

struct wreplsrv_push_notify_io {
	struct {
		struct wreplsrv_partner *partner;
		bool inform;
		bool propagate;
	} in;
};
