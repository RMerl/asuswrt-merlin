/* $Id: file_access_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/file_access.h>
#include <pj/unicode.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/os.h>
#include <windows.h>
#include <time.h>

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    /* WinCE lacks READ_CONTROL so we must use GENERIC_READ */
#   define CONTROL_ACCESS   GENERIC_READ
#else
#   define CONTROL_ACCESS   READ_CONTROL
#endif


/*
 * pj_file_exists()
 */
PJ_DEF(pj_bool_t) pj_file_exists(const char *filename)
{
    PJ_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;

    PJ_ASSERT_RETURN(filename != NULL, 0);

    hFile = CreateFile(PJ_STRING_TO_NATIVE(filename,wfilename,sizeof(wfilename)), 
		       CONTROL_ACCESS, 
		       FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    CloseHandle(hFile);
    return PJ_TRUE;
}


/*
 * pj_file_size()
 */
PJ_DEF(pj_off_t) pj_file_size(const char *filename)
{
    PJ_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;
    DWORD sizeLo, sizeHi;
    pj_off_t size;

    PJ_ASSERT_RETURN(filename != NULL, -1);

    hFile = CreateFile(PJ_STRING_TO_NATIVE(filename, wfilename,sizeof(wfilename)), 
		       CONTROL_ACCESS, 
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    sizeLo = GetFileSize(hFile, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE) {
        DWORD dwStatus = GetLastError();
        if (dwStatus != NO_ERROR) {
            CloseHandle(hFile);
            return -1;
        }
    }

    size = sizeHi;
    size = (size << 32) + sizeLo;

    CloseHandle(hFile);
    return size;
}


/*
 * pj_file_delete()
 */
PJ_DEF(pj_status_t) pj_file_delete(const char *filename)
{
    PJ_DECL_UNICODE_TEMP_BUF(wfilename,256)

    PJ_ASSERT_RETURN(filename != NULL, PJ_EINVAL);

    if (DeleteFile(PJ_STRING_TO_NATIVE(filename,wfilename,sizeof(wfilename))) == FALSE)
        return PJ_RETURN_OS_ERROR(GetLastError());

    return PJ_SUCCESS;
}


/*
 * pj_file_move()
 */
PJ_DEF(pj_status_t) pj_file_move( const char *oldname, const char *newname)
{
    PJ_DECL_UNICODE_TEMP_BUF(woldname,256)
    PJ_DECL_UNICODE_TEMP_BUF(wnewname,256)
    BOOL rc;

    PJ_ASSERT_RETURN(oldname!=NULL && newname!=NULL, PJ_EINVAL);

#if PJ_WIN32_WINNT >= 0x0400
    rc = MoveFileEx(PJ_STRING_TO_NATIVE(oldname,woldname,sizeof(woldname)), 
		    PJ_STRING_TO_NATIVE(newname,wnewname,sizeof(wnewname)), 
                    MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING);
#else
    rc = MoveFile(PJ_STRING_TO_NATIVE(oldname,woldname,sizeof(woldname)), 
		  PJ_STRING_TO_NATIVE(newname,wnewname,sizeof(wnewname)));
#endif

    if (!rc)
        return PJ_RETURN_OS_ERROR(GetLastError());

    return PJ_SUCCESS;
}


static pj_status_t file_time_to_time_val(const FILETIME *file_time,
                                         pj_time_val *time_val)
{
    FILETIME local_file_time;
    SYSTEMTIME localTime;
    pj_parsed_time pt;

    if (!FileTimeToLocalFileTime(file_time, &local_file_time))
	return PJ_RETURN_OS_ERROR(GetLastError());

    if (!FileTimeToSystemTime(file_time, &localTime))
        return PJ_RETURN_OS_ERROR(GetLastError());

    //if (!SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime))
    //    return PJ_RETURN_OS_ERROR(GetLastError());

    pj_bzero(&pt, sizeof(pt));
    pt.year = localTime.wYear;
    pt.mon = localTime.wMonth-1;
    pt.day = localTime.wDay;
    pt.wday = localTime.wDayOfWeek;

    pt.hour = localTime.wHour;
    pt.min = localTime.wMinute;
    pt.sec = localTime.wSecond;
    pt.msec = localTime.wMilliseconds;

    return pj_time_encode(&pt, time_val);
}

/*
 * pj_file_getstat()
 */
PJ_DEF(pj_status_t) pj_file_getstat(const char *filename, pj_file_stat *stat)
{
    PJ_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;
    DWORD sizeLo, sizeHi;
    FILETIME creationTime, accessTime, writeTime;

    PJ_ASSERT_RETURN(filename!=NULL && stat!=NULL, PJ_EINVAL);

    hFile = CreateFile(PJ_STRING_TO_NATIVE(filename,wfilename,sizeof(wfilename)), 
		       CONTROL_ACCESS, 
		       FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return PJ_RETURN_OS_ERROR(GetLastError());

    sizeLo = GetFileSize(hFile, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE) {
        DWORD dwStatus = GetLastError();
        if (dwStatus != NO_ERROR) {
            CloseHandle(hFile);
            return PJ_RETURN_OS_ERROR(dwStatus);
        }
    }

    stat->size = sizeHi;
    stat->size = (stat->size << 32) + sizeLo;

    if (GetFileTime(hFile, &creationTime, &accessTime, &writeTime)==FALSE) {
        DWORD dwStatus = GetLastError();
        CloseHandle(hFile);
        return PJ_RETURN_OS_ERROR(dwStatus);
    }

    CloseHandle(hFile);

    if (file_time_to_time_val(&creationTime, &stat->ctime) != PJ_SUCCESS)
        return PJ_RETURN_OS_ERROR(GetLastError());

    file_time_to_time_val(&accessTime, &stat->atime);
    file_time_to_time_val(&writeTime, &stat->mtime);

    return PJ_SUCCESS;
}

