
#include "network_backends.h"

#include "network.h"
#include "log.h"
#include "sys-mmap.h"

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#define MMAP_CHUNK_SIZE (512*1024)

#ifdef HAVE_LIBSMBCLIENT_H
#include <libsmbclient.h>
#endif

#define DBE 0

off_t mmap_align_offset(off_t start) {
	static long pagesize = 0;
	if (0 == pagesize) {
		pagesize = sysconf(_SC_PAGESIZE);
		force_assert(pagesize < MMAP_CHUNK_SIZE);
	}
	force_assert(start >= (start % pagesize));
	return start - (start % pagesize);
}

#if defined(USE_MMAP)

static volatile int sigbus_jmp_valid;
static sigjmp_buf sigbus_jmp;

static void sigbus_handler(int sig) {
	UNUSED(sig);
	if (sigbus_jmp_valid) siglongjmp(sigbus_jmp, 1);
	log_failed_assert(__FILE__, __LINE__, "SIGBUS");
}

#if 0
/* read mmap()ed data into local buffer */
#define LOCAL_BUFFERING 1
#endif

int network_write_file_chunk_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	chunk* const c = cq->first;
	off_t offset, toSend, file_end;
	ssize_t r;
	size_t mmap_offset, mmap_avail;
	const char *data;

	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;
	file_end = c->file.start + c->file.length; /* offset to file end in this chunk */

	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	if (0 != network_open_file_chunk(srv, con, cq)) return -1;

	/* setup SIGBUS handler, but don't activate sigbus_jmp_valid yet */
	if (0 != sigsetjmp(sigbus_jmp, 1)) {
		sigbus_jmp_valid = 0;

		log_error_write(srv, __FILE__, __LINE__, "sbd", "SIGBUS in mmap:",
			c->file.name, c->file.fd);

		munmap(c->file.mmap.start, c->file.mmap.length);
		c->file.mmap.start = MAP_FAILED;
#ifdef LOCAL_BUFFERING
		buffer_reset(c->mem);
#endif

		return -1;
	}

	signal(SIGBUS, sigbus_handler);

	/* mmap the buffer if offset is outside old mmap area or not mapped at all */
	if (MAP_FAILED == c->file.mmap.start
		|| offset < c->file.mmap.offset
		|| offset >= (off_t)(c->file.mmap.offset + c->file.mmap.length)) {

		if (MAP_FAILED != c->file.mmap.start) {
			munmap(c->file.mmap.start, c->file.mmap.length);
			c->file.mmap.start = MAP_FAILED;
		}

		/* Optimizations for the future:
		 *
		 * adaptive mem-mapping
		 *   the problem:
		 *     we mmap() the whole file. If someone has alot large files and 32bit
		 *     machine the virtual address area will be unrun and we will have a failing
		 *     mmap() call.
		 *   solution:
		 *     only mmap 16M in one chunk and move the window as soon as we have finished
		 *     the first 8M
		 *
		 * read-ahead buffering
		 *   the problem:
		 *     sending out several large files in parallel trashes the read-ahead of the
		 *     kernel leading to long wait-for-seek times.
		 *   solutions: (increasing complexity)
		 *     1. use madvise
		 *     2. use a internal read-ahead buffer in the chunk-structure
		 *     3. use non-blocking IO for file-transfers
		 *   */

		c->file.mmap.offset = mmap_align_offset(offset);

		/* all mmap()ed areas are MMAP_CHUNK_SIZE except the last which might be smaller */
		c->file.mmap.length = MMAP_CHUNK_SIZE;
		if (c->file.mmap.offset > file_end - (off_t)c->file.mmap.length) {
			c->file.mmap.length = file_end - c->file.mmap.offset;
		}

		if (MAP_FAILED == (c->file.mmap.start = mmap(NULL, c->file.mmap.length, PROT_READ, MAP_SHARED, c->file.fd, c->file.mmap.offset))) {
			log_error_write(srv, __FILE__, __LINE__, "ssbdoo", "mmap failed:",
				strerror(errno), c->file.name, c->file.fd, c->file.mmap.offset, (off_t) c->file.mmap.length);
			return -1;
		}

#if defined(LOCAL_BUFFERING)
		sigbus_jmp_valid = 1;
		buffer_copy_string_len(c->mem, c->file.mmap.start, c->file.mmap.length);
		sigbus_jmp_valid = 0;
#else
#  if defined(HAVE_MADVISE)
		/* don't advise files < 64Kb */
		if (c->file.mmap.length > (64*1024)) {
			/* darwin 7 is returning EINVAL all the time and I don't know how to
			 * detect this at runtime.
			 *
			 * ignore the return value for now */
			madvise(c->file.mmap.start, c->file.mmap.length, MADV_WILLNEED);
		}
#  endif
#endif
	}

	force_assert(offset >= c->file.mmap.offset);
	mmap_offset = offset - c->file.mmap.offset;
	force_assert(c->file.mmap.length > mmap_offset);
	mmap_avail = c->file.mmap.length - mmap_offset;
	if (toSend > (off_t) mmap_avail) toSend = mmap_avail;

#if defined(LOCAL_BUFFERING)
	data = c->mem->ptr + mmap_offset;
#else
	data = c->file.mmap.start + mmap_offset;
#endif

	sigbus_jmp_valid = 1;
#if defined(__WIN32)
	r = send(fd, data, toSend, 0);
#else /* __WIN32 */
	r = write(fd, data, toSend);
#endif /* __WIN32 */
	sigbus_jmp_valid = 0;

#if defined(__WIN32)
	if (r < 0) {
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
	if (r < 0) {
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

#endif /* USE_MMAP */

int network_write_smbfile_chunk_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {

	
	chunk* const c = cq->first;
	ssize_t r;
	off_t offset;
	off_t toSend;
	stat_cache_entry *sce = NULL;
	//off_t buffer_size = 256*1024;
	off_t buffer_size = 512*1024;
	char* buff = NULL;
#ifdef HAVE_LIBSMBCLIENT_H

	force_assert(NULL != c);
	force_assert(SMB_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);
	
	Cdbg(DBE, "*****************************************");
	
	offset = c->file.start + c->offset;
	toSend = ( c->file.length - c->offset > buffer_size) ? buffer_size : c->file.length - c->offset;
			
	if (toSend > *p_max_bytes) toSend = *p_max_bytes;
	
	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}
		
	if (0 != network_open_file_chunk(srv, con, cq)) return -1;

	buff = malloc(buffer_size);
	
	if(buff==NULL){
		Cdbg(DBE, "malloc fail");
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}
	
	memset(buff, '\0', buffer_size);		

	Cdbg(DBE, "size of off_t=%d", sizeof(off_t));
	Cdbg(DBE, "c->file.fd=[%d], c->file.length=[%lld]", c->file.fd, c->file.length);

	smbc_wrapper_lseek(con, c->file.fd, offset, SEEK_SET );
	
	Cdbg(DBE, "offset=[%lld], toSend=[%lld]", offset, toSend);
	r = smbc_wrapper_read(con, c->file.fd, buff, (size_t)toSend);
	
	Cdbg(DBE, "read complete, r=[%lld]", r);
	
	if( toSend == -1 || r < 0 ) {

		free(buff);
		
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

		free(buff);
		
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

	free(buff);
	
	if (r >= 0) {
		chunkqueue_mark_written(cq, r);
		*p_max_bytes -= r;
	}
#endif

	return (r > 0 && r == toSend) ? 0 : -3;
}

