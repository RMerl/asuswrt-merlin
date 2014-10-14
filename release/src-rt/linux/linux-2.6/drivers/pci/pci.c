/*
 *	PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 *	Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 *	David Mosberger-Tang
 *
 *	Copyright 1997 -- 2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/log2.h>
#include <linux/pci-aspm.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <asm/setup.h>
#include "pci.h"

const char *pci_power_names[] = {
	"error", "D0", "D1", "D2", "D3hot", "D3cold", "unknown",
};
EXPORT_SYMBOL_GPL(pci_power_names);

int isa_dma_bridge_buggy;
EXPORT_SYMBOL(isa_dma_bridge_buggy);

int pci_pci_problems;
EXPORT_SYMBOL(pci_pci_problems);

unsigned int pci_pm_d3_delay;

static void pci_pme_list_scan(struct work_struct *work);

static LIST_HEAD(pci_pme_list);
static DEFINE_MUTEX(pci_pme_list_mutex);
static DECLARE_DELAYED_WORK(pci_pme_work, pci_pme_list_scan);

struct pci_pme_device {
	struct list_head list;
	struct pci_dev *dev;
};

#define PME_TIMEOUT 1000 /* How long between PME checks */

static void pci_dev_d3_sleep(struct pci_dev *dev)
{
	unsigned int delay = dev->d3_delay;

	if (delay < pci_pm_d3_delay)
		delay = pci_pm_d3_delay;

	msleep(delay);
}

#ifdef CONFIG_PCI_DOMAINS
int pci_domains_supported = 1;
#endif

#define DEFAULT_CARDBUS_IO_SIZE		(256)
#define DEFAULT_CARDBUS_MEM_SIZE	(64*1024*1024)
/* pci=cbmemsize=nnM,cbiosize=nn can override this */
unsigned long pci_cardbus_io_size = DEFAULT_CARDBUS_IO_SIZE;
unsigned long pci_cardbus_mem_size = DEFAULT_CARDBUS_MEM_SIZE;

#define DEFAULT_HOTPLUG_IO_SIZE		(256)
#define DEFAULT_HOTPLUG_MEM_SIZE	(2*1024*1024)
/* pci=hpmemsize=nnM,hpiosize=nn can override this */
unsigned long pci_hotplug_io_size  = DEFAULT_HOTPLUG_IO_SIZE;
unsigned long pci_hotplug_mem_size = DEFAULT_HOTPLUG_MEM_SIZE;

/*
 * The default CLS is used if arch didn't set CLS explicitly and not
 * all pci devices agree on the same value.  Arch can override either
 * the dfl or actual value as it sees fit.  Don't forget this is
 * measured in 32-bit words, not bytes.
 */
u8 pci_dfl_cache_line_size __devinitdata = L1_CACHE_BYTES >> 2;
u8 pci_cache_line_size;

/**
 * pci_bus_max_busnr - returns maximum PCI bus number of given bus' children
 * @bus: pointer to PCI bus structure to search
 *
 * Given a PCI bus, returns the highest PCI bus number present in the set
 * including the given PCI bus and its list of child PCI buses.
 */
unsigned char pci_bus_max_busnr(struct pci_bus* bus)
{
	struct list_head *tmp;
	unsigned char max, n;

	max = bus->subordinate;
	list_for_each(tmp, &bus->children) {
		n = pci_bus_max_busnr(pci_bus_b(tmp));
		if(n > max)
			max = n;
	}
	return max;
}
EXPORT_SYMBOL_GPL(pci_bus_max_busnr);

#ifdef CONFIG_HAS_IOMEM
void __iomem *pci_ioremap_bar(struct pci_dev *pdev, int bar)
{
	/*
	 * Make sure the BAR is actually a memory resource, not an IO resource
	 */
	if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM)) {
		WARN_ON(1);
		return NULL;
	}
	return ioremap_nocache(pci_resource_start(pdev, bar),
				     pci_resource_len(pdev, bar));
}
EXPORT_SYMBOL_GPL(pci_ioremap_bar);
#endif

#if 0
/**
 * pci_max_busnr - returns maximum PCI bus number
 *
 * Returns the highest PCI bus number present in the system global list of
 * PCI buses.
 */
unsigned char __devinit
pci_max_busnr(void)
{
	struct pci_bus *bus = NULL;
	unsigned char max, n;

	max = 0;
	while ((bus = pci_find_next_bus(bus)) != NULL) {
		n = pci_bus_max_busnr(bus);
		if(n > max)
			max = n;
	}
	return max;
}

#endif  /*  0  */

#define PCI_FIND_CAP_TTL	48

static int __pci_find_next_cap_ttl(struct pci_bus *bus, unsigned int devfn,
				   u8 pos, int cap, int *ttl)
{
	u8 id;

	while ((*ttl)--) {
		pci_bus_read_config_byte(bus, devfn, pos, &pos);
		if (pos < 0x40)
			break;
		pos &= ~3;
		pci_bus_read_config_byte(bus, devfn, pos + PCI_CAP_LIST_ID,
					 &id);
		if (id == 0xff)
			break;
		if (id == cap)
			return pos;
		pos += PCI_CAP_LIST_NEXT;
	}
	return 0;
}

static int __pci_find_next_cap(struct pci_bus *bus, unsigned int devfn,
			       u8 pos, int cap)
{
	int ttl = PCI_FIND_CAP_TTL;

	return __pci_find_next_cap_ttl(bus, devfn, pos, cap, &ttl);
}

int pci_find_next_capability(struct pci_dev *dev, u8 pos, int cap)
{
	return __pci_find_next_cap(dev->bus, dev->devfn,
				   pos + PCI_CAP_LIST_NEXT, cap);
}
EXPORT_SYMBOL_GPL(pci_find_next_capability);

static int __pci_bus_find_cap_start(struct pci_bus *bus,
				    unsigned int devfn, u8 hdr_type)
{
	u16 status;

	pci_bus_read_config_word(bus, devfn, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;

	switch (hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		return PCI_CAPABILITY_LIST;
	case PCI_HEADER_TYPE_CARDBUS:
		return PCI_CB_CAPABILITY_LIST;
	default:
		return 0;
	}

	return 0;
}

/**
 * pci_find_capability - query for devices' capabilities 
 * @dev: PCI device to query
 * @cap: capability code
 *
 * Tell if a device supports a given PCI capability.
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.  Possible values for @cap:
 *
 *  %PCI_CAP_ID_PM           Power Management 
 *  %PCI_CAP_ID_AGP          Accelerated Graphics Port 
 *  %PCI_CAP_ID_VPD          Vital Product Data 
 *  %PCI_CAP_ID_SLOTID       Slot Identification 
 *  %PCI_CAP_ID_MSI          Message Signalled Interrupts
 *  %PCI_CAP_ID_CHSWP        CompactPCI HotSwap 
 *  %PCI_CAP_ID_PCIX         PCI-X
 *  %PCI_CAP_ID_EXP          PCI Express
 */
int pci_find_capability(struct pci_dev *dev, int cap)
{
	int pos;

	pos = __pci_bus_find_cap_start(dev->bus, dev->devfn, dev->hdr_type);
	if (pos)
		pos = __pci_find_next_cap(dev->bus, dev->devfn, pos, cap);

	return pos;
}

/**
 * pci_bus_find_capability - query for devices' capabilities 
 * @bus:   the PCI bus to query
 * @devfn: PCI device to query
 * @cap:   capability code
 *
 * Like pci_find_capability() but works for pci devices that do not have a
 * pci_dev structure set up yet. 
 *
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.
 */
int pci_bus_find_capability(struct pci_bus *bus, unsigned int devfn, int cap)
{
	int pos;
	u8 hdr_type;

	pci_bus_read_config_byte(bus, devfn, PCI_HEADER_TYPE, &hdr_type);

	pos = __pci_bus_find_cap_start(bus, devfn, hdr_type & 0x7f);
	if (pos)
		pos = __pci_find_next_cap(bus, devfn, pos, cap);

	return pos;
}

/**
 * pci_find_ext_capability - Find an extended capability
 * @dev: PCI device to query
 * @cap: capability code
 *
 * Returns the address of the requested extended capability structure
 * within the device's PCI configuration space or 0 if the device does
 * not support it.  Possible values for @cap:
 *
 *  %PCI_EXT_CAP_ID_ERR		Advanced Error Reporting
 *  %PCI_EXT_CAP_ID_VC		Virtual Channel
 *  %PCI_EXT_CAP_ID_DSN		Device Serial Number
 *  %PCI_EXT_CAP_ID_PWR		Power Budgeting
 */
int pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	u32 header;
	int ttl;
	int pos = PCI_CFG_SPACE_SIZE;

	/* minimum 8 bytes per capability */
	ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

	if (dev->cfg_size <= PCI_CFG_SPACE_SIZE)
		return 0;

	if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
		return 0;

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;

		if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
			break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_find_ext_capability);

/**
 * pci_bus_find_ext_capability - find an extended capability
 * @bus:   the PCI bus to query
 * @devfn: PCI device to query
 * @cap:   capability code
 *
 * Like pci_find_ext_capability() but works for pci devices that do not have a
 * pci_dev structure set up yet.
 *
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.
 */
int pci_bus_find_ext_capability(struct pci_bus *bus, unsigned int devfn,
				int cap)
{
	u32 header;
	int ttl;
	int pos = PCI_CFG_SPACE_SIZE;

	/* minimum 8 bytes per capability */
	ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

	if (!pci_bus_read_config_dword(bus, devfn, pos, &header))
		return 0;
	if (header == 0xffffffff || header == 0)
		return 0;

	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;

		if (!pci_bus_read_config_dword(bus, devfn, pos, &header))
			break;
	}

	return 0;
}

static int __pci_find_next_ht_cap(struct pci_dev *dev, int pos, int ht_cap)
{
	int rc, ttl = PCI_FIND_CAP_TTL;
	u8 cap, mask;

	if (ht_cap == HT_CAPTYPE_SLAVE || ht_cap == HT_CAPTYPE_HOST)
		mask = HT_3BIT_CAP_MASK;
	else
		mask = HT_5BIT_CAP_MASK;

	pos = __pci_find_next_cap_ttl(dev->bus, dev->devfn, pos,
				      PCI_CAP_ID_HT, &ttl);
	while (pos) {
		rc = pci_read_config_byte(dev, pos + 3, &cap);
		if (rc != PCIBIOS_SUCCESSFUL)
			return 0;

		if ((cap & mask) == ht_cap)
			return pos;

		pos = __pci_find_next_cap_ttl(dev->bus, dev->devfn,
					      pos + PCI_CAP_LIST_NEXT,
					      PCI_CAP_ID_HT, &ttl);
	}

	return 0;
}
/**
 * pci_find_next_ht_capability - query a device's Hypertransport capabilities
 * @dev: PCI device to query
 * @pos: Position from which to continue searching
 * @ht_cap: Hypertransport capability code
 *
 * To be used in conjunction with pci_find_ht_capability() to search for
 * all capabilities matching @ht_cap. @pos should always be a value returned
 * from pci_find_ht_capability().
 *
 * NB. To be 100% safe against broken PCI devices, the caller should take
 * steps to avoid an infinite loop.
 */
