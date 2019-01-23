/*
   Unix SMB/CIFS implementation.

   provide glue functions between heimdal and samba

   Copyright (C) Andrew Tridgell 2005

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

#include "../replace/replace.h"
#include "heimdal/lib/gssapi/gssapi_mech.h"

gssapi_mech_interface __gss_ntlm_initialize(void)
{
	return NULL;
}
