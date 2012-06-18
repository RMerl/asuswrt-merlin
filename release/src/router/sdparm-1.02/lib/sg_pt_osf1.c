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
 * Copyright (c) 2005-2007 Douglas Gilbert.
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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <io/common/devgetinfo.h>
#include <io/common/iotypes.h>
#include <io/cam/cam.h>
#include <io/cam/uagt.h>
#include <io/cam/rzdisk.h>
#include <io/cam/scsi_opcodes.h>
#include <io/cam/scsi_all.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sg_pt.h"
#include "sg_lib.h"

/* Changed to use struct sg_pt_base 20070403 */


#define OSF1_MAXDEV 64

struct osf1_dev_channel {
    int bus;
    int tgt;
    int lun;
};

// Private table of open devices: guaranteed zero on startup since
// part of static data.
static struct osf1_dev_channel *devicetable[OSF1_MAXDEV] = {0};
static char *cam_dev = "/dev/cam";
static int camfd;
static int camopened = 0;

struct sg_pt_osf1_scsi {
    unsigned char * cdb;
    int cdb_len;
    unsigned char * sense;
    int sense_len;
    unsigned char * dxferp;
    int dxfer_len;
    int dxfer_dir;
    int scsi_status;
    int resid;
    int sense_resid;
    int in_err;
    int os_err;
    int transport_err;
};

struct sg_pt_base {
    struct sg_pt_osf1_scsi impl;
};


/* Returns >= 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_open_device(const char * device_name,
                        int read_only,
                        int verbose)
{
    struct osf1_dev_channel *fdchan;
    int fd, k;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;

    if (!camopened) {
        camfd = open(cam_dev, O_RDWR, 0);
        if (camfd < 0)
            return -1;
        camopened++;
    }

    // Search table for a free entry
    for (k = 0; k < OSF1_MAXDEV; k++)
        if (! devicetable[k])
            break;

    if (k == OSF1_MAXDEV) {
        if (verbose)
            fprintf(sg_warnings_strm, "too many open devices "
                    "(%d)\n", OSF1_MAXDEV);
        errno=EMFILE;
        return -1;
    }

    fdchan = (struct osf1_dev_channel *)calloc(1,
                                sizeof(struct osf1_dev_channel));
    if (fdchan == NULL) {
        // errno already set by call to malloc()
        return -1;
    }

    fd = open(device_name, O_RDONLY|O_NONBLOCK);
    if (fd > 0) {
        device_info_t devinfo;
        bzero(&devinfo, sizeof(devinfo));
        if (ioctl(fd, DEVGETINFO, &devinfo) == 0) {
            fdchan->bus = devinfo.v1.businfo.bus.scsi.bus_num;
            fdchan->tgt = devinfo.v1.businfo.bus.scsi.tgt_id;
            fdchan->lun = devinfo.v1.businfo.bus.scsi.lun;
        }
        close (fd);
    } else {
        free(fdchan);
        return -1;
    }

    devicetable[k] = fdchan;
    return k;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_close_device(int device_fd)
{
    struct osf1_dev_channel *fdchan;
    int i;

    if ((device_fd < 0) || (device_fd >= OSF1_MAXDEV)) {
        errno = ENODEV;
        return -1;
    }

    fdchan = devicetable[device_fd];
    if (NULL == fdchan) {
        errno = ENODEV;
        return -1;
    }

    free(fdchan);
    devicetable[device_fd] = NULL;

    for (i = 0; i < OSF1_MAXDEV; i++) {
        if (devicetable[i])
            break;
    }
    if (i == OSF1_MAXDEV) {
        close(camfd);
        camopened = 0;
    }
    return 0;
}

struct sg_pt_base * construct_scsi_pt_obj()
{
    struct sg_pt_osf1_scsi * ptp;

    ptp = (struct sg_pt_osf1_scsi *)malloc(sizeof(struct sg_pt_osf1_scsi));
    if (ptp) {
        bzero(ptp, sizeof(struct sg_pt_osf1_scsi));
        ptp->dxfer_dir = CAM_DIR_NONE;
    }
    return (struct sg_pt_base *)ptp;
}

void destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp)
        free(ptp);
}

void set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                     int cdb_len)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp->cdb)
        ++ptp->in_err;
    ptp->cdb = (unsigned char *)cdb;
    ptp->cdb_len = cdb_len;
}

void set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                       int max_sense_len)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp->sense)
        ++ptp->in_err;
    bzero(sense, max_sense_len);
    ptp->sense = sense;
    ptp->sense_len = max_sense_len;
}

void set_scsi_pt_data_in(struct sg_pt_base * vp,             /* from device */
                         unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp->dxferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->dxferp = dxferp;
        ptp->dxfer_len = dxfer_len;
        ptp->dxfer_dir = CAM_DIR_IN;
    }
}

