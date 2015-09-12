/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  "LZMA compression" file system			File: cfe_lzmafs.c
    *  
    *  This is more of a filesystem "hook" than an actual file system.
    *  You can stick it on the front of the chain of file systems
    *  that CFE calls and it will route data read from the
    *  underlying filesystem through LZMA before passing it up to the
    *  user.
    *  
    *  Author: 
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

#if CFG_LZMA

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_arena.h"

#include "cfe_error.h"

#include "cfe.h"
#include "cfe_mem.h"
#include "initdata.h"
#include "addrspace.h"		/* for macros dealing with addresses */
#include "cfe_fileops.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_console.h"

#include "cfe.h"
#include "LzmaDec.h"

/*  *********************************************************************
    *  LZMAFS context
    ********************************************************************* */

/*
 * File system context - describes overall file system info,
 * such as the handle to the underlying device.
 */

typedef struct lzmafs_fsctx_s {
    void *lzmafsctx_subfsctx;
    const fileio_dispatch_t *lzmafsctx_subops;
    int lzmafsctx_refcnt;
} lzmafs_fsctx_t;

/*
 * File context - describes an open file on the file system.
 * For raw devices, this is pretty meaningless, but we do
 * keep track of where we are.
 */

#define LZMAFS_INBUFSIZE	1024
#define LZMAFS_OUTBUFSIZE	16*1024

typedef struct lzmafs_file_s {
    lzmafs_fsctx_t *lzmafs_fsctx;
    int lzmafs_fileoffset;
    void *lzmafs_subfile;
    uint8_t *lzmafs_inbuf;
    uint8_t *lzmafs_outbuf;
    int lzmafs_inlen;
	int lzmafs_outlen;
	int lzmafs_unpackSize;
	uint8_t *lzmafs_inptr;
	uint8_t *lzmafs_outptr;
	int  lzmafs_eofseen;
	CLzmaDec *lzmafs_state;
}lzmafs_file_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */
static int lzmafs_fileop_init(void **fsctx,void *ctx);
static int lzmafs_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int lzmafs_fileop_read(void *ref,uint8_t *buf,int len);
static int lzmafs_fileop_write(void *ref,uint8_t *buf,int len);
static int lzmafs_fileop_seek(void *ref,int offset,int how);
static void lzmafs_fileop_close(void *ref);
static void lzmafs_fileop_uninit(void *fsctx);