int pci_find_next_ht_capability(struct pci_dev *dev, int pos, int ht_cap)
{
	return __pci_find_next_ht_cap(dev, pos + PCI_CAP_LIST_NEXT, ht_cap);
}
EXPORT_SYMBOL_GPL(pci_find_next_ht_capability);

/**
 * pci_find_ht_capability - query a device's Hypertransport capabilities
 * @dev: PCI device to query
 * @ht_cap: Hypertransport capability code
 *
 * Tell if a device supports a given Hypertransport capability.
 * Returns an address within the device's PCI configuration space
 * or 0 in case the device does not support the request capability.
 * The address points to the PCI capability, of type PCI_CAP_ID_HT,
 * which has a Hypertransport capability matching @ht_cap.
 */
int pci_find_ht_capability(struct pci_dev *dev, int ht_cap)
{
	int pos;

	pos = __pci_bus_find_cap_start(dev->bus, dev->devfn, dev->hdr_type);
	if (pos)
		pos = __pci_find_next_ht_cap(dev, pos, ht_cap);

	return pos;
}
EXPORT_SYMBOL_GPL(pci_find_ht_capability);

/**
 * pci_find_parent_resource - return resource region of parent bus of given region
 * @dev: PCI device structure contains resources to be searched
 * @res: child resource record for which parent is sought
 *
 *  For given resource region of given device, return the resource
 *  region of parent bus the given region is contained in or where
 *  it should be allocated from.
 */
struct resource *
pci_find_parent_resource(const struct pci_dev *dev, struct resource *res)
{
	const struct pci_bus *bus = dev->bus;
	int i;
	struct resource *best = NULL, *r;

	pci_bus_for_each_resource(bus, r, i) {
		if (!r)
			continue;
		if (res->start && !(res->start >= r->start && res->end <= r->end))
			continue;	/* Not contained */
		if ((res->flags ^ r->flags) & (IORESOURCE_IO | IORESOURCE_MEM))
			continue;	/* Wrong type */
		if (!((res->flags ^ r->flags) & IORESOURCE_PREFETCH))
			return r;	/* Exact match */
		/* We can't insert a non-prefetch resource inside a prefetchable parent .. */
		if (r->flags & IORESOURCE_PREFETCH)
			continue;
		/* .. but we can put a prefetchable resource inside a non-prefetchable one */
		if (!best)
			best = r;
	}
	return best;
}

/**
 * pci_restore_bars - restore a devices BAR values (e.g. after wake-up)
 * @dev: PCI device to have its BARs restored
 *
 * Restore the BAR values for a given device, so as to make it
 * accessible by its driver.
 */
static void
pci_restore_bars(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_BRIDGE_RESOURCES; i++)
		pci_update_resource(dev, i);
}

static struct pci_platform_pm_ops *pci_platform_pm;

int pci_set_platform_pm(struct pci_platform_pm_ops *ops)
{
	if (!ops->is_manageable || !ops->set_state || !ops->choose_state
	    || !ops->sleep_wake || !ops->can_wakeup)
		return -EINVAL;
	pci_platform_pm = ops;
	return 0;
}

static inline bool platform_pci_power_manageable(struct pci_dev *dev)
{
	return pci_platform_pm ? pci_platform_pm->is_manageable(dev) : false;
}

static inline int platform_pci_set_power_state(struct pci_dev *dev,
                                                pci_power_t t)
{
	return pci_platform_pm ? pci_platform_pm->set_state(dev, t) : -ENOSYS;
}

static inline pci_power_t platform_pci_choose_state(struct pci_dev *dev)
{
	return pci_platform_pm ?
			pci_platform_pm->choose_state(dev) : PCI_POWER_ERROR;
}

static inline bool platform_pci_can_wakeup(struct pci_dev *dev)
{
	return pci_platform_pm ? pci_platform_pm->can_wakeup(dev) : false;
}

static inline int platform_pci_sleep_wake(struct pci_dev *dev, bool enable)
{
	return pci_platform_pm ?
			pci_platform_pm->sleep_wake(dev, enable) : -ENODEV;
}

static inline int platform_pci_run_wake(struct pci_dev *dev, bool enable)
{
	return pci_platform_pm ?
			pci_platform_pm->run_wake(dev, enable) : -ENODEV;
}

/**
 * pci_raw_set_power_state - Use PCI PM registers to set the power state of
 *                           given PCI device
 * @dev: PCI device to handle.
 * @state: PCI power state (D0, D1, D2, D3hot) to put the device into.
 *
 * RETURN VALUE:
 * -EINVAL if the requested state is invalid.
 * -EIO if device does not support PCI PM or its PM capabilities register has a
 * wrong version, or device doesn't support the requested state.
 * 0 if device already is in the requested state.
 * 0 if device's power state has been successfully changed.
 */
static int pci_raw_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	u16 pmcsr;
	bool need_restore = false;

	/* Check if we're already there */
	if (dev->current_state == state)
		return 0;

	if (!dev->pm_cap)
		return -EIO;

	if (state < PCI_D0 || state > PCI_D3hot)
		return -EINVAL;

	/* Validate current state:
	 * Can enter D0 from any state, but if we can only go deeper 
	 * to sleep if we're already in a low power state
	 */
	if (state != PCI_D0 && dev->current_state <= PCI_D3cold
	    && dev->current_state > state) {
		dev_err(&dev->dev, "invalid power transition "
			"(from state %d to %d)\n", dev->current_state, state);
		return -EINVAL;
	}

	/* check if this device supports the desired state */
	if ((state == PCI_D1 && !dev->d1_support)
	   || (state == PCI_D2 && !dev->d2_support))
		return -EIO;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);

	/* If we're (effectively) in D3, force entire word to 0.
	 * This doesn't affect PME_Status, disables PME_En, and
	 * sets PowerState to 0.
	 */
	switch (dev->current_state) {
	case PCI_D0:
	case PCI_D1:
	case PCI_D2:
		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		pmcsr |= state;
		break;
	case PCI_D3hot:
	case PCI_D3cold:
	case PCI_UNKNOWN: /* Boot-up */
		if ((pmcsr & PCI_PM_CTRL_STATE_MASK) == PCI_D3hot
		 && !(pmcsr & PCI_PM_CTRL_NO_SOFT_RESET))
			need_restore = true;
		/* Fall-through: force to D0 */
	default:
		pmcsr = 0;
		break;
	}

	/* enter specified state */
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, pmcsr);

	/* Mandatory power management transition delays */
	/* see PCI PM 1.1 5.6.1 table 18 */
	if (state == PCI_D3hot || dev->current_state == PCI_D3hot)
		pci_dev_d3_sleep(dev);
	else if (state == PCI_D2 || dev->current_state == PCI_D2)
		udelay(PCI_PM_D2_DELAY);

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	dev->current_state = (pmcsr & PCI_PM_CTRL_STATE_MASK);
	if (dev->current_state != state && printk_ratelimit())
		dev_info(&dev->dev, "Refused to change power state, "
			"currently in D%d\n", dev->current_state);

	/* According to section 5.4.1 of the "PCI BUS POWER MANAGEMENT
	 * INTERFACE SPECIFICATION, REV. 1.2", a device transitioning
	 * from D3hot to D0 _may_ perform an internal reset, thereby
	 * going to "D0 Uninitialized" rather than "D0 Initialized".
	 * For example, at least some versions of the 3c905B and the
	 * 3c556B exhibit this behaviour.
	 *
	 * At least some laptop BIOSen (e.g. the Thinkpad T21) leave
	 * devices in a D3hot state at boot.  Consequently, we need to
	 * restore at least the BARs so that the device will be
	 * accessible to its driver.
	 */
	if (need_restore)
		pci_restore_bars(dev);

	if (dev->bus->self)
		pcie_aspm_pm_state_change(dev->bus->self);

	return 0;
}

/**
 * pci_update_current_state - Read PCI power state of given device from its
 *                            PCI PM registers and cache it
 * @dev: PCI device to handle.
 * @state: State to cache in case the device doesn't have the PM capability
 */
void pci_update_current_state(struct pci_dev *dev, pci_power_t state)
{
	if (dev->pm_cap) {
		u16 pmcsr;

		pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
		dev->current_state = (pmcsr & PCI_PM_CTRL_STATE_MASK);
	} else {
		dev->current_state = state;
	}
}

/**
 * pci_platform_power_transition - Use platform to change device power state
 * @dev: PCI device to handle.
 * @state: State to put the device into.
 */
static int pci_platform_power_transition(struct pci_dev *dev, pci_power_t state)
{
	int error;

	if (platform_pci_power_manageable(dev)) {
		error = platform_pci_set_power_state(dev, state);
		if (!error)
			pci_update_current_state(dev, state);
	} else {
		error = -ENODEV;
		/* Fall back to PCI_D0 if native PM is not supported */
		if (!dev->pm_cap)
			dev->current_state = PCI_D0;
	}

	return error;
}

/**
 * __pci_start_power_transition - Start power transition of a PCI device
 * @dev: PCI device to handle.
 * @state: State to put the device into.
 */
static void __pci_start_power_transition(struct pci_dev *dev, pci_power_t state)
{
	if (state == PCI_D0)
		pci_platform_power_transition(dev, PCI_D0);
}

/**
 * __pci_complete_power_transition - Complete power transition of a PCI device
 * @dev: PCI device to handle.
 * @state: State to put the device into.
 *
 * This function should not be called directly by device drivers.
 */
int __pci_complete_power_transition(struct pci_dev *dev, pci_power_t state)
{
	return state >= PCI_D0 ?
			pci_platform_power_transition(dev, state) : -EINVAL;
}
EXPORT_SYMBOL_GPL(__pci_complete_power_transition);

/**
 * pci_set_power_state - Set the power state of a PCI device
 * @dev: PCI device to handle.
 * @state: PCI power state (D0, D1, D2, D3hot) to put the device into.
 *
 * Transition a device to a new power state, using the platform firmware and/or
 * the device's PCI PM registers.
 *
 * RETURN VALUE:
 * -EINVAL if the requested state is invalid.
 * -EIO if device does not support PCI PM or its PM capabilities register has a
 * wrong version, or device doesn't support the requested state.
 * 0 if device already is in the requested state.
 * 0 if device's power state has been successfully changed.
 */
int pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	int error;

	/* bound the state we're entering */
	if (state > PCI_D3hot)
		state = PCI_D3hot;
	else if (state < PCI_D0)
		state = PCI_D0;
	else if ((state == PCI_D1 || state == PCI_D2) && pci_no_d1d2(dev))
		/*
		 * If the device or the parent bridge do not support PCI PM,
		 * ignore the request if we're doing anything other than putting
		 * it into D0 (which would only happen on boot).
		 */
		return 0;

	__pci_start_power_transition(dev, state);

	/* This device is quirked not to be put into D3, so
	   don't put it in D3 */
	if (state == PCI_D3hot && (dev->dev_flags & PCI_DEV_FLAGS_NO_D3))
		return 0;

	error = pci_raw_set_power_state(dev, state);

	if (!__pci_complete_power_transition(dev, state))
		error = 0;
	/*
	 * When aspm_policy is "powersave" this call ensures
	 * that ASPM is configured.
	 */
	if (!error && dev->bus->self)
		pcie_aspm_powersave_config_link(dev->bus->self);

	return error;
}

/**
 * pci_choose_state - Choose the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: target sleep state for the whole system. This is the value
 *	that is passed to suspend() function.
 *
 * Returns PCI power state suitable for given device and given system
 * message.
 */

pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
	pci_power_t ret;

	if (!pci_find_capability(dev, PCI_CAP_ID_PM))
		return PCI_D0;

	ret = platform_pci_choose_state(dev);
	if (ret != PCI_POWER_ERROR)
		return ret;

	switch (state.event) {
	case PM_EVENT_ON:
		return PCI_D0;
	case PM_EVENT_FREEZE:
	case PM_EVENT_PRETHAW:
		/* REVISIT both freeze and pre-thaw "should" use D0 */
	case PM_EVENT_SUSPEND:
	case PM_EVENT_HIBERNATE:
		return PCI_D3hot;
	default:
		dev_info(&dev->dev, "unrecognized suspend event %d\n",
			 state.event);
		BUG();
	}
	return PCI_D0;
}

EXPORT_SYMBOL(pci_choose_state);

#define PCI_EXP_SAVE_REGS	7

#define pcie_cap_has_devctl(type, flags)	1
#define pcie_cap_has_lnkctl(type, flags)		\
		((flags & PCI_EXP_FLAGS_VERS) > 1 ||	\
		 (type == PCI_EXP_TYPE_ROOT_PORT ||	\
		  type == PCI_EXP_TYPE_ENDPOINT ||	\
		  type == PCI_EXP_TYPE_LEG_END))
#define pcie_cap_has_sltctl(type, flags)		\
		((flags & PCI_EXP_FLAGS_VERS) > 1 ||	\
		 ((type == PCI_EXP_TYPE_ROOT_PORT) ||	\
		  (type == PCI_EXP_TYPE_DOWNSTREAM &&	\
		   (flags & PCI_EXP_FLAGS_SLOT))))
#define pcie_cap_has_rtctl(type, flags)			\
		((flags & PCI_EXP_FLAGS_VERS) > 1 ||	\
		 (type == PCI_EXP_TYPE_ROOT_PORT ||	\
		  type == PCI_EXP_TYPE_RC_EC))
#define pcie_cap_has_devctl2(type, flags)		\
		((flags & PCI_EXP_FLAGS_VERS) > 1)
#define pcie_cap_has_lnkctl2(type, flags)		\
		((flags & PCI_EXP_FLAGS_VERS) > 1)
#define pcie_cap_has_sltctl2(type, flags)		\
		((flags & PCI_EXP_FLAGS_VERS) > 1)

static int pci_save_pcie_state(struct pci_dev *dev)
{
	int pos, i = 0;
	struct pci_cap_saved_state *save_state;
	u16 *cap;
	u16 flags;

	pos = pci_pcie_cap(dev);
	if (!pos)
		return 0;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_EXP);
	if (!save_state) {
		dev_err(&dev->dev, "buffer not found in %s\n", __func__);
		return -ENOMEM;
	}
	cap = (u16 *)&save_state->data[0];

	pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &flags);

	if (pcie_cap_has_devctl(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &cap[i++]);
	if (pcie_cap_has_lnkctl(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_LNKCTL, &cap[i++]);
	if (pcie_cap_has_sltctl(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_SLTCTL, &cap[i++]);
	if (pcie_cap_has_rtctl(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_RTCTL, &cap[i++]);
	if (pcie_cap_has_devctl2(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_DEVCTL2, &cap[i++]);
	if (pcie_cap_has_lnkctl2(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_LNKCTL2, &cap[i++]);
	if (pcie_cap_has_sltctl2(dev->pcie_type, flags))
		pci_read_config_word(dev, pos + PCI_EXP_SLTCTL2, &cap[i++]);

	return 0;
}

static void pci_restore_pcie_state(struct pci_dev *dev)
{
	int i = 0, pos;
	struct pci_cap_saved_state *save_state;
	u16 *cap;
	u16 flags;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_EXP);
	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (!save_state || pos <= 0)
		return;
	cap = (u16 *)&save_state->data[0];

	pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &flags);

	if (pcie_cap_has_devctl(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, cap[i++]);
	if (pcie_cap_has_lnkctl(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_LNKCTL, cap[i++]);
	if (pcie_cap_has_sltctl(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_SLTCTL, cap[i++]);
	if (pcie_cap_has_rtctl(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_RTCTL, cap[i++]);
	if (pcie_cap_has_devctl2(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_DEVCTL2, cap[i++]);
	if (pcie_cap_has_lnkctl2(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_LNKCTL2, cap[i++]);
	if (pcie_cap_has_sltctl2(dev->pcie_type, flags))
		pci_write_config_word(dev, pos + PCI_EXP_SLTCTL2, cap[i++]);
}


static int pci_save_pcix_state(struct pci_dev *dev)
{
	int pos;
	struct pci_cap_saved_state *save_state;

	pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (pos <= 0)
		return 0;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_PCIX);
	if (!save_state) {
		dev_err(&dev->dev, "buffer not found in %s\n", __func__);
		return -ENOMEM;
	}

	pci_read_config_word(dev, pos + PCI_X_CMD, (u16 *)save_state->data);

	return 0;
}

static void pci_restore_pcix_state(struct pci_dev *dev)
{
	int i = 0, pos;
	struct pci_cap_saved_state *save_state;
	u16 *cap;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_PCIX);
	pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!save_state || pos <= 0)
		return;
	cap = (u16 *)&save_state->data[0];

	pci_write_config_word(dev, pos + PCI_X_CMD, cap[i++]);
}


/**
 * pci_save_state - save the PCI configuration space of a device before suspending
 * @dev: - PCI device that we're dealing with
 */
int
pci_save_state(struct pci_dev *dev)
{
	int i;
	/* XXX: 100% dword access ok here? */
	for (i = 0; i < 16; i++)
		pci_read_config_dword(dev, i * 4, &dev->saved_config_space[i]);
	dev->state_saved = true;
	if ((i = pci_save_pcie_state(dev)) != 0)
		return i;
	if ((i = pci_save_pcix_state(dev)) != 0)
		return i;
	return 0;
}

/** 
 * pci_restore_state - Restore the saved state of a PCI device
 * @dev: - PCI device that we're dealing with
 */
void pci_restore_state(struct pci_dev *dev)
{
	int i;
	u32 val;

	if (!dev->state_saved)
		return;

	/* PCI Express register must be restored first */
	pci_restore_pcie_state(dev);

	/*
	 * The Base Address register should be programmed before the command
	 * register(s)
	 */
	for (i = 15; i >= 0; i--) {
		pci_read_config_dword(dev, i * 4, &val);
		if (val != dev->saved_config_space[i]) {
			dev_printk(KERN_DEBUG, &dev->dev, "restoring config "
				"space at offset %#x (was %#x, writing %#x)\n",
				i, val, (int)dev->saved_config_space[i]);
			pci_write_config_dword(dev,i * 4,
				dev->saved_config_space[i]);
		}
	}
	pci_restore_pcix_state(dev);
	pci_restore_msi_state(dev);
	pci_restore_iov_state(dev);

	dev->state_saved = false;
}

static int do_pci_enable_device(struct pci_dev *dev, int bars)
{
	int err;

	err = pci_set_power_state(dev, PCI_D0);
	if (err < 0 && err != -EIO)
		return err;
	err = pcibios_enable_device(dev, bars);
	if (err < 0)
		return err;
	pci_fixup_device(pci_fixup_enable, dev);

	return 0;
}

/**
 * pci_reenable_device - Resume abandoned device
 * @dev: PCI device to be resumed
 *
 *  Note this function is a backend of pci_default_resume and is not supposed
 *  to be called by normal code, write proper resume handler and use it instead.
 */
int pci_reenable_device(struct pci_dev *dev)
{
	if (pci_is_enabled(dev))
		return do_pci_enable_device(dev, (1 << PCI_NUM_RESOURCES) - 1);
	return 0;
}

static int __pci_enable_device_flags(struct pci_dev *dev,
				     resource_size_t flags)
{
	int err;
	int i, bars = 0;

	/*
	 * Power state could be unknown at this point, either due to a fresh
	 * boot or a device removal call.  So get the current power state
	 * so that things like MSI message writing will behave as expected
	 * (e.g. if the device really is in D0 at enable time).
	 */
	if (dev->pm_cap) {
		u16 pmcsr;
		pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
		dev->current_state = (pmcsr & PCI_PM_CTRL_STATE_MASK);
	}

	if (atomic_add_return(1, &dev->enable_cnt) > 1)
		return 0;		/* already enabled */

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++)
		if (dev->resource[i].flags & flags)
			bars |= (1 << i);

	err = do_pci_enable_device(dev, bars);
	if (err < 0)
		atomic_dec(&dev->enable_cnt);
	return err;
}

/**
 * pci_enable_device_io - Initialize a device for use with IO space
 * @dev: PCI device to be initialized
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable I/O resources. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 */
int pci_enable_device_io(struct pci_dev *dev)
{
	return __pci_enable_device_flags(dev, IORESOURCE_IO);
}

/**
 * pci_enable_device_mem - Initialize a device for use with Memory space
 * @dev: PCI device to be initialized
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable Memory resources. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 */
int pci_enable_device_mem(struct pci_dev *dev)
{
	return __pci_enable_device_flags(dev, IORESOURCE_MEM);
}

/**
 * pci_enable_device - Initialize device before it's used by a driver.
 * @dev: PCI device to be initialized
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable I/O and memory. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 *
 *  Note we don't actually enable the device many times if we call
 *  this function repeatedly (we just increment the count).
 */
int pci_enable_device(struct pci_dev *dev)
{
	return __pci_enable_device_flags(dev, IORESOURCE_MEM | IORESOURCE_IO);
}

/*
 * Managed PCI resources.  This manages device on/off, intx/msi/msix
 * on/off and BAR regions.  pci_dev itself records msi/msix status, so
 * there's no need to track it separately.  pci_devres is initialized
 * when a device is enabled using managed PCI device enable interface.
 */
struct pci_devres {
	unsigned int enabled:1;
	unsigned int pinned:1;
	unsigned int orig_intx:1;
	unsigned int restore_intx:1;
	u32 region_mask;
};

static void pcim_release(struct device *gendev, void *res)
{
	struct pci_dev *dev = container_of(gendev, struct pci_dev, dev);
	struct pci_devres *this = res;
	int i;

	if (dev->msi_enabled)
		pci_disable_msi(dev);
	if (dev->msix_enabled)
		pci_disable_msix(dev);

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++)
		if (this->region_mask & (1 << i))
			pci_release_region(dev, i);

	if (this->restore_intx)
		pci_intx(dev, this->orig_intx);

	if (this->enabled && !this->pinned)
		pci_disable_device(dev);
}

static struct pci_devres * get_pci_dr(struct pci_dev *pdev)
{
	struct pci_devres *dr, *new_dr;

	dr = devres_find(&pdev->dev, pcim_release, NULL, NULL);
	if (dr)
		return dr;

	new_dr = devres_alloc(pcim_release, sizeof(*new_dr), GFP_KERNEL);
	if (!new_dr)
		return NULL;
	return devres_get(&pdev->dev, new_dr, NULL, NULL);
}

static struct pci_devres * find_pci_dr(struct pci_dev *pdev)
{
	if (pci_is_managed(pdev))
		return devres_find(&pdev->dev, pcim_release, NULL, NULL);
	return NULL;
}

/**
 * pcim_enable_device - Managed pci_enable_device()
 * @pdev: PCI device to be initialized
 *
 * Managed pci_enable_device().
 */
int pcim_enable_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;
	int rc;

	dr = get_pci_dr(pdev);
	if (unlikely(!dr))
		return -ENOMEM;
	if (dr->enabled)
		return 0;

	rc = pci_enable_device(pdev);
	if (!rc) {
		pdev->is_managed = 1;
		dr->enabled = 1;
	}
	return rc;
}

/**
 * pcim_pin_device - Pin managed PCI device
 * @pdev: PCI device to pin
 *
 * Pin managed PCI device @pdev.  Pinned device won't be disabled on
 * driver detach.  @pdev must have been enabled with
 * pcim_enable_device().
 */
void pcim_pin_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;

	dr = find_pci_dr(pdev);
	WARN_ON(!dr || !dr->enabled);
	if (dr)
		dr->pinned = 1;
}

/**
 * pcibios_disable_device - disable arch specific PCI resources for device dev
 * @dev: the PCI device to disable
 *
 * Disables architecture specific PCI resources for the device. This
 * is the default implementation. Architecture implementations can
 * override this.
 */
void __attribute__ ((weak)) pcibios_disable_device (struct pci_dev *dev) {}

static void do_pci_disable_device(struct pci_dev *dev)
{
	u16 pci_command;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	if (pci_command & PCI_COMMAND_MASTER) {
		pci_command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	}

	pcibios_disable_device(dev);
}

/**
 * pci_disable_enabled_device - Disable device without updating enable_cnt
 * @dev: PCI device to disable
 *
 * NOTE: This function is a backend of PCI power management routines and is
 * not supposed to be called drivers.
 */
void pci_disable_enabled_device(struct pci_dev *dev)
{
	if (pci_is_enabled(dev))
		do_pci_disable_device(dev);
}

/**
 * pci_disable_device - Disable PCI device after use
 * @dev: PCI device to be disabled
 *
 * Signal to the system that the PCI device is not in use by the system
 * anymore.  This only involves disabling PCI bus-mastering, if active.
 *
 * Note we don't actually disable the device until all callers of
 * pci_enable_device() have called pci_disable_device().
 */
void
pci_disable_device(struct pci_dev *dev)
{
	struct pci_devres *dr;

	dr = find_pci_dr(dev);
	if (dr)
		dr->enabled = 0;

	if (atomic_sub_return(1, &dev->enable_cnt) != 0)
		return;

	do_pci_disable_device(dev);

	dev->is_busmaster = 0;
}

/**
 * pcibios_set_pcie_reset_state - set reset state for device dev
 * @dev: the PCIe device reset
 * @state: Reset state to enter into
 *
 *
 * Sets the PCIe reset state for the device. This is the default
 * implementation. Architecture implementations can override this.
 */
int __attribute__ ((weak)) pcibios_set_pcie_reset_state(struct pci_dev *dev,
							enum pcie_reset_state state)
{
	return -EINVAL;
}

/**
 * pci_set_pcie_reset_state - set reset state for device dev
 * @dev: the PCIe device reset
 * @state: Reset state to enter into
 *
 *
 * Sets the PCI reset state for the device.
 */
int pci_set_pcie_reset_state(struct pci_dev *dev, enum pcie_reset_state state)
{
	return pcibios_set_pcie_reset_state(dev, state);
}

/**
 * pci_check_pme_status - Check if given device has generated PME.
 * @dev: Device to check.
 *
 * Check the PME status of the device and if set, clear it and clear PME enable
 * (if set).  Return 'true' if PME status and PME enable were both set or
 * 'false' otherwise.
 */
bool pci_check_pme_status(struct pci_dev *dev)
{
	int pmcsr_pos;
	u16 pmcsr;
	bool ret = false;

	if (!dev->pm_cap)
		return false;

	pmcsr_pos = dev->pm_cap + PCI_PM_CTRL;
	pci_read_config_word(dev, pmcsr_pos, &pmcsr);
	if (!(pmcsr & PCI_PM_CTRL_PME_STATUS))
		return false;

	/* Clear PME status. */
	pmcsr |= PCI_PM_CTRL_PME_STATUS;
	if (pmcsr & PCI_PM_CTRL_PME_ENABLE) {
		/* Disable PME to avoid interrupt flood. */
		pmcsr &= ~PCI_PM_CTRL_PME_ENABLE;
		ret = true;
	}

	pci_write_config_word(dev, pmcsr_pos, pmcsr);

	return ret;
}

/**
 * pci_pme_wakeup - Wake up a PCI device if its PME Status bit is set.
 * @dev: Device to handle.
 * @ign: Ignored.
 *
 * Check if @dev has generated PME and queue a resume request for it in that
 * case.
 */
static int pci_pme_wakeup(struct pci_dev *dev, void *ign)
{
	if (pci_check_pme_status(dev)) {
		pci_wakeup_event(dev);
		pm_request_resume(&dev->dev);
	}
	return 0;
}

/**
 * pci_pme_wakeup_bus - Walk given bus and wake up devices on it, if necessary.
 * @bus: Top bus of the subtree to walk.
 */
void pci_pme_wakeup_bus(struct pci_bus *bus)
{
	if (bus)
		pci_walk_bus(bus, pci_pme_wakeup, NULL);
}

/**
 * pci_pme_capable - check the capability of PCI device to generate PME#
 * @dev: PCI device to handle.
 * @state: PCI state from which device will issue PME#.
 */
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state)
{
	if (!dev->pm_cap)
		return false;

	return !!(dev->pme_support & (1 << state));
}

static void pci_pme_list_scan(struct work_struct *work)
{
	struct pci_pme_device *pme_dev;

	mutex_lock(&pci_pme_list_mutex);
	if (!list_empty(&pci_pme_list)) {
		list_for_each_entry(pme_dev, &pci_pme_list, list)
			pci_pme_wakeup(pme_dev->dev, NULL);
		schedule_delayed_work(&pci_pme_work, msecs_to_jiffies(PME_TIMEOUT));
	}
	mutex_unlock(&pci_pme_list_mutex);
}

/**
 * pci_external_pme - is a device an external PCI PME source?
 * @dev: PCI device to check
 *
 */

static bool pci_external_pme(struct pci_dev *dev)
{
	if (pci_is_pcie(dev) || dev->bus->number == 0)
		return false;
	return true;
}

/**
 * pci_pme_active - enable or disable PCI device's PME# function
 * @dev: PCI device to handle.
 * @enable: 'true' to enable PME# generation; 'false' to disable it.
 *
 * The caller must verify that the device is capable of generating PME# before
 * calling this function with @enable equal to 'true'.
 */
void pci_pme_active(struct pci_dev *dev, bool enable)
{
	u16 pmcsr;

	if (!dev->pm_cap)
		return;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	/* Clear PME_Status by writing 1 to it and enable PME# */
	pmcsr |= PCI_PM_CTRL_PME_STATUS | PCI_PM_CTRL_PME_ENABLE;
	if (!enable)
		pmcsr &= ~PCI_PM_CTRL_PME_ENABLE;

	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, pmcsr);

	/* PCI (as opposed to PCIe) PME requires that the device have
	   its PME# line hooked up correctly. Not all hardware vendors
	   do this, so the PME never gets delivered and the device
	   remains asleep. The easiest way around this is to
	   periodically walk the list of suspended devices and check
	   whether any have their PME flag set. The assumption is that
	   we'll wake up often enough anyway that this won't be a huge
	   hit, and the power savings from the devices will still be a
	   win. */

	if (pci_external_pme(dev)) {
		struct pci_pme_device *pme_dev;
		if (enable) {
			pme_dev = kmalloc(sizeof(struct pci_pme_device),
					  GFP_KERNEL);
			if (!pme_dev)
				goto out;
			pme_dev->dev = dev;
			mutex_lock(&pci_pme_list_mutex);
			list_add(&pme_dev->list, &pci_pme_list);
			if (list_is_singular(&pci_pme_list))
				schedule_delayed_work(&pci_pme_work,
						      msecs_to_jiffies(PME_TIMEOUT));
			mutex_unlock(&pci_pme_list_mutex);
		} else {
			mutex_lock(&pci_pme_list_mutex);
			list_for_each_entry(pme_dev, &pci_pme_list, list) {
				if (pme_dev->dev == dev) {
					list_del(&pme_dev->list);
					kfree(pme_dev);
					break;
				}
			}
			mutex_unlock(&pci_pme_list_mutex);
		}
	}

out:
	dev_printk(KERN_DEBUG, &dev->dev, "PME# %s\n",
			enable ? "enabled" : "disabled");
}

