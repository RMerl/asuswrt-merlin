/* Copyright (C) 1991, 92, 96, 97, 98, 99, 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _SYS_UIO_H
#define _SYS_UIO_H	1

#include <features.h>

#include <sys/types.h>

__BEGIN_DECLS

/* This file defines `struct iovec'.  */
#include <bits/uio.h>


/* Read data from file descriptor FD, and put the result in the
   buffers described by IOVEC, which is a vector of COUNT `struct iovec's.
   The buffers are filled in the order specified.
   Operates just like `read' (see <unistd.h>) except that data are
   put in IOVEC instead of a contiguous buffer.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern ssize_t readv (int __fd, __const struct iovec *__iovec, int __count);

/* Write data pointed by the buffers described by IOVEC, which
   is a vector of COUNT `struct iovec's, to file descriptor FD.
   The data is written in the order specified.
   Operates just like `write' (see <unistd.h>) except that the data
   are taken from IOVEC instead of a contiguous buffer.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern ssize_t writev (int __fd, __const struct iovec *__iovec, int __count);

__END_DECLS

#endif /* sys/uio.h */
