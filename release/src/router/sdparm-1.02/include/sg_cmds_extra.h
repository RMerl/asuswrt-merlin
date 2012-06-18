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
#ifndef SG_CMDS_EXTRA_H
#define SG_CMDS_EXTRA_H

/*
 * Copyright (c) 2004-2006 Douglas Gilbert.
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

#ifdef __cplusplus
extern "C" {
#endif


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
extern int sg_ll_ata_pt(int sg_fd, const unsigned char * cdbp, int cdb_len,
                          int timeout_secs,  void * dinp, void * doutp,
                          int dlen, unsigned char * sensep,
                          int max_sense_len, unsigned char * ata_return_dp,
                          int max_ata_return_len, int * residp, int verbose);

/* Invokes a FORMAT UNIT (SBC-3) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Format unit not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_format_unit(int sg_fd, int fmtpinfo, int rto_req,
                             int longlist, int fmtdata, int cmplist,
                             int dlist_format, int timeout_secs,
                             void * paramp, int param_len, int noisy,
                             int verbose);

/* Invokes a SCSI GET CONFIGURATION command (MMC-3,4,5).
 * Returns 0 when successful, SG_LIB_CAT_INVALID_OP if command not
 * supported, SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
extern int sg_ll_get_config(int sg_fd, int rt, int starting, void * resp,
                            int mx_resp_len, int noisy, int verbose);

/* Invokes a SCSI PERSISTENT RESERVE IN command (SPC). Returns 0
 * when successful, SG_LIB_CAT_INVALID_OP if command not supported,
 * SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
extern int sg_ll_persistent_reserve_in(int sg_fd, int rq_servact,
                                       void * resp, int mx_resp_len,
                                       int noisy, int verbose);

/* Invokes a SCSI PERSISTENT RESERVE OUT command (SPC). Returns 0
 * when successful, SG_LIB_CAT_INVALID_OP if command not supported,
 * SG_LIB_CAT_ILLEGAL_REQ if field in cdb not supported,
 * SG_LIB_CAT_UNIT_ATTENTION, SG_LIB_CAT_ABORTED_COMMAND, else -1 */
extern int sg_ll_persistent_reserve_out(int sg_fd, int rq_servact,
                                        int rq_scope, unsigned int rq_type,
                                        void * paramp, int param_len,
                                        int noisy, int verbose);

/* Invokes a SCSI READ BUFFER command (SPC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_read_buffer(int sg_fd, int mode, int buffer_id,
                             int buffer_offset, void * resp,
                             int mx_resp_len, int noisy, int verbose);

/* Invokes a SCSI READ DEFECT DATA (10) command (SBC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_read_defect10(int sg_fd, int req_plist, int req_glist,
                               int dl_format, void * resp, int mx_resp_len,
                               int noisy, int verbose);

/* Invokes a SCSI READ LONG (10) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> READ LONG(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_read_long10(int sg_fd, int pblock, int correct,
                             unsigned long lba, void * resp, int xfer_len,
                             int * offsetp, int noisy, int verbose);

/* Invokes a SCSI READ LONG (16) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> READ LONG(16) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 *  -1 -> other failure */
extern int sg_ll_read_long16(int sg_fd, int pblock, int correct,
                             unsigned long long llba, void * resp,
                             int xfer_len, int * offsetp, int noisy,
                             int verbose);

/* Invokes a SCSI READ MEDIA SERIAL NUMBER command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Read media serial number not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_read_media_serial_num(int sg_fd, void * resp,
                                       int mx_resp_len, int noisy,
                                       int verbose);

/* Invokes a SCSI REASSIGN BLOCKS command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_NOT_READY -> device not ready, -1 -> other failure */
extern int sg_ll_reassign_blocks(int sg_fd, int longlba, int longlist,
                                 void * paramp, int param_len, int noisy,
                                 int verbose);

/* Invokes a SCSI RECEIVE DIAGNOSTIC RESULTS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Receive diagnostic results not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_receive_diag(int sg_fd, int pcv, int pg_code, void * resp,
                              int mx_resp_len, int noisy, int verbose);

/* Invokes a SCSI REPORT IDENTIFYING INFORMATION command. This command was
 * called REPORT DEVICE IDENTIFIER prior to spc4r07. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report identifying information not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_report_id_info(int sg_fd, int itype, void * resp,
                                int max_resp_len, int noisy, int verbose);

/* Invokes a SCSI REPORT TARGET PORT GROUPS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Target Port Groups not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_UNIT_ATTENTION, -1 -> other failure */
extern int sg_ll_report_tgt_prt_grp(int sg_fd, void * resp,
                                    int mx_resp_len, int noisy, int verbose);

/* Invokes a SCSI SET TARGET PORT GROUPS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Target Port Groups not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_ABORTED_COMMAND,
 * SG_LIB_CAT_UNIT_ATTENTION, -1 -> other failure */
extern int sg_ll_set_tgt_prt_grp(int sg_fd, void * paramp,
                                    int param_len, int noisy, int verbose);

/* Invokes a SCSI SEND DIAGNOSTIC command. Foreground, extended self tests can
 * take a long time, if so set long_duration flag. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Send diagnostic not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_send_diag(int sg_fd, int sf_code, int pf_bit, int sf_bit,
                           int devofl_bit, int unitofl_bit, int long_duration,
                           void * paramp, int param_len, int noisy,
                           int verbose);

/* Invokes a SCSI SET IDENTIFYING INFORMATION command. This command was
 * called SET DEVICE IDENTIFIER prior to spc4r07. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Set identifying information not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_set_id_info(int sg_fd, int itype, void * paramp,
                             int param_len, int noisy, int verbose);

/* Invokes a SCSI VERIFY (10) command (SBC and MMC).
 * Note that 'veri_len' is in blocks while 'data_out_len' is in bytes.
 * Returns of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Verify(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_MEDIUM_HARD -> medium or hardware error, no valid info,
 * SG_LIB_CAT_MEDIUM_HARD_WITH_INFO -> as previous, with valid info,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_verify10(int sg_fd, int dpo, int bytechk, unsigned long lba,
                          int veri_len, void * data_out, int data_out_len,
                          unsigned long * infop, int noisy, int verbose);

/* Invokes a SCSI WRITE BUFFER command (SPC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_write_buffer(int sg_fd, int mode, int buffer_id,
                              int buffer_offset, void * paramp,
                              int param_len, int noisy, int verbose);

/* Invokes a SCSI WRITE LONG (10) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> WRITE LONG(10) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_write_long10(int sg_fd, int cor_dis, int wr_uncor, int pblock,
                              unsigned long lba, void * data_out,
                              int xfer_len, int * offsetp, int noisy,
                              int verbose);

/* Invokes a SCSI WRITE LONG (16) command (SBC). Note that 'xfer_len'
 * is in bytes. Returns 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> WRITE LONG(16) not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * SG_LIB_CAT_ILLEGAL_REQ_WITH_INFO -> bad field in cdb, with info
 * field written to 'offsetp', SG_LIB_CAT_UNIT_ATTENTION,
 * SG_LIB_CAT_NOT_READY -> device not ready, SG_LIB_CAT_ABORTED_COMMAND,
 * -1 -> other failure */
extern int sg_ll_write_long16(int sg_fd, int cor_dis, int wr_uncor, int pblock,
                              unsigned long long llba, void * data_out,
                              int xfer_len, int * offsetp, int noisy,
                              int verbose);

#ifdef __cplusplus
}
#endif

#endif
