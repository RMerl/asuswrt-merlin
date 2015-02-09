/*
	Copyright (C) 2004 - 2009 Ivo van Doorn <IvDoorn@gmail.com>
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2x00pci
	Abstract: Data structures for the rt2x00pci module.
 */

#ifndef RT2X00PCI_H
#define RT2X00PCI_H

#include <linux/io.h>
#include <linux/pci.h>

/*
 * This variable should be used with the
 * pci_driver structure initialization.
 */
#define PCI_DEVICE_DATA(__ops)	.driver_data = (kernel_ulong_t)(__ops)

/*
 * Register access.
 */
static inline void rt2x00pci_register_read(struct rt2x00_dev *rt2x00dev,
					   const unsigned int offset,
					   u32 *value)
{
	*value = readl(rt2x00dev->csr.base + offset);
}

static inline void rt2x00pci_register_multiread(struct rt2x00_dev *rt2x00dev,
						const unsigned int offset,
						void *value, const u32 length)
{
	memcpy_fromio(value, rt2x00dev->csr.base + offset, length);
}

static inline void rt2x00pci_register_write(struct rt2x00_dev *rt2x00dev,
					    const unsigned int offset,
					    u32 value)
{
	writel(value, rt2x00dev->csr.base + offset);
}

static inline void rt2x00pci_register_multiwrite(struct rt2x00_dev *rt2x00dev,
						 const unsigned int offset,
						 const void *value,
						 const u32 length)
{
	memcpy_toio(rt2x00dev->csr.base + offset, value, length);
}

/**
 * rt2x00pci_regbusy_read - Read from register with busy check
 * @rt2x00dev: Device pointer, see &struct rt2x00_dev.
 * @offset: Register offset
 * @field: Field to check if register is busy
 * @reg: Pointer to where register contents should be stored
 *
 * This function will read the given register, and checks if the
 * register is busy. If it is, it will sleep for a couple of
 * microseconds before reading the register again. If the register
 * is not read after a certain timeout, this function will return
 * FALSE.
 */
int rt2x00pci_regbusy_read(struct rt2x00_dev *rt2x00dev,
			   const unsigned int offset,
			   const struct rt2x00_field32 field,
			   u32 *reg);

/**
 * struct queue_entry_priv_pci: Per entry PCI specific information
 *
 * @desc: Pointer to device descriptor
 * @desc_dma: DMA pointer to &desc.
 * @data: Pointer to device's entry memory.
 * @data_dma: DMA pointer to &data.
 */
struct queue_entry_priv_pci {
	__le32 *desc;
	dma_addr_t desc_dma;
};

/**
 * rt2x00pci_rxdone - Handle RX done events
 * @rt2x00dev: Device pointer, see &struct rt2x00_dev.
 */
void rt2x00pci_rxdone(struct rt2x00_dev *rt2x00dev);

/*
 * Device initialization handlers.
 */
int rt2x00pci_initialize(struct rt2x00_dev *rt2x00dev);
void rt2x00pci_uninitialize(struct rt2x00_dev *rt2x00dev);

/*
 * PCI driver handlers.
 */
int rt2x00pci_probe(struct pci_dev *pci_dev, const struct pci_device_id *id);
void rt2x00pci_remove(struct pci_dev *pci_dev);
#ifdef CONFIG_PM
int rt2x00pci_suspend(struct pci_dev *pci_dev, pm_message_t state);
int rt2x00pci_resume(struct pci_dev *pci_dev);
#else
#define rt2x00pci_suspend	NULL
#define rt2x00pci_resume	NULL
#endif /* CONFIG_PM */

#endif /* RT2X00PCI_H */
