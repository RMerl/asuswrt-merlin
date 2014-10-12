/* 
   Unix SMB/CIFS implementation.

   provide glue functions between heimdal and samba

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009
   
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

#include "heimdal/lib/hdb/hdb_locl.h"

krb5_error_code
hdb_sqlite_create(krb5_context context, HDB **db, const char *argument)
{
	return EINVAL;
}

