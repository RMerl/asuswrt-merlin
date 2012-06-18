/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Copyright (c) 2006-2007 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* version 1.04 2007/04/3 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "sg_pt.h"
#include "sg_lib.h"
#include "sg_pt_win32.h"

/* Use the Microsoft SCSI Pass Through (SPT) interface. It has two
 * variants: "SPT" where data is double buffered; and "SPTD" where data
 * pointers to the user space are passed to the OS. Only Windows
 * 2000, 2003 and XP are supported (i.e. not 95,98 or ME).
 * Currently there is no ASPI interface which relies on a dll
 * from adpatec.
 * This code uses cygwin facilities and is built in a cygwin
 * shell. It can be run in a normal DOS shell if the cygwin1.dll
 * file is put in an appropriate place.
 */

#define DEF_TIMEOUT 60       /* 60 seconds */
#define MAX_OPEN_SIMULT 8
#define WIN32_FDOFFSET 32

struct sg_pt_handle {
    int in_use;
    HANDLE fh;
    char adapter[32];
    int bus;
    int target;
    int lun;
};

struct sg_pt_handle handle_arr[MAX_OPEN_SIMULT];

struct sg_pt_win32_scsi {
#ifdef SPTD
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;
#else
    SCSI_PASS_THROUGH_WITH_BUFFERS swb;
#endif
    unsigned char * dxferp;
    int dxfer_len;
    unsigned char * sensep;
    int sense_len;
    int scsi_status;
    int resid;
    int sense_resid;
    int in_err;
    int os_err;                 /* pseudo unix error */
    int transport_err;          /* windows error number */
};

struct sg_pt_base {
    struct sg_pt_win32_scsi impl;
};

/* Returns >= 0 if successful. If error in Unix returns negated errno.
 * Optionally accept leading "\\.\". If given something of the form
 * "ScSi<num>:<bus>,<target>,<lun>" where the values in angle brackets
 * are integers, then will attempt to open "\\.\SCSI<num>:" and save the
 * other three values for the DeviceIoControl call. The trailing ".<lun>"
 * is optionally and if not given 0 is assumed. Since "PhysicalDrive"
 * is a lot of keystrokes, "PD" is accepted and converted to the longer
 * form.
 */
int scsi_pt_open_device(const char * device_name,
                        int read_only __attribute__ ((unused)),
                        int verbose)
{
    int len, k, adapter_num, bus, target, lun, off, got_scsi_name;
    int index, num, got_pd_name, pd_num;
    struct sg_pt_handle * shp;
    char buff[8];

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    /* lock */
    for (k = 0; k < MAX_OPEN_SIMULT; k++)
        if (0 == handle_arr[k].in_use)
            break;
    if (k == MAX_OPEN_SIMULT) {
        if (verbose)
            fprintf(sg_warnings_strm, "too many open handles "
                    "(%d)\n", MAX_OPEN_SIMULT);
        return -EMFILE;
    } else
        handle_arr[k].in_use = 1;
    /* unlock */
    index = k;
    shp = handle_arr + index;
    adapter_num = 0;
    bus = 0;    /* also known as 'PathId' in MS docs */
    target = 0;
    lun = 0;
    got_pd_name = 0;
    got_scsi_name = 0;
    len = strlen(device_name);
    if ((len > 4) && (0 == strncmp("\\\\.\\", device_name, 4)))
        off = 4;
    else
        off = 0;
    if (len > (off + 2)) {
        buff[0] = toupper(device_name[off + 0]);
        buff[1] = toupper(device_name[off + 1]);
        if (0 == strncmp("PD", buff, 2)) {
            num = sscanf(device_name + off + 2, "%d", &pd_num);
            if (1 == num)
                got_pd_name = 1;
        }
        if (0 == got_pd_name) {
            buff[2] = toupper(device_name[off + 2]);
            buff[3] = toupper(device_name[off + 3]);
            if (0 == strncmp("SCSI", buff, 4)) {
                num = sscanf(device_name + off + 4, "%d:%d,%d,%d",
                             &adapter_num, &bus, &target, &lun);
                if (num < 3) {
                    if (verbose)
                        fprintf(sg_warnings_strm, "expected format like: "
                                "'SCSI<port>:<bus>.<target>[.<lun>]'\n");
                    shp->in_use = 0;
                    return -EINVAL;
                }
                got_scsi_name = 1;
            }
        }
    }
    shp->bus = bus;
    shp->target = target;
    shp->lun = lun;
    memset(shp->adapter, 0, sizeof(shp->adapter));
    strncpy(shp->adapter, "\\\\.\\", 4);
    if (got_pd_name)
        snprintf(shp->adapter + 4, sizeof(shp->adapter) - 5,
                 "PhysicalDrive%d", pd_num);
    else if (got_scsi_name)
        snprintf(shp->adapter + 4, sizeof(shp->adapter) - 5, "SCSI%d:",
                 adapter_num);
    else
        snprintf(shp->adapter + 4, sizeof(shp->adapter) - 5, "%s",
                 device_name + off);
    shp->fh = CreateFile(shp->adapter, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, 0, NULL);
    if (shp->fh == INVALID_HANDLE_VALUE) {
        if (verbose)
            fprintf(sg_warnings_strm, "Windows CreateFile error=%ld\n",
                    GetLastError());
        shp->in_use = 0;
        return -ENODEV;
    }
    return index + WIN32_FDOFFSET;
}
            

