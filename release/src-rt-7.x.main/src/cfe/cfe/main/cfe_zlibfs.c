/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  "Compressed" file system			File: cfe_zlibfs.c
    *  
    *  This is more of a filesystem "hook" than an actual file system.
    *  You can stick it on the front of the chain of file systems
    *  that CFE calls and it will route data read from the
    *  underlying filesystem through ZLIB before passing it up to the
    *  user.
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

#if CFG_ZLIB

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

#include "zlib.h"

/*  *********************************************************************
    *  ZLIBFS context
    ********************************************************************* */

/*
 * File system context - describes overall file system info,
 * such as the handle to the underlying device.
 */

typedef struct zlibfs_fsctx_s {
    void *zlibfsctx_subfsctx;
    const fileio_dispatch_t *zlibfsctx_subops;
    int zlibfsctx_refcnt;
} zlibfs_fsctx_t;

/*
 * File context - describes an open file on the file system.
 * For raw devices, this is pretty meaningless, but we do
 * keep track of where we are.
 */

#define ZLIBFS_BUFSIZE	1024
typedef struct zlibfs_file_s {
    zlibfs_fsctx_t *zlibfs_fsctx;
    int zlibfs_fileoffset;
    void *zlibfs_subfile;
    z_stream zlibfs_stream;
    uint8_t *zlibfs_inbuf;
    uint8_t *zlibfs_outbuf;
    int zlibfs_outlen;
    uint8_t *zlibfs_outptr;
    int zlibfs_eofseen;
} zlibfs_file_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int zlibfs_fileop_init(void **fsctx,void *ctx);
static int zlibfs_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int zlibfs_fileop_read(void *ref,uint8_t *buf,int len);
static int zlibfs_fileop_write(void *ref,uint8_t *buf,int len);
static int zlibfs_fileop_seek(void *ref,int offset,int how);
static void zlibfs_fileop_close(void *ref);
static void zlibfs_fileop_uninit(void *fsctx);

voidpf zcalloc(voidpf opaque,unsigned items, unsigned size);
void zcfree(voidpf opaque,voidpf ptr);

/*  *********************************************************************
    *  ZLIB fileio dispatch table
    ********************************************************************* */


static uint8_t gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */


/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

const fileio_dispatch_t zlibfs_fileops = {
    "z",
    0,
    zlibfs_fileop_init,
    zlibfs_fileop_open,
    zlibfs_fileop_read,
    zlibfs_fileop_write,
    zlibfs_fileop_seek,
    zlibfs_fileop_close,
    zlibfs_fileop_uninit
};

/*
 * Utility functions needed by the ZLIB routines
 */
voidpf zcalloc(voidpf opaque,unsigned items, unsigned size)
{
    void *ptr;

    ptr = KMALLOC(items*size,0);
    if (ptr) lib_memset(ptr,0,items*size);
    return ptr;
}

void zcfree(voidpf opaque,voidpf ptr)
{
    KFREE(ptr);
}


static int zlibfs_fileop_init(void **newfsctx,void *curfsvoid)
{
    zlibfs_fsctx_t *fsctx;
    fileio_ctx_t *curfsctx = (fileio_ctx_t *) curfsvoid;

    *newfsctx = NULL;

    fsctx = KMALLOC(sizeof(zlibfs_fsctx_t),0);
    if (!fsctx) {
	return CFE_ERR_NOMEM;
	}

    fsctx->zlibfsctx_refcnt = 0;
    fsctx->zlibfsctx_subops  =  curfsctx->ops;
    fsctx->zlibfsctx_subfsctx = curfsctx->fsctx;

    *newfsctx = fsctx;

    return 0;
}


static int get_byte(zlibfs_file_t *file,uint8_t *ch)
{
    int res;

    res = BDREAD(file->zlibfs_fsctx->zlibfsctx_subops,
		 file->zlibfs_subfile,
		 ch,
		 1);

    return res;
}


