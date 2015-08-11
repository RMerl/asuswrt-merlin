/* $Id: file.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "test.h"
#include <pjlib.h>

#if INCLUDE_FILE_TEST

#define FILENAME                "testfil1.txt"
#define NEWNAME                 "testfil2.txt"
#define INCLUDE_FILE_TIME_TEST  0

static char buffer[11] = {'H', 'e', 'l', 'l', 'o', ' ',
		          'W', 'o', 'r', 'l', 'd' };

static int file_test_internal(void)
{
    enum { FILE_MAX_AGE = 1000 };
    pj_oshandle_t fd = 0;
    pj_status_t status;
    char readbuf[sizeof(buffer)+16];
    pj_file_stat stat;
    pj_time_val start_time;
    pj_ssize_t size;
    pj_off_t pos;

    PJ_LOG(3,("", "..file io test.."));

    /* Get time. */
    pj_gettimeofday(&start_time);

    /* Delete original file if exists. */
    if (pj_file_exists(FILENAME))
        pj_file_delete(FILENAME);

    /*
     * Write data to the file.
     */
    status = pj_file_open(NULL, FILENAME, PJ_O_WRONLY, &fd);
    if (status != PJ_SUCCESS) {
        app_perror("...file_open() error", status);
        return -10;
    }

    size = sizeof(buffer);
    status = pj_file_write(fd, buffer, &size);
    if (status != PJ_SUCCESS) {
        app_perror("...file_write() error", status);
        pj_file_close(fd);
        return -20;
    }
    if (size != sizeof(buffer))
        return -25;

    status = pj_file_close(fd);
    if (status != PJ_SUCCESS) {
        app_perror("...file_close() error", status);
        return -30;
    }

    /* Check the file existance and size. */
    if (!pj_file_exists(FILENAME))
        return -40;

    if (pj_file_size(FILENAME) != sizeof(buffer))
        return -50;

    /* Get file stat. */
    status = pj_file_getstat(FILENAME, &stat);
    if (status != PJ_SUCCESS)
        return -60;

    /* Check stat size. */
    if (stat.size != sizeof(buffer))
        return -70;

#if INCLUDE_FILE_TIME_TEST
    /* Check file creation time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.ctime, start_time))
        return -80;
    /* Check file creation time is not much later. */
    PJ_TIME_VAL_SUB(stat.ctime, start_time);
    if (stat.ctime.sec > FILE_MAX_AGE)
        return -90;

    /* Check file modification time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.mtime, start_time))
        return -80;
    /* Check file modification time is not much later. */
    PJ_TIME_VAL_SUB(stat.mtime, start_time);
    if (stat.mtime.sec > FILE_MAX_AGE)
        return -90;

    /* Check file access time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.atime, start_time))
        return -80;
    /* Check file access time is not much later. */
    PJ_TIME_VAL_SUB(stat.atime, start_time);
    if (stat.atime.sec > FILE_MAX_AGE)
        return -90;
#endif

    /*
     * Re-open the file and read data.
     */
    status = pj_file_open(NULL, FILENAME, PJ_O_RDONLY, &fd);
    if (status != PJ_SUCCESS) {
        app_perror("...file_open() error", status);
        return -100;
    }

    size = 0;
    while (size < (pj_ssize_t)sizeof(readbuf)) {
        pj_ssize_t read;
        read = 1;
        status = pj_file_read(fd, &readbuf[size], &read);
        if (status != PJ_SUCCESS) {
	    PJ_LOG(3,("", "...error reading file after %d bytes (error follows)", 
		      size));
            app_perror("...error", status);
            return -110;
        }
        if (read == 0) {
            // EOF
            break;
        }
        size += read;
    }

    if (size != sizeof(buffer))
        return -120;

    /*
    if (!pj_file_eof(fd, PJ_O_RDONLY))
        return -130;
     */

    if (pj_memcmp(readbuf, buffer, size) != 0)
        return -140;

    /* Seek test. */
    status = pj_file_setpos(fd, 4, PJ_SEEK_SET);
    if (status != PJ_SUCCESS) {
        app_perror("...file_setpos() error", status);
        return -141;
    }

    /* getpos test. */
    status = pj_file_getpos(fd, &pos);
    if (status != PJ_SUCCESS) {
        app_perror("...file_getpos() error", status);
        return -142;
    }
    if (pos != 4)
        return -143;

    status = pj_file_close(fd);
    if (status != PJ_SUCCESS) {
        app_perror("...file_close() error", status);
        return -150;
    }

    /*
     * Rename test.
     */
    status = pj_file_move(FILENAME, NEWNAME);
    if (status != PJ_SUCCESS) {
        app_perror("...file_move() error", status);
        return -160;
    }

    if (pj_file_exists(FILENAME))
        return -170;
    if (!pj_file_exists(NEWNAME))
        return -180;

    if (pj_file_size(NEWNAME) != sizeof(buffer))
        return -190;

    /* Delete test. */
    status = pj_file_delete(NEWNAME);
    if (status != PJ_SUCCESS) {
        app_perror("...file_delete() error", status);
        return -200;
    }

    if (pj_file_exists(NEWNAME))
        return -210;

    PJ_LOG(3,("", "...success"));
    return PJ_SUCCESS;
}


int file_test(void)
{
    int rc = file_test_internal();

    /* Delete test file if exists. */
    if (pj_file_exists(FILENAME))
        pj_file_delete(FILENAME);

    return rc;
}

#else
int dummy_file_test;
#endif

