/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Queue management prototypes		File: lib_queue.h
    *  
    *  Constants, structures, and function prototypes for the queue
    *  manager.
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


#ifndef _LIB_QUEUE_H
#define _LIB_QUEUE_H

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define q_init(q) (q)->q_prev = (q), (q)->q_next = (q)
#define q_isempty(q) ((q)->q_next == (q))
#define q_getfirst(q) ((q)->q_next)
#define q_getlast(q) ((q)->q_prev)

/*  *********************************************************************
    *  Types
    ********************************************************************* */


typedef struct queue_s {
    struct queue_s *q_next;
    struct queue_s *q_prev;
} queue_t;


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

void q_enqueue(queue_t *,queue_t *);
void q_dequeue(queue_t *);
queue_t *q_deqnext(queue_t *);
int q_map(queue_t *qb,int (*func)(queue_t *,unsigned int,unsigned int),
	  unsigned int a,unsigned int b);
int q_count(queue_t *);
int q_find(queue_t *,queue_t *);


#endif
