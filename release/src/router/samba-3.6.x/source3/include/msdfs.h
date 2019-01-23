/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   MSDfs services for Samba
   Copyright (C) Shirish Kalele 2000

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

#ifndef _MSDFS_H
#define _MSDFS_H

#define REFERRAL_TTL 600

/* Flags used in trans2 Get Referral reply */
#define DFSREF_REFERRAL_SERVER 0x1
#define DFSREF_STORAGE_SERVER  0x2

/* Referral sizes */
#define VERSION2_REFERRAL_SIZE 0x16
#define VERSION3_REFERRAL_SIZE 0x22
#define REFERRAL_HEADER_SIZE 0x08

/* Maximum number of referrals for each Dfs volume */
#define MAX_REFERRAL_COUNT   256
#define MAX_MSDFS_JUNCTIONS 256

struct client_dfs_referral {
	uint32 proximity;
	uint32 ttl;
	char *dfspath;
};

struct referral {
	char *alternate_path; /* contains the path referred */
	uint32 proximity;
	uint32 ttl; /* how long should client cache referral */
};

struct junction_map {
	char *service_name;
	char *volume_name;
	const char *comment;
	int referral_count;
	struct referral* referral_list;
};

struct dfs_path {
	char *hostname;
	char *servicename;
	char *reqpath;
	bool posix_path;
};

#endif /* _MSDFS_H */