/* Returns 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_close_device(int device_fd)
{
    struct sg_pt_handle * shp;
    int index;

    index = device_fd - WIN32_FDOFFSET;

    if ((index < 0) || (index >= WIN32_FDOFFSET))
        return -ENODEV;
    shp = handle_arr + index;
    CloseHandle(shp->fh);
    shp->bus = 0;
    shp->target = 0;
    shp->lun = 0;
    memset(shp->adapter, 0, sizeof(shp->adapter));
    shp->in_use = 0;
    return 0;
}

struct sg_pt_base * construct_scsi_pt_obj()
{
    struct sg_pt_win32_scsi * psp;

    psp = (struct sg_pt_win32_scsi *)malloc(sizeof(struct sg_pt_win32_scsi));
    if (psp) {
        memset(psp, 0, sizeof(struct sg_pt_win32_scsi));
        psp->swb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
        psp->swb.spt.SenseInfoLength = SCSI_MAX_SENSE_LEN;
        psp->swb.spt.SenseInfoOffset =
                offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
        psp->swb.spt.TimeOutValue = DEF_TIMEOUT;
    }
    return (struct sg_pt_base *)psp;
}

void destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp) {
        free(psp);
    }
}

void set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                     int cdb_len)
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp->swb.spt.CdbLength > 0) 
        ++psp->in_err;
    if (cdb_len > (int)sizeof(psp->swb.spt.Cdb)) {
        ++psp->in_err;
        return;
    }
    memcpy(psp->swb.spt.Cdb, cdb, cdb_len);
    psp->swb.spt.CdbLength = cdb_len;
}

void set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                       int sense_len)
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp->sensep)
        ++psp->in_err;
    memset(sense, 0, sense_len);
    psp->sensep = sense;
    psp->sense_len = sense_len;
}

void set_scsi_pt_data_in(struct sg_pt_base * vp,             /* from device */
                         unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp->dxferp)
        ++psp->in_err;
    if (dxfer_len > 0) {
        psp->dxferp = dxferp;
        psp->dxfer_len = dxfer_len;
        psp->swb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    }
}

void set_scsi_pt_data_out(struct sg_pt_base * vp,            /* to device */
                          const unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp->dxferp)
        ++psp->in_err;
    if (dxfer_len > 0) {
        psp->dxferp = (unsigned char *)dxferp;
        psp->dxfer_len = dxfer_len;
        psp->swb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
    }
}

void set_scsi_pt_packet_id(struct sg_pt_base * vp __attribute__ ((unused)),
                           int pack_id __attribute__ ((unused)))
{
}

void set_scsi_pt_tag(struct sg_pt_base * vp,
                     unsigned long long tag __attribute__ ((unused)))
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    ++psp->in_err;
}