static int check_header(zlibfs_file_t *file)
{
    int method; /* method byte */
    int flags;  /* flags byte */
    uInt len;
    uint8_t c;
    int res;

    /* Check the gzip magic header */
    for (len = 0; len < 2; len++) {
	res = get_byte(file,&c);
	if (c != gz_magic[len]) {
	    return -1;
	}
    }

    get_byte(file,&c); method = (int) c;
    get_byte(file,&c); flags = (int) c;

    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
	return -1;
	}

    /* Discard time, xflags and OS code: */
    for (len = 0; len < 6; len++) (void)get_byte(file,&c);

    if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
	get_byte(file,&c);
	len = (uInt) c;
	get_byte(file,&c);
	len += ((uInt)c)<<8;
	/* len is garbage if EOF but the loop below will quit anyway */
	while ((len-- != 0) && (get_byte(file,&c) == 1)) ;
    }
    if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
	while ((get_byte(file,&c) == 1) && (c != 0)) ;
    }
    if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
	while ((get_byte(file,&c) == 1) && (c != 0)) ;
    }
    if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
	for (len = 0; len < 2; len++) (void)get_byte(file,&c);
    }

    return 0;
}

static int zlibfs_fileop_open(void **ref,void *fsctx_arg,char *filename,int mode)
{
    zlibfs_fsctx_t *fsctx;
    zlibfs_file_t *file;
    int err;

    if (mode != FILE_MODE_READ) return CFE_ERR_UNSUPPORTED;

    fsctx = (zlibfs_fsctx_t *) fsctx_arg;

    file = KMALLOC(sizeof(zlibfs_file_t),0);
    if (!file) {
	return CFE_ERR_NOMEM;
	}

    file->zlibfs_fileoffset = 0;
    file->zlibfs_fsctx = fsctx;
    file->zlibfs_inbuf = NULL;
    file->zlibfs_outbuf = NULL;
    file->zlibfs_eofseen = 0;

    err = BDOPEN(fsctx->zlibfsctx_subops,&(file->zlibfs_subfile),
		 fsctx->zlibfsctx_subfsctx,filename);

    if (err != 0) {
	goto error2;
	return err;
	}

    /* Open the zstream */

    file->zlibfs_inbuf = KMALLOC(ZLIBFS_BUFSIZE,0);
    file->zlibfs_outbuf = KMALLOC(ZLIBFS_BUFSIZE,0);

    if (!file->zlibfs_inbuf || !file->zlibfs_outbuf) {
	err = CFE_ERR_NOMEM;
	goto error;
	}

    file->zlibfs_stream.next_in = NULL;
    file->zlibfs_stream.avail_in = 0;
    file->zlibfs_stream.next_out = file->zlibfs_outbuf;
    file->zlibfs_stream.avail_out = ZLIBFS_BUFSIZE;
    file->zlibfs_stream.zalloc = (alloc_func)0;
    file->zlibfs_stream.zfree = (free_func)0;

    file->zlibfs_outlen = 0;	
    file->zlibfs_outptr = file->zlibfs_outbuf;

    err = inflateInit2(&(file->zlibfs_stream),-15);
    if (err != Z_OK) {
	err = CFE_ERR;
	goto error;
	}

    check_header(file);

    fsctx->zlibfsctx_refcnt++;

    *ref = file;
    return 0;

error:
    BDCLOSE(file->zlibfs_fsctx->zlibfsctx_subops,file->zlibfs_subfile);
error2:
    if (file->zlibfs_inbuf) KFREE(file->zlibfs_inbuf);
    if (file->zlibfs_outbuf) KFREE(file->zlibfs_outbuf);
    KFREE(file);
    return err;
}

