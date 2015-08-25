/* $Id: file_access.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_FILE_ACCESS_H__
#define __PJ_FILE_ACCESS_H__

/**
 * @file file_access.h
 * @brief File manipulation and access.
 */
#include <pj/types.h>

PJ_BEGIN_DECL 

/**
 * @defgroup PJ_FILE_ACCESS File Access
 * @ingroup PJ_IO
 * @{
 *
 */

/**
 * This structure describes file information, to be obtained by
 * calling #pj_file_getstat(). The time information in this structure
 * is in local time.
 */
typedef struct pj_file_stat
{
    pj_off_t        size;   /**< Total file size.               */
    pj_time_val     atime;  /**< Time of last access.           */
    pj_time_val     mtime;  /**< Time of last modification.     */
    pj_time_val     ctime;  /**< Time of last creation.         */
} pj_file_stat;


/**
 * Returns non-zero if the specified file exists.
 *
 * @param filename      The file name.
 *
 * @return              Non-zero if the file exists.
 */
PJ_DECL(pj_bool_t) pj_file_exists(const char *filename);

/**
 * Returns the size of the file.
 *
 * @param filename      The file name.
 *
 * @return              The file size in bytes or -1 on error.
 */
PJ_DECL(pj_off_t) pj_file_size(const char *filename);

/**
 * Delete a file.
 *
 * @param filename      The filename.
 *
 * @return              PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_file_delete(const char *filename);

/**
 * Move a \c oldname to \c newname. If \c newname already exists,
 * it will be overwritten.
 *
 * @param oldname       The file to rename.
 * @param newname       New filename to assign.
 *
 * @return              PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_file_move( const char *oldname, 
                                   const char *newname);


/**
 * Return information about the specified file. The time information in
 * the \c stat structure will be in local time.
 *
 * @param filename      The filename.
 * @param stat          Pointer to variable to receive file information.
 *
 * @return              PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_file_getstat(const char *filename, pj_file_stat *stat);


/** @} */

PJ_END_DECL


#endif	/* __PJ_FILE_ACCESS_H__ */
