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
 * Copyright (c) 2007 Douglas Gilbert.
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

/* version 1.00 2007/5/3 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>

/* Solaris headers */
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/impl/types.h>
#include <sys/scsi/impl/uscsi.h>

#include "sg_pt.h"
#include "sg_lib.h"


#define DEF_TIMEOUT 60       /* 60 seconds */

struct sg_pt_solaris_scsi {
    struct uscsi_cmd uscsi;
    int max_sense_len;
    int in_err;
    int os_err;
};

struct sg_pt_base {
    struct sg_pt_solaris_scsi impl;
};



/* Returns >= 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_open_device(const char * device_name, int read_only,
                        int verbose)
{
    int oflags = O_NONBLOCK | O_RDWR;
    int fd;

    read_only = read_only;  /* ignore read_only, suppress warning */
    if (verbose > 1) {
        fprintf(stderr, "open %s with flags=0x%x\n", device_name, oflags);
    }
    fd = open(device_name, oflags);
    if (fd < 0)
        fd = -errno;
    return fd;
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int scsi_pt_close_device(int device_fd)
{
    int res;

    res = close(device_fd);
    if (res < 0)
        res = -errno;
    return res;
}


struct sg_pt_base * construct_scsi_pt_obj()
{
    struct sg_pt_solaris_scsi * ptp;

    ptp = (struct sg_pt_solaris_scsi *)
          malloc(sizeof(struct sg_pt_solaris_scsi));
    if (ptp) {
        memset(ptp, 0, sizeof(struct sg_pt_solaris_scsi));
        ptp->uscsi.uscsi_timeout = DEF_TIMEOUT;
        ptp->uscsi.uscsi_flags = USCSI_READ | USCSI_ISOLATE | USCSI_RQENABLE;
        ptp->uscsi.uscsi_timeout = DEF_TIMEOUT;
    }
    return (struct sg_pt_base *)ptp;
}

void destruct_scsi_pt_obj(struct sg_pt_base * vp)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp)
        free(ptp);
}

void set_scsi_pt_cdb(struct sg_pt_base * vp, const unsigned char * cdb,
                     int cdb_len)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp->uscsi.uscsi_cdb)
        ++ptp->in_err;
    ptp->uscsi.uscsi_cdb = (char *)cdb;
    ptp->uscsi.uscsi_cdblen = cdb_len;
}

void set_scsi_pt_sense(struct sg_pt_base * vp, unsigned char * sense,
                       int max_sense_len)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp->uscsi.uscsi_rqbuf)
        ++ptp->in_err;
    memset(sense, 0, max_sense_len);
    ptp->uscsi.uscsi_rqbuf = (char *)sense;
    ptp->uscsi.uscsi_rqlen = max_sense_len;
    ptp->max_sense_len = max_sense_len;
}

void set_scsi_pt_data_in(struct sg_pt_base * vp,  /* from device */
                         unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp->uscsi.uscsi_bufaddr)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->uscsi.uscsi_bufaddr = (char *)dxferp;
        ptp->uscsi.uscsi_buflen = dxfer_len;
        ptp->uscsi.uscsi_flags = USCSI_READ | USCSI_ISOLATE | USCSI_RQENABLE;
    }
}

void set_scsi_pt_data_out(struct sg_pt_base * vp,   /* to device */
                          const unsigned char * dxferp, int dxfer_len)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp->uscsi.uscsi_bufaddr)
        ++ptp->in_err;
    if (dxfer_len > 0) {
        ptp->uscsi.uscsi_bufaddr = (char *)dxferp;
        ptp->uscsi.uscsi_buflen = dxfer_len;
        ptp->uscsi.uscsi_flags = USCSI_WRITE | USCSI_ISOLATE | USCSI_RQENABLE;
    }
}

void set_scsi_pt_packet_id(struct sg_pt_base * vp, int pack_id)
{
    // struct sg_pt_solaris_scsi * ptp = &vp->impl;

    vp = vp;          		/* ignore and suppress warning */
    pack_id = pack_id;          /* ignore and suppress warning */
}

