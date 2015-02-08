/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  HT7520 (Golem) Bridge Support			File: dev_ht7520.c
    *  
    *********************************************************************  
    *
    *  Copyright 2002,2003
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
#include "lib_physio.h"

#include "pcireg.h"
#include "pcivar.h"

extern int eoi_implemented;

void ht7520apic_preset (pcitag_t tag);
void ht7520apic_setup (pcitag_t tag);


/* PLX HT7520 (LDT to PCI-X bridge + APIC) specific definitions */

#define	PCI_VENDOR_AMD			0x1022
#define PCI_PRODUCT_PLX_HT7520		0x7450
#define PCI_PRODUCT_PLX_HT7520_APIC	0x7451

/* HT7520 specific registers */

/* APIC configuration registers */

#define APIC_CONTROL_REG                0x0044

#define APIC_CONTROL_OSVISBAR           (1 << 0)
#define APIC_CONTROL_IOAEN              (1 << 1)

#define APIC_BASE_ADDR_REG              0x0048

/* APIC registers in BAR0 memory space */

#define HT7520_APIC_INDEX_REG           0x0000
#define HT7520_APIC_DATA_REG            0x0010

#define APIC_ID_INDEX                   0x00
#define APIC_VERSION_INDEX              0x01
#define APIC_ARBID_INDEX                0x02
#define APIC_RDR_BASE_INDEX             0x10
#define APIC_RDR_LO_INDEX(n)            (APIC_RDR_BASE_INDEX + 2*(n))
#define APIC_RDR_HI_INDEX(n)            (APIC_RDR_BASE_INDEX + 2*(n) + 1)

#define RDR_HI_DEST_SHIFT               (56-32)
#define RDR_HI_DEST_MASK                (0xff << RDR_HI_DEST_SHIFT)
#define RDR_LO_IM                       (1 << 16)
#define RDR_LO_TM                       (1 << 15)
#define RDR_LO_IRR                      (1 << 14)
#define RDR_LO_POL                      (1 << 13)
#define RDR_LO_DS                       (1 << 12)
#define RDR_LO_DM                       (1 << 11)
#define RDR_LO_MT_SHIFT                 8
#define RDR_LO_MT_MASK                  (3 << RDR_LO_MT_SHIFT)
#define RDR_LO_IV_SHIFT                 0
#define RDR_LO_IV_MASK                  (0xff << RDR_LO_IV_SHIFT)

void
ht7520apic_preset (pcitag_t tag)
{
    pcireg_t ctrl;

    /* For some reason, BAR0 (necessary for setting the interrupt
       mapping) is hidden by default; the following makes it
       visible. */
    ctrl = pci_conf_read(tag, APIC_CONTROL_REG);
    ctrl |= APIC_CONTROL_IOAEN | APIC_CONTROL_OSVISBAR;
    pci_conf_write(tag, APIC_CONTROL_REG, ctrl);
    ctrl = pci_conf_read(tag, APIC_CONTROL_REG);   /* push */
}

void
ht7520apic_setup (pcitag_t tag)
{
    int port, bus, device, function;
    pcitag_t br_tag;
    int secondary;
    struct pci_bus *pb;
    unsigned offset;
    phys_addr_t apic_addr;
    uint32_t rdrh, rdrl;
    int i;

    /* The HT7520 splits the bridge and APIC functionality between two
       functions.  The following code depends upon a known
       relationship between the bridge and APIC tags, with a temporary
       fudge for the simulator.  NB: We assume that the bridge
       function has already been initialized. */

    pci_break_tag(tag, &port, &bus, &device, &function);

#ifdef _FUNCSIM_
    br_tag = pci_make_tag(port, bus, device-2, 0);
#else
    br_tag = pci_make_tag(port, bus, device, function-1);
#endif
    secondary = (pci_conf_read(br_tag, PPB_BUSINFO_REG) >> 8) & 0xff;
    pb = pci_businfo(port, secondary);

    /* Set up interrupt mappings. */
    pci_map_mem(tag, PCI_MAPREG(0), PCI_MATCH_BITS, &apic_addr);

    offset = pb->inta_shift % 4;
    for (i = 0; i < 4; i++) {
	phys_write32(apic_addr + HT7520_APIC_INDEX_REG, APIC_RDR_HI_INDEX(i));
	rdrh = 0x03 << RDR_HI_DEST_SHIFT;        /* CPU 0 + CPU 1 */
	phys_write32(apic_addr + HT7520_APIC_DATA_REG, rdrh);
	rdrh = phys_read32(apic_addr + HT7520_APIC_DATA_REG);  /* push */

	phys_write32(apic_addr + HT7520_APIC_INDEX_REG, APIC_RDR_LO_INDEX(i));
	if (eoi_implemented) {
	    /* Passes >=2 have working EOI. Trigger=Level */
	    rdrl = (RDR_LO_TM |                      /* Level */
		    RDR_LO_POL |                     /* Active Low */
		    RDR_LO_DM |                      /* Logical */
		    0x0 << RDR_LO_MT_SHIFT |         /* Fixed */
		    (56+offset) << RDR_LO_IV_SHIFT); /* Vector */
	} else {
	    /* Pass 1 lacks working EOI. Trigger=Edge.  Note that
	       LO_POL appears mis-documented for edges.  */
	    rdrl = (RDR_LO_DM |                      /* Logical */
		    0x0 << RDR_LO_MT_SHIFT |         /* Fixed */
		    (56+offset) << RDR_LO_IV_SHIFT); /* Vector */
	}
	phys_write32(apic_addr + HT7520_APIC_DATA_REG, rdrl);
	offset = (offset + 1) % 4;
    }
}
