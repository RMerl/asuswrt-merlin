/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  SP1011 (Sturgeon) Support			File: dev_sp1011.c
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

#include "pcireg.h"
#include "pcivar.h"

extern int eoi_implemented;

void sturgeon_setup (pcitag_t tag, pci_flags_t flags);


/* Sturgeon LDT-to-PCI bridge (LPB) specific definitions.  */

#define	PCI_VENDOR_API			0x14d9
#define PCI_PRODUCT_API_STURGEON	0x0010

/* Sturgeon specific registers */
#define LPB_READ_CTRL_REG               0x60
#define LPB_READ_CTRL_MASK              0xffffff

#define LPB_READ_CTRL_PREF_EN           (1 << 0)
#define LPB_READ_CTRL_RD_PREF_EN        (1 << 1)
#define LPB_READ_CTRL_MULT_PREF_SHIFT   2
#define LPB_READ_CTRL_MULT_PREF_MASK    (7 << LPB_READ_CTRL_MULT_PREF_SHIFT)
#define LPB_READ_CTRL_LINE_PREF_SHIFT   5
#define LPB_READ_CTRL_LINE_PREF_MASK    (7 << LPB_READ_CTRL_LINE_PREF_SHIFT)
#define LPB_READ_CTRL_DEL_REQ_SHIFT     8
#define LPB_READ_CTRL_DEL_REQ_MASK      (3 << LPB_READ_CTRL_DEL_REQ_SHIFT)

#define LPB_INT_CTRL_BASE               0xa0

#define LPB_INT_CTRL_ENABLE             (1 << 15)
#define LPB_INT_CTRL_DESTMODE           (1 << 14)
#define LPB_INT_CTRL_DEST_SHIFT         6
#define LPB_INT_CTRL_DEST_MASK          (0xff << LPB_INT_CTRL_DEST_SHIFT)
#define LPB_INT_CTRL_MSGTYPE_SHIFT      4
#define LPB_INT_CTRL_MSGTYPE_MASK       (0x3 << LPB_INT_CTRL_MSGTYPE_SHIFT)
#define LPB_INT_CTRL_POLARITY           (1 << 3)
#define LPB_INT_CTRL_TRIGGERMODE        (1 << 2)
#define LPB_INT_CTRL_VECTOR_SHIFT       0
#define LPB_INT_CTRL_VECTOR_MASK        (0x3 << 0)

#define LPB_INT_BLOCK1_REG              0xc4

void
sturgeon_setup (pcitag_t tag, pci_flags_t flags)
{
    int port, bus, device, function;
    int secondary;
    struct pci_bus *pb;
    unsigned offset;
    pcireg_t t, ctrl, intmap;

    pci_break_tag(tag, &port, &bus, &device, &function);

    secondary = (pci_conf_read(tag, PPB_BUSINFO_REG) >> 8) & 0xff;
    pb = pci_businfo(port, secondary);

    /* set up READ CONTROL register for selected prefetch option */
    ctrl = pci_conf_read(tag, LPB_READ_CTRL_REG) & ~LPB_READ_CTRL_MASK;
    if ((flags & PCI_FLG_LDT_PREFETCH) != 0) {
	/* Prefetch enable: all cycle types, 2 delayed requests,
	   4 lines of fetch ahead for MemRdMult */
	ctrl |= (LPB_READ_CTRL_PREF_EN
		 | LPB_READ_CTRL_RD_PREF_EN
		 | (3 << LPB_READ_CTRL_MULT_PREF_SHIFT)
		 | (1 << LPB_READ_CTRL_DEL_REQ_SHIFT));
    } else {
	/* No prefetch */
	ctrl |= 0;
    }
    pci_conf_write(tag, LPB_READ_CTRL_REG, ctrl);

    /* It's apparently not possible for software to distinguish
       the CSWARM's debug Sturgeon (which has floating interrupt
       inputs), so we route interrupts only if there are secondary
       devices. */
    if (pb->ndev > 0) {
	/* Setup interrupt mapping for Block 1:
	   Enabled, Dest=Logical (CPU 0 + CPU 1), Type=Fixed */
	intmap = (LPB_INT_CTRL_ENABLE |
		  LPB_INT_CTRL_DESTMODE |                /* Logical */
		  (0x3 << LPB_INT_CTRL_DEST_SHIFT) |     /* CPU 0+1 */
		  (0x0 << LPB_INT_CTRL_MSGTYPE_SHIFT));  /* Fixed   */
	if (eoi_implemented) {
	    /* Passes >=2 have working EOI. Trigger=Level */
	    intmap |= LPB_INT_CTRL_TRIGGERMODE;          /* Level   */
	}  else {
	    /* Pass 1 lacks working EOI. Trigger=Edge */
	    intmap |= 0;                                 /* Edge    */
	}

	offset = pb->inta_shift % 4;
	t = (intmap + offset);
	offset = (offset+1) % 4;
	t |= (intmap + offset) << 16;
	pci_conf_write(tag, LPB_INT_CTRL_BASE + 8, t);

	offset = (offset+1) % 4;
	t = (intmap + offset);
	offset = (offset+1) % 4;
	t |= (intmap + offset) << 16;
	pci_conf_write(tag, LPB_INT_CTRL_BASE + 12, t);

	t = pci_conf_read(tag, LPB_INT_BLOCK1_REG);
	t &= 0xffffff00;
	t |= (0x40 | (56 >> 2));
	pci_conf_write(tag, LPB_INT_BLOCK1_REG, t);
    }
} 
