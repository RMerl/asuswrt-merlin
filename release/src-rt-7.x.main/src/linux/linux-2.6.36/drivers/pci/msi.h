/*
 * Copyright (C) 2003-2004 Intel
 * Copyright (C) Tom Long Nguyen (tom.l.nguyen@intel.com)
 */

#ifndef MSI_H
#define MSI_H

#define PCI_MSIX_ENTRY_SIZE		16
#define  PCI_MSIX_ENTRY_LOWER_ADDR	0
#define  PCI_MSIX_ENTRY_UPPER_ADDR	4
#define  PCI_MSIX_ENTRY_DATA		8
#define  PCI_MSIX_ENTRY_VECTOR_CTRL	12

#define msi_control_reg(base)		(base + PCI_MSI_FLAGS)
#define msi_lower_address_reg(base)	(base + PCI_MSI_ADDRESS_LO)
#define msi_upper_address_reg(base)	(base + PCI_MSI_ADDRESS_HI)
#define msi_data_reg(base, is64bit)	\
	(base + ((is64bit == 1) ? PCI_MSI_DATA_64 : PCI_MSI_DATA_32))
#define msi_mask_reg(base, is64bit)	\
	(base + ((is64bit == 1) ? PCI_MSI_MASK_64 : PCI_MSI_MASK_32))
#define is_64bit_address(control)	(!!(control & PCI_MSI_FLAGS_64BIT))
#define is_mask_bit_support(control)	(!!(control & PCI_MSI_FLAGS_MASKBIT))

#define msix_table_offset_reg(base)	(base + 0x04)
#define msix_pba_offset_reg(base)	(base + 0x08)
#define msix_table_size(control) 	((control & PCI_MSIX_FLAGS_QSIZE)+1)
#define multi_msix_capable(control)	msix_table_size((control))

#endif /* MSI_H */
