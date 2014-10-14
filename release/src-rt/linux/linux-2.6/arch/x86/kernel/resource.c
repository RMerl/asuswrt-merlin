#include <linux/ioport.h>
#include <asm/e820.h>

static void resource_clip(struct resource *res, resource_size_t start,
			  resource_size_t end)
{
	resource_size_t low = 0, high = 0;

	if (res->end < start || res->start > end)
		return;		/* no conflict */

	if (res->start < start)
		low = start - res->start;

	if (res->end > end)
		high = res->end - end;

	/* Keep the area above or below the conflict, whichever is larger */
	if (low > high)
		res->end = start - 1;
	else
		res->start = end + 1;
}

static void remove_e820_regions(struct resource *avail)
{
	int i;
	struct e820entry *entry;

	for (i = 0; i < e820.nr_map; i++) {
		entry = &e820.map[i];

		resource_clip(avail, entry->addr,
			      entry->addr + entry->size - 1);
	}
}

void arch_remove_reservations(struct resource *avail)
{
	/* Trim out BIOS areas (low 1MB and high 2MB) and E820 regions */
	if (avail->flags & IORESOURCE_MEM) {
		if (avail->start < BIOS_END)
			avail->start = BIOS_END;
		resource_clip(avail, BIOS_ROM_BASE, BIOS_ROM_END);

		remove_e820_regions(avail);
	}
}
