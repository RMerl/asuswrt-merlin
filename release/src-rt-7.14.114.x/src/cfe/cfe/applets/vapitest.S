/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Verification Test APIs			File: vapitest.S
    *
    *  This module contains special low-level routines for use
    *  by verification programs.
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


#include "sbmips.h"


#include "vapi.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */
		.text

LEAF(_start)

		VAPI_LOG_SETBUF(0x80030000,0x80040000)


		VAPI_LOG_CONST(0x100,0xABCDEF)
		VAPI_LOG_REG(0x101,sp)
		VAPI_LOG_BUFFER(0x102,testbuf,10)
		VAPI_PUTS("Hello world.\n")
		VAPI_LOG_SOCSTATE(0x103,SOC_AGENT_DUART)
		VAPI_PRINTGPRS();
		VAPI_LOG_CONST(0x1EE,0xEEEEEEEE)
		VAPI_LOG_GPRS(0x199)
		VAPI_SETLEDS('V','A','P','I')

		VAPI_EXIT_CONST(0)


END(_start)

testbuf:	.dword	0x123456789ABCDEF0
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword	0xAABBCCDD
		.dword  0xFEDCBA9876543210



/*  *********************************************************************
    *  End
    ********************************************************************* */
