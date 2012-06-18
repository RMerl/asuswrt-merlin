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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_cmds_extra.h"
#include "sg_pt.h"



#define SENSE_BUFF_LEN 32       /* Arbitrary, could be larger */

#define DEF_PT_TIMEOUT 60       /* 60 seconds */
#define LONG_PT_TIMEOUT 7200    /* 7,200 seconds == 120 minutes */

#define SERVICE_ACTION_IN_16_CMD 0x9e
#define SERVICE_ACTION_IN_16_CMDLEN 16
#define SERVICE_ACTION_OUT_16_CMD 0x9f
#define SERVICE_ACTION_OUT_16_CMDLEN 16
#define READ_LONG_16_SA 0x11
#define WRITE_LONG_16_SA 0x11
#define MAINTENANCE_IN_CMD 0xa3
#define MAINTENANCE_IN_CMDLEN 12
#define REPORT_TGT_PRT_GRP_SA 0xa
#define REPORT_IDENTIFYING_INFORMATION_SA 0x5
#define MAINTENANCE_OUT_CMD 0xa4
#define MAINTENANCE_OUT_CMDLEN 12
#define SET_IDENTIFYING_INFORMATION_SA 0x6
#define SET_TGT_PRT_GRP_SA 0xa
#define SEND_DIAGNOSTIC_CMD   0x1d
#define SEND_DIAGNOSTIC_CMDLEN  6
#define RECEIVE_DIAGNOSTICS_CMD   0x1c
#define RECEIVE_DIAGNOSTICS_CMDLEN  6
#define READ_DEFECT10_CMD     0x37
#define READ_DEFECT10_CMDLEN    10
#define SERVICE_ACTION_IN_12_CMD 0xab
#define SERVICE_ACTION_IN_12_CMDLEN 12
#define READ_MEDIA_SERIAL_NUM_SA 0x1
#define FORMAT_UNIT_CMD 0x4
#define FORMAT_UNIT_CMDLEN 6
#define REASSIGN_BLKS_CMD     0x7
#define REASSIGN_BLKS_CMDLEN  6
#define GET_CONFIG_CMD 0x46
#define GET_CONFIG_CMD_LEN 10
#define PERSISTENT_RESERVE_IN_CMD 0x5e
#define PERSISTENT_RESERVE_IN_CMDLEN 10
#define PERSISTENT_RESERVE_OUT_CMD 0x5f
#define PERSISTENT_RESERVE_OUT_CMDLEN 10
#define READ_LONG10_CMD 0x3e
#define READ_LONG10_CMDLEN 10
#define WRITE_LONG10_CMD 0x3f
#define WRITE_LONG10_CMDLEN 10
#define VERIFY10_CMD 0x2f
#define VERIFY10_CMDLEN 10
#define ATA_PT_12_CMD 0xa1
#define ATA_PT_12_CMDLEN 12
#define ATA_PT_16_CMD 0x85
#define ATA_PT_16_CMDLEN 16
#define READ_BUFFER_CMD 0x3c
#define READ_BUFFER_CMDLEN 10
#define WRITE_BUFFER_CMD 0x3b
#define WRITE_BUFFER_CMDLEN 10



