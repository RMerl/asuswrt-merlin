/*
 *  Unix SMB/CIFS implementation.
 *  error mapping functions
 *  Copyright (C) Andrew Tridgell 2001
 *  Copyright (C) Andrew Bartlett 2001
 *  Copyright (C) Tim Potter 2000
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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LIBSMB_ERRORMAP_WBC_H_
#define _LIBSMB_ERRORMAP_WBC_H_

/* The following definitions come from libsmb/errormap_wbc.c  */

NTSTATUS map_nt_error_from_wbcErr(wbcErr wbc_err);

#endif /* _LIBSMB_ERRORMAP_WBC_H_ */
