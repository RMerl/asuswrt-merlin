/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006

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

struct libnet_UnbecomeDC {
	struct {
		const char *domain_dns_name;
		const char *domain_netbios_name;
		const char *source_dsa_address;
		const char *dest_dsa_netbios_name;
	} in;

	struct {
		const char *error_string;
	} out;
};
