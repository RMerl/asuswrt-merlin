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
 * Copyright (c) 2006 Douglas Gilbert.
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

/*
 * This file shows the relationship between various SCSI device naming
 * schemes in Windows OSes (Windows 200, 2003 and XP) as seen by
 * The SCSI Pass Through (SPT) interface. N.B. ASPI32 is not used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>

#include "sg_lib.h"
#include "sg_pt_win32.h"

#define MAX_SCSI_ELEMS 1024
#define MAX_ADAPTER_NUM 64
#define MAX_PHYSICALDRIVE_NUM 512
#define MAX_CDROM_NUM 512
#define MAX_TAPE_NUM 512
#define MAX_HOLE_COUNT 8
#define SCSI2_INQ_RESP_LEN 36
#define DEF_TIMEOUT 20
#define INQUIRY_CMD 0x12
#define INQUIRY_CMDLEN 6

struct w_scsi_elem {
    char    in_use;
    char    scsi_adapter_valid;
    UCHAR   port_num;           /* <n> in '\\.\SCSI<n>:' adapter name */
    UCHAR   bus;                /* also known as pathId */
    UCHAR   target;
    UCHAR   lun;
    UCHAR   device_claimed;     /* class driver claimed this lu */
    UCHAR   dubious_scsi;       /* set if inq_resp[4] is zero */
    char    pdt;                /* peripheral device type (see SPC-4) */
    char    volume_valid;
    char    volume_multiple;    /* multiple partitions mapping to volumes */
    UCHAR   volume_letter;      /* lowest 'C:' through to 'Z:' */
    char    physicaldrive_valid;
    char    cdrom_valid;
    char    tape_valid;
    int     physicaldrive_num;
    int     cdrom_num;
    int     tape_num;
    unsigned char inq_resp[SCSI2_INQ_RESP_LEN];
};

static struct w_scsi_elem w_scsi_arr[MAX_SCSI_ELEMS];

static int next_unused_scsi_elem = 0;
static int next_elem_after_scsi_adapter_valid = 0;


static char * get_err_str(DWORD err, int max_b_len, char * b)
{
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
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf, 0, NULL );
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

static int findElemIndex(UCHAR port_num, UCHAR bus, UCHAR target, UCHAR lun)
{
    int k;
    struct w_scsi_elem * sep;

    for (k = 0; k < next_unused_scsi_elem; ++k) {
        sep = w_scsi_arr + k;
        if ((port_num == sep->port_num) && (bus == sep->bus) &&
            (target == sep->target) && (lun == sep->lun))
            return k;
#if 0
        if (port_num < sep->port_num)
            break;      /* assume port_num sorted ascending */
#endif
    }
    return -1;
}

