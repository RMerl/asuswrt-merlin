/*
 * Unix SMB/CIFS implementation.
 *
 * Endpoint server for the epmapper pipe
 *
 * Copyright (C) 2010-2011 Andreas Schneider <asn@samba.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SRV_EPMAPPER_H_
#define _SRV_EPMAPPER_H_

/**
 * @brief Cleanup memory and other stuff.
 */
void srv_epmapper_cleanup(void);

bool srv_epmapper_delete_endpoints(struct pipes_struct *p);

#endif /*_SRV_EPMAPPER_H_ */

/* vim: set ts=8 sw=8 noet cindent syntax=c.doxygen: */
