#ifndef BLKID_TOPOLOGY_H
#define BLKID_TOPOLOGY_H

#include "blkidP.h"

extern int blkid_topology_set_alignment_offset(blkid_probe pr, int val);
extern int blkid_topology_set_minimum_io_size(blkid_probe pr, unsigned long val);
extern int blkid_topology_set_optimal_io_size(blkid_probe pr, unsigned long val);
extern int blkid_topology_set_physical_sector_size(blkid_probe pr, unsigned long val);

/*
 * topology probers
 */
#ifdef __linux__
extern const struct blkid_idinfo ioctl_tp_idinfo;
extern const struct blkid_idinfo md_tp_idinfo;
extern const struct blkid_idinfo evms_tp_idinfo;
extern const struct blkid_idinfo sysfs_tp_idinfo;
extern const struct blkid_idinfo dm_tp_idinfo;
extern const struct blkid_idinfo lvm_tp_idinfo;
#endif

#endif /* BLKID_TOPOLOGY_H */

