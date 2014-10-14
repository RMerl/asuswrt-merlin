/*
 * Arch specific extensions to struct device
 *
 * This file is released under the GPLv2
 */
#ifndef _ASM_SPARC64_DEVICE_H
#define _ASM_SPARC64_DEVICE_H

struct device_node;
struct of_device;

struct dev_archdata {
	void			*iommu;
	void			*stc;
	void			*host_controller;

	struct device_node	*prom_node;
	struct of_device	*op;

	unsigned int		msi_num;
};

#endif /* _ASM_SPARC64_DEVICE_H */
