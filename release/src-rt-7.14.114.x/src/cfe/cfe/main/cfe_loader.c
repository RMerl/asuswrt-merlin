/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Loader API 				File: cfe_loader.c
    *  
    *  This is the main API for the program loader.  CFE supports
    *  multiple "installable" methods for loading programs, allowing
    *  us to deal with a variety of methods for moving programs
    *  into memory for execution.
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

#include "cfe_error.h"

#include "cfe.h"
#include "cfe_fileops.h"

#include "cfe_loader.h"

#include "bsp_config.h"


/*  *********************************************************************
    *  Externs
    ********************************************************************* */

#if CFG_LDR_ELF
extern const cfe_loader_t elfloader;
#endif
extern const cfe_loader_t rawloader;
#if CFG_LDR_SREC
extern const cfe_loader_t srecloader;
#endif

/*  *********************************************************************
    *  Loader list
    ********************************************************************* */

const cfe_loader_t * const cfe_loaders[] = {
#if CFG_LDR_ELF
    &elfloader,
#endif
    &rawloader,
#if CFG_LDR_SREC
    &srecloader,
#endif
    NULL};

/*  *********************************************************************
    *  cfe_findloader(name)
    *  
    *  Find a loader by name
    *  
    *  Input parameters: 
    *  	   name - name of loader to find
    *  	   
    *  Return value:
    *  	   pointer to loader structure or NULL
    ********************************************************************* */

const cfe_loader_t *cfe_findloader(char *name)
{
    const cfe_loader_t * const *ldr;

    ldr = cfe_loaders;

    while (*ldr) {
	if (strcmp(name,(*ldr)->name) == 0) return (*ldr);
	ldr++;
	}

    return NULL;
}


/*  *********************************************************************
    *  cfe_load_progam(name,args)
    *  
    *  Look up a loader and call it.
    *  
    *  Input parameters: 
    *  	   name - name of loader to run
    *  	   args - arguments to pass
    *  	   
    *  Return value:
    *  	   return value
    ********************************************************************* */

int cfe_load_program(char *name,cfe_loadargs_t *la)
{
    const cfe_loader_t *ldr;
    int res;

    ldr = cfe_findloader(name);
    if (!ldr) return CFE_ERR_LDRNOTAVAIL;

    res = LDRLOAD(ldr,la);

    return res;
}
