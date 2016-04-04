/* $Id: file_io.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_FILE_IO_H__
#define __PJ_FILE_IO_H__

/**
 * @file file_io.h
 * @brief Simple file I/O abstraction.
 */
#include <pj/types.h>

PJ_BEGIN_DECL 

/**
 * @defgroup PJ_FILE_IO File I/O
 * @ingroup PJ_IO
 * @{
 *
 * This file contains functionalities to perform file I/O. The file
 * I/O can be implemented with various back-end, either using native
 * file API or ANSI stream. 
 *
 * @section pj_file_size_limit_sec Size Limits
 *
 * There may be limitation on the size that can be handled by the
 * #pj_file_setpos() or #pj_file_getpos() functions. The API itself
 * uses 64-bit integer for the file offset/position (where available); 
 * however some backends (such as ANSI) may only support signed 32-bit 
 * offset resolution.
 *
 * Reading and writing operation uses signed 32-bit integer to indicate
 * the size.
 *
 *
 */

/**
 * These enumerations are used when opening file. Values PJ_O_RDONLY,
 * PJ_O_WRONLY, and PJ_O_RDWR are mutually exclusive. Value PJ_O_APPEND
 * can only be used when the file is opened for writing. 
 */
enum pj_file_access
{
    PJ_O_RDONLY     = 0x1101,   /**< Open file for reading.             */
    PJ_O_WRONLY     = 0x1102,   /**< Open file for writing.             */
    PJ_O_RDWR       = 0x1103,   /**< Open file for reading and writing. 
                                     File will be truncated.            */
    PJ_O_APPEND     = 0x1108,   /**< Append to existing file.           */
	PJ_O_SYSLOG     = 0x1110    /**< Write to sys log.                  */
};

/**
 * The seek directive when setting the file position with #pj_file_setpos.
 */
enum pj_file_seek_type
{
    PJ_SEEK_SET     = 0x1201,   /**< Offset from beginning of the file. */
    PJ_SEEK_CUR     = 0x1202,   /**< Offset from current position.      */
    PJ_SEEK_END     = 0x1203    /**< Size of the file plus offset.      */
};

/**
 * Set syslog facility. 
 * The function must be called once before using pj_file_open, 
 * pj_file_close or pj_file_write, if PJ_O_SYSLOG is set to log_file_flags.
 * The write_to_syslog global variable is also set to PJ_TRUE, if this 
 * function is called.
 *
 * @param facility      The facility of syslog. This argument is same as 
 *                      the facility argument of openlog() API on linux.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DEF(pj_status_t) pj_syslog_facility(int facility);

/**
 * Open the file as specified in \c pathname with the specified
 * mode, and return the handle in \c fd. All files will be opened
 * as binary.
 *
 * @param pool          Pool to allocate memory for the new file descriptor.
 * @param pathname      The file name to open.
 * @param flags         Open flags, which is bitmask combination of
 *                      #pj_file_access enum. The flag must be either
 *                      PJ_O_RDONLY, PJ_O_WRONLY, or PJ_O_RDWR. When file
 *                      writing is specified, existing file will be 
 *                      truncated unless PJ_O_APPEND is specified.
 * @param fd            The returned descriptor.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_open(pj_pool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
								  pj_oshandle_t *fd,
								  unsigned *size);

/**
 * Close an opened file descriptor.
 *
 * @param fd            The file descriptor.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_close(pj_oshandle_t fd);

/**
 * Write data with the specified size to an opened file.
 *
 * @param fd            The file descriptor.
 * @param data          Data to be written to the file.
 * @param size          On input, specifies the size of data to be written.
 *                      On return, it contains the number of data actually
 *                      written to the file.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_write(pj_oshandle_t fd,
                                   const void *data,
                                   pj_ssize_t *size);

/**
 * Read data from the specified file. When end-of-file condition is set,
 * this function will return PJ_SUCCESS but the size will contain zero.
 *
 * @param fd            The file descriptor.
 * @param data          Pointer to buffer to receive the data.
 * @param size          On input, specifies the maximum number of data to
 *                      read from the file. On output, it contains the size
 *                      of data actually read from the file. It will contain
 *                      zero when EOF occurs.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 *                      When EOF occurs, the return is PJ_SUCCESS but size
 *                      will report zero.
 */
PJ_DECL(pj_status_t) pj_file_read(pj_oshandle_t fd,
                                  void *data,
                                  pj_ssize_t *size);

/**
 * Set file position to new offset according to directive \c whence.
 *
 * @param fd            The file descriptor.
 * @param offset        The new file position to set.
 * @param whence        The directive.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_setpos(pj_oshandle_t fd,
                                    pj_off_t offset,
                                    enum pj_file_seek_type whence);

/**
 * Get current file position.
 *
 * @param fd            The file descriptor.
 * @param pos           On return contains the file position as measured
 *                      from the beginning of the file.
 *
 * @return              PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_getpos(pj_oshandle_t fd,
                                    pj_off_t *pos);

/**
 * Flush file buffers.
 *
 * @param fd		The file descriptor.
 *
 * @return		PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pj_file_flush(pj_oshandle_t fd);


/** @} */


PJ_END_DECL

#endif  /* __PJ_FILE_IO_H__ */

