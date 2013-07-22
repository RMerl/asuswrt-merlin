/*
 * topology - gathers information about device topology
 *
 * Copyright 2009 Red Hat, Inc.  All rights reserved.
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "topology.h"

/**
 * SECTION:topology
 * @title: Topology information
 * @short_description: block device topology information.
 *
 * The topology chain provides details about Linux block devices, for more
 * information see:
 *
 *      Linux kernel Documentation/ABI/testing/sysfs-block
 *
 * NAME=value (tags) interface is enabled by blkid_probe_enable_topology(),
 * and provides:
 *
 * @LOGICAL_SECTOR_SIZE: this is the smallest unit the storage device can
 *                       address. It is typically 512 bytes.
 *
 * @PHYSICAL_SECTOR_SIZE: this is the smallest unit a physical storage device
 *                        can write atomically. It is usually the same as the
 *                        logical sector size but may be bigger.
 *
 * @MINIMUM_IO_SIZE: minimum size which is the device's preferred unit of I/O.
 *                   For RAID arrays it is often the stripe chunk size.
 *
 * @OPTIMAL_IO_SIZE: usually the stripe width for RAID or zero. For RAID arrays
 *                   it is usually the stripe width or the internal track size.
 *
 * @ALIGNMENT_OFFSET: indicates how many bytes the beginning of the device is
 *                    offset from the disk's natural alignment.
 *
 * The NAME=value tags are not defined when the corresponding topology value
 * is zero. The MINIMUM_IO_SIZE should be always defined if kernel provides
 * topology information.
 *
 * Binary interface:
 *
 * blkid_probe_get_topology()
 *
 * blkid_topology_get_'VALUENAME'()
 */
static int topology_probe(blkid_probe pr, struct blkid_chain *chn);
static void topology_free(blkid_probe pr, void *data);
static int topology_is_complete(blkid_probe pr);
static int topology_set_logical_sector_size(blkid_probe pr);

/*
 * Binary interface
 */
struct blkid_struct_topology {
	unsigned long	alignment_offset;
	unsigned long	minimum_io_size;
	unsigned long	optimal_io_size;
	unsigned long	logical_sector_size;
	unsigned long	physical_sector_size;
};

/*
 * Topology chain probing functions
 */
static const struct blkid_idinfo *idinfos[] =
{
#ifdef __linux__
	&ioctl_tp_idinfo,
	&sysfs_tp_idinfo,
	&md_tp_idinfo,
	&dm_tp_idinfo,
	&lvm_tp_idinfo,
	&evms_tp_idinfo
#endif
};


/*
 * Driver definition
 */
const struct blkid_chaindrv topology_drv = {
	.id           = BLKID_CHAIN_TOPLGY,
	.name         = "topology",
	.dflt_enabled = FALSE,
	.idinfos      = idinfos,
	.nidinfos     = ARRAY_SIZE(idinfos),
	.probe        = topology_probe,
	.safeprobe    = topology_probe,
	.free_data    = topology_free
};

/**
 * blkid_probe_enable_topology:
 * @pr: probe
 * @enable: TRUE/FALSE
 *
 * Enables/disables the topology probing for non-binary interface.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_enable_topology(blkid_probe pr, int enable)
{
	if (!pr)
		return -1;
	pr->chains[BLKID_CHAIN_TOPLGY].enabled = enable;
	return 0;
}

/**
 * blkid_probe_get_topology:
 * @pr: probe
 *
 * This is a binary interface for topology values. See also blkid_topology_*
 * functions.
 *
 * This function is independent on blkid_do_[safe,full]probe() and
 * blkid_probe_enable_topology() calls.
 *
 * WARNING: the returned object will be overwritten by the next
 *          blkid_probe_get_topology() call for the same @pr. If you want to
 *          use more blkid_topopogy objects in the same time you have to create
 *          more blkid_probe handlers (see blkid_new_probe()).
 *
 * Returns: blkid_topopogy, or NULL in case of error.
 */
blkid_topology blkid_probe_get_topology(blkid_probe pr)
{
	return (blkid_topology) blkid_probe_get_binary_data(pr,
			&pr->chains[BLKID_CHAIN_TOPLGY]);
}

/*
 * The blkid_do_probe() backend.
 */
static int topology_probe(blkid_probe pr, struct blkid_chain *chn)
{
	size_t i;

	if (!pr || chn->idx < -1)
		return -1;

	if (!S_ISBLK(pr->mode))
		return -1;	/* nothing, works with block devices only */

	if (chn->binary) {
		DBG(DEBUG_LOWPROBE, printf("initialize topology binary data\n"));

		if (chn->data)
			/* reset binary data */
			memset(chn->data, 0,
					sizeof(struct blkid_struct_topology));
		else {
			chn->data = calloc(1,
					sizeof(struct blkid_struct_topology));
			if (!chn->data)
				return -1;
		}
	}

	blkid_probe_chain_reset_vals(pr, chn);

	DBG(DEBUG_LOWPROBE,
		printf("--> starting probing loop [TOPOLOGY idx=%d]\n",
		chn->idx));

	i = chn->idx < 0 ? 0 : chn->idx + 1U;

	for ( ; i < ARRAY_SIZE(idinfos); i++) {
		const struct blkid_idinfo *id = idinfos[i];

		chn->idx = i;

		if (id->probefunc) {
			DBG(DEBUG_LOWPROBE, printf(
				"%s: call probefunc()\n", id->name));
			if (id->probefunc(pr, NULL) != 0)
				continue;
		}

		if (!topology_is_complete(pr))
			continue;

		/* generic for all probing drivers */
		topology_set_logical_sector_size(pr);

		DBG(DEBUG_LOWPROBE,
			printf("<-- leaving probing loop (type=%s) [TOPOLOGY idx=%d]\n",
			id->name, chn->idx));
		return 0;
	}

	DBG(DEBUG_LOWPROBE,
		printf("<-- leaving probing loop (failed) [TOPOLOGY idx=%d]\n",
		chn->idx));
	return 1;
}

