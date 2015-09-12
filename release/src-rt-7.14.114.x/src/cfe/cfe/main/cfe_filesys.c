/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Filesystem manager			File: cfe_filesys.c
    *  
    *  This module maintains the list of available file systems.
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

#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"

#include "cfe_error.h"
#include "cfe_fileops.h"

#include "cfe.h"

#include "bsp_config.h"

/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern const fileio_dispatch_t raw_fileops;
#if CFG_NETWORK
extern const fileio_dispatch_t tftp_fileops;
#if CFG_TCP && CFG_HTTPFS
extern const fileio_dispatch_t http_fileops;
#endif
#endif
#if CFG_FATFS
extern const fileio_dispatch_t fatfs_fileops;
extern const fileio_dispatch_t pfatfs_fileops;
#endif
#if CFG_ZLIB
extern const fileio_dispatch_t zlibfs_fileops;
#endif
#if CFG_LZMA
extern const fileio_dispatch_t lzmafs_fileops;
#endif
extern const fileio_dispatch_t memory_fileops;
/*  *********************************************************************
    *  File system list
    ********************************************************************* */

static const fileio_dispatch_t * const cfe_filesystems[] = {
    &raw_fileops,
#if CFG_NETWORK
    &tftp_fileops,
#if (CFG_TCP) & (CFG_HTTPFS)
    &http_fileops,
#endif
#endif
#if CFG_FATFS
    &fatfs_fileops,
    &pfatfs_fileops,
#endif
#if CFG_ZLIB
    &zlibfs_fileops,
#endif
#if CFG_LZMA
    &lzmafs_fileops,
#endif
	&memory_fileops,
    NULL
};

/*  *********************************************************************
    *  cfe_findfilesys(name)
    *  
    *  Locate the dispatch record for a particular file system.
    *  
    *  Input parameters: 
    *  	   name - name of filesys to locate
    *  	   
    *  Return value:
    *  	   pointer to dispatch table or NULL if not found
    ********************************************************************* */

const fileio_dispatch_t *cfe_findfilesys(const char *name)
{
    const fileio_dispatch_t * const *disp;
    
    disp = cfe_filesystems;

    while (*disp) {
	if (strcmp((*disp)->method,name) == 0) return *disp;
	disp++;
	}

    return NULL;
}

/*  *********************************************************************
    *  fs_init(name,fsctx,device)
    *  
    *  Initialize a filesystem context
    *  
    *  Input parameters: 
    *  	   name - name of file system
    *  	   fsctx - returns a filesystem context
    *  	   device - device name or other info
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */
int fs_init(char *fsname,fileio_ctx_t **fsctx,void *device)
{
    fileio_ctx_t *ctx;
    int res;
    const fileio_dispatch_t *ops;

    ops = cfe_findfilesys((const char *)fsname);
    if (!ops) return CFE_ERR_FSNOTAVAIL;

    ctx = (fileio_ctx_t *) KMALLOC(sizeof(fileio_ctx_t),0);
    if (!ctx) return CFE_ERR_NOMEM;

    ctx->ops = ops;
    res = BDINIT(ops,&(ctx->fsctx),device);

    if (res != 0) {
	KFREE(ctx);
	}

    *fsctx = ctx;

    return res;
}


/*  *********************************************************************
    *  fs_hook(fsctx,name)
    *  
    *  "Hook" a filesystem to attach a filter, like a decompression
    *  or decryption engine.
    *  
    *  Input parameters: 
    *  	   fsctx - result from a previous fs_init 
    *  	   name - name of fs to hook onto the beginning
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int fs_hook(fileio_ctx_t *fsctx,char *fsname)
{
    void *hookfsctx;
    const fileio_dispatch_t *ops;
    int res;

    /*
     * Find the hook filesystem (well, actually a filter)
     */

    ops = cfe_findfilesys((const char *)fsname);
    if (!ops) return CFE_ERR_FSNOTAVAIL;

    /* 
     * initialize our hook file filter.
     */

    res = BDINIT(ops,&hookfsctx,fsctx);
    if (res != 0) return res;

    /*
     * Now replace dispatch table for current filesystem
     * with the hook's dispatch table.  When fs_read is called,
     * we'll go to the hook, and the hook will call the original.
     *
     * When fs_uninit is called, the hook will call the original's
     * uninit routine.
     */

    fsctx->ops = ops;
    fsctx->fsctx = hookfsctx;

    return 0;

}

/*  *********************************************************************
    *  fs_uninit(fsctx)
    *  
    *  Uninitialize a file system context.
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context to remove (from fs_init)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */
int fs_uninit(fileio_ctx_t *fsctx)
{
    BDUNINIT(fsctx->ops,fsctx->fsctx);

    KFREE(fsctx);

    return 0;
}


/*  *********************************************************************
    *  fs_open(fsctx,ref,filename,mode)
    *  
    *  Open a file on the file system
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context (from fs_init)
    *  	   ref - returns file handle
    *  	   filename - name of file to open
    *  	   mode - file open mode
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */
int fs_open(fileio_ctx_t *fsctx,void **ref,char *filename,int mode)
{
    return BDOPEN2(fsctx->ops,ref,fsctx->fsctx,filename,mode);
}

/*  *********************************************************************
    *  fs_close(fsctx,ref)
    *  
    *  Close a file on the file system
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context (from fs_init)
    *  	   ref - file handle (from fs_open)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */
int fs_close(fileio_ctx_t *fsctx,void *ref)
{
    BDCLOSE(fsctx->ops,ref);

    return 0;
}


/*  *********************************************************************
    *  fs_read(fsctx,ref,buffer,len)
    *  
    *  Read data from the device.
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context (from fs_init)
    *  	   ref - file handle (from fs_open)
    *  	   buffer - buffer pointer
    *  	   len - length
    *  	   
    *  Return value:
    *  	   number of bytes read
    *  	   0=eof
    *  	   <0 = error
    ********************************************************************* */

int fs_read(fileio_ctx_t *fsctx,void *ref,uint8_t *buffer,int len)
{
    return BDREAD(fsctx->ops,ref,buffer,len);
}

/*  *********************************************************************
    *  fs_write(fsctx,ref,buffer,len)
    *  
    *  write data from the device.
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context (from fs_init)
    *  	   ref - file handle (from fs_open)
    *  	   buffer - buffer pointer
    *  	   len - length
    *  	   
    *  Return value:
    *  	   number of bytes written
    *  	   0=eof
    *  	   <0 = error
    ********************************************************************* */

int fs_write(fileio_ctx_t *fsctx,void *ref,uint8_t *buffer,int len)
{
    return BDWRITE(fsctx->ops,ref,buffer,len);
}

/*  *********************************************************************
    *  fs_seek(fsctx,ref,offset,how)
    *  
    *  move file pointer on the device
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context (from fs_init)
    *  	   ref - file handle (from fs_open)
    *  	   offset - distance to move
    *  	   how - origin (FILE_SEEK_xxx)
    *  	   
    *  Return value:
    *  	   new offset
    *  	   <0 = error
    ********************************************************************* */

int fs_seek(fileio_ctx_t *fsctx,void *ref,int offset,int how)
{
    return BDSEEK(fsctx->ops,ref,offset,how);
}
