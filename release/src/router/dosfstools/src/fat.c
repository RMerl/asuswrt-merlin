/* fat.c - Read/write access to the FAT

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.

   On Debian systems, the complete text of the GNU General Public License
   can be found in /usr/share/common-licenses/GPL-3 file.
*/

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "dosfsck.h"
#include "io.h"
#include "check.h"
#include "fat.h"

/**
 * Fetch the FAT entry for a specified cluster.
 *
 * @param[out]  entry	    Cluster to which cluster of interest is linked
 * @param[in]	fat	    FAT table for the partition
 * @param[in]	cluster     Cluster of interest
 * @param[in]	fs          Information from the FAT boot sectors (bits per FAT entry)
 */
void get_fat(FAT_ENTRY * entry, void *fat, unsigned long cluster, DOS_FS * fs)
{
    unsigned char *ptr;

    switch (fs->fat_bits) {
    case 12:
	ptr = &((unsigned char *)fat)[cluster * 3 / 2];
	entry->value = 0xfff & (cluster & 1 ? (ptr[0] >> 4) | (ptr[1] << 4) :
				(ptr[0] | ptr[1] << 8));
	break;
    case 16:
	entry->value = CF_LE_W(((unsigned short *)fat)[cluster]);
	break;
    case 32:
	/* According to M$, the high 4 bits of a FAT32 entry are reserved and
	 * are not part of the cluster number. So we cut them off. */
	{
	    unsigned long e = CF_LE_L(((unsigned int *)fat)[cluster]);
	    entry->value = e & 0xfffffff;
	    entry->reserved = e >> 28;
	}
	break;
    default:
	die("Bad FAT entry size: %d bits.", fs->fat_bits);
    }
}

/**
 * Build a bookkeeping structure from the partition's FAT table.
 * If the partition has multiple FATs and they don't agree, try to pick a winner,
 * and queue a command to overwrite the loser.
 * One error that is fixed here is a cluster that links to something out of range.
 *
 * @param[inout]    fs      Information about the filesystem
 */
void read_fat(DOS_FS * fs)
{
    int eff_size;
    unsigned long i;
    void *first, *second = NULL;
    int first_ok, second_ok;
    unsigned long total_num_clusters;

    /* Clean up from previous pass */
    free(fs->fat);
    free(fs->cluster_owner);
    fs->fat = NULL;
    fs->cluster_owner = NULL;

    total_num_clusters = fs->clusters + 2UL;
    eff_size = (total_num_clusters * fs->fat_bits + 7) / 8ULL;
    first = alloc(eff_size);
    fs_read(fs->fat_start, eff_size, first);
    if (fs->nfats > 1) {
	second = alloc(eff_size);
	fs_read(fs->fat_start + fs->fat_size, eff_size, second);
    }
    if (second && memcmp(first, second, eff_size) != 0) {
	FAT_ENTRY first_media, second_media;
	get_fat(&first_media, first, 0, fs);
	get_fat(&second_media, second, 0, fs);
	first_ok = (first_media.value & FAT_EXTD(fs)) == FAT_EXTD(fs);
	second_ok = (second_media.value & FAT_EXTD(fs)) == FAT_EXTD(fs);
	if (first_ok && !second_ok) {
	    printf("FATs differ - using first FAT.\n");
	    fs_write(fs->fat_start + fs->fat_size, eff_size, first);
	}
	if (!first_ok && second_ok) {
	    printf("FATs differ - using second FAT.\n");
	    fs_write(fs->fat_start, eff_size, second);
	    memcpy(first, second, eff_size);
	}
	if (first_ok && second_ok) {
	    if (interactive) {
		printf("FATs differ but appear to be intact. Use which FAT ?\n"
		       "1) Use first FAT\n2) Use second FAT\n");
		if (get_key("12", "?") == '1') {
		    fs_write(fs->fat_start + fs->fat_size, eff_size, first);
		} else {
		    fs_write(fs->fat_start, eff_size, second);
		    memcpy(first, second, eff_size);
		}
	    } else {
		printf("FATs differ but appear to be intact. Using first "
		       "FAT.\n");
		fs_write(fs->fat_start + fs->fat_size, eff_size, first);
	    }
	}
	if (!first_ok && !second_ok) {
	    printf("Both FATs appear to be corrupt. Giving up.\n");
	    exit(1);
	}
    }
    if (second) {
	free(second);
    }
    fs->fat = (unsigned char *)first;

    fs->cluster_owner = alloc(total_num_clusters * sizeof(DOS_FILE *));
    memset(fs->cluster_owner, 0, (total_num_clusters * sizeof(DOS_FILE *)));

    /* Truncate any cluster chains that link to something out of range */
    for (i = 2; i < fs->clusters + 2; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);
	if (curEntry.value == 1) {
	    printf("Cluster %ld out of range (1). Setting to EOF.\n", i - 2);
	    set_fat(fs, i, -1);
	}
	if (curEntry.value >= fs->clusters + 2 &&
	    (curEntry.value < FAT_MIN_BAD(fs))) {
	    printf("Cluster %ld out of range (%ld > %ld). Setting to EOF.\n",
		   i - 2, curEntry.value, fs->clusters + 2 - 1);
	    set_fat(fs, i, -1);
	}
    }
}

