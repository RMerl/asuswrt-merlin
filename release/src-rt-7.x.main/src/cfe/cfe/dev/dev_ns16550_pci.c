/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  NS16550 UART driver (PCI)	File: dev_ns16550_pci.c
    *  
    *  This is a console device driver for a PCI NS16550 UART
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
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"

#include "pcivar.h"
#include "pcireg.h"


/* Probe routine for real UART driver */
extern void ns16550_uart_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr);

/* Probe routine for this UART driver. */
static void ns16550pci_uart_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr);

/* We just glom onto the dispatch table in the real driver */
extern const cfe_devdisp_t ns16550_uart_dispatch;

const cfe_driver_t ns16550pci_uart = {
    "PCI NS16550 UART",
    "uart",
    CFE_DEV_SERIAL,
    &ns16550_uart_dispatch,
    ns16550pci_uart_probe
};


static void
ns16550pci_uart_probe(cfe_driver_t *drv,
		      unsigned long probe_a, unsigned long probe_b, 
		      void *probe_ptr)
{
    int index;

    /* 
     * NS16550-compatible UART on the PCI bus
     * probe_a, probe_b and probe_ptr are unused.
     *
     * This is for generic 16550-like UARTs. We use the PCI class and
     * interface codes to identify compatible hardware.  We require
     * 16550 or better compatibility. The 16{6,7,8,9}50 parts are
     * claimed to be 16550 compatible but are as yet untested.
     */

    for (index = 0; ; index++) {
	pcitag_t tag;
	pcireg_t cr;
	phys_addr_t pa;

	if (pci_find_class(PCI_CLASS_COMMUNICATIONS, index, &tag) != 0) {
	    break;
	    }

	cr = pci_conf_read(tag, PCI_CLASS_REG);
	if (PCI_SUBCLASS(cr) == PCI_SUBCLASS_COMMUNICATIONS_SERIAL
	    && PCI_INTERFACE(cr) >= 0x02          /* 16550 .. */
	    && PCI_INTERFACE(cr) <= 0x06) {       /* 16950 */
	    if (pci_map_io(tag, PCI_MAPREG(0), PCI_MATCH_BYTES, &pa) == 0)
		ns16550_uart_probe(drv, pa, 0, NULL);
	    }
	}
}