/**
 * __pci_enable_wake - enable PCI device as wakeup event source
 * @dev: PCI device affected
 * @state: PCI state from which device will issue wakeup events
 * @runtime: True if the events are to be generated at run time
 * @enable: True to enable event generation; false to disable
 *
 * This enables the device as a wakeup event source, or disables it.
 * When such events involves platform-specific hooks, those hooks are
 * called automatically by this routine.
 *
 * Devices with legacy power management (no standard PCI PM capabilities)
 * always require such platform hooks.
 *
 * RETURN VALUE:
 * 0 is returned on success
 * -EINVAL is returned if device is not supposed to wake up the system
 * Error code depending on the platform is returned if both the platform and
 * the native mechanism fail to enable the generation of wake-up events
 */
int __pci_enable_wake(struct pci_dev *dev, pci_power_t state,
		      bool runtime, bool enable)
{
	int ret = 0;

	if (enable && !runtime && !device_may_wakeup(&dev->dev))
		return -EINVAL;

	/* Don't do the same thing twice in a row for one device. */
	if (!!enable == !!dev->wakeup_prepared)
		return 0;

	/*
	 * According to "PCI System Architecture" 4th ed. by Tom Shanley & Don
	 * Anderson we should be doing PME# wake enable followed by ACPI wake
	 * enable.  To disable wake-up we call the platform first, for symmetry.
	 */

	if (enable) {
		int error;

		if (pci_pme_capable(dev, state))
			pci_pme_active(dev, true);
		else
			ret = 1;
		error = runtime ? platform_pci_run_wake(dev, true) :
					platform_pci_sleep_wake(dev, true);
		if (ret)
			ret = error;
		if (!ret)
			dev->wakeup_prepared = true;
	} else {
		if (runtime)
			platform_pci_run_wake(dev, false);
		else
			platform_pci_sleep_wake(dev, false);
		pci_pme_active(dev, false);
		dev->wakeup_prepared = false;
	}

	return ret;
}
EXPORT_SYMBOL(__pci_enable_wake);

/**
 * pci_wake_from_d3 - enable/disable device to wake up from D3_hot or D3_cold
 * @dev: PCI device to prepare
 * @enable: True to enable wake-up event generation; false to disable
 *
 * Many drivers want the device to wake up the system from D3_hot or D3_cold
 * and this function allows them to set that up cleanly - pci_enable_wake()
 * should not be called twice in a row to enable wake-up due to PCI PM vs ACPI
 * ordering constraints.
 *
 * This function only returns error code if the device is not capable of
 * generating PME# from both D3_hot and D3_cold, and the platform is unable to
 * enable wake-up power for it.
 */
