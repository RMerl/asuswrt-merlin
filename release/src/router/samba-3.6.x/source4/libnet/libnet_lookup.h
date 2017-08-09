/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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


struct libnet_Lookup {
	struct {
		const char *hostname;
		int type;
		struct resolve_context *resolve_ctx;
	} in;
	struct {
		const char **address;
	} out;
};


struct libnet_LookupDCs {
	struct {
		const char *domain_name;
		int name_type;
	} in;
	struct {
		int num_dcs;
		struct nbt_dc_name *dcs;
	} out;
};


struct libnet_LookupName {
	struct {
		const char *name;
		const char *domain_name;
	} in;
	struct {
		struct dom_sid *sid;
		int rid;
		enum lsa_SidType sid_type;
		const char *sidstr;
		const char *error_string;
	} out;
};


/*
 * Monitor messages sent from libnet_lookup.c functions
 */

struct msg_net_lookup_dc {
	const char *domain_name;
	const char *hostname;
	const char *address;
};

