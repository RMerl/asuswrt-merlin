/*
 * Unix SMB/CIFS implementation.
 * Utility functions to transfer files.
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TRANSFER_FILE_H__
#define __TRANSFER_FILE_H__

ssize_t transfer_file_internal(void *in_file,
			       void *out_file,
			       size_t n,
			       ssize_t (*read_fn)(void *, void *, size_t),
			       ssize_t (*write_fn)(void *, const void *, size_t));

SMB_OFF_T transfer_file(int infd, int outfd, SMB_OFF_T n);

#endif /* __TRANSFER_FILE_H__ */
