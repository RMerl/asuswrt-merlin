/* vi: set sw=4 ts=4: */
/*
 * File: e2fsbb.h
 *
 * Redefine a bunch of e2fsprogs stuff to use busybox routines
 * instead.  This makes upgrade between e2fsprogs versions easy.
 */

#ifndef E2FSBB_H
#define E2FSBB_H 1

#include "libbb.h"

/* version we've last synced against */
#define E2FSPROGS_VERSION "1.38"
#define E2FSPROGS_DATE "30-Jun-2005"

typedef long errcode_t;
#define ERRCODE_RANGE 8
#define error_message(code) strerror((int) (code & ((1<<ERRCODE_RANGE)-1)))

/* header defines */
#define ENABLE_HTREE 1
#define HAVE_ERRNO_H 1
#define HAVE_EXT2_IOCTLS 1
#define HAVE_LINUX_FD_H 1
#define HAVE_MNTENT_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NET_IF_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_MOUNT_H 1
#define HAVE_SYS_QUEUE_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* Endianness */
#if BB_BIG_ENDIAN
#define ENABLE_SWAPFS 1
#define WORDS_BIGENDIAN 1
#endif

#endif