/* Invokes a SCSI REPORT TARGET PORT GROUPS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Target Port Groups not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_UNIT_ATTENTION, -1 -> other failure */
int sg_ll_report_tgt_prt_grp(int sg_fd, void * resp,
                             int mx_resp_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char rtpgCmdBlk[MAINTENANCE_IN_CMDLEN] =
                         {MAINTENANCE_IN_CMD, REPORT_TGT_PRT_GRP_SA,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rtpgCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rtpgCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rtpgCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rtpgCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    report target port groups cdb: ");
        for (k = 0; k < MAINTENANCE_IN_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rtpgCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "report target port groups: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rtpgCmdBlk, sizeof(rtpgCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "report target port group", res,
                               mx_resp_len, sense_b, noisy, verbose,
                               &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
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

/* Invokes a SCSI SET TARGET PORT GROUPS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Set Target Port Groups not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_UNIT_ATTENTION, -1 -> other failure */
int sg_ll_set_tgt_prt_grp(int sg_fd, void * paramp,
			  int param_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char stpgCmdBlk[MAINTENANCE_OUT_CMDLEN] =
                         {MAINTENANCE_OUT_CMD, SET_TGT_PRT_GRP_SA,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    stpgCmdBlk[6] = (param_len >> 24) & 0xff;
    stpgCmdBlk[7] = (param_len >> 16) & 0xff;
    stpgCmdBlk[8] = (param_len >> 8) & 0xff;
    stpgCmdBlk[9] = param_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    set target port groups cdb: ");
        for (k = 0; k < MAINTENANCE_OUT_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", stpgCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
        if ((verbose > 1) && paramp && param_len) {
            fprintf(sg_warnings_strm, "    set target port groups "
                    "parameter list:\n");
            dStrHex((const char *)paramp, param_len, -1);
        }
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "set target port groups: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, stpgCmdBlk, sizeof(stpgCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "set target port group", res, 0,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
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

/* Invokes a SCSI SEND DIAGNOSTIC command. Foreground, extended self tests can
 * take a long time, if so set long_duration flag. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Send diagnostic not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_send_diag(int sg_fd, int sf_code, int pf_bit, int sf_bit,
                    int devofl_bit, int unitofl_bit, int long_duration,
                    void * paramp, int param_len, int noisy,
                    int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char senddiagCmdBlk[SEND_DIAGNOSTIC_CMDLEN] = 
        {SEND_DIAGNOSTIC_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    senddiagCmdBlk[1] = (unsigned char)((sf_code << 5) | (pf_bit << 4) |
                        (sf_bit << 2) | (devofl_bit << 1) | unitofl_bit);
    senddiagCmdBlk[3] = (unsigned char)((param_len >> 8) & 0xff);
    senddiagCmdBlk[4] = (unsigned char)(param_len & 0xff);

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Send diagnostic cmd: ");
        for (k = 0; k < SEND_DIAGNOSTIC_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", senddiagCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
        if ((verbose > 1) && paramp && param_len) {
            fprintf(sg_warnings_strm, "    Send diagnostic parameter "
                    "list:\n");
            dStrHex((const char *)paramp, param_len, -1);
        }
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "send diagnostic: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, senddiagCmdBlk, sizeof(senddiagCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, 
                     (long_duration ? LONG_PT_TIMEOUT : DEF_PT_TIMEOUT),
                     verbose);
    ret = sg_cmds_process_resp(ptvp, "send diagnostic", res, 0, sense_b,
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

/* Invokes a SCSI RECEIVE DIAGNOSTIC RESULTS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Receive diagnostic results not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_receive_diag(int sg_fd, int pcv, int pg_code, void * resp, 
                       int mx_resp_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char rcvdiagCmdBlk[RECEIVE_DIAGNOSTICS_CMDLEN] = 
        {RECEIVE_DIAGNOSTICS_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rcvdiagCmdBlk[1] = (unsigned char)(pcv ? 0x1 : 0);
    rcvdiagCmdBlk[2] = (unsigned char)(pg_code);
    rcvdiagCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rcvdiagCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Receive diagnostic results cmd: ");
        for (k = 0; k < RECEIVE_DIAGNOSTICS_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rcvdiagCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "receive diagnostic results: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rcvdiagCmdBlk, sizeof(rcvdiagCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "receive diagnostic results", res,
                               mx_resp_len, sense_b, noisy, verbose,
                               &sense_cat);
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

/* Invokes a SCSI READ DEFECT DATA (10) command (SBC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_read_defect10(int sg_fd, int req_plist, int req_glist,
                        int dl_format, void * resp, int mx_resp_len,
                        int noisy, int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char rdefCmdBlk[READ_DEFECT10_CMDLEN] = 
        {READ_DEFECT10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rdefCmdBlk[2] = (unsigned char)(((req_plist << 4) & 0x10) |
                         ((req_glist << 3) & 0x8) | (dl_format & 0x7));
    rdefCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rdefCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_strm, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_strm, "    read defect (10) cdb: ");
        for (k = 0; k < READ_DEFECT10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rdefCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read defect (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rdefCmdBlk, sizeof(rdefCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read defect (10)", res, mx_resp_len,
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
            fprintf(sg_warnings_strm, "    read defect (10): response%s\n",
                    (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI READ MEDIA SERIAL NUMBER command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Read media serial number not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_read_media_serial_num(int sg_fd, void * resp, int mx_resp_len,
                                int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char rmsnCmdBlk[SERVICE_ACTION_IN_12_CMDLEN] =
                         {SERVICE_ACTION_IN_12_CMD, READ_MEDIA_SERIAL_NUM_SA,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rmsnCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rmsnCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rmsnCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rmsnCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    read media serial number cdb: ");
        for (k = 0; k < SERVICE_ACTION_IN_12_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rmsnCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read media serial number: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rmsnCmdBlk, sizeof(rmsnCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read media serial number", res,
                               mx_resp_len, sense_b, noisy, verbose,
                               &sense_cat);
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
            fprintf(sg_warnings_strm, "    read media serial number: respon"
                    "se%s\n", (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI REPORT IDENTIFYING INFORMATION command. This command was
 * called REPORT DEVICE IDENTIFIER prior to spc4r07. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report identifying information not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_report_id_info(int sg_fd, int itype, void * resp,
                         int max_resp_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char riiCmdBlk[MAINTENANCE_IN_CMDLEN] = {MAINTENANCE_IN_CMD,
                        REPORT_IDENTIFYING_INFORMATION_SA,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    riiCmdBlk[6] = (max_resp_len >> 24) & 0xff;
    riiCmdBlk[7] = (max_resp_len >> 16) & 0xff;
    riiCmdBlk[8] = (max_resp_len >> 8) & 0xff;
    riiCmdBlk[9] = max_resp_len & 0xff;
    riiCmdBlk[10] |= (itype << 1) & 0xfe;
    
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Report identifying information cdb: ");
        for (k = 0; k < MAINTENANCE_IN_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", riiCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "report identifying information: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, riiCmdBlk, sizeof(riiCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, max_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "report identifying information", res,
                               max_resp_len, sense_b, noisy, verbose,
                               &sense_cat);
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
            fprintf(sg_warnings_strm, "    report identifying information: "
                    "response%s\n", (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI SET IDENTIFYING INFORMATION command. This command was
 * called SET DEVICE IDENTIFIER prior to spc4r07. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Set identifying information not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_set_id_info(int sg_fd, int itype, void * paramp,
                      int param_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char siiCmdBlk[MAINTENANCE_OUT_CMDLEN] = {MAINTENANCE_OUT_CMD,
                         SET_IDENTIFYING_INFORMATION_SA,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    siiCmdBlk[6] = (param_len >> 24) & 0xff;
    siiCmdBlk[7] = (param_len >> 16) & 0xff;
    siiCmdBlk[8] = (param_len >> 8) & 0xff;
    siiCmdBlk[9] = param_len & 0xff;
    siiCmdBlk[10] |= (itype << 1) & 0xfe;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Set identifying information cdb: ");
        for (k = 0; k < MAINTENANCE_OUT_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", siiCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
        if ((verbose > 1) && paramp && param_len) {
            fprintf(sg_warnings_strm, "    Set identifying information "
                    "parameter list:\n");
            dStrHex((const char *)paramp, param_len, -1);
        }
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "Set identifying information: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, siiCmdBlk, sizeof(siiCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "set identifying information", res, 0,
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

/* Invokes a FORMAT UNIT (SBC-3) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Format unit not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_format_unit(int sg_fd, int fmtpinfo, int rto_req, int longlist,
                      int fmtdata, int cmplst, int dlist_format,
                      int timeout_secs, void * paramp, int param_len,
                      int noisy, int verbose)
{
    int k, res, ret, sense_cat, tmout;
    unsigned char fuCmdBlk[FORMAT_UNIT_CMDLEN] = 
                {FORMAT_UNIT_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (fmtpinfo)
        fuCmdBlk[1] |= 0x80;
    if (rto_req)
        fuCmdBlk[1] |= 0x40;
    if (longlist)
        fuCmdBlk[1] |= 0x20;
    if (fmtdata)
        fuCmdBlk[1] |= 0x10;
    if (cmplst)
        fuCmdBlk[1] |= 0x8;
    if (dlist_format)
        fuCmdBlk[1] |= (dlist_format & 0x7);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    tmout = (timeout_secs > 0) ? timeout_secs : DEF_PT_TIMEOUT;
    if (verbose) {
        fprintf(sg_warnings_strm, "    format cdb: ");
        for (k = 0; k < 6; ++k)
            fprintf(sg_warnings_strm, "%02x ", fuCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if ((verbose > 1) && (param_len > 0)) {
        fprintf(sg_warnings_strm, "    format parameter list:\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "format unit: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, fuCmdBlk, sizeof(fuCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, tmout, verbose);
    ret = sg_cmds_process_resp(ptvp, "format unit", res, 0, sense_b,
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

/* Invokes a SCSI REASSIGN BLOCKS command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_NOT_READY -> device not ready, -1 -> other failure */
int sg_ll_reassign_blocks(int sg_fd, int longlba, int longlist,
                          void * paramp, int param_len, int noisy,
                          int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char reassCmdBlk[REASSIGN_BLKS_CMDLEN] = 
        {REASSIGN_BLKS_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    reassCmdBlk[1] = (unsigned char)(((longlba << 1) & 0x2) | (longlist & 0x1));
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    reassign blocks cdb: ");
        for (k = 0; k < REASSIGN_BLKS_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", reassCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    if (verbose > 1) {
        fprintf(sg_warnings_strm, "    reassign blocks parameter list\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "reassign blocks: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, reassCmdBlk, sizeof(reassCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "reassign blocks", res, 0, sense_b,
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

/* Invokes a SCSI GET CONFIGURATION command (MMC-3,4,5).
 * Returns 0 when successful, SG_LIB_CAT_INVALID_OP if command not
 * supported, SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
int sg_ll_get_config(int sg_fd, int rt, int starting, void * resp,
                     int mx_resp_len, int noisy, int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char gcCmdBlk[GET_CONFIG_CMD_LEN] = {GET_CONFIG_CMD, 0, 0, 0, 
                                                  0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if ((rt < 0) || (rt > 3)) {
        fprintf(sg_warnings_strm, "Bad rt value: %d\n", rt);
        return -1;
    }
    gcCmdBlk[1] = (rt & 0x3);
    if ((starting < 0) || (starting > 0xffff)) {
        fprintf(sg_warnings_strm, "Bad starting field number: 0x%x\n",
                starting);
        return -1;
    }
    gcCmdBlk[2] = (unsigned char)((starting >> 8) & 0xff);
    gcCmdBlk[3] = (unsigned char)(starting & 0xff);
    if ((mx_resp_len < 0) || (mx_resp_len > 0xffff)) {
        fprintf(sg_warnings_strm, "Bad mx_resp_len: 0x%x\n", starting);
        return -1;
    }
    gcCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    gcCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);

    if (verbose) {
        fprintf(sg_warnings_strm, "    Get Configuration cdb: ");
        for (k = 0; k < GET_CONFIG_CMD_LEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", gcCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "get configuration: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, gcCmdBlk, sizeof(gcCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "get configuration", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
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
            fprintf(sg_warnings_strm, "    get configuration: response%s\n",
                    (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI PERSISTENT RESERVE IN command (SPC). Returns 0
 * when successful, SG_LIB_CAT_INVALID_OP if command not supported,
 * SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
int sg_ll_persistent_reserve_in(int sg_fd, int rq_servact, void * resp,
                                int mx_resp_len, int noisy, int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char prinCmdBlk[PERSISTENT_RESERVE_IN_CMDLEN] =
                 {PERSISTENT_RESERVE_IN_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (rq_servact > 0)
        prinCmdBlk[1] = (unsigned char)(rq_servact & 0x1f);
    prinCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    prinCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Persistent Reservation In cmd: ");
        for (k = 0; k < PERSISTENT_RESERVE_IN_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", prinCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "persistent reservation in: out of "
                "memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, prinCmdBlk, sizeof(prinCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "persistent reservation in", res, mx_resp_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
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
            fprintf(sg_warnings_strm, "    persistent reserve in: "
                    "response%s\n", (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI PERSISTENT RESERVE OUT command (SPC). Returns 0
 * when successful, SG_LIB_CAT_INVALID_OP if command not supported,
 * SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
int sg_ll_persistent_reserve_out(int sg_fd, int rq_servact, int rq_scope,
                                 unsigned int rq_type, void * paramp,
                                 int param_len, int noisy, int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char proutCmdBlk[PERSISTENT_RESERVE_OUT_CMDLEN] =
                 {PERSISTENT_RESERVE_OUT_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (rq_servact > 0)
        proutCmdBlk[1] = (unsigned char)(rq_servact & 0x1f);
    proutCmdBlk[2] = (((rq_scope & 0xf) << 4) | (rq_type & 0xf));
    proutCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    proutCmdBlk[8] = (unsigned char)(param_len & 0xff);

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Persistent Reservation Out cmd: ");
        for (k = 0; k < PERSISTENT_RESERVE_OUT_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", proutCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
        if (verbose > 1) {
            fprintf(sg_warnings_strm, "    Persistent Reservation Out parameters:\n");
            dStrHex((const char *)paramp, param_len, 0);
        }
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "persistent reserve out: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, proutCmdBlk, sizeof(proutCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "persistent reserve out", res, 0,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
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

static int has_blk_ili(unsigned char * sensep, int sb_len)
{
    int resp_code;
    const unsigned char * cup;

    if (sb_len < 8)
        return 0;
    resp_code = (0x7f & sensep[0]);
    if (resp_code >= 0x72) { /* descriptor format */
        /* find block command descriptor */
        if ((cup = sg_scsi_sense_desc_find(sensep, sb_len, 0x5)))
            return ((cup[3] & 0x20) ? 1 : 0);
    } else /* fixed */
        return ((sensep[2] & 0x20) ? 1 : 0);
    return 0;
}

/* Invokes a SCSI READ LONG (10) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> READ LONG(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_NOT_READY -> device not ready, -1 -> other failure */
int sg_ll_read_long10(int sg_fd, int pblock, int correct, unsigned long lba,
                      void * resp, int xfer_len, int * offsetp,
                      int noisy, int verbose)
{
    int k, res, sense_cat, ret;
    unsigned char readLongCmdBlk[READ_LONG10_CMDLEN];
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    memset(readLongCmdBlk, 0, READ_LONG10_CMDLEN);
    readLongCmdBlk[0] = READ_LONG10_CMD;
    if (pblock)
        readLongCmdBlk[1] |= 0x4;
    if (correct)
        readLongCmdBlk[1] |= 0x2;

    readLongCmdBlk[2] = (lba >> 24) & 0xff;
    readLongCmdBlk[3] = (lba >> 16) & 0xff;
    readLongCmdBlk[4] = (lba >> 8) & 0xff;
    readLongCmdBlk[5] = lba & 0xff;
    readLongCmdBlk[7] = (xfer_len >> 8) & 0xff;
    readLongCmdBlk[8] = xfer_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Read Long (10) cmd: ");
        for (k = 0; k < READ_LONG10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", readLongCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read long (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, readLongCmdBlk, sizeof(readLongCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, xfer_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read long (10)", res, xfer_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_ILLEGAL_REQ:
            {
                int valid, slen, ili;
                unsigned long long ull = 0;

                slen = get_scsi_pt_sense_len(ptvp);
                valid = sg_get_sense_info_fld(sense_b, slen, &ull);
                ili = has_blk_ili(sense_b, slen);
                if (valid && ili) {
                    if (offsetp)
                        *offsetp = (int)(long long)ull;
                    ret = SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO;
                } else {
                    if (verbose > 1)
                        fprintf(sg_warnings_strm, "  info field: 0x%" PRIx64
                                ",  valid: %d, ili: %d\n", ull, valid, ili);
                    ret = SG_LIB_CAT_ILLEGAL_REQ;
                }
            }
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

/* Invokes a SCSI READ LONG (16) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> READ LONG(16) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_NOT_READY -> device not ready, -1 -> other failure */
int sg_ll_read_long16(int sg_fd, int pblock, int correct,
                      unsigned long long llba, void * resp, int xfer_len,
                      int * offsetp, int noisy, int verbose)
{
    int k, res, sense_cat, ret;
    unsigned char readLongCmdBlk[SERVICE_ACTION_IN_16_CMDLEN];
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    memset(readLongCmdBlk, 0, sizeof(readLongCmdBlk));
    readLongCmdBlk[0] = SERVICE_ACTION_IN_16_CMD;
    readLongCmdBlk[1] = READ_LONG_16_SA;
    if (pblock)
        readLongCmdBlk[14] |= 0x2;
    if (correct)
        readLongCmdBlk[14] |= 0x1;

    readLongCmdBlk[2] = (llba >> 56) & 0xff;
    readLongCmdBlk[3] = (llba >> 48) & 0xff;
    readLongCmdBlk[4] = (llba >> 40) & 0xff;
    readLongCmdBlk[5] = (llba >> 32) & 0xff;
    readLongCmdBlk[6] = (llba >> 24) & 0xff;
    readLongCmdBlk[7] = (llba >> 16) & 0xff;
    readLongCmdBlk[8] = (llba >> 8) & 0xff;
    readLongCmdBlk[9] = llba & 0xff;
    readLongCmdBlk[12] = (xfer_len >> 8) & 0xff;
    readLongCmdBlk[13] = xfer_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Read Long (16) cmd: ");
        for (k = 0; k < SERVICE_ACTION_IN_16_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", readLongCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read long (16): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, readLongCmdBlk, sizeof(readLongCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, xfer_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read long (16)", res, xfer_len,
                               sense_b, noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_ILLEGAL_REQ:
            {
                int valid, slen, ili;
                unsigned long long ull = 0;

                slen = get_scsi_pt_sense_len(ptvp);
                valid = sg_get_sense_info_fld(sense_b, slen, &ull);
                ili = has_blk_ili(sense_b, slen);
                if (valid && ili) {
                    if (offsetp)
                        *offsetp = (int)(long long)ull;
                    ret = SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO;
                } else {
                    if (verbose > 1)
                        fprintf(sg_warnings_strm, "  info field: 0x%" PRIx64
                                ",  valid: %d, ili: %d\n", ull, valid, ili);
                    ret = SG_LIB_CAT_ILLEGAL_REQ;
                }
            }
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

/* Invokes a SCSI WRITE LONG (10) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> WRITE LONG(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, -1 -> other failure */
int sg_ll_write_long10(int sg_fd, int cor_dis, int wr_uncor, int pblock,
                       unsigned long lba, void * data_out, int xfer_len,
                       int * offsetp, int noisy, int verbose)
{
    int k, res, sense_cat, ret;
    unsigned char writeLongCmdBlk[WRITE_LONG10_CMDLEN];
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    memset(writeLongCmdBlk, 0, WRITE_LONG10_CMDLEN);
    writeLongCmdBlk[0] = WRITE_LONG10_CMD;
    if (cor_dis)
        writeLongCmdBlk[1] |= 0x80;
    if (wr_uncor)
        writeLongCmdBlk[1] |= 0x40;
    if (pblock)
        writeLongCmdBlk[1] |= 0x20;
  
    writeLongCmdBlk[2] = (lba >> 24) & 0xff;
    writeLongCmdBlk[3] = (lba >> 16) & 0xff;
    writeLongCmdBlk[4] = (lba >> 8) & 0xff;
    writeLongCmdBlk[5] = lba & 0xff;
    writeLongCmdBlk[7] = (xfer_len >> 8) & 0xff;
    writeLongCmdBlk[8] = xfer_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Write Long (10) cmd: ");
        for (k = 0; k < (int)sizeof(writeLongCmdBlk); ++k)
            fprintf(sg_warnings_strm, "%02x ", writeLongCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "write long(10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, writeLongCmdBlk, sizeof(writeLongCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)data_out, xfer_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "write long(10)", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_ILLEGAL_REQ:
            {
                int valid, slen, ili;
                unsigned long long ull = 0;

                slen = get_scsi_pt_sense_len(ptvp);
                valid = sg_get_sense_info_fld(sense_b, slen, &ull);
                ili = has_blk_ili(sense_b, slen);
                if (valid && ili) {
                    if (offsetp)
                        *offsetp = (int)(long long)ull;
                    ret = SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO;
                } else {
                    if (verbose > 1)
                        fprintf(sg_warnings_strm, "  info field: 0x%" PRIx64
                                ",  valid: %d, ili: %d\n", ull, valid, ili);
                    ret = SG_LIB_CAT_ILLEGAL_REQ;
                }
            }
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

/* Invokes a SCSI WRITE LONG (16) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> WRITE LONG(16) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready,
 * SG_LIB_CAT_ABORTED_COMMAND, -1 -> other failure */
int sg_ll_write_long16(int sg_fd, int cor_dis, int wr_uncor, int pblock,
                       unsigned long long llba, void * data_out,
                       int xfer_len, int * offsetp, int noisy, int verbose)
{
    int k, res, sense_cat, ret;
    unsigned char writeLongCmdBlk[SERVICE_ACTION_OUT_16_CMDLEN];
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    memset(writeLongCmdBlk, 0, sizeof(writeLongCmdBlk));
    writeLongCmdBlk[0] = SERVICE_ACTION_OUT_16_CMD;
    writeLongCmdBlk[1] = WRITE_LONG_16_SA;
    if (cor_dis)
        writeLongCmdBlk[1] |= 0x80;
    if (wr_uncor)
        writeLongCmdBlk[1] |= 0x40;
    if (pblock)
        writeLongCmdBlk[1] |= 0x20;

    writeLongCmdBlk[2] = (llba >> 56) & 0xff;
    writeLongCmdBlk[3] = (llba >> 48) & 0xff;
    writeLongCmdBlk[4] = (llba >> 40) & 0xff;
    writeLongCmdBlk[5] = (llba >> 32) & 0xff;
    writeLongCmdBlk[6] = (llba >> 24) & 0xff;
    writeLongCmdBlk[7] = (llba >> 16) & 0xff;
    writeLongCmdBlk[8] = (llba >> 8) & 0xff;
    writeLongCmdBlk[9] = llba & 0xff;
    writeLongCmdBlk[12] = (xfer_len >> 8) & 0xff;
    writeLongCmdBlk[13] = xfer_len & 0xff;
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Write Long (16) cmd: ");
        for (k = 0; k < SERVICE_ACTION_OUT_16_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", writeLongCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "write long(16): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, writeLongCmdBlk, sizeof(writeLongCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)data_out, xfer_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "write long(16)", res, 0, sense_b,
                               noisy, verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        case SG_LIB_CAT_ILLEGAL_REQ:
            {
                int valid, slen, ili;
                unsigned long long ull = 0;

                slen = get_scsi_pt_sense_len(ptvp);
                valid = sg_get_sense_info_fld(sense_b, slen, &ull);
                ili = has_blk_ili(sense_b, slen);
                if (valid && ili) {
                    if (offsetp)
                        *offsetp = (int)(long long)ull;
                    ret = SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO;
                } else {
                    if (verbose > 1)
                        fprintf(sg_warnings_strm, "  info field: 0x%" PRIx64
                                ",  valid: %d, ili: %d\n", ull, valid, ili);
                    ret = SG_LIB_CAT_ILLEGAL_REQ;
                }
            }
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

/* Invokes a SCSI VERIFY (10) command (SBC and MMC).
 * Note that 'veri_len' is in blocks while 'data_out_len' is in bytes.
 * Returns of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Verify(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_MEDIUM_HARD -> medium or hardware error, no valid info,
 * SG_LIB_CAT_MEDIUM_HARD_WITH_INFO -> as previous, with valid info,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_verify10(int sg_fd, int dpo, int bytechk, unsigned long lba,
                   int veri_len, void * data_out, int data_out_len,
                   unsigned long * infop, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char vCmdBlk[VERIFY10_CMDLEN] = 
                {VERIFY10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (dpo)
        vCmdBlk[1] |= 0x10;
    if (bytechk)
        vCmdBlk[1] |= 0x2;
    vCmdBlk[2] = (unsigned char)((lba >> 24) & 0xff);
    vCmdBlk[3] = (unsigned char)((lba >> 16) & 0xff);
    vCmdBlk[4] = (unsigned char)((lba >> 8) & 0xff);
    vCmdBlk[5] = (unsigned char)(lba & 0xff);
    vCmdBlk[7] = (unsigned char)((veri_len >> 8) & 0xff);
    vCmdBlk[8] = (unsigned char)(veri_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose > 1) {
        fprintf(sg_warnings_strm, "    Verify(10) cdb: ");
        for (k = 0; k < VERIFY10_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", vCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "verify (10): out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, vCmdBlk, sizeof(vCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    if (data_out_len > 0)
        set_scsi_pt_data_out(ptvp, (unsigned char *)data_out, data_out_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "verify (10)", res, 0, sense_b,
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
        case SG_LIB_CAT_MEDIUM_HARD:
            {
                int valid, slen;
                unsigned long long ull = 0;

                slen = get_scsi_pt_sense_len(ptvp);
                valid = sg_get_sense_info_fld(sense_b, slen, &ull);
                if (valid) {
                    if (infop)
                        *infop = (unsigned long)ull;
                    ret = SG_LIB_CAT_MEDIUM_HARD_WITH_INFO;
                } else
                    ret = SG_LIB_CAT_MEDIUM_HARD;
            }
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

/* Invokes a ATA PASS-THROUGH (12 or 16) SCSI command (SAT). If cdb_len
 * is 12 then a ATA PASS-THROUGH (12) command is called. If cdb_len is 16
 * then a ATA PASS-THROUGH (16) command is called. If cdb_len is any other
 * value -1 is returned. After copying from cdbp to an internal buffer,
 * the first byte (i.e. offset 0) is set to 0xa1 if cdb_len is 12; or is
 * set to 0x85 if cdb_len is 16. The last byte (offset 11 or offset 15) is
 * set to 0x0 in the internal buffer. If timeout_secs <= 0 then the timeout
 * is set to 60 seconds. For data in or out transfers set dinp or doutp,
 * and dlen to the number of bytes to transfer. If dlen is zero then no data
 * transfer is assumed. If sense buffer obtained then it is written to
 * sensep, else sensep[0] is set to 0x0. If ATA return descriptor is obtained
 * then written to ata_return_dp, else ata_return_dp[0] is set to 0x0. Either
 * sensep or ata_return_dp (or both) may be NULL pointers. Returns SCSI
 * status value (>= 0) or -1 if other error. Users are expected to check the
 * sense buffer themselves. If available the data in resid is written to
 * residp.
 */
int sg_ll_ata_pt(int sg_fd, const unsigned char * cdbp, int cdb_len,
                 int timeout_secs, void * dinp, void * doutp, int dlen,
                 unsigned char * sensep, int max_sense_len,
                 unsigned char * ata_return_dp, int max_ata_return_len,
                 int * residp, int verbose)
{
    int k, res, slen, duration;
    unsigned char aptCmdBlk[ATA_PT_16_CMDLEN] = 
                {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    unsigned char * sp;
    const unsigned char * ucp;
    struct sg_pt_base * ptvp;
    const char * cnamep;
    char b[256];
    int ret = -1;

    b[0] = '\0';
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    cnamep = (12 == cdb_len) ?
             "ATA pass through (12)" : "ATA pass through (16)";
    if ((NULL == cdbp) || ((12 != cdb_len) && (16 != cdb_len))) {
        if (verbose) {
            if (NULL == cdbp)
                fprintf(sg_warnings_strm, "%s NULL cdb pointer\n", cnamep);
            else
                fprintf(sg_warnings_strm, "cdb_len must be 12 or 16\n");
        }
        return -1;
    }
    aptCmdBlk[0] = (12 == cdb_len) ? ATA_PT_12_CMD : ATA_PT_16_CMD;
    if (sensep && (max_sense_len >= (int)sizeof(sense_b))) {
        sp = sensep;
        slen = max_sense_len;
    } else {
        sp = sense_b;
        slen = sizeof(sense_b);
    }
    if (12 == cdb_len)
        memcpy(aptCmdBlk + 1, cdbp + 1, ((cdb_len > 11) ? 10 : (cdb_len - 1)));
    else
        memcpy(aptCmdBlk + 1, cdbp + 1, ((cdb_len > 15) ? 14 : (cdb_len - 1)));
    if (verbose) {
        fprintf(sg_warnings_strm, "    %s cdb: ", cnamep);
        for (k = 0; k < cdb_len; ++k)
            fprintf(sg_warnings_strm, "%02x ", aptCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "%s: out of memory\n", cnamep);
        return -1;
    }
    set_scsi_pt_cdb(ptvp, aptCmdBlk, cdb_len);
    set_scsi_pt_sense(ptvp, sp, slen);
    if (dlen > 0) {
        if (dinp)
            set_scsi_pt_data_in(ptvp, (unsigned char *)dinp, dlen);
        else if (doutp)
            set_scsi_pt_data_out(ptvp, (unsigned char *)doutp, dlen);
    }
    res = do_scsi_pt(ptvp, sg_fd,
                     ((timeout_secs > 0) ? timeout_secs : DEF_PT_TIMEOUT),
                     verbose);
    if (SCSI_PT_DO_BAD_PARAMS == res) {
        if (verbose)
            fprintf(sg_warnings_strm, "%s: bad parameters\n", cnamep);
        goto out;
    } else if (SCSI_PT_DO_TIMEOUT == res) {
        if (verbose)
            fprintf(sg_warnings_strm, "%s: timeout\n", cnamep);
        goto out;
    } else if (res > 2) {
        if (verbose)
            fprintf(sg_warnings_strm, "%s: do_scsi_pt: errno=%d\n",
                    cnamep, -res);
    }

    if ((verbose > 2) && ((duration = get_scsi_pt_duration_ms(ptvp)) >= 0))
        fprintf(sg_warnings_strm, "      duration=%d ms\n", duration);

    switch (get_scsi_pt_result_category(ptvp)) {
    case SCSI_PT_RESULT_GOOD:
        if ((sensep) && (max_sense_len > 0))
            *sensep = 0;
        if ((ata_return_dp) && (max_ata_return_len > 0))
            *ata_return_dp = 0;
        if (residp && (dlen > 0))
            *residp = get_scsi_pt_resid(ptvp);
        ret = 0;
        break;
    case SCSI_PT_RESULT_STATUS: /* other than GOOD + CHECK CONDITION */
        if ((sensep) && (max_sense_len > 0))
            *sensep = 0;
        if ((ata_return_dp) && (max_ata_return_len > 0))
            *ata_return_dp = 0;
        ret = get_scsi_pt_status_response(ptvp);
        break;
    case SCSI_PT_RESULT_SENSE:
        if (sensep && (sp != sensep)) {
            k = get_scsi_pt_sense_len(ptvp);
            k = (k > max_sense_len) ? max_sense_len : k;
            memcpy(sensep, sp, k);
        }
        if (ata_return_dp && (max_ata_return_len > 0))  {
            /* search for ATA return descriptor */
            ucp = sg_scsi_sense_desc_find(sp, slen, 0x9);
            if (ucp) {
                k = ucp[1] + 2;
                k = (k > max_ata_return_len) ? max_ata_return_len : k;
                memcpy(ata_return_dp, ucp, k);
            } else
                ata_return_dp[0] = 0x0;
        }
        if (residp && (dlen > 0))
            *residp = get_scsi_pt_resid(ptvp);
        ret = get_scsi_pt_status_response(ptvp);
        break;
    case SCSI_PT_RESULT_TRANSPORT_ERR:
        if (verbose)
            fprintf(sg_warnings_strm, "%s: transport error: %s\n",
                    cnamep, get_scsi_pt_transport_err_str(ptvp, sizeof(b) ,
                                                          b));
        break;
    case SCSI_PT_RESULT_OS_ERR:
        if (verbose)
            fprintf(sg_warnings_strm, "%s: os error: %s\n",
                    cnamep, get_scsi_pt_os_err_str(ptvp, sizeof(b) , b));
        break;
    default:
        if (verbose)
            fprintf(sg_warnings_strm, "%s: unknown pt_result_category=%d\n",
                    cnamep, get_scsi_pt_result_category(ptvp));
        break;
    }

out:
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI READ BUFFER command (SPC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_read_buffer(int sg_fd, int mode, int buffer_id, int buffer_offset,
                      void * resp, int mx_resp_len, int noisy, int verbose)
{
    int res, k, ret, sense_cat;
    unsigned char rbufCmdBlk[READ_BUFFER_CMDLEN] = 
        {READ_BUFFER_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rbufCmdBlk[1] = (unsigned char)(mode & 0x1f);
    rbufCmdBlk[2] = (unsigned char)(buffer_id & 0xff);
    rbufCmdBlk[3] = (unsigned char)((buffer_offset >> 16) & 0xff);
    rbufCmdBlk[4] = (unsigned char)((buffer_offset >> 8) & 0xff);
    rbufCmdBlk[5] = (unsigned char)(buffer_offset & 0xff);
    rbufCmdBlk[6] = (unsigned char)((mx_resp_len >> 16) & 0xff);
    rbufCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rbufCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    read buffer cdb: ");
        for (k = 0; k < READ_BUFFER_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", rbufCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "read buffer: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rbufCmdBlk, sizeof(rbufCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "read buffer", res, mx_resp_len,
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
            fprintf(sg_warnings_strm, "    read buffer: response%s\n",
                    (ret > 256 ? ", first 256 bytes" : ""));
            dStrHex((const char *)resp, (ret > 256 ? 256 : ret), -1);
        }
        ret = 0;
    }
    destruct_scsi_pt_obj(ptvp);
    return ret;
}

/* Invokes a SCSI WRITE BUFFER command (SPC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
int sg_ll_write_buffer(int sg_fd, int mode, int buffer_id, int buffer_offset,
                       void * paramp, int param_len, int noisy, int verbose)
{
    int k, res, ret, sense_cat;
    unsigned char wbufCmdBlk[WRITE_BUFFER_CMDLEN] = 
        {WRITE_BUFFER_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    wbufCmdBlk[1] = (unsigned char)(mode & 0x1f);
    wbufCmdBlk[2] = (unsigned char)(buffer_id & 0xff);
    wbufCmdBlk[3] = (unsigned char)((buffer_offset >> 16) & 0xff);
    wbufCmdBlk[4] = (unsigned char)((buffer_offset >> 8) & 0xff);
    wbufCmdBlk[5] = (unsigned char)(buffer_offset & 0xff);
    wbufCmdBlk[6] = (unsigned char)((param_len >> 16) & 0xff);
    wbufCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    wbufCmdBlk[8] = (unsigned char)(param_len & 0xff);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    if (verbose) {
        fprintf(sg_warnings_strm, "    Write buffer cmd: ");
        for (k = 0; k < WRITE_BUFFER_CMDLEN; ++k)
            fprintf(sg_warnings_strm, "%02x ", wbufCmdBlk[k]);
        fprintf(sg_warnings_strm, "\n");
        if ((verbose > 1) && paramp && param_len) {
            fprintf(sg_warnings_strm, "    Write buffer parameter list%s:\n",
                    ((param_len > 256) ? " (first 256 bytes)" : ""));
            dStrHex((const char *)paramp,
                    ((param_len > 256) ? 256 : param_len), -1);
        }
    }

    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "write buffer: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, wbufCmdBlk, sizeof(wbufCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_out(ptvp, (unsigned char *)paramp, param_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_PT_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "write buffer", res, 0, sense_b,
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
