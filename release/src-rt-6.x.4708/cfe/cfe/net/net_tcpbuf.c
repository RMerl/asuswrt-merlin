/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP Protocol 				File: net_tcp.c
    *  
    *  This file contains a very simple TCP.
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

#include "net_tcpbuf.h"



/*  *********************************************************************
    *  tmb_init(buf)
    *  
    *  Initialize a modulo buffer's pointers to the "empty" state
    *  
    *  Input parameters: 
    *  	   buf - modulo buffer structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void tmb_init(tcpmodbuf_t *buf)
{
    buf->tmb_addptr = 0;
    buf->tmb_remptr = 0;
    buf->tmb_len = 0;
}

/*  *********************************************************************
    *  tmb_adjust(buf,amt)
    *  
    *  Move the "remove" pointer ahead by 'amt' without retrieving the
    *  data.
    *  
    *  Input parameters: 
    *  	   buf - modulo buffer structure
    *      amt - number of bytes to move
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


void tmb_adjust(tcpmodbuf_t *buf,int amt)
{
    buf->tmb_len -= amt;
    buf->tmb_remptr = (buf->tmb_remptr + amt) % buf->tmb_bufsize;
}

/*  *********************************************************************
    *  tmb_alloc(buf,size)
    *  
    *  Allocate memory for the modulo buffer.
    *  
    *  Input parameters: 
    *  	   buf - modulo buffer structure
    *  	   size - size of data in modulo buffer
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   -1 if error
    ********************************************************************* */

int tmb_alloc(tcpmodbuf_t *buf,int size)
{
    buf->tmb_buf = KMALLOC(size,0);
    if (!buf->tmb_buf) return -1;
    buf->tmb_bufsize = size;

    tmb_init(buf);

    return 0;
}

/*  *********************************************************************
    *  tmb_free(buf)
    *  
    *  Free memory associated with the modulo buffer
    *  
    *  Input parameters: 
    *  	   buf - modulo buffer
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void tmb_free(tcpmodbuf_t *buf)
{
    if (buf->tmb_buf) KFREE(buf->tmb_buf);
    buf->tmb_buf = NULL;
}


/*  *********************************************************************
    *  tmb_copyin(tmb,buf,len,update)
    *  
    *  Copy data into the modulo buffer at the 'add' pointer
    *  
    *  Input parameters: 
    *  	   tmb - modulo buffer structure
    *  	   buf,len - buffer and length of buffer to copy in
    *  	   update - true to advance 'add' pointer (usually true for copyin)
    *  	   
    *  Return value:
    *  	   number of bytes actually added to the buffer
    ********************************************************************* */

int tmb_copyin(tcpmodbuf_t *tmb,uint8_t *buf,int len,int update)
{
    int maxlen;
    int l;
    int newptr;
    int retlen;

    if (len == 0) return 0;

    /* Set 'maxlen' to the max # of bytes we will send now */
    maxlen = tmb->tmb_bufsize - tmb->tmb_len;
    if (maxlen > len) maxlen = len;

    retlen = maxlen;		/* we'll return this later. */

    /* Copy the bytes into the buffer and deal with buffer wrap */
    l = tmb->tmb_bufsize - tmb->tmb_addptr;
    if (l > maxlen) l = maxlen;

    memcpy(tmb->tmb_buf + tmb->tmb_addptr,buf,l);
    maxlen -= l;
    buf += l;

    if (maxlen) {
	memcpy(tmb->tmb_buf,buf,maxlen);
	newptr = maxlen;
	}
    else {
	newptr = tmb->tmb_addptr + l;
	}

    if (update) {
	tmb->tmb_len += retlen;		/* this many more in the buffer */
	tmb->tmb_addptr = newptr;
	}

    return retlen;
}

/*  *********************************************************************
    *  tmb_copyout(tmb,buf,len,update)
    *  
    *  Copy data out of the modulo buffer from the 'remove' pointer
    *  
    *  Input parameters: 
    *  	   tmb - modulo buffer structure
    *  	   buf,len - buffer and length of buffer to copy out
    *  	   update - true to advance 'remove' pointer 
    *  	   
    *  Return value:
    *  	   number of bytes actually removed from the buffer
    ********************************************************************* */

int tmb_copyout(tcpmodbuf_t *tmb,uint8_t *buf,int len,int update)
{
    int maxlen;
    int l;
    int newptr;
    int retlen;

    /* Compute how many bytes to return */
    maxlen = tmb->tmb_len;
    if (maxlen > len) maxlen = len;

    retlen = maxlen;

    l = tmb->tmb_bufsize - tmb->tmb_remptr;
    if (l > maxlen) l = maxlen;

    memcpy(buf,tmb->tmb_buf + tmb->tmb_remptr,l);
    buf += l;
    maxlen -= l;

    if (maxlen) {
	memcpy(buf,tmb->tmb_buf,maxlen);
	newptr = maxlen;
	}
    else {
	newptr = tmb->tmb_remptr + l;
	}

    if (update) {
	tmb->tmb_len -= retlen;
	tmb->tmb_remptr = newptr;
	}

    return retlen;
}

/*  *********************************************************************
    *  tmb_copyout2(tmb,buf,offset,len)
    *  
    *  Copy data out of the modulo buffer from a specific offset from
    *  the remove pointer.  This is done without updating the
    *  remove pointer - we use this to dig data out when segmenting
    *  packets for transmission.
    *  
    *  Input parameters: 
    *  	   tmb - modulo buffer structure
    *  	   buf,len - buffer and length of buffer to copy out
    *  	   offset - offset from remove pointer to start
    *  	   
    *  Return value:
    *  	   number of bytes actually copied out of the buffer
    ********************************************************************* */

int tmb_copyout2(tcpmodbuf_t *tmb,uint8_t *buf,int offset,int len)
{
    int maxlen;
    int l;
    int retlen;
    int remptr;

    /* Compute how many bytes to return */
    maxlen = tmb->tmb_len - offset;

    /* if offset is beyond length, get out now. */
    if (maxlen <= 0) return 0;

    /* otherwise choose min(max_can_copy,max_to_copy) */
    if (maxlen > len) maxlen = len;
    retlen = maxlen;

    /* Adjust remove pointer for offset */
    remptr = (tmb->tmb_remptr + offset) % tmb->tmb_bufsize;

    l = tmb->tmb_bufsize - remptr;
    if (l > maxlen) l = maxlen;

    memcpy(buf,tmb->tmb_buf + remptr,l);
    buf += l;
    maxlen -= l;

    if (maxlen) {
	memcpy(buf,tmb->tmb_buf,maxlen);
	}

    return retlen;
}