int pci_wake_from_d3(struct pci_dev *dev, bool enable)
{
	return pci_pme_capable(dev, PCI_D3cold) ?
			pci_enable_wake(dev, PCI_D3cold, enable) :
			pci_enable_wake(dev, PCI_D3hot, enable);
}

/**
 * pci_target_state - find an appropriate low power state for a given PCI dev
 * @dev: PCI device
 *
 * Use underlying platform code to find a supported low power state for @dev.
 * If the platform can't manage @dev, return the deepest state from which it
 * can generate wake events, based on any available PME info.
 */
pci_power_t pci_target_state(struct pci_dev *dev)
{
	pci_power_t target_state = PCI_D3hot;

	if (platform_pci_power_manageable(dev)) {
		/*
		 * Call the platform to choose the target state of the device
		 * and enable wake-up from this state if supported.
		 */
		pci_power_t state = platform_pci_choose_state(dev);

		switch (state) {
		case PCI_POWER_ERROR:
		case PCI_UNKNOWN:
			break;
		case PCI_D1:
		case PCI_D2:
			if (pci_no_d1d2(dev))
				break;
		default:
			target_state = state;
		}
	} else if (!dev->pm_cap) {
		target_state = PCI_D0;
	} else if (device_may_wakeup(&dev->dev)) {
		/*
		 * Find the deepest state from which the device can generate
		 * wake-up events, make it the target state and enable device
		 * to generate PME#.
		 */
		if (dev->pme_support) {
			while (target_state
			      && !(dev->pme_support & (1 << target_state)))
				target_state--;
		}
	}

	return target_state;
}

/**
 * pci_prepare_to_sleep - prepare PCI device for system-wide transition into a sleep state
 * @dev: Device to handle.
 *
 * Choose the power state appropriate for the device depending on whether
 * it can wake up the system and/or is power manageable by the platform
 * (PCI_D3hot is the default) and put the device into that state.
 */
int pci_prepare_to_sleep(struct pci_dev *dev)
{
	pci_power_t target_state = pci_target_state(dev);
	int error;

	if (target_state == PCI_POWER_ERROR)
		return -EIO;

	pci_enable_wake(dev, target_state, device_may_wakeup(&dev->dev));

	error = pci_set_power_state(dev, target_state);

	if (error)
		pci_enable_wake(dev, target_state, false);

	return error;
}

/**
 * pci_back_from_sleep - turn PCI device on during system-wide transition into working state
 * @dev: Device to handle.
 *
 * Disable device's system wake-up capability and put it into D0.
 */
int pci_back_from_sleep(struct pci_dev *dev)
{
	pci_enable_wake(dev, PCI_D0, false);
	return pci_set_power_state(dev, PCI_D0);
}

/**
 * pci_finish_runtime_suspend - Carry out PCI-specific part of runtime suspend.
 * @dev: PCI device being suspended.
 *
 * Prepare @dev to generate wake-up events at run time and put it into a low
 * power state.
 */
int pci_finish_runtime_suspend(struct pci_dev *dev)
{
	pci_power_t target_state = pci_target_state(dev);
	int error;

	if (target_state == PCI_POWER_ERROR)
		return -EIO;

	__pci_enable_wake(dev, target_state, true, pci_dev_run_wake(dev));

	error = pci_set_power_state(dev, target_state);

	if (error)
		__pci_enable_wake(dev, target_state, true, false);

	return error;
}

/**
 * pci_dev_run_wake - Check if device can generate run-time wake-up events.
 * @dev: Device to check.
 *
 * Return true if the device itself is cabable of generating wake-up events
 * (through the platform or using the native PCIe PME) or if the device supports
 * PME and one of its upstream bridges can generate wake-up events.
 */
bool pci_dev_run_wake(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;

	if (device_run_wake(&dev->dev))
		return true;

	if (!dev->pme_support)
		return false;

	while (bus->parent) {
		struct pci_dev *bridge = bus->self;

		if (device_run_wake(&bridge->dev))
			return true;

		bus = bus->parent;
	}

	/* We have reached the root bus. */
	if (bus->bridge)
		return device_run_wake(bus->bridge);

	return false;
}
EXPORT_SYMBOL_GPL(pci_dev_run_wake);

/**
 * pci_pm_init - Initialize PM functions of given PCI device
 * @dev: PCI device to handle.
 */
void pci_pm_init(struct pci_dev *dev)
{
	int pm;
	u16 pmc;

	pm_runtime_forbid(&dev->dev);
	device_enable_async_suspend(&dev->dev);
	dev->wakeup_prepared = false;

	dev->pm_cap = 0;

	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (!pm)
		return;
	/* Check device's ability to generate PME# */
	pci_read_config_word(dev, pm + PCI_PM_PMC, &pmc);

	if ((pmc & PCI_PM_CAP_VER_MASK) > 3) {
		dev_err(&dev->dev, "unsupported PM cap regs version (%u)\n",
			pmc & PCI_PM_CAP_VER_MASK);
		return;
	}

	dev->pm_cap = pm;
	dev->d3_delay = PCI_PM_D3_WAIT;

	dev->d1_support = false;
	dev->d2_support = false;
	if (!pci_no_d1d2(dev)) {
		if (pmc & PCI_PM_CAP_D1)
			dev->d1_support = true;
		if (pmc & PCI_PM_CAP_D2)
			dev->d2_support = true;

		if (dev->d1_support || dev->d2_support)
			dev_printk(KERN_DEBUG, &dev->dev, "supports%s%s\n",
				   dev->d1_support ? " D1" : "",
				   dev->d2_support ? " D2" : "");
	}

	pmc &= PCI_PM_CAP_PME_MASK;
	if (pmc) {
		dev_printk(KERN_DEBUG, &dev->dev,
			 "PME# supported from%s%s%s%s%s\n",
			 (pmc & PCI_PM_CAP_PME_D0) ? " D0" : "",
			 (pmc & PCI_PM_CAP_PME_D1) ? " D1" : "",
			 (pmc & PCI_PM_CAP_PME_D2) ? " D2" : "",
			 (pmc & PCI_PM_CAP_PME_D3) ? " D3hot" : "",
			 (pmc & PCI_PM_CAP_PME_D3cold) ? " D3cold" : "");
		dev->pme_support = pmc >> PCI_PM_CAP_PME_SHIFT;
		/*
		 * Make device's PM flags reflect the wake-up capability, but
		 * let the user space enable it to wake up the system as needed.
		 */
		device_set_wakeup_capable(&dev->dev, true);
		/* Disable the PME# generation functionality */
		pci_pme_active(dev, false);
	} else {
		dev->pme_support = 0;
	}
}

/**
 * platform_pci_wakeup_init - init platform wakeup if present
 * @dev: PCI device
 *
 * Some devices don't have PCI PM caps but can still generate wakeup
 * events through platform methods (like ACPI events).  If @dev supports
 * platform wakeup events, set the device flag to indicate as much.  This
 * may be redundant if the device also supports PCI PM caps, but double
 * initialization should be safe in that case.
 */
void platform_pci_wakeup_init(struct pci_dev *dev)
{
	if (!platform_pci_can_wakeup(dev))
		return;

	device_set_wakeup_capable(&dev->dev, true);
	platform_pci_sleep_wake(dev, false);
}

/**
 * pci_add_save_buffer - allocate buffer for saving given capability registers
 * @dev: the PCI device
 * @cap: the capability to allocate the buffer for
 * @size: requested size of the buffer
 */
static int pci_add_cap_save_buffer(
	struct pci_dev *dev, char cap, unsigned int size)
{
	int pos;
	struct pci_cap_saved_state *save_state;

	pos = pci_find_capability(dev, cap);
	if (pos <= 0)
		return 0;

	save_state = kzalloc(sizeof(*save_state) + size, GFP_KERNEL);
	if (!save_state)
		return -ENOMEM;

	save_state->cap_nr = cap;
	pci_add_saved_cap(dev, save_state);

	return 0;
}

/**
 * pci_allocate_cap_save_buffers - allocate buffers for saving capabilities
 * @dev: the PCI device
 */
void pci_allocate_cap_save_buffers(struct pci_dev *dev)
{
	int error;

	error = pci_add_cap_save_buffer(dev, PCI_CAP_ID_EXP,
					PCI_EXP_SAVE_REGS * sizeof(u16));
	if (error)
		dev_err(&dev->dev,
			"unable to preallocate PCI Express save buffer\n");

	error = pci_add_cap_save_buffer(dev, PCI_CAP_ID_PCIX, sizeof(u16));
	if (error)
		dev_err(&dev->dev,
			"unable to preallocate PCI-X save buffer\n");
}

/**
 * pci_enable_ari - enable ARI forwarding if hardware support it
 * @dev: the PCI device
 */
void pci_enable_ari(struct pci_dev *dev)
{
	int pos;
	u32 cap;
	u16 ctrl;
	struct pci_dev *bridge;

	if (!pci_is_pcie(dev) || dev->devfn)
		return;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ARI);
	if (!pos)
		return;

	bridge = dev->bus->self;
	if (!bridge || !pci_is_pcie(bridge))
		return;

	pos = pci_pcie_cap(bridge);
	if (!pos)
		return;

	pci_read_config_dword(bridge, pos + PCI_EXP_DEVCAP2, &cap);
	if (!(cap & PCI_EXP_DEVCAP2_ARI))
		return;

	pci_read_config_word(bridge, pos + PCI_EXP_DEVCTL2, &ctrl);
	ctrl |= PCI_EXP_DEVCTL2_ARI;
	pci_write_config_word(bridge, pos + PCI_EXP_DEVCTL2, ctrl);

	bridge->ari_enabled = 1;
}

static int pci_acs_enable;

/**
 * pci_request_acs - ask for ACS to be enabled if supported
 */
void pci_request_acs(void)
{
	pci_acs_enable = 1;
}

/**
 * pci_enable_acs - enable ACS if hardware support it
 * @dev: the PCI device
 */
void pci_enable_acs(struct pci_dev *dev)
{
	int pos;
	u16 cap;
	u16 ctrl;

	if (!pci_acs_enable)
		return;

	if (!pci_is_pcie(dev))
		return;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ACS);
	if (!pos)
		return;

	pci_read_config_word(dev, pos + PCI_ACS_CAP, &cap);
	pci_read_config_word(dev, pos + PCI_ACS_CTRL, &ctrl);

	/* Source Validation */
	ctrl |= (cap & PCI_ACS_SV);

	/* P2P Request Redirect */
	ctrl |= (cap & PCI_ACS_RR);

	/* P2P Completion Redirect */
	ctrl |= (cap & PCI_ACS_CR);

	/* Upstream Forwarding */
	ctrl |= (cap & PCI_ACS_UF);

	pci_write_config_word(dev, pos + PCI_ACS_CTRL, ctrl);
}

/**
 * pci_swizzle_interrupt_pin - swizzle INTx for device behind bridge
 * @dev: the PCI device
 * @pin: the INTx pin (1=INTA, 2=INTB, 3=INTD, 4=INTD)
 *
 * Perform INTx swizzling for a device behind one level of bridge.  This is
 * required by section 9.1 of the PCI-to-PCI bridge specification for devices
 * behind bridges on add-in cards.  For devices with ARI enabled, the slot
 * number is always 0 (see the Implementation Note in section 2.2.8.1 of
 * the PCI Express Base Specification, Revision 2.1)
 */
