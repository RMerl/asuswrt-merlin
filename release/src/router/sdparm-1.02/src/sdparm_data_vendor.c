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

#include <stdlib.h>
#include "sdparm.h"


/*
 * sdparm is a utility program to access and change SCSI device
 * (logical unit) mode page fields and do some other housekeeping.
 *
 * This file contains vendor data tables that may be useful for other
 * programs.
 */


/* Vendor specific mode pages */
struct sdparm_vendor_name_t sdparm_vendor_id[] = {
    {VENDOR_SEAGATE, "sea", "Seagate disk"},
    {VENDOR_HITACHI, "hit", "Hitachi disk"},
    {VENDOR_MAXTOR, "max", "Maxtor disk"},
    {VENDOR_FUJITSU, "fuj", "Fujitsu disk"},
    {0, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_v_seagate_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "ua", "Unit attention (seagate)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_v_seagate_arr[] = {
    /* Unit attention page [0x0] Seagate */
    {"PM", UNIT_ATTENTION_MP, 0, 0, 2, 7, 1, MF_COMMON,
        "Performance Mode",
        "0: adaptive cache ('server mode')\t"
        "1: number of cache segments as per caching page ('desktop mode')"},
    {"SSM", UNIT_ATTENTION_MP, 0, 0, 2, 6, 1, 0,
        "Synchronous select mode (SPI)",
        "0: drive will not initiate WDTR or SDTR\t"
        "1: drive may initiate WDTR or SDTR"},
    {"IL", UNIT_ATTENTION_MP, 0, 0, 2, 5, 1, MF_COMMON,
        "Inquiry length",
        "0: more than 36 bytes in response\t"
        "1: 36 byte response as per SCSI-2"},
    {"UA", UNIT_ATTENTION_MP, 0, 0, 2, 4, 1, MF_COMMON,
        "Unit attention",
        "0: unit attention condition for all initiators after reset\t"
        "1: no check condition with unit attention after reset"},
    {"DFUA", UNIT_ATTENTION_MP, 0, 0, 2, 3, 1, 0,
        "Disable force unit access (FUA)",
        "0: honour FUA bit setting on READ and WRITE commands\t"
        "1: ignore FUA bit setting"},
    {"ROUND", UNIT_ATTENTION_MP, 0, 0, 2, 2, 1, 0,
        "Reporting of log parameter rounding (wrap around)",
        "0: do not report (silently round)\t"
        "1: report rounding (as per SPC-4)"},
    {"STRICT", UNIT_ATTENTION_MP, 0, 0, 2, 1, 1, MF_COMMON,
        "Strict when trying to alter unchangeable mode page fields",
        "0: silently ignore\t"
        "1: report as error"},
    {"SCSI2", UNIT_ATTENTION_MP, 0, 0, 2, 0, 1, MF_COMMON,
        "SCSI-2 lengths for control and caching mode pages",
        "0: as per recent standards\t"
        "1: SCSI-2 lengths: control, 6; caching, 10"},
    {"SSEEK", UNIT_ATTENTION_MP, 0, 0, 3, 6, 1, 0,
        "Self seek",
        "0: off (normal operating mode)\t"
        "1: enter self seek mode (test power dissipation, acoustics, etc)"},
    {"JIT3", UNIT_ATTENTION_MP, 0, 0, 4, 3, 1, 0,
        "Just in time 3, slowest seek type",
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT2", UNIT_ATTENTION_MP, 0, 0, 4, 2, 1, 0,
        "Just in time 2, second slowest seek type",
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT1", UNIT_ATTENTION_MP, 0, 0, 4, 1, 1, 0,
        "Just in time 1, second fastest seek type",
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT0", UNIT_ATTENTION_MP, 0, 0, 4, 0, 1, 0,
        "Just in time 0, fastest seek type",
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_v_hitachi_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "vup", "Vendor unique parameters (hitachi)",
        NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_v_hitachi_arr[] = {
    /* Vendor unique parameters page [0x0] Hitachi */
    {"MRG", UNIT_ATTENTION_MP, 0, 0, 2, 3, 1, 0,
        "Merge Glist into Plist (during format)", NULL},
    {"VGMDE", UNIT_ATTENTION_MP, 0, 0, 3, 6, 1, MF_COMMON,
        "Veggie mode (do random seeks when idle)", NULL},
    {"RRNDE", UNIT_ATTENTION_MP, 0, 0, 3, 1, 1, 0,
        "Report recovered non data errors (when PER set)", NULL},
    {"FDD", UNIT_ATTENTION_MP, 0, 0, 5, 4, 1, 0,
        "Format degraded disable (reporting for Test Unit Ready)", NULL},
    {"CAEN", UNIT_ATTENTION_MP, 0, 0, 5, 1, 1, MF_COMMON,
        "Command aging enable", NULL},
    {"IGRA", UNIT_ATTENTION_MP, 0, 0, 6, 7, 1, MF_COMMON,
        "Ignore reassigned LBA (when RC also set)", NULL},
    {"AVERP", UNIT_ATTENTION_MP, 0, 0, 6, 6, 1, MF_COMMON,
        "AV ERP mode (maximum retry count for read errors)",
        "0: use default (ignore RRC)\t"
        "1: use RRC field"},
    {"OCT", UNIT_ATTENTION_MP, 0, 0, 6, 3, 12, 0,
        "Overall command timer, 0 -> disabled (50 ms)", NULL},
    {"TT", UNIT_ATTENTION_MP, 0, 0, 9, 7, 8, 0,
        "Temperature threshold (celsius), 0 -> 85C", NULL},
    {"CAL", UNIT_ATTENTION_MP, 0, 0, 10, 7, 16, 0,
        "Command aging limit (50 ms), 0 -> 85C", NULL},
    {"RRT", UNIT_ATTENTION_MP, 0, 0, 12, 7, 8, 0,
        "Read reporting threshold for read recovered errors when PER set",
        NULL},
    {"WRT", UNIT_ATTENTION_MP, 0, 0, 13, 7, 8, 0,
        "Write reporting threshold for write recovered errors when PER set",
        NULL},
    {"DRRT", UNIT_ATTENTION_MP, 0, 0, 14, 7, 1, 0,
        "Disable restore reassign target",
        "0: REASSIGN attempts to recovery old data\t"
        "1: REASSIGN ignores old data"},
    {"FFMT", UNIT_ATTENTION_MP, 0, 0, 14, 3, 1, 0,
        "Fast format enable, format without writes to customer media", NULL},
    {"FCERT", UNIT_ATTENTION_MP, 0, 0, 15, 5, 1, 0,
        "Format certification (enable)", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_v_maxtor_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "uac", "Unit attention condition (maxtor)",
        NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_v_maxtor_arr[] = {
    /* Unit attention page [0x0] Seagate */
    {"DUA", UNIT_ATTENTION_MP, 0, 0, 2, 4, 1, MF_COMMON,
        "Disable unit attention", NULL},
};

static struct sdparm_mode_page_t sdparm_v_fujitsu_mode_pg[] = {
    {0x21, 0, 0, 0, "aerp", "Additional error recovery parameters (fujitsu)",
        NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_v_fujitsu_arr[] = {
    /* Additional error recovery parameters page [0x21] Fujitsu */
    {"RDSE", 0x21, 0, 0, 2, 3, 4, MF_COMMON,
        "Retries during a seek error", "0: no repositioning retries"},
};

/* Indexed by VENDOR_* define */
struct sdparm_vendor_pair sdparm_vendor_mp[] = {
    {sdparm_v_seagate_mode_pg, sdparm_mitem_v_seagate_arr},
    {sdparm_v_hitachi_mode_pg, sdparm_mitem_v_hitachi_arr},
    {sdparm_v_maxtor_mode_pg, sdparm_mitem_v_maxtor_arr},
    {sdparm_v_fujitsu_mode_pg, sdparm_mitem_v_fujitsu_arr},
};

int sdparm_vendor_mp_len = 
        sizeof(sdparm_vendor_mp) / sizeof(sdparm_vendor_mp[0]);

