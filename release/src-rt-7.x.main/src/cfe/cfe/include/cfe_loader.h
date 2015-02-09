/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Loader API 				File: cfe_loader.h
    *  
    *  This is the main API for the program loader.
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

#ifndef _CFE_LOADER_H_
#define _CFE_LOADER_H_

#define LOADFLG_NOISY		0x0001	/* print out noisy info */
#define LOADFLG_EXECUTE		0x0002	/* execute loaded program */
#define LOADFLG_SPECADDR	0x0004	/* Use a specific size & addr */
#define LOADFLG_NOBB		0x0008	/* don't look for a boot block */
#define LOADFLG_NOCLOSE		0x0010	/* don't close network */
#define LOADFLG_COMPRESSED	0x0020	/* file is compressed */
#define LOADFLG_BATCH		0x0040	/* batch file */

typedef struct cfe_loadargs_s {
    char *la_filename;			/* name of file on I/O device */
    char *la_filesys;			/* file system name */
    char *la_device;			/* device name (ide0, etc.) */
    char *la_options;			/* args to pass to loaded prog */
    char *la_loader;			/* binary file loader to use */
    unsigned int la_flags;		/* various flags */
    long la_address;			/* used by SPECADDR only */
    unsigned long la_maxsize;		/* used by SPECADDR only */
    long la_entrypt;			/* returned entry point */
} cfe_loadargs_t;


typedef struct cfe_loader_s {
    char *name;				/* name of loader */
    int (*loader)(cfe_loadargs_t *);	/* access function */
    int flags;				/* flags */
} cfe_loader_t;

typedef struct cfe_frag_info_s {
	uint32_t *func_ptr;
	uint32_t des_addr;
	uint32_t src_addr;
	uint32_t len;
} cfe_frag_info_t;

#define LDRLOAD(ldr,arg) (*((ldr)->loader))(arg)

int cfe_load_program(char *name,cfe_loadargs_t *la);
const cfe_loader_t *cfe_findloader(char *name);
void splitpath(char *path,char **devname,char **filename);
void cfe_go(cfe_loadargs_t *la);
int cfe_boot(char *ldrname,cfe_loadargs_t *la);
int cfe_savedata(char *fsname,char *devname,char *filename,uint8_t *start,uint8_t *end);

#endif /* _CFE_LOADER_H_ */
