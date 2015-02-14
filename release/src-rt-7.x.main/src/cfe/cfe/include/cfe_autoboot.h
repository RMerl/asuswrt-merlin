/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Automatic OS bootstrap			File: cfe_autoboot.h
    *  
    *  This module handles OS bootstrap stuff.  We use this version
    *  to do "automatic" booting; walking down a list of possible boot
    *  options, trying them until something good happens.
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


#define CFE_AUTOBOOT_END	0
#define CFE_AUTOBOOT_NETWORK	1
#define CFE_AUTOBOOT_DISK	2
#define CFE_AUTOBOOT_RAW	3

typedef struct cfe_autoboot_method_s {
    queue_t ab_qblock;
    int ab_type;
    int ab_flags;
    char *ab_dev;
    char *ab_loader;
    char *ab_filesys;
    char *ab_file;
} cfe_autoboot_method_t;

#define CFE_AUTOFLG_POLLCONSOLE	1	/* boot can be interrupted */
#define CFE_AUTOFLG_TRYFOREVER	2	/* keep trying forever */

int cfe_autoboot(char *dev,int flags);
int cfe_add_autoboot(int type,int flags,char *dev,char *loader,char *filesys,char *file);
