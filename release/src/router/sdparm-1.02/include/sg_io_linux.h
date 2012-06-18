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
#ifndef SG_IO_LINUX_H
#define SG_IO_LINUX_H

/*
 * Copyright (c) 2004-2005 Douglas Gilbert.
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
 * Version 1.02 [20070121]
 */

/*
 * This header file contains linux specific information related to the SCSI
 * command pass through in the SCSI generic (sg) driver and the linux
 * block layer.
 */

#include "sg_lib.h"
#include "sg_linux_inc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The following are 'host_status' codes */
#ifndef DID_OK
#define DID_OK 0x00
#endif
#ifndef DID_NO_CONNECT
#define DID_NO_CONNECT 0x01     /* Unable to connect before timeout */
#define DID_BUS_BUSY 0x02       /* Bus remain busy until timeout */
#define DID_TIME_OUT 0x03       /* Timed out for some other reason */
#define DID_BAD_TARGET 0x04     /* Bad target (id?) */
#define DID_ABORT 0x05          /* Told to abort for some other reason */
#define DID_PARITY 0x06         /* Parity error (on SCSI bus) */
#define DID_ERROR 0x07          /* Internal error */
#define DID_RESET 0x08          /* Reset by somebody */
#define DID_BAD_INTR 0x09       /* Received an unexpected interrupt */
#define DID_PASSTHROUGH 0x0a    /* Force command past mid-level */
#define DID_SOFT_ERROR 0x0b     /* The low-level driver wants a retry */
#endif
#ifndef DID_IMM_RETRY
#define DID_IMM_RETRY 0x0c      /* Retry without decrementing retry count  */
#endif
#ifndef DID_REQUEUE
#define DID_REQUEUE 0x0d        /* Requeue command (no immediate retry) also
                                 * without decrementing the retry count    */
#endif

/* These defines are to isolate applications from kernel define changes */
#define SG_LIB_DID_OK           DID_OK
#define SG_LIB_DID_NO_CONNECT   DID_NO_CONNECT
#define SG_LIB_DID_BUS_BUSY     DID_BUS_BUSY
#define SG_LIB_DID_TIME_OUT     DID_TIME_OUT
#define SG_LIB_DID_BAD_TARGET   DID_BAD_TARGET
#define SG_LIB_DID_ABORT        DID_ABORT
#define SG_LIB_DID_PARITY       DID_PARITY
#define SG_LIB_DID_ERROR        DID_ERROR
#define SG_LIB_DID_RESET        DID_RESET
#define SG_LIB_DID_BAD_INTR     DID_BAD_INTR
#define SG_LIB_DID_PASSTHROUGH  DID_PASSTHROUGH
#define SG_LIB_DID_SOFT_ERROR   DID_SOFT_ERROR
#define SG_LIB_DID_IMM_RETRY    DID_IMM_RETRY
#define SG_LIB_DID_REQUEUE      DID_REQUEUE

/* The following are 'driver_status' codes */
#ifndef DRIVER_OK
#define DRIVER_OK 0x00
#endif
#ifndef DRIVER_BUSY
#define DRIVER_BUSY 0x01
#define DRIVER_SOFT 0x02
#define DRIVER_MEDIA 0x03
#define DRIVER_ERROR 0x04
#define DRIVER_INVALID 0x05
#define DRIVER_TIMEOUT 0x06
#define DRIVER_HARD 0x07
#define DRIVER_SENSE 0x08       /* Sense_buffer has been set */

/* Following "suggests" are "or-ed" with one of previous 8 entries */
#define SUGGEST_RETRY 0x10
#define SUGGEST_ABORT 0x20
#define SUGGEST_REMAP 0x30
#define SUGGEST_DIE 0x40
#define SUGGEST_SENSE 0x80
#define SUGGEST_IS_OK 0xff
#endif
#ifndef DRIVER_MASK
#define DRIVER_MASK 0x0f
#endif
#ifndef SUGGEST_MASK
#define SUGGEST_MASK 0xf0
#endif

/* These defines are to isolate applications from kernel define changes */
#define SG_LIB_DRIVER_OK        DRIVER_OK
#define SG_LIB_DRIVER_BUSY      DRIVER_BUSY
#define SG_LIB_DRIVER_SOFT      DRIVER_SOFT
#define SG_LIB_DRIVER_MEDIA     DRIVER_MEDIA
#define SG_LIB_DRIVER_ERROR     DRIVER_ERROR
#define SG_LIB_DRIVER_INVALID   DRIVER_INVALID
#define SG_LIB_DRIVER_TIMEOUT   DRIVER_TIMEOUT
#define SG_LIB_DRIVER_HARD      DRIVER_HARD
#define SG_LIB_DRIVER_SENSE     DRIVER_SENSE
#define SG_LIB_SUGGEST_RETRY    SUGGEST_RETRY
#define SG_LIB_SUGGEST_ABORT    SUGGEST_ABORT
#define SG_LIB_SUGGEST_REMAP    SUGGEST_REMAP
#define SG_LIB_SUGGEST_DIE      SUGGEST_DIE
#define SG_LIB_SUGGEST_SENSE    SUGGEST_SENSE
#define SG_LIB_SUGGEST_IS_OK    SUGGEST_IS_OK
#define SG_LIB_DRIVER_MASK      DRIVER_MASK
#define SG_LIB_SUGGEST_MASK     SUGGEST_MASK

extern void sg_print_masked_status(int masked_status);
extern void sg_print_host_status(int host_status);
extern void sg_print_driver_status(int driver_status);

/* sg_chk_n_print() returns 1 quietly if there are no errors/warnings
   else it prints errors/warnings (prefixed by 'leadin') to
   'sg_warnings_fd' and returns 0. */
extern int sg_chk_n_print(const char * leadin, int masked_status,
                          int host_status, int driver_status,
                          const unsigned char * sense_buffer, int sb_len,
                          int raw_sinfo);

/* The following function declaration is for the sg version 3 driver. */
struct sg_io_hdr;
/* sg_chk_n_print3() returns 1 quietly if there are no errors/warnings;
   else it prints errors/warnings (prefixed by 'leadin') to
   'sg_warnings_fd' and returns 0. */
extern int sg_chk_n_print3(const char * leadin, struct sg_io_hdr * hp,
                           int raw_sinfo);

/* Calls sg_scsi_normalize_sense() after obtaining the sense buffer and
   its length from the struct sg_io_hdr pointer. If these cannot be
   obtained, 0 is returned. */
extern int sg_normalize_sense(const struct sg_io_hdr * hp, 
                              struct sg_scsi_sense_hdr * sshp);

extern int sg_err_category(int masked_status, int host_status,
               int driver_status, const unsigned char * sense_buffer,
               int sb_len);

extern int sg_err_category_new(int scsi_status, int host_status,
               int driver_status, const unsigned char * sense_buffer,
               int sb_len);

/* The following function declaration is for the sg version 3 driver. */
extern int sg_err_category3(struct sg_io_hdr * hp);


/* Note about SCSI status codes found in older versions of Linux.
   Linux has traditionally used a 1 bit right shifted and masked 
   version of SCSI standard status codes. Now CHECK_CONDITION
   and friends (in <scsi/scsi.h>) are deprecated. */

#ifdef __cplusplus
}
#endif

#endif
