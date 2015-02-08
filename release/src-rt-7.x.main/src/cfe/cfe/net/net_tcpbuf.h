/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP Protocol Definitions			File: net_tcp.h
    *  
    *  This file contains TCP protocol-specific definitions
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
    *  Macros
    ********************************************************************* */

#define tmb_remlen(tmb) ((tmb)->tmb_bufsize - (tmb)->tmb_len)
#define tmb_curlen(tmb) ((tmb)->tmb_len)


/*  *********************************************************************
    *  Modulo Buffer
    ********************************************************************* */

typedef struct tcpmodbuf_s {
    uint8_t 	*tmb_buf;		/* Buffer */
    int		tmb_bufsize;		/* size of buffer */
    int		tmb_addptr;		/* current "add" pointer */
    int		tmb_remptr;		/* current "remove" pointer */
    int		tmb_len;		/* amount of data in the buffer */
} tcpmodbuf_t;


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

void tmb_init(tcpmodbuf_t *buf);
void tmb_adjust(tcpmodbuf_t *buf,int amt);
int tmb_alloc(tcpmodbuf_t *buf,int size);
void tmb_free(tcpmodbuf_t *buf);
int tmb_copyin(tcpmodbuf_t *tmb,uint8_t *buf,int len,int update);
int tmb_copyout(tcpmodbuf_t *tmb,uint8_t *buf,int len,int update);
int tmb_copyout2(tcpmodbuf_t *tmb,uint8_t *buf,int offset,int len);
