#include "network_backends.h"

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include "sys-socket.h"

#include <sys/time.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#define DBE 0

int network_open_file_chunk(server *srv, connection *con, chunkqueue *cq) {
	chunk* const c = cq->first;
	off_t file_size, offset, toSend;

	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type || SMB_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	//Cdbg(1,"0 c->file.start=%lld, c->offset=%lld, c->file.length=%lld", c->file.start, c->offset, c->file.length);
	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;

	if (-1 == c->file.fd) {
		stat_cache_entry *sce = NULL;

		if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
			log_error_write(srv, __FILE__, __LINE__, "ssb", "stat-cache failed:", strerror(errno), c->file.name);
			return -1;
		}
		
		if( c->type == SMB_CHUNK ){
			if (-1 == (c->file.fd = smbc_wrapper_open(con,c->file.name->ptr, O_RDONLY, 0755))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));
				return -1;
			}
		}
		else{
			if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY|O_NOCTTY))) {
				log_error_write(srv, __FILE__, __LINE__, "ssb", "open failed:", strerror(errno), c->file.name);
				return -1;
			}

			fd_close_on_exec(c->file.fd);
		}

		file_size = sce->st.st_size;
	} 
	else {	
		struct stat st;

		if( c->type == SMB_CHUNK ){
			if (-1 == smbc_wrapper_stat(con, c->file.name->ptr, &st)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "smbc_wrapper_stat failed:", strerror(errno));
				return -1;
			}
		}
		else{
			if (-1 == fstat(c->file.fd, &st)) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "fstat failed:", strerror(errno));
				return -1;
			}
		}
		
		file_size = st.st_size;
	}
	//Cdbg(1,"1 file_size=%d, toSend=%d, offset=%d", file_size, toSend, offset);
	
	if (offset > file_size || toSend > file_size || offset > file_size - toSend) {
		//Cdbg(1,"2 file_size=%d", file_size);
		log_error_write(srv, __FILE__, __LINE__, "sb", "file was shrinked:", c->file.name);
		return -1;
	}

	return 0;
}

int network_write_file_chunk_no_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	off_t offset, toSend;
	ssize_t r;

	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;
	if (toSend > 64*1024) toSend = 64*1024; /* max read 64kb in one step */
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;

	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	if (0 != network_open_file_chunk(srv, con, cq)) return -1;

	buffer_string_prepare_copy(srv->tmp_buf, toSend);

	if (-1 == lseek(c->file.fd, offset, SEEK_SET)) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "lseek: ", strerror(errno));
		return -1;
	}
	if (-1 == (toSend = read(c->file.fd, srv->tmp_buf->ptr, toSend))) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "read: ", strerror(errno));
		return -1;
	}

#if defined(__WIN32)
	if ((r = send(fd, srv->tmp_buf->ptr, toSend, 0)) < 0) {
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
	if ((r = write(fd, srv->tmp_buf->ptr, toSend)) < 0) {
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

	return (r > 0 && r == toSend) ? 0 : -3;
}