/**
 * Update the FAT entry for a specified cluster
 * (i.e., change the cluster it links to).
 * Queue a command to write out this change.
 *
 * @param[in,out]   fs          Information about the filesystem
 * @param[in]	    cluster     Cluster to change
 * @param[in]       new	        Cluster to link to
 *				Special values:
 *				   0 == free cluster
 *				  -1 == end-of-chain
 *				  -2 == bad cluster
 */
void set_fat(DOS_FS * fs, unsigned long cluster, unsigned long new)
{
    unsigned char *data = NULL;
    int size;
    loff_t offs;

    if ((long)new == -1)
	new = FAT_EOF(fs);
    else if ((long)new == -2)
	new = FAT_BAD(fs);
    switch (fs->fat_bits) {
    case 12:
	data = fs->fat + cluster * 3 / 2;
	offs = fs->fat_start + cluster * 3 / 2;
	if (cluster & 1) {
	    FAT_ENTRY prevEntry;
	    get_fat(&prevEntry, fs->fat, cluster - 1, fs);
	    data[0] = ((new & 0xf) << 4) | (prevEntry.value >> 8);
	    data[1] = new >> 4;
	} else {
	    FAT_ENTRY subseqEntry;
	    get_fat(&subseqEntry, fs->fat, cluster + 1, fs);
	    data[0] = new & 0xff;
	    data[1] = (new >> 8) | (cluster == fs->clusters - 1 ? 0 :
				    (0xff & subseqEntry.value) << 4);
	}
	size = 2;
	break;
    case 16:
	data = fs->fat + cluster * 2;
	offs = fs->fat_start + cluster * 2;
	*(unsigned short *)data = CT_LE_W(new);
	size = 2;
	break;
    case 32:
	{
	    FAT_ENTRY curEntry;
	    get_fat(&curEntry, fs->fat, cluster, fs);

	    data = fs->fat + cluster * 4;
	    offs = fs->fat_start + cluster * 4;
	    /* According to M$, the high 4 bits of a FAT32 entry are reserved and
	     * are not part of the cluster number. So we never touch them. */
	    *(unsigned long *)data = CT_LE_L((new & 0xfffffff) |
					     (curEntry.reserved << 28));
	    size = 4;
	}
	break;
    default:
	die("Bad FAT entry size: %d bits.", fs->fat_bits);
    }
    fs_write(offs, size, data);
    if (fs->nfats > 1) {
	fs_write(offs + fs->fat_size, size, data);
    }
}

int bad_cluster(DOS_FS * fs, unsigned long cluster)
{
    FAT_ENTRY curEntry;
    get_fat(&curEntry, fs->fat, cluster, fs);

    return FAT_IS_BAD(fs, curEntry.value);
}

/**
 * Get the cluster to which the specified cluster is linked.
 * If the linked cluster is marked bad, abort.
 *
 * @param[in]   fs          Information about the filesystem
 * @param[in]	cluster     Cluster to follow
 *
 * @return  -1              'cluster' is at the end of the chain
 * @return  Other values    Next cluster in this chain
 */
unsigned long next_cluster(DOS_FS * fs, unsigned long cluster)
{
    unsigned long value;
    FAT_ENTRY curEntry;

    get_fat(&curEntry, fs->fat, cluster, fs);

    value = curEntry.value;
    if (FAT_IS_BAD(fs, value))
	die("Internal error: next_cluster on bad cluster");
    return FAT_IS_EOF(fs, value) ? -1 : value;
}

loff_t cluster_start(DOS_FS * fs, unsigned long cluster)
{
    return fs->data_start + ((loff_t) cluster -
			     2) * (unsigned long long)fs->cluster_size;
}

