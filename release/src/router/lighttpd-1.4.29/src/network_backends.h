#ifndef _NETWORK_BACKENDS_H_
#define _NETWORK_BACKENDS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings.h"

#include <sys/types.h>

/* on linux 2.4.x you get either sendfile or LFS */
#if defined HAVE_SYS_SENDFILE_H && defined HAVE_SENDFILE && (!defined _LARGEFILE_SOURCE || defined HAVE_SENDFILE64) && defined HAVE_WRITEV && defined(__linux__) && !defined HAVE_SENDFILE_BROKEN
# define USE_LINUX_SENDFILE
# include <sys/sendfile.h>
# include <sys/uio.h>
#endif

#if defined HAVE_SYS_UIO_H && defined HAVE_SENDFILE && defined HAVE_WRITEV && (defined(__FreeBSD__) || defined(__DragonFly__))
# define USE_FREEBSD_SENDFILE
# include <sys/uio.h>
#endif

#if defined HAVE_SYS_SENDFILE_H && defined HAVE_SENDFILEV && defined HAVE_WRITEV && defined(__sun)
# define USE_SOLARIS_SENDFILEV
# include <sys/sendfile.h>
# include <sys/uio.h>
#endif

#if defined HAVE_SYS_UIO_H && defined HAVE_WRITEV
# define USE_WRITEV
# include <sys/uio.h>
#endif

#if defined HAVE_SYS_MMAN_H && defined HAVE_MMAP
# define USE_MMAP
# include <sys/mman.h>
/* NetBSD 1.3.x needs it */
# ifndef MAP_FAILED
#  define MAP_FAILED -1
# endif
#endif

#if defined HAVE_SYS_UIO_H && defined HAVE_WRITEV && defined HAVE_SEND_FILE && defined(__aix)
# define USE_AIX_SENDFILE
#endif

#include "base.h"

/* return values:
 * >= 0 : chunks completed
 *   -1 : error (on our side)
 *   -2 : remote close
 */

int network_write_chunkqueue_write(server *srv, connection *con, int fd, chunkqueue *cq);
int network_write_chunkqueue_writev(server *srv, connection *con, int fd, chunkqueue *cq);
int network_write_chunkqueue_linuxsendfile(server *srv, connection *con, int fd, chunkqueue *cq);
int network_write_chunkqueue_freebsdsendfile(server *srv, connection *con, int fd, chunkqueue *cq);
int network_write_chunkqueue_solarissendfilev(server *srv, connection *con, int fd, chunkqueue *cq);
#ifdef USE_OPENSSL
int network_write_chunkqueue_openssl(server *srv, connection *con, SSL *ssl, chunkqueue *cq);
#endif

#endif
