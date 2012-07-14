/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Filesystem dispatch defs	       		File: cfe_fileops.h
    *  
    *  CFE supports multiple access methods to files on boot
    *  media.  This module contains the dispatch table structures
    *  for "file systems".
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


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define FILE_SEEK_BEGINNING	0
#define FILE_SEEK_CURRENT	1

#define FILE_MODE_READ	1
#define FILE_MODE_WRITE	2

/* These flags should not conflict with the loader arg flags (see cfe_loadargs_t) */
#define FSYS_TYPE_NETWORK	0x40000000

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define BDINIT(ops,fsctx,name) (ops)->init((fsctx),(name))
#define BDOPEN(ops,ref,fsctx,name) (ops)->open((ref),(fsctx),(name),FILE_MODE_READ)
#define BDOPEN2(ops,ref,fsctx,name,mode) (ops)->open((ref),(fsctx),(name),(mode))
#define BDOPEN_WR(ops,ref,fsctx,name) (ops)->open((ref),(fsctx),(name),FILE_MODE_WRITE)
#define BDREAD(ops,ref,buf,len) (ops)->read((ref),(buf),(len))
#define BDWRITE(ops,ref,buf,len) (ops)->write((ref),(buf),(len))
#define BDCLOSE(ops,ref) (ops)->close((ref))
#define BDUNINIT(ops,ref) (ops)->uninit((ref))
#define BDSEEK(ops,ref,offset,how) (ops)->seek((ref),(offset),(how))

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct fileio_dispatch_s {
    const char *method;
    unsigned int loadflags;
    int (*init)(void **fsctx,void *device);
    int (*open)(void **ref,void *fsctx,char *filename,int mode);
    int (*read)(void *ref,uint8_t *buf,int len);
    int (*write)(void *ref,uint8_t *buf,int len);
    int (*seek)(void *ref,int offset,int how);
    void (*close)(void *ref);
    void (*uninit)(void *devctx);
} fileio_dispatch_t;

typedef struct fileio_ctx_s {
    const fileio_dispatch_t *ops;
    void *fsctx;
} fileio_ctx_t;

const fileio_dispatch_t *cfe_findfilesys(const char *name);

int fs_init(char *fsname,fileio_ctx_t **fsctx,void *device);
int fs_uninit(fileio_ctx_t *);
int fs_open(fileio_ctx_t *,void **ref,char *filename,int mode);
int fs_close(fileio_ctx_t *,void *ref);
int fs_read(fileio_ctx_t *,void *ref,uint8_t *buffer,int len);
int fs_write(fileio_ctx_t *,void *ref,uint8_t *buffer,int len);
int fs_seek(fileio_ctx_t *,void *ref,int offset,int how);
int fs_hook(fileio_ctx_t *fsctx,char *fsname);
