/*  *********************************************************************
 *  Broadcom Common Firmware Environment (CFE)
 *
 *  "Memory" file system			File: cfe_memfs.c
 *
 *  This filesystem driver lets you data from memory by treating
 *  it as a filesystem. Note that, there is no concept of filename..
 *  directly memory location to read from. Also, no writing is permitted for now
 *
 *  Author:  Abhay Bhorkar (abhorkar@broadcom.com)
 *
 * $Id: cfe_memfs.c 241205 2011-02-17 21:57:41Z $
 *
 * ********************************************************************
 *
 *  Copyright 2005
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
 * ********************************************************************
 */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"

#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_loader.h"

/*  *********************************************************************
 *  Memory context
 *  ********************************************************************
 */

/*
 * File system context - describes overall file system info,
 * such as the handle to the underlying device.
 */

typedef struct memory_fsctx_s {
	int memory_tmp;			/* context not really needed */
} memory_fsctx_t;

/*
 * File context - describes an open file on the file system.
 */
typedef struct memory_file_s {
	memory_fsctx_t *memory_fsctx;
	uint8_t *memory_bptr;
	int memory_blen;
	int memory_offset;
	long memory_start;
} memory_file_t;

/*  *********************************************************************
 *  Prototypes
 *  ********************************************************************
 */

static int memory_fileop_init(void **fsctx, void *devicename);
static int memory_fileop_open(void **ref, void *fsctx, char *filename, int mode);
static int memory_fileop_read(void *ref, uint8_t *buf, int len);
static int memory_fileop_write(void *ref, uint8_t *buf, int len);
static int memory_fileop_seek(void *ref, int offset, int how);
static void memory_fileop_close(void *ref);
static void memory_fileop_uninit(void *fsctx);

/*  *********************************************************************
 *  RAW fileio dispatch table
 *  ********************************************************************
 */

const fileio_dispatch_t memory_fileops = {
	"memory",
	LOADFLG_SPECADDR,
	memory_fileop_init,
	memory_fileop_open,
	memory_fileop_read,
	memory_fileop_write,
	memory_fileop_seek,
	memory_fileop_close,
	memory_fileop_uninit
};

static int memory_fileop_init(void **newfsctx, void *dev)
{
	memory_fsctx_t *fsctx;

	*newfsctx = NULL;

	fsctx = KMALLOC(sizeof(memory_fsctx_t), 0);
	if (!fsctx) {
		return CFE_ERR_NOMEM;
	}

	fsctx->memory_tmp = 0;
	*newfsctx = fsctx;

	return 0;
}

/* filename describes the location in memory in string */
static int memory_fileop_open(void **ref, void *fsctx_arg, char *filename, int mode)
{
	memory_fsctx_t *fsctx;
	memory_file_t *file;

	if (mode != FILE_MODE_READ)
	    return CFE_ERR_UNSUPPORTED;

	fsctx = (memory_fsctx_t *) fsctx_arg;

	file = KMALLOC(sizeof(memory_file_t), 0);
	if (!file) {
		return CFE_ERR_NOMEM;
	}

	if (filename[0] == ':')
		filename++; /* Skip over colon */

	file->memory_start = xtoi(filename);

	file->memory_offset = 0;

	*ref = file;
	return 0;
}

static int memory_fileop_read(void *ref, uint8_t *buf, int len)
{
	memory_file_t *file = (memory_file_t *) ref;

	memcpy(buf, (void *)(file->memory_start + file->memory_offset), len);

	file->memory_offset += len;

	return len;		/* Any error becomes "EOF" */
}

static int memory_fileop_write(void *ref, uint8_t *buf, int len)
{
	return CFE_ERR_UNSUPPORTED;
}

static int memory_fileop_seek(void *ref, int offset, int how)
{
	memory_file_t *file = (memory_file_t *) ref;
	int newoffset;

	switch (how) {
	case FILE_SEEK_BEGINNING:
		newoffset = offset;
		break;
	case FILE_SEEK_CURRENT:
		newoffset = file->memory_offset + offset;
		break;
	default:
		newoffset = offset;
		break;
	};

	file->memory_offset = newoffset;

	return file->memory_offset;
}

static void memory_fileop_close(void *ref)
{
	memory_file_t *file = (memory_file_t *) ref;

	KFREE(file);
}

static void memory_fileop_uninit(void *fsctx_arg)
{
	memory_fsctx_t *fsctx = (memory_fsctx_t *) fsctx_arg;

	KFREE(fsctx);
}
