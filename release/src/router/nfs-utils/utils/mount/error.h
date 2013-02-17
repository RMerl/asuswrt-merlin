/*
 * error.h:  Common error handling functions
 *
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2007 Chuck Lever <chuck.lever@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

#ifndef _NFS_UTILS_MOUNT_ERROR_H
#define _NFS_UTILS_MOUNT_ERROR_H

char *nfs_strerror(int);

void mount_error(const char *, const char *, int);
void rpc_mount_errors(char *, int, int);
void sys_mount_errors(char *, int, int, int);

void umount_error(int, const char *);

#endif	/* _NFS_UTILS_MOUNT_ERROR_H */
