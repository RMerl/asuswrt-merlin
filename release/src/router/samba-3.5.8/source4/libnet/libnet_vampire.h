/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Brad Henry 2005
   
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

#ifndef __LIBNET_VAMPIRE_H__
#define __LIBNET_VAMPIRE_H__

struct libnet_Vampire {
	struct {
		const char *domain_name;
		const char *netbios_name;
		const char *targetdir;
	} in;
	
	struct {
		struct dom_sid *domain_sid;
		const char *domain_name;
		const char *error_string;
	} out;
};


#endif /* __LIBNET_VAMPIRE_H__ */
