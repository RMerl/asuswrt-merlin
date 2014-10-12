/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Andrew Bartlett 2010
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

/* This API header is private between the 'net' binary and and libnet */

/* This function is to init the libnetapi subsystem, without
 * re-reading config files or setting debug levels etc */
NET_API_STATUS libnetapi_net_init(struct libnetapi_ctx **ctx);
