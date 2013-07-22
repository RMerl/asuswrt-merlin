/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Queue Management routines		File: lib_queue.c
    *  
    *  Routines to manage doubly-linked queues.
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
#include "lib_queue.h"


/*  *********************************************************************
    *  Q_ENQUEUE(qb,item)	
    *  				
    *  Add item to a queue
    *  				
    *  Input Parameters: 	
    *      qb - queue block	
    *      item - item to add	
    *      			
    *  Return Value:		
    *      Nothing.		
    ********************************************************************* */

void q_enqueue(queue_t *qb,queue_t *item)
{
    qb->q_prev->q_next = item;
    item->q_next = qb;
    item->q_prev = qb->q_prev;
    qb->q_prev = item;
}


/*  *********************************************************************
    *  Q_DEQUEUE(element)	
    *  				
    *  Remove an element from the queue
    *  					
    *  Input Parameters: 		
    *      element - element to remove	
    *      				
    *  Return Value:			
    *      Nothing.			
    ********************************************************************* */

void q_dequeue(queue_t *item)
{
    item->q_prev->q_next = item->q_next;
    item->q_next->q_prev = item->q_prev;
}


/*  *********************************************************************
    *  Q_DEQNEXT(qb)
    *
    *  Dequeue next element from the specified queue
    * 
    *  Input Parameters: 	
    *      qb - queue block	
    *      			
    *  Return Value:		
    *      next element, or NULL
    ********************************************************************* */

queue_t *q_deqnext(queue_t *qb)
{
    if (qb->q_next == qb) {
	return NULL;
	}

    qb = qb->q_next;

    qb->q_prev->q_next = qb->q_next;
    qb->q_next->q_prev = qb->q_prev;

    return qb;
}


/*  *********************************************************************
    *  Q_MAP(qb)
    * 
    *  "Map" a queue, calling the specified function for each
    *  element in the queue
    *  
    *  If the function returns nonzero, q_map will terminate.
    *  
    *  Input Parameters: 		
    *      qb - queue block		
    *      fn - function pointer	
    *      a,b - parameters for the function
    *  
    *  Return Value:	
    *      return value from function, or zero if entire queue 
    *      was mapped.	
    ********************************************************************* */

int q_map(queue_t *qb, int (*func)(queue_t *,unsigned int,unsigned int),
	  unsigned int a,unsigned int b)
{
    queue_t *qe;
    queue_t *nextq;
    int res;

    qe = qb;

    qe = qb->q_next;

    while (qe != qb) {
	nextq = qe->q_next;
	if ((res = (*func)(qe,a,b))) return res;
	qe = nextq;
	}

    return 0;
}





/*  *********************************************************************
    *  Q_COUNT(qb)							*
    *  									*
    *  Counts the elements on a queue (not interlocked)			*
    *  									*
    *  Input Parameters: 						*
    *      qb - queue block						*
    *      								*
    *  Return Value:							*
    *      number of elements						*
    ********************************************************************* */
int q_count(queue_t *qb)
{
    queue_t *qe;
    int res = 0;

    qe = qb;

    while (qe->q_next != qb) {
	qe = qe->q_next;
	res++;
	}

    return res;
}




/*  *********************************************************************
    *  Q_FIND(qb,item)
    *  
    *  Determines if a particular element is on a queue.
    *  				
    *  Input Parameters: 	
    *      qb - queue block
    *      item - queue element 
    *      	
    *  Return Value:
    *      0 - not on queue        
    *      >0 - position on queue  
    ********************************************************************* */
int q_find(queue_t *qb,queue_t *item)
{
    queue_t *q;
    int res = 1;

    q = qb->q_next;

    while (q != item) {
	if (q == qb) return 0;
	q = q->q_next;
	res++;
	}

    return res;
}
