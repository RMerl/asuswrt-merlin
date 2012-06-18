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

/* version 1.07 2007/4/3 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
// #include <sys/ata.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <fcntl.h>
#include <stddef.h>

#include "sg_pt.h"
#include "sg_lib.h"


#define FREEBSD_MAXDEV 64
#define FREEBSD_FDOFFSET 16;


struct freebsd_dev_channel {
  char* devname;                // the SCSI device name
  int   unitnum;                // the SCSI unit number
  struct cam_device* cam_dev;
};

// Private table of open devices: guaranteed zero on startup since
// part of static data.
static struct freebsd_dev_channel *devicetable[FREEBSD_MAXDEV];

#define DEF_TIMEOUT 60000       /* 60,000 milliseconds (60 seconds) */

struct sg_pt_freebsd_scsi {
    struct cam_device* cam_dev; // copy held for error processing
    union ccb *ccb;
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
    struct sg_pt_freebsd_scsi impl;
};


/* Returns >= 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_open_device(const char * device_name,
                        int read_only __attribute__ ((unused)),
                        int verbose)
{
    struct freebsd_dev_channel *fdchan;
    struct cam_device* cam_dev;
    int k;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    // Search table for a free entry
    for (k = 0; k < FREEBSD_MAXDEV; k++)
        if (! devicetable[k])
            break;
  
    // If no free entry found, return error.  We have max allowed number
    // of "file descriptors" already allocated.
    if (k == FREEBSD_MAXDEV) {
        if (verbose)
            fprintf(sg_warnings_strm, "too many open file descriptors "
                    "(%d)\n", FREEBSD_MAXDEV);
        errno = EMFILE;
        return -1;
    }

    fdchan = (struct freebsd_dev_channel *)
                calloc(1,sizeof(struct freebsd_dev_channel));
    if (fdchan == NULL) {
        // errno already set by call to malloc()
        return -1;
    }

    if (! (fdchan->devname = (char *)calloc(1, DEV_IDLEN+1)))
         return -1;

    if (cam_get_device(device_name, fdchan->devname, DEV_IDLEN,
                       &(fdchan->unitnum)) == -1) {
        if (verbose)
            fprintf(sg_warnings_strm, "bad device name structure\n");
        errno = EINVAL;
        return -1;
    }

    if (! (cam_dev = cam_open_spec_device(fdchan->devname,
                                          fdchan->unitnum, O_RDWR, NULL))) {
        if (verbose)
            fprintf(sg_warnings_strm, "cam_open_spec_device: %s\n",
                    cam_errbuf);
        errno = EPERM;    /* permissions or no CAM */
        return -1;
    }
    fdchan->cam_dev = cam_dev;
    // return pointer to "file descriptor" table entry, properly offset.
    devicetable[k] = fdchan;
    return k + FREEBSD_FDOFFSET;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_close_device(int device_fd)
{
    struct freebsd_dev_channel *fdchan;
    int fd = device_fd - FREEBSD_FDOFFSET;

    if ((fd < 0) || (fd >= FREEBSD_MAXDEV)) {
        errno = ENODEV;
        return -1;
    }
    fdchan = devicetable[fd];
    if (NULL == fdchan) {
        errno = ENODEV;
        return -1;
    }
    if (fdchan->devname)
        free(fdchan->devname);
    if (fdchan->cam_dev)
        cam_close_device(fdchan->cam_dev);
    free(fdchan);
    devicetable[fd] = NULL;
    return 0;
}

struct sg_pt_base * construct_scsi_pt_obj()
{
    struct sg_pt_freebsd_scsi * ptp;

    ptp = (struct sg_pt_freebsd_scsi *)
                malloc(sizeof(struct sg_pt_freebsd_scsi));
    if (ptp) {
        memset(ptp, 0, sizeof(struct sg_pt_freebsd_scsi));
        ptp->dxfer_dir = CAM_DIR_NONE;
    }
    return (struct sg_pt_base *)ptp;
}

void destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    if (ptp) {
        if (ptp->ccb)
            cam_freeccb(ptp->ccb);
        free(ptp);
    }
}

void set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                     int cdb_len)
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    if (ptp->cdb)
        ++ptp->in_err;
    ptp->cdb = (unsigned char *)cdb;
    ptp->cdb_len = cdb_len;
}

void set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                       int max_sense_len)
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    if (ptp->sense)
        ++ptp->in_err;
    memset(sense, 0, max_sense_len);
    ptp->sense = sense;
    ptp->sense_len = max_sense_len;
}

void set_scsi_pt_data_in(struct sg_pt_base * vp,             /* from device */
                         unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

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
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    if (ptp->dxferp)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->dxferp = (unsigned char *)dxferp;
        ptp->dxfer_len = dxfer_len;
        ptp->dxfer_dir = CAM_DIR_OUT;
    }
}

void set_scsi_pt_packet_id(struct sg_pt_base * vp __attribute__ ((unused)),
                           int pack_id __attribute__ ((unused)))
{
}

void set_scsi_pt_tag(struct sg_pt_base * vp,
                     unsigned long long tag __attribute__ ((unused)))
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

