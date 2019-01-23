/*
 *  Unix SMB/CIFS implementation.
 *
 *  SVCCTL RPC server keys initialization
 *
 *  Copyright (c) 2011      Andreas Schneider <asn@samba.org>
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

#ifndef SRV_SERVICES_REG_H
#define SRV_SERVICES_REG_H

bool svcctl_init_winreg(struct messaging_context *msg_ctx);

#endif /* SRV_SERVICES_REG_H */

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