/**
 * Update internal bookkeeping to show that the specified cluster belongs
 * to the specified dentry.
 *
 * @param[in,out]   fs          Information about the filesystem
 * @param[in]	    cluster     Cluster being assigned
 * @param[in]	    owner       Information on dentry that owns this cluster
 *                              (may be NULL)
 */
void set_owner(DOS_FS * fs, unsigned long cluster, DOS_FILE * owner)
{
    if (fs->cluster_owner == NULL)
	die("Internal error: attempt to set owner in non-existent table");

    if (owner && fs->cluster_owner[cluster]
	&& (fs->cluster_owner[cluster] != owner))
	die("Internal error: attempt to change file owner");
    fs->cluster_owner[cluster] = owner;
}

DOS_FILE *get_owner(DOS_FS * fs, unsigned long cluster)
{
    if (fs->cluster_owner == NULL)
	return NULL;
    else
	return fs->cluster_owner[cluster];
}

void fix_bad(DOS_FS * fs)
{
    unsigned long i;

    if (verbose)
	printf("Checking for bad clusters.\n");
    for (i = 2; i < fs->clusters + 2; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);

	if (!get_owner(fs, i) && !FAT_IS_BAD(fs, curEntry.value))
	    if (!fs_test(cluster_start(fs, i), fs->cluster_size)) {
		printf("Cluster %lu is unreadable.\n", i);
		set_fat(fs, i, -2);
	    }
    }
}

void reclaim_free(DOS_FS * fs)
{
    int reclaimed;
    unsigned long i;

    if (verbose)
	printf("Checking for unused clusters.\n");
    reclaimed = 0;
    for (i = 2; i < fs->clusters + 2; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);

	if (!get_owner(fs, i) && curEntry.value &&
	    !FAT_IS_BAD(fs, curEntry.value)) {
	    set_fat(fs, i, 0);
	    reclaimed++;
	}
    }
    if (reclaimed)
	printf("Reclaimed %d unused cluster%s (%llu bytes).\n", reclaimed,
	       reclaimed == 1 ? "" : "s",
	       (unsigned long long)reclaimed * fs->cluster_size);
}

/**
 * Assign the specified owner to all orphan chains (except cycles).
 * Break cross-links between orphan chains.
 *
 * @param[in,out]   fs             Information about the filesystem
 * @param[in]	    owner          dentry to be assigned ownership of orphans
 * @param[in,out]   num_refs	   For each orphan cluster [index], how many
 *				   clusters link to it.
 * @param[in]	    start_cluster  Where to start scanning for orphans
 */
static void tag_free(DOS_FS * fs, DOS_FILE * owner, unsigned long *num_refs,
		     unsigned long start_cluster)
{
    int prev;
    unsigned long i, walk;

    if (start_cluster == 0)
	start_cluster = 2;

    for (i = start_cluster; i < fs->clusters + 2; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);

	/* If the current entry is the head of an un-owned chain... */
	if (curEntry.value && !FAT_IS_BAD(fs, curEntry.value) &&
	    !get_owner(fs, i) && !num_refs[i]) {
	    prev = 0;
	    /* Walk the chain, claiming ownership as we go */
	    for (walk = i; walk != -1; walk = next_cluster(fs, walk)) {
		if (!get_owner(fs, walk)) {
		    set_owner(fs, walk, owner);
		} else {
		    /* We've run into cross-links between orphaned chains,
		     * or a cycle with a tail.
		     * Terminate this orphan chain (break the link)
		     */
		    set_fat(fs, prev, -1);

		    /* This is not necessary because 'walk' is owned and thus
		     * will never become the head of a chain (the only case
		     * that would matter during reclaim to files).
		     * It's easier to decrement than to prove that it's
		     * unnecessary.
		     */
		    num_refs[walk]--;
		    break;
		}
		prev = walk;
	    }
	}
    }
}

/**
 * Recover orphan chains to files, handling any cycles or cross-links.
 *
 * @param[in,out]   fs             Information about the filesystem
 */
