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

/* NOTICE:
 *    On 5th October 2004 (v1.00) this file name was changed from sg_err.c
 *    to sg_lib.c and the previous GPL was changed to a FreeBSD license.
 *    The intention is to maintain this file and the related sg_lib.h file
 *    as open source and encourage their unencumbered use.
 *
 * CONTRIBUTIONS:
 *    This file started out as a copy of SCSI opcodes, sense keys and
 *    additional sense codes (ASC/ASCQ) kept in the Linux SCSI subsystem
 *    in the kernel source file: drivers/scsi/constant.c . That file
 *    bore this notice: "Copyright (C) 1993, 1994, 1995 Eric Youngdale"
 *    and a GPL notice.
 *
 *    Much of the data in this file is derived from SCSI draft standards
 *    found at http://www.t10.org with the "SCSI Primary Commands-4" (SPC-4)
 *    being the central point of reference.
 *
 *    Other contributions:
 *      Version 0.91 (20031116)
 *          sense key specific field (bytes 15-17) decoding [Trent Piepho]
 *
 * CHANGELOG (changes prior to v0.97 removed):
 *      v0.97 (20040830)
 *        safe_strerror(), rename sg_decode_sense() to sg_normalize_sense()
 *        decode descriptor sense data format in full
 *      v0.98 (20040924) [SPC-3 rev 21]
 *        renamed from sg_err.c to sg_lib.c 
 *        factor out sg_get_num() and sg_get_llnum() into this file
 *        add 'no_ascii<0' variant to dStrHex for ASCII-hex output only
 *      v1.00 (20041012)
 *        renamed from sg_err.c to sg_lib.c 
 *        change GPL to FreeBSD license 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sg_lib.h"


static char * version_str = "1.37 20071005";    /* spc-4 rev 11 */

FILE * sg_warnings_strm = NULL;        /* would like to default to stderr */

/* Commands with service actions that change the command name */
#define SG_MAINTENANCE_IN 0xa3
#define SG_MAINTENANCE_OUT 0xa4
#define SG_SERVICE_ACTION_IN_12 0xab
#define SG_SERVICE_ACTION_OUT_12 0xa9
#define SG_SERVICE_ACTION_IN_16 0x9e
#define SG_SERVICE_ACTION_OUT_16 0x9f
#define SG_VARIABLE_LENGTH_CMD 0x7f

static void dStrHexErr(const char* str, int len, int b_len, char * b);

struct value_name_t {
    int value;
    int peri_dev_type; /* only non-zero to disambiguate by command set */
    const char * name;
};

static const struct value_name_t normal_opcodes[] = {
    {0, 0, "Test Unit Ready"},
    {0x1, 0, "Rezero Unit"},
    {0x1, 1, "Rewind"},
    {0x3, 0, "Request Sense"},
    {0x4, 0, "Format Unit"},
    {0x4, 1, "Format medium"},
    {0x4, 2, "Format"},
    {0x5, 0, "Read Block Limits"},
    {0x7, 0, "Reassign Blocks"},
    {0x7, 8, "Initialize element status"},
    {0x8, 0, "Read(6)"},
    {0x8, 3, "Receive"},
    {0xa, 0, "Write(6)"},
    {0xa, 2, "Print"},
    {0xa, 3, "Send"},
    {0xb, 0, "Seek(6)"},
    {0xb, 1, "Set capacity"},
    {0xb, 2, "Slew and print"},
    {0xf, 0, "Read reverse(6)"},
    {0x10, 0, "Write filemarks(6)"},
    {0x10, 2, "Synchronize buffer"},
    {0x11, 0, "Space(6)"},
    {0x12, 0, "Inquiry"},
    {0x13, 0, "Verify(6)"},  /* SSC */
    {0x14, 0, "Recover buffered data"},
    {0x15, 0, "Mode select(6)"},
    {0x16, 0, "Reserve(6)"},    /* obsolete in SPC-4 r11 */
    {0x16, 8, "Reserve element(6)"},
    {0x17, 0, "Release(6)"},    /* obsolete in SPC-4 r11 */
    {0x17, 8, "Release element(6)"},
    {0x18, 0, "Copy"},          /* obsolete in SPC-4 r11 */
    {0x19, 0, "Erase(6)"},
    {0x1a, 0, "Mode sense(6)"},
    {0x1b, 0, "Start stop unit"},
    {0x1b, 1, "Load unload"},
    {0x1b, 0x12, "Load unload"},
    {0x1b, 2, "Stop print"},
    {0x1c, 0, "Receive diagnostic results"},
    {0x1d, 0, "Send diagnostic"},
    {0x1e, 0, "Prevent allow medium removal"},
    {0x23, 0, "Read Format capacities"},
    {0x24, 0, "Set window"},
    {0x25, 0, "Read capacity(10)"},
    {0x25, 0xf, "Read card capacity"},
    {0x28, 0, "Read(10)"},
    {0x29, 0, "Read generation"},
    {0x2a, 0, "Write(10)"},
    {0x2b, 0, "Seek(10)"},
    {0x2b, 1, "Locate(10)"},
    {0x2b, 8, "Position to element"},
    {0x2c, 0, "Erase(10)"},
    {0x2d, 0, "Read updated block"},
    {0x2e, 0, "Write and verify(10)"},
    {0x2f, 0, "Verify(10)"},
    {0x30, 0, "Search data high(10)"},
    {0x31, 0, "Search data equal(10)"},
    {0x32, 0, "Search data low(10)"},
    {0x33, 0, "Set limits(10)"},
    {0x34, 0, "Pre-fetch(10)"},
    {0x34, 1, "Read position"},
    {0x35, 0, "Synchronize cache(10)"},
    {0x36, 0, "Lock unlock cache(10)"},
    {0x37, 0, "Read defect data(10)"},
    {0x37, 8, "Initialize element status with range"},
    {0x38, 0, "Medium scan"},
    {0x39, 0, "Compare"},               /* obsolete in SPC-4 r11 */
    {0x3a, 0, "Copy and verify"},       /* obsolete in SPC-4 r11 */
    {0x3b, 0, "Write buffer"},
    {0x3c, 0, "Read buffer"},
    {0x3d, 0, "Update block"},
    {0x3e, 0, "Read long(10)"},
    {0x3f, 0, "Write long(10)"},
    {0x40, 0, "Change definition"},     /* obsolete in SPC-4 r11 */
    {0x41, 0, "Write same(10)"},
    {0x42, 0, "Read sub-channel"},
    {0x43, 0, "Read TOC/PMA/ATIP"},
    {0x44, 0, "Report density support"},
    {0x45, 0, "Play audio(10)"},
    {0x46, 0, "Get configuration"},
    {0x47, 0, "Play audio msf"},
    {0x4a, 0, "Get event status notification"},
    {0x4b, 0, "Pause/resume"},
    {0x4c, 0, "Log select"},
    {0x4d, 0, "Log sense"},
    {0x4e, 0, "Stop play/scan"},
    {0x50, 0, "Xdwrite(10)"},
    {0x51, 0, "Xpwrite(10)"},
    {0x51, 5, "Read disk information"},
    {0x52, 0, "Xdread(10)"},
    {0x52, 5, "Read track information"},
    {0x53, 0, "Reserve track"},
    {0x54, 0, "Send OPC information"},
    {0x55, 0, "Mode select(10)"},
    {0x56, 0, "Reserve(10)"},           /* obsolete in SPC-4 r11 */
    {0x56, 8, "Reserve element(10)"},
    {0x57, 0, "Release(10)"},           /* obsolete in SPC-4 r11 */
    {0x57, 8, "Release element(10)"},
    {0x58, 0, "Repair track"},
    {0x5a, 0, "Mode sense(10)"},
    {0x5b, 0, "Close track/session"},
    {0x5c, 0, "Read buffer capacity"},
    {0x5d, 0, "Send cue sheet"},
    {0x5e, 0, "Persistent reserve in"},
    {0x5f, 0, "Persistent reserve out"},
    {0x80, 0, "Xdwrite extended(16)"},
    {0x80, 1, "Write filemarks(16)"},
    {0x81, 0, "Rebuild(16)"},
    {0x81, 1, "Read reverse(16)"},
    {0x82, 0, "Regenerate(16)"},
    {0x83, 0, "Extended copy"},
    {0x84, 0, "Receive copy results"},
    {0x85, 0, "ATA command pass through(16)"},  /* was 0x98 in spc3 rev21c */
    {0x86, 0, "Access control in"},
    {0x87, 0, "Access control out"},
    {0x88, 0, "Read(16)"},
    {0x8a, 0, "Write(16)"},
    {0x8b, 0, "Orwrite(16)"},
    {0x8c, 0, "Read attribute"},
    {0x8d, 0, "Write attribute"},
    {0x8e, 0, "Write and verify(16)"},
    {0x8f, 0, "Verify(16)"},
    {0x90, 0, "Pre-fetch(16)"},
    {0x91, 0, "Synchronize cache(16)"},
    {0x91, 1, "Space(16)"},
    {0x92, 0, "Lock unlock cache(16)"},
    {0x92, 1, "Locate(16)"},
    {0x93, 0, "Write same(16)"},
    {0x93, 1, "Erase(16)"},
    {0x9e, 0, "Service action in(16)"},
    {0x9f, 0, "Service action out(16)"},
    {0xa0, 0, "Report luns"},
    {0xa1, 0, "ATA command pass through(12)"},
    {0xa1, 5, "Blank"},
    {0xa2, 0, "Security protocol in"},
    {0xa3, 0, "Maintenance in"},
    {0xa3, 5, "Send key"},
    {0xa4, 0, "Maintenance out"},
    {0xa4, 5, "Report key"},
    {0xa5, 0, "Move medium"},
    {0xa5, 5, "Play audio(12)"},
    {0xa6, 0, "Exchange medium"},
    {0xa6, 5, "Load/unload medium"},
    {0xa7, 0, "Move medium attached"},
    {0xa7, 5, "Set read ahead"},
    {0xa8, 0, "Read(12)"},
    {0xa9, 0, "Service action out(12)"},
    {0xaa, 0, "Write(12)"},
    {0xab, 0, "Service action in(12)"},
    {0xac, 0, "erase(12)"},
    {0xac, 5, "Get performance"},
    {0xad, 5, "Read DVD/BD structure"},
    {0xae, 0, "Write and verify(12)"},
    {0xaf, 0, "Verify(12)"},
    {0xb0, 0, "Search data high(12)"},
    {0xb1, 0, "Search data equal(12)"},
    {0xb1, 8, "Open/close import/export element"},
    {0xb2, 0, "Search data low(12)"},
    {0xb3, 0, "Set limits(12)"},
    {0xb4, 0, "Read element status attached"},
    {0xb5, 0, "Security protocol out"},
    {0xb5, 8, "Request volume element address"},
    {0xb6, 0, "Send volume tag"},
    {0xb6, 5, "Set streaming"},
    {0xb7, 0, "Read defect data(12)"},
    {0xb8, 0, "Read element status"},
    {0xb9, 0, "Read CD msf"},
    {0xba, 0, "Redundancy group in"},
    {0xba, 5, "Scan"},
    {0xbb, 0, "Redundancy group out"},
    {0xbb, 5, "Set CD speed"},
    {0xbc, 0, "Spare in"},
    {0xbd, 0, "Spare out"},
    {0xbd, 5, "Mechanism status"},
    {0xbe, 0, "Volume set in"},
    {0xbe, 5, "Read CD"},
    {0xbf, 0, "Volume set out"},
    {0xbf, 5, "Send DVD/BD structure"},
};

#define NORMAL_OPCODES_SZ \
        (int)(sizeof(normal_opcodes) / sizeof(normal_opcodes[0]))


static const struct value_name_t maint_in_arr[] = {
    {0x5, 0, "Report identifying information"},
                /* was "Report device identifier" prior to spc4r07 */
    {0xa, 0, "Report target port groups"},
    {0xb, 0, "Report aliases"},
    {0xc, 0, "Report supported operation codes"},
    {0xd, 0, "Report supported task management functions"},
    {0xe, 0, "Report priority"},
    {0xf, 0, "Report timestamp"},
    {0x10, 0, "Maintenance in"},
};

#define MAINT_IN_SZ \
        (int)(sizeof(maint_in_arr) / sizeof(maint_in_arr[0]))

