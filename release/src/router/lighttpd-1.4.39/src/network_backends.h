#ifndef _NETWORK_BACKENDS_H_
#define _NETWORK_BACKENDS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings.h"

#include <sys/types.h>

/* on linux 2.4.x you get either sendfile or LFS */
#if defined HAVE_SYS_SENDFILE_H && defined HAVE_SENDFILE && (!defined _LARGEFILE_SOURCE || defined HAVE_SENDFILE64) && defined(__linux__) && !defined HAVE_SENDFILE_BROKEN
# ifdef USE_SENDFILE
#  error "can't have more than one sendfile implementation"
# endif
# define USE_SENDFILE "linux-sendfile"
# define USE_LINUX_SENDFILE
#endif

#if defined HAVE_SENDFILE && (defined(__FreeBSD__) || defined(__DragonFly__))
# ifdef USE_SENDFILE
#  error "can't have more than one sendfile implementation"
# endif
# define USE_SENDFILE "freebsd-sendfile"
# define USE_FREEBSD_SENDFILE
#endif

#if defined HAVE_SENDFILE && defined(__APPLE__)
# ifdef USE_SENDFILE
#  error "can't have more than one sendfile implementation"
# endif
# define USE_SENDFILE "darwin-sendfile"
# define USE_DARWIN_SENDFILE
#endif

#if defined HAVE_SYS_SENDFILE_H && defined HAVE_SENDFILEV && defined(__sun)
# ifdef USE_SENDFILE
#  error "can't have more than one sendfile implementation"
# endif
# define USE_SENDFILE "solaris-sendfilev"
# define USE_SOLARIS_SENDFILEV
#endif

/* not supported so far
#if defined HAVE_SEND_FILE && defined(__aix)
# ifdef USE_SENDFILE
#  error "can't have more than one sendfile implementation"
# endif
# define USE_SENDFILE "aix-sendfile"
# define USE_AIX_SENDFILE
#endif
*/

#if defined HAVE_SYS_UIO_H && defined HAVE_WRITEV
# define USE_WRITEV
#endif

#if defined HAVE_SYS_MMAN_H && defined HAVE_MMAP && defined ENABLE_MMAP
# define USE_MMAP
#endif

#include "base.h"

/* return values:
 * >= 0 : no error
 *   -1 : error (on our side)
 *   -2 : remote close
 */

int network_write_chunkqueue_write(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes);

#if defined(USE_WRITEV)
int network_write_chunkqueue_writev(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes); /* fallback to write */
#endif

#if defined(USE_SENDFILE)
int network_write_chunkqueue_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes); /* fallback to write */
#endif

#if defined(USE_OPENSSL)
int network_write_chunkqueue_openssl(server *srv, connection *con, SSL *ssl, chunkqueue *cq, off_t max_bytes);
#endif

/* write next chunk(s); finished chunks are removed afterwards after successful writes.
 * return values: similar as backends (0 succes, -1 error, -2 remote close, -3 try again later (EINTR/EAGAIN)) */
/* next chunk must be MEM_CHUNK. use write()/send() */
int network_write_mem_chunk(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes);

#if defined(USE_WRITEV)
/* next chunk must be MEM_CHUNK. send multiple mem chunks using writev() */
int network_writev_mem_chunks(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes);
#else
/* fallback to write()/send() */
static inline int network_writev_mem_chunks(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	return network_write_mem_chunk(srv, con, fd, cq, p_max_bytes);
}
#endif

/* next chunk must be FILE_CHUNK. use temporary buffer (srv->tmp_buf) to read into, then write()/send() it */
int network_write_file_chunk_no_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes);

off_t mmap_align_offset(off_t start);
#if defined(USE_MMAP)
/* next chunk must be FILE_CHUNK. send mmap()ed file with write() */
int network_write_file_chunk_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes);
#else
/* fallback to no_mmap */
static inline int network_write_file_chunk_mmap(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	return network_write_file_chunk_no_mmap(srv, con, fd, cq, p_max_bytes);
}
#endif

#if defined(USE_SENDFILE)
int network_write_file_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes);
#else
/* fallback to mmap */
static inline int network_write_file_chunk_sendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t *p_max_bytes) {
	return network_write_file_chunk_mmap(srv, con, fd, cq, p_max_bytes);
}
#endif

/* next chunk must be FILE_CHUNK. return values: 0 success (=> -1 != cq->first->file.fd), -1 error */
int network_open_file_chunk(server *srv, connection *con, chunkqueue *cq);

#endif
