/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Timer defs and prototypes		File: cfe_timer.h
    *  
    *  Definitions, prototypes, and macros related to the timer.
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

#ifndef _CFE_TIMER_T
#define _CFE_TIMER_T

void cfe_bg_init(void);
void cfe_bg_add(void (*func)(void *),void *arg);
void cfe_bg_remove(void (*func)(void *));

void background(void);

#define POLL() background()

typedef int64_t cfe_timer_t;

void cfe_timer_init(void);
extern volatile int64_t cfe_ticks;
extern int cfe_cpu_speed;

void cfe_sleep(int ticks);
void cfe_usleep(int usec);
void cfe_nsleep(int nsec);

#define CFE_HZ 10			/* ticks per second */

#define TIMER_SET(x,v)  x = cfe_ticks + (v)
#define TIMER_EXPIRED(x) ((x) && (cfe_ticks > (x)))
#define TIMER_CLEAR(x) x = 0
#define TIMER_RUNNING(x) ((x) != 0)

#endif