static int zlibfs_fileop_read(void *ref,uint8_t *buf,int len)
{
    zlibfs_file_t *file = (zlibfs_file_t *) ref;
    int res = 0;
    int err;
    int amtcopy;
    int ttlcopy = 0;

    if (len == 0) return 0;

    while (len) {

	/* Figure the amount to copy.  This is the min of what we
	   have left to do and what is available. */
	amtcopy = len;
	if (amtcopy > file->zlibfs_outlen) {
	    amtcopy = file->zlibfs_outlen;
	    }

	/* Copy the data. */

	if (buf) {
	    memcpy(buf,file->zlibfs_outptr,amtcopy);
	    buf += amtcopy;
	    }

	/* Update the pointers. */
	file->zlibfs_outptr += amtcopy;
	file->zlibfs_outlen -= amtcopy;
	len -= amtcopy;
	ttlcopy += amtcopy;

	/* If we've eaten all of the output, reset and call inflate
	   again. */

	if (file->zlibfs_outlen == 0) {
	    /* If no input data to decompress, get some more if we can. */
	    if (file->zlibfs_eofseen) break;
	    if (file->zlibfs_stream.avail_in == 0) {
		res = BDREAD(file->zlibfs_fsctx->zlibfsctx_subops,
			     file->zlibfs_subfile,
			     file->zlibfs_inbuf,		   
			     ZLIBFS_BUFSIZE);
		/* If at EOF or error, get out. */
		if (res <= 0) break;
		file->zlibfs_stream.next_in = file->zlibfs_inbuf;
		file->zlibfs_stream.avail_in = res;
		}

	    /* inflate the input data. */
	    file->zlibfs_stream.next_out = file->zlibfs_outbuf;
	    file->zlibfs_stream.avail_out = ZLIBFS_BUFSIZE;
	    file->zlibfs_outptr = file->zlibfs_outbuf;
	    err = inflate(&(file->zlibfs_stream),Z_SYNC_FLUSH);
	    if (err == Z_STREAM_END) {
		/* We can get a partial buffer fill here. */
	        file->zlibfs_eofseen = 1;
		}
	    else if (err != Z_OK) {
		res = CFE_ERR;
		break;
		}
	    file->zlibfs_outlen = file->zlibfs_stream.next_out -
		file->zlibfs_outptr;
	    }

	}

    file->zlibfs_fileoffset += ttlcopy;

    return (res < 0) ? res : ttlcopy;
}

static int zlibfs_fileop_write(void *ref,uint8_t *buf,int len)
{
    return CFE_ERR_UNSUPPORTED;
}

static int zlibfs_fileop_seek(void *ref,int offset,int how)
{
    zlibfs_file_t *file = (zlibfs_file_t *) ref;
    int res;
    int delta;

    switch (how) {
	case FILE_SEEK_BEGINNING:
	    delta = offset - file->zlibfs_fileoffset;
	    break;
	case FILE_SEEK_CURRENT:
	    delta = offset;
	    break;
	default:
	    return CFE_ERR_UNSUPPORTED;
	    break;
	}

    /* backward seeking not allowed on compressed streams */
    if (delta < 0) {
	return CFE_ERR_UNSUPPORTED;
	}

    res = zlibfs_fileop_read(ref,NULL,delta);

    if (res < 0) return res;

    return file->zlibfs_fileoffset;
}


static void zlibfs_fileop_close(void *ref)
{
    zlibfs_file_t *file = (zlibfs_file_t *) ref;

    file->zlibfs_fsctx->zlibfsctx_refcnt--;

    inflateEnd(&(file->zlibfs_stream));

    BDCLOSE(file->zlibfs_fsctx->zlibfsctx_subops,file->zlibfs_subfile);

    KFREE(file);
}

static void zlibfs_fileop_uninit(void *fsctx_arg)
{
    zlibfs_fsctx_t *fsctx = (zlibfs_fsctx_t *) fsctx_arg;

    if (fsctx->zlibfsctx_refcnt) {
	xprintf("zlibfs_fileop_uninit: warning: refcnt not zero\n");
	}

    BDUNINIT(fsctx->zlibfsctx_subops,fsctx->zlibfsctx_subfsctx);

    KFREE(fsctx);
}


#endif
