/**
 * @file  large_fd_set.h
 *
 * @brief Macro's and functions for manipulation of large file descriptor sets.
 */


#ifndef LARGE_FD_SET_H
#define LARGE_FD_SET_H


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if defined(HAVE_WINSOCK_H) && !defined(_WINSOCKAPI_) && !defined(_WINSOCK_H)
#error <winsock.h> or <winsock2.h> must have been included before this file.
#endif


#ifdef __cplusplus
extern "C" {
#endif



/**
 * Add socket fd to the set *fdset if not yet present.
 * Enlarges the set if necessary.
 */
#define NETSNMP_LARGE_FD_SET(fd, fdset) \
                    netsnmp_large_fd_setfd(fd, fdset)

/**
 * Remove socket fd from the set *fdset.
 * Do nothing if fd is not present in *fdset.
 * Do nothing if fd >= fdset->lfs_setsize.
 */
#define NETSNMP_LARGE_FD_CLR(fd, fdset) \
                    netsnmp_large_fd_clr(fd, fdset)

/**
 * Test whether set *fdset contains socket fd.
 * Evaluates to zero (false) if fd >= fdset->lfs_setsize.
 */
#define NETSNMP_LARGE_FD_ISSET(fd, fdset) \
                    netsnmp_large_fd_is_set(fd, fdset)

#if !defined(cygwin) && defined(HAVE_WINSOCK_H)

/**
 * Number of bytes needed to store a number of file descriptors as a
 * struct fd_set.
 */
#define NETSNMP_FD_SET_BYTES(setsize)                           \
    (sizeof(fd_set) + ((setsize) - FD_SETSIZE) * sizeof(SOCKET))

/** Remove all sockets from the set *fdset. */
#define NETSNMP_LARGE_FD_ZERO(fdset) \
    do { (fdset)->lfs_setptr->fd_count = 0; } while(0)


NETSNMP_IMPORT
void   netsnmp_large_fd_setfd( SOCKET fd, netsnmp_large_fd_set *fdset);
NETSNMP_IMPORT
void   netsnmp_large_fd_clr(   SOCKET fd, netsnmp_large_fd_set *fdset);
NETSNMP_IMPORT
int    netsnmp_large_fd_is_set(SOCKET fd, netsnmp_large_fd_set *fdset);

#else

/**
 * Size of a single element of the array with file descriptor bitmasks.
 *
 * According to SUSv2, this array must have the name fds_bits. See also
 * <a href="http://www.opengroup.org/onlinepubs/007908775/xsh/systime.h.html">The Single UNIX Specification, Version 2, &lt;sys/time.h&gt;</a>.
 */
#define NETSNMP_FD_MASK_SIZE sizeof(((fd_set*)0)->fds_bits[0])

/** Number of bits in one element of the fd_set.fds_bits array. */
#define NETSNMP_BITS_PER_FD_MASK (8 * NETSNMP_FD_MASK_SIZE)

/** Number of elements needed for the fds_bits array. */
#define NETSNMP_FD_SET_ELEM_COUNT(setsize) \
    (setsize + NETSNMP_BITS_PER_FD_MASK - 1) / NETSNMP_BITS_PER_FD_MASK

/**
 * Number of bytes needed to store a number of file descriptors as a
 * struct fd_set.
 */
#define NETSNMP_FD_SET_BYTES(setsize)                                   \
    (sizeof(fd_set) + NETSNMP_FD_SET_ELEM_COUNT((setsize) - FD_SETSIZE) \
     * NETSNMP_FD_MASK_SIZE)

/** Remove all file descriptors from the set *fdset. */
#define NETSNMP_LARGE_FD_ZERO(fdset)                            \
    do {                                                        \
        memset((fdset)->lfs_setptr, 0,                          \
               NETSNMP_FD_SET_BYTES((fdset)->lfs_setsize));     \
    } while (0)


void   netsnmp_large_fd_setfd( int fd, netsnmp_large_fd_set *fdset);
void   netsnmp_large_fd_clr(   int fd, netsnmp_large_fd_set *fdset);
int    netsnmp_large_fd_is_set(int fd, netsnmp_large_fd_set *fdset);

#endif

/**
 * Initialize a netsnmp_large_fd_set structure.
 *
 * Note: this function only initializes the lfs_setsize and lfs_setptr
 * members of netsnmp_large_fd_set, not the file descriptor set itself.
 * The file descriptor set must be initialized separately, e.g. via
 * NETSNMP_LARGE_FD_CLR().
 */
NETSNMP_IMPORT
void   netsnmp_large_fd_set_init(   netsnmp_large_fd_set *fdset, int setsize);

/**
 * Modify the size of a file descriptor set and preserve the first
 * min(fdset->lfs_setsize, setsize) file descriptors.
 *
 * Returns 1 upon success or 0 if memory allocation failed.
 */
int    netsnmp_large_fd_set_resize( netsnmp_large_fd_set *fdset, int setsize);

/**
 * Synchronous I/O multiplexing for large file descriptor sets.
 *
 * On POSIX systems, any file descriptor set with size below numfds will be
 * resized before invoking select().
 *
 * @see See also select(2) for more information.
 */
NETSNMP_IMPORT
int    netsnmp_large_fd_set_select(int numfds, netsnmp_large_fd_set *readfds,
			    netsnmp_large_fd_set *writefds,
			    netsnmp_large_fd_set *exceptfds,
			    struct timeval *timeout);

/** Deallocate the memory allocated by netsnmp_large_fd_set_init. */
NETSNMP_IMPORT
void   netsnmp_large_fd_set_cleanup(netsnmp_large_fd_set *fdset);

/**
 * Copy an fd_set to a netsnmp_large_fd_set structure.
 *
 * @note dst must have been initialized before this function is called.
 */
void   netsnmp_copy_fd_set_to_large_fd_set(netsnmp_large_fd_set *dst,
                                                   const fd_set *src);

/**
 * Copy a netsnmp_large_fd_set structure into an fd_set.
 *
 * @return 0 upon success, -1 when copying fails because *src is too large to
 *         fit into *dst.
 */
int    netsnmp_copy_large_fd_set_to_fd_set(              fd_set *dst,
                                     const netsnmp_large_fd_set *src);

#ifdef __cplusplus
}
#endif


#endif /* LARGE_FD_SET_H */