u8 pci_swizzle_interrupt_pin(struct pci_dev *dev, u8 pin)
{
	int slot;

	if (pci_ari_enabled(dev->bus))
		slot = 0;
	else
		slot = PCI_SLOT(dev->devfn);

	return (((pin - 1) + slot) % 4) + 1;
}

int
pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge)
{
	u8 pin;

	pin = dev->pin;
	if (!pin)
		return -1;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*bridge = dev;
	return pin;
}

/**
 * pci_common_swizzle - swizzle INTx all the way to root bridge
 * @dev: the PCI device
 * @pinp: pointer to the INTx pin value (1=INTA, 2=INTB, 3=INTD, 4=INTD)
 *
 * Perform INTx swizzling for a device.  This traverses through all PCI-to-PCI
 * bridges all the way up to a PCI root bus.
 */
u8 pci_common_swizzle(struct pci_dev *dev, u8 *pinp)
{
	u8 pin = *pinp;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*pinp = pin;
	return PCI_SLOT(dev->devfn);
}

/**
 *	pci_release_region - Release a PCI bar
 *	@pdev: PCI device whose resources were previously reserved by pci_request_region
 *	@bar: BAR to release
 *
 *	Releases the PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_region.  Call this function only
 *	after all use of the PCI regions has ceased.
 */
void pci_release_region(struct pci_dev *pdev, int bar)
{
	struct pci_devres *dr;

	if (pci_resource_len(pdev, bar) == 0)
		return;
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO)
		release_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM)
		release_mem_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));

	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask &= ~(1 << bar);
}

/**
 *	__pci_request_region - Reserved PCI I/O and memory resource
 *	@pdev: PCI device whose resources are to be reserved
 *	@bar: BAR to be reserved
 *	@res_name: Name to be associated with resource.
 *	@exclusive: whether the region access is exclusive or not
 *
 *	Mark the PCI region associated with PCI device @pdev BR @bar as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	If @exclusive is set, then the region is marked so that userspace
 *	is explicitly not allowed to map the resource via /dev/mem or
 * 	sysfs MMIO access.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
static int __pci_request_region(struct pci_dev *pdev, int bar, const char *res_name,
									int exclusive)
{
	struct pci_devres *dr;

	if (pci_resource_len(pdev, bar) == 0)
		return 0;
		
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
		if (!request_region(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar), res_name))
			goto err_out;
	}
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
		if (!__request_mem_region(pci_resource_start(pdev, bar),
					pci_resource_len(pdev, bar), res_name,
					exclusive))
			goto err_out;
	}

	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask |= 1 << bar;

	return 0;

err_out:
	dev_warn(&pdev->dev, "BAR %d: can't reserve %pR\n", bar,
		 &pdev->resource[bar]);
	return -EBUSY;
}

/**
 *	pci_request_region - Reserve PCI I/O and memory resource
 *	@pdev: PCI device whose resources are to be reserved
 *	@bar: BAR to be reserved
 *	@res_name: Name to be associated with resource
 *
 *	Mark the PCI region associated with PCI device @pdev BAR @bar as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
int pci_request_region(struct pci_dev *pdev, int bar, const char *res_name)
{
	return __pci_request_region(pdev, bar, res_name, 0);
}

/**
 *	pci_request_region_exclusive - Reserved PCI I/O and memory resource
 *	@pdev: PCI device whose resources are to be reserved
 *	@bar: BAR to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark the PCI region associated with PCI device @pdev BR @bar as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 *
 *	The key difference that _exclusive makes it that userspace is
 *	explicitly not allowed to map the resource via /dev/mem or
 * 	sysfs.
 */
int pci_request_region_exclusive(struct pci_dev *pdev, int bar, const char *res_name)
{
	return __pci_request_region(pdev, bar, res_name, IORESOURCE_EXCLUSIVE);
}
/**
 * pci_release_selected_regions - Release selected PCI I/O and memory resources
 * @pdev: PCI device whose resources were previously reserved
 * @bars: Bitmask of BARs to be released
 *
 * Release selected PCI I/O and memory resources previously reserved.
 * Call this function only after all use of the PCI regions has ceased.
 */
void pci_release_selected_regions(struct pci_dev *pdev, int bars)
{
	int i;

	for (i = 0; i < 6; i++)
		if (bars & (1 << i))
			pci_release_region(pdev, i);
}

int __pci_request_selected_regions(struct pci_dev *pdev, int bars,
				 const char *res_name, int excl)
{
	int i;

	for (i = 0; i < 6; i++)
		if (bars & (1 << i))
			if (__pci_request_region(pdev, i, res_name, excl))
				goto err_out;
	return 0;

err_out:
	while(--i >= 0)
		if (bars & (1 << i))
			pci_release_region(pdev, i);

	return -EBUSY;
}


/**
 * pci_request_selected_regions - Reserve selected PCI I/O and memory resources
 * @pdev: PCI device whose resources are to be reserved
 * @bars: Bitmask of BARs to be requested
 * @res_name: Name to be associated with resource
 */
int pci_request_selected_regions(struct pci_dev *pdev, int bars,
				 const char *res_name)
{
	return __pci_request_selected_regions(pdev, bars, res_name, 0);
}

int pci_request_selected_regions_exclusive(struct pci_dev *pdev,
				 int bars, const char *res_name)
{
	return __pci_request_selected_regions(pdev, bars, res_name,
			IORESOURCE_EXCLUSIVE);
}

/**
 *	pci_release_regions - Release reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources were previously reserved by pci_request_regions
 *
 *	Releases all PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_regions.  Call this function only
 *	after all use of the PCI regions has ceased.
 */

void pci_release_regions(struct pci_dev *pdev)
{
	pci_release_selected_regions(pdev, (1 << 6) - 1);
}

/**
 *	pci_request_regions - Reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources are to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark all PCI regions associated with PCI device @pdev as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
int pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	return pci_request_selected_regions(pdev, ((1 << 6) - 1), res_name);
}

/**
 *	pci_request_regions_exclusive - Reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources are to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark all PCI regions associated with PCI device @pdev as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	pci_request_regions_exclusive() will mark the region so that
 * 	/dev/mem and the sysfs MMIO access will not be allowed.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
int pci_request_regions_exclusive(struct pci_dev *pdev, const char *res_name)
{
	return pci_request_selected_regions_exclusive(pdev,
					((1 << 6) - 1), res_name);
}

static void __pci_set_master(struct pci_dev *dev, bool enable)
{
	u16 old_cmd, cmd;

	pci_read_config_word(dev, PCI_COMMAND, &old_cmd);
	if (enable)
		cmd = old_cmd | PCI_COMMAND_MASTER;
	else
		cmd = old_cmd & ~PCI_COMMAND_MASTER;
	if (cmd != old_cmd) {
		dev_dbg(&dev->dev, "%s bus mastering\n",
			enable ? "enabling" : "disabling");
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	dev->is_busmaster = enable;
}

/**
 * pci_set_master - enables bus-mastering for device dev
 * @dev: the PCI device to enable
 *
 * Enables bus-mastering on the device and calls pcibios_set_master()
 * to do the needed arch specific settings.
 */
void pci_set_master(struct pci_dev *dev)
{
	__pci_set_master(dev, true);
	pcibios_set_master(dev);
}

/**
 * pci_clear_master - disables bus-mastering for device dev
 * @dev: the PCI device to disable
 */
void pci_clear_master(struct pci_dev *dev)
{
	__pci_set_master(dev, false);
}

/**
 * pci_set_cacheline_size - ensure the CACHE_LINE_SIZE register is programmed
 * @dev: the PCI device for which MWI is to be enabled
 *
 * Helper function for pci_set_mwi.
 * Originally copied from drivers/net/acenic.c.
 * Copyright 1998-2001 by Jes Sorensen, <jes@trained-monkey.org>.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int pci_set_cacheline_size(struct pci_dev *dev)
{
	u8 cacheline_size;

	if (!pci_cache_line_size)
		return -EINVAL;

	/* Validate current setting: the PCI_CACHE_LINE_SIZE must be
	   equal to or multiple of the right value. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size >= pci_cache_line_size &&
	    (cacheline_size % pci_cache_line_size) == 0)
		return 0;

	/* Write the correct value. */
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cache_line_size);
	/* Read it back. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size == pci_cache_line_size)
		return 0;

	dev_printk(KERN_DEBUG, &dev->dev, "cache line size of %d is not "
		   "supported\n", pci_cache_line_size << 2);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(pci_set_cacheline_size);

#ifdef PCI_DISABLE_MWI
int pci_set_mwi(struct pci_dev *dev)
{
	return 0;
}

int pci_try_set_mwi(struct pci_dev *dev)
{
	return 0;
}

void pci_clear_mwi(struct pci_dev *dev)
{
}

#else

/**
 * pci_set_mwi - enables memory-write-invalidate PCI transaction
 * @dev: the PCI device for which MWI is enabled
 *
 * Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int
pci_set_mwi(struct pci_dev *dev)
{
	int rc;
	u16 cmd;

	rc = pci_set_cacheline_size(dev);
	if (rc)
		return rc;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_INVALIDATE)) {
		dev_dbg(&dev->dev, "enabling Mem-Wr-Inval\n");
		cmd |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	
	return 0;
}

/**
 * pci_try_set_mwi - enables memory-write-invalidate PCI transaction
 * @dev: the PCI device for which MWI is enabled
 *
 * Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND.
 * Callers are not required to check the return value.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int pci_try_set_mwi(struct pci_dev *dev)
{
	int rc = pci_set_mwi(dev);
	return rc;
}

/**
 * pci_clear_mwi - disables Memory-Write-Invalidate for device dev
 * @dev: the PCI device to disable
 *
 * Disables PCI Memory-Write-Invalidate transaction on the device
 */
void
pci_clear_mwi(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (cmd & PCI_COMMAND_INVALIDATE) {
		cmd &= ~PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
}
#endif /* ! PCI_DISABLE_MWI */

/**
 * pci_intx - enables/disables PCI INTx for device dev
 * @pdev: the PCI device to operate on
 * @enable: boolean: whether to enable or disable PCI INTx
 *
 * Enables/disables PCI INTx for device dev
 */
void
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
		struct pci_devres *dr;

		pci_write_config_word(pdev, PCI_COMMAND, new);

		dr = find_pci_dr(pdev);
		if (dr && !dr->restore_intx) {
			dr->restore_intx = 1;
			dr->orig_intx = !enable;
		}
	}
}

/**
 * pci_msi_off - disables any msi or msix capabilities
 * @dev: the PCI device to operate on
 *
 * If you want to use msi see pci_enable_msi and friends.
 * This is a lower level primitive that allows us to disable
 * msi operation at the device level.
 */
void pci_msi_off(struct pci_dev *dev)
{
	int pos;
	u16 control;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (pos) {
		pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &control);
		control &= ~PCI_MSI_FLAGS_ENABLE;
		pci_write_config_word(dev, pos + PCI_MSI_FLAGS, control);
	}
	pos = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (pos) {
		pci_read_config_word(dev, pos + PCI_MSIX_FLAGS, &control);
		control &= ~PCI_MSIX_FLAGS_ENABLE;
		pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);
	}
}
EXPORT_SYMBOL_GPL(pci_msi_off);

int pci_set_dma_max_seg_size(struct pci_dev *dev, unsigned int size)
{
	return dma_set_max_seg_size(&dev->dev, size);
}
EXPORT_SYMBOL(pci_set_dma_max_seg_size);

