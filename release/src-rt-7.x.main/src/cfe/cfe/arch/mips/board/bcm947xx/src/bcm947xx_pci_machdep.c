/* PCI config support for emulated PCI devices (SI cores) only */
/*
 * Machine-specific functions for PCI autoconfiguration.
 *
 * Copyright (c) 2000-2001, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

#include "cfe_pci.h"
#include "pcivar.h"
#include "pcireg.h"
#include "pci_internal.h"

#include <typedefs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndpci.h>

/* default PCI mem regions in PCI space */
#define PCI_MEM_SPACE_BASE	SI_PCI_DMA
#define PCI_MEM_SPACE_SIZE	SI_PCI_DMA_SZ

const cons_t pci_optnames[] = {
    {"verbose",PCI_FLG_VERBOSE},
    {NULL,0}
};

static struct pci_bus _pci_bus[1] = {
    {
	0,		/* minimum grant */
	255,		/* maximum latency */
	0,		/* devsel time = fast */
	0,		/* we don't support fast back-to-back */
	0,		/* we don't support prefetch */
	0,		/* we don't support 66 MHz */
	0,		/* we don't support 64 bits */
	4000000,	/* bandwidth: in 0.25us cycles / sec */
	1		/* initially one device on bus (i.e. us) */
    }
};
static int _pci_nbus = 1;

static si_t *sih = NULL;

extern int _pciverbose;

/* The following functions provide for device-specific setup required
   during configuration.  There is nothing host-specific about them,
   and it would be better to do the packaging and registration in a
   more modular way. */

int
pci_nextbus(int port)
{
	return -1;
}
  
int
pci_maxbus(int port)
{
	return _pci_nbus - 1;
}

struct pci_bus *
pci_businfo(int port, int bus)
{
	return bus < _pci_nbus ? &_pci_bus[bus] : NULL;
}

pcireg_t
pci_minmemaddr(int port)
{
	return PCI_MEM_SPACE_BASE;
}

pcireg_t
pci_maxmemaddr(int port)
{
	return pci_minmemaddr(port) + PCI_MEM_SPACE_SIZE;
}

pcireg_t
pci_minioaddr(int port)
{
	return 0;
}

pcireg_t
pci_maxioaddr(int port)
{
	return 0;
}

/*
 * Called to initialise the bridge at the beginning of time
 */
int
pci_hwinit(int port, pci_flags_t flags)
{
	/* attach to backplane */
	if (!(sih = si_kattach(SI_OSH)))
		return -1;

	/* emulate PCI devices */
	hndpci_init_cores(sih);
	return 0;
}


/*
 * Called to reinitialise the bridge after we've scanned each PCI device
 * and know what is possible.
 */
void
pci_hwreinit(int port, pci_flags_t flags)
{
}

void
pci_flush(void)
{
}

pcitag_t
pci_make_tag(int port, int bus, int device, int function)
{
	return (bus << 16) | (device << 11) | (function << 8);
}

void
pci_break_tag(pcitag_t tag, int *portp, int *busp, int *devicep, int *functionp)
{
	if (portp) *portp = 0;
	if (busp) *busp = (tag >> 16) & 0xff;
	if (devicep) *devicep = (tag >> 11) & 0x1f;
	if (functionp) *functionp = (tag >> 8) & 0x07;
}

int
pci_canscan(pcitag_t tag)
{
	int bus;
	
	pci_break_tag(tag, NULL, &bus, NULL, NULL);

	return bus == 0;
}

pcireg_t
pci_conf_read(pcitag_t tag, int reg)
{
	int bus, device, function;
	pcireg_t data;

	pci_break_tag(tag, NULL, &bus, &device, &function); 

	if (hndpci_read_config(sih, (uint32_t)bus, (uint32_t)device, 
	                       (uint32_t)function, (uint32_t)reg,
	                       (void *)&data, sizeof(pcireg_t)))
		data = 0xffffffff;

	return data;
}


void
pci_conf_write(pcitag_t tag, int reg, pcireg_t data)
{
	int bus, device, function;

	pci_break_tag(tag, NULL, &bus, &device, &function);

	hndpci_write_config(sih, (uint32_t)bus, (uint32_t)device, 
	                    (uint32_t)function, (uint32_t)reg,
	                    (void *)&data, sizeof(pcireg_t));
}