static const struct value_name_t maint_out_arr[] = {
    {0x6, 0, "Set identifying information"},
                /* was "Set device identifier" prior to spc4r07 */
    {0xa, 0, "Set target port groups"},
    {0xb, 0, "Change aliases"},
    {0xe, 0, "Set priority"},
    {0xf, 0, "Set timestamp"},
    {0x10, 0, "Maintenance out"},
};

#define MAINT_OUT_SZ \
        (int)(sizeof(maint_out_arr) / sizeof(maint_out_arr[0]))

static const struct value_name_t serv_in12_arr[] = {
    {0x1, 0, "Read media serial number"},
};

#define SERV_IN12_SZ  \
        (int)(sizeof(serv_in12_arr) / sizeof(serv_in12_arr[0]))

static const struct value_name_t serv_out12_arr[] = {
    {0xff, 0, "Impossible command name"},
};

#define SERV_OUT12_SZ \
        (int)(sizeof(serv_out12_arr) / sizeof(serv_in12_arr[0]))

static const struct value_name_t serv_in16_arr[] = {
    {0x10, 0, "Read capacity(16)"},
    {0x11, 0, "Read long(16)"},
};

#define SERV_IN16_SZ  \
        (int)(sizeof(serv_in16_arr) / sizeof(serv_in16_arr[0]))

static const struct value_name_t serv_out16_arr[] = {
    {0x11, 0, "Write long(16)"},
    {0x1f, 0x12, "Notify data transfer device(16)"},
};

#define SERV_OUT16_SZ \
        (int)(sizeof(serv_out16_arr) / sizeof(serv_in16_arr[0]))

static const struct value_name_t variable_length_arr[] = {
    {0x1, 0, "Rebuild(32)"},
    {0x2, 0, "Regenerate(32)"},
    {0x3, 0, "Xdread(32)"},
    {0x4, 0, "Xdwrite(32)"},
    {0x5, 0, "Xdwrite extended(32)"},
    {0x6, 0, "Xpwrite(32)"},
    {0x7, 0, "Xdwriteread(32)"},
    {0x8, 0, "Xdwrite extended(64)"},
    {0x9, 0, "Read(32)"},
    {0xa, 0, "Verify(32)"},
    {0xb, 0, "Write(32)"},
    {0xc, 0, "Write an verify(32)"},
    {0xd, 0, "Write same(32)"},
    {0x8801, 0, "Format OSD"},
    {0x8802, 0, "Create (osd)"},
    {0x8803, 0, "List (osd)"},
    {0x8805, 0, "Read (osd)"},
    {0x8806, 0, "Write (osd)"},
    {0x8807, 0, "Append (osd)"},
    {0x8808, 0, "Flush (osd)"},
    {0x880a, 0, "Remove (osd)"},
    {0x880b, 0, "Create partition (osd)"},
    {0x880c, 0, "Remove partition (osd)"},
    {0x880e, 0, "Get attributes (osd)"},
    {0x880f, 0, "Set attributes (osd)"},
    {0x8812, 0, "Create and write (osd)"},
    {0x8815, 0, "Create collection (osd)"},
    {0x8816, 0, "Remove collection (osd)"},
    {0x8817, 0, "List collection (osd)"},
    {0x8818, 0, "Set key (osd)"},
    {0x8819, 0, "Set master key (osd)"},
    {0x881a, 0, "Flush collection (osd)"},
    {0x881b, 0, "Flush partition (osd)"},
    {0x881c, 0, "Flush OSD"},
    {0x8f7e, 0, "Perform SCSI command (osd)"},
    {0x8f7f, 0, "Perform task management function (osd)"},
};

#define VARIABLE_LENGTH_SZ \
        (int)(sizeof(variable_length_arr) / sizeof(variable_length_arr[0]))


/* searches 'arr' for match on 'value' then 'peri_type'. If matches
   'value' but not 'peri_type' the yields first 'value' match entry.
   There are 'arr_sz' elements of 'arr', if no match yields NULL. */
static const struct value_name_t *
get_value_name(const struct value_name_t * arr, int arr_sz, int value,
               int peri_type)
{
    const struct value_name_t * maxp = arr + arr_sz;
    const struct value_name_t * vp = arr;
    const struct value_name_t * holdp;

    for (; vp < maxp; ++vp) {
        if (value == vp->value) {
            if (peri_type == vp->peri_dev_type)
                return vp;
            holdp = vp;
            while (((vp + 1) < maxp) && 
                   (value == (vp + 1)->value)) {
                ++vp;
                if (peri_type == vp->peri_dev_type)
                    return vp;
            }
            return holdp;
        }
    }
    return NULL;
}

void
sg_set_warnings_strm(FILE * warnings_strm)
{
    sg_warnings_strm = warnings_strm;
}

#define CMD_NAME_LEN 128

void
sg_print_command(const unsigned char * command) 
{
    int k, sz;
    char buff[CMD_NAME_LEN];

    sg_get_command_name(command, 0, CMD_NAME_LEN, buff);
    buff[CMD_NAME_LEN - 1] = '\0';

    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    fprintf(sg_warnings_strm, "%s [", buff);
    if (SG_VARIABLE_LENGTH_CMD == command[0]) 
        sz = command[7] + 8;
    else
        sz = sg_get_command_size(command[0]);
    for (k = 0; k < sz; ++k)
        fprintf(sg_warnings_strm, "%02x ", command[k]);
    fprintf(sg_warnings_strm, "]\n");
}

void
sg_get_scsi_status_str(int scsi_status, int buff_len, char * buff)
{
    const char * ccp;

    scsi_status &= 0x7e; /* sanitize as much as possible */
    switch (scsi_status) {
        case 0: ccp = "Good"; break;
        case 0x2: ccp = "Check Condition"; break;
        case 0x4: ccp = "Condition Met"; break;
        case 0x8: ccp = "Busy"; break;
        case 0x10: ccp = "Intermediate (obsolete)"; break;
        case 0x14: ccp = "Intermediate-Condition Met (obs)"; break;
        case 0x18: ccp = "Reservation Conflict"; break;
        case 0x22: ccp = "Command Terminated (obsolete)"; break;
        case 0x28: ccp = "Task set Full"; break;
        case 0x30: ccp = "ACA Active"; break;
        case 0x40: ccp = "Task Aborted"; break;
        default: ccp = "Unknown status"; break;
    }
    strncpy(buff, ccp, buff_len);
}

void
sg_print_scsi_status(int scsi_status) 
{
    char buff[128];

    sg_get_scsi_status_str(scsi_status, sizeof(buff) - 1, buff);
    buff[sizeof(buff) - 1] = '\0';
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    fprintf(sg_warnings_strm, "%s ", buff);
}


struct error_info {
    unsigned char code1, code2;
    const char * text;
};

struct error_info2 {
    unsigned char code1, code2_min, code2_max;
    const char * text;
};

static struct error_info2 additional2[] =
{
    {0x40,0x01,0x7f,"Ram failure [0x%x]"},
    {0x40,0x80,0xff,"Diagnostic failure on component [0x%x]"},
    {0x41,0x01,0xff,"Data path failure [0x%x]"},
    {0x42,0x01,0xff,"Power-on or self-test failure [0x%x]"},
    {0x4d,0x00,0xff,"Tagged overlapped commands [0x%x]"},
    {0x70,0x00,0xff,"Decompression exception short algorithm id of 0x%x"},
    {0, 0, 0, NULL}
};

