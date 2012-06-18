/*
   Unix SMB/CIFS implementation.
   ads (active directory) utility library

   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Andrew Tridgell 2001

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

#include "includes.h"

const char *ads_get_ldap_server_name(ADS_STRUCT *ads)
{
	return ads->config.ldap_server_name;
}
