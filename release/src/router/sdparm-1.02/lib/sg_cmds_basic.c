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
 * Copyright (c) 1999-2007 Douglas Gilbert.
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
 * CONTENTS
 *    Some SCSI commands are executed in many contexts and hence began
 *    to appear in several sg3_utils utilities. This files centralizes
 *    some of the low level command execution code. In most cases the
 *    interpretation of the command response is left to the each
 *    utility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_pt.h"


static char * version_str = "1.40 20070803";


#define SENSE_BUFF_LEN 32       /* Arbitrary, could be larger */
#define EBUFF_SZ 256

#define DEF_PT_TIMEOUT 60       /* 60 seconds */
#define START_PT_TIMEOUT 120    /* 120 seconds == 2 minutes */
#define LONG_PT_TIMEOUT 7200    /* 7,200 seconds == 120 minutes */

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6
#define SYNCHRONIZE_CACHE_CMD     0x35
#define SYNCHRONIZE_CACHE_CMDLEN  10
#define SERVICE_ACTION_IN_16_CMD 0x9e
#define SERVICE_ACTION_IN_16_CMDLEN 16
#define READ_CAPACITY_16_SA 0x10
#define READ_CAPACITY_10_CMD 0x25
#define READ_CAPACITY_10_CMDLEN 10
#define MODE_SENSE6_CMD      0x1a
#define MODE_SENSE6_CMDLEN   6
#define MODE_SENSE10_CMD     0x5a
#define MODE_SENSE10_CMDLEN  10
#define MODE_SELECT6_CMD   0x15
#define MODE_SELECT6_CMDLEN   6
#define MODE_SELECT10_CMD   0x55
#define MODE_SELECT10_CMDLEN  10
#define REQUEST_SENSE_CMD 0x3
#define REQUEST_SENSE_CMDLEN 6
#define REPORT_LUNS_CMD 0xa0
#define REPORT_LUNS_CMDLEN 12
#define LOG_SENSE_CMD     0x4d
#define LOG_SENSE_CMDLEN  10
#define LOG_SELECT_CMD     0x4c
#define LOG_SELECT_CMDLEN  10
#define TUR_CMD  0x0
#define TUR_CMDLEN  6
#define START_STOP_CMD          0x1b
#define START_STOP_CMDLEN       6
#define PREVENT_ALLOW_CMD    0x1e
#define PREVENT_ALLOW_CMDLEN   6

#define MODE6_RESP_HDR_LEN 4
#define MODE10_RESP_HDR_LEN 8
#define MODE_RESP_ARB_LEN 1024

#define INQUIRY_RESP_INITIAL_LEN 36


const char * sg_cmds_version()
{
    return version_str;
}

/* Returns file descriptor >= 0 if successful. If error in Unix returns
   negated errno. */
int sg_cmds_open_device(const char * device_name, int read_only,
                        int verbose)
{
    return scsi_pt_open_device(device_name, read_only, verbose);
}

/* Returns 0 if successful. If error in Unix returns negated errno. */
int sg_cmds_close_device(int device_fd)
{
    return scsi_pt_close_device(device_fd);
}


/* Returns -2 for sense data (may not be fatal), -1 for failed or the
   number of bytes fetched. For data out (to device) or no data, set
   'mx_resp_len' to 0 or less. If -2 returned then sense category
   output via 'o_sense_cat' pointer (if not NULL). Outputs to
   sg_warnings_strm (def: stderr) if problems; depending on 'noisy'
   and 'verbose' */
