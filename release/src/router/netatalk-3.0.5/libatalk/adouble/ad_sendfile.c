/*
 * Copyright (c) 1999 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 *
 * NOTE: the following uses the fact that sendfile() only exists on
 * machines with SA_RESTART behaviour. this is all very machine specific. 
 *
 * sendfile chainsaw from samba.
 Unix SMB/Netbios implementation.
 Version 2.2.x / 3.0.x
 sendfile implementations.
 Copyright (C) Jeremy Allison 2002.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef WITH_SENDFILE
#include <stdio.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>  

#include <atalk/adouble.h>
#include <atalk/logger.h>

#include "ad_lock.h"

#if defined(SENDFILE_FLAVOR_LINUX)
#include <sys/sendfile.h>

ssize_t sys_sendfile(int tofd, int fromfd, off_t *offset, size_t count)
{
    return sendfile(tofd, fromfd, offset, count);
}

#elif defined(SENDFILE_FLAVOR_SOLARIS)
#include <sys/sendfile.h>

ssize_t sys_sendfile(int tofd, int fromfd, off_t *offset, size_t count)
{
    return sendfile(tofd, fromfd, offset, count);
}

#elif defined(SENDFILE_FLAVOR_BSD )
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
ssize_t sys_sendfile(int tofd, int fromfd, off_t *offset, size_t count)
{
    off_t len;
    int ret;

    ret = sendfile(fromfd, tofd, *offset, count, NULL, &len, 0);

    *offset += len;

    if (ret != 0)
        return -1;
    return len;
}

#else

ssize_t sys_sendfile(int out_fd, int in_fd, off_t *_offset, size_t count)
{
    /* No sendfile syscall. */
    errno = ENOSYS;
    return -1;
}
#endif

/* ------------------------------- */
int ad_readfile_init(const struct adouble *ad, 
				       const int eid, off_t *off,
				       const int end)
{
  int fd;

  if (end) 
    *off = ad_size(ad, eid) - *off;

  if (eid == ADEID_DFORK) {
    fd = ad_data_fileno(ad);
  } else {
    *off += ad_getentryoff(ad, eid);
    fd = ad_reso_fileno(ad);
  }

  return fd;
}


/* ------------------------ */
#if 0
#ifdef HAVE_SENDFILE_WRITE
/* read from a socket and write to an adouble file */
ssize_t ad_writefile(struct adouble *ad, const int eid, 
		     const int sock, off_t off, const int end,
		     const size_t len)
{
#ifdef __linux__
  ssize_t cc;
  int fd;

  fd = ad_sendfile_init(ad, eid, &off, end);
  if ((cc = sys_sendfile(fd, sock, &off, len)) < 0)
    return -1;

  if ((eid != ADEID_DFORK) && (off > ad_getentrylen(ad, eid))) 
    ad_setentrylen(ad, eid, off);

  return cc;
#endif /* __linux__ */
}
#endif /* HAVE_SENDFILE_WRITE */
#endif /* 0 */
#endif
