/*
 *  Unix SMB/CIFS implementation.
 *
 *  Virtual Windows Registry Layer
 *
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007-2008
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

/*
 * Portion of reg_api that references regfio.c code.
 * These are the savekey and restorekey calls.
 * These calls are currently only used in the WINREG rpc server.
 */

#ifndef _REG_API_REGF_H
#define _REG_API_REGF_H

WERROR reg_restorekey(struct registry_key *key, const char *fname);
WERROR reg_savekey(struct registry_key *key, const char *fname);

#endif /* _REG_API_REGF_H */