static struct error_info additional[] =
{
    {0x00,0x00,"No additional sense information"},
    {0x00,0x01,"Filemark detected"},
    {0x00,0x02,"End-of-partition/medium detected"},
    {0x00,0x03,"Setmark detected"},
    {0x00,0x04,"Beginning-of-partition/medium detected"},
    {0x00,0x05,"End-of-data detected"},
    {0x00,0x06,"I/O process terminated"},
    {0x00,0x11,"Audio play operation in progress"},
    {0x00,0x12,"Audio play operation paused"},
    {0x00,0x13,"Audio play operation successfully completed"},
    {0x00,0x14,"Audio play operation stopped due to error"},
    {0x00,0x15,"No current audio status to return"},
    {0x00,0x16,"operation in progress"},
    {0x00,0x17,"Cleaning requested"},
    {0x00,0x18,"Erase operation in progress"},
    {0x00,0x19,"Locate operation in progress"},
    {0x00,0x1a,"Rewind operation in progress"},
    {0x00,0x1b,"Set capacity operation in progress"},
    {0x00,0x1c,"Verify operation in progress"},
    {0x00,0x1d,"ATA pass through information available"},
    {0x01,0x00,"No index/sector signal"},
    {0x02,0x00,"No seek complete"},
    {0x03,0x00,"Peripheral device write fault"},
    {0x03,0x01,"No write current"},
    {0x03,0x02,"Excessive write errors"},
    {0x04,0x00,"Logical unit not ready, cause not reportable"},
    {0x04,0x01,"Logical unit is in process of becoming ready"},
    {0x04,0x02,"Logical unit not ready, " 
                "initializing command required"},
    {0x04,0x03,"Logical unit not ready, " 
                "manual intervention required"},
    {0x04,0x04,"Logical unit not ready, format in progress"},
    {0x04,0x05,"Logical unit not ready, rebuild in progress"},
    {0x04,0x06,"Logical unit not ready, recalculation in progress"},
    {0x04,0x07,"Logical unit not ready, operation in progress"},
    {0x04,0x08,"Logical unit not ready, long write in progress"},
    {0x04,0x09,"Logical unit not ready, self-test in progress"},
    {0x04,0x0a,"Logical unit " 
                "not accessible, asymmetric access state transition"},
    {0x04,0x0b,"Logical unit " 
                "not accessible, target port in standby state"},
    {0x04,0x0c,"Logical unit " 
                "not accessible, target port in unavailable state"},
    {0x04,0x10,"Logical unit not ready, "
                "auxiliary memory not accessible"},
    {0x04,0x11,"Logical unit not ready, "
                "notify (enable spinup) required"},
    {0x04,0x12,"Logical unit not ready, offline"},
    {0x05,0x00,"Logical unit does not respond to selection"},
    {0x06,0x00,"No reference position found"},
    {0x07,0x00,"Multiple peripheral devices selected"},
    {0x08,0x00,"Logical unit communication failure"},
    {0x08,0x01,"Logical unit communication time-out"},
    {0x08,0x02,"Logical unit communication parity error"},
    {0x08,0x03,"Logical unit communication CRC error (Ultra-DMA/32)"},
    {0x08,0x04,"Unreachable copy target"},
    {0x09,0x00,"Track following error"},
    {0x09,0x01,"Tracking servo failure"},
    {0x09,0x02,"Focus servo failure"},
    {0x09,0x03,"Spindle servo failure"},
    {0x09,0x04,"Head select fault"},
    {0x0A,0x00,"Error log overflow"},
    {0x0B,0x00,"Warning"},
    {0x0B,0x01,"Warning - specified temperature exceeded"},
    {0x0B,0x02,"Warning - enclosure degraded"},
    {0x0B,0x03,"Warning - background self-test failed"},
    {0x0B,0x04,"Warning - background pre-scan detected medium error"},
    {0x0B,0x05,"Warning - background medium scan detected medium error"},
    {0x0C,0x00,"Write error"},
    {0x0C,0x01,"Write error - recovered with auto reallocation"},
    {0x0C,0x02,"Write error - auto reallocation failed"},
    {0x0C,0x03,"Write error - recommend reassignment"},
    {0x0C,0x04,"Compression check miscompare error"},
    {0x0C,0x05,"Data expansion occurred during compression"},
    {0x0C,0x06,"Block not compressible"},
    {0x0C,0x07,"Write error - recovery needed"},
    {0x0C,0x08,"Write error - recovery failed"},
    {0x0C,0x09,"Write error - loss of streaming"},
    {0x0C,0x0A,"Write error - padding blocks added"},
    {0x0C,0x0B,"Auxiliary memory write error"},
    {0x0C,0x0C,"Write error - unexpected unsolicited data"},
    {0x0C,0x0D,"Write error - not enough unsolicited data"},
    {0x0C,0x0F,"Defects in error window"},
    {0x0D,0x00,"Error detected by third party temporary initiator"},
    {0x0D,0x01,"Third party device failure"},
    {0x0D,0x02,"Copy target device not reachable"},
    {0x0D,0x03,"Incorrect copy target device type"},
    {0x0D,0x04,"Copy target device data underrun"},
    {0x0D,0x05,"Copy target device data overrun"},
    {0x0E,0x00,"Invalid information unit"},
    {0x0E,0x01,"Information unit too short"},
    {0x0E,0x02,"Information unit too long"},
    {0x0E,0x03,"Invalid field in command information unit"},
    {0x10,0x00,"Id CRC or ECC error"},
    {0x10,0x01,"Logical block guard check failed"},
    {0x10,0x02,"Logical block application tag check failed"},
    {0x10,0x03,"Logical block reference tag check failed"},
    {0x11,0x00,"Unrecovered read error"},
    {0x11,0x01,"Read retries exhausted"},
    {0x11,0x02,"Error too long to correct"},
    {0x11,0x03,"Multiple read errors"},
    {0x11,0x04,"Unrecovered read error - auto reallocate failed"},
    {0x11,0x05,"L-EC uncorrectable error"},
    {0x11,0x06,"CIRC unrecovered error"},
    {0x11,0x07,"Data re-synchronization error"},
    {0x11,0x08,"Incomplete block read"},
    {0x11,0x09,"No gap found"},
    {0x11,0x0A,"Miscorrected error"},
    {0x11,0x0B,"Unrecovered read error - recommend reassignment"},
    {0x11,0x0C,"Unrecovered read error - recommend rewrite the data"},
    {0x11,0x0D,"De-compression CRC error"},
    {0x11,0x0E,"Cannot decompress using declared algorithm"},
    {0x11,0x0F,"Error reading UPC/EAN number"},
    {0x11,0x10,"Error reading ISRC number"},
    {0x11,0x11,"Read error - loss of streaming"},
    {0x11,0x12,"Auxiliary memory read error"},
    {0x11,0x13,"Read error - failed retransmission request"},
    {0x11,0x14,"Read error - LBA marked bad by application client"},
    {0x12,0x00,"Address mark not found for id field"},
    {0x13,0x00,"Address mark not found for data field"},
    {0x14,0x00,"Recorded entity not found"},
    {0x14,0x01,"Record not found"},
    {0x14,0x02,"Filemark or setmark not found"},
    {0x14,0x03,"End-of-data not found"},
    {0x14,0x04,"Block sequence error"},
    {0x14,0x05,"Record not found - recommend reassignment"},
    {0x14,0x06,"Record not found - data auto-reallocated"},
    {0x14,0x07,"Locate operation failure"},
    {0x15,0x00,"Random positioning error"},
    {0x15,0x01,"Mechanical positioning error"},
    {0x15,0x02,"Positioning error detected by read of medium"},
    {0x16,0x00,"Data synchronization mark error"},
    {0x16,0x01,"Data sync error - data rewritten"},
    {0x16,0x02,"Data sync error - recommend rewrite"},
    {0x16,0x03,"Data sync error - data auto-reallocated"},
    {0x16,0x04,"Data sync error - recommend reassignment"},
    {0x17,0x00,"Recovered data with no error correction applied"},
    {0x17,0x01,"Recovered data with retries"},
    {0x17,0x02,"Recovered data with positive head offset"},
    {0x17,0x03,"Recovered data with negative head offset"},
    {0x17,0x04,"Recovered data with retries and/or circ applied"},
    {0x17,0x05,"Recovered data using previous sector id"},
    {0x17,0x06,"Recovered data without ECC - data auto-reallocated"},
    {0x17,0x07,"Recovered data without ECC - recommend reassignment"},
    {0x17,0x08,"Recovered data without ECC - recommend rewrite"},
    {0x17,0x09,"Recovered data without ECC - data rewritten"},
    {0x18,0x00,"Recovered data with error correction applied"},
    {0x18,0x01,"Recovered data with error corr. & retries applied"},
    {0x18,0x02,"Recovered data - data auto-reallocated"},
    {0x18,0x03,"Recovered data with CIRC"},
    {0x18,0x04,"Recovered data with L-EC"},
    {0x18,0x05,"Recovered data - recommend reassignment"},
    {0x18,0x06,"Recovered data - recommend rewrite"},
    {0x18,0x07,"Recovered data with ECC - data rewritten"},
    {0x18,0x08,"Recovered data with linking"},
    {0x19,0x00,"Defect list error"},
    {0x19,0x01,"Defect list not available"},
    {0x19,0x02,"Defect list error in primary list"},
    {0x19,0x03,"Defect list error in grown list"},
    {0x1A,0x00,"Parameter list length error"},
    {0x1B,0x00,"Synchronous data transfer error"},
    {0x1C,0x00,"Defect list not found"},
    {0x1C,0x01,"Primary defect list not found"},
    {0x1C,0x02,"Grown defect list not found"},
    {0x1D,0x00,"Miscompare during verify operation"},
    {0x1E,0x00,"Recovered id with ECC correction"},
    {0x1F,0x00,"Partial defect list transfer"},
    {0x20,0x00,"Invalid command operation code"},
    {0x20,0x01,"Access denied - initiator pending-enrolled"},
    {0x20,0x02,"Access denied - no access rights"},
    {0x20,0x03,"Access denied - invalid mgmt id key"},
    {0x20,0x04,"Illegal command while in write capable state"},
    {0x20,0x05,"Write type operation while in read capable state (obs)"},
    {0x20,0x06,"Illegal command while in explicit address mode"},
    {0x20,0x07,"Illegal command while in implicit address mode"},
    {0x20,0x08,"Access denied - enrollment conflict"},
    {0x20,0x09,"Access denied - invalid LU identifier"},
    {0x20,0x0A,"Access denied - invalid proxy token"},
    {0x20,0x0B,"Access denied - ACL LUN conflict"},
    {0x21,0x00,"Logical block address out of range"},
    {0x21,0x01,"Invalid element address"},
    {0x21,0x02,"Invalid address for write"},
    {0x21,0x03,"Invalid write crossing layer jump"},
    {0x22,0x00,"Illegal function (use 20 00, 24 00, or 26 00)"},
    {0x24,0x00,"Invalid field in cdb"},
    {0x24,0x01,"CDB decryption error"},
    {0x24,0x02,"Invalid cdb field while in explicit block model (obs)"},
    {0x24,0x03,"Invalid cdb field while in implicit block model (obs)"},
    {0x24,0x04,"Security audit value frozen"},
    {0x24,0x05,"Security working key frozen"},
    {0x24,0x06,"Nonce not unique"},
    {0x24,0x07,"Nonce timestamp out of range"},
    {0x25,0x00,"Logical unit not supported"},
    {0x26,0x00,"Invalid field in parameter list"},
    {0x26,0x01,"Parameter not supported"},
    {0x26,0x02,"Parameter value invalid"},
    {0x26,0x03,"Threshold parameters not supported"},
    {0x26,0x04,"Invalid release of persistent reservation"},
    {0x26,0x05,"Data decryption error"},
    {0x26,0x06,"Too many target descriptors"},
    {0x26,0x07,"Unsupported target descriptor type code"},
    {0x26,0x08,"Too many segment descriptors"},
    {0x26,0x09,"Unsupported segment descriptor type code"},
    {0x26,0x0A,"Unexpected inexact segment"},
    {0x26,0x0B,"Inline data length exceeded"},
    {0x26,0x0C,"Invalid operation for copy source or destination"},
    {0x26,0x0D,"Copy segment granularity violation"},
    {0x26,0x0E,"Invalid parameter while port is enabled"},
    {0x26,0x0F,"Invalid data-out buffer integrity check value"},
    {0x26,0x10,"Data decryption key fail limit reached"},
    {0x26,0x11,"Incomplete key-associated data set"},
    {0x26,0x12,"Vendor specific key reference not found"},
    {0x27,0x00,"Write protected"},
    {0x27,0x01,"Hardware write protected"},
    {0x27,0x02,"Logical unit software write protected"},
    {0x27,0x03,"Associated write protect"},
    {0x27,0x04,"Persistent write protect"},
    {0x27,0x05,"Permanent write protect"},
    {0x27,0x06,"Conditional write protect"},
    {0x28,0x00,"Not ready to ready change, medium may have changed"},
    {0x28,0x01,"Import or export element accessed"},
    {0x28,0x02,"Format-layer may have changed"},
    {0x29,0x00,"Power on, reset, or bus device reset occurred"},
    {0x29,0x01,"Power on occurred"},
    {0x29,0x02,"SCSI bus reset occurred"},
    {0x29,0x03,"Bus device reset function occurred"},
    {0x29,0x04,"Device internal reset"},
    {0x29,0x05,"Transceiver mode changed to single-ended"},
    {0x29,0x06,"Transceiver mode changed to lvd"},
    {0x29,0x07,"I_T nexus loss occurred"},
    {0x2A,0x00,"Parameters changed"},
    {0x2A,0x01,"Mode parameters changed"},
    {0x2A,0x02,"Log parameters changed"},
    {0x2A,0x03,"Reservations preempted"},
    {0x2A,0x04,"Reservations released"},
    {0x2A,0x05,"Registrations preempted"},
    {0x2A,0x06,"Asymmetric access state changed"},
    {0x2A,0x07,"Implicit asymmetric access state transition failed"},
    {0x2A,0x08,"Priority changed"},
    {0x2A,0x09,"Capacity data has changed"},
    {0x2A,0x10,"Timestamp changed"},
    {0x2A,0x11,"Data encryption parameters changed by another i_t nexus"},
    {0x2A,0x12,"Data encryption parameters changed by vendor specific event"},
    {0x2A,0x13,"Data encryption key instance counter has changed"},
    {0x2B,0x00,"Copy cannot execute since host cannot disconnect"},
    {0x2C,0x00,"Command sequence error"},
    {0x2C,0x01,"Too many windows specified"},
    {0x2C,0x02,"Invalid combination of windows specified"},
    {0x2C,0x03,"Current program area is not empty"},
    {0x2C,0x04,"Current program area is empty"},
    {0x2C,0x05,"Illegal power condition request"},
    {0x2C,0x06,"Persistent prevent conflict"},
    {0x2C,0x07,"Previous busy status"},
    {0x2C,0x08,"Previous task set full status"},
    {0x2C,0x09,"Previous reservation conflict status"},
    {0x2C,0x0A,"Partition or collection contains user objects"},
    {0x2C,0x0B,"Not reserved"},
    {0x2D,0x00,"Overwrite error on update in place"},
    {0x2E,0x00,"Insufficient time for operation"},
    {0x2F,0x00,"Commands cleared by another initiator"},
    {0x2F,0x01,"Commands cleared by power loss notification"},
    {0x2F,0x02,"Commands cleared by device server"},
    {0x30,0x00,"Incompatible medium installed"},
    {0x30,0x01,"Cannot read medium - unknown format"},
    {0x30,0x02,"Cannot read medium - incompatible format"},
    {0x30,0x03,"Cleaning cartridge installed"},
    {0x30,0x04,"Cannot write medium - unknown format"},
    {0x30,0x05,"Cannot write medium - incompatible format"},
    {0x30,0x06,"Cannot format medium - incompatible medium"},
    {0x30,0x07,"Cleaning failure"},
    {0x30,0x08,"Cannot write - application code mismatch"},
    {0x30,0x09,"Current session not fixated for append"},
    {0x30,0x0A,"Cleaning request rejected"},
    {0x30,0x0B,"Cleaning tape expired"},
    {0x30,0x0C,"WORM medium - overwrite attempted"},
    {0x30,0x0D,"WORM medium - integrity check"},
    {0x30,0x10,"Medium not formatted"},
    {0x31,0x00,"Medium format corrupted"},
    {0x31,0x01,"Format command failed"},
    {0x31,0x02,"Zoned formatting failed due to spare linking"},
    {0x32,0x00,"No defect spare location available"},
    {0x32,0x01,"Defect list update failure"},
    {0x33,0x00,"Tape length error"},
    {0x34,0x00,"Enclosure failure"},
    {0x35,0x00,"Enclosure services failure"},
    {0x35,0x01,"Unsupported enclosure function"},
    {0x35,0x02,"Enclosure services unavailable"},
    {0x35,0x03,"Enclosure services transfer failure"},
    {0x35,0x04,"Enclosure services transfer refused"},
    {0x35,0x05,"Enclosure services checksum error"},
    {0x36,0x00,"Ribbon, ink, or toner failure"},
    {0x37,0x00,"Rounded parameter"},
    {0x38,0x00,"Event status notification"},
    {0x38,0x02,"Esn - power management class event"},
    {0x38,0x04,"Esn - media class event"},
    {0x38,0x06,"Esn - device busy class event"},
    {0x39,0x00,"Saving parameters not supported"},
    {0x3A,0x00,"Medium not present"},
    {0x3A,0x01,"Medium not present - tray closed"},
    {0x3A,0x02,"Medium not present - tray open"},
    {0x3A,0x03,"Medium not present - loadable"},
    {0x3A,0x04,"Medium not present - medium auxiliary memory accessible"},
    {0x3B,0x00,"Sequential positioning error"},
    {0x3B,0x01,"Tape position error at beginning-of-medium"},
    {0x3B,0x02,"Tape position error at end-of-medium"},
    {0x3B,0x03,"Tape or electronic vertical forms unit not ready"},
    {0x3B,0x04,"Slew failure"},
    {0x3B,0x05,"Paper jam"},
    {0x3B,0x06,"Failed to sense top-of-form"},
    {0x3B,0x07,"Failed to sense bottom-of-form"},
    {0x3B,0x08,"Reposition error"},
    {0x3B,0x09,"Read past end of medium"},
    {0x3B,0x0A,"Read past beginning of medium"},
    {0x3B,0x0B,"Position past end of medium"},
    {0x3B,0x0C,"Position past beginning of medium"},
    {0x3B,0x0D,"Medium destination element full"},
    {0x3B,0x0E,"Medium source element empty"},
    {0x3B,0x0F,"End of medium reached"},
    {0x3B,0x11,"Medium magazine not accessible"},
    {0x3B,0x12,"Medium magazine removed"},
    {0x3B,0x13,"Medium magazine inserted"},
    {0x3B,0x14,"Medium magazine locked"},
    {0x3B,0x15,"Medium magazine unlocked"},
    {0x3B,0x16,"Mechanical positioning or changer error"},
    {0x3B,0x17,"Read past end of user object"},
    {0x3D,0x00,"Invalid bits in identify message"},
    {0x3E,0x00,"Logical unit has not self-configured yet"},
    {0x3E,0x01,"Logical unit failure"},
    {0x3E,0x02,"Timeout on logical unit"},
    {0x3E,0x03,"Logical unit failed self-test"},
    {0x3E,0x04,"Logical unit unable to update self-test log"},
    {0x3F,0x00,"Target operating conditions have changed"},
    {0x3F,0x01,"Microcode has been changed"},
    {0x3F,0x02,"Changed operating definition"},
    {0x3F,0x03,"Inquiry data has changed"},
    {0x3F,0x04,"Component device attached"},
    {0x3F,0x05,"Device identifier changed"},
    {0x3F,0x06,"Redundancy group created or modified"},
    {0x3F,0x07,"Redundancy group deleted"},
    {0x3F,0x08,"Spare created or modified"},
    {0x3F,0x09,"Spare deleted"},
    {0x3F,0x0A,"Volume set created or modified"},
    {0x3F,0x0B,"Volume set deleted"},
    {0x3F,0x0C,"Volume set deassigned"},
    {0x3F,0x0D,"Volume set reassigned"},
    {0x3F,0x0E,"Reported luns data has changed"},
    {0x3F,0x0F,"Echo buffer overwritten"},
    {0x3F,0x10,"Medium loadable"},
    {0x3F,0x11,"Medium auxiliary memory accessible"},
    {0x3F,0x12,"iSCSI IP address added"},
    {0x3F,0x13,"iSCSI IP address removed"},
    {0x3F,0x14,"iSCSI IP address changed"},

