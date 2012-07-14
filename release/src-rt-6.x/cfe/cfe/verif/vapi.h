/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Verification Test APIs			File: vapi.h
    *
    *  This module contains special low-level routines for use
    *  by verification programs.
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


#define VAPI_EPTSEAL		0x56415049
#define VAPI_CFESEAL		0xCFE10000
#define VAPI_SEAL_MASK		0xFFFF0000
#define VAPI_FMT_MASK		0x000000FF
#define VAPI_PRID_MASK		0x0000FF00

#define VAPI_DIAG_ENTRY		0x80020000
#define VAPI_DIAG_ENTRY_UNC	0xA0020000
#define VAPI_DIAG_ENTRY_MC	0xBFD00000
#define VAPI_MAGIC_NUMBER	0x1CFE2CFE3CFE4CFE
#define VAPI_MAGIC_NUMBER_UNC	0x0CFE1CFE2CFE3CFE
#define VAPI_MAGIC_NUMBER_MC	0xACFEBCFECCFEDCFE

#define VAPI_FMT_GPRS		0
#define VAPI_FMT_SOC		1
#define VAPI_FMT_DATA		2
#define VAPI_FMT_BUFFER		3
#define VAPI_FMT_TRACE		4
#define VAPI_FMT_EXIT		5
#define VAPI_FMT_FPRS		6

#define VAPI_PRNUM_SHIFT	8

#define VAPI_REC_SIGNATURE	0
#define VAPI_REC_SIZE		8
#define VAPI_REC_RA		16
#define VAPI_REC_DATA		24

#define VAPI_IDX_SIGNATURE	0
#define VAPI_IDX_SIZE		1
#define VAPI_IDX_RA		2
#define VAPI_IDX_DATA		3


#define VAPI_FUNC_EXIT		0x9fc00510
#define VAPI_FUNC_DUMPGPRS	0x9fc00520
#define VAPI_FUNC_SETLOG	0x9fc00530
#define VAPI_FUNC_LOGVALUE	0x9fc00540
#define VAPI_FUNC_LOGDATA	0x9fc00550
#define VAPI_FUNC_LOGTRACE	0x9fc00560
#define VAPI_FUNC_LOGSOC	0x9fc00570
#define VAPI_FUNC_LOGGPRS	0x9fc00580
#define VAPI_FUNC_DUMPSTRING	0x9fc00590
#define VAPI_FUNC_SETLEDS	0x9fc005a0
#define VAPI_FUNC_LOGFPRS	0x9fc005b0


#define VAPI_LOG_SETBUF(start,end)					\
	.set push ;							\
	.set reorder ;							\
	la	a0, start ;						\
	la	a1, end ;						\
	li	k0, VAPI_FUNC_SETLOG ;					\
	jalr	k0 ;							\
	.set pop

#define VAPI_EXIT_CONST(val)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, val ;						\
	li	k0, VAPI_FUNC_EXIT ;					\
	jr	k0 ;							\
	.set pop

#define VAPI_EXIT_REG(val)						\
	.set push ;							\
	.set reorder ;							\
	move	a0, val ;						\
	li	k0, VAPI_FUNC_EXIT ;					\
	jr	k0 ;							\
	.set pop

#define VAPI_LOG_CONST(id,value)					\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	li	a1, value ;						\
	li	k0, VAPI_FUNC_LOGVALUE ;				\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_REG(id,value)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	move	a1, value ;						\
	li	k0, VAPI_FUNC_LOGVALUE ;				\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_BUFFER(id,addr,nwords)					\
	.set push ;							\
	.set reorder ;							\
	li	a0,id ;							\
	la	a1,addr ;						\
	li	a2,nwords ;						\
	li	k0, VAPI_FUNC_LOGDATA ;					\
	jalr	k0 ;							\
	.set pop

#define VAPI_PUTS(text)							\
	.set push ;							\
	.set reorder ;							\
	b	1f ;							\
2:	.asciz text ;							\
	.align 4 ;							\
1:	la	a0, 2b ;						\
	li	k0, VAPI_FUNC_DUMPSTRING ;				\
	jalr	k0 ;							\
	.set pop

#define VAPI_PRINTGPRS()						\
	.set push ;							\
	.set reorder ;							\
	li	k0, VAPI_FUNC_DUMPGPRS ;				\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_GPRS(id)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	li	k0, VAPI_FUNC_LOGGPRS ;					\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_FPRS(id)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	li	k0, VAPI_FUNC_LOGFPRS ;					\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_TRACE(id)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	li	k0, VAPI_FUNC_LOGTRACE ;				\
	jalr	k0 ;							\
	.set pop

#define VAPI_LOG_SOCSTATE(id,bits)					\
	.set push ;							\
	.set reorder ;							\
	li	a0, id ;						\
	li	a1, bits ;						\
	li	k0, VAPI_FUNC_LOGSOC ;					\
	jalr	k0 ;							\
	.set pop

#define VAPI_SETLEDS(a,b,c,d)						\
	.set push ;							\
	.set reorder ;							\
	li	a0, ((a) << 24) | ((b) << 16) | ((c) << 8) | (d) ;	\
	li	k0, VAPI_FUNC_SETLEDS ;					\
	jalr	k0 ;							\
	.set pop

#ifndef SOC_AGENT_MC0
#define SOC_AGENT_MC0 0x00000001
#define SOC_AGENT_MC1 0x00000002
#define SOC_AGENT_MC 0x00000003
#define SOC_AGENT_L2 0x00000004
#define SOC_AGENT_MACDMA0 0x00000008
#define SOC_AGENT_MACDMA1 0x00000010
#define SOC_AGENT_MACDMA2 0x00000020
#define SOC_AGENT_MACDMA 0x00000038
#define SOC_AGENT_MACRMON0 0x00000040
#define SOC_AGENT_MACRMON1 0x00000080
#define SOC_AGENT_MACRMON2 0x00000100
#define SOC_AGENT_MACRMON 0x000001C0
#define SOC_AGENT_MAC0 0x00000200
#define SOC_AGENT_MAC1 0x00000400
#define SOC_AGENT_MAC2 0x00000800
#define SOC_AGENT_MAC 0x00000E00
#define SOC_AGENT_DUART 0x00001000
#define SOC_AGENT_GENCS 0x00002000
#define SOC_AGENT_GEN 0x00004000
#define SOC_AGENT_GPIO 0x00008000
#define SOC_AGENT_SMBUS0 0x00010000
#define SOC_AGENT_SMBUS1 0x00020000
#define SOC_AGENT_SMBUS 0x00030000
#define SOC_AGENT_TIMER 0x00040000
#define SOC_AGENT_SCD 0x00080000
#define SOC_AGENT_BUSERR 0x00100000
#define SOC_AGENT_DM 0x00200000
#define SOC_AGENT_IMR0 0x00400000
#define SOC_AGENT_IMR1 0x00800000
#define SOC_AGENT_IMR 0x00C00000
#endif
