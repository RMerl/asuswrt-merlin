/**
 * @file large_fd_set.c
 *
 * @brief Macro's and functions for manipulation of large file descriptor sets.
 */


#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#include <string.h> /* memset(), which is invoked by FD_ZERO() */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/library/large_fd_set.h>


#if !defined(cygwin) && defined(HAVE_WINSOCK_H)

void
netsnmp_large_fd_setfd(SOCKET fd, netsnmp_large_fd_set * fdset)
{
    unsigned        i;

    netsnmp_assert(fd != INVALID_SOCKET);

    if (fdset->lfs_set.fd_count == fdset->lfs_setsize)
        netsnmp_large_fd_set_resize(fdset, 2 * (fdset->lfs_setsize + 1));

    for (i = 0; i < fdset->lfs_set.fd_count; i++) {
        if (fdset->lfs_set.fd_array[i] == (SOCKET) (fd))
            break;
    }

    if (i == fdset->lfs_set.fd_count
        && fdset->lfs_set.fd_count < fdset->lfs_setsize) {
        fdset->lfs_set.fd_count++;
        fdset->lfs_set.fd_array[i] = fd;
    }
}

void
netsnmp_large_fd_clr(SOCKET fd, netsnmp_large_fd_set * fdset)
{
    unsigned        i;

    netsnmp_assert(fd != INVALID_SOCKET);

    for (i = 0; i < fdset->lfs_set.fd_count; i++) {
        if (fdset->lfs_set.fd_array[i] == fd) {
            while (i < fdset->lfs_set.fd_count - 1) {
                fdset->lfs_set.fd_array[i] =
                    fdset->lfs_set.fd_array[i + 1];
                i++;
            }
            fdset->lfs_set.fd_count--;
            break;
        }
    }
}

int
netsnmp_large_fd_is_set(SOCKET fd, netsnmp_large_fd_set * fdset)
{
    unsigned int    i;

    netsnmp_assert(fd != INVALID_SOCKET);

    for (i = 0; i < fdset->lfs_set.fd_count; i++) {
        if (fdset->lfs_set.fd_array[i] == fd)
            return 1;
    }
    return 0;
}

#else

void
netsnmp_large_fd_setfd(int fd, netsnmp_large_fd_set * fdset)
{
    netsnmp_assert(fd >= 0);

    while (fd >= (int)fdset->lfs_setsize)
        netsnmp_large_fd_set_resize(fdset, 2 * (fdset->lfs_setsize + 1));

    FD_SET(fd, fdset->lfs_setptr);
}

void
netsnmp_large_fd_clr(int fd, netsnmp_large_fd_set * fdset)
{
    netsnmp_assert(fd >= 0);

    if ((unsigned)fd < fdset->lfs_setsize)
        FD_CLR(fd, fdset->lfs_setptr);
}

int
netsnmp_large_fd_is_set(int fd, netsnmp_large_fd_set * fdset)
{
    netsnmp_assert(fd >= 0);

    return (unsigned)fd < fdset->lfs_setsize && FD_ISSET(fd, fdset->lfs_setptr);
}

#endif

void
netsnmp_large_fd_set_init(netsnmp_large_fd_set * fdset, int setsize)
{
    fdset->lfs_setsize = 0;
    fdset->lfs_setptr  = NULL;
#if !defined(cygwin) && defined(HAVE_WINSOCK_H)
    fdset->lfs_set.fd_count = 0;
#endif
    netsnmp_large_fd_set_resize(fdset, setsize);
}

int
netsnmp_large_fd_set_select(int numfds, netsnmp_large_fd_set *readfds,
                     netsnmp_large_fd_set *writefds,
                     netsnmp_large_fd_set *exceptfds,
                     struct timeval *timeout)
{
#if defined(cygwin) || !defined(HAVE_WINSOCK_H)
    /* Bit-set representation: make sure all fds have at least size 'numfds'. */
    if (readfds && readfds->lfs_setsize < numfds)
        netsnmp_large_fd_set_resize(readfds, numfds);
    if (writefds && writefds->lfs_setsize < numfds)
        netsnmp_large_fd_set_resize(writefds, numfds);
    if (exceptfds && exceptfds->lfs_setsize < numfds)
        netsnmp_large_fd_set_resize(exceptfds, numfds);
#else
    /* Array representation: no resizing is necessary. */
#endif

    return select(numfds,
            readfds ? readfds->lfs_setptr : NULL,
            writefds ? writefds->lfs_setptr : NULL,
            exceptfds ? exceptfds->lfs_setptr : NULL,
            timeout);
}

int
netsnmp_large_fd_set_resize(netsnmp_large_fd_set * fdset, int setsize)
{
    int             fd_set_bytes;

    if (fdset->lfs_setsize == setsize)
        goto success;

    if (setsize > FD_SETSIZE) {
        fd_set_bytes = NETSNMP_FD_SET_BYTES(setsize);
        if (fdset->lfs_setsize > FD_SETSIZE) {
            fdset->lfs_setptr = realloc(fdset->lfs_setptr, fd_set_bytes);
            if (!fdset->lfs_setptr)
                goto out_of_mem;
        } else {
            fdset->lfs_setptr = malloc(fd_set_bytes);
            if (!fdset->lfs_setptr)
                goto out_of_mem;
            *fdset->lfs_setptr = fdset->lfs_set;
        }
    } else {
        if (fdset->lfs_setsize > FD_SETSIZE) {
            fdset->lfs_set = *fdset->lfs_setptr;
            free(fdset->lfs_setptr);
        }
        fdset->lfs_setptr = &fdset->lfs_set;
    }

#if defined(cygwin) || !defined(HAVE_WINSOCK_H)
    {
        int             i;

        /*
         * Unix: when enlarging, clear the file descriptors defined in the
         * resized *fdset but that were not defined in the original *fdset.
         */
        for (i = fdset->lfs_setsize; i < setsize; i++)
            FD_CLR(i, fdset->lfs_setptr);
    }
#endif

    fdset->lfs_setsize = setsize;
#if !defined(cygwin) && defined(HAVE_WINSOCK_H)
    if (setsize < fdset->lfs_set.fd_count)
        fdset->lfs_set.fd_count = setsize;
#endif
success:
    return 1;

out_of_mem:
    fdset->lfs_setsize = 0;
#if !defined(cygwin) && defined(HAVE_WINSOCK_H)
    fdset->lfs_set.fd_count = 0;
#endif
    return 0;
}

void
netsnmp_large_fd_set_cleanup(netsnmp_large_fd_set * fdset)
{
    netsnmp_large_fd_set_resize(fdset, 0);
    fdset->lfs_setsize = 0;
    fdset->lfs_setptr  = NULL;
}

void
netsnmp_copy_fd_set_to_large_fd_set(netsnmp_large_fd_set * dst,
                                    const fd_set * src)
{
    netsnmp_large_fd_set_resize(dst, FD_SETSIZE);
    *dst->lfs_setptr = *src;
}

int
netsnmp_copy_large_fd_set_to_fd_set(fd_set * dst,
                                    const netsnmp_large_fd_set * src)
{
    /* Report failure if *src is larger than FD_SETSIZE. */
    if (src->lfs_setsize > FD_SETSIZE) {
        FD_ZERO(dst);
        return -1;
    }

    *dst = *src->lfs_setptr;

#if !(!defined(cygwin) && defined(HAVE_WINSOCK_H))
    {
        int             i;

        /* Unix: clear any file descriptors defined in *dst but not in *src. */
        for (i = src->lfs_setsize; i < FD_SETSIZE; ++i)
            FD_CLR(i, dst);
    }
#endif

    return 0;
}