static void *SzAlloc(void *p, size_t size) { p = p; return KMALLOC(size, 0); }
static void SzFree(void *p, void *address) { p = p; KFREE(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

/*  *********************************************************************
    *  LZMA fileio dispatch table
    ********************************************************************* */

const fileio_dispatch_t lzmafs_fileops = {
    "lzma",
    0,
    lzmafs_fileop_init,
    lzmafs_fileop_open,
    lzmafs_fileop_read,
    lzmafs_fileop_write,
    lzmafs_fileop_seek,
    lzmafs_fileop_close,
    lzmafs_fileop_uninit
};

static int lzmafs_fileop_init(void **newfsctx,void *curfsvoid)
{
    lzmafs_fsctx_t *fsctx;
    fileio_ctx_t *curfsctx = (fileio_ctx_t *) curfsvoid;

    *newfsctx = NULL;

    fsctx = KMALLOC(sizeof(lzmafs_fsctx_t),0);
    if (!fsctx) {
		return CFE_ERR_NOMEM;
	}

    fsctx->lzmafsctx_refcnt = 0;
    fsctx->lzmafsctx_subops  =  curfsctx->ops;
    fsctx->lzmafsctx_subfsctx = curfsctx->fsctx;
    *newfsctx = fsctx;
    return 0;
}

static int lzmafs_fileop_open(void **ref,void *fsctx_arg,char *filename,int mode)
{
    lzmafs_fsctx_t *fsctx;
    lzmafs_file_t *file;
    int err = 0, i;
	/* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
	unsigned char header[LZMA_PROPS_SIZE + 8];

	if (mode != FILE_MODE_READ) 
		return CFE_ERR_UNSUPPORTED;

	fsctx = (lzmafs_fsctx_t *) fsctx_arg;

	file = KMALLOC(sizeof(lzmafs_file_t), 0);
	if (!file) {
		return CFE_ERR_NOMEM;
	}

	file->lzmafs_fileoffset = 0;
	file->lzmafs_fsctx = fsctx;
	file->lzmafs_inlen = 0;
	file->lzmafs_eofseen = 0;
	file->lzmafs_state = KMALLOC(sizeof(CLzmaDec), 0);
	if (!file->lzmafs_state)
		goto error2;

	file->lzmafs_outlen = 0;
	
    /* Open the raw file system */
    err = BDOPEN(fsctx->lzmafsctx_subops, &(file->lzmafs_subfile),
		 fsctx->lzmafsctx_subfsctx, filename);
    if (err != 0)
		goto error2;

	err = BDREAD(file->lzmafs_fsctx->lzmafsctx_subops,
				 file->lzmafs_subfile,
				 header,		   
				 sizeof(header));
	if (err < 0)
		goto error;

	file->lzmafs_unpackSize = 0;
	/* uncompressed size */
	for (i = 0; i < 8; i++)
		file->lzmafs_unpackSize += header[LZMA_PROPS_SIZE + i] << (i * 8);

	LzmaDec_Construct(file->lzmafs_state);
	RINOK(LzmaDec_Allocate(file->lzmafs_state, header, LZMA_PROPS_SIZE, &g_Alloc));

	file->lzmafs_inbuf = KMALLOC(LZMAFS_INBUFSIZE, 0);
	file->lzmafs_outbuf = KMALLOC(LZMAFS_OUTBUFSIZE, 0);
	if (!file->lzmafs_inbuf || !file->lzmafs_outbuf) {
		err = CFE_ERR_NOMEM;
		goto error;
	}

	file->lzmafs_outptr = file->lzmafs_outbuf;
	LzmaDec_Init(file->lzmafs_state);	
	fsctx->lzmafsctx_refcnt++;
    *ref = file;
    return 0;

error:
    BDCLOSE(file->lzmafs_fsctx->lzmafsctx_subops, file->lzmafs_subfile);
error2:
    if (file->lzmafs_inbuf) 
		KFREE(file->lzmafs_inbuf);
    if (file->lzmafs_outbuf) 
		KFREE(file->lzmafs_outbuf);
	if (file->lzmafs_state) {
		LzmaDec_Free(file->lzmafs_state, &g_Alloc);
		KFREE(file->lzmafs_state);
	}
	if (file)
		KFREE(file);
    return err;
}

static int 
lzmafs_fileop_read(void *ref, uint8_t *buf, int len)
{
	lzmafs_file_t *file = (lzmafs_file_t *) ref;
	int res = 0;
	int err;
	int amtcopy;
	int ttlcopy = 0;
	unsigned int in_processed;
	ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
	ELzmaStatus status;

    if (len == 0) return 0;

    while (len) {

		/* Figure the amount to copy.  This is the min of what we
	       have left to do and what is available. */
	   amtcopy = len;
	   if (amtcopy > file->lzmafs_outlen) {
		   amtcopy = file->lzmafs_outlen;
	   }

	   /* Copy the data. */
	   if (buf) {
		   memcpy(buf, file->lzmafs_outptr, amtcopy);
		   buf += amtcopy;
	   }

	   /* Update the pointers. */
	   file->lzmafs_outptr += amtcopy;
	   file->lzmafs_outlen -= amtcopy;
	   len -= amtcopy;
	   ttlcopy += amtcopy;
	   file->lzmafs_unpackSize -= amtcopy;

	   if (file->lzmafs_unpackSize == 0) {
		   file->lzmafs_eofseen = 1;
	   }
	   /* If we've eaten all of the output, reset and call decode
	      again. */

	   if (file->lzmafs_outlen == 0) {
			/* If no input data to decompress, get some more if we can. */
			if (file->lzmafs_eofseen) 
				break;
			if (file->lzmafs_inlen == 0) {
				file->lzmafs_inlen = BDREAD(file->lzmafs_fsctx->lzmafsctx_subops,
							file->lzmafs_subfile,
							file->lzmafs_inbuf,
							LZMAFS_INBUFSIZE);
				/* If at EOF or error, get out. */
				if (file->lzmafs_inlen <= 0) 
					break;
				file->lzmafs_inptr = file->lzmafs_inbuf;
			}
			in_processed  = file->lzmafs_inlen;
			file->lzmafs_outlen = LZMAFS_OUTBUFSIZE;
			file->lzmafs_outptr = file->lzmafs_outbuf;
			
			if (file->lzmafs_outlen > file->lzmafs_unpackSize) {
				file->lzmafs_outlen = (SizeT)file->lzmafs_unpackSize;
				finishMode = LZMA_FINISH_END;
			}
			/* decompress the input data. */
			err = LzmaDec_DecodeToBuf(file->lzmafs_state, file->lzmafs_outbuf, (SizeT *)&file->lzmafs_outlen, 
						file->lzmafs_inptr, &in_processed, finishMode, &status);
			file->lzmafs_inptr += in_processed;
			file->lzmafs_inlen -= in_processed;
			if (err != SZ_OK) {
				res = CFE_ERR;
				break;
			}
	    }
	}

    file->lzmafs_fileoffset += ttlcopy;
    return (res < 0) ? res : ttlcopy;
}

static int lzmafs_fileop_write(void *ref,uint8_t *buf,int len)
{
    return CFE_ERR_UNSUPPORTED;
}

static int lzmafs_fileop_seek(void *ref,int offset,int how)
{
	return CFE_ERR_UNSUPPORTED;
}


static void lzmafs_fileop_close(void *ref)
{
    lzmafs_file_t *file = (lzmafs_file_t *) ref;

    file->lzmafs_fsctx->lzmafsctx_refcnt--;

    BDCLOSE(file->lzmafs_fsctx->lzmafsctx_subops,file->lzmafs_subfile);
	if (file->lzmafs_inbuf)
		KFREE(file->lzmafs_inbuf);
	if (file->lzmafs_outbuf)
		KFREE(file->lzmafs_outbuf);
	if (file->lzmafs_state) {
		LzmaDec_Free(file->lzmafs_state, &g_Alloc);
		KFREE(file->lzmafs_state);
	}
	if (file)
		KFREE(file);
}

static void lzmafs_fileop_uninit(void *fsctx_arg)
{
    lzmafs_fsctx_t *fsctx = (lzmafs_fsctx_t *) fsctx_arg;

    if (fsctx->lzmafsctx_refcnt) {
		xprintf("lzmafs_fileop_uninit: warning: refcnt not zero\n");
	}

    BDUNINIT(fsctx->lzmafsctx_subops,fsctx->lzmafsctx_subfsctx);

    KFREE(fsctx);
}
#endif