void set_scsi_pt_task_management(struct sg_pt_base * vp,
                                 int tmf_code __attribute__ ((unused)))
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    ++psp->in_err;
}

void set_scsi_pt_task_attr(struct sg_pt_base * vp,
                           int attrib __attribute__ ((unused)),
                           int priority __attribute__ ((unused)))
{
    struct sg_pt_win32_scsi * psp = &vp->impl;

    ++psp->in_err;
}

int do_scsi_pt(struct sg_pt_base * vp, int device_fd, int time_secs,
               int verbose)
{
    int index = device_fd - WIN32_FDOFFSET;
    struct sg_pt_win32_scsi * psp = &vp->impl;
    struct sg_pt_handle * shp;
    BOOL status;
    ULONG returned;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (psp->in_err) {
        if (verbose)
            fprintf(sg_warnings_strm, "Replicated or unused set_scsi_pt...\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    if (0 == psp->swb.spt.CdbLength) {
        if (verbose)
            fprintf(sg_warnings_strm, "No command (cdb) given\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }

    index = device_fd - WIN32_FDOFFSET;
    if ((index < 0) || (index >= WIN32_FDOFFSET)) {
        if (verbose)
            fprintf(sg_warnings_strm, "Bad file descriptor\n");
        psp->os_err = ENODEV;
        return -psp->os_err;
    }
    shp = handle_arr + index;
    if (0 == shp->in_use) {
        if (verbose)
            fprintf(sg_warnings_strm, "File descriptor closed??\n");
        psp->os_err = ENODEV;
        return -psp->os_err;
    }
#ifdef SPTD
    psp->swb.spt.Length = sizeof (SCSI_PASS_THROUGH_DIRECT);
#else
    if (psp->dxfer_len > (int)sizeof(psp->swb.ucDataBuf)) {
        if (verbose)
            fprintf(sg_warnings_strm, "dxfer_len (%d) too large (limit %d "
                    "bytes)\n", psp->dxfer_len, sizeof(psp->swb.ucDataBuf));
        psp->os_err = ENOMEM;
        return -psp->os_err;

    }
    psp->swb.spt.Length = sizeof (SCSI_PASS_THROUGH);
    psp->swb.spt.DataBufferOffset =
                offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
#endif
    psp->swb.spt.PathId = shp->bus;
    psp->swb.spt.TargetId = shp->target;
    psp->swb.spt.Lun = shp->lun;
    psp->swb.spt.TimeOutValue = time_secs;
    psp->swb.spt.DataTransferLength = psp->dxfer_len;
    if (verbose > 4) {
        fprintf(stderr, " spt:: adapter: %s  Length=%d ScsiStatus=%d "
                "PathId=%d TargetId=%d Lun=%d\n", shp->adapter,
                (int)psp->swb.spt.Length,
                (int)psp->swb.spt.ScsiStatus, (int)psp->swb.spt.PathId,
                (int)psp->swb.spt.TargetId, (int)psp->swb.spt.Lun);
        fprintf(stderr, "    CdbLength=%d SenseInfoLength=%d DataIn=%d "
                "DataTransferLength=%lu\n",
                (int)psp->swb.spt.CdbLength, (int)psp->swb.spt.SenseInfoLength,
                (int)psp->swb.spt.DataIn, psp->swb.spt.DataTransferLength);
#ifdef SPTD
        fprintf(stderr, "    TimeOutValue=%lu SenseInfoOffset=%lu\n",
                psp->swb.spt.TimeOutValue, psp->swb.spt.SenseInfoOffset);
#else
        fprintf(stderr, "    TimeOutValue=%lu DataBufferOffset=%lu "
                "SenseInfoOffset=%lu\n", psp->swb.spt.TimeOutValue,
                psp->swb.spt.DataBufferOffset, psp->swb.spt.SenseInfoOffset);
#endif
    }
#ifdef SPTD
    psp->swb.spt.DataBuffer = psp->dxferp;
    status = DeviceIoControl(shp->fh, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                            &psp->swb,
                            sizeof(psp->swb),
                            &psp->swb,
                            sizeof(psp->swb),
                            &returned,
                            NULL);
#else
    if ((psp->dxfer_len > 0) && (SCSI_IOCTL_DATA_OUT == psp->swb.spt.DataIn))
        memcpy(psp->swb.ucDataBuf, psp->dxferp, psp->dxfer_len);
    status = DeviceIoControl(shp->fh, IOCTL_SCSI_PASS_THROUGH,
                            &psp->swb,
                            sizeof(psp->swb),
                            &psp->swb,
                            sizeof(psp->swb),
                            &returned,
                            NULL);
#endif
    if (! status) {
        psp->transport_err = GetLastError();
        if (verbose)
            fprintf(sg_warnings_strm, "Windows DeviceIoControl error=%d\n",
                    psp->transport_err);
        psp->os_err = EIO;
        return 0;       /* let app find transport error */
    }
#ifndef SPTD
    if ((psp->dxfer_len > 0) && (SCSI_IOCTL_DATA_IN == psp->swb.spt.DataIn))
        memcpy(psp->dxferp, psp->swb.ucDataBuf, psp->dxfer_len);
#endif

    psp->scsi_status = psp->swb.spt.ScsiStatus;
    if ((SAM_STAT_CHECK_CONDITION == psp->scsi_status) ||
        (SAM_STAT_COMMAND_TERMINATED == psp->scsi_status))
        memcpy(psp->sensep, psp->swb.ucSenseBuf, psp->sense_len);
    else
        psp->sense_len = 0;
    psp->sense_resid = 0;
    if ((psp->dxfer_len > 0) && (psp->swb.spt.DataTransferLength > 0))
        psp->resid = psp->dxfer_len - psp->swb.spt.DataTransferLength;
    else
        psp->resid = 0;

    return 0;
}

int get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;

    if (psp->transport_err)     /* give transport error highest priority */
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if (psp->os_err)
        return SCSI_PT_RESULT_OS_ERR;
    else if ((SAM_STAT_CHECK_CONDITION == psp->scsi_status) ||
             (SAM_STAT_COMMAND_TERMINATED == psp->scsi_status))
        return SCSI_PT_RESULT_SENSE;
    else if (psp->scsi_status)
        return SCSI_PT_RESULT_STATUS;
    else
        return SCSI_PT_RESULT_GOOD;
}

int get_scsi_pt_resid(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;

    return psp->resid;
}

int get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;

    return psp->scsi_status;
}

int get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;
    int len;

    len = psp->sense_len - psp->sense_resid;
    return (len > 0) ? len : 0;
}