int
pci_conf_write_acked(pcitag_t tag, int reg, pcireg_t data)
{
	pci_conf_write(tag, reg, data);
	return 1;
}

pcireg_t
pci_conf_read16(pcitag_t tag, int reg)
{
	int bus, device, function;
	uint16_t data;

	pci_break_tag(tag, NULL, &bus, &device, &function); 

	if (hndpci_read_config(sih, (uint32_t)bus, (uint32_t)device, 
	                       (uint32_t)function, (uint32_t)reg,
	                       (void *)&data, sizeof(data)))
		data = 0xffff;

	return data;
}

void
pci_conf_write16(pcitag_t tag, int reg, pcireg_t data)
{
	int bus, device, function;
	uint16_t tmp = (uint16_t)data;

	pci_break_tag(tag, NULL, &bus, &device, &function);

	hndpci_write_config(sih, (uint32_t)bus, (uint32_t)device, 
	                    (uint32_t)function, (uint32_t)reg,
	                    (void *)&tmp, sizeof(tmp));
}

int
pci_map_io(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
	return -1;
}

int
pci_map_mem(pcitag_t tag, int reg, pci_endian_t endian, phys_addr_t *pap)
{
	pcireg_t address;
	phys_addr_t pa;

	if (reg == PCI_MAPREG_ROM) {
		/* expansion ROM */
		address = pci_conf_read(tag, reg);
		if (!(address & PCI_MAPREG_ROM)) {
			pci_tagprintf(tag, "pci_map_mem: attempt to map missing rom\n");
			return -1;
		}
		pa = address & PCI_MAPREG_ROM_ADDR_MASK;
	}
	else {
		if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
			if (_pciverbose >= 1)
				pci_tagprintf(tag, "pci_map_mem: bad request\n");
			return -1;
		}
	
		address = pci_conf_read(tag, reg);
		if ((address & PCI_MAPREG_TYPE_IO) != 0) {
			if (_pciverbose >= 1)
				pci_tagprintf(tag, "pci_map_mem: attempt to memory map an I/O region\n");
			return -1;
		}
	
		switch (address & PCI_MAPREG_MEM_TYPE_MASK) {
		case PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_MEM_TYPE_32BIT_1M:
			break;
		case PCI_MAPREG_MEM_TYPE_64BIT:
			if (_pciverbose >= 1)
				pci_tagprintf (tag, "pci_map_mem: attempt to map 64-bit region tag=0x%X @ addr=%X\n", tag, address);
			break;
		default:
			if (_pciverbose >= 1)
				pci_tagprintf (tag, "pci_map_mem: reserved mapping type\n");
			return -1;
		}
		pa = address & PCI_MAPREG_MEM_ADDR_MASK;
	}

	if (_pciverbose >= 1)
		pci_tagprintf (tag, "pci_map_mem: addr=0x%X pa=0x%X\n", address, pa);

	*pap = pa;
	return 0;
}

int
pci_probe_tag(pcitag_t tag)
{
	pcireg_t data;

	if (!pci_canscan(tag))
		return 0;

	data = pci_conf_read(tag,PCI_ID_REG);
	if ((data == 0) || (data == 0xffffffff))
		return 0;

	return 1;
}

int
pci_map_window(phys_addr_t va,
	       unsigned int offset, unsigned int len,
	       int l2ca, int endian)
{
	return -1;
}

int
pci_unmap_window(unsigned int offset, unsigned int len)
{
	return -1;
}

int
pci_device_preset(pcitag_t tag)
{
	return -1;
}

void
pci_device_setup(pcitag_t tag)
{
}

/* Called for each bridge after configuring the secondary bus, to allow
   device-specific initialization. */
void
pci_bridge_setup(pcitag_t tag, pci_flags_t flags)
{
}

/* The base shift of a slot or device on the motherboard. */
uint8_t
pci_int_shift_0(pcitag_t tag)
{
	return 0;
}

uint8_t
pci_int_map_0(pcitag_t tag)
{
	return 0;
}

uint8_t
pci_int_line(uint8_t pci_int)
{
	return 0;
}
