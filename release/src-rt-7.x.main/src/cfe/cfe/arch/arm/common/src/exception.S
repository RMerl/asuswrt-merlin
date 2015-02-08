/*  *********************************************************************
    *  BCM947xx Board Support Package
    *  
    *  Exception Handler			File: exception.S
    *  
    *  Author: 
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


//#include "sbmips.h"
//#include "exception.h"
#include <arminc.h>
#include <armmacros.h>
//#include "cpu_config.h"		/* for definition of HAZARD and ERET */
//#include "bsp_config.h"

/*  *********************************************************************
    *  _exc_setvector(xtype,addr)
    *  
    *  Set an exception vector address
    *  
    *  Input parameters: 
    *  	   xtype - exception vector type
    *  	   addr - routine address
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

FUNC(_exc_setvector)

		mov pc,lr

END(_exc_setvector)


/*  *********************************************************************
    *  _exc_cache_crash_sim()
    *  
    *  As _exc_crash_sim, but distinguish cache error exception.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing - does not return
    ********************************************************************* */


FUNC(_exc_cache_crash_sim)
END(_exc_cache_crash_sim)


/*  *********************************************************************
    *  _exc_restart()
    *  
    *  Restart the firmware at the boot address
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

FUNC(_exc_restart)

		ldr	lr,=0xFFFF0000		/* ROM restart vector */
		mov	pc,lr

END(_exc_restart)


/*  *********************************************************************
    * _exc_clear_sr_exl()
    *	
    * Clear SR(EXL) and return to caller.
    *
    * Input parameters:	
    *	nothing
    *
    * Return value:	
    *	nothing					
    ********************************************************************* */
	
FUNC(_exc_clear_sr_exl)
		mov pc,lr
END(_exc_clear_sr_exl)			

/*  *********************************************************************
    * _exc_clear_sr_erl()
    *	
    * Clear SR(ERL) and return to caller.
    *
    * Input parameters:	
    *	nothing
    *
    * Return value:	
    *	nothing					
    ********************************************************************* */
	
FUNC(_exc_clear_sr_erl)
		mov pc,lr
	
END(_exc_clear_sr_erl)			
	

/*  *********************************************************************
    *  End
    ********************************************************************* */