int get_scsi_pt_duration_ms(const struct sg_pt_base * vp __attribute__ ((unused)))
{
    // const struct sg_pt_freebsd_scsi * psp = &vp->impl;

    return -1;
}

int get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;

    return psp->transport_err;
}

int get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;

    return psp->os_err;
}


char * get_scsi_pt_transport_err_str(const struct sg_pt_base * vp,
                                     int max_b_len, char * b)
{
    struct sg_pt_win32_scsi * psp = (struct sg_pt_win32_scsi *)&vp->impl;
    LPVOID lpMsgBuf;
    int k, num, ch;

    if (max_b_len < 2) {
        if (1 == max_b_len)
            b[0] = '\0';
        return b;
    }
    memset(b, 0, max_b_len);
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        psp->transport_err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
    num = lstrlen((LPCTSTR)lpMsgBuf);
    if (num < 1)
        return b;
    num = (num < max_b_len) ? num : (max_b_len - 1);
    for (k = 0; k < num; ++k) {
        ch = *((LPCTSTR)lpMsgBuf + k);
        if ((ch >= 0x0) && (ch < 0x7f))
            b[k] = ch & 0x7f;
        else
            b[k] = '?';
    }
    return b;
}

char * get_scsi_pt_os_err_str(const struct sg_pt_base * vp,
                              int max_b_len, char * b)
{
    const struct sg_pt_win32_scsi * psp = &vp->impl;
    const char * cp;

    cp = safe_strerror(psp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}