    /*
     * ASC 0x40, 0x41 and 0x42 overridden by "additional2" array entries
     * for ascq > 1. Preferred error message for this group is
     * "Diagnostic failure on component nn (80h-ffh)".
     */
    {0x40,0x00,"Ram failure (should use 40 nn)"},
    {0x41,0x00,"Data path failure (should use 40 nn)"},
    {0x42,0x00,"Power-on or self-test failure (should use 40 nn)"},

    {0x43,0x00,"Message error"},
    {0x44,0x00,"Internal target failure"},
    {0x44,0x71,"ATA device failed Set Features"},
    {0x45,0x00,"Select or reselect failure"},
    {0x46,0x00,"Unsuccessful soft reset"},
    {0x47,0x00,"SCSI parity error"},
    {0x47,0x01,"Data phase CRC error detected"},
    {0x47,0x02,"SCSI parity error detected during st data phase"},
    {0x47,0x03,"Information unit iuCRC error detected"},
    {0x47,0x04,"Asynchronous information protection error detected"},
    {0x47,0x05,"Protocol service CRC error"},
    {0x47,0x06,"Phy test function in progress"},
    {0x47,0x7F,"Some commands cleared by iSCSI protocol event"},
    {0x48,0x00,"Initiator detected error message received"},
    {0x49,0x00,"Invalid message error"},
    {0x4A,0x00,"Command phase error"},
    {0x4B,0x00,"Data phase error"},
    {0x4B,0x01,"Invalid target port transfer tag received"},
    {0x4B,0x02,"Too much write data"},
    {0x4B,0x03,"Ack/nak timeout"},
    {0x4B,0x04,"Nak received"},
    {0x4B,0x05,"Data offset error"},
    {0x4B,0x06,"Initiator response timeout"},
    {0x4C,0x00,"Logical unit failed self-configuration"},
    /*
     * ASC 0x4D overridden by an "additional2" array entry
     * so there is no need to have them here.
     */
    /* {0x4D,0x00,"Tagged overlapped commands (nn = queue tag)"}, */

    {0x4E,0x00,"Overlapped commands attempted"},
    {0x50,0x00,"Write append error"},
    {0x50,0x01,"Write append position error"},
    {0x50,0x02,"Position error related to timing"},
    {0x51,0x00,"Erase failure"},
    {0x51,0x01,"Erase failure - incomplete erase operation detected"},
    {0x52,0x00,"Cartridge fault"},
    {0x53,0x00,"Media load or eject failed"},
    {0x53,0x01,"Unload tape failure"},
    {0x53,0x02,"Medium removal prevented"},
    {0x53,0x03,"Medium removal prevented by data transfer element"},
    {0x53,0x04,"Medium thread or unthread failure"},
    {0x54,0x00,"SCSI to host system interface failure"},
    {0x55,0x00,"System resource failure"},
    {0x55,0x01,"System buffer full"},
    {0x55,0x02,"Insufficient reservation resources"},
    {0x55,0x03,"Insufficient resources"},
    {0x55,0x04,"Insufficient registration resources"},
    {0x55,0x05,"Insufficient access control resources"},
    {0x55,0x06,"Auxiliary memory out of space"},
    {0x55,0x07,"Quota error"},
    {0x55,0x08,"Maximum number of supplemental decryption keys exceeded"},
    {0x57,0x00,"Unable to recover table-of-contents"},
    {0x58,0x00,"Generation does not exist"},
    {0x59,0x00,"Updated block read"},
    {0x5A,0x00,"Operator request or state change input"},
    {0x5A,0x01,"Operator medium removal request"},
    {0x5A,0x02,"Operator selected write protect"},
    {0x5A,0x03,"Operator selected write permit"},
    {0x5B,0x00,"Log exception"},
    {0x5B,0x01,"Threshold condition met"},
    {0x5B,0x02,"Log counter at maximum"},
    {0x5B,0x03,"Log list codes exhausted"},
    {0x5C,0x00,"Rpl status change"},
    {0x5C,0x01,"Spindles synchronized"},
    {0x5C,0x02,"Spindles not synchronized"},
    {0x5D,0x00,"Failure prediction threshold exceeded"},
    {0x5D,0x01,"Media failure prediction threshold exceeded"},
    {0x5D,0x02,"Logical unit failure prediction threshold exceeded"},
    {0x5D,0x03,"spare area exhaustion prediction threshold exceeded"},
    {0x5D,0x10,"Hardware impending failure general hard drive failure"},
    {0x5D,0x11,"Hardware impending failure drive error rate too high" },
    {0x5D,0x12,"Hardware impending failure data error rate too high" },
    {0x5D,0x13,"Hardware impending failure seek error rate too high" },
    {0x5D,0x14,"Hardware impending failure too many block reassigns"},
    {0x5D,0x15,"Hardware impending failure access times too high" },
    {0x5D,0x16,"Hardware impending failure start unit times too high" },
    {0x5D,0x17,"Hardware impending failure channel parametrics"},
    {0x5D,0x18,"Hardware impending failure controller detected"},
    {0x5D,0x19,"Hardware impending failure throughput performance"},
    {0x5D,0x1A,"Hardware impending failure seek time performance"},
    {0x5D,0x1B,"Hardware impending failure spin-up retry count"},
    {0x5D,0x1C,"Hardware impending failure drive calibration retry count"},
    {0x5D,0x20,"Controller impending failure general hard drive failure"},
    {0x5D,0x21,"Controller impending failure drive error rate too high" },
    {0x5D,0x22,"Controller impending failure data error rate too high" },
    {0x5D,0x23,"Controller impending failure seek error rate too high" },
    {0x5D,0x24,"Controller impending failure too many block reassigns"},
    {0x5D,0x25,"Controller impending failure access times too high" },
    {0x5D,0x26,"Controller impending failure start unit times too high" },
    {0x5D,0x27,"Controller impending failure channel parametrics"},
    {0x5D,0x28,"Controller impending failure controller detected"},
    {0x5D,0x29,"Controller impending failure throughput performance"},
    {0x5D,0x2A,"Controller impending failure seek time performance"},
    {0x5D,0x2B,"Controller impending failure spin-up retry count"},
    {0x5D,0x2C,"Controller impending failure drive calibration retry count"},
    {0x5D,0x30,"Data channel impending failure general hard drive failure"},
    {0x5D,0x31,"Data channel impending failure drive error rate too high" },
    {0x5D,0x32,"Data channel impending failure data error rate too high" },
    {0x5D,0x33,"Data channel impending failure seek error rate too high" },
    {0x5D,0x34,"Data channel impending failure too many block reassigns"},
    {0x5D,0x35,"Data channel impending failure access times too high" },
    {0x5D,0x36,"Data channel impending failure start unit times too high" },
    {0x5D,0x37,"Data channel impending failure channel parametrics"},
    {0x5D,0x38,"Data channel impending failure controller detected"},
    {0x5D,0x39,"Data channel impending failure throughput performance"},
    {0x5D,0x3A,"Data channel impending failure seek time performance"},
    {0x5D,0x3B,"Data channel impending failure spin-up retry count"},
    {0x5D,0x3C,"Data channel impending failure drive calibration retry count"},
    {0x5D,0x40,"Servo impending failure general hard drive failure"},
    {0x5D,0x41,"Servo impending failure drive error rate too high" },
    {0x5D,0x42,"Servo impending failure data error rate too high" },
    {0x5D,0x43,"Servo impending failure seek error rate too high" },
    {0x5D,0x44,"Servo impending failure too many block reassigns"},
    {0x5D,0x45,"Servo impending failure access times too high" },
    {0x5D,0x46,"Servo impending failure start unit times too high" },
    {0x5D,0x47,"Servo impending failure channel parametrics"},
    {0x5D,0x48,"Servo impending failure controller detected"},
    {0x5D,0x49,"Servo impending failure throughput performance"},
    {0x5D,0x4A,"Servo impending failure seek time performance"},
    {0x5D,0x4B,"Servo impending failure spin-up retry count"},
    {0x5D,0x4C,"Servo impending failure drive calibration retry count"},
    {0x5D,0x50,"Spindle impending failure general hard drive failure"},
    {0x5D,0x51,"Spindle impending failure drive error rate too high" },
    {0x5D,0x52,"Spindle impending failure data error rate too high" },
    {0x5D,0x53,"Spindle impending failure seek error rate too high" },
    {0x5D,0x54,"Spindle impending failure too many block reassigns"},
    {0x5D,0x55,"Spindle impending failure access times too high" },
    {0x5D,0x56,"Spindle impending failure start unit times too high" },
    {0x5D,0x57,"Spindle impending failure channel parametrics"},
    {0x5D,0x58,"Spindle impending failure controller detected"},
    {0x5D,0x59,"Spindle impending failure throughput performance"},
    {0x5D,0x5A,"Spindle impending failure seek time performance"},
    {0x5D,0x5B,"Spindle impending failure spin-up retry count"},
    {0x5D,0x5C,"Spindle impending failure drive calibration retry count"},
    {0x5D,0x60,"Firmware impending failure general hard drive failure"},
    {0x5D,0x61,"Firmware impending failure drive error rate too high" },
    {0x5D,0x62,"Firmware impending failure data error rate too high" },
    {0x5D,0x63,"Firmware impending failure seek error rate too high" },
    {0x5D,0x64,"Firmware impending failure too many block reassigns"},
    {0x5D,0x65,"Firmware impending failure access times too high" },
    {0x5D,0x66,"Firmware impending failure start unit times too high" },
    {0x5D,0x67,"Firmware impending failure channel parametrics"},
    {0x5D,0x68,"Firmware impending failure controller detected"},
    {0x5D,0x69,"Firmware impending failure throughput performance"},
    {0x5D,0x6A,"Firmware impending failure seek time performance"},
    {0x5D,0x6B,"Firmware impending failure spin-up retry count"},
    {0x5D,0x6C,"Firmware impending failure drive calibration retry count"},
    {0x5D,0xFF,"Failure prediction threshold exceeded (false)"},
    {0x5E,0x00,"Low power condition on"},
    {0x5E,0x01,"Idle condition activated by timer"},
    {0x5E,0x02,"Standby condition activated by timer"},
    {0x5E,0x03,"Idle condition activated by command"},
    {0x5E,0x04,"Standby condition activated by command"},
    {0x5E,0x41,"Power state change to active"},
    {0x5E,0x42,"Power state change to idle"},
    {0x5E,0x43,"Power state change to standby"},
    {0x5E,0x45,"Power state change to sleep"},
    {0x5E,0x47,"Power state change to device control"},
    {0x60,0x00,"Lamp failure"},
    {0x61,0x00,"Video acquisition error"},
    {0x61,0x01,"Unable to acquire video"},
    {0x61,0x02,"Out of focus"},
    {0x62,0x00,"Scan head positioning error"},
    {0x63,0x00,"End of user area encountered on this track"},
    {0x63,0x01,"Packet does not fit in available space"},
    {0x64,0x00,"Illegal mode for this track"},
    {0x64,0x01,"Invalid packet size"},
    {0x65,0x00,"Voltage fault"},
    {0x66,0x00,"Automatic document feeder cover up"},
    {0x66,0x01,"Automatic document feeder lift up"},
    {0x66,0x02,"Document jam in automatic document feeder"},
    {0x66,0x03,"Document miss feed automatic in document feeder"},
    {0x67,0x00,"Configuration failure"},
    {0x67,0x01,"Configuration of incapable logical units failed"},
    {0x67,0x02,"Add logical unit failed"},
    {0x67,0x03,"Modification of logical unit failed"},
    {0x67,0x04,"Exchange of logical unit failed"},
    {0x67,0x05,"Remove of logical unit failed"},
    {0x67,0x06,"Attachment of logical unit failed"},
    {0x67,0x07,"Creation of logical unit failed"},
    {0x67,0x08,"Assign failure occurred"},
    {0x67,0x09,"Multiply assigned logical unit"},
    {0x67,0x0A,"Set target port groups command failed"},
    {0x67,0x0B,"ATA device feature not enabled"},
    {0x68,0x00,"Logical unit not configured"},
    {0x69,0x00,"Data loss on logical unit"},
    {0x69,0x01,"Multiple logical unit failures"},
    {0x69,0x02,"Parity/data mismatch"},
    {0x6A,0x00,"Informational, refer to log"},
    {0x6B,0x00,"State change has occurred"},
    {0x6B,0x01,"Redundancy level got better"},
    {0x6B,0x02,"Redundancy level got worse"},
    {0x6C,0x00,"Rebuild failure occurred"},
    {0x6D,0x00,"Recalculate failure occurred"},
    {0x6E,0x00,"Command to logical unit failed"},
    {0x6F,0x00,"Copy protection key exchange failure - authentication "
               "failure"},
    {0x6F,0x01,"Copy protection key exchange failure - key not present"},
    {0x6F,0x02,"Copy protection key exchange failure - key not established"},
    {0x6F,0x03,"Read of scrambled sector without authentication"},
    {0x6F,0x04,"Media region code is mismatched to logical unit region"},
    {0x6F,0x05,"Drive region must be permanent/region reset count error"},
    {0x6F,0x06,"Insufficient block count for binding nonce recording"},
    {0x6F,0x07,"Conflict in binding nonce recording"},
    /*
     * ASC 0x70 overridden by an "additional2" array entry
     * so there is no need to have them here.
     */
    /* {0x70,0x00,"Decompression exception short algorithm id of nn"}, */