void set_scsi_pt_data_out(struct sg_pt_base * vp,            /* to device */
                          const unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp->dxferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->dxferp = (unsigned char *)dxferp;
        ptp->dxfer_len = dxfer_len;
        ptp->dxfer_dir = CAM_DIR_OUT;
    }
}

void set_scsi_pt_packet_id(struct sg_pt_base * vp, int pack_id)
{
}

void set_scsi_pt_tag(struct sg_pt_base * vp, unsigned long long tag)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

void set_scsi_pt_task_management(struct sg_pt_base * vp, int tmf_code)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

void set_scsi_pt_task_attr(struct sg_pt_base * vp, int attrib, int priority)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

static int release_sim(struct sg_pt_base *vp, int device_fd, int verbose) {
    struct sg_pt_osf1_scsi * ptp = &vp->impl;
    struct osf1_dev_channel *fdchan = devicetable[device_fd];
    UAGT_CAM_CCB uagt;
    CCB_RELSIM relsim;
    int retval;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;

    bzero(&uagt, sizeof(uagt));
    bzero(&relsim, sizeof(relsim));

    uagt.uagt_ccb = (CCB_HEADER *) &relsim;
    uagt.uagt_ccblen = sizeof(relsim);

    relsim.cam_ch.cam_ccb_len = sizeof(relsim);
    relsim.cam_ch.cam_func_code = XPT_REL_SIMQ;
    relsim.cam_ch.cam_flags = CAM_DIR_IN | CAM_DIS_CALLBACK;
    relsim.cam_ch.cam_path_id = fdchan->bus;
    relsim.cam_ch.cam_target_id = fdchan->tgt;
    relsim.cam_ch.cam_target_lun = fdchan->lun;

    retval = ioctl(camfd, UAGT_CAM_IO, &uagt);
    if (retval < 0) {
        if (verbose)
            fprintf(sg_warnings_strm, "CAM ioctl error (Release SIM Queue)\n");
    }
    return retval;
}

