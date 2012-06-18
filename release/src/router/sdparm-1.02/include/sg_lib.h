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
#ifndef SG_LIB_H
#define SG_LIB_H

/*
 * Copyright (c) 2004-2007 Douglas Gilbert.
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
 *
 * On 5th October 2004 a FreeBSD license was added to this file.
 * The intention is to keep this file and the related sg_lib.c file
 * as open source and encourage their unencumbered use.
 *
 * Current version number is in the sg_lib.c file.
 */


/*
 * This header file contains defines and function declarations that may
 * be useful to applications that communicate with devices that use a
 * SCSI command set. These command sets have names like SPC-4, SBC-3,
 * SSC-3, SES-2 and draft standards defining them can be found at
 * http://www.t10.org . Virtually all devices in the Linux SCSI subsystem
 * utilize SCSI command sets. Many devices in other Linux device subsystems
 * utilize SCSI command sets either natively or via emulation (e.g. a
 * parallel ATA disk in a USB enclosure).
 */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SAM_STAT_GOOD
/* The SCSI status codes as found in SAM-4 at www.t10.org */
#define SAM_STAT_GOOD 0x0
#define SAM_STAT_CHECK_CONDITION 0x2
#define SAM_STAT_CONDITION_MET 0x4
#define SAM_STAT_BUSY 0x8
#define SAM_STAT_INTERMEDIATE 0x10              /* obsolete in SAM-4 */
#define SAM_STAT_INTERMEDIATE_CONDITION_MET 0x14  /* obsolete in SAM-4 */
#define SAM_STAT_RESERVATION_CONFLICT 0x18
#define SAM_STAT_COMMAND_TERMINATED 0x22        /* obsolete in SAM-3 */
#define SAM_STAT_TASK_SET_FULL 0x28
#define SAM_STAT_ACA_ACTIVE 0x30
#define SAM_STAT_TASK_ABORTED 0x40
#endif

/* The SCSI sense key codes as found in SPC-4 at www.t10.org */
#define SPC_SK_NO_SENSE 0x0
#define SPC_SK_RECOVERED_ERROR 0x1
#define SPC_SK_NOT_READY 0x2
#define SPC_SK_MEDIUM_ERROR 0x3
#define SPC_SK_HARDWARE_ERROR 0x4
#define SPC_SK_ILLEGAL_REQUEST 0x5
#define SPC_SK_UNIT_ATTENTION 0x6
#define SPC_SK_DATA_PROTECT 0x7
#define SPC_SK_BLANK_CHECK 0x8
#define SPC_SK_COPY_ABORTED 0xa
#define SPC_SK_ABORTED_COMMAND 0xb
#define SPC_SK_VOLUME_OVERFLOW 0xd
#define SPC_SK_MISCOMPARE 0xe

/* Transport protocol identifiers */
#define TPROTO_FCP 0
#define TPROTO_SPI 1
#define TPROTO_SSA 2
#define TPROTO_1394 3
#define TPROTO_SRP 4
#define TPROTO_ISCSI 5
#define TPROTO_SAS 6
#define TPROTO_ADT 7
#define TPROTO_ATA 8
#define TPROTO_NONE 0xf


/* Returns length of SCSI command given the opcode (first byte). 
   Yields the wrong answer for variable length commands (opcode=0x7f)
   and potentially some vendor specific commands. */
extern int sg_get_command_size(unsigned char cdb_byte0);

/* Command name given pointer to the cdb. Certain command names
   depend on peripheral type (give 0 if unknown). Places command
   name into buff and will write no more than buff_len bytes. */
extern void sg_get_command_name(const unsigned char * cdbp, int peri_type,
                                int buff_len, char * buff);

/* Command name given only the first byte (byte 0) of a cdb and
 * peripheral type. */
extern void sg_get_opcode_name(unsigned char cdb_byte0, int peri_type,
                               int buff_len, char * buff);

/* Command name given opcode (byte 0), service action and peripheral type.
   If no service action give 0, if unknown peripheral type give 0. */
extern void sg_get_opcode_sa_name(unsigned char cdb_byte0, int service_action,
                                  int peri_type, int buff_len, char * buff);

/* Fetch scsi status string. */
extern void sg_get_scsi_status_str(int scsi_status, int buff_len, char * buff);

/* This is a slightly stretched SCSI sense "descriptor" format header.
   The addition is to allow the 0x70 and 0x71 response codes. The idea
   is to place the salient data of both "fixed" and "descriptor" sense
   format into one structure to ease application processing.
   The original sense buffer should be kept around for those cases
   in which more information is required (e.g. the LBA of a MEDIUM ERROR). */
struct sg_scsi_sense_hdr {
    unsigned char response_code; /* permit: 0x0, 0x70, 0x71, 0x72, 0x73 */
    unsigned char sense_key;
    unsigned char asc;
    unsigned char ascq;
    unsigned char byte4;
    unsigned char byte5;
    unsigned char byte6;
    unsigned char additional_length;
};

