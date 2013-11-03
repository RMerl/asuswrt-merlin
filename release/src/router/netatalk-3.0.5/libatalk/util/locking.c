/*
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

/*!
 * @file
 * Netatalk utility functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <fcntl.h>
#include <atalk/util.h>

/*!
 * @def read_lock(fd, offset, whence, len)
 * @brief place read lock on file
 *
 * @param   fd         (r) File descriptor
 * @param   offset     (r) byte offset relative to l_whence
 * @param   whence     (r) SEEK_SET, SEEK_CUR, SEEK_END
 * @param   len        (r) no. of bytes (0 means to EOF)
 *
 * @returns 0 on success, -1 on failure with
 *          fcntl return value and errno
 */

/*!
 * @def write_lock(fd, offset, whence, len)
 * @brief place write lock on file
 *
 * @param   fd         (r) File descriptor
 * @param   offset     (r) byte offset relative to l_whence
 * @param   whence     (r) SEEK_SET, SEEK_CUR, SEEK_END
 * @param   len        (r) no. of bytes (0 means to EOF)
 *
 * @returns 0 on success, -1 on failure with
 *          fcntl return value and errno
 */

/*!
 * @def unlock(fd, offset, whence, len)
 * @brief unlock a file
 *
 * @param   fd         (r) File descriptor
 * @param   offset     (r) byte offset relative to l_whence
 * @param   whence     (r) SEEK_SET, SEEK_CUR, SEEK_END
 * @param   len        (r) no. of bytes (0 means to EOF)
 *
 * @returns 0 on success, -1 on failure with
 *          fcntl return value and errno
 */

/*!
 * @brief lock a file with fctnl
 *
 * This function is called via the macros:
 * read_lock, write_lock, un_lock
 *
 * @param   fd         (r) File descriptor
 * @param   cmd        (r) cmd to fcntl, only F_SETLK is usable here
 * @param   type       (r) F_RDLCK, F_WRLCK, F_UNLCK
 * @param   offset     (r) byte offset relative to l_whence
 * @param   whence     (r) SEEK_SET, SEEK_CUR, SEEK_END
 * @param   len        (r) no. of bytes (0 means to EOF)
 *
 * @returns 0 on success, -1 on failure with
 *          fcntl return value and errno
 *
 * @sa read_lock, write_lock, unlock
 */
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;

    return (fcntl(fd, cmd, &lock));
}