    {0x71,0x00,"Decompression exception long algorithm id"},
    {0x72,0x00,"Session fixation error"},
    {0x72,0x01,"Session fixation error writing lead-in"},
    {0x72,0x02,"Session fixation error writing lead-out"},
    {0x72,0x03,"Session fixation error - incomplete track in session"},
    {0x72,0x04,"Empty or partially written reserved track"},
    {0x72,0x05,"No more track reservations allowed"},
    {0x72,0x06,"RMZ extension is not allowed"},
    {0x72,0x07,"No more test zone extensions are allowed"},
    {0x73,0x00,"CD control error"},
    {0x73,0x01,"Power calibration area almost full"},
    {0x73,0x02,"Power calibration area is full"},
    {0x73,0x03,"Power calibration area error"},
    {0x73,0x04,"Program memory area update failure"},
    {0x73,0x05,"Program memory area is full"},
    {0x73,0x06,"RMA/PMA is almost full"},
    {0x73,0x10,"Current power calibration area almost full"},
    {0x73,0x11,"Current power calibration area is full"},
    {0x73,0x17,"RDZ is full"},
    {0x74,0x00,"Security error"},
    {0x74,0x01,"Unable to decrypt data"},
    {0x74,0x02,"Unencrypted data encountered while decrypting"},
    {0x74,0x03,"Incorrect data encryption key"},
    {0x74,0x04,"Cryptographic integrity validation failed"},
    {0x74,0x05,"Error decrypting data"},
    {0x74,0x06,"Unknown signature verification key"},
    {0x74,0x07,"Encryption parameters not useable"},
    {0x74,0x08,"Digital signature validation failure"},
    {0x74,0x09,"Encryption mode mismatch on read"},
    {0x74,0x0a,"Encrypted block not raw read enabled"},
    {0x74,0x0b,"Incorrect Encryption parameters"},
    {0x74,0x71,"Logical unit access not authorized"},
    {0, 0, NULL}
};

static const char *sense_key_desc[] = {
    "No Sense",                 /* Filemark, ILI and/or EOM; progress
                                   indication (during FORMAT); power
                                   condition sensing (REQUEST SENSE) */
    "Recovered Error",          /* The last command completed successfully
                                   but used error correction */
    "Not Ready",                /* The addressed target is not ready */
    "Medium Error",             /* Data error detected on the medium */
    "Hardware Error",           /* Controller or device failure */
    "Illegal Request",
    "Unit Attention",           /* Removable medium was changed, or
                                   the target has been reset */
    "Data Protect",             /* Access to the data is blocked */
    "Blank Check",              /* Reached unexpected written or unwritten
                                   region of the medium */
    "Key=9",                    /* Vendor specific */
    "Copy Aborted",             /* COPY or COMPARE was aborted */
    "Aborted Command",          /* The target aborted the command */
    "Equal",                    /* SEARCH DATA found data equal (obsolete) */
    "Volume Overflow",          /* Medium full with data to be written */
    "Miscompare",               /* Source data and data on the medium
                                   do not agree */
    "Key=15"                    /* Reserved */
};

char *
sg_get_sense_key_str(int sense_key, int buff_len, char * buff)
{
    if ((sense_key >= 0) && (sense_key < 16))
         snprintf(buff, buff_len, "%s", sense_key_desc[sense_key]);
    else
         snprintf(buff, buff_len, "invalid value: 0x%x", sense_key);
    return buff;
}

char *
sg_get_asc_ascq_str(int asc, int ascq, int buff_len, char * buff)
{
    int k, num, rlen;
    int found = 0;
    struct error_info * eip;
    struct error_info2 * ei2p;

    for (k = 0; additional2[k].text; ++k) {
        ei2p = &additional2[k];
        if ((ei2p->code1 == asc) &&
            (ascq >= ei2p->code2_min)  &&
            (ascq <= ei2p->code2_max)) {
            found = 1;
            num = snprintf(buff, buff_len, "Additional sense: ");
            rlen = buff_len - num;
            num += snprintf(buff + num, ((rlen > 0) ? rlen : 0),
                            ei2p->text, ascq);
        }
    }
    if (found)
        return buff;

    for (k = 0; additional[k].text; ++k) {
        eip = &additional[k];
        if (eip->code1 == asc &&
            eip->code2 == ascq) {
            found = 1;
            snprintf(buff, buff_len, "Additional sense: %s", eip->text);
        }
    }
    if (! found) {
        if (asc >= 0x80)
            snprintf(buff, buff_len, "vendor specific ASC=%2x, ASCQ=%2x",
                     asc, ascq);
        else if (ascq >= 0x80)
            snprintf(buff, buff_len, "ASC=%2x, vendor specific qualification "
                     "ASCQ=%2x", asc, ascq);
        else
            snprintf(buff, buff_len, "ASC=%2x, ASCQ=%2x", asc, ascq);
    }
    return buff;
}

const unsigned char *
sg_scsi_sense_desc_find(const unsigned char * sensep, int sense_len,
                        int desc_type)
{
    int add_sen_len, add_len, desc_len, k;
    const unsigned char * descp;

    if ((sense_len < 8) || (0 == (add_sen_len = sensep[7])))
        return NULL;
    if ((sensep[0] < 0x72) || (sensep[0] > 0x73))
        return NULL;
    add_sen_len = (add_sen_len < (sense_len - 8)) ?
                         add_sen_len : (sense_len - 8);
    descp = &sensep[8];
    for (desc_len = 0, k = 0; k < add_sen_len; k += desc_len) {
        descp += desc_len;
        add_len = (k < (add_sen_len - 1)) ? descp[1]: -1;
        desc_len = add_len + 2;
        if (descp[0] == desc_type)
            return descp;
        if (add_len < 0) /* short descriptor ?? */
            break;
    }
    return NULL;
}

int
sg_get_sense_info_fld(const unsigned char * sensep, int sb_len,
                      unsigned long long * info_outp)
{
    int j;
    const unsigned char * ucp;
    unsigned long long ull;

    if (info_outp)
        *info_outp = 0;
    if (sb_len < 7)
        return 0;
    switch (sensep[0] & 0x7f) {
    case 0x70:
    case 0x71:
        if (info_outp)
            *info_outp = (sensep[3] << 24) + (sensep[4] << 16) +
                         (sensep[5] << 8) + sensep[6];
        return (sensep[0] & 0x80) ? 1 : 0;
    case 0x72:
    case 0x73:
        ucp = sg_scsi_sense_desc_find(sensep, sb_len, 0 /* info desc */);
        if (ucp && (0xa == ucp[1])) {
            ull = 0;
            for (j = 0; j < 8; ++j) {
                if (j > 0)
                    ull <<= 8;
                ull |= ucp[4 + j];
            }
            if (info_outp)
                *info_outp = ull;
            return !!(ucp[2] & 0x80);   /* since spc3r23 should be set */
        } else
            return 0;
    default:
        return 0;
    }
}

int
sg_get_sense_progress_fld(const unsigned char * sensep, int sb_len,
                          int * progress_outp)
{
    const unsigned char * ucp;
    int sk;

    if (sb_len < 7)
        return 0;
    switch (sensep[0] & 0x7f) {
    case 0x70:
    case 0x71:
        sk = (sensep[2] & 0xf);
        if ((sb_len < 18) ||
            ((SPC_SK_NO_SENSE != sk) && (SPC_SK_NOT_READY != sk)))
            return 0;
        if (sensep[15] & 0x80) {
            if (progress_outp)
                *progress_outp = (sensep[16] << 8) + sensep[17];
            return 1;
        } else
            return 0;
    case 0x72:
    case 0x73:
        sk = (sensep[1] & 0xf);
        if ((SPC_SK_NO_SENSE != sk) && (SPC_SK_NOT_READY != sk))
            return 0;
        ucp = sg_scsi_sense_desc_find(sensep, sb_len, 2 /* sense key spec. */);
        if (ucp && (0x6 == ucp[1]) && (0x80 & ucp[4])) {
            if (progress_outp)
                *progress_outp = (ucp[5] << 8) + ucp[6];
            return 1;
        } else
            return 0;
    default:
        return 0;
    }
}

