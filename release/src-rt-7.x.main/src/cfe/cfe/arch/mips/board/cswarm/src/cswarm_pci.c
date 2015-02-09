/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: cswarm_pci.c
    *  
    *  This is the part of the board support package for boards
    *  that support PCI. It describes the board-specific slots/devices
    *  and wiring thereof.
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

/* PCI interrupt mapping on the CSWARM board:
   Only device ids 5 and 6 are implemented as PCI connectors, and
   the only on-board device has id 7 (USB bridge).

   Slot    IDSEL   DevID  INT{A,B,C,D}   shift
  (PHB)      -       0       {A,-,-,-}     0
  (LHB)      -       1       {-,-,-,-}     0
    0       16       5       {A,B,C,D}     0 (identity)
    1       17       6       {B,C,D,A}     1 (A->B, B->C, C->D, D->A)
  (USB)     18       7       {C,D,-,-}     2 (A->C, B->D, C->A, D->B)
    -                                      3 (A->D, B->A, C->B, D->C) 

   Device 1 is the LDT host bridge.  By giving it a shift of 0,
   the normal rotation algorithm gives the correct result for devices
   on the secondary bus of the LDT host bridge (bus 1).  There are
   two API 10ll LDT-PCI bridges, the first of which is wired in
   diagnostic mode and must not be configured.  Firmware must
   program the second API 10ll LDT-PCI bridge so that the normal
   rotation algorithm gives correct results for its secondary (bus 3):

    0       16       0       {B,C,D,A}     1 (A->B, B->C, C->D, D->A)
*/

extern int _pciverbose;

/* Return the base shift of a slot or device on the motherboard.
   This is board specific, for the CSWARM only. */
uint8_t
pci_int_shift_0(pcitag_t tag)
{
    int bus, device;

    pci_break_tag(tag, NULL, &bus, &device, NULL);

    if (bus != 0)
	return 0;
    switch (device) {
    case 0:
	return 0;
    case 5: case 6: case 7:
        return ((device - 5) % 4);
    default:
	return 0;
    }
}

/* Return the mapping of a CSWARM device/function interrupt to an
   interrupt line.  For the SB-1250, return 1-4 to indicate the
   pci_inta - pci_intd inputs to the interrupt mapper, respectively,
   or 0 if there is no mapping.  This is board specific, and the
   version below is for CSWARM, 32-bit slots only. */
uint8_t
pci_int_map_0(pcitag_t tag)
{
    pcireg_t data;
    int pin, bus, device;

    data = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG);
    pin = PCI_INTERRUPT_PIN(data);
    if (pin == 0) {
	/* No IRQ used. */
	return 0;
    }
    if (pin > 4) {
	if (_pciverbose >= 1)
	    pci_tagprintf(tag, "pci_map_int: bad interrupt pin %d\n", pin);
	return 0;
    }

    pci_break_tag(tag, NULL, &bus, &device, NULL);

    if (bus != 0)
	return 0;

    switch (device) {
    case 0:
    case 5: case 6: case 7:
        return (((pin - 1) + pci_int_shift_0(tag)) % 4) + 1;
    default:
        return 0;
    }
}
