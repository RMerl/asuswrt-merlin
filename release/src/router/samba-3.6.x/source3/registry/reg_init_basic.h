/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _REG_INIT_BASIC_H
#define _REG_INIT_BASIC_H

WERROR registry_init_common(void);
WERROR registry_init_basic(void);

#endif /* _REG_INIT_BASIC_H */