void set_scsi_pt_task_management(struct sg_pt_base * vp,
                                 int tmf_code __attribute__ ((unused)))
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

void set_scsi_pt_task_attr(struct sg_pt_base * vp,
                           int attrib __attribute__ ((unused)),
                           int priority __attribute__ ((unused)))
{
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    ++ptp->in_err;
}

int do_scsi_pt(struct sg_pt_base * vp, int device_fd, int time_secs,
               int verbose)
{
    int fd = device_fd - FREEBSD_FDOFFSET;
    struct sg_pt_freebsd_scsi * ptp = &vp->impl;
    struct freebsd_dev_channel *fdchan;
    union ccb *ccb;
    int len, timout_ms;

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

    if ((fd < 0) || (fd >= FREEBSD_MAXDEV)) {
        if (verbose)
            fprintf(sg_warnings_strm, "Bad file descriptor\n");
        ptp->os_err = ENODEV;
        return -ptp->os_err;
    }
    fdchan = devicetable[fd];
    if (NULL == fdchan) {
        if (verbose)
            fprintf(sg_warnings_strm, "File descriptor closed??\n");
        ptp->os_err = ENODEV;
        return -ptp->os_err;
    }
    if (NULL == fdchan->cam_dev) {
        if (verbose)
            fprintf(sg_warnings_strm, "No open CAM device\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }

    if (! (ccb = cam_getccb(fdchan->cam_dev))) {
        if (verbose)
            fprintf(sg_warnings_strm, "cam_getccb: failed\n");
        ptp->os_err = ENOMEM;
        return -ptp->os_err;
    }
    ptp->ccb = ccb;

    // clear out structure, except for header that was filled in for us
    bzero(&(&ccb->ccb_h)[1],
            sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

    timout_ms = (time_secs > 0) ? (time_secs * 1000) : DEF_TIMEOUT;
    cam_fill_csio(&ccb->csio,
                  /* retries */ 1,
                  /* cbfcnp */ NULL,
                  /* flags */ ptp->dxfer_dir,
                  /* tagaction */ MSG_SIMPLE_Q_TAG,
                  /* dataptr */ ptp->dxferp,
                  /* datalen */ ptp->dxfer_len,
                  /* senselen */ ptp->sense_len,
                  /* cdblen */ ptp->cdb_len,
                  /* timeout (millisecs) */ timout_ms);
    memcpy(ccb->csio.cdb_io.cdb_bytes, ptp->cdb, ptp->cdb_len);

    if (cam_send_ccb(fdchan->cam_dev, ccb) < 0) {
        if (verbose) {
            warn("error sending SCSI ccb");
 #if __FreeBSD_version > 500000
            cam_error_print(fdchan->cam_dev, ccb, CAM_ESF_ALL,
                            CAM_EPF_ALL, stderr);
 #endif
        }
        cam_freeccb(ptp->ccb);
        ptp->ccb = NULL;
        ptp->os_err = EIO;
        return -ptp->os_err;
    }

    if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) ||
        ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR)) {
        ptp->scsi_status = ccb->csio.scsi_status;
        ptp->resid = ccb->csio.resid;
        ptp->sense_resid = ccb->csio.sense_resid;

        if ((SAM_STAT_CHECK_CONDITION == ptp->scsi_status) ||
            (SAM_STAT_COMMAND_TERMINATED == ptp->scsi_status)) {
            len = ptp->sense_len - ptp->sense_resid;
            if (len)
                memcpy(ptp->sense, &(ccb->csio.sense_data), len);
        }
    } else
        ptp->transport_err = 1;

    ptp->cam_dev = fdchan->cam_dev;     // for error processing
    return 0;
}

int get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

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
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    return ptp->resid;
}

int get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    return ptp->scsi_status;
}

int get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;
    int len;

    len = ptp->sense_len - ptp->sense_resid;
    return (len > 0) ? len : 0;
}

int get_scsi_pt_duration_ms(const struct sg_pt_base * vp __attribute__ ((unused)))
{
    // const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    return -1;
}

int get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    return ptp->transport_err;
}

int get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    return ptp->os_err;
}


char * get_scsi_pt_transport_err_str(const struct sg_pt_base * vp,
                                     int max_b_len, char * b)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;

    if (0 == ptp->transport_err) {
        strncpy(b, "no transport error available", max_b_len);
        b[max_b_len - 1] = '\0';
        return b;
    }
#if __FreeBSD_version > 500000
    if (ptp->cam_dev)
        cam_error_string(ptp->cam_dev, ptp->ccb, b, max_b_len, CAM_ESF_ALL,
                         CAM_EPF_ALL);
    else {
        strncpy(b, "no transport error available", max_b_len);
        b[max_b_len - 1] = '\0';
   }
#else
    strncpy(b, "no transport error available", max_b_len);
    b[max_b_len - 1] = '\0';
#endif
    return b;
}

char * get_scsi_pt_os_err_str(const struct sg_pt_base * vp,
                              int max_b_len, char * b)
{
    const struct sg_pt_freebsd_scsi * ptp = &vp->impl;
    const char * cp;

    cp = safe_strerror(ptp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}