int pci_set_dma_seg_boundary(struct pci_dev *dev, unsigned long mask)
{
	return dma_set_seg_boundary(&dev->dev, mask);
}
EXPORT_SYMBOL(pci_set_dma_seg_boundary);

static int pcie_flr(struct pci_dev *dev, int probe)
{
	int i;
	int pos;
	u32 cap;
	u16 status, control;

	pos = pci_pcie_cap(dev);
	if (!pos)
		return -ENOTTY;

	pci_read_config_dword(dev, pos + PCI_EXP_DEVCAP, &cap);
	if (!(cap & PCI_EXP_DEVCAP_FLR))
		return -ENOTTY;

	if (probe)
		return 0;

	/* Wait for Transaction Pending bit clean */
	for (i = 0; i < 4; i++) {
		if (i)
			msleep((1 << (i - 1)) * 100);

		pci_read_config_word(dev, pos + PCI_EXP_DEVSTA, &status);
		if (!(status & PCI_EXP_DEVSTA_TRPND))
			goto clear;
	}

	dev_err(&dev->dev, "transaction is not cleared; "
			"proceeding with reset anyway\n");

clear:
	pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &control);
	control |= PCI_EXP_DEVCTL_BCR_FLR;
	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, control);

	msleep(100);

	return 0;
}

static int pci_af_flr(struct pci_dev *dev, int probe)
{
	int i;
	int pos;
	u8 cap;
	u8 status;

	pos = pci_find_capability(dev, PCI_CAP_ID_AF);
	if (!pos)
		return -ENOTTY;

	pci_read_config_byte(dev, pos + PCI_AF_CAP, &cap);
	if (!(cap & PCI_AF_CAP_TP) || !(cap & PCI_AF_CAP_FLR))
		return -ENOTTY;

	if (probe)
		return 0;

	/* Wait for Transaction Pending bit clean */
	for (i = 0; i < 4; i++) {
		if (i)
			msleep((1 << (i - 1)) * 100);

		pci_read_config_byte(dev, pos + PCI_AF_STATUS, &status);
		if (!(status & PCI_AF_STATUS_TP))
			goto clear;
	}

	dev_err(&dev->dev, "transaction is not cleared; "
			"proceeding with reset anyway\n");

clear:
	pci_write_config_byte(dev, pos + PCI_AF_CTRL, PCI_AF_CTRL_FLR);
	msleep(100);

	return 0;
}

static int pci_pm_reset(struct pci_dev *dev, int probe)
{
	u16 csr;

	if (!dev->pm_cap)
		return -ENOTTY;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &csr);
	if (csr & PCI_PM_CTRL_NO_SOFT_RESET)
		return -ENOTTY;

	if (probe)
		return 0;

	if (dev->current_state != PCI_D0)
		return -EINVAL;

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D3hot;
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, csr);
	pci_dev_d3_sleep(dev);

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D0;
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, csr);
	pci_dev_d3_sleep(dev);

	return 0;
}

static int pci_parent_bus_reset(struct pci_dev *dev, int probe)
{
	u16 ctrl;
	struct pci_dev *pdev;

	if (pci_is_root_bus(dev->bus) || dev->subordinate || !dev->bus->self)
		return -ENOTTY;

	list_for_each_entry(pdev, &dev->bus->devices, bus_list)
		if (pdev != dev)
			return -ENOTTY;

	if (probe)
		return 0;

	pci_read_config_word(dev->bus->self, PCI_BRIDGE_CONTROL, &ctrl);
	ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev->bus->self, PCI_BRIDGE_CONTROL, ctrl);
	msleep(100);

	ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev->bus->self, PCI_BRIDGE_CONTROL, ctrl);
	msleep(100);

	return 0;
}

static int pci_dev_reset(struct pci_dev *dev, int probe)
{
	int rc;

	might_sleep();

	if (!probe) {
		pci_block_user_cfg_access(dev);
		/* block PM suspend, driver probe, etc. */
		device_lock(&dev->dev);
	}

	rc = pci_dev_specific_reset(dev, probe);
	if (rc != -ENOTTY)
		goto done;

	rc = pcie_flr(dev, probe);
	if (rc != -ENOTTY)
		goto done;

	rc = pci_af_flr(dev, probe);
	if (rc != -ENOTTY)
		goto done;

	rc = pci_pm_reset(dev, probe);
	if (rc != -ENOTTY)
		goto done;

	rc = pci_parent_bus_reset(dev, probe);
done:
	if (!probe) {
		device_unlock(&dev->dev);
		pci_unblock_user_cfg_access(dev);
	}

	return rc;
}

/**
 * __pci_reset_function - reset a PCI device function
 * @dev: PCI device to reset
 *
 * Some devices allow an individual function to be reset without affecting
 * other functions in the same device.  The PCI device must be responsive
 * to PCI config space in order to use this function.
 *
 * The device function is presumed to be unused when this function is called.
 * Resetting the device will make the contents of PCI configuration space
 * random, so any caller of this must be prepared to reinitialise the
 * device including MSI, bus mastering, BARs, decoding IO and memory spaces,
 * etc.
 *
 * Returns 0 if the device function was successfully reset or negative if the
 * device doesn't support resetting a single function.
 */
int __pci_reset_function(struct pci_dev *dev)
{
	return pci_dev_reset(dev, 0);
}
EXPORT_SYMBOL_GPL(__pci_reset_function);

/**
 * pci_probe_reset_function - check whether the device can be safely reset
 * @dev: PCI device to reset
 *
 * Some devices allow an individual function to be reset without affecting
 * other functions in the same device.  The PCI device must be responsive
 * to PCI config space in order to use this function.
 *
 * Returns 0 if the device function can be reset or negative if the
 * device doesn't support resetting a single function.
 */
int pci_probe_reset_function(struct pci_dev *dev)
{
	return pci_dev_reset(dev, 1);
}

/**
 * pci_reset_function - quiesce and reset a PCI device function
 * @dev: PCI device to reset
 *
 * Some devices allow an individual function to be reset without affecting
 * other functions in the same device.  The PCI device must be responsive
 * to PCI config space in order to use this function.
 *
 * This function does not just reset the PCI portion of a device, but
 * clears all the state associated with the device.  This function differs
 * from __pci_reset_function in that it saves and restores device state
 * over the reset.
 *
 * Returns 0 if the device function was successfully reset or negative if the
 * device doesn't support resetting a single function.
 */
int pci_reset_function(struct pci_dev *dev)
{
	int rc;

	rc = pci_dev_reset(dev, 1);
	if (rc)
		return rc;

	pci_save_state(dev);

	/*
	 * both INTx and MSI are disabled after the Interrupt Disable bit
	 * is set and the Bus Master bit is cleared.
	 */
	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE);

	rc = pci_dev_reset(dev, 0);

	pci_restore_state(dev);

	return rc;
}
EXPORT_SYMBOL_GPL(pci_reset_function);

/**
 * pcix_get_max_mmrbc - get PCI-X maximum designed memory read byte count
 * @dev: PCI device to query
 *
 * Returns mmrbc: maximum designed memory read count in bytes
 *    or appropriate error value.
 */
int pcix_get_max_mmrbc(struct pci_dev *dev)
{
	int cap;
	u32 stat;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_dword(dev, cap + PCI_X_STATUS, &stat))
		return -EINVAL;

	return 512 << ((stat & PCI_X_STATUS_MAX_READ) >> 21);
}
EXPORT_SYMBOL(pcix_get_max_mmrbc);

/**
 * pcix_get_mmrbc - get PCI-X maximum memory read byte count
 * @dev: PCI device to query
 *
 * Returns mmrbc: maximum memory read count in bytes
 *    or appropriate error value.
 */
int pcix_get_mmrbc(struct pci_dev *dev)
{
	int cap;
	u16 cmd;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_word(dev, cap + PCI_X_CMD, &cmd))
		return -EINVAL;

	return 512 << ((cmd & PCI_X_CMD_MAX_READ) >> 2);
}
EXPORT_SYMBOL(pcix_get_mmrbc);

/**
 * pcix_set_mmrbc - set PCI-X maximum memory read byte count
 * @dev: PCI device to query
 * @mmrbc: maximum memory read count in bytes
 *    valid values are 512, 1024, 2048, 4096
 *
 * If possible sets maximum memory read byte count, some bridges have erratas
 * that prevent this.
 */
int pcix_set_mmrbc(struct pci_dev *dev, int mmrbc)
{
	int cap;
	u32 stat, v, o;
	u16 cmd;

	if (mmrbc < 512 || mmrbc > 4096 || !is_power_of_2(mmrbc))
		return -EINVAL;

	v = ffs(mmrbc) - 10;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_dword(dev, cap + PCI_X_STATUS, &stat))
		return -EINVAL;

	if (v > (stat & PCI_X_STATUS_MAX_READ) >> 21)
		return -E2BIG;

	if (pci_read_config_word(dev, cap + PCI_X_CMD, &cmd))
		return -EINVAL;

	o = (cmd & PCI_X_CMD_MAX_READ) >> 2;
	if (o != v) {
		if (v > o && dev->bus &&
		   (dev->bus->bus_flags & PCI_BUS_FLAGS_NO_MMRBC))
			return -EIO;

		cmd &= ~PCI_X_CMD_MAX_READ;
		cmd |= v << 2;
		if (pci_write_config_word(dev, cap + PCI_X_CMD, cmd))
			return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL(pcix_set_mmrbc);

/**
 * pcie_get_readrq - get PCI Express read request size
 * @dev: PCI device to query
 *
 * Returns maximum memory read request in bytes
 *    or appropriate error value.
 */
int pcie_get_readrq(struct pci_dev *dev)
{
	int ret, cap;
	u16 ctl;

	cap = pci_pcie_cap(dev);
	if (!cap)
		return -EINVAL;

	ret = pci_read_config_word(dev, cap + PCI_EXP_DEVCTL, &ctl);
	if (!ret)
		ret = 128 << ((ctl & PCI_EXP_DEVCTL_READRQ) >> 12);

	return ret;
}
EXPORT_SYMBOL(pcie_get_readrq);

/**
 * pcie_set_readrq - set PCI Express maximum memory read request
 * @dev: PCI device to query
 * @rq: maximum memory read count in bytes
 *    valid values are 128, 256, 512, 1024, 2048, 4096
 *
 * If possible sets maximum read byte count
 */
int pcie_set_readrq(struct pci_dev *dev, int rq)
{
	int cap, err = -EINVAL;
	u16 ctl, v;

	if (rq < 128 || rq > 4096 || !is_power_of_2(rq))
		goto out;

	v = (ffs(rq) - 8) << 12;

	cap = pci_pcie_cap(dev);
	if (!cap)
		goto out;

	err = pci_read_config_word(dev, cap + PCI_EXP_DEVCTL, &ctl);
	if (err)
		goto out;

	if ((ctl & PCI_EXP_DEVCTL_READRQ) != v) {
		ctl &= ~PCI_EXP_DEVCTL_READRQ;
		ctl |= v;
		err = pci_write_config_dword(dev, cap + PCI_EXP_DEVCTL, ctl);
	}

out:
	return err;
}
EXPORT_SYMBOL(pcie_set_readrq);

/**
 * pci_select_bars - Make BAR mask from the type of resource
 * @dev: the PCI device for which BAR mask is made
 * @flags: resource type mask to be selected
 *
 * This helper routine makes bar mask from the type of resource.
 */
int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	int i, bars = 0;
	for (i = 0; i < PCI_NUM_RESOURCES; i++)
		if (pci_resource_flags(dev, i) & flags)
			bars |= (1 << i);
	return bars;
}

