/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  "Raw" file system			File: cfe_rawfs.c
    *  
    *  This filesystem dispatch is used to read files directly
    *  from a block device.  For example, you can 'dd' an elf 
    *  file directly onto a disk or flash card and then run
    *  it using this module.
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
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_console.h"

#include "cfe.h"

/*  *********************************************************************
    *  RAW context
    ********************************************************************* */

/*
 * File system context - describes overall file system info,
 * such as the handle to the underlying device.
 */

typedef struct raw_fsctx_s {
    int raw_dev;
    int raw_isconsole;
    int raw_refcnt;
} raw_fsctx_t;

/*
 * File context - describes an open file on the file system.
 * For raw devices, this is pretty meaningless, but we do
 * keep track of where we are.
 */

typedef struct raw_file_s {
    raw_fsctx_t *raw_fsctx;
    int raw_fileoffset;
    int raw_baseoffset;			/* starting offset of raw "file" */
    int raw_length;			/* length of file, -1 for whole device */
} raw_file_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int raw_fileop_init(void **fsctx,void *devicename);
static int raw_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int raw_fileop_read(void *ref,uint8_t *buf,int len);
static int raw_fileop_write(void *ref,uint8_t *buf,int len);
static int raw_fileop_seek(void *ref,int offset,int how);
static void raw_fileop_close(void *ref);
static void raw_fileop_uninit(void *fsctx);

/*  *********************************************************************
    *  RAW fileio dispatch table
    ********************************************************************* */

const fileio_dispatch_t raw_fileops = {
    "raw",
    0,
    raw_fileop_init,
    raw_fileop_open,
    raw_fileop_read,
    raw_fileop_write,
    raw_fileop_seek,
    raw_fileop_close,
    raw_fileop_uninit
};

static int raw_fileop_init(void **newfsctx,void *dev)
{
    raw_fsctx_t *fsctx;
    char *devicename = (char *) dev;

    *newfsctx = NULL;

    fsctx = KMALLOC(sizeof(raw_fsctx_t),0);
    if (!fsctx) {
	return CFE_ERR_NOMEM;
	}

    if (console_name && strcmp(devicename,console_name) == 0) {	
	fsctx->raw_dev = console_handle;
	fsctx->raw_isconsole = TRUE;
	}
    else {
	fsctx->raw_dev = cfe_open(devicename);
	fsctx->raw_isconsole = FALSE;
	}

    fsctx->raw_refcnt = 0;

    if (fsctx->raw_dev >= 0) {
	*newfsctx = fsctx;
	return 0;
	}

    KFREE(fsctx);

    return CFE_ERR_FILENOTFOUND;
}

static int raw_fileop_open(void **ref,void *fsctx_arg,char *filename,int mode)
{
    raw_fsctx_t *fsctx;
    raw_file_t *file;
    char temp[100];
    char *len;

    if (mode != FILE_MODE_READ) return CFE_ERR_UNSUPPORTED;

    fsctx = (raw_fsctx_t *) fsctx_arg;

    file = KMALLOC(sizeof(raw_file_t),0);
    if (!file) {
	return CFE_ERR_NOMEM;
	}

    file->raw_fileoffset = 0;
    file->raw_fsctx = fsctx;

    /* Assume the whole device. */
    file->raw_baseoffset = 0;
    file->raw_length = -1;

    /*
     * If a filename was specified, it will be in the form
     * offset,length - for example, 0x10000,0x200
     * Parse this into two pieces and set up our internal
     * file extent information.  you can use either decimal
     * or "0x" notation.
     */
    if (filename) {
	lib_trimleading(filename);
	strncpy(temp,filename,sizeof(temp));
	len = strchr(temp,','); 
	if (len) *len++ = '\0';
	if (temp[0]) {
	    file->raw_baseoffset = lib_atoi(temp);
	    }
	if (len) {
	    file->raw_length = lib_atoi(len);
	    }
	}

    fsctx->raw_refcnt++;

    *ref = file;
    return 0;
}

static int raw_fileop_read(void *ref,uint8_t *buf,int len)
{
    raw_file_t *file = (raw_file_t *) ref;
    int res;

    /*
     * Bound the length based on our "file length" if one
     * was specified.
     */

    if (file->raw_length >= 0) {
	if ((file->raw_length - file->raw_fileoffset) < len) {
	    len = file->raw_length - file->raw_fileoffset;
	    }
	}

    if (len == 0) return 0;

    /*
     * Read the data, adding in the base address.
     */

    res = cfe_readblk(file->raw_fsctx->raw_dev,
		      file->raw_baseoffset + file->raw_fileoffset,
		      buf,len);

    if (res > 0) {
	file->raw_fileoffset += res;
	}

    return res;
}

static int raw_fileop_write(void *ref,uint8_t *buf,int len)
{
    return CFE_ERR_UNSUPPORTED;
}

static int raw_fileop_seek(void *ref,int offset,int how)
{
    raw_file_t *file = (raw_file_t *) ref;

    switch (how) {
	case FILE_SEEK_BEGINNING:
	    file->raw_fileoffset = offset;
	    break;
	case FILE_SEEK_CURRENT:
	    file->raw_fileoffset += offset;
	    if (file->raw_fileoffset < 0) file->raw_fileoffset = 0;
	    break;
	default:
	    break;
	}

    /*
     * Make sure we don't attempt to seek past the end of the file.
     */

    if (file->raw_length >= 0) {
	if (file->raw_fileoffset > file->raw_length) {
	    file->raw_fileoffset = file->raw_length;
	    }
	}

    return file->raw_fileoffset;
}


static void raw_fileop_close(void *ref)
{
    raw_file_t *file = (raw_file_t *) ref;

    file->raw_fsctx->raw_refcnt--;

    KFREE(file);
}

static void raw_fileop_uninit(void *fsctx_arg)
{
    raw_fsctx_t *fsctx = (raw_fsctx_t *) fsctx_arg;

    if (fsctx->raw_refcnt) {
	xprintf("raw_fileop_uninit: warning: refcnt not zero\n");
	}

    if (fsctx->raw_isconsole == FALSE) {
	cfe_close(fsctx->raw_dev);
	}

    KFREE(fsctx);
}