static BOOL fetchInquiry(HANDLE fh, unsigned char * resp, int max_resp_len,
                         SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER * afterCall,
                         int verbose)
{
    BOOL success;
    int len;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdw;
    ULONG dummy;        /* also acts to align next array */
    BYTE inqResp[SCSI2_INQ_RESP_LEN];
    unsigned char inqCdb[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0, 0,
                                            SCSI2_INQ_RESP_LEN, 0};
    DWORD err;
    char b[256];

    memset(&sptdw, 0, sizeof(sptdw));
    memset(inqResp, 0, sizeof(inqResp));
    sptdw.spt.Length = sizeof (SCSI_PASS_THROUGH_DIRECT);
    sptdw.spt.CdbLength = sizeof(inqCdb);
    sptdw.spt.SenseInfoLength = SCSI_MAX_SENSE_LEN;
    sptdw.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sptdw.spt.DataTransferLength = SCSI2_INQ_RESP_LEN;
    sptdw.spt.TimeOutValue = DEF_TIMEOUT;
    sptdw.spt.DataBuffer = inqResp;
    sptdw.spt.SenseInfoOffset =
                offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    memcpy(sptdw.spt.Cdb, inqCdb, sizeof(inqCdb));

    success = DeviceIoControl(fh, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                              &sptdw, sizeof(sptdw),
                              &sptdw, sizeof(sptdw),
                              &dummy, NULL);
    if (! success) {
        if (verbose) {
            err = GetLastError();
            fprintf(stderr, "fetchInquiry: DeviceIoControl for INQUIRY, "
                    "err=%lu\n\t%s", err, get_err_str(err, sizeof(b), b));
        }
        return success;
    }
    if (afterCall)
        memcpy(afterCall, &sptdw, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
    if (resp) {
        len = (SCSI2_INQ_RESP_LEN > max_resp_len) ?
              max_resp_len : SCSI2_INQ_RESP_LEN;
        memcpy(resp, inqResp, len);
    }
    return success;
}

int sg_do_wscan(char letter, int verbose)
{
    int k, j, m, hole_count, index, matched;
    DWORD err;
    HANDLE fh;
    ULONG dummy;
    BOOL success;
    BYTE bus;
    PSCSI_ADAPTER_BUS_INFO  ai;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdw;
    unsigned char inqResp[SCSI2_INQ_RESP_LEN];
    char adapter_name[64];
    char inqDataBuff[2048];
    char b[256];
    struct w_scsi_elem * sep;

    memset(w_scsi_arr, 0, sizeof(w_scsi_arr));
    hole_count = 0;
    for (k = 0; k < MAX_ADAPTER_NUM; ++k) {
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\SCSI%d:", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            hole_count = 0;
            success = DeviceIoControl(fh, IOCTL_SCSI_GET_INQUIRY_DATA,
                                      NULL, 0, inqDataBuff, sizeof(inqDataBuff),
                                      &dummy, FALSE);
            if (success) {
                PSCSI_BUS_DATA pbd;
                PSCSI_INQUIRY_DATA pid;
                int num_lus, off, len;

                ai = (PSCSI_ADAPTER_BUS_INFO)inqDataBuff;
                for (bus = 0; bus < ai->NumberOfBusses; bus++) {
                    pbd = ai->BusData + bus;
                    num_lus = pbd->NumberOfLogicalUnits;
                    off = pbd->InquiryDataOffset;
                    for (j = 0; j < num_lus; ++j) {
                        if ((off < (int)sizeof(SCSI_ADAPTER_BUS_INFO)) ||
                            (off > ((int)sizeof(inqDataBuff) -
                                    (int)sizeof(SCSI_INQUIRY_DATA))))
                            break;
                        pid = (PSCSI_INQUIRY_DATA)(inqDataBuff + off);
                        m = next_unused_scsi_elem++;
                        if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                            fprintf(stderr, "Too many scsi devices (more "
                                    "than %d)\n", MAX_SCSI_ELEMS);
                            return SG_LIB_CAT_OTHER;
                        }
                        next_elem_after_scsi_adapter_valid =
                                        next_unused_scsi_elem;
                        sep = w_scsi_arr + m;
                        sep->in_use = 1;
                        sep->scsi_adapter_valid = 1;
                        sep->port_num = k;
                        sep->bus = pid->PathId;
                        sep->target = pid->TargetId;
                        sep->lun = pid->Lun;
                        sep->device_claimed = pid->DeviceClaimed;
                        len = pid->InquiryDataLength;
                        len = (len > SCSI2_INQ_RESP_LEN) ?
                              SCSI2_INQ_RESP_LEN : len;
                        memcpy(sep->inq_resp, pid->InquiryData, len);
                        sep->pdt = sep->inq_resp[0] & 0x3f;
                        if (0 == sep->inq_resp[4])
                            sep->dubious_scsi = 1;

                        if (verbose > 1) {
                            fprintf(stderr, "%s: PathId=%d TargetId=%d Lun=%d ",
                                    adapter_name, pid->PathId, pid->TargetId, pid->Lun);
                            fprintf(stderr, "  DeviceClaimed=%d\n", pid->DeviceClaimed);
                            dStrHex((const char *)(pid->InquiryData), pid->InquiryDataLength, 0);
                        }
                        off = pid->NextInquiryDataOffset;
                    }
                }
            } else {
                err = GetLastError();
                fprintf(stderr, "%s: IOCTL_SCSI_GET_INQUIRY_DATA failed "
                        "err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            CloseHandle(fh);
        } else {
            if (verbose > 2) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }

    for (k = 0; k < 24; ++k) {
        matched = 0;
        sep = NULL;
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\%c:", 'C' + k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            success  = DeviceIoControl(fh, IOCTL_SCSI_GET_ADDRESS,
                                       NULL, 0, inqDataBuff, sizeof(inqDataBuff),
                                       &dummy, FALSE);
            if (success) {
                PSCSI_ADDRESS pa;

                pa = (PSCSI_ADDRESS)inqDataBuff;
                index = findElemIndex(pa->PortNumber, pa->PathId,
                                      pa->TargetId, pa->Lun);
                if (index >= 0) {
                    sep = w_scsi_arr + index;
                    matched = 1;
                } else {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->port_num = pa->PortNumber;
                    sep->bus = pa->PathId;
                    sep->target = pa->TargetId;
                    sep->lun = pa->Lun;
                    sep->device_claimed = 1;
                }
                if (sep->volume_valid) {
                    sep->volume_multiple = 1;
                    if (('C' + k) == letter)
                        sep->volume_letter = letter;
                } else {
                    sep->volume_valid = 1;
                    sep->volume_letter = 'C' + k;
                }
                if (verbose > 1)
                    fprintf(stderr, "%c: PortNum=%d PathId=%d TargetId=%d "
                            "Lun=%d  index=%d\n", 'C' + k, pa->PortNumber,
                            pa->PathId, pa->TargetId, pa->Lun, index);
                if (matched) {
                    CloseHandle(fh);
                    continue;
                }
            } else {
                if (verbose > 1) {
                    err = GetLastError();
                    fprintf(stderr, "%c: IOCTL_SCSI_GET_ADDRESS err=%lu\n\t"
                            "%s", 'C' + k, err, get_err_str(err, sizeof(b), b));
                }
            }
            if (fetchInquiry(fh, inqResp, sizeof(inqResp), &sptdw,
                             verbose)) {
                if (sptdw.spt.ScsiStatus) {
                    if (verbose) {
                        fprintf(stderr, "%c: INQUIRY failed:  ", 'C' + k);
                        sg_print_scsi_status(sptdw.spt.ScsiStatus);
                        sg_print_sense("    ", sptdw.ucSenseBuf,
                                       sizeof(sptdw.ucSenseBuf), 0);
                    }
                    CloseHandle(fh);
                    continue;
                }
                if (NULL == sep) {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->device_claimed = 1;
                    sep->volume_valid = 1;
                    sep->volume_letter = 'C' + k;
                }
                memcpy(sep->inq_resp, inqResp, sizeof(sep->inq_resp));
                sep->pdt = sep->inq_resp[0] & 0x3f;
                if (0 == sep->inq_resp[4])
                    sep->dubious_scsi = 1;
            }
            CloseHandle(fh);
        }
    }

    hole_count = 0;
    for (k = 0; k < MAX_PHYSICALDRIVE_NUM; ++k) {
        matched = 0;
        sep = NULL;
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\PhysicalDrive%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            hole_count = 0;
            success  = DeviceIoControl(fh, IOCTL_SCSI_GET_ADDRESS,
                                       NULL, 0, inqDataBuff, sizeof(inqDataBuff),
                                       &dummy, FALSE);
            if (success) {
                PSCSI_ADDRESS pa;

                pa = (PSCSI_ADDRESS)inqDataBuff;
                index = findElemIndex(pa->PortNumber, pa->PathId,
                                      pa->TargetId, pa->Lun);
                if (index >= 0) {
                    sep = w_scsi_arr + index;
                    matched = 1;
                } else {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->port_num = pa->PortNumber;
                    sep->bus = pa->PathId;
                    sep->target = pa->TargetId;
                    sep->lun = pa->Lun;
                    sep->device_claimed = 1;
                }
                sep->physicaldrive_valid = 1;
                sep->physicaldrive_num = k;
                if (verbose > 1)
                    fprintf(stderr, "PD%d: PortNum=%d PathId=%d TargetId=%d "
                            "Lun=%d  index=%d\n", k, pa->PortNumber,
                            pa->PathId, pa->TargetId, pa->Lun, index);
                if (matched) {
                    CloseHandle(fh);
                    continue;
                }
            } else {
                if (verbose > 1) {
                    err = GetLastError();
                    fprintf(stderr, "PD%d: IOCTL_SCSI_GET_ADDRESS err=%lu\n\t"
                            "%s", k, err, get_err_str(err, sizeof(b), b));
                }
            }
            if (fetchInquiry(fh, inqResp, sizeof(inqResp), &sptdw,
                             verbose)) {
                if (sptdw.spt.ScsiStatus) {
                    if (verbose) {
                        fprintf(stderr, "PD%d: INQUIRY failed:  ", k);
                        sg_print_scsi_status(sptdw.spt.ScsiStatus);
                        sg_print_sense("    ", sptdw.ucSenseBuf,
                                       sizeof(sptdw.ucSenseBuf), 0);
                    }
                    CloseHandle(fh);
                    continue;
                }
                if (NULL == sep) {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->device_claimed = 1;
                    sep->physicaldrive_valid = 1;
                    sep->physicaldrive_num = k;
                }
                memcpy(sep->inq_resp, inqResp, sizeof(sep->inq_resp));
                sep->pdt = sep->inq_resp[0] & 0x3f;
                if (0 == sep->inq_resp[4])
                    sep->dubious_scsi = 1;
            }
            CloseHandle(fh);
        } else {
            if (verbose > 2) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }

    hole_count = 0;
    for (k = 0; k < MAX_CDROM_NUM; ++k) {
        matched = 0;
        sep = NULL;
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\CDROM%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            hole_count = 0;
            success  = DeviceIoControl(fh, IOCTL_SCSI_GET_ADDRESS,
                                       NULL, 0, inqDataBuff, sizeof(inqDataBuff),
                                       &dummy, FALSE);
            if (success) {
                PSCSI_ADDRESS pa;

                pa = (PSCSI_ADDRESS)inqDataBuff;
                index = findElemIndex(pa->PortNumber, pa->PathId,
                                      pa->TargetId, pa->Lun);
                if (index >= 0) {
                    sep = w_scsi_arr + index;
                    matched = 1;
                } else {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->port_num = pa->PortNumber;
                    sep->bus = pa->PathId;
                    sep->target = pa->TargetId;
                    sep->lun = pa->Lun;
                    sep->device_claimed = 1;
                }
                sep->cdrom_valid = 1;
                sep->cdrom_num = k;
                if (verbose > 1)
                    fprintf(stderr, "CDROM%d: PortNum=%d PathId=%d TargetId=%d "
                            "Lun=%d  index=%d\n", k, pa->PortNumber,
                            pa->PathId, pa->TargetId, pa->Lun, index);
                if (matched) {
                    CloseHandle(fh);
                    continue;
                }
            } else {
                if (verbose > 1) {
                    err = GetLastError();
                    fprintf(stderr, "CDROM%d: IOCTL_SCSI_GET_ADDRESS err=%lu\n\t"
                            "%s", k, err, get_err_str(err, sizeof(b), b));
                }
            }
            if (fetchInquiry(fh, inqResp, sizeof(inqResp), &sptdw,
                             verbose)) {
                if (sptdw.spt.ScsiStatus) {
                    if (verbose) {
                        fprintf(stderr, "CDROM%d: INQUIRY failed:  ", k);
                        sg_print_scsi_status(sptdw.spt.ScsiStatus);
                        sg_print_sense("    ", sptdw.ucSenseBuf,
                                       sizeof(sptdw.ucSenseBuf), 0);
                    }
                    CloseHandle(fh);
                    continue;
                }
                if (NULL == sep) {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->device_claimed = 1;
                    sep->cdrom_valid = 1;
                    sep->cdrom_num = k;
                }
                memcpy(sep->inq_resp, inqResp, sizeof(sep->inq_resp));
                sep->pdt = sep->inq_resp[0] & 0x3f;
                if (0 == sep->inq_resp[4])
                    sep->dubious_scsi = 1;
            }
            CloseHandle(fh);
        } else {
            if (verbose > 3) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }

    hole_count = 0;
    for (k = 0; k < MAX_TAPE_NUM; ++k) {
        matched = 0;
        sep = NULL;
        snprintf(adapter_name, sizeof (adapter_name), "\\\\.\\TAPE%d", k);
        fh = CreateFile(adapter_name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (fh != INVALID_HANDLE_VALUE) {
            hole_count = 0;
            success  = DeviceIoControl(fh, IOCTL_SCSI_GET_ADDRESS,
                                       NULL, 0, inqDataBuff, sizeof(inqDataBuff),
                                       &dummy, FALSE);
            if (success) {
                PSCSI_ADDRESS pa;

                pa = (PSCSI_ADDRESS)inqDataBuff;
                index = findElemIndex(pa->PortNumber, pa->PathId,
                                      pa->TargetId, pa->Lun);
                if (index >= 0) {
                    sep = w_scsi_arr + index;
                    matched = 1;
                } else {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->port_num = pa->PortNumber;
                    sep->bus = pa->PathId;
                    sep->target = pa->TargetId;
                    sep->lun = pa->Lun;
                    sep->device_claimed = 1;
                }
                sep->tape_valid = 1;
                sep->tape_num = k;
                if (verbose > 1)
                    fprintf(stderr, "TAPE%d: PortNum=%d PathId=%d TargetId=%d "
                            "Lun=%d  index=%d\n", k, pa->PortNumber,
                            pa->PathId, pa->TargetId, pa->Lun, index);
                if (matched) {
                    CloseHandle(fh);
                    continue;
                }
            } else {
                if (verbose > 1) {
                    err = GetLastError();
                    fprintf(stderr, "TAPE%d: IOCTL_SCSI_GET_ADDRESS "
                            "err=%lu\n\t%s", k, err,
                            get_err_str(err, sizeof(b), b));
                }
            }
            if (fetchInquiry(fh, inqResp, sizeof(inqResp), &sptdw,
                             verbose)) {
                if (sptdw.spt.ScsiStatus) {
                    if (verbose) {
                        fprintf(stderr, "TAPE%d: INQUIRY failed:  ", k);
                        sg_print_scsi_status(sptdw.spt.ScsiStatus);
                        sg_print_sense("    ", sptdw.ucSenseBuf,
                                       sizeof(sptdw.ucSenseBuf), 0);
                    }
                    CloseHandle(fh);
                    continue;
                }
                if (NULL == sep) {
                    m = next_unused_scsi_elem++;
                    if (next_unused_scsi_elem > MAX_SCSI_ELEMS) {
                        fprintf(stderr, "Too many scsi devices (more "
                                "than %d)\n", MAX_SCSI_ELEMS);
                        return SG_LIB_CAT_OTHER;
                    }
                    sep = w_scsi_arr + m;
                    sep->in_use = 1;
                    sep->device_claimed = 1;
                    sep->tape_valid = 1;
                    sep->tape_num = k;
                }
                memcpy(sep->inq_resp, inqResp, sizeof(sep->inq_resp));
                sep->pdt = sep->inq_resp[0] & 0x3f;
                if (0 == sep->inq_resp[4])
                    sep->dubious_scsi = 1;
            }
            CloseHandle(fh);
        } else {
            if (verbose > 4) {
                err = GetLastError();
                fprintf(stderr, "%s: CreateFile failed err=%lu\n\t%s",
                        adapter_name, err, get_err_str(err, sizeof(b), b));
            }
            if (++hole_count >= MAX_HOLE_COUNT)
                break;
        }
    }

    for (k = 0; k < MAX_SCSI_ELEMS; ++k) {
        sep = w_scsi_arr + k;
        if (0 == sep->in_use)
            break;
        if (sep->scsi_adapter_valid) {
            snprintf(b, sizeof(b), "SCSI%d:%d,%d,%d ", sep->port_num,
                     sep->bus, sep->target, sep->lun);
            printf("%-18s", b);
        } else
            printf("                  ");
        if (sep->volume_valid)
            printf("%c: %c  ", sep->volume_letter,
                   (sep->volume_multiple ? '+' : ' '));
        else
            printf("      ");
        if (sep->physicaldrive_valid) {
            snprintf(b, sizeof(b), "PD%d ", sep->physicaldrive_num);
            printf("%-9s", b);
        } else if (sep->cdrom_valid) {
            snprintf(b, sizeof(b), "CDROM%d ", sep->cdrom_num);
            printf("%-9s", b);
        } else if (sep->tape_valid) {
            snprintf(b, sizeof(b), "TAPE%d ", sep->tape_num);
            printf("%-9s", b);
        } else
            printf("         ");

        memcpy(b, sep->inq_resp + 8, SCSI2_INQ_RESP_LEN);
        for (j = 0; j < 28; ++j) {
            if ((b[j] < 0x20) || (b[j] > 0x7e))
                b[j] = ' ';
        }
        b[28] = '\0';
        printf("%-30s", b);
        if (sep->dubious_scsi)
            printf("*     ");
        else if ((! sep->physicaldrive_valid) && (! sep->cdrom_valid) &&
                 (! sep->tape_valid))
            printf("pdt=%-2d", sep->pdt);
        else
            printf("      ");

        printf("\n");
    }
    return 0;
}
