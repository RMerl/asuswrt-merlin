/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library
 *  Copyright (C) Michael Adam 2009
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

#ifndef __LIBSMBCONF_INIT_H__
#define __LIBSMBCONF_INIT_H__

struct smbconf_ctx;

/**
 * intialization dispatcher function.
 * takes source string in the form of "backend:path"
 */
sbcErr smbconf_init(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
		    const char *source);

#endif /*  _LIBSMBCONF_INIT_H_  */