static void topology_free(blkid_probe pr __attribute__((__unused__)),
			  void *data)
{
	free(data);
}

static int topology_set_value(blkid_probe pr, const char *name,
				size_t structoff, unsigned long data)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (!chn)
		return -1;
	if (!data)
		return 0;	/* ignore zeros */

	if (chn->binary) {
		unsigned long *v =
			(unsigned long *) (chn->data + structoff);
		*v = data;
		return 0;
	}
	return blkid_probe_sprintf_value(pr, name, "%llu", data);
}

/* the topology info is complete when we have at least "minimum_io_size" which
 * is provided by all blkid topology drivers */
static int topology_is_complete(blkid_probe pr)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (!chn)
		return FALSE;

	if (chn->binary && chn->data) {
		blkid_topology tp = (blkid_topology) chn->data;
		if (tp->minimum_io_size)
			return TRUE;
	}

	return __blkid_probe_lookup_value(pr, "MINIMUM_IO_SIZE") ? TRUE : FALSE;
}

int blkid_topology_set_alignment_offset(blkid_probe pr, int val)
{
	unsigned long xval;

	/* Welcome to Hell. The kernel is able to return -1 as an
	 * alignment_offset if no compatible sizes and alignments
	 * exist for stacked devices.
	 *
	 * There is no way how libblkid caller can respond to the value -1, so
	 * we will hide this corner case...
	 *
	 * (TODO: maybe we can export an extra boolean value 'misaligned' rather
	 *  then complete hide this problem.)
	 */
	xval = val < 0 ? 0 : val;

	return topology_set_value(pr,
			"ALIGNMENT_OFFSET",
			offsetof(struct blkid_struct_topology, alignment_offset),
			xval);
}

int blkid_topology_set_minimum_io_size(blkid_probe pr, unsigned long val)
{
	return topology_set_value(pr,
			"MINIMUM_IO_SIZE",
			offsetof(struct blkid_struct_topology, minimum_io_size),
			val);
}

int blkid_topology_set_optimal_io_size(blkid_probe pr, unsigned long val)
{
	return topology_set_value(pr,
			"OPTIMAL_IO_SIZE",
			offsetof(struct blkid_struct_topology, optimal_io_size),
			val);
}

/* BLKSSZGET is provided on all systems since 2.3.3 -- so we don't have to
 * waste time with sysfs.
 */
static int topology_set_logical_sector_size(blkid_probe pr)
{
	unsigned long val = blkid_probe_get_sectorsize(pr);

	if (!val)
		return -1;

	return topology_set_value(pr,
			"LOGICAL_SECTOR_SIZE",
			offsetof(struct blkid_struct_topology, logical_sector_size),
			val);
}

int blkid_topology_set_physical_sector_size(blkid_probe pr, unsigned long val)
{
	return topology_set_value(pr,
			"PHYSICAL_SECTOR_SIZE",
			offsetof(struct blkid_struct_topology, physical_sector_size),
			val);
}

/**
 * blkid_topology_get_alignment_offset:
 * @tp: topology
 *
 * Returns: alignment offset in bytes or 0.
 */
unsigned long blkid_topology_get_alignment_offset(blkid_topology tp)
{
	return tp ? tp->alignment_offset : 0;
}

/**
 * blkid_topology_get_minimum_io_size:
 * @tp: topology
 *
 * Returns: minimum io size in bytes or 0.
 */
unsigned long blkid_topology_get_minimum_io_size(blkid_topology tp)
{
	return tp ? tp->minimum_io_size : 0;
}

/**
 * blkid_topology_get_optimal_io_size
 * @tp: topology
 *
 * Returns: optimal io size in bytes or 0.
 */
unsigned long blkid_topology_get_optimal_io_size(blkid_topology tp)
{
	return tp ? tp->optimal_io_size : 0;
}

/**
 * blkid_topology_get_logical_sector_size
 * @tp: topology
 *
 * Returns: logical sector size (BLKSSZGET ioctl) in bytes or 0.
 */
unsigned long blkid_topology_get_logical_sector_size(blkid_topology tp)
{
	return tp ? tp->logical_sector_size : 0;
}

/**
 * blkid_topology_get_physical_sector_size
 * @tp: topology
 *
 * Returns: logical sector size (BLKSSZGET ioctl) in bytes or 0.
 */
unsigned long blkid_topology_get_physical_sector_size(blkid_topology tp)
{
	return tp ? tp->physical_sector_size : 0;
}

