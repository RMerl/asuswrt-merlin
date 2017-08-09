/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000,
 *  Copyright (C) Jeremy Allison               2001-2002,
 *  Copyright (C) Gerald Carter		       2000-2004,
 *  Copyright (C) Tim Potter                   2001-2002.
 *  Copyright (C) Guenther Deschner            2009-2010.
 *  Copyright (C) Andreas Schneider            2010.
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

#ifndef _RPC_SERVER_SPOOLSS_SRV_SPOOLSS_NT_H_
#define _RPC_SERVER_SPOOLSS_SRV_SPOOLSS_NT_H_

/* The following definitions come from rpc_server/srv_spoolss_nt.c  */
void srv_spoolss_cleanup(void);

void do_drv_upgrade_printer(struct messaging_context *msg,
			    void *private_data,
			    uint32_t msg_type,
			    struct server_id server_id,
			    DATA_BLOB *data);
void update_monitored_printq_cache(struct messaging_context *msg_ctx);

#endif /* _RPC_SERVER_SPOOLSS_SRV_SPOOLSS_NT_H_ */
