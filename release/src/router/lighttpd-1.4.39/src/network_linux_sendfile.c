#include "network_backends.h"

#if defined(USE_LINUX_SENDFILE)

#include "network.h"
#include "log.h"

#include <sys/sendfile.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_LIBSMBCLIENT_H
#include <libsmbclient.h>
#define BUFF_SIZE (1460*10)
#endif

#define DBE 0

int network_write_file_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	ssize_t r;
	off_t offset;
	off_t toSend;
	
	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;
	
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;
	
	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	if (0 != network_open_file_chunk(srv, con, cq)) return -1;
	
	if (-1 == (r = sendfile(fd, c->file.fd, &offset, toSend))) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd",
					"sendfile failed:", strerror(errno), fd);
			return -1;
		}
	}

	if (r >= 0) {
		chunkqueue_mark_written(cq, r);
		*p_max_bytes -= r;
	}
	
	return (r > 0 && r == toSend) ? 0 : -3;
}

int network_write_smbfile_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {

	
	chunk* const c = cq->first;
	ssize_t r;
	off_t offset;
	off_t toSend;
	stat_cache_entry *sce = NULL;

#ifdef HAVE_LIBSMBCLIENT_H

	force_assert(NULL != c);
	force_assert(SMB_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	char buff[BUFF_SIZE]={0};

	Cdbg(DBE, "*****************************************");
	
	offset = c->file.start + c->offset;
	toSend = ( c->file.length - c->offset > BUFF_SIZE) ? BUFF_SIZE : c->file.length - c->offset;
			
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;
	
	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}
		
	if (0 != network_open_file_chunk(srv, con, cq)) return -1;
	
	//Cdbg(DBE, "c->file.fd=[%d], c->file.length=[%zu]", c->file.fd, c->file.length);

	smbc_wrapper_lseek(con, c->file.fd, offset, SEEK_SET );
	
	//Cdbg(DBE, "offset=%zu, toSend=%zu", offset, toSend);
	r = smbc_wrapper_read(con, c->file.fd, buff, toSend);
	
	Cdbg(DBE, "toSend=[%lld], errno=[%d], r=[%lld]", toSend, errno, r);
	
	if( toSend == -1 || r < 0 ) {
		smbc_wrapper_close(con, c->file.fd);
		
		switch (errno) {
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd",
					"sendfile failed:", strerror(errno), fd);
			return -1;
		}
	}
	else if (r == 0) {
		int oerrno = errno;
		/* We got an event to write but we wrote nothing
		 *
		 * - the file shrinked -> error
		 * - the remote side closed inbetween -> remote-close */
		
		if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
			/* file is gone ? */
			return -1;
		}
		
		if (offset > sce->st.st_size) {
			/* file shrinked, close the connection */
			errno = oerrno;
	
			return -1;
		}
	
		errno = oerrno;
		return -2;
	}
	
	r = write(fd, buff, toSend);
	
	if (r >= 0) {
		chunkqueue_mark_written(cq, r);
		*p_max_bytes -= r;
	}
#endif

	return (r > 0 && r == toSend) ? 0 : -3;
}

#endif /* USE_LINUX_SENDFILE */
