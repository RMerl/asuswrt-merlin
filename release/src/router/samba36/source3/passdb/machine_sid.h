/*
 *  Unix SMB/CIFS implementation.
 *  Password and authentication handling
 *  Copyright (C) Jeremy Allison 		1996-2002
 *  Copyright (C) Andrew Tridgell		2002
 *  Copyright (C) Gerald (Jerry) Carter		2000
 *  Copyright (C) Stefan (metze) Metzmacher	2002
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The following definitions come from passdb/machine_sid.c  */

struct dom_sid  *get_global_sam_sid(void);
void reset_global_sam_sid(void) ;
bool sid_check_is_domain(const struct dom_sid  *sid);
bool sid_check_is_in_our_domain(const struct dom_sid  *sid);
