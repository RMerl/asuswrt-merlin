#ifndef LINUX_SPI_FLASH_H
#define LINUX_SPI_FLASH_H

struct mtd_partition;

struct flash_platform_data {
	char		*name;
	struct mtd_partition *parts;
	unsigned int	nr_parts;

	char		*type;

	/* we'll likely add more ... use JEDEC IDs, etc */
};

#endif
