/* $Id: file_io_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/file_io.h>
#include <pj/unicode.h>
#include <pj/errno.h>
#include <pj/assert.h>

#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#   define INVALID_SET_FILE_POINTER     ((DWORD)-1)
#endif

static pj_status_t set_file_pointer(pj_oshandle_t fd,
    pj_off_t offset,
    pj_off_t* newPos,
    DWORD dwMoveMethod)
{
#if defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8
    LARGE_INTEGER liDistance, liNewPos;

    liDistance.QuadPart = offset;
    if (!SetFilePointerEx(fd, liDistance, &liNewPos, dwMoveMethod)) {
	return PJ_RETURN_OS_ERROR(GetLastError());
    }
    *newPos = liNewPos.QuadPart;
#else
    DWORD dwNewPos;
    LONG  hi32;

    hi32 = (LONG)(offset >> 32);

    dwNewPos = SetFilePointer(fd, (long)offset, &hi32, dwMoveMethod);
    if (dwNewPos == (DWORD)INVALID_SET_FILE_POINTER) {
	DWORD dwStatus = GetLastError();
	if (dwStatus != 0)
	    return PJ_RETURN_OS_ERROR(dwStatus);
	/* dwNewPos actually is not an error. */
    }
    *newPos = hi32;
    *newPos = (*newPos << 32) + dwNewPos;
#endif

    return PJ_SUCCESS;
}

/**
 * Check for end-of-file condition on the specified descriptor.
 *
 * @param fd            The file descriptor.
 * @param access        The desired access.
 *
 * @return              Non-zero if file is EOF.
 */
PJ_DECL(pj_bool_t) pj_file_eof(pj_oshandle_t fd, 
                               enum pj_file_access access);