static const char * scsi_pdt_strs[] = {
    /* 0 */ "disk",
    "tape",
    "printer",
    "processor",        /* often SAF-TE (seldom scanner) device */
    "write once optical disk",
    /* 5 */ "cd/dvd",
    "scanner",
    "optical memory device",
    "medium changer",
    "communications",
    /* 0xa */ "graphics [0xa]",
    "graphics [0xb]",
    "storage array controller",
    "enclosure services device",
    "simplified direct access device",
    "optical card reader/writer device",
    /* 0x10 */ "bridge controller commands",
    "object based storage",
    "automation/driver interface",
    "0x13", "0x14", "0x15", "0x16", "0x17", "0x18",
    "0x19", "0x1a", "0x1b", "0x1c", "0x1d", 
    "well known logical unit",
    "no physical device on this lu",
};

char *
sg_get_pdt_str(int pdt, int buff_len, char * buff)
{
    if ((pdt < 0) || (pdt > 31))
        snprintf(buff, buff_len, "bad pdt");
    else
        snprintf(buff, buff_len, "%s", scsi_pdt_strs[pdt]);
    return buff;
}

/* Print descriptor format sense descriptors (assumes sense buffer is
   in descriptor format) */
static void
sg_get_sense_descriptors_str(const unsigned char * sense_buffer, int sb_len,
                             int buff_len, char * buff)
{
    int add_sen_len, add_len, desc_len, k, j, sense_key, processed;
    int n, progress;
    const unsigned char * descp;
    char b[256];

    if ((NULL == buff) || (buff_len <= 0))
        return;
    buff[0] = '\0';
    if ((sb_len < 8) || (0 == (add_sen_len = sense_buffer[7])))
        return;
    add_sen_len = (add_sen_len < (sb_len - 8)) ? add_sen_len : (sb_len - 8);
    descp = &sense_buffer[8];
    sense_key = (sense_buffer[1] & 0xf);
    for (desc_len = 0, k = 0; k < add_sen_len; k += desc_len) {
        descp += desc_len;
        add_len = (k < (add_sen_len - 1)) ? descp[1]: -1;
        desc_len = add_len + 2;
        n = 0;
        n += sprintf(b + n, "  Descriptor type: ");
        processed = 1;
        switch (descp[0]) {
        case 0:
            n += sprintf(b + n, "Information\n");
            if ((add_len >= 10) && (0x80 & descp[2])) {
                n += sprintf(b + n, "    0x");
                for (j = 0; j < 8; ++j)
                    n += sprintf(b + n, "%02x", descp[4 + j]);
                n += sprintf(b + n, "\n");
            } else
                processed = 0;
            break;
        case 1:
            n += sprintf(b + n, "Command specific\n");
            if (add_len >= 10) {
                n += sprintf(b + n, "    0x");
                for (j = 0; j < 8; ++j)
                    n += sprintf(b + n, "%02x", descp[4 + j]);
                n += sprintf(b + n, "\n");
            } else
                processed = 0;
            break;
        case 2:
            n += sprintf(b + n, "Sense key specific:");
            switch (sense_key) {
            case SPC_SK_ILLEGAL_REQUEST:
                n += sprintf(b + n, " Field pointer\n");
                if (add_len < 6) {
                    processed = 0;
                    break;
                }
                n += sprintf(b + n, "    Error in %s byte %d",
                        (descp[4] & 0x40) ? "Command" : "Data",
                        (descp[5] << 8) | descp[6]);
                if (descp[4] & 0x08) {
                    n += sprintf(b + n, " bit %d\n", descp[4] & 0x07);
                } else
                    n += sprintf(b + n, "\n");
                break;
            case SPC_SK_HARDWARE_ERROR:
            case SPC_SK_MEDIUM_ERROR:
            case SPC_SK_RECOVERED_ERROR:
                n += sprintf(b + n, " Actual retry count\n");
                if (add_len < 6) {
                    processed = 0;
                    break;
                }
                n += sprintf(b + n, "    0x%02x%02x\n", descp[5],
                        descp[6]);
                break;
            case SPC_SK_NO_SENSE:
            case SPC_SK_NOT_READY:
                n += sprintf(b + n, " Progress indication: ");
                if (add_len < 6) {
                    processed = 0;
                    n += sprintf(b + n, " field too short\n");
                    break;
                }
                progress = (descp[5] << 8) + descp[6];
                n += sprintf(b + n, "%d %%\n", 
                        (progress * 100) / 0x10000);
                break;
            case SPC_SK_COPY_ABORTED:
                n += sprintf(b + n, " Segment pointer\n");
                if (add_len < 6) {
                    processed = 0;
                    break;
                }
                n += sprintf(b + n, "    Relative to start of %s, byte %d",
                        (descp[4] & 0x20) ? "segment descriptor" : 
                                            "parameter list",
                        (descp[5] << 8) | descp[6]);
                if (descp[4] & 0x08)
                    n += sprintf(b + n, " bit %d\n", descp[4] & 0x07);
                else
                    n += sprintf(b + n, "\n");
                break;
            default:
                n += sprintf(b + n, " Sense_key: 0x%x unexpected\n",
                        sense_key);
                processed = 0;
                break;
            }
            break;
        case 3:
            n += sprintf(b + n, "Field replaceable unit\n");
            if (add_len >= 2)
                n += sprintf(b + n, "    code=0x%x\n", descp[3]);
            else
                processed = 0;
            break;
        case 4:
            n += sprintf(b + n, "Stream commands\n");
            if (add_len >= 2) {
                if (descp[3] & 0x80)
                    n += sprintf(b + n, "    FILEMARK");
                if (descp[3] & 0x40)
                    n += sprintf(b + n, "    End Of Medium (EOM)");
                if (descp[3] & 0x20)
                    n += sprintf(b + n, "    Incorrect Length Indicator "
                            "(ILI)");
                n += sprintf(b + n, "\n");
            } else
                processed = 0;
            break;
        case 5:
            n += sprintf(b + n, "Block commands\n");
            if (add_len >= 2)
                n += sprintf(b + n, "    Incorrect Length Indicator "
                        "(ILI) %s\n", (descp[3] & 0x20) ? "set" : "clear");
            else
                processed = 0;
            break;
        case 6:
            n += sprintf(b + n, "OSD object identification\n");
            processed = 0;
            break;
        case 7:
            n += sprintf(b + n, "OSD response integrity check value\n");
            processed = 0;
            break;
        case 8:
            n += sprintf(b + n, "OSD attribute identification\n");
            processed = 0;
            break;
        case 9:
            n += sprintf(b + n, "ATA Return\n");
            if (add_len >= 12) {
                int extended, sector_count;

                extended = descp[2] & 1;
                sector_count = descp[5] + (extended ? (descp[4] << 8) : 0);
                n += sprintf(b + n, "    extended=%d  error=0x%x "
                        " sector_count=0x%x\n", extended, descp[3],
                        sector_count);
                if (extended)
                    n += sprintf(b + n, "    lba=0x%02x%02x%02x%02x%02x%02x\n",
                                 descp[10], descp[8], descp[6],
                                 descp[11], descp[9], descp[7]);
                else
                    n += sprintf(b + n, "    lba=0x%02x%02x%02x\n",
                                 descp[11], descp[9], descp[7]);
                n += sprintf(b + n, "    device=0x%x  status=0x%x\n",
                        descp[12], descp[13]);
            } else
                processed = 0;
            break;
        default:
            n += sprintf(b + n, "Unknown or vendor specific [0x%x]\n",
                    descp[0]);
            processed = 0;
            break;
        }
        if (! processed) {
            if (add_len > 0) {
                n += sprintf(b + n, "    ");
                for (j = 0; (j < add_len) && ((k + j + 2) < add_sen_len);
                     ++j) {
                    if ((j > 0) && (0 == (j % 24)))
                        n += sprintf(b + n, "\n    ");
                    n += sprintf(b + n, "%02x ", descp[j + 2]);
                }
                n += sprintf(b + n, "\n");
            }
        }
        if (add_len < 0)
            n += sprintf(b + n, "    short descriptor\n");
        j = strlen(buff);
        if ((n + j) >= buff_len) {
            strncpy(buff + j, b, buff_len - j);
            buff[buff_len - 1] = '\0';
            break;
        }
        strcpy(buff + j, b);
        if (add_len < 0)
            break;
    }
}

