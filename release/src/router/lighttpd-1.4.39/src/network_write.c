#include "network_backends.h"

#include "network.h"
#include "log.h"

#include "sys-socket.h"

#include <unistd.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_LIBSMBCLIENT_H
#include <libsmbclient.h>
#endif

#define DBE 1

int network_write_mem_chunk(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	off_t c_len;
	ssize_t r;
	UNUSED(con);
	
	force_assert(NULL != c);
	force_assert(MEM_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= (off_t)buffer_string_length(c->mem));

	c_len = buffer_string_length(c->mem) - c->offset;
	if (c_len > *p_max_bytes) c_len = *p_max_bytes;

	if (0 == c_len) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

#if defined(__WIN32)
	if ((r = send(fd, c->mem->ptr + c->offset, c_len, 0)) < 0) {
		int lastError = WSAGetLastError();
		switch (lastError) {
		case WSAEINTR:
		case WSAEWOULDBLOCK:
			break;
		case WSAECONNRESET:
		case WSAETIMEDOUT:
		case WSAECONNABORTED:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sdd",
				"send failed: ", lastError, fd);
			return -1;
		}
	}
#else /* __WIN32 */
	if ((r = write(fd, c->mem->ptr + c->offset, c_len)) < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd",
				"write failed:", strerror(errno), fd);
			return -1;
		}
	}
#endif /* __WIN32 */

	if (r >= 0) {
		*p_max_bytes -= r;
		chunkqueue_mark_written(cq, r);
	}

	return (r > 0 && r == c_len) ? 0 : -3;
}

int network_write_chunkqueue_write(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	while (max_bytes > 0 && NULL != cq->first) {
		int r = -1;

		switch (cq->first->type) {
		case MEM_CHUNK:
			r = network_write_mem_chunk(srv, con, fd, cq, &max_bytes);
			break;
		case FILE_CHUNK:
			r = network_write_file_chunk_mmap(srv, con, fd, cq, &max_bytes);
			break;		
		}

		if (-3 == r) return 0;
		if (0 != r) return r;
	}

	return 0;
}

int network_write_chunkqueue_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	while (max_bytes > 0 && NULL != cq->first) {
		int r = -1;
		
		switch (cq->first->type) {
		case MEM_CHUNK:
			r = network_writev_mem_chunks(srv, con, fd, cq, &max_bytes);
			break;
		case FILE_CHUNK:
			r = network_write_file_chunk_sendfile(srv, con, fd, cq, &max_bytes);
			break;
		case SMB_CHUNK:
#ifndef EMBEDDED_EANBLE
			r = network_write_smbfile_chunk_sendfile(srv, con, fd, cq, &max_bytes);			
#endif
			break;
		}

		if (-3 == r) return 0;
		if (0 != r) return r;
	}

	return 0;
}
