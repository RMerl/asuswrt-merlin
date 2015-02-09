#ifndef _PLATFORMS_ISERIES_PCI_H
#define _PLATFORMS_ISERIES_PCI_H

/*
 * Created by Allan Trautman on Tue Feb 20, 2001.
 *
 * Define some useful macros for the iSeries pci routines.
 * Copyright (C) 2001  Allan H Trautman, IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * Change Activity:
 *   Created Feb 20, 2001
 *   Added device reset, March 22, 2001
 *   Ported to ppc64, May 25, 2001
 * End Change Activity
 */

/*
 * Decodes Linux DevFn to iSeries DevFn, bridge device, or function.
 * For Linux, see PCI_SLOT and PCI_FUNC in include/linux/pci.h
 */

#define ISERIES_PCI_AGENTID(idsel, func)	\
	(((idsel & 0x0F) << 4) | (func & 0x07))
#define ISERIES_ENCODE_DEVICE(agentid)		\
	((0x10) | ((agentid & 0x20) >> 2) | (agentid & 0x07))

#define ISERIES_GET_DEVICE_FROM_SUBBUS(subbus)		((subbus >> 5) & 0x7)
#define ISERIES_GET_FUNCTION_FROM_SUBBUS(subbus)	((subbus >> 2) & 0x7)

struct pci_dev;

#ifdef CONFIG_PCI
extern void	iSeries_pcibios_init(void);
extern void	iSeries_pci_final_fixup(void);
extern void 	iSeries_pcibios_fixup_resources(struct pci_dev *dev);
#else
static inline void	iSeries_pcibios_init(void) { }
static inline void	iSeries_pci_final_fixup(void) { }
static inline void 	iSeries_pcibios_fixup_resources(struct pci_dev *dev) {}
#endif

#endif /* _PLATFORMS_ISERIES_PCI_H */