int sg_cmds_process_resp(struct sg_pt_base * ptvp, const char * leadin, int res,
                         int mx_resp_len, const unsigned char * sense_b,
                         int noisy, int verbose, int * o_sense_cat)
{
    int got, cat, duration, slen, scat, n, resid;
    char b[1024];

    if (NULL == leadin)
        leadin = "";
    if (res < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_strm, "%s: pass through os error: %s\n",
                    leadin, safe_strerror(-res));
        return -1;
    } else if (SCSI_PT_DO_BAD_PARAMS == res) {
        fprintf(sg_warnings_strm, "%s: bad pass through setup\n", leadin);
        return -1;
    } else if (SCSI_PT_DO_TIMEOUT == res) {
        fprintf(sg_warnings_strm, "%s: pass through timeout\n", leadin);
        return -1;
    }
    if ((verbose > 2) && ((duration = get_scsi_pt_duration_ms(ptvp)) >= 0))
        fprintf(sg_warnings_strm, "      duration=%d ms\n", duration);
    resid = (mx_resp_len > 0) ? get_scsi_pt_resid(ptvp) : 0;
    switch ((cat = get_scsi_pt_result_category(ptvp))) {
    case SCSI_PT_RESULT_GOOD:
        if (mx_resp_len > 0) {
            got = mx_resp_len - resid;
            if (verbose && (resid > 0))
                fprintf(sg_warnings_strm, "    %s: requested %d bytes but "
                        "got %d bytes\n", leadin, mx_resp_len, got);
            return got;
        } else
            return 0;
    case SCSI_PT_RESULT_STATUS: /* other than GOOD and CHECK CONDITION */
        if (verbose || noisy) {
            sg_get_scsi_status_str(get_scsi_pt_status_response(ptvp),
                                   sizeof(b), b);
            fprintf(sg_warnings_strm, "%s: scsi status: %s\n", leadin, b);
        }
        return -1;
    case SCSI_PT_RESULT_SENSE:
        slen = get_scsi_pt_sense_len(ptvp);
        scat = sg_err_category_sense(sense_b, slen);
        switch (scat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
        case SG_LIB_CAT_NO_SENSE:
            n = 0;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_MEDIUM_HARD:
        default:
            n = noisy;
            break;
        }
        if (verbose || n) {
            sg_get_sense_str(leadin, sense_b, slen, (verbose > 1),
                             sizeof(b), b);
            fprintf(sg_warnings_strm, "%s", b);
        }
        if (verbose && (mx_resp_len > 0) && (resid > 0)) {
            got = mx_resp_len - resid;
            if ((verbose > 2) || (got > 0))
                fprintf(sg_warnings_strm, "    requested %d bytes but "
                        "got %d bytes\n", mx_resp_len, got);
        }
        if (o_sense_cat)
            *o_sense_cat = scat;
        return -2;
    case SCSI_PT_RESULT_TRANSPORT_ERR:
        if (verbose || noisy) {
            get_scsi_pt_transport_err_str(ptvp, sizeof(b), b);
            fprintf(sg_warnings_strm, "%s: transport: %s\n", leadin, b);
        }
        return -1;
    case SCSI_PT_RESULT_OS_ERR:
        if (verbose || noisy) {
            get_scsi_pt_os_err_str(ptvp, sizeof(b), b);
            fprintf(sg_warnings_strm, "%s: os: %s\n", leadin, b);
        }
        return -1;
    default:
        fprintf(sg_warnings_strm, "%s: unknown pass through result "
                "category (%d)\n", leadin, cat);
        return -1;
    }
}

/* Invokes a SCSI INQUIRY command and yields the response
 * Returns 0 when successful, SG_LIB_CAT_INVALID_OP -> not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_MALFORMED -> bad response, -1 -> other errors */
int sg_ll_inquiry(int sg_fd, int cmddt, int evpd, int pg_op, 
                  void * resp, int mx_resp_len, int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    unsigned char * up;
    struct sg_pt_base * ptvp;

    if (cmddt)
        inqCmdBlk[1] |= 2;
    if (evpd)
        inqCmdBlk[1] |= 1;
    inqCmdBlk[2] = (unsigned char)pg_op;
    /* 16 bit allocation length (was 8) is a recent SPC-3 addition */
    inqCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    inqCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    inquiry cdb: ");
        for (k = 0; k < INQUIRY_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", inqCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if (resp && (mx_resp_len > 0)) {
        up = (unsigned char *)resp;
        up[0] = 0x7f;   /* defensive prefill */
        if (mx_resp_len > 4)
            up[4] = 0;
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "inquiry: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, inqCmdBlk, sizeof(inqCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "inquiry", res, mx_resp_len, sense_b,
                               noisy, verbose, &sense_cat);
    destruct_scsi_pt_obj(ptvp);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else if (ret < 4) {
        if (verbose)
            fprintf(sg_warnings_strm, "inquiry: got too few "
                    "bytes (%d)\n", ret);
        ret = SG_LIB_CAT_MALFORMED;
    } else
        ret = 0;

    return ret;
}

