/*
   Unix SMB/CIFS implementation.
   DNS utility library
   Copyright (C) Gerald (Jerry) Carter           2006.
   Copyright (C) Jeremy Allison                  2007.

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

#ifndef _LIBADS_SITENAME_CACHE_H_
#define _LIBADS_SITENAME_CACHE_H_

bool sitename_store(const char *realm, const char *sitename);
char *sitename_fetch(const char *realm);
bool stored_sitename_changed(const char *realm, const char *sitename);

#endif /* _LIBADS_SITENAME_CACHE_H_ */