/* Maps the salient data from a sense buffer which is in either fixed or
   descriptor format into a structure mimicking a descriptor format
   header (i.e. the first 8 bytes of sense descriptor format).
   If zero response code returns 0. Otherwise returns 1 and if 'sshp' is
   non-NULL then zero all fields and then set the appropriate fields in
   that structure. sshp::additional_length is always 0 for response
   codes 0x70 and 0x71 (fixed format). */
extern int sg_scsi_normalize_sense(const unsigned char * sensep, 
                                   int sense_len,
                                   struct sg_scsi_sense_hdr * sshp);

/* Attempt to find the first SCSI sense data descriptor that matches the
   given 'desc_type'. If found return pointer to start of sense data
   descriptor; otherwise (including fixed format sense data) returns NULL. */
extern const unsigned char * sg_scsi_sense_desc_find(
                const unsigned char * sensep, int sense_len, int desc_type);

/* Yield string associated with sense_key value. Returns 'buff'. */
extern char * sg_get_sense_key_str(int sense_key, int buff_len, char * buff);

/* Yield string associated with ASC/ASCQ values. Returns 'buff'. */
extern char * sg_get_asc_ascq_str(int asc, int ascq, int buff_len,
                                  char * buff);

/* Returns 1 if valid bit set, 0 if valid bit clear. Irrespective the
   information field is written out via 'info_outp' (except when it is
   NULL). Handles both fixed and descriptor sense formats. */
extern int sg_get_sense_info_fld(const unsigned char * sensep, int sb_len,
                                 unsigned long long * info_outp);

/* Returns 1 if sense key is NO_SENSE or NOT_READY and SKSV is set. Places
   progress field from sense data where progress_outp points. If progress
   field is not available returns 0. Handles both fixed and descriptor
   sense formats. N.B. App should multiply by 100 and divide by 65536
   to get percentage completion from given value. */
extern int sg_get_sense_progress_fld(const unsigned char * sensep,
                                     int sb_len, int * progress_outp);

/* Closely related to sg_print_sense(). Puts decode sense data in 'buff'.
   Usually multiline with multiple '\n' including one trailing. If
   'raw_sinfo' set appends sense buffer in hex. */
extern void sg_get_sense_str(const char * leadin,
                             const unsigned char * sense_buffer, int sb_len,
                             int raw_sinfo, int buff_len, char * buff);

/* Yield string associated with peripheral device type (pdt). Returns
   'buff'. If 'pdt' out of range yields "bad pdt" string. */
extern char * sg_get_pdt_str(int pdt, int buff_len, char * buff);

extern FILE * sg_warnings_strm;

extern void sg_set_warnings_strm(FILE * warnings_strm);

/* The following "print" functions send ACSII to 'sg_warnings_strm' file
   descriptor (default value is stderr) */
extern void sg_print_command(const unsigned char * command);
extern void sg_print_sense(const char * leadin,
                           const unsigned char * sense_buffer, int sb_len,
                           int raw_info);
extern void sg_print_scsi_status(int scsi_status);

/* Utilities can use these process status values for syntax errors and
   file (device node) problems (e.g. not found or permissions). */
#define SG_LIB_SYNTAX_ERROR 1
#define SG_LIB_FILE_ERROR 15

/* The sg_err_category_sense() function returns one of the following.
   These may be used as process status values (on exit). Notice that
   some of the lower values correspond to SCSI sense key values. */
#define SG_LIB_CAT_CLEAN 0      /* No errors or other information */
/* Value 1 left unused for utilities to use SG_LIB_SYNTAX_ERROR */
#define SG_LIB_CAT_NOT_READY 2  /* interpreted from sense buffer */
                                /*       [sk,asc,ascq: 0x2,*,*] */
#define SG_LIB_CAT_MEDIUM_HARD 3 /* medium or hardware error, blank check */
                                /*       [sk,asc,ascq: 0x3/0x4/0x8,*,*] */
#define SG_LIB_CAT_ILLEGAL_REQ 5 /* Illegal request (other than invalid */
                                /* opcode):   [sk,asc,ascq: 0x5,*,*] */
#define SG_LIB_CAT_UNIT_ATTENTION 6 /* interpreted from sense buffer */
                                /*       [sk,asc,ascq: 0x6,*,*] */
        /* was SG_LIB_CAT_MEDIA_CHANGED earlier [sk,asc,ascq: 0x6,0x28,*] */
#define SG_LIB_CAT_INVALID_OP 9 /* (Illegal request,) Invalid opcode: */
                                /*       [sk,asc,ascq: 0x5,0x20,0x0] */
#define SG_LIB_CAT_ABORTED_COMMAND 11 /* interpreted from sense buffer */
                                /*       [sk,asc,ascq: 0xb,*,*] */
#define SG_LIB_CAT_NO_SENSE 20  /* sense data with key of "no sense" */
                                /*       [sk,asc,ascq: 0x0,*,*] */
#define SG_LIB_CAT_RECOVERED 21 /* Successful command after recovered err */
                                /*       [sk,asc,ascq: 0x1,*,*] */
#define SG_LIB_CAT_MALFORMED 97 /* Response to SCSI command malformed */
#define SG_LIB_CAT_SENSE 98     /* Something else is in the sense buffer */
#define SG_LIB_CAT_OTHER 99     /* Some other error/warning has occurred
                                   (e.g. a transport or driver error) */

