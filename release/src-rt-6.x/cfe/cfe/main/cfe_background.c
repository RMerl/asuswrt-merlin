/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Timer routines				File: cfe_timer.c
    *  
    *  This module manages the list of routines to call periodically
    *  during "background processing."  CFE has no interrupts or
    *  multitasking, so this is just where all the polling routines
    *  get called from.
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
#include "lib_printf.h"

#include "cfe_timer.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define MAX_BACKGROUND_TASKS	16


/*  *********************************************************************
    *  Globals
    ********************************************************************* */

static void (*cfe_bg_tasklist[MAX_BACKGROUND_TASKS])(void *);
static void *cfe_bg_args[MAX_BACKGROUND_TASKS];


/*  *********************************************************************
    *  cfe_bg_init()
    *  
    *  Initialize the background task list
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_bg_init(void)
{
    memset(cfe_bg_tasklist,0,sizeof(cfe_bg_tasklist));
}


/*  *********************************************************************
    *  cfe_bg_add(func,arg)
    *  
    *  Add a function to be called periodically in the background
    *  polling loop.
    *  
    *  Input parameters: 
    *  	   func - function pointer
    *      arg - arg to pass to function
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_bg_add(void (*func)(void *x),void *arg)
{
    int idx;

    for (idx = 0; idx < MAX_BACKGROUND_TASKS; idx++) {
	if (cfe_bg_tasklist[idx] == NULL) {
	    cfe_bg_tasklist[idx] = func;
	    cfe_bg_args[idx] = arg;
	    return;
	    }
	}
}

/*  *********************************************************************
    *  cfe_bg_remove(func)
    *  
    *  Remove a function from the background polling loop
    *  
    *  Input parameters: 
    *  	   func - function pointer
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void cfe_bg_remove(void (*func)(void *))
{
    int idx;

    for (idx = 0; idx < MAX_BACKGROUND_TASKS; idx++) {
	if (cfe_bg_tasklist[idx] == func) break;
	}

    if (idx == MAX_BACKGROUND_TASKS) return;

    for (; idx < MAX_BACKGROUND_TASKS-1; idx++) {
	cfe_bg_tasklist[idx] = cfe_bg_tasklist[idx+1];
	cfe_bg_args[idx] = cfe_bg_args[idx+1];
	}

    cfe_bg_tasklist[idx] = NULL;
    cfe_bg_args[idx] = NULL;
}


/*  *********************************************************************
    *  background()
    *  
    *  The main loop and other places that wait for stuff call
    *  this routine to make sure the background handlers get their
    *  time.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void background(void)
{
    int idx;
    void (*func)(void *arg);

    for (idx = 0; idx < MAX_BACKGROUND_TASKS; idx++) {
	func = cfe_bg_tasklist[idx];
	if (func == NULL) break;
	(*func)(cfe_bg_args[idx]);
	}
}
