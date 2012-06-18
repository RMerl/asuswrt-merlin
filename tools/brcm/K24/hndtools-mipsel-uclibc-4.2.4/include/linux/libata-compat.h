#ifndef __LIBATA_COMPAT_H__
#define __LIBATA_COMPAT_H__

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/slab.h>

typedef u32 __le32;
typedef u64 __le64;

#define DMA_64BIT_MASK 0xffffffffffffffffULL
#define DMA_32BIT_MASK 0x00000000ffffffffULL

/* These definitions mirror those in pci.h, so they can be used
 * interchangeably with their PCI_ counterparts */
enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

#define offset_in_page(p)	((unsigned long)(p) & ~PAGE_MASK)

#define MODULE_VERSION(ver_str)

/* remaps usage of KM_IRQ0 onto KM_SOFTIRQ0. KM_IRQ0 only exists on ia64 in
 * 2.4. Warning: this will also remap KM_IRQ0 on ia64, so be careful about
 * the files included after this file. */

#define KM_IRQ0	KM_SOFTIRQ0

struct device {
	struct pci_dev pdev;
};

static inline struct pci_dev *to_pci_dev(struct device *dev)
{
	return (struct pci_dev *) dev;
}

#define pdev_printk(lvl, pdev, fmt, args...)			\
	do {							\
		printk("%s%s(%s): ", lvl,			\
			(pdev)->driver && (pdev)->driver->name ? \
				(pdev)->driver->name : "PCI",	\
			pci_name(pdev));			\
		printk(fmt, ## args);				\
	} while (0)

static inline int pci_enable_msi(struct pci_dev *dev) { return -1; }
static inline void pci_disable_msi(struct pci_dev *dev) {}

static inline int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	if (mask == (u64)dev->dma_mask)
		return 0;
	return -EIO;
}

/* NOTE: dangerous! we ignore the 'gfp' argument */
#define dma_alloc_coherent(dev,sz,dma,gfp) \
	pci_alloc_consistent(to_pci_dev(dev),(sz),(dma))
#define dma_free_coherent(dev,sz,addr,dma_addr) \
	pci_free_consistent(to_pci_dev(dev),(sz),(addr),(dma_addr))

#define dma_map_sg(dev,a,b,c) \
	pci_map_sg(to_pci_dev(dev),(a),(b),(c))
#define dma_unmap_sg(dev,a,b,c) \
	pci_unmap_sg(to_pci_dev(dev),(a),(b),(c))

#define dma_map_single(dev,a,b,c) \
	pci_map_single(to_pci_dev(dev),(a),(b),(c))
#define dma_unmap_single(dev,a,b,c) \
	pci_unmap_single(to_pci_dev(dev),(a),(b),(c))

#define dma_mapping_error(addr) (0)

#define dev_get_drvdata(dev) \
	pci_get_drvdata(to_pci_dev(dev))
#define dev_set_drvdata(dev,ptr) \
	pci_set_drvdata(to_pci_dev(dev),(ptr))

static inline void *kcalloc(size_t nmemb, size_t size, int flags)
{
	size_t total = nmemb * size;
	void *mem = kmalloc(total, flags);
	if (mem)
		memset(mem, 0, total);
	return mem;
}

static inline void *kzalloc(size_t size, int flags)
{
	return kcalloc(1, size, flags);
}

static inline void pci_iounmap(struct pci_dev *pdev, void *mem)
{
	iounmap(mem);
}

/**
 * pci_intx - enables/disables PCI INTx for device dev
 * @pdev: the PCI device to operate on
 * @enable: boolean: whether to enable or disable PCI INTx
 *
 * Enables/disables PCI INTx for device dev
 */
static inline void
pci_intx(struct pci_dev *pdev, int enable)
{
	u16 pci_command, new;

	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);

	if (enable) {
		new = pci_command & ~PCI_COMMAND_INTX_DISABLE;
	} else {
		new = pci_command | PCI_COMMAND_INTX_DISABLE;
	}

	if (new != pci_command) {
		pci_write_config_word(pdev, PCI_COMMAND, new);
	}
}

static inline void __iomem *
pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	unsigned long start = pci_resource_start(dev, bar);
	unsigned long len = pci_resource_len(dev, bar);
	unsigned long flags = pci_resource_flags(dev, bar);

	if (!len || !start)
		return NULL;
	if (maxlen && len > maxlen)
		len = maxlen;
	if (flags & IORESOURCE_IO) {
		BUG();
	}
	if (flags & IORESOURCE_MEM) {
		if (flags & IORESOURCE_CACHEABLE)
			return ioremap(start, len);
		return ioremap_nocache(start, len);
	}
	/* What? */
	return NULL;
}

static inline void sg_set_buf(struct scatterlist *sg, void *buf,
			      unsigned int buflen)
{
	sg->page = virt_to_page(buf);
	sg->offset = offset_in_page(buf);
	sg->length = buflen;
}

static inline void sg_init_one(struct scatterlist *sg, void *buf,
			       unsigned int buflen)
{
	memset(sg, 0, sizeof(*sg));
	sg_set_buf(sg, buf, buflen);
}

#endif /* __LIBATA_COMPAT_H__ */