void set_scsi_pt_tag(struct sg_pt_base * vp, unsigned long long tag)
{
    // struct sg_pt_solaris_scsi * ptp = &vp->impl;

    vp = vp;          		/* ignore and suppress warning */
    tag = tag;          	/* ignore and suppress warning */
}

/* Note that task management function codes are transport specific */
void set_scsi_pt_task_management(struct sg_pt_base * vp, int tmf_code)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    ++ptp->in_err;
    tmf_code = tmf_code;        /* dummy to silence compiler */
}

void set_scsi_pt_task_attr(struct sg_pt_base * vp, int attribute,
                           int priority)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    ++ptp->in_err;
    attribute = attribute;      /* dummy to silence compiler */
    priority = priority;        /* dummy to silence compiler */
}

int do_scsi_pt(struct sg_pt_base * vp, int fd, int time_secs, int verbose)
{
    struct sg_pt_solaris_scsi * ptp = &vp->impl;

    if (ptp->in_err) {
        if (verbose)
            fprintf(stderr, "Replicated or unused set_scsi_pt... "
                    "functions\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    if (NULL == ptp->uscsi.uscsi_cdb) {
        if (verbose)
            fprintf(stderr, "No SCSI command (cdb) given\n");
        return SCSI_PT_DO_BAD_PARAMS;
    }
    if (time_secs > 0)
        ptp->uscsi.uscsi_timeout = time_secs;

    if (ioctl(fd, USCSICMD, &ptp->uscsi)) {
        ptp->os_err = errno;
	if ((EIO == ptp->os_err) && ptp->uscsi.uscsi_status) {
	    ptp->os_err = 0;
	    return 0;
	}
        if (verbose)
            fprintf(stderr, "ioctl(USCSICMD) failed with os_err "
                    "(errno) = %d\n", ptp->os_err);
        return -ptp->os_err;
    }
    return 0;
}

int get_scsi_pt_result_category(const struct sg_pt_base * vp)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;
    int scsi_st = ptp->uscsi.uscsi_status;

    if (ptp->os_err)
        return SCSI_PT_RESULT_OS_ERR;
    else if ((SAM_STAT_CHECK_CONDITION == scsi_st) ||
             (SAM_STAT_COMMAND_TERMINATED == scsi_st))
        return SCSI_PT_RESULT_SENSE;
    else if (scsi_st)
        return SCSI_PT_RESULT_STATUS;
    else
        return SCSI_PT_RESULT_GOOD;
}

int get_scsi_pt_resid(const struct sg_pt_base * vp)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    return ptp->uscsi.uscsi_resid;
}

int get_scsi_pt_status_response(const struct sg_pt_base * vp)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    return ptp->uscsi.uscsi_status;
}

int get_scsi_pt_sense_len(const struct sg_pt_base * vp)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;
    int res;

    if (ptp->max_sense_len > 0) {
        res = ptp->max_sense_len - ptp->uscsi.uscsi_rqresid;
        return (res > 0) ? res : 0;
    }
    return 0;
}

int get_scsi_pt_duration_ms(const struct sg_pt_base * vp)
{
    // const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    vp = vp;          	/* ignore and suppress warning */
    return -1;          /* not available */
}

int get_scsi_pt_transport_err(const struct sg_pt_base * vp)
{
    // const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    vp = vp;          	/* ignore and suppress warning */
    return 0;
}

int get_scsi_pt_os_err(const struct sg_pt_base * vp)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    return ptp->os_err;
}

char * get_scsi_pt_transport_err_str(const struct sg_pt_base * vp,
                                     int max_b_len, char * b)
{
    // const struct sg_pt_solaris_scsi * ptp = &vp->impl;

    vp = vp;          	/* ignore and suppress warning */
    if (max_b_len > 0)
        b[0] = '\0';

    return b;
}

char * get_scsi_pt_os_err_str(const struct sg_pt_base * vp,
                              int max_b_len, char * b)
{
    const struct sg_pt_solaris_scsi * ptp = &vp->impl;
    const char * cp;

    cp = safe_strerror(ptp->os_err);
    strncpy(b, cp, max_b_len);
    if ((int)strlen(cp) >= max_b_len)
        b[max_b_len - 1] = '\0';
    return b;
}
