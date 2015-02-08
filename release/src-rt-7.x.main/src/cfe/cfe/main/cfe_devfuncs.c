/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Device Function stubs			File: cfe_devfuncs.c
    *  
    *  This module contains device function stubs (small routines to
    *  call the standard "iocb" interface entry point to CFE).
    *  There should be one routine here per iocb function call.
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
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"

extern int cfe_iocb_dispatch(cfe_iocb_t *iocb);

static int cfe_strlen(char *name)
{
    int count = 0;

    while (*name) {
	count++;
	name++;
	}

    return count;
}

int cfe_open(char *name)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_OPEN;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_buffer_t);
    iocb.plist.iocb_buffer.buf_offset = 0;
    iocb.plist.iocb_buffer.buf_ptr = (cfe_ptr_t)name;
    iocb.plist.iocb_buffer.buf_length = cfe_strlen(name);

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status < 0) ? iocb.iocb_status : iocb.iocb_handle;
}

int cfe_close(int handle)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_CLOSE;
    iocb.iocb_status = 0;
    iocb.iocb_handle = handle;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = 0;

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status);

}

int cfe_readblk(int handle,cfe_offset_t offset,unsigned char *buffer,int length)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_READ;
    iocb.iocb_status = 0;
    iocb.iocb_handle = handle;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_buffer_t);
    iocb.plist.iocb_buffer.buf_offset = offset;
    iocb.plist.iocb_buffer.buf_ptr = buffer;
    iocb.plist.iocb_buffer.buf_length = length;

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status < 0) ? iocb.iocb_status : iocb.plist.iocb_buffer.buf_retlen;
}

int cfe_read(int handle,unsigned char *buffer,int length)
{
    return cfe_readblk(handle,0,buffer,length);
}


int cfe_writeblk(int handle,cfe_offset_t offset,unsigned char *buffer,int length)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_WRITE;
    iocb.iocb_status = 0;
    iocb.iocb_handle = handle;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_buffer_t);
    iocb.plist.iocb_buffer.buf_offset = offset;
    iocb.plist.iocb_buffer.buf_ptr = buffer;
    iocb.plist.iocb_buffer.buf_length = length;

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status < 0) ? iocb.iocb_status : iocb.plist.iocb_buffer.buf_retlen;
}

int cfe_write(int handle,unsigned char *buffer,int length)
{
    return cfe_writeblk(handle,0,buffer,length);
}


int cfe_ioctl(int handle,unsigned int ioctlnum,
	      unsigned char *buffer,int length,int *retlen,
	      cfe_offset_t offset)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_IOCTL;
    iocb.iocb_status = 0;
    iocb.iocb_handle = handle;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_buffer_t);
    iocb.plist.iocb_buffer.buf_offset = offset;
    iocb.plist.iocb_buffer.buf_ioctlcmd = (cfe_offset_t) ioctlnum;
    iocb.plist.iocb_buffer.buf_ptr = buffer;
    iocb.plist.iocb_buffer.buf_length = length;

    cfe_iocb_dispatch(&iocb);

    if (retlen) *retlen = iocb.plist.iocb_buffer.buf_retlen;
    return iocb.iocb_status;
}

int cfe_inpstat(int handle)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_INPSTAT;
    iocb.iocb_status = 0;
    iocb.iocb_handle = handle;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_inpstat_t);
    iocb.plist.iocb_inpstat.inp_status = 0;

    cfe_iocb_dispatch(&iocb);

    if (iocb.iocb_status < 0) return iocb.iocb_status;

    return iocb.plist.iocb_inpstat.inp_status;

}

long long cfe_getticks(void)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_FW_GETTIME;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_time_t);
    iocb.plist.iocb_time.ticks = 0;

    cfe_iocb_dispatch(&iocb);

    return iocb.plist.iocb_time.ticks;

}

int cfe_getenv(char *name,char *dest,int destlen)
{
    cfe_iocb_t iocb;

    *dest = NULL;

    iocb.iocb_fcode = CFE_CMD_ENV_GET;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_envbuf_t);
    iocb.plist.iocb_envbuf.enum_idx = 0;
    iocb.plist.iocb_envbuf.name_ptr = (cfe_ptr_t)name;
    iocb.plist.iocb_envbuf.name_length = strlen(name)+1;
    iocb.plist.iocb_envbuf.val_ptr = (cfe_ptr_t)dest;
    iocb.plist.iocb_envbuf.val_length = destlen;

    cfe_iocb_dispatch(&iocb);

    return iocb.iocb_status;
}

int cfe_exit(int warm,int code)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_FW_RESTART;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = warm ? CFE_FLG_WARMSTART : 0;
    iocb.iocb_psize = sizeof(iocb_exitstat_t);
    iocb.plist.iocb_exitstat.status = code;

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status);

}

int cfe_flushcache(int flg)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_FW_FLUSHCACHE;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = flg;
    iocb.iocb_psize = 0;

    cfe_iocb_dispatch(&iocb);

    return iocb.iocb_status;
}

int cfe_getdevinfo(char *name)
{
    cfe_iocb_t iocb;

    iocb.iocb_fcode = CFE_CMD_DEV_GETINFO;
    iocb.iocb_status = 0;
    iocb.iocb_handle = 0;
    iocb.iocb_flags = 0;
    iocb.iocb_psize = sizeof(iocb_buffer_t);
    iocb.plist.iocb_buffer.buf_offset = 0;
    iocb.plist.iocb_buffer.buf_ptr = (cfe_ptr_t)name;
    iocb.plist.iocb_buffer.buf_length = cfe_strlen(name);

    cfe_iocb_dispatch(&iocb);

    return (iocb.iocb_status < 0) ? iocb.iocb_status : (int)iocb.plist.iocb_buffer.buf_devflags;
}
