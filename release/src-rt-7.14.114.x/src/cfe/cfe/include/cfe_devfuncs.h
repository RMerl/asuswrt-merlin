/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Device function prototypes		File: cfe_devfuncs.h
    *  
    *  This module contains prototypes for cfe_devfuncs.c, a set
    *  of wrapper routines to the IOCB interface.  This file,
    *  along with cfe_devfuncs.c, can be incorporated into programs
    *  that need to call CFE.
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

#define CFE_EPTSEAL 0x43464531
#if CFG_BIENDIAN && defined(__MIPSEB)
#define CFE_EPTSEAL_REV 0x31454643
#endif

#define CFE_APISEAL  0xBFC004E0
#define CFE_APIENTRY 0xBFC00500



#ifndef __ASSEMBLER__
int cfe_open(char *name);
int cfe_close(int handle);
int cfe_readblk(int handle,cfe_offset_t offset,unsigned char *buffer,int length);
int cfe_read(int handle,unsigned char *buffer,int length);
int cfe_writeblk(int handle,cfe_offset_t offset,unsigned char *buffer,int length);
int cfe_write(int handle,unsigned char *buffer,int length);
int cfe_ioctl(int handle,unsigned int ioctlnum,unsigned char *buffer,int length,int *retlen,
	      cfe_offset_t offset);
int cfe_inpstat(int handle);
int cfe_getenv(char *name,char *dest,int destlen);
long long cfe_getticks(void);
int cfe_exit(int warm,int code);
int cfe_flushcache(int flg);
int cfe_getdevinfo(char *name);
int cfe_flushcache(int);
#endif