int do_scsi_pt(struct sg_pt_base * vp, int device_fd, int time_secs, int verbose)
{
    struct sg_pt_osf1_scsi * ptp = &vp->impl;
    struct osf1_dev_channel *fdchan;
    int len, retval;
    CCB_SCSIIO ccb;
    UAGT_CAM_CCB uagt;
    unsigned char sensep[ADDL_SENSE_LENGTH];


    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (ptp->in_err) {
        if (verbose)
            fprintf(sg_warnings_strm, "Replicated or unused set_scsi_pt...\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    if (NULL == ptp->cdb) {
        if (verbose)
            fprintf(sg_warnings_strm, "No command (cdb) given\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }

    if ((device_fd < 0) || (device_fd >= OSF1_MAXDEV)) {
        if (verbose)
            fprintf(sg_warnings_strm, "Bad file descriptor\n");
        ptp->os_err = ENODEV;
        return -ptp->os_err;
    }
    fdchan = devicetable[device_fd];
    if (NULL == fdchan) {
        if (verbose)
            fprintf(sg_warnings_strm, "File descriptor closed??\n");
        ptp->os_err = ENODEV;
        return -ptp->os_err;
    }
    if (0 == camopened) {
        if (verbose)
            fprintf(sg_warnings_strm, "No open CAM device\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }


    bzero(&uagt, sizeof(uagt));
    bzero(&ccb, sizeof(ccb));

    uagt.uagt_ccb = (CCB_HEADER *) &ccb;
    uagt.uagt_ccblen = sizeof(ccb);
    uagt.uagt_snsbuf = ccb.cam_sense_ptr = ptp->sense ? ptp->sense : sensep;
    uagt.uagt_snslen = ccb.cam_sense_len = ptp->sense ? ptp->sense_len : sizeof sensep;
    uagt.uagt_buffer = ccb.cam_data_ptr =  ptp->dxferp;
    uagt.uagt_buflen = ccb.cam_dxfer_len = ptp->dxfer_len;

    ccb.cam_timeout = time_secs;
    ccb.cam_ch.my_addr = (CCB_HEADER *) &ccb;
    ccb.cam_ch.cam_ccb_len = sizeof(ccb);
    ccb.cam_ch.cam_func_code = XPT_SCSI_IO;
    ccb.cam_ch.cam_flags = ptp->dxfer_dir;
    ccb.cam_cdb_len = ptp->cdb_len;
    memcpy(ccb.cam_cdb_io.cam_cdb_bytes, ptp->cdb, ptp->cdb_len);
    ccb.cam_ch.cam_path_id = fdchan->bus;
    ccb.cam_ch.cam_target_id = fdchan->tgt;
    ccb.cam_ch.cam_target_lun = fdchan->lun;

    if (ioctl(camfd, UAGT_CAM_IO, &uagt) < 0) {
        if (verbose)
            fprintf(sg_warnings_strm, "CAN I/O Error\n");
        ptp->os_err = EIO;
        return -ptp->os_err;
    }

    if (((ccb.cam_ch.cam_status & CAM_STATUS_MASK) == CAM_REQ_CMP) ||
            ((ccb.cam_ch.cam_status & CAM_STATUS_MASK) == CAM_REQ_CMP_ERR)) {
        ptp->scsi_status = ccb.cam_scsi_status;
        ptp->resid = ccb.cam_resid;
        if (ptp->sense)
            ptp->sense_resid = ccb.cam_sense_resid;
    } else {
        ptp->transport_err = 1;
    }

    /* If the SIM queue is frozen, release SIM queue. */
    if (ccb.cam_ch.cam_status & CAM_SIM_QFRZN)
        release_sim(vp, device_fd, verbose);

    return 0;
}

int get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (ptp->os_err)
        return SCSI_PT_RESULT_OS_ERR;
    else if (ptp->transport_err)
        return SCSI_PT_RESULT_TRANSPORT_ERR;
    else if ((SAM_STAT_CHECK_CONDITION == ptp->scsi_status) ||
             (SAM_STAT_COMMAND_TERMINATED == ptp->scsi_status))
        return SCSI_PT_RESULT_SENSE;
    else if (ptp->scsi_status)
        return SCSI_PT_RESULT_STATUS;
    else
        return SCSI_PT_RESULT_GOOD;
}

int get_scsi_pt_resid(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    return ptp->resid;
}

int get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    return ptp->scsi_status;
}

int get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;
    int len;

    len = ptp->sense_len - ptp->sense_resid;
    return (len > 0) ? len : 0;
}

int get_scsi_pt_duration_ms(const struct sg_pt_base * vp)
{
    // const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    return -1;
}

int get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    return ptp->transport_err;
}

int get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    return ptp->os_err;
}


char * get_scsi_pt_transport_err_str(const struct sg_pt_base * vp, int max_b_len, char * b)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;

    if (0 == ptp->transport_err) {
        strncpy(b, "no transport error available", max_b_len);
        b[max_b_len - 1] = '\0';
        return b;
    }
    strncpy(b, "no transport error available", max_b_len);
    b[max_b_len - 1] = '\0';
    return b;
}

char * get_scsi_pt_os_err_str(const struct sg_pt_base * vp,
                              int max_b_len, char * b)
{
    const struct sg_pt_osf1_scsi * ptp = &vp->impl;
    const char * cp;

    cp = safe_strerror(ptp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}
