/*  *********************************************************************
    *  SB1125 Board Support Package
    *  
    *  Xilinx programming module		File: xilinx.c
    *
    *  Routines to program Xilinx FPGAs.
    *  
    *  Author:  Stefan Ludwig, Mitch Lichtenberg
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
#include "lib_printf.h"
#include "sbmips.h"
#include "sb1250_defs.h"
#include "sb1250_regs.h"

#include "xilinx.h"

#define DEBUG

static int xilinx_open(xpgminfo_t *xi) 
{
    uint64_t val = SBREADCSR(A_GPIO_DIRECTION);

    /*
     * Save old direction register and set things the way
     * we want for programming.
     */

    xi->olddir = val;

    /* set GPIO PROGRAM_pin, DIN_pin and CCLK_pin to output */
    val |= (xi->PGMpin | xi->DINpin | xi->CCLKpin);

    SBWRITECSR(A_GPIO_DIRECTION,val);
    return 0;
}

static int xilinx_close(xpgminfo_t *xi)
{
    /*
     * Set the direction bits back the way they were.
     */
    
    SBWRITECSR(A_GPIO_DIRECTION,(xi->olddir));

    return 0;
}

static int xilinx_reset(xpgminfo_t *xi)
{
    int i;

    /* pull PROG and CCLK low */
    SBWRITECSR(A_GPIO_PIN_CLR,(xi->PGMpin | xi->CCLKpin));

    /* wait for INIT low */
    for (i = 0; i != XPGM_TIMEOUT; i++) {
	if (!(SBREADCSR(A_GPIO_READ) & (xi->INITpin))) break;
	}

    if (i == XPGM_TIMEOUT) {
	return XPGM_ERR_INIT_NEVER_LOW;
	}

    /* pull prog high */
    SBWRITECSR(A_GPIO_PIN_SET,(xi->PGMpin));

    /* wait for INIT high */
    for (i = 0; i != XPGM_TIMEOUT; i++) {
	if ((SBREADCSR(A_GPIO_READ) & (xi->INITpin))) break;
	}

    if (i == XPGM_TIMEOUT) {
	return XPGM_ERR_INIT_NEVER_HIGH;
	}
    return 0;
}


int xilinx_program(xpgminfo_t *xi,uint8_t *configBits, int nofBits) 
{

    int i, rc;

    if (xilinx_open(xi)) {
	return XPGM_ERR_OPEN_FAILED;
	}

    rc = xilinx_reset(xi);
    if (rc) {
	xilinx_close(xi);
	return rc;
	}

    /* wait for 4 us */
    for (i = 0; i < 4000; i++) {
	SBREADCSR(A_GPIO_READ);
	}

    /* load bitstream */
    for (i = 0; i < nofBits; i++) {
	if ((configBits[i / 8] << (i % 8)) & 0x80) {
	    SBWRITECSR(A_GPIO_PIN_SET,xi->DINpin);
	    }
	else {
	    SBWRITECSR(A_GPIO_PIN_CLR,xi->DINpin);
	    }
	SBWRITECSR(A_GPIO_PIN_CLR,xi->CCLKpin);
	SBWRITECSR(A_GPIO_PIN_SET,xi->CCLKpin);


	/* ensure INIT high */
#ifndef DEBUG
	if (!(SBREADCSR(A_GPIO_READ) & xi->INITpin)) {
	    xilinx_close(xi);
	    return XPGM_ERR_INIT_NOT_HIGH;
	    }
#endif
	}

    /* wait for 4 us */
    for (i = 0; i < 4000; i++) {
	SBREADCSR(A_GPIO_READ);
	}

    /* if DONE not high, give it a few CCLKs */
    if (!(SBREADCSR(A_GPIO_READ) & xi->DONEpin)) {
	
	SBWRITECSR(A_GPIO_PIN_SET,xi->DINpin);
	for (i = 0; i < 10; i++) {
	    SBWRITECSR(A_GPIO_PIN_CLR,xi->CCLKpin);
	    SBWRITECSR(A_GPIO_PIN_SET,xi->CCLKpin);
	    }
	}

    if (!(SBREADCSR(A_GPIO_READ) & xi->DONEpin)) {
	xilinx_close(xi);
	return XPGM_ERR_DONE_NOT_HIGH;
	}

    xilinx_close(xi);
    return 0;
}
