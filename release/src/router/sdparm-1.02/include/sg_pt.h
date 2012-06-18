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
#ifndef SG_PT_H
#define SG_PT_H

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

#ifdef __cplusplus
extern "C" {
#endif

/* This declaration hides the fact that each implementation has its own
   structure "derived" (using a C++ term) from this one. It compiles
   because 'struct sg_pt_base' is only referenced (by pointer: 'objp')
   in this interface. An instance of this structure represents the
   context of one SCSI command. */
struct sg_pt_base;



/* Returns >= 0 if successful. If error in Unix returns negated errno. */
extern int scsi_pt_open_device(const char * device_name, int read_only,
                               int verbose);

/* Returns 0 if successful. If error in Unix returns negated errno. */
extern int scsi_pt_close_device(int device_fd);


/* Create one struct sg_pt_base object per SCSI command issued */
extern struct sg_pt_base * construct_scsi_pt_obj(void);

/* Only invoke once per objp */
extern void set_scsi_pt_cdb(struct sg_pt_base * objp,
                            const unsigned char * cdb, int cdb_len);
/* Only invoke once per objp. Zeroes given 'sense' buffer. */
extern void set_scsi_pt_sense(struct sg_pt_base * objp, unsigned char * sense,
                              int max_sense_len);
/* Invoke at most once per objp */
extern void set_scsi_pt_data_in(struct sg_pt_base * objp,   /* from device */
                                unsigned char * dxferp, int dxfer_len);
/* Invoke at most once per objp */
extern void set_scsi_pt_data_out(struct sg_pt_base * objp,    /* to device */
                                 const unsigned char * dxferp, int dxfer_len);
/* The following "set_"s implementations may be dummies */
extern void set_scsi_pt_packet_id(struct sg_pt_base * objp, int pack_id);
extern void set_scsi_pt_tag(struct sg_pt_base * objp, unsigned long long tag);
extern void set_scsi_pt_task_management(struct sg_pt_base * objp,
                                        int tmf_code);
extern void set_scsi_pt_task_attr(struct sg_pt_base * objp, int attribute,
                                  int priority);

#define SCSI_PT_DO_START_OK 0
#define SCSI_PT_DO_BAD_PARAMS 1
#define SCSI_PT_DO_TIMEOUT 2
/* If OS start error, negated error value (e.g. Unix '-errno') returned,
   return 0 if okay (i.e. at the very least: command sent). Positive
   return values are errors (see SCSI_PT_DO_* defines). */
extern int do_scsi_pt(struct sg_pt_base * objp, int fd, int timeout_secs,
                      int verbose);

#define SCSI_PT_RESULT_GOOD 0
#define SCSI_PT_RESULT_STATUS 1 /* other than GOOD and CHECK CONDITION */
#define SCSI_PT_RESULT_SENSE 2
#define SCSI_PT_RESULT_TRANSPORT_ERR 3
#define SCSI_PT_RESULT_OS_ERR 4
/* highest numbered applicable category returned */
extern int get_scsi_pt_result_category(const struct sg_pt_base * objp);

/* If not available return 0 */
extern int get_scsi_pt_resid(const struct sg_pt_base * objp);
/* Returns SCSI status value (from device that received the
   command). */
extern int get_scsi_pt_status_response(const struct sg_pt_base * objp);
/* Actual sense length returned. If sense data is present but
   actual sense length is not known, return 'max_sense_len' */
extern int get_scsi_pt_sense_len(const struct sg_pt_base * objp);
/* If not available return 0 */
extern int get_scsi_pt_os_err(const struct sg_pt_base * objp);
extern char * get_scsi_pt_os_err_str(const struct sg_pt_base * objp,
                                     int max_b_len, char * b);
/* If not available return 0 */
extern int get_scsi_pt_transport_err(const struct sg_pt_base * objp);
extern char * get_scsi_pt_transport_err_str(const struct sg_pt_base * objp,
                                            int max_b_len, char * b);

/* If not available return -1 */
extern int get_scsi_pt_duration_ms(const struct sg_pt_base * objp);


/* Should be invoked once per objp after other processing is complete in
   order to clean up resources. For ever successful construct_scsi_pt_obj()
   call there should be one destruct_scsi_pt_obj().  */
extern void destruct_scsi_pt_obj(struct sg_pt_base * objp);

#ifdef __cplusplus
}
#endif

#endif
