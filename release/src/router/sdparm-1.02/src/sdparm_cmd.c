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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_cmds_basic.h"

/* sdparm_cmd.c : contains code to implement commands
 * (i.e "--command=<cmd>") in sdparm.
 */


#define RCAP_REPLY_LEN 8
#define RCAP16_REPLY_LEN 32

static int
do_cmd_read_capacity(int sg_fd, int verbose)
{
    int res, k, do16;
    unsigned int last_blk_addr, block_size;
    unsigned char resp_buff[RCAP16_REPLY_LEN];
    unsigned long long llast_blk_addr;
    double sz_mib;

    do16 = 0;
    res = sg_ll_readcap_10(sg_fd, 0 /* pmi */, 0 /* lba */, resp_buff,
                           RCAP_REPLY_LEN, 1, verbose);
    if (0 == res) {
        last_blk_addr = ((resp_buff[0] << 24) | (resp_buff[1] << 16) |
                         (resp_buff[2] << 8) | resp_buff[3]);
        if (0xffffffff != last_blk_addr) {
            block_size = ((resp_buff[4] << 24) | (resp_buff[5] << 16) |
                         (resp_buff[6] << 8) | resp_buff[7]);
            printf("blocks: %u\n", last_blk_addr + 1);
            printf("block_length: %u\n", block_size);
            sz_mib = ((double)(last_blk_addr + 1) * block_size) / 
                      (double)(1048576);
#ifdef SG3_UTILS_MINGW
            printf("capacity_mib: %g\n", sz_mib);
#else
            printf("capacity_mib: %.1f\n", sz_mib);
#endif
        } else
            do16 = 1;
    } else
        return res;
    if (do16) {
        /* within SERVICE ACTION IN. May need RW or root permissions. */
        res = sg_ll_readcap_16(sg_fd, 0 /* pmi */, 0 /* llba */, resp_buff,
                               RCAP16_REPLY_LEN, 1, verbose);
        if (0 == res) {
            for (k = 0, llast_blk_addr = 0; k < 8; ++k) {
                llast_blk_addr <<= 8;
                llast_blk_addr |= resp_buff[k];
            }
            block_size = ((resp_buff[8] << 24) | (resp_buff[9] << 16) |
                          (resp_buff[10] << 8) | resp_buff[11]);
            printf("blocks: %" PRIu64 "\n", llast_blk_addr + 1);
            printf("block_length: %u\n", block_size);
            sz_mib = ((double)(llast_blk_addr + 1) * block_size) / 
                      (double)(1048576);
#ifdef SG3_UTILS_MINGW
            printf("capacity_mib: %g\n", sz_mib);
#else
            printf("capacity_mib: %.1f\n", sz_mib);
#endif
        } else
            return res;
    }
    return 0;
}

static int
do_cmd_sense(int sg_fd, int hex, int quiet, int verbose)
{
    int res, resp_len, sk, asc, ascq, progress, something;
    unsigned char buff[32];
    char b[128];

    memset(buff, 0, sizeof(buff));
    res = sg_ll_request_sense(sg_fd, 0 /* fixed format */,
                              buff, sizeof(buff), 1, verbose);
    if (0 == res) {
        resp_len = buff[7] + 8;
        if (resp_len > (int)sizeof(buff))
            resp_len = sizeof(buff);
        sk = (0xf & buff[2]);
        if (hex) {
            dStrHex((const char *)buff, resp_len, 1);
            return 0;
        }
        something = 0;
        if (verbose) {
            fprintf(stderr, "Decode response as sense data:\n");
            sg_print_sense(NULL, buff, resp_len, 0);
            if (verbose > 1) {
                fprintf(stderr, "\nOutput response in hex\n");
                dStrHex((const char *)buff, resp_len, 1);
            }
            something = 1;
        }
        asc = (resp_len > 12) ? buff[12] : 0;
        ascq = (resp_len > 13) ? buff[13] : 0;
        if (sg_get_sense_progress_fld(buff, resp_len, &progress)) {
            printf("Operation in progress, %d%% done\n",
                   progress * 100 / 65536);
            something = 1;
        }
        if (0 == sk) {  /* NO SENSE */
            /* check for hardware threshold exceeded or warning */
            if ((0xb == asc) || (0x5d == asc))
                printf("%s\n", sg_get_asc_ascq_str(asc, ascq,
                                                   (int)sizeof(b), b));
            /* check for low power conditions */
            if (0x5e == asc)
                printf("%s\n", sg_get_asc_ascq_str(asc, ascq,
                                                   (int)sizeof(b), b));
            return 0;
        } else {
            if (! (something || verbose || quiet)) {
                fprintf(stderr, "Decode response as sense data:\n");
                sg_print_sense(NULL, buff, resp_len, 0);
            }
            return 0;
        }
    } else if (SG_LIB_CAT_INVALID_OP == res)
        fprintf(stderr, "Request Sense command not supported\n");
    else if (SG_LIB_CAT_ILLEGAL_REQ == res)
        fprintf(stderr, "bad field in Request Sense cdb\n");
    else if (SG_LIB_CAT_NOT_READY == res)
        fprintf(stderr, "Request Sense failed, device not ready\n");
    else if (SG_LIB_CAT_ABORTED_COMMAND == res)
        fprintf(stderr, "Request Sense failed, aborted command\n");
    else {
        fprintf(stderr, "Request Sense command failed\n");
        if (0 == verbose)
            fprintf(stderr, "    try the '-v' option for "
                    "more information\n");
    }
    return res;
}

