#ifndef __OF_ADDRESS_H
#define __OF_ADDRESS_H
#include <linux/ioport.h>
#include <linux/of.h>

extern u64 of_translate_address(struct device_node *np, const u32 *addr);
extern int of_address_to_resource(struct device_node *dev, int index,
				  struct resource *r);
extern void __iomem *of_iomap(struct device_node *device, int index);

/* Extract an address from a device, returns the region size and
 * the address space flags too. The PCI version uses a BAR number
 * instead of an absolute index
 */
extern const u32 *of_get_address(struct device_node *dev, int index,
			   u64 *size, unsigned int *flags);

#ifndef pci_address_to_pio
static inline unsigned long pci_address_to_pio(phys_addr_t addr) { return -1; }
#define pci_address_to_pio pci_address_to_pio
#endif

#ifdef CONFIG_PCI
extern const u32 *of_get_pci_address(struct device_node *dev, int bar_no,
			       u64 *size, unsigned int *flags);
extern int of_pci_address_to_resource(struct device_node *dev, int bar,
				      struct resource *r);
#else /* CONFIG_PCI */
static inline int of_pci_address_to_resource(struct device_node *dev, int bar,
				             struct resource *r)
{
	return -ENOSYS;
}

static inline const u32 *of_get_pci_address(struct device_node *dev,
		int bar_no, u64 *size, unsigned int *flags)
{
	return NULL;
}
#endif /* CONFIG_PCI */


#endif /* __OF_ADDRESS_H */
