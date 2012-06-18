/*
 * $Id: $
 *
 * Generic IO abstraction for files, sockets, http streams, etc
 */

#ifndef _IO_H_
#define _IO_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef WIN32
# include <stdint.h>
# include <netinet/in.h>
# include <sys/socket.h>
#else
#define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <ws2tcpip.h>
#endif

#include "io-errors.h"

typedef void* IOHANDLE;
typedef void* IO_WAITHANDLE;

#define INVALID_HANDLE NULL

#ifdef WIN32 /* MSVC, really */
# define SOCKET_T SOCKET
# define FILE_T HANDLE
# define mode_t int
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
# define SOCKET_T int
# define FILE_T int
#endif


/*
 * Valid protocol options:
 *
 * udplisten:
 *  multicast=0/1 (default 0)
 *  mcast_group=multicast ip (required if multicast=1)
 *  mcast_ttl=<n> (defaults to 4)
 *
 *  Example URI for UPnP:
 *    udplisten://1900?multicast=1&mcast_group=239.255.255.250&mcast_ttl=4
 *
 * listen:
 *  backlog=<n> (default to 5)
 *  reuse=0/1 (defaults to 1) - SO_REUSEADDR
 */

extern int io_init(void);
extern int io_deinit(void);
extern void io_set_errhandler(void(*err_handler)(int, char *));
extern void io_set_lockhandler(void(*lock_handler)(int));

extern IOHANDLE io_new(void);
extern int io_open(IOHANDLE io, char *fmt, ...);
extern int io_close(IOHANDLE io);
extern int io_read(IOHANDLE io, unsigned char *buf, uint32_t *len);
extern int io_read_timeout(IOHANDLE io, unsigned char *buf, uint32_t *len,
                           uint32_t *ms);
extern int io_write(IOHANDLE io, unsigned char *buf, uint32_t *len);
extern int io_printf(IOHANDLE io, char *fmt, ...);
extern int io_size(IOHANDLE io, uint64_t *size);
extern int io_setpos(IOHANDLE io, uint64_t offset, int whence);
extern int io_getpos(IOHANDLE io, uint64_t *pos);
extern int io_buffer(IOHANDLE io); /* unimplemented */
extern int io_readline(IOHANDLE io, unsigned char *buf, uint32_t *len);
extern int io_readline_timeout(IOHANDLE io, unsigned char *buf, uint32_t *len,
    uint32_t *ms);
extern char* io_errstr(IOHANDLE io);
extern int io_errcode(IOHANDLE io);
extern void io_dispose(IOHANDLE io);

/* Given a listen:// socket, accept and return the child socket.
 * the child IOHANDLE should have already been allocated with io_new, and
 * even on error must be io_dispose'd */
extern int io_listen_accept(IOHANDLE io, IOHANDLE child, struct in_addr *host);

/* Given an iohandle that has already been io_new'ed, attach an exising
 * socket or file handle to the iohandle */
extern int io_file_attach(IOHANDLE io, FILE_T fd);
extern int io_socket_attach(IOHANDLE io, SOCKET_T fd);

/* udp equivalents to recvfrom/sendto */
extern int io_udp_recvfrom(IOHANDLE io, unsigned char *buf, uint32_t *len,
                           struct sockaddr_in *si_remote, socklen_t *si_len);
extern int io_udp_sendto(IOHANDLE io, unsigned char *buf, uint32_t *len,
                         struct sockaddr_in *si_remote, socklen_t si_len);

extern int io_isproto(IOHANDLE io, char *proto);


/* Some wrappers to level os-specific file handling */
extern int io_stat(unsigned char *file);
extern int io_find(unsigned char *file, unsigned char *current, unsigned char **returned, int len);

#define IO_WAIT_READ  1
#define IO_WAIT_WRITE 2
#define IO_WAIT_ERROR 4

extern IO_WAITHANDLE io_wait_new(void);
extern int io_wait_add(IO_WAITHANDLE wh, IOHANDLE io, int type);
extern int io_wait(IO_WAITHANDLE wh, uint32_t *ms);
extern int io_wait_status(IO_WAITHANDLE wh, IOHANDLE io);
extern int io_wait_dispose(IO_WAITHANDLE wh);

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

#endif /* _IO_H_ */
