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


enum service_type { DOMAIN_SAMR, DOMAIN_LSA };

/*
 * struct definition for opening a domain
 */

struct libnet_DomainOpen {
	struct {
		enum service_type type;
		const char *domain_name;
		uint32_t access_mask;
	} in;
	struct {
		struct policy_handle domain_handle;
		const char *error_string;
	} out;
};


struct libnet_DomainClose {
	struct {
		enum service_type type;
		const char *domain_name;
	} in;
	struct {
		const char *error_string;
	} out;
};


struct libnet_DomainList {
	struct {
		const char *hostname;
	} in;
	struct {
		int count;
		
		struct domainlist {
			const char *sid;
			const char *name;
		} *domains;

		const char *error_string;
	} out;
};


struct msg_rpc_lookup_domain {
	const char *domain_name;
};