/**
 * pci_resource_bar - get position of the BAR associated with a resource
 * @dev: the PCI device
 * @resno: the resource number
 * @type: the BAR type to be filled in
 *
 * Returns BAR position in config space, or 0 if the BAR is invalid.
 */
int pci_resource_bar(struct pci_dev *dev, int resno, enum pci_bar_type *type)
{
	int reg;

	if (resno < PCI_ROM_RESOURCE) {
		*type = pci_bar_unknown;
		return PCI_BASE_ADDRESS_0 + 4 * resno;
	} else if (resno == PCI_ROM_RESOURCE) {
		*type = pci_bar_mem32;
		return dev->rom_base_reg;
	} else if (resno < PCI_BRIDGE_RESOURCES) {
		/* device specific resource */
		reg = pci_iov_resource_bar(dev, resno, type);
		if (reg)
			return reg;
	}

	dev_err(&dev->dev, "BAR %d: invalid resource\n", resno);
	return 0;
}

/* Some architectures require additional programming to enable VGA */
static arch_set_vga_state_t arch_set_vga_state;

void __init pci_register_set_vga_state(arch_set_vga_state_t func)
{
	arch_set_vga_state = func;	/* NULL disables */
}

static int pci_set_vga_state_arch(struct pci_dev *dev, bool decode,
		      unsigned int command_bits, bool change_bridge)
{
	if (arch_set_vga_state)
		return arch_set_vga_state(dev, decode, command_bits,
						change_bridge);
	return 0;
}

/**
 * pci_set_vga_state - set VGA decode state on device and parents if requested
 * @dev: the PCI device
 * @decode: true = enable decoding, false = disable decoding
 * @command_bits: PCI_COMMAND_IO and/or PCI_COMMAND_MEMORY
 * @change_bridge: traverse ancestors and change bridges
 */
int pci_set_vga_state(struct pci_dev *dev, bool decode,
		      unsigned int command_bits, bool change_bridge)
{
	struct pci_bus *bus;
	struct pci_dev *bridge;
	u16 cmd;
	int rc;

	WARN_ON(command_bits & ~(PCI_COMMAND_IO|PCI_COMMAND_MEMORY));

	/* ARCH specific VGA enables */
	rc = pci_set_vga_state_arch(dev, decode, command_bits, change_bridge);
	if (rc)
		return rc;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (decode == true)
		cmd |= command_bits;
	else
		cmd &= ~command_bits;
	pci_write_config_word(dev, PCI_COMMAND, cmd);

	if (change_bridge == false)
		return 0;

	bus = dev->bus;
	while (bus) {
		bridge = bus->self;
		if (bridge) {
			pci_read_config_word(bridge, PCI_BRIDGE_CONTROL,
					     &cmd);
			if (decode == true)
				cmd |= PCI_BRIDGE_CTL_VGA;
			else
				cmd &= ~PCI_BRIDGE_CTL_VGA;
			pci_write_config_word(bridge, PCI_BRIDGE_CONTROL,
					      cmd);
		}
		bus = bus->parent;
	}
	return 0;
}

#define RESOURCE_ALIGNMENT_PARAM_SIZE COMMAND_LINE_SIZE
static char resource_alignment_param[RESOURCE_ALIGNMENT_PARAM_SIZE] = {0};
static DEFINE_SPINLOCK(resource_alignment_lock);

/**
 * pci_specified_resource_alignment - get resource alignment specified by user.
 * @dev: the PCI device to get
 *
 * RETURNS: Resource alignment if it is specified.
 *          Zero if it is not specified.
 */
resource_size_t pci_specified_resource_alignment(struct pci_dev *dev)
{
	int seg, bus, slot, func, align_order, count;
	resource_size_t align = 0;
	char *p;

	spin_lock(&resource_alignment_lock);
	p = resource_alignment_param;
	while (*p) {
		count = 0;
		if (sscanf(p, "%d%n", &align_order, &count) == 1 &&
							p[count] == '@') {
			p += count + 1;
		} else {
			align_order = -1;
		}
		if (sscanf(p, "%x:%x:%x.%x%n",
			&seg, &bus, &slot, &func, &count) != 4) {
			seg = 0;
			if (sscanf(p, "%x:%x.%x%n",
					&bus, &slot, &func, &count) != 3) {
				/* Invalid format */
				printk(KERN_ERR "PCI: Can't parse resource_alignment parameter: %s\n",
					p);
				break;
			}
		}
		p += count;
		if (seg == pci_domain_nr(dev->bus) &&
			bus == dev->bus->number &&
			slot == PCI_SLOT(dev->devfn) &&
			func == PCI_FUNC(dev->devfn)) {
			if (align_order == -1) {
				align = PAGE_SIZE;
			} else {
				align = 1 << align_order;
			}
			/* Found */
			break;
		}
		if (*p != ';' && *p != ',') {
			/* End of param or invalid format */
			break;
		}
		p++;
	}
	spin_unlock(&resource_alignment_lock);
	return align;
}

/**
 * pci_is_reassigndev - check if specified PCI is target device to reassign
 * @dev: the PCI device to check
 *
 * RETURNS: non-zero for PCI device is a target device to reassign,
 *          or zero is not.
 */
int pci_is_reassigndev(struct pci_dev *dev)
{
	return (pci_specified_resource_alignment(dev) != 0);
}

ssize_t pci_set_resource_alignment_param(const char *buf, size_t count)
{
	if (count > RESOURCE_ALIGNMENT_PARAM_SIZE - 1)
		count = RESOURCE_ALIGNMENT_PARAM_SIZE - 1;
	spin_lock(&resource_alignment_lock);
	strncpy(resource_alignment_param, buf, count);
	resource_alignment_param[count] = '\0';
	spin_unlock(&resource_alignment_lock);
	return count;
}

ssize_t pci_get_resource_alignment_param(char *buf, size_t size)
{
	size_t count;
	spin_lock(&resource_alignment_lock);
	count = snprintf(buf, size, "%s", resource_alignment_param);
	spin_unlock(&resource_alignment_lock);
	return count;
}

static ssize_t pci_resource_alignment_show(struct bus_type *bus, char *buf)
{
	return pci_get_resource_alignment_param(buf, PAGE_SIZE);
}

static ssize_t pci_resource_alignment_store(struct bus_type *bus,
					const char *buf, size_t count)
{
	return pci_set_resource_alignment_param(buf, count);
}

BUS_ATTR(resource_alignment, 0644, pci_resource_alignment_show,
					pci_resource_alignment_store);

static int __init pci_resource_alignment_sysfs_init(void)
{
	return bus_create_file(&pci_bus_type,
					&bus_attr_resource_alignment);
}

late_initcall(pci_resource_alignment_sysfs_init);

static void __devinit pci_no_domains(void)
{
#ifdef CONFIG_PCI_DOMAINS
	pci_domains_supported = 0;
#endif
}

/**
 * pci_ext_cfg_enabled - can we access extended PCI config space?
 * @dev: The PCI device of the root bridge.
 *
 * Returns 1 if we can access PCI extended config space (offsets
 * greater than 0xff). This is the default implementation. Architecture
 * implementations can override this.
 */
int __attribute__ ((weak)) pci_ext_cfg_avail(struct pci_dev *dev)
{
	return 1;
}

void __weak pci_fixup_cardbus(struct pci_bus *bus)
{
}
EXPORT_SYMBOL(pci_fixup_cardbus);

static int __init pci_setup(char *str)
{
	while (str) {
		char *k = strchr(str, ',');
		if (k)
			*k++ = 0;
		if (*str && (str = pcibios_setup(str)) && *str) {
			if (!strcmp(str, "nomsi")) {
				pci_no_msi();
			} else if (!strcmp(str, "noaer")) {
				pci_no_aer();
			} else if (!strcmp(str, "nodomains")) {
				pci_no_domains();
			} else if (!strncmp(str, "cbiosize=", 9)) {
				pci_cardbus_io_size = memparse(str + 9, &str);
			} else if (!strncmp(str, "cbmemsize=", 10)) {
				pci_cardbus_mem_size = memparse(str + 10, &str);
			} else if (!strncmp(str, "resource_alignment=", 19)) {
				pci_set_resource_alignment_param(str + 19,
							strlen(str + 19));
			} else if (!strncmp(str, "ecrc=", 5)) {
				pcie_ecrc_get_policy(str + 5);
			} else if (!strncmp(str, "hpiosize=", 9)) {
				pci_hotplug_io_size = memparse(str + 9, &str);
			} else if (!strncmp(str, "hpmemsize=", 10)) {
				pci_hotplug_mem_size = memparse(str + 10, &str);
			} else {
				printk(KERN_ERR "PCI: Unknown option `%s'\n",
						str);
			}
		}
		str = k;
	}
	return 0;
}
early_param("pci", pci_setup);

EXPORT_SYMBOL(pci_reenable_device);
EXPORT_SYMBOL(pci_enable_device_io);
EXPORT_SYMBOL(pci_enable_device_mem);
EXPORT_SYMBOL(pci_enable_device);
EXPORT_SYMBOL(pcim_enable_device);
EXPORT_SYMBOL(pcim_pin_device);
EXPORT_SYMBOL(pci_disable_device);
EXPORT_SYMBOL(pci_find_capability);
EXPORT_SYMBOL(pci_bus_find_capability);
EXPORT_SYMBOL(pci_release_regions);
EXPORT_SYMBOL(pci_request_regions);
EXPORT_SYMBOL(pci_request_regions_exclusive);
EXPORT_SYMBOL(pci_release_region);
EXPORT_SYMBOL(pci_request_region);
EXPORT_SYMBOL(pci_request_region_exclusive);
EXPORT_SYMBOL(pci_release_selected_regions);
EXPORT_SYMBOL(pci_request_selected_regions);
EXPORT_SYMBOL(pci_request_selected_regions_exclusive);
EXPORT_SYMBOL(pci_set_master);
EXPORT_SYMBOL(pci_clear_master);
EXPORT_SYMBOL(pci_set_mwi);
EXPORT_SYMBOL(pci_try_set_mwi);
EXPORT_SYMBOL(pci_clear_mwi);
EXPORT_SYMBOL_GPL(pci_intx);
EXPORT_SYMBOL(pci_assign_resource);
EXPORT_SYMBOL(pci_find_parent_resource);
EXPORT_SYMBOL(pci_select_bars);

EXPORT_SYMBOL(pci_set_power_state);
EXPORT_SYMBOL(pci_save_state);
EXPORT_SYMBOL(pci_restore_state);
EXPORT_SYMBOL(pci_pme_capable);
EXPORT_SYMBOL(pci_pme_active);
EXPORT_SYMBOL(pci_wake_from_d3);
EXPORT_SYMBOL(pci_target_state);
EXPORT_SYMBOL(pci_prepare_to_sleep);
EXPORT_SYMBOL(pci_back_from_sleep);
EXPORT_SYMBOL_GPL(pci_set_pcie_reset_state);
