/* 
   Unix SMB/CIFS implementation.
   Samba3 ctdb connection handling
   Copyright (C) Volker Lendecke 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _CTDBD_CONN_H
#define _CTDBD_CONN_H

#include <tdb.h>

struct ctdbd_connection;
struct messaging_context;
struct messaging_rec;

NTSTATUS ctdbd_messaging_connection(TALLOC_CTX *mem_ctx,
				    struct ctdbd_connection **pconn);

uint32 ctdbd_vnn(const struct ctdbd_connection *conn);

NTSTATUS ctdbd_register_msg_ctx(struct ctdbd_connection *conn,
				struct messaging_context *msg_ctx);
struct messaging_context *ctdb_conn_msg_ctx(struct ctdbd_connection *conn);

int ctdbd_conn_get_fd(struct ctdbd_connection *conn);

NTSTATUS ctdbd_messaging_send(struct ctdbd_connection *conn,
			      uint32 dst_vnn, uint64 dst_srvid,
			      struct messaging_rec *msg);

bool ctdbd_process_exists(struct ctdbd_connection *conn, uint32 vnn,
			  pid_t pid);

char *ctdbd_dbpath(struct ctdbd_connection *conn,
		   TALLOC_CTX *mem_ctx, uint32_t db_id);

NTSTATUS ctdbd_db_attach(struct ctdbd_connection *conn, const char *name,
			 uint32_t *db_id, int tdb_flags);

NTSTATUS ctdbd_migrate(struct ctdbd_connection *conn, uint32 db_id,
		       TDB_DATA key);

NTSTATUS ctdbd_fetch(struct ctdbd_connection *conn, uint32 db_id,
		     TDB_DATA key, TALLOC_CTX *mem_ctx, TDB_DATA *data);

NTSTATUS ctdbd_traverse(uint32 db_id,
			void (*fn)(TDB_DATA key, TDB_DATA data,
				   void *private_data),
			void *private_data);

NTSTATUS ctdbd_register_ips(struct ctdbd_connection *conn,
			    const struct sockaddr_storage *server,
			    const struct sockaddr_storage *client,
			    void (*release_ip_handler)(const char *ip_addr,
						       void *private_data),
			    void *private_data);

NTSTATUS ctdbd_register_reconfigure(struct ctdbd_connection *conn);

NTSTATUS ctdbd_control_local(struct ctdbd_connection *conn, uint32 opcode, 
			     uint64_t srvid, uint32_t flags, TDB_DATA data, 
			     TALLOC_CTX *mem_ctx, TDB_DATA *outdata,
			     int *cstatus);
NTSTATUS ctdb_watch_us(struct ctdbd_connection *conn);
NTSTATUS ctdb_unwatch(struct ctdbd_connection *conn);

#endif /* _CTDBD_CONN_H */
