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

#ifndef __LIBSMBCONF_REG_H__
#define __LIBSMBCONF_REG_H__

struct smbconf_ctx;

/**
 * initialization functions for the registry backend modules
 */

sbcErr smbconf_init_reg(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
			const char *path);


#endif /*  _LIBSMBCONF_REG_H_  */
