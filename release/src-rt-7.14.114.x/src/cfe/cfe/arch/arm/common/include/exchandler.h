/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Exception Handler definitions		File: exchandler.h
    *  
    *  Exception handler functions and constants
    *  
    *  Author:  Binh Vo (binh@broadcom.com)
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


#ifndef _LIB_SETJMP_H
#include "lib_setjmp.h"
#endif

#ifndef _LIB_QUEUE_H
#include "lib_queue.h"
#endif

#define MEM_BYTE     1
#define MEM_HALFWORD 2
#define MEM_WORD     3
#define MEM_QUADWORD 4

#define EXC_NORMAL_RETURN 0
#define EXC_CHAIN_EXC     1

typedef struct jmpbuf_s {
  queue_t stack;
  jmp_buf jmpbuf;
} jmpbuf_t;

typedef struct exc_handler_s {
  int catch_exc;
  int chain_exc;
  queue_t jmpbuf_stack;
} exc_handler_t;

extern exc_handler_t exc_handler;

jmpbuf_t *exc_initialize_block(void);
void exc_cleanup_block(jmpbuf_t *);
void exc_cleanup_handler(jmpbuf_t *, int);
void exc_longjmp_handler(void);
int mem_peek(void*, long, int);
int mem_poke(long, uint64_t, int);


#define exc_try(jb) (lib_setjmp(((jb)->jmpbuf)))
