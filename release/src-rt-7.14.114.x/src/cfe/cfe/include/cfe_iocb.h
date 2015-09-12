/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCB definitions				File: cfe_iocb.h
    *  
    *  This module describes CFE's IOCB structure, the main
    *  data structure used to communicate API requests with CFE.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#ifndef _CFE_IOCB_H
#define _CFE_IOCB_H

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define CFE_CMD_FW_GETINFO	0
#define CFE_CMD_FW_RESTART	1
#define CFE_CMD_FW_BOOT		2
#define CFE_CMD_FW_CPUCTL	3
#define CFE_CMD_FW_GETTIME      4
#define CFE_CMD_FW_MEMENUM	5
#define CFE_CMD_FW_FLUSHCACHE	6

#define CFE_CMD_DEV_GETHANDLE	9
#define CFE_CMD_DEV_ENUM	10
#define CFE_CMD_DEV_OPEN	11
#define CFE_CMD_DEV_INPSTAT	12
#define CFE_CMD_DEV_READ	13
#define CFE_CMD_DEV_WRITE	14
#define CFE_CMD_DEV_IOCTL	15
#define CFE_CMD_DEV_CLOSE	16
#define CFE_CMD_DEV_GETINFO	17

#define CFE_CMD_ENV_ENUM	20
#define CFE_CMD_ENV_GET		22
#define CFE_CMD_ENV_SET		23
#define CFE_CMD_ENV_DEL		24

#define CFE_CMD_MAX		32

#define CFE_CMD_VENDOR_USE	0x8000	/* codes above this are for customer use */

#define CFE_MI_RESERVED	0		/* memory is reserved, do not use */
#define CFE_MI_AVAILABLE 1		/* memory is available */

#define CFE_FLG_WARMSTART     0x00000001
#define CFE_FLG_FULL_ARENA    0x00000001
#define CFE_FLG_ENV_PERMANENT 0x00000001

#define CFE_CPU_CMD_START 1
#define CFE_CPU_CMD_STOP 0

#define CFE_STDHANDLE_CONSOLE	0

#define CFE_DEV_NETWORK 	1
#define CFE_DEV_DISK		2
#define CFE_DEV_FLASH		3
#define CFE_DEV_SERIAL		4
#define CFE_DEV_CPU		5
#define CFE_DEV_NVRAM		6
#define CFE_DEV_CLOCK           7
#define CFE_DEV_OTHER		8
#define CFE_DEV_MASK		0x0F

#define CFE_CACHE_FLUSH_D	1
#define CFE_CACHE_INVAL_I	2
#define CFE_CACHE_INVAL_D	4
#define CFE_CACHE_INVAL_L2	8
#define CFE_CACHE_FLUSH_L2	16
#define CFE_CACHE_INVAL_RANGE	32
#define CFE_CACHE_FLUSH_RANGE	64

#ifndef __ASSEMBLER__

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

#define unsigned signed		/* Kludge to get unsigned size-shaped type. */
typedef __SIZE_TYPE__ cfe_int_t;
#undef unsigned
typedef __SIZE_TYPE__ cfe_uint_t;
typedef unsigned long long cfe_offset_t;
typedef long long cfe_int64_t;
typedef unsigned char *cfe_ptr_t;

typedef struct iocb_buffer_s {
    cfe_offset_t  buf_offset;		/* offset on device (bytes) */
    cfe_ptr_t buf_ptr;			/* pointer to a buffer */
    cfe_uint_t   buf_length;		/* length of this buffer */
    cfe_uint_t   buf_retlen;		/* returned length (for read ops) */
    cfe_uint_t   buf_ioctlcmd;		/* IOCTL command (used only for IOCTLs) */
} iocb_buffer_t;

#define buf_devflags buf_ioctlcmd	/* returned device info flags */

typedef struct iocb_inpstat_s {
    cfe_uint_t inp_status;		/* 1 means input available */
} iocb_inpstat_t;

typedef struct iocb_envbuf_s {
    int enum_idx;			/* 0-based enumeration index */
    cfe_ptr_t name_ptr;			/* name string buffer */
    cfe_int_t name_length;		/* size of name buffer */
    cfe_ptr_t val_ptr;			/* value string buffer */
    cfe_int_t val_length;		/* size of value string buffer */
} iocb_envbuf_t;

typedef struct iocb_cpuctl_s {
    cfe_uint_t  cpu_number;		/* cpu number to control */
    cfe_uint_t  cpu_command;		/* command to issue to CPU */
    cfe_uint_t  start_addr;		/* CPU start address */
    cfe_uint_t  gp_val;			/* starting GP value */
    cfe_uint_t  sp_val;			/* starting SP value */
    cfe_uint_t  a1_val;			/* starting A1 value */
} iocb_cpuctl_t;

typedef struct iocb_time_s {
    long long ticks;			/* current time in ticks */
} iocb_time_t;

typedef struct iocb_exitstat_s {
    cfe_int_t status;
} iocb_exitstat_t;

typedef struct iocb_meminfo_s {
    cfe_int_t mi_idx;			/* 0-based enumeration index */
    cfe_int_t mi_type;			/* type of memory block */
    cfe_int64_t mi_addr;		/* physical start address */
    cfe_int64_t mi_size;		/* block size */
} iocb_meminfo_t;

#define CFE_FWI_64BIT		0x00000001
#define CFE_FWI_32BIT		0x00000002
#define CFE_FWI_RELOC		0x00000004
#define CFE_FWI_UNCACHED	0x00000008
#define CFE_FWI_MULTICPU	0x00000010
#define CFE_FWI_FUNCSIM		0x00000020
#define CFE_FWI_RTLSIM		0x00000040

typedef struct iocb_fwinfo_s {
    cfe_int64_t fwi_version;		/* major, minor, eco version */
    cfe_int64_t fwi_totalmem;		/* total installed mem */
    cfe_int64_t fwi_flags;		/* various flags */
    cfe_int64_t fwi_boardid;		/* board ID */
    cfe_int64_t fwi_bootarea_va;	/* VA of boot area */
    cfe_int64_t fwi_bootarea_pa;	/* PA of boot area */
    cfe_int64_t fwi_bootarea_size;	/* size of boot area */
    cfe_int64_t fwi_reserved1;
    cfe_int64_t fwi_reserved2;
    cfe_int64_t fwi_reserved3;
} iocb_fwinfo_t;


typedef struct cfe_iocb_s {
    cfe_uint_t iocb_fcode;		/* IOCB function code */
    cfe_int_t iocb_status;		/* return status */
    cfe_int_t iocb_handle;		/* file/device handle */
    cfe_uint_t iocb_flags;		/* flags for this IOCB */
    cfe_uint_t iocb_psize;		/* size of parameter list */
    union {
	iocb_buffer_t iocb_buffer;	/* buffer parameters */
	iocb_inpstat_t iocb_inpstat;	/* input status parameters */
	iocb_envbuf_t iocb_envbuf;	/* environment function parameters */
	iocb_cpuctl_t iocb_cpuctl;	/* CPU control parameters */
	iocb_time_t   iocb_time;	/* timer parameters */
	iocb_meminfo_t iocb_meminfo;	/* memory arena info parameters */
	iocb_fwinfo_t iocb_fwinfo;	/* firmware information */
	iocb_exitstat_t iocb_exitstat;	/* Exit Status */
    } plist;
} cfe_iocb_t;

#endif

#endif