/* Fetch sense information */
void
sg_get_sense_str(const char * leadin, const unsigned char * sense_buffer,
                 int sb_len, int raw_sinfo, int buff_len, char * buff)
{
    int len, valid, progress, n, r;
    unsigned int info;
    int descriptor_format = 0;
    const char * error = NULL;
    char error_buff[64];
    char b[256];
    struct sg_scsi_sense_hdr ssh;

    if ((NULL == buff) || (buff_len <= 0))
        return;
    buff[buff_len - 1] = '\0';
    --buff_len;
    n = 0;
    if (sb_len < 1) {
            snprintf(buff, buff_len, "sense buffer empty\n");
            return;
    }
    if (leadin) {
        n += snprintf(buff + n, buff_len - n, "%s: ", leadin);
        if (n >= buff_len)
            return;
    }
    len = sb_len;
    if (sg_scsi_normalize_sense(sense_buffer, sb_len, &ssh)) {
        switch (ssh.response_code) {
        case 0x70:      /* fixed, current */
            error = "Fixed format, current"; 
            len = (sb_len > 7) ? (sense_buffer[7] + 8) : sb_len;
            len = (len > sb_len) ? sb_len : len;
            break;
        case 0x71:      /* fixed, deferred */
            /* error related to a previous command */
            error = "Fixed format, <<<deferred>>>";
            len = (sb_len > 7) ? (sense_buffer[7] + 8) : sb_len;
            len = (len > sb_len) ? sb_len : len;
            break;
        case 0x72:      /* descriptor, current */
            descriptor_format = 1;
            error = "Descriptor format, current";
            break;
        case 0x73:      /* descriptor, deferred */
            descriptor_format = 1;
            error = "Descriptor format, <<<deferred>>>";
            break;
        case 0x0:
            error = "Response code: 0x0 (?)";
            break;
        default:
            snprintf(error_buff, sizeof(error_buff), 
                     "Unknown response code: 0x%x", ssh.response_code);
            error = error_buff;
            break;
        }
        n += snprintf(buff + n, buff_len - n, " %s;  Sense key: %s\n ",
                      error, sense_key_desc[ssh.sense_key]);
        if (n >= buff_len)
            return;
        if (descriptor_format) {
            n += snprintf(buff + n, buff_len - n, "%s\n",
                          sg_get_asc_ascq_str(ssh.asc, ssh.ascq,
                                              sizeof(b), b));
            if (n >= buff_len)
                return;
            sg_get_sense_descriptors_str(sense_buffer, len, buff_len - n,
                                         buff + n);
            n = strlen(buff);
            if (n >= buff_len)
                return;
        } else if (len > 2) {   /* fixed format */
            if (len > 12) {
                n += snprintf(buff + n, buff_len - n, "%s\n",
                              sg_get_asc_ascq_str(ssh.asc, ssh.ascq,
                                                  sizeof(b), b));
                if (n >= buff_len)
                    return;
            }
            r = 0;
            valid = sense_buffer[0] & 0x80;
            if (len > 6) {
                info = (unsigned int)((sense_buffer[3] << 24) |
                        (sense_buffer[4] << 16) | (sense_buffer[5] << 8) |
                        sense_buffer[6]);
                if (valid)
                    r += sprintf(b + r, "  Info fld=0x%x [%u] ", info,
                                 info);
                else if (info > 0)
                    r += sprintf(b + r, "  Valid=0, Info fld=0x%x [%u] ",
                                 info, info);
            } else
                info = 0;
            if (sense_buffer[2] & 0xe0) {
                if (sense_buffer[2] & 0x80)
                   r += sprintf(b + r, " FMK");
                            /* current command has read a filemark */
                if (sense_buffer[2] & 0x40)
                   r += sprintf(b + r, " EOM");
                            /* end-of-medium condition exists */
                if (sense_buffer[2] & 0x20)
                   r += sprintf(b + r, " ILI");
                            /* incorrect block length requested */
                r += sprintf(b + r, "\n");
            } else if (valid || (info > 0))
                r += sprintf(b + r, "\n");
            if ((len >= 14) && sense_buffer[14])
                r += sprintf(b + r, "  Field replaceable unit code: "
                             "%d\n", sense_buffer[14]);
            if ((len >= 18) && (sense_buffer[15] & 0x80)) {
                /* sense key specific decoding */
                switch (ssh.sense_key) {
                case SPC_SK_ILLEGAL_REQUEST:
                    r += sprintf(b + r, "  Sense Key Specific: Error in "
                                 "%s byte %d", (sense_buffer[15] & 0x40) ?
                                                 "Command" : "Data",
                                 (sense_buffer[16] << 8) | sense_buffer[17]);
                    if (sense_buffer[15] & 0x08)
                        r += sprintf(b + r, " bit %d\n",
                                     sense_buffer[15] & 0x07);
                    else
                        r += sprintf(b + r, "\n");
                    break;
                case SPC_SK_NO_SENSE:
                case SPC_SK_NOT_READY:
                    progress = (sense_buffer[16] << 8) + sense_buffer[17];
                    r += sprintf(b + r, "  Progress indication: %d %%\n",
                                (progress * 100) / 0x10000);
                    break;
                case SPC_SK_HARDWARE_ERROR:
                case SPC_SK_MEDIUM_ERROR:
                case SPC_SK_RECOVERED_ERROR:
                    r += sprintf(b + r, "  Actual retry count: "
                                 "0x%02x%02x\n", sense_buffer[16],
                                 sense_buffer[17]);
                    break;
                case SPC_SK_COPY_ABORTED:
                    r += sprintf(b + r, "  Segment pointer: ");
                    r += sprintf(b + r, "Relative to start of %s, byte %d",
                                 (sense_buffer[15] & 0x20) ?
                                     "segment descriptor" : "parameter list",
                                 (sense_buffer[16] << 8) + sense_buffer[17]);
                    if (sense_buffer[15] & 0x08)
                        r += sprintf(b + r, " bit %d\n",
                                     sense_buffer[15] & 0x07);
                    else
                        r += sprintf(b + r, "\n");
                    break;
                default:
                    r += sprintf(b + r, "  Sense_key: 0x%x unexpected\n",
                                 ssh.sense_key);
                    break;
                }
            }
            if (r > 0) {
                n += snprintf(buff + n, buff_len - n, "%s", b);
                if (n >= buff_len)
                    return;
            }
        } else { 
            n += snprintf(buff + n, buff_len - n, " fixed descriptor "
                          "length too short, len=%d\n", len);
            if (n >= buff_len)
                return;
        }
    } else {    /* non-extended SCSI-1 sense data ?? */
        if (sb_len < 4) {
            n += snprintf(buff + n, buff_len - n, "sense buffer too short "
                          "(4 byte minimum)\n");
            return;
        }
        r = 0;
        r += sprintf(b + r, "Probably uninitialized data.\n  Try to view "
                     "as SCSI-1 non-extended sense:\n");
        r += sprintf(b + r, "  AdValid=%d  Error class=%d  Error code=%d\n",
                     !!(sense_buffer[0] & 0x80),
                     ((sense_buffer[0] >> 4) & 0x7),
                     (sense_buffer[0] & 0xf));
        if (sense_buffer[0] & 0x80)
            r += sprintf(b + r, "  lba=0x%x\n",
                         ((sense_buffer[1] & 0x1f) << 16) +
                         (sense_buffer[2] << 8) + sense_buffer[3]);
        n += snprintf(buff + n, buff_len - n, "%s\n", b);
        if (n >= buff_len)
            return;
        len = sb_len;
        if (len > 32)
            len = 32;   /* trim in case there is a lot of rubbish */
    }
    if (raw_sinfo) {
        n += snprintf(buff + n, buff_len - n, " Raw sense data (in hex):\n");
        if (n >= buff_len)
            return;
        dStrHexErr((const char *)sense_buffer, len, buff_len - n, buff + n);
    }
}

/* Print sense information */
void
sg_print_sense(const char * leadin, const unsigned char * sense_buffer,
               int sb_len, int raw_sinfo)
{
    char b[1024];

    sg_get_sense_str(leadin, sense_buffer, sb_len, raw_sinfo, sizeof(b), b);
    if (NULL == sg_warnings_strm)
        sg_warnings_strm = stderr;
    fprintf(sg_warnings_strm, "%s", b);
}

int
sg_scsi_normalize_sense(const unsigned char * sensep, int sb_len,
                        struct sg_scsi_sense_hdr * sshp)
{
    if (sshp)
        memset(sshp, 0, sizeof(struct sg_scsi_sense_hdr));
    if ((NULL == sensep) || (0 == sb_len) || (0x70 != (0x70 & sensep[0])))
        return 0;
    if (sshp) {
        sshp->response_code = (0x7f & sensep[0]);
        if (sshp->response_code >= 0x72) {  /* descriptor format */
            if (sb_len > 1)
                sshp->sense_key = (0xf & sensep[1]);
            if (sb_len > 2)
                sshp->asc = sensep[2];
            if (sb_len > 3)
                sshp->ascq = sensep[3];
            if (sb_len > 7)
                sshp->additional_length = sensep[7];
        } else {                              /* fixed format */
            if (sb_len > 2)
                sshp->sense_key = (0xf & sensep[2]);
            if (sb_len > 7) {
                sb_len = (sb_len < (sensep[7] + 8)) ? sb_len : 
                                                      (sensep[7] + 8);
                if (sb_len > 12)
                    sshp->asc = sensep[12];
                if (sb_len > 13)
                    sshp->ascq = sensep[13];
            }
        }
    }
    return 1;
}

int
sg_err_category_sense(const unsigned char * sense_buffer, int sb_len)
{
    struct sg_scsi_sense_hdr ssh;

    if ((sense_buffer && (sb_len > 2)) &&
        (sg_scsi_normalize_sense(sense_buffer, sb_len, &ssh))) {
        switch (ssh.sense_key) {
        case SPC_SK_NO_SENSE:
            return SG_LIB_CAT_NO_SENSE;
        case SPC_SK_RECOVERED_ERROR:
            return SG_LIB_CAT_RECOVERED;
        case SPC_SK_NOT_READY:
            return SG_LIB_CAT_NOT_READY;
        case SPC_SK_MEDIUM_ERROR: 
        case SPC_SK_HARDWARE_ERROR: 
        case SPC_SK_BLANK_CHECK: 
            return SG_LIB_CAT_MEDIUM_HARD;
        case SPC_SK_UNIT_ATTENTION:
            return SG_LIB_CAT_UNIT_ATTENTION;
            /* used to return SG_LIB_CAT_MEDIA_CHANGED when ssh.asc==0x28 */
        case SPC_SK_ILLEGAL_REQUEST:
            if ((0x20 == ssh.asc) && (0x0 == ssh.ascq))
                return SG_LIB_CAT_INVALID_OP;
            else
                return SG_LIB_CAT_ILLEGAL_REQ;
            break;
        case SPC_SK_ABORTED_COMMAND:
            return SG_LIB_CAT_ABORTED_COMMAND;
        }
    }
    return SG_LIB_CAT_SENSE;
}

/* gives wrong answer for variable length command (opcode=0x7f) */
int
sg_get_command_size(unsigned char opcode)
{
    switch ((opcode >> 5) & 0x7) {
    case 0:
        return 6;
    case 1: case 2: case 6: case 7:
        return 10;
    case 3: case 5:
        return 12;
        break;
    case 4:
        return 16;
    default:
        return 10;
    }
}

void
sg_get_command_name(const unsigned char * cmdp, int peri_type, int buff_len,
                    char * buff)
{
    int service_action;

    if ((NULL == buff) || (buff_len < 1))
        return;
    if (NULL == cmdp) {
        strncpy(buff, "<null> command pointer", buff_len);
        return;
    }
    service_action = (SG_VARIABLE_LENGTH_CMD == cmdp[0]) ?
                     (cmdp[1] & 0x1f) : ((cmdp[8] << 8) | cmdp[9]);
    sg_get_opcode_sa_name(cmdp[0], service_action, peri_type, buff_len, buff);
}


void
sg_get_opcode_sa_name(unsigned char cmd_byte0, int service_action,
                      int peri_type, int buff_len, char * buff)
{
    const struct value_name_t * vnp;

    if ((NULL == buff) || (buff_len < 1))
        return;
    switch ((int)cmd_byte0) {
    case SG_VARIABLE_LENGTH_CMD:
        vnp = get_value_name(variable_length_arr, VARIABLE_LENGTH_SZ,
                             service_action, peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Variable length service action=0x%x",
                     service_action);
        break;
    case SG_MAINTENANCE_IN:
        vnp = get_value_name(maint_in_arr, MAINT_IN_SZ, service_action, 
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Maintenance in service action=0x%x",
                     service_action);
        break;
    case SG_MAINTENANCE_OUT:
        vnp = get_value_name(maint_out_arr, MAINT_OUT_SZ, service_action, 
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Maintenance out service action=0x%x",
                     service_action);
        break;
    case SG_SERVICE_ACTION_IN_12:
        vnp = get_value_name(serv_in12_arr, SERV_IN12_SZ, service_action, 
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Service action in(12)=0x%x",
                     service_action);
        break;
    case SG_SERVICE_ACTION_OUT_12:
        vnp = get_value_name(serv_out12_arr, SERV_OUT12_SZ, service_action,
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Service action out(12)=0x%x",
                     service_action);
        break;
    case SG_SERVICE_ACTION_IN_16:
        vnp = get_value_name(serv_in16_arr, SERV_IN16_SZ, service_action, 
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Service action in(16)=0x%x",
                     service_action);
        break;
    case SG_SERVICE_ACTION_OUT_16:
        vnp = get_value_name(serv_out16_arr, SERV_OUT16_SZ, service_action,
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Service action out(16)=0x%x",
                     service_action);
        break;
    default:
        sg_get_opcode_name(cmd_byte0, peri_type, buff_len, buff);
        break;
    }
}

void
sg_get_opcode_name(unsigned char cmd_byte0, int peri_type, int buff_len,
                   char * buff)
{
    const struct value_name_t * vnp;
    int grp;

    if ((NULL == buff) || (buff_len < 1))
        return;
    if (SG_VARIABLE_LENGTH_CMD == cmd_byte0) {
        strncpy(buff, "Variable length", buff_len);
        return;
    }
    grp = (cmd_byte0 >> 5) & 0x7;
    switch (grp) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
        vnp = get_value_name(normal_opcodes, NORMAL_OPCODES_SZ, cmd_byte0,
                             peri_type);
        if (vnp)
            strncpy(buff, vnp->name, buff_len);
        else
            snprintf(buff, buff_len, "Opcode=0x%x", (int)cmd_byte0);
        break;
    case 3:
        snprintf(buff, buff_len, "Reserved [0x%x]", (int)cmd_byte0);
        break;
    case 6:
    case 7:
        snprintf(buff, buff_len, "Vendor specific [0x%x]", (int)cmd_byte0);
        break;
    default:
        snprintf(buff, buff_len, "Opcode=0x%x", (int)cmd_byte0);
        break;
    }
}

int
sg_vpd_dev_id_iter(const unsigned char * initial_desig_desc, int page_len,
                   int * off, int m_assoc, int m_desig_type, int m_code_set)
{
    const unsigned char * ucp;
    int k, c_set, assoc, desig_type;

    for (k = *off, ucp = initial_desig_desc ; (k + 3) < page_len; ) {
        k = (k < 0) ? 0 : (k + ucp[k + 3] + 4);
        if ((k + 4) > page_len)
            break;
        c_set = (ucp[k] & 0xf);
        if ((m_code_set >= 0) && (m_code_set != c_set))
            continue;
        assoc = ((ucp[k + 1] >> 4) & 0x3);
        if ((m_assoc >= 0) && (m_assoc != assoc))
            continue;
        desig_type = (ucp[k + 1] & 0xf);
        if ((m_desig_type >= 0) && (m_desig_type != desig_type))
            continue;
        *off = k;
        return 0;
    }
    return (k == page_len) ? -1 : -2;
}