const struct sdparm_command *
sdp_build_cmd(const char * cmd_str, int * rwp)
{
    const struct sdparm_command * scmdp;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp) {
        if (0 == strcmp(scmdp->name, cmd_str))
            break;
    }
    if (scmdp->name) {
        if (rwp) {
            if ((CMD_READY  == scmdp->cmd_num) ||
                (CMD_SENSE  == scmdp->cmd_num) ||
                (CMD_CAPACITY  == scmdp->cmd_num))
                *rwp = 0;
            else
                *rwp = 1;
        }
        return scmdp;
    } else
        return NULL;
}

void
sdp_enumerate_commands()
{
    const struct sdparm_command * scmdp;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp)
        printf("  %s\n", scmdp->name);
}

/* Returns 0 if successful */
int
sdp_process_cmd(int sg_fd, const struct sdparm_command * scmdp, int pdt,
                const struct sdparm_opt_coll * opts)
{
    int res, progress;

    if (! (opts->flexible ||
          (CMD_READY == scmdp->cmd_num) ||
          (CMD_SENSE == scmdp->cmd_num) ||
          (0 == pdt) || (5 == pdt)) ) {
        fprintf(stderr, "this command only valid on a disk or cd/dvd; "
                "use '--flexible' to override\n");
        return SG_LIB_SYNTAX_ERROR;
    }
    switch (scmdp->cmd_num)
    {
    case CMD_CAPACITY:
        res = do_cmd_read_capacity(sg_fd, opts->verbose);
        break;
    case CMD_EJECT:
        res = sg_ll_start_stop_unit(sg_fd, 0 /* immed */, 0 /* fl_num */,
                                    0 /* power cond. */, 0 /* fl */,
                                    1 /*loej */, 0 /* start */, 1 /* noisy */,
                                    opts->verbose);
        break;
    case CMD_LOAD:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 1, 1, 1, opts->verbose);
        break;
    case CMD_READY:
        progress = -1;
        res = sg_ll_test_unit_ready_progress(sg_fd, 0, &progress, 0,
                                             opts->verbose);
        if (0 == res)
            printf("Ready\n");
        else {
            if (progress >= 0)
                printf("Not ready, progress indication: %d%% done\n",
                       (progress * 100) / 65536);
            else
                printf("Not ready\n");
        }
        break;
    case CMD_SENSE:
        res = do_cmd_sense(sg_fd, opts->hex, opts->quiet, opts->verbose);
        break;
    case CMD_START:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 0, 1, 1, opts->verbose);
        break;
    case CMD_STOP:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 0, 0, 1, opts->verbose);
        break;
    case CMD_SYNC:
        res = sg_ll_sync_cache_10(sg_fd, 0, 0, 0, 0, 0, 1, opts->verbose);
        break;
    case CMD_UNLOCK:
        res = sg_ll_prevent_allow(sg_fd, 0, 1, opts->verbose);
        break;
    default:
        fprintf(stderr, "unknown cmd number [%d]\n", scmdp->cmd_num);
        return SG_LIB_SYNTAX_ERROR;
    }
    return res;
}