extern int sg_err_category_sense(const unsigned char * sense_buffer,
                                 int sb_len);

/* Here are some additional sense data categories that are not returned
   by sg_err_category_sense() but are returned by some related functions. */
#define SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO 17 /* Illegal request (other than */
                                /* invalid opcode) plus 'info' field: */
                                /*  [sk,asc,ascq: 0x5,*,*] */
#define SG_LIB_CAT_MEDIUM_HARD_WITH_INFO 18 /* medium or hardware error */
                                /* sense key plus 'info' field: */
                                /*       [sk,asc,ascq: 0x3/0x4,*,*] */
#define SG_LIB_CAT_TIMEOUT 33


/* Iterates to next designation descriptor in the device identification
   VPD page. The 'initial_desig_desc' should point to start of first
   descriptor with 'page_len' being the number of valid bytes in that
   and following descriptors. To start, 'off' should point to a negative
   value, thereafter it should point to the value yielded by the previous
   call. If 0 returned then 'initial_desig_desc + *off' should be a valid
   descriptor; returns -1 if normal end condition and -2 for an abnormal
   termination. Matches association, designator_type and/or code_set when
   any of those values are greater than or equal to zero. */
extern int sg_vpd_dev_id_iter(const unsigned char * initial_desig_desc,
                              int page_len, int * off, int m_assoc,
                              int m_desig_type, int m_code_set);


/* <<< General purpose (i.e. not SCSI specific) utility functions >>> */

/* Always returns valid string even if errnum is wild (or library problem).
   If errnum is negative, flip its sign. */
extern char * safe_strerror(int errnum);


/* Print (to stdout) 'str' of bytes in hex, 16 bytes per line optionally
   followed at the right hand side of the line with an ASCII interpretation.
   Each line is prefixed with an address, starting at 0 for str[0]..str[15].
   All output numbers are in hex. 'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 16 ASCII-hex bytes
       = 0     in addition, the bytes are listed in ASCII to the right
       < 0     only the ASCII-hex bytes are listed (i.e. without address)
*/
extern void dStrHex(const char* str, int len, int no_ascii);

/* Returns 1 when executed on big endian machine; else returns 0.
   Useful for displaying ATA identify words (which need swapping on a
   big endian machine).
*/
extern int sg_is_big_endian();

/* Extract character sequence from ATA words as in the model string
   in a IDENTIFY DEVICE response. Returns number of characters
   written to 'ochars' before 0 character is found or 'num' words
   are processed. */
extern int sg_ata_get_chars(const unsigned short * word_arr, int start_word,
                            int num_words, int is_big_endian, char * ochars);

/* Print (to stdout) 16 bit 'words' in hex, 8 words per line optionally
   followed at the right hand side of the line with an ASCII interpretation
   (pairs of ASCII characters in big endian order (upper first)).
   Each line is prefixed with an address, starting at 0.
   All output numbers are in hex. 'no_ascii' allows for 3 output types:
       > 0     each line has address then up to 8 ASCII-hex words
       = 0     in addition, the words are listed in ASCII pairs to the right
       = -1    only the ASCII-hex words are listed (i.e. without address)
       = -2    only the ASCII-hex words, formatted for "hdparm --Istdin"
       < -2    same as -1
   If 'swapb' non-zero then bytes in each word swapped. Needs to be set
   for ATA IDENTIFY DEVICE response on big-endian machines.
*/
extern void dWordHex(const unsigned short* words, int num, int no_ascii,
                     int swapb);

/* If the number in 'buf' can not be decoded or the multiplier is unknown
   then -1 is returned. Accepts a hex prefix (0x or 0X) or a 'h' (or 'H')
   suffix. Otherwise a decimal multiplier suffix may be given. Recognised
   multipliers: c C  *1;  w W  *2; b  B *512;  k K KiB  *1,024;
   KB  *1,000;  m M MiB  *1,048,576; MB *1,000,000; g G GiB *1,073,741,824;
   GB *1,000,000,000 and <n>x<m> which multiplies <n> by <m> . */
extern int sg_get_num(const char * buf);

/* If the number in 'buf' can not be decoded then -1 is returned. Accepts a
   hex prefix (0x or 0X) or a 'h' (or 'H') suffix; otherwise decimal is
   assumed. Does not accept multipliers. Accept a comma (","), a whitespace
   or newline as terminator.  */
extern int sg_get_num_nomult(const char * buf);

/* If the number in 'buf' can not be decoded or the multiplier is unknown
   then -1LL is returned. Accepts a hex prefix (0x or 0X) or a 'h' (or 'H')
   suffix. Otherwise a decimal multiplier suffix may be given. In addition
   to supporting the multipliers of sg_get_num(), this function supports:
   t T TiB  *(2**40); TB *(10**12); p P PiB  *(2**50); PB  *(10**15) . */
extern long long sg_get_llnum(const char * buf);

extern const char * sg_lib_version();

#ifdef __cplusplus
}
#endif

#endif