/* safe_strerror() contributed by Clayton Weaver <cgweav at email dot com>
   Allows for situation in which strerror() is given a wild value (or the
   C library is incomplete) and returns NULL. Still not thread safe.
 */

static char safe_errbuf[64] = {'u', 'n', 'k', 'n', 'o', 'w', 'n', ' ',
                               'e', 'r', 'r', 'n', 'o', ':', ' ', 0};

char *
safe_strerror(int errnum)
{
    size_t len;
    char * errstr;
  
    if (errnum < 0)
        errnum = -errnum;
    errstr = strerror(errnum);
    if (NULL == errstr) {
        len = strlen(safe_errbuf);
        snprintf(safe_errbuf + len, sizeof(safe_errbuf) - len, "%i", errnum);
        safe_errbuf[sizeof(safe_errbuf) - 1] = '\0';  /* bombproof */
        return safe_errbuf;
    }
    return errstr;
}


/* Note the ASCII-hex output goes to stdout. [Most other output from functions
   in this file go to sg_warnings_strm (default stderr).] 
   'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 16 ASCII-hex bytes
       = 0     in addition, the bytes are listed in ASCII to the right
       < 0     only the ASCII-hex bytes are listed (i.e. without address) */
void
dStrHex(const char* str, int len, int no_ascii)
{
    const char* p = str;
    unsigned char c;
    char buff[82];
    int a = 0;
    const int bpstart = 5;
    const int cpstart = 60;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k;

    if (len <= 0)
        return;
    memset(buff, ' ', 80);
    buff[80] = '\0';
    if (no_ascii < 0) {
        for (k = 0; k < len; k++) {
            c = *p++;
            bpos += 3;
            if (bpos == (bpstart + (9 * 3)))
                bpos++;
            sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
            buff[bpos + 2] = ' ';
            if ((k > 0) && (0 == ((k + 1) % 16))) {
                printf("%.60s\n", buff);
                bpos = bpstart;
                memset(buff, ' ', 80);
            }
        }
        if (bpos > bpstart)
            printf("%.60s\n", buff);
        return;
    }
    /* no_ascii>=0, start each line with address (offset) */
    k = sprintf(buff + 1, "%.2x", a);
    buff[k + 1] = ' ';

    for (i = 0; i < len; i++) {
        c = *p++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if (no_ascii)
            buff[cpos++] = ' ';
        else {
            if ((c < ' ') || (c >= 0x7f))
                c = '.';
            buff[cpos++] = c;
        }
        if (cpos > (cpstart + 15)) {
            printf("%.76s\n", buff);
            bpos = bpstart;
            cpos = cpstart;
            a += 16;
            memset(buff, ' ', 80);
            k = sprintf(buff + 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart)
        printf("%.76s\n", buff);
}

/* Output to ASCII-Hex bytes to 'b' not to exceed 'b_len' characters.
 * 16 bytes per line with an extra space between the 8th and 9th bytes */
static void
dStrHexErr(const char* str, int len, int b_len, char * b)
{
    const char * p = str;
    unsigned char c;
    char buff[82];
    const int bpstart = 5;
    int bpos = bpstart;
    int k, n;

    if (len <= 0)
        return;
    n = 0;
    memset(buff, ' ', 80);
    buff[80] = '\0';
    for (k = 0; k < len; k++) {
        c = *p++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if ((k > 0) && (0 == ((k + 1) % 16))) {
            n += snprintf(b + n, b_len - n, "%.60s\n", buff);
            if (n >= b_len)
                return;
            bpos = bpstart;
            memset(buff, ' ', 80);
        }
    }
    if (bpos > bpstart)
        n += snprintf(b + n, b_len - n, "%.60s\n", buff);
    return;
}

/* Returns 1 when executed on big endian machine; else returns 0.
   Useful for displaying ATA identify words (which need swapping on a
   big endian machine). */
int
sg_is_big_endian()
{
    union u_t {
        unsigned short s;
        unsigned char c[sizeof(unsigned short)];
    } u;

    u.s = 0x0102;
    return (u.c[0] == 0x01);     /* The lowest address contains
                                    the most significant byte */
}

static unsigned short
swapb_ushort(unsigned short u)
{
    unsigned short r;

    r = (u >> 8) & 0xff;
    r |= ((u & 0xff) << 8);
    return r;
}

/* Note the ASCII-hex output goes to stdout. [Most other output from functions
   in this file go to sg_warnings_strm (default stderr).] 
   'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 8 ASCII-hex 16 bit words
       = 0     in addition, the ASCI bytes pairs are listed to the right
       = -1    only the ASCII-hex words are listed (i.e. without address)
       = -2    only the ASCII-hex words, formatted for "hdparm --Istdin"
       < -2    same as -1
   If 'swapb' non-zero then bytes in each word swapped. Needs to be set
   for ATA IDENTIFY DEVICE response on big-endian machines. */
void
dWordHex(const unsigned short* words, int num, int no_ascii, int swapb)
{
    const unsigned short * p = words;
    unsigned short c;
    char buff[82];
    unsigned char upp, low;
    int a = 0;
    const int bpstart = 3;
    const int cpstart = 52;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k;

    if (num <= 0)
        return;
    memset(buff, ' ', 80);
    buff[80] = '\0';
    if (no_ascii < 0) {
        for (k = 0; k < num; k++) {
            c = *p++;
            if (swapb)
                c = swapb_ushort(c);
            bpos += 5;
            sprintf(&buff[bpos], "%.4x", (unsigned int)c);
            buff[bpos + 4] = ' ';
            if ((k > 0) && (0 == ((k + 1) % 8))) {
                if (-2 == no_ascii)
                    printf("%.39s\n", buff +8);
                else
                    printf("%.47s\n", buff);
                bpos = bpstart;
                memset(buff, ' ', 80);
            }
        }
        if (bpos > bpstart) {
            if (-2 == no_ascii)
                printf("%.39s\n", buff +8);
            else
                printf("%.47s\n", buff);
        }
        return;
    }
    /* no_ascii>=0, start each line with address (offset) */
    k = sprintf(buff + 1, "%.2x", a);
    buff[k + 1] = ' ';

    for (i = 0; i < num; i++) {
        c = *p++;
        if (swapb)
            c = swapb_ushort(c);
        bpos += 5;
        sprintf(&buff[bpos], "%.4x", (unsigned int)c);
        buff[bpos + 4] = ' ';
        if (no_ascii) {
            buff[cpos++] = ' ';
            buff[cpos++] = ' ';
            buff[cpos++] = ' ';
        } else {
            upp = (c >> 8) & 0xff;
            low = c & 0xff;
            if ((upp < 0x20) || (upp >= 0x7f))
                upp = '.';
            buff[cpos++] = upp;
            if ((low < 0x20) || (low >= 0x7f))
                low = '.';
            buff[cpos++] = low;
            buff[cpos++] = ' ';
        }
        if (cpos > (cpstart + 23)) {
            printf("%.76s\n", buff);
            bpos = bpstart;
            cpos = cpstart;
            a += 8;
            memset(buff, ' ', 80);
            k = sprintf(buff + 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart)
        printf("%.76s\n", buff);
}

/* If the number in 'buf' can be decoded or the multiplier is unknown
   then -1 is returned. Accepts a hex prefix (0x or 0X) or a decimal
   multiplier suffix (as per GNU's dd (since 2002: SI and IEC 60027-2)).
   Main (SI) multipliers supported: K, M, G. */
int
sg_get_num(const char * buf)
{
    int res, num, n, len;
    unsigned int unum;
    char * cp;
    char c = 'c';
    char c2, c3;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d%c%c%c", &num, &c, &c2, &c3);
    if (res < 1)
        return -1LL;
    else if (1 == res)
        return num;
    else {
        if (res > 2)
            c2 = toupper(c2);
        if (res > 3)
            c3 = toupper(c3);
        switch (toupper(c)) {
        case 'C':
            return num;
        case 'W':
            return num * 2;
        case 'B':
            return num * 512;
        case 'K':
            if (2 == res)
                return num * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1024;
            return -1;
        case 'M':
            if (2 == res)
                return num * 1048576;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1048576;
            return -1;
        case 'G':
            if (2 == res)
                return num * 1073741824;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1073741824;
            return -1;
        case 'X':
            cp = strchr(buf, 'x');
            if (NULL == cp)
                cp = strchr(buf, 'X');
            if (cp) {
                n = sg_get_num(cp + 1);
                if (-1 != n)
                    return num * n;
            }
            return -1;
        default:
            if (NULL == sg_warnings_strm)
                sg_warnings_strm = stderr;
            fprintf(sg_warnings_strm, "unrecognized multiplier\n");
            return -1;
        }
    }
}

/* If the number in 'buf' can not be decoded then -1 is returned. Accepts a
   hex prefix (0x or 0X) or a 'h' (or 'H') suffix; otherwise decimal is
   assumed. Does not accept multipliers. Accept a comma (","), a whitespace
   or newline as terminator.  */
int
sg_get_num_nomult(const char * buf)
{
    int res, len, num;
    unsigned int unum;
    const char * commap;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    commap = strchr(buf + 1, ',');
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else if (commap && ('H' == toupper(*(commap - 1)))) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else if ((NULL == commap) && ('H' == toupper(buf[len - 1]))) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d", &num);
    if (1 == res)
        return num;
    else
        return -1;
}

/* If the number in 'buf' can be decoded or the multiplier is unknown
   then -1LL is returned. Accepts a hex prefix (0x or 0X) or a decimal
   multiplier suffix (as per GNU's dd (since 2002: SI and IEC 60027-2)).
   Main (SI) multipliers supported: K, M, G, T, P. */
long long
sg_get_llnum(const char * buf)
{
    int res, len;
    long long num, ll;
    unsigned long long unum;
    char * cp;
    char c = 'c';
    char c2, c3;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1LL;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%" SCNx64 "", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%" SCNx64 "", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%" SCNd64 "%c%c%c", &num, &c, &c2, &c3);
    if (res < 1)
        return -1LL;
    else if (1 == res)
        return num;
    else {
        if (res > 2)
            c2 = toupper(c2);
        if (res > 3)
            c3 = toupper(c3);
        switch (toupper(c)) {
        case 'C':
            return num;
        case 'W':
            return num * 2;
        case 'B':
            return num * 512;
        case 'K':
            if (2 == res)
                return num * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1024;
            return -1LL;
        case 'M':
            if (2 == res)
                return num * 1048576;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1048576;
            return -1LL;
        case 'G':
            if (2 == res)
                return num * 1073741824;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1073741824;
            return -1LL;
        case 'T':
            if (2 == res)
                return num * 1099511627776LL;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000000LL;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1099511627776LL;
            return -1LL;
        case 'P':
            if (2 == res)
                return num * 1099511627776LL * 1024;
            if (('B' == c2) || ('D' == c2))
                return num * 1000000000000LL * 1000;
            if (('I' == c2) && (4 == res) && ('B' == c3))
                return num * 1099511627776LL * 1024;
            return -1LL;
        case 'X':
            cp = strchr(buf, 'x');
            if (NULL == cp)
                cp = strchr(buf, 'X');
            if (cp) {
                ll = sg_get_llnum(cp + 1);
                if (-1LL != ll)
                    return num * ll;
            }
            return -1LL;
        default:
            if (NULL == sg_warnings_strm)
                sg_warnings_strm = stderr;
            fprintf(sg_warnings_strm, "unrecognized multiplier\n");
            return -1LL;
        }
    }
}

/* Extract character sequence from ATA words as in the model string
   in a IDENTIFY DEVICE response. Returns number of characters
   written to 'ochars' before 0 character is found or 'num' words
   are processed. */
int
sg_ata_get_chars(const unsigned short * word_arr, int start_word,
                 int num_words, int is_big_endian, char * ochars)
{
    int k;
    unsigned short s;
    char a, b;
    char * op = ochars;

    for (k = start_word; k < (start_word + num_words); ++k) {
        s = word_arr[k];
        if (is_big_endian) {
            a = s & 0xff;
            b = (s >> 8) & 0xff;
        } else {
            a = (s >> 8) & 0xff;
            b = s & 0xff;
        }
        if (a == 0)
            break;
        *op++ = a;
        if (b == 0)
            break;
        *op++ = b;
    }
    return op - ochars;
}

const char *
sg_lib_version()
{
    return version_str;
}
