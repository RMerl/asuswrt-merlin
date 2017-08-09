/* 
   Unix SMB/CIFS implementation.
   session handling for recording currently vailid vuids
   
   Copyright (C) tridge@samba.org 2001
   Copyright (C) Andew Bartlett <abartlet@samba.org> 2001
   Copyright (C) Gerald (Jerry) Carter  2006
   
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

/* a "session" is claimed when we do a SessionSetupX operation
   and is yielded when the corresponding vuid is destroyed.

   sessions are used to populate utmp and PAM session structures
*/

struct sessionid {
	uid_t uid;
	gid_t gid;
	fstring username;
	fstring hostname;
	fstring netbios_name;
	fstring remote_machine;
	fstring id_str;
	uint32  id_num;
	struct server_id pid;
	fstring ip_addr_str;
	time_t connect_start;
};

