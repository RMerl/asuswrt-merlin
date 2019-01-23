/* 
   Unix SMB/CIFS implementation.
   Packet handling
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

/*
 * A packet context is a wrapper around a bidirectional file descriptor,
 * hiding the handling of individual requests.
 */

struct packet_context;

/*
 * Initialize a packet context. The fd is given to the packet context, meaning
 * that it is automatically closed when the packet context is freed.
 */
struct packet_context *packet_init(TALLOC_CTX *mem_ctx, int fd);

/*
 * Pull data from the fd
 */
NTSTATUS packet_fd_read(struct packet_context *ctx);

/*
 * Sync read, wait for the next chunk
 */
NTSTATUS packet_fd_read_sync(struct packet_context *ctx, int timeout);

/*
 * Handle an incoming packet:
 * Return False if none is available
 * Otherwise return True and store the callback result in *status
 * Callback must either talloc_move or talloc_free buf
 */
bool packet_handler(struct packet_context *ctx,
		    bool (*full_req)(const uint8_t *buf,
				     size_t available,
				     size_t *length,
				     void *private_data),
		    NTSTATUS (*callback)(uint8_t *buf, size_t length,
					 void *private_data),
		    void *private_data,
		    NTSTATUS *status);

/*
 * How many bytes of outgoing data do we have pending?
 */
size_t packet_outgoing_bytes(struct packet_context *ctx);

/*
 * Push data to the fd
 */
NTSTATUS packet_fd_write(struct packet_context *ctx);

/*
 * Sync flush all outgoing bytes
 */
NTSTATUS packet_flush(struct packet_context *ctx);

/*
 * Send a list of DATA_BLOBs
 *
 * Example:  packet_send(ctx, 2, data_blob_const(&size, sizeof(size)),
 *			 data_blob_const(buf, size));
 */
NTSTATUS packet_send(struct packet_context *ctx, int num_blobs, ...);

/*
 * Get the packet context's file descriptor
 */
int packet_get_fd(struct packet_context *ctx);