/* Yields most of first 36 bytes of a standard INQUIRY (evpd==0) response.
 * Returns 0 when successful, SG_LIB_CAT_INVALID_OP -> not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_MALFORMED -> bad response, -1 -> other errors */
int sg_simple_inquiry(int sg_fd, struct sg_simple_inquiry_resp * inq_data,
                      int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    unsigned char inq_resp[INQUIRY_RESP_INITIAL_LEN];
    struct sg_pt_base * ptvp;

    if (inq_data) {
        memset(inq_data, 0, sizeof(* inq_data));
        inq_data->peripheral_qualifier = 0x3;
        inq_data->peripheral_type = 0x1f;
    }
    inqCmdBlk[4] = (unsigned char)sizeof(inq_resp);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    inquiry cdb: ");
        for (k = 0; k < INQUIRY_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", inqCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    memset(inq_resp, 0, sizeof(inq_resp));
    inq_resp[0] = 0x7f; /* defensive prefill */
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "inquiry: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, inqCmdBlk, sizeof(inqCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, inq_resp, sizeof(inq_resp));
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "inquiry", res, sizeof(inq_resp),
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else if (ret < 4) {
        if (verbose)
            fprintf(sg_warnings_strm, "inquiry: got too few "
                    "bytes (%d)\n", ret);
        ret = SG_LIB_CAT_MALFORMED;
    } else
        ret = 0;

    if (0 == ret) {
        inq_data->peripheral_qualifier = (inq_resp[0] >> 5) & 0x7;
        inq_data->peripheral_type = inq_resp[0] & 0x1f;
        inq_data->rmb = (inq_resp[1] & 0x80) ? 1 : 0;
        inq_data->version = inq_resp[2];
        inq_data->byte_3 = inq_resp[3];
        inq_data->byte_5 = inq_resp[5];
        inq_data->byte_6 = inq_resp[6];
        inq_data->byte_7 = inq_resp[7];
        memcpy(inq_data->vendor, inq_resp + 8, 8);
        memcpy(inq_data->product, inq_resp + 16, 16);
        memcpy(inq_data->revision, inq_resp + 32, 4);
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI TEST UNIT READY command.
 * 'pack_id' is just for diagnostics, safe to set to 0.
 * Looks for progress indicator if 'progress' non-NULL;
 * if found writes value [0..65535] else write -1.
 * Return of 0 -> success, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_NOT_READY ->
 * device not ready, -1 -> other failure */
int sg_ll_test_unit_ready_progress(int sg_fd, int pack_id, int * progress,
                                   int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char turCmdBlk[TUR_CMDLEN] = {TUR_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    test unit ready cdb: ");
        for (k = 0; k < TUR_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", turCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "test unit ready: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, turCmdBlk, sizeof(turCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_packet_id(ptvp, pack_id);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "test unit ready", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        if (progress) {
            int slen = get_scsi_pt_sense_len(ptvp);

            if (! sg_get_sense_progress_fld(sense_b, slen, progress))
                *progress = -1;
        }
        switch (sense_cat) {
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI TEST UNIT READY command.
 * 'pack_id' is just for diagnostics, safe to set to 0.
 * Return of 0 -> success, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_NOT_READY ->
 * device not ready, -1 -> other failure */
int sg_ll_test_unit_ready(int sg_fd, int pack_id, int noisy, int verbose)
{
    return sg_ll_test_unit_ready_progress(sg_fd, pack_id, NULL, noisy,
                                          verbose);
}

/* Invokes a SCSI SYNCHRONIZE CACHE (10) command. Return of 0 -> success,
 * SG_LIB_CAT_UNIT_ATTENTION -> repeat,
 * SG_LIB_CAT_INVALID_OP -> cdb not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_NOT_READY -> device not ready, -1 -> other failure */
int sg_ll_sync_cache_10(int sg_fd, int sync_nv, int immed, int group,
                        unsigned int lba, unsigned int count, int noisy,
                        int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char scCmdBlk[SYNCHRONIZE_CACHE_CMDLEN] =
                {SYNCHRONIZE_CACHE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (sync_nv)
        scCmdBlk[1] |= 4;
    if (immed)
        scCmdBlk[1] |= 2;
    scCmdBlk[2] = (lba >> 24) & 0xff;
    scCmdBlk[3] = (lba >> 16) & 0xff;
    scCmdBlk[4] = (lba >> 8) & 0xff;
    scCmdBlk[5] = lba & 0xff;
    scCmdBlk[6] = group & 0x1f;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (count > 0xffff) {
        fprintf(sg_warnings_strm, "count too big\n");
        return -1;
    }
    scCmdBlk[7] = (count >> 8) & 0xff;
    scCmdBlk[8] = count & 0xff;

    if (verbose) {
        fprintf(sg_warnings_strm, "    synchronize cache(10) cdb: ");
        for (k = 0; k < SYNCHRONIZE_CACHE_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", scCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "synchronize cache(10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, scCmdBlk, sizeof(scCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "synchronize cache(10)", res, 0,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI READ CAPACITY (16) command. Returns 0 -> success,
 * SG_LIB_CAT_UNIT_ATTENTION -> media changed??, SG_LIB_CAT_INVALID_OP
 *  -> cdb not supported, SG_LIB_CAT_IlLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_NOT_READY -> device not ready,
 * -1 -> other failure */
int sg_ll_readcap_16(int sg_fd, int pmi, unsigned long long llba, 
                     void * resp, int mx_resp_len, int noisy, int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rcCmdBlk[SERVICE_ACTION_IN_16_CMDLEN] = 
                        {SERVICE_ACTION_IN_16_CMD, READ_CAPACITY_16_SA, 
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (pmi) { /* lbs only valid when pmi set */
        rcCmdBlk[14] |= 1;
        rcCmdBlk[2] = (llba >> 56) & 0xff;
        rcCmdBlk[3] = (llba >> 48) & 0xff;
        rcCmdBlk[4] = (llba >> 40) & 0xff;
        rcCmdBlk[5] = (llba >> 32) & 0xff;
        rcCmdBlk[6] = (llba >> 24) & 0xff;
        rcCmdBlk[7] = (llba >> 16) & 0xff;
        rcCmdBlk[8] = (llba >> 8) & 0xff;
        rcCmdBlk[9] = llba & 0xff;
    }
    /* Allocation length, no guidance in SBC-2 rev 15b */
    rcCmdBlk[10] = (mx_resp_len >> 24) & 0xff;
    rcCmdBlk[11] = (mx_resp_len >> 16) & 0xff;
    rcCmdBlk[12] = (mx_resp_len >> 8) & 0xff;
    rcCmdBlk[13] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    read capacity (16) cdb: ");
        for (k = 0; k < SERVICE_ACTION_IN_16_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rcCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read capacity (16): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rcCmdBlk, sizeof(rcCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read capacity (16)", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI READ CAPACITY (10) command. Returns 0 -> success,
 * SG_LIB_CAT_UNIT_ATTENTION -> media changed??, SG_LIB_CAT_INVALID_OP
 *  -> cdb not supported, SG_LIB_CAT_IlLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_NOT_READY -> device not ready,
 * -1 -> other failure */
int sg_ll_readcap_10(int sg_fd, int pmi, unsigned int lba, 
                     void * resp, int mx_resp_len, int noisy, int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rcCmdBlk[READ_CAPACITY_10_CMDLEN] =
                         {READ_CAPACITY_10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (pmi) { /* lbs only valid when pmi set */
        rcCmdBlk[8] |= 1;
        rcCmdBlk[2] = (lba >> 24) & 0xff;
        rcCmdBlk[3] = (lba >> 16) & 0xff;
        rcCmdBlk[4] = (lba >> 8) & 0xff;
        rcCmdBlk[5] = lba & 0xff;
    }
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    read capacity (10) cdb: ");
        for (k = 0; k < READ_CAPACITY_10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rcCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read capacity (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rcCmdBlk, sizeof(rcCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read capacity (10)", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI MODE SENSE (6) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_UNIT_ATTENTION,
 * -1 -> other failure */
int sg_ll_mode_sense6(int sg_fd, int dbd, int pc, int pg_code, int sub_pg_code,
                      void * resp, int mx_resp_len, int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char modesCmdBlk[MODE_SENSE6_CMDLEN] = 
        {MODE_SENSE6_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    modesCmdBlk[1] = (unsigned char)(dbd ? 0x8 : 0);
    modesCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    modesCmdBlk[3] = (unsigned char)(sub_pg_code & 0xff);
    modesCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (mx_resp_len > 0xff) {
        fprintf(sg_warnings_strm, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_strm, "    mode sense (6) cdb: ");
        for (k = 0; k < MODE_SENSE6_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "mode sense (6): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, modesCmdBlk, sizeof(modesCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "mode sense (6)", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else {
        if ((verbose > 2) && (ret > 0)) {
            fprintf(sg_warnings_strm, "    mode sense (6): response%s\n",
                    (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI MODE SENSE (10) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_UNIT_ATTENTION,
 * -1 -> other failure */
int sg_ll_mode_sense10(int sg_fd, int llbaa, int dbd, int pc, int pg_code,
                       int sub_pg_code, void * resp, int mx_resp_len,
                       int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char modesCmdBlk[MODE_SENSE10_CMDLEN] = 
        {MODE_SENSE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    modesCmdBlk[1] = (unsigned char)((dbd ? 0x8 : 0) | (llbaa ? 0x10 : 0));
    modesCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    modesCmdBlk[3] = (unsigned char)(sub_pg_code & 0xff);
    modesCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    modesCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_strm, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_strm, "    mode sense (10) cdb: ");
        for (k = 0; k < MODE_SENSE10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "mode sense (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, modesCmdBlk, sizeof(modesCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "mode sense (10)", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else {
        if ((verbose > 2) && (ret > 0)) {
            fprintf(sg_warnings_strm, "    mode sense (10): response%s\n",
                    (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI MODE SELECT (6) command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_UNIT_ATTENTION,
 * -1 -> other failure */
int sg_ll_mode_select6(int sg_fd, int pf, int sp, void * paramp,
                       int param_len, int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char modesCmdBlk[MODE_SELECT6_CMDLEN] = 
        {MODE_SELECT6_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    modesCmdBlk[1] = (unsigned char)(((pf << 4) & 0x10) | (sp & 0x1));
    modesCmdBlk[4] = (unsigned char)(param_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (param_len > 0xff) {
        fprintf(sg_warnings_strm, "mode select (6): param_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_strm, "    mode select (6) cdb: ");
        for (k = 0; k < MODE_SELECT6_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if (verbose > 1) {
        fprintf(sg_warnings_strm, "    mode select (6) parameter list\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "mode select (6): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, modesCmdBlk, sizeof(modesCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "mode select (6)", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI MODE SELECT (10) command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, SG_LIB_CAT_UNIT_ATTENTION,
 * -1 -> other failure */
int sg_ll_mode_select10(int sg_fd, int pf, int sp, void * paramp,
                       int param_len, int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char modesCmdBlk[MODE_SELECT10_CMDLEN] = 
        {MODE_SELECT10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    modesCmdBlk[1] = (unsigned char)(((pf << 4) & 0x10) | (sp & 0x1));
    modesCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    modesCmdBlk[8] = (unsigned char)(param_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (param_len > 0xffff) {
        fprintf(sg_warnings_strm, "mode select (10): param_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_strm, "    mode select (10) cdb: ");
        for (k = 0; k < MODE_SELECT10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if (verbose > 1) {
        fprintf(sg_warnings_strm, "    mode select (10) parameter list\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "mode select (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, modesCmdBlk, sizeof(modesCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "mode select (10)", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* MODE SENSE commands yield a response that has block descriptors followed
 * by mode pages. In most cases users are interested in the first mode page.
 * This function returns the (byte) offset of the start of the first mode
 * page. Set mode_sense_6 to 1 for MODE SENSE (6) and 0 for MODE SENSE (10).
 * Returns >= 0 is successful or -1 if failure. If there is a failure
 * a message is written to err_buff. */
int sg_mode_page_offset(const unsigned char * resp, int resp_len,
                        int mode_sense_6, char * err_buff, int err_buff_len)
{
    int bd_len;
    int calc_len;
    int offset;

    if ((NULL == resp) || (resp_len < 4) ||
        ((! mode_sense_6) && (resp_len < 8))) {
        snprintf(err_buff, err_buff_len, "given response length too short: "
                 "%d\n", resp_len);
        return -1;
    }
    if (mode_sense_6) {
        calc_len = resp[0] + 1;
        bd_len = resp[3];
        offset = bd_len + MODE6_RESP_HDR_LEN;
    } else {
        calc_len = (resp[0] << 8) + resp[1] + 2;
        bd_len = (resp[6] << 8) + resp[7];
        /* LongLBA doesn't change this calculation */
        offset = bd_len + MODE10_RESP_HDR_LEN;
    }
    if ((offset + 2) > resp_len) {
        snprintf(err_buff, err_buff_len, "given response length "
                 "too small, offset=%d given_len=%d bd_len=%d\n",
                 offset, resp_len, bd_len);
         offset = -1;
    } else if ((offset + 2) > calc_len) {
        snprintf(err_buff, err_buff_len, "calculated response "
                 "length too small, offset=%d calc_len=%d bd_len=%d\n",
                 offset, calc_len, bd_len);
        offset = -1;
    }
    return offset;
}

/* Fetches current, changeable, default and/or saveable modes pages as
 * indicated by pcontrol_arr for given pg_code and sub_pg_code. If
 * mode6==0 then use MODE SENSE (10) else use MODE SENSE (6). If
 * flexible set and mode data length seems wrong then try and
 * fix (compensating hack for bad device or driver). pcontrol_arr
 * should have 4 elements for output of current, changeable, default
 * and saved values respectively. Each element should be NULL or
 * at least mx_mpage_len bytes long.
 * Return of 0 -> overall success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_MALFORMED -> bad response, -1 -> other failure.
 * If success_mask pointer is not NULL then first zeros it. Then set bits
 * 0, 1, 2 and/or 3 if the current, changeable, default and saved values
 * respectively have been fetched. If error on current page
 * then stops and returns that error; otherwise continues if an error is
 * detected but returns the first error encountered.  */
int sg_get_mode_page_controls(int sg_fd, int mode6, int pg_code,
                              int sub_pg_code, int dbd, int flexible,
                              int mx_mpage_len, int * success_mask,
                              void * pcontrol_arr[], int * reported_len,
                              int verbose)
{
    int k, n, res, offset, calc_len, xfer_len, resp_mode6;
    unsigned char buff[MODE_RESP_ARB_LEN];
    char ebuff[EBUFF_SZ];
    int first_err = 0;

    if (success_mask)
        *success_mask = 0;
    if (reported_len)
        *reported_len = 0;
    if (mx_mpage_len < 4)
        return 0;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    memset(ebuff, 0, sizeof(ebuff));
    /* first try to find length of current page response */
    memset(buff, 0, MODE10_RESP_HDR_LEN);
    if (mode6)  /* want first 8 bytes just in case */
        res = sg_ll_mode_sense6(sg_fd, dbd, 0 /* pc */, pg_code,
                                sub_pg_code, buff, MODE10_RESP_HDR_LEN, 0,
                                verbose);
    else
        res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, dbd,
                                 0 /* pc */, pg_code, sub_pg_code, buff,
                                 MODE10_RESP_HDR_LEN, 0, verbose);
    if (0 != res)
        return res;
    n = buff[0];
    if (reported_len)
        *reported_len = mode6 ? (n + 1) : ((n << 8) + buff[1] + 2);
    resp_mode6 = mode6;
    if (flexible) {
        if (mode6 && (n < 3)) {
            resp_mode6 = 0;
            if (verbose)
                fprintf(sg_warnings_strm, ">>> msense(6) but resp[0]=%d so "
                        "try msense(10) response processing\n", n);
        }
        if ((0 == mode6) && (n > 5)) {
            if ((n > 11) && (0 == (n % 2)) && (0 == buff[4]) &&
                (0 == buff[5]) && (0 == buff[6])) {
                buff[1] = n;
                buff[0] = 0;
                if (verbose)
                    fprintf(sg_warnings_strm, ">>> msense(10) but resp[0]=%d "
                            "and not msense(6) response so fix length\n", n);
            } else
                resp_mode6 = 1;
        }
    }
    if (verbose && (resp_mode6 != mode6))
        fprintf(sg_warnings_strm, ">>> msense(%d) but resp[0]=%d "
                "so switch response processing\n", (mode6 ? 6 : 10),
                buff[0]);
    calc_len = resp_mode6 ? (buff[0] + 1) : ((buff[0] << 8) + buff[1] + 2);
    if (calc_len > MODE_RESP_ARB_LEN)
        calc_len = MODE_RESP_ARB_LEN;
    offset = sg_mode_page_offset(buff, calc_len, resp_mode6,
                                 ebuff, EBUFF_SZ);
    if (offset < 0) {
        if (('\0' != ebuff[0]) && (verbose > 0))
            fprintf(sg_warnings_strm, "sg_get_mode_page_controls: %s\n",
                    ebuff);
        return SG_LIB_CAT_MALFORMED;
    }
    xfer_len = calc_len - offset;
    if (xfer_len > mx_mpage_len)
        xfer_len = mx_mpage_len;

    for (k = 0; k < 4; ++k) {
        if (NULL == pcontrol_arr[k])
            continue;
        memset(pcontrol_arr[k], 0, mx_mpage_len);
        if (mode6)
            res = sg_ll_mode_sense6(sg_fd, dbd, k /* pc */,
                                    pg_code, sub_pg_code, buff,
                                    calc_len, 0, verbose);
        else
            res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, dbd,
                                     k /* pc */, pg_code, sub_pg_code,
                                     buff, calc_len, 0, verbose);
        if (0 != res) {
            if (0 == first_err)
                first_err = res;
            if (0 == k)
                break;  /* if problem on current page, it won't improve */
            else
                continue;
        }
        if (xfer_len > 0)
            memcpy(pcontrol_arr[k], buff + offset, xfer_len);
        if (success_mask)
            *success_mask |= (1 << k);
    }
    return first_err;
}

/* Invokes a SCSI REQUEST SENSE command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Request Sense not supported??,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ABORTED_COMMAND, -1 -> other failure */
int sg_ll_request_sense(int sg_fd, int desc, void * resp, int mx_resp_len,
                        int noisy, int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rsCmdBlk[REQUEST_SENSE_CMDLEN] = 
        {REQUEST_SENSE_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (desc)
        rsCmdBlk[1] |= 0x1;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (mx_resp_len > 0xfc) {
        fprintf(sg_warnings_strm, "SPC-3 says request sense allocation "
                "length should be <= 252\n");
        return -1;
    }
    rsCmdBlk[4] = mx_resp_len & 0xff;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Request Sense cmd: ");
        for (k = 0; k < REQUEST_SENSE_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rsCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "request sense: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rsCmdBlk, sizeof(rsCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "request sense", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_NOT_READY:      /* shouldn't happen ?? */
        default:
            ret = -1;
            break;
        }
    } else {
        if ((mx_resp_len >= 8) && (ret < 8)) {
            if (verbose)
                fprintf(sg_warnings_strm, "    request sense: got %d "
                        "bytes in response, too short\n", ret);
            ret = -1;
        } else
            ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI REPORT LUNS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Luns not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_NOT_READY (shouldn't happen), -1 -> other failure */
int sg_ll_report_luns(int sg_fd, int select_report, void * resp,
                      int mx_resp_len, int noisy, int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rlCmdBlk[REPORT_LUNS_CMDLEN] =
                         {REPORT_LUNS_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rlCmdBlk[2] = select_report & 0xff;
    rlCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rlCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rlCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rlCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    report luns cdb: ");
        for (k = 0; k < REPORT_LUNS_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rlCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "report luns: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rlCmdBlk, sizeof(rlCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "report luns", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
        case SG_LIB_CAT_NOT_READY:      /* shouldn't happen ?? */
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI LOG SENSE command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Log Sense not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_log_sense(int sg_fd, int ppc, int sp, int pc, int pg_code, 
                    int subpg_code, int paramp, unsigned char * resp,
                    int mx_resp_len, int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char logsCmdBlk[LOG_SENSE_CMDLEN] = 
        {LOG_SENSE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_strm, "mx_resp_len too big\n");
        return -1;
    }
    logsCmdBlk[1] = (unsigned char)((ppc ? 2 : 0) | (sp ? 1 : 0));
    logsCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    logsCmdBlk[3] = (unsigned char)(subpg_code & 0xff);
    logsCmdBlk[5] = (unsigned char)((paramp >> 8) & 0xff);
    logsCmdBlk[6] = (unsigned char)(paramp & 0xff);
    logsCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    logsCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (verbose) {
        fprintf(sg_warnings_strm, "    log sense cdb: ");
        for (k = 0; k < LOG_SENSE_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", logsCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "log sense: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, logsCmdBlk, sizeof(logsCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "log sense", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI LOG SELECT command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Log Select not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_log_select(int sg_fd, int pcr, int sp, int pc,
                     int pg_code, int subpg_code,
                     unsigned char * paramp, int param_len, 
                     int noisy, int verbose)
{
    int res, ret, k, sense_cat;
    unsigned char logsCmdBlk[LOG_SELECT_CMDLEN] = 
        {LOG_SELECT_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (param_len > 0xffff) {
        fprintf(sg_warnings_strm, "log select: param_len too big\n");
        return -1;
    }
    logsCmdBlk[1] = (unsigned char)((pcr ? 2 : 0) | (sp ? 1 : 0));
    logsCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    logsCmdBlk[3] = (unsigned char)(subpg_code & 0xff);
    logsCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    logsCmdBlk[8] = (unsigned char)(param_len & 0xff);
    if (verbose) {
        fprintf(sg_warnings_strm, "    log select cdb: ");
        for (k = 0; k < LOG_SELECT_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", logsCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if ((verbose > 1) && (param_len > 0)) {
        fprintf(sg_warnings_strm, "    log select parameter list\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "log select: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, logsCmdBlk, sizeof(logsCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "log select", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI START STOP UNIT command (MMC + SBC).
 * Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Start stop unit not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_start_stop_unit(int sg_fd, int immed, int fl_num, int power_cond,
                          int fl, int loej, int start, int noisy, int verbose)
{
    unsigned char ssuBlk[START_STOP_CMDLEN] = {START_STOP_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    int k, res, ret, sense_cat;
    struct sg_pt_base * ptvp;

    ssuBlk[1] = immed & 1;
    ssuBlk[3] = fl_num & 3;
    ssuBlk[4] = ((power_cond & 0xf) << 4) | (fl ? 0x4 : 0) |
                 (loej ? 0x2 : 0) | (start ? 0x1 : 0);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Start stop unit command:");
        for (k = 0; k < (int)sizeof(ssuBlk); ++k)
                fprintf (sg_warnings_strm, " %02x", ssuBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "start stop unit: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, ssuBlk, sizeof(ssuBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    res = do_scsi_pt(ptvp, sg_fd, START_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "start stop unit", res, 0,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
            ret = 0;
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI PREVENT ALLOW MEDIUM REMOVAL command (SPC-3)
 * prevent==0 allows removal, prevent==1 prevents removal ...
 * Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> command not supported
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_prevent_allow(int sg_fd, int prevent, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char pCmdBlk[PREVENT_ALLOW_CMDLEN] = 
                {PREVENT_ALLOW_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if ((prevent < 0) || (prevent > 3)) {
        fprintf(sg_warnings_strm, "prevent argument should be 0, 1, 2 or 3\n");
        return -1;
    }
    pCmdBlk[4] |= (prevent & 0x3);
    if (verbose) {
        fprintf(sg_warnings_strm, "    Prevent allow medium removal cdb: ");
        for (k = 0; k < PREVENT_ALLOW_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", pCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "prevent allow medium removal: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, pCmdBlk, sizeof(pCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "prevent allow medium removal", res, 0,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
            ret = 0;
    destruct_scsi_pt_obj(ptvp);
    return ret;
}