void reclaim_file(DOS_FS * fs)
{
    DOS_FILE orphan;
    int reclaimed, files;
    int changed = 0;
    unsigned long i, next, walk;
    unsigned long *num_refs = NULL;	/* Only for orphaned clusters */
    unsigned long total_num_clusters;

    if (verbose)
	printf("Reclaiming unconnected clusters.\n");

    total_num_clusters = fs->clusters + 2UL;
    num_refs = alloc(total_num_clusters * sizeof(unsigned long));
    memset(num_refs, 0, (total_num_clusters * sizeof(unsigned long)));

    /* Guarantee that all orphan chains (except cycles) end cleanly
     * with an end-of-chain mark.
     */

    for (i = 2; i < total_num_clusters; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);

	next = curEntry.value;
	if (!get_owner(fs, i) && next && next < fs->clusters + 2) {
	    /* Cluster is linked, but not owned (orphan) */
	    FAT_ENTRY nextEntry;
	    get_fat(&nextEntry, fs->fat, next, fs);

	    /* Mark it end-of-chain if it links into an owned cluster,
	     * a free cluster, or a bad cluster.
	     */
	    if (get_owner(fs, next) || !nextEntry.value ||
		FAT_IS_BAD(fs, nextEntry.value))
		set_fat(fs, i, -1);
	    else
		num_refs[next]++;
	}
    }

    /* Scan until all the orphans are accounted for,
     * and all cycles and cross-links are broken
     */
    do {
	tag_free(fs, &orphan, num_refs, changed);
	changed = 0;

	/* Any unaccounted-for orphans must be part of a cycle */
	for (i = 2; i < total_num_clusters; i++) {
	    FAT_ENTRY curEntry;
	    get_fat(&curEntry, fs->fat, i, fs);

	    if (curEntry.value && !FAT_IS_BAD(fs, curEntry.value) &&
		!get_owner(fs, i)) {
		if (!num_refs[curEntry.value]--)
		    die("Internal error: num_refs going below zero");
		set_fat(fs, i, -1);
		changed = curEntry.value;
		printf("Broke cycle at cluster %lu in free chain.\n", i);

		/* If we've created a new chain head,
		 * tag_free() can claim it
		 */
		if (num_refs[curEntry.value] == 0)
		    break;
	    }
	}
    }
    while (changed);

    /* Now we can start recovery */
    files = reclaimed = 0;
    for (i = 2; i < total_num_clusters; i++)
	/* If this cluster is the head of an orphan chain... */
	if (get_owner(fs, i) == &orphan && !num_refs[i]) {
	    DIR_ENT de;
	    loff_t offset;
	    files++;
	    offset = alloc_rootdir_entry(fs, &de, "FSCK%04d");
	    de.start = CT_LE_W(i & 0xffff);
	    if (fs->fat_bits == 32)
		de.starthi = CT_LE_W(i >> 16);
	    for (walk = i; walk > 0 && walk != -1;
		 walk = next_cluster(fs, walk)) {
		de.size = CT_LE_L(CF_LE_L(de.size) + fs->cluster_size);
		reclaimed++;
	    }
	    fs_write(offset, sizeof(DIR_ENT), &de);
	}
    if (reclaimed)
	printf("Reclaimed %d unused cluster%s (%llu bytes) in %d chain%s.\n",
	       reclaimed, reclaimed == 1 ? "" : "s",
	       (unsigned long long)reclaimed * fs->cluster_size, files,
	       files == 1 ? "" : "s");

    free(num_refs);
}

unsigned long update_free(DOS_FS * fs)
{
    unsigned long i;
    unsigned long free = 0;
    int do_set = 0;

    for (i = 2; i < fs->clusters + 2; i++) {
	FAT_ENTRY curEntry;
	get_fat(&curEntry, fs->fat, i, fs);

	if (!get_owner(fs, i) && !FAT_IS_BAD(fs, curEntry.value))
	    ++free;
    }

    if (!fs->fsinfo_start)
	return free;

    if (verbose)
	printf("Checking free cluster summary.\n");
    if (fs->free_clusters != 0xFFFFFFFF) {
	if (free != fs->free_clusters) {
	    printf("Free cluster summary wrong (%ld vs. really %ld)\n",
		   fs->free_clusters, free);
	    if (interactive)
		printf("1) Correct\n2) Don't correct\n");
	    else
		printf("  Auto-correcting.\n");
	    if (!interactive || get_key("12", "?") == '1')
		do_set = 1;
	}
    } else {
	printf("Free cluster summary uninitialized (should be %ld)\n", free);
	if (rw) {
	    if (interactive)
		printf("1) Set it\n2) Leave it uninitialized\n");
	    else
		printf("  Auto-setting.\n");
	    if (!interactive || get_key("12", "?") == '1')
		do_set = 1;
	}
    }

    if (do_set) {
	unsigned long le_free = CT_LE_L(free);
	fs->free_clusters = free;
	fs_write(fs->fsinfo_start + offsetof(struct info_sector, free_clusters),
		 sizeof(le_free), &le_free);
    }

    return free;
}