PJ_DEF(pj_status_t) pj_syslog_facility(int facility)
{
	PJ_UNUSED_ARG(facility);
	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_open( pj_pool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  pj_oshandle_t *fd,
								  unsigned *size)
{
    PJ_DECL_UNICODE_TEMP_BUF(wpathname, 256)
    HANDLE hFile;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreationDisposition = 0;
    DWORD dwFlagsAndAttributes = 0;

    PJ_UNUSED_ARG(pool);

    PJ_ASSERT_RETURN(pathname!=NULL, PJ_EINVAL);

    if ((flags & PJ_O_WRONLY) == PJ_O_WRONLY) {
        dwDesiredAccess |= GENERIC_WRITE;
        if ((flags & PJ_O_APPEND) == PJ_O_APPEND) {
#if !defined(PJ_WIN32_WINCE) || !PJ_WIN32_WINCE
	    /* FILE_APPEND_DATA is invalid on WM2003 and WM5, but it seems
	     * to be working on WM6. All are tested on emulator though.
	     * Removing this also seem to work (i.e. data is appended), so
	     * I guess this flag is "optional".
	     * See http://trac.pjsip.org/repos/ticket/825
	     */
            dwDesiredAccess |= FILE_APPEND_DATA;
#endif
	    dwCreationDisposition |= OPEN_ALWAYS;
        } else {
            dwDesiredAccess &= ~(FILE_APPEND_DATA);
            dwCreationDisposition |= CREATE_ALWAYS;
        }
    }
    if ((flags & PJ_O_RDONLY) == PJ_O_RDONLY) {
        dwDesiredAccess |= GENERIC_READ;
        if (flags == PJ_O_RDONLY)
            dwCreationDisposition |= OPEN_EXISTING;
    }

	if (!flags) {
		dwDesiredAccess |= GENERIC_WRITE;
		dwCreationDisposition |= TRUNCATE_EXISTING;
	}

    if (dwDesiredAccess == 0) {
        pj_assert(!"Invalid file open flags");
        return PJ_EINVAL;
    }

    dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    
    dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

#if defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8  
    hFile = CreateFile2(PJ_STRING_TO_NATIVE(pathname,
			wpathname, sizeof(wpathname)),
			dwDesiredAccess, dwShareMode, dwCreationDisposition,
			NULL);
#else
    hFile = CreateFile(PJ_STRING_TO_NATIVE(pathname,
		       wpathname, sizeof(wpathname)),
		       dwDesiredAccess, dwShareMode, NULL,
		       dwCreationDisposition, dwFlagsAndAttributes, NULL);
#endif

    if (hFile == INVALID_HANDLE_VALUE) {
	DWORD lastErr = GetLastError();	
        *fd = 0;
        return PJ_RETURN_OS_ERROR(lastErr);
    }

    if ((flags & PJ_O_APPEND) == PJ_O_APPEND) {
	pj_status_t status;

	status = pj_file_setpos(hFile, 0, PJ_SEEK_END);
	if (status != PJ_SUCCESS) {
	    pj_file_close(hFile);
	    return status;
	}
	if (size)
		pj_file_getpos(hFile, size);
    }

    *fd = hFile;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_close(pj_oshandle_t fd)
{
    if (CloseHandle(fd)==0)
        return PJ_RETURN_OS_ERROR(GetLastError());
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_write( pj_oshandle_t fd,
                                   const void *data,
                                   pj_ssize_t *size)
{
    BOOL rc;
    DWORD bytesWritten;

    rc = WriteFile(fd, data, (DWORD)*size, &bytesWritten, NULL);
    if (!rc) {
        *size = -1;
        return PJ_RETURN_OS_ERROR(GetLastError());
    }

    *size = bytesWritten;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_read( pj_oshandle_t fd,
                                  void *data,
                                  pj_ssize_t *size)
{
    BOOL rc;
    DWORD bytesRead;

    rc = ReadFile(fd, data, (DWORD)*size, &bytesRead, NULL);
    if (!rc) {
        *size = -1;
        return PJ_RETURN_OS_ERROR(GetLastError());
    }

    *size = bytesRead;
    return PJ_SUCCESS;
}

/*
PJ_DEF(pj_bool_t) pj_file_eof(pj_oshandle_t fd, enum pj_file_access access)
{
    BOOL rc;
    DWORD dummy = 0, bytes;
    DWORD dwStatus;

    if ((access & PJ_O_RDONLY) == PJ_O_RDONLY) {
        rc = ReadFile(fd, &dummy, 0, &bytes, NULL);
    } else if ((access & PJ_O_WRONLY) == PJ_O_WRONLY) {
        rc = WriteFile(fd, &dummy, 0, &bytes, NULL);
    } else {
        pj_assert(!"Invalid access");
        return PJ_TRUE;
    }

    dwStatus = GetLastError();
    if (dwStatus==ERROR_HANDLE_EOF)
        return PJ_TRUE;

    return 0;
}
*/

PJ_DEF(pj_status_t) pj_file_setpos( pj_oshandle_t fd,
                                    pj_off_t offset,
                                    enum pj_file_seek_type whence)
{
    DWORD dwMoveMethod;
    pj_off_t newPos;

    if (whence == PJ_SEEK_SET)
        dwMoveMethod = FILE_BEGIN;
    else if (whence == PJ_SEEK_CUR)
        dwMoveMethod = FILE_CURRENT;
    else if (whence == PJ_SEEK_END)
        dwMoveMethod = FILE_END;
    else {
        pj_assert(!"Invalid whence in file_setpos");
        return PJ_EINVAL;
    }

    if (set_file_pointer(fd, offset, &newPos, dwMoveMethod) != PJ_SUCCESS) {
	return PJ_RETURN_OS_ERROR(GetLastError());
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_getpos( pj_oshandle_t fd,
                                    pj_off_t *pos)
{
    if (set_file_pointer(fd, 0, pos, FILE_CURRENT) != PJ_SUCCESS) {
	return PJ_RETURN_OS_ERROR(GetLastError());
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_file_flush(pj_oshandle_t fd)
{
    BOOL rc;

    rc = FlushFileBuffers(fd);

    if (!rc) {
	DWORD dwStatus = GetLastError();
        if (dwStatus != 0)
            return PJ_RETURN_OS_ERROR(dwStatus);
    }

    return PJ_SUCCESS;
}
