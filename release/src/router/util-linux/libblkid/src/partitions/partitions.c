/*
 * partitions - partition tables parsing
 *
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>

#include "partitions.h"
#include "sysfs.h"

/**
 * SECTION: partitions
 * @title: Partitions probing
 * @short_description: partitions tables detection and parsing
 *
 * This chain supports binary and NAME=value interfaces, but complete PT
 * description is provided by binary interface only. The libblkid prober is
 * compatible with kernel partition tables parser. The parser does not return
 * empty (size=0) partitions or special hidden partitions.
 *
 * NAME=value interface, supported tags:
 *
 * @PTTYPE: partition table type (dos, gpt, etc.).
 *
 * @PART_ENTRY_SCHEME: partition table type
 *
 * @PART_ENTRY_NAME: partition name (gpt and mac only)
 *
 * @PART_ENTRY_UUID: partition UUID (gpt only)
 *
 * @PART_ENTRY_TYPE: partition type, 0xNN (e.g 0x82) or type UUID (gpt only) or type string (mac)
 *
 * @PART_ENTRY_FLAGS: partition flags (e.g. boot_ind) or  attributes (e.g. gpt attributes)
 *
 * @PART_ENTRY_NUMBER: partition number
 *
 * @PART_ENTRY_OFFSET: the begin of the partition
 *
 * @PART_ENTRY_SIZE: size of the partition
 *
 * @PART_ENTRY_DISK: whole-disk maj:min
 *
 * Example:
 *
 * <informalexample>
 *  <programlisting>
 * blkid_probe pr;
 * const char *ptname;
 *
 * pr = blkid_new_probe_from_filename(devname);
 * if (!pr)
 *	err("%s: faild to open device", devname);
 *
 * blkid_probe_enable_partitions(pr, TRUE);
 * blkid_do_fullprobe(pr);
 *
 * blkid_probe_lookup_value(pr, "PTTYPE", &ptname, NULL);
 * printf("%s partition type detected\n", pttype);
 *
 * blkid_free_probe(pr);
 *
 * // don't forget to check return codes in your code!
 *  </programlisting>
 * </informalexample>
 *
 * Binary interface:
 *
 * <informalexample>
 *  <programlisting>
 * blkid_probe pr;
 * blkid_partlist ls;
 * int nparts, i;
 *
 * pr = blkid_new_probe_from_filename(devname);
 * if (!pr)
 *	err("%s: faild to open device", devname);
 *
 * ls = blkid_probe_get_partitions(pr);
 * nparts = blkid_partlist_numof_partitions(ls);
 *
 * for (i = 0; i < nparts; i++) {
 *      blkid_partition par = blkid_partlist_get_partition(ls, i);
 *      printf("#%d: %llu %llu  0x%x",
 *               blkid_partition_get_partno(par),
 *               blkid_partition_get_start(par),
 *               blkid_partition_get_size(par),
 *               blkid_partition_get_type(par));
 * }
 *
 * blkid_free_probe(pr);
 *
 * // don't forget to check return codes in your code!
 *  </programlisting>
 * </informalexample>
 */

/*
 * Chain driver function
 */
static int partitions_probe(blkid_probe pr, struct blkid_chain *chn);
static void partitions_free_data(blkid_probe pr, void *data);

/*
 * Partitions chain probing functions
 */
static const struct blkid_idinfo *idinfos[] =
{
	&aix_pt_idinfo,
	&sgi_pt_idinfo,
	&sun_pt_idinfo,
	&dos_pt_idinfo,
	&gpt_pt_idinfo,
	&mac_pt_idinfo,
	&ultrix_pt_idinfo,
	&bsd_pt_idinfo,
	&unixware_pt_idinfo,
	&solaris_x86_pt_idinfo,
	&minix_pt_idinfo
};

/*
 * Driver definition
 */
const struct blkid_chaindrv partitions_drv = {
	.id           = BLKID_CHAIN_PARTS,
	.name         = "partitions",
	.dflt_enabled = FALSE,
	.idinfos      = idinfos,
	.nidinfos     = ARRAY_SIZE(idinfos),
	.has_fltr     = TRUE,
	.probe        = partitions_probe,
	.safeprobe    = partitions_probe,
	.free_data    = partitions_free_data
};


/*
 * For compatibility with the rest of libblkid API (with the old high-level
 * API) we use completely opaque typedefs for all structs. Don't forget that
 * the final blkid_* types are pointers! See blkid.h.
 *
 * [Just for the record, I hate typedef for pointers --kzak]
 */

/* exported as opaque type "blkid_parttable" */
struct blkid_struct_parttable {
	const char	*type;		/* partition table type */
	blkid_loff_t	offset;		/* begin of the partition table */
	int		nparts;		/* number of partitions */
	blkid_partition	parent;		/* parent of nested partition table */

	struct list_head t_tabs;	/* all tables */
};

/* exported as opaque type "blkid_partition" */
struct blkid_struct_partition {
	blkid_loff_t	start;		/* begin of the partition */
	blkid_loff_t	size;		/* size of the partitions */

	int		type;		/* partition type */
	char		typestr[37];	/* partition type string (GPT and Mac) */

	unsigned long long flags;	/* partition flags / attributes */

	int		partno;		/* partition number */
	char		uuid[37];	/* UUID (when supported by PT), e.g GPT */
	unsigned char	name[128];	/* Partition in UTF8 name (when supporte by PT), e.g. Mac */

	blkid_parttable	tab;		/* partition table */
};

/* exported as opaque type "blkid_partlist" */
struct blkid_struct_partlist {
	int		next_partno;	/* next partition number */
	blkid_partition next_parent;	/* next parent if parsing nested PT */

	int		nparts;		/* number of partitions */
	int		nparts_max;	/* max.number of partitions */
	blkid_partition	parts;		/* array of partitions */

	struct list_head l_tabs;	/* list of partition tables */
};

static int blkid_partitions_probe_partition(blkid_probe pr);

/**
 * blkid_probe_enable_partitions:
 * @pr: probe
 * @enable: TRUE/FALSE
 *
 * Enables/disables the partitions probing for non-binary interface.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_enable_partitions(blkid_probe pr, int enable)
{
	if (!pr)
		return -1;
	pr->chains[BLKID_CHAIN_PARTS].enabled = enable;
	return 0;
}

/**
 * blkid_probe_set_partitions_flags:
 * @pr: prober
 * @flags: BLKID_PARTS_* flags
 *
 * Sets probing flags to the partitions prober. This function is optional.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_set_partitions_flags(blkid_probe pr, int flags)
{
	if (!pr)
		return -1;

	pr->chains[BLKID_CHAIN_PARTS].flags = flags;
	return 0;
}

/**
 * blkid_probe_reset_partitions_filter:
 * @pr: prober
 *
 * Resets partitions probing filter
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_reset_partitions_filter(blkid_probe pr)
{
	return __blkid_probe_reset_filter(pr, BLKID_CHAIN_PARTS);
}

/**
 * blkid_probe_invert_partitions_filter:
 * @pr: prober
 *
 * Inverts partitions probing filter
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_invert_partitions_filter(blkid_probe pr)
{
	return __blkid_probe_invert_filter(pr, BLKID_CHAIN_PARTS);
}

/**
 * blkid_probe_filter_partitions_type:
 * @pr: prober
 * @flag: filter BLKID_FLTR_{NOTIN,ONLYIN} flag
 * @names: NULL terminated array of probing function names (e.g. "vfat").
 *
 *  %BLKID_FLTR_NOTIN  - probe for all items which are NOT IN @names
 *
 *  %BLKID_FLTR_ONLYIN - probe for items which are IN @names
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_filter_partitions_type(blkid_probe pr, int flag, char *names[])
{
	return __blkid_probe_filter_types(pr, BLKID_CHAIN_PARTS, flag, names);
}

/**
 * blkid_probe_get_partitions:
 * @pr: probe
 *
 * This is a binary interface for partitions. See also blkid_partlist_*
 * functions.
 *
 * This function is independent on blkid_do_[safe,full]probe() and
 * blkid_probe_enable_partitions() calls.
 *
 * WARNING: the returned object will be overwritten by the next
 *          blkid_probe_get_partitions() call for the same @pr. If you want to
 *          use more blkid_partlist objects in the same time you have to create
 *          more blkid_probe handlers (see blkid_new_probe()).
 *
 * Returns: list of partitions, or NULL in case of error.
 */
blkid_partlist blkid_probe_get_partitions(blkid_probe pr)
{
	return (blkid_partlist) blkid_probe_get_binary_data(pr,
			&pr->chains[BLKID_CHAIN_PARTS]);
}

/* for internal usage only */
blkid_partlist blkid_probe_get_partlist(blkid_probe pr)
{
	return (blkid_partlist) pr->chains[BLKID_CHAIN_PARTS].data;
}

static void blkid_probe_set_partlist(blkid_probe pr, blkid_partlist ls)
{
	pr->chains[BLKID_CHAIN_PARTS].data = ls;
}

static void ref_parttable(blkid_parttable tab)
{
	tab->nparts++;
}

static void unref_parttable(blkid_parttable tab)
{
	tab->nparts--;

	if (tab->nparts <= 0) {
		list_del(&tab->t_tabs);
		free(tab);
	}
}

/* free all allocated parttables */
static void free_parttables(blkid_partlist ls)
{
	if (!ls || !ls->l_tabs.next)
		return;

	/* remove unassigned partition tables */
	while (!list_empty(&ls->l_tabs)) {
		blkid_parttable tab = list_entry(ls->l_tabs.next,
					struct blkid_struct_parttable, t_tabs);
		unref_parttable(tab);
	}
}

static void reset_partlist(blkid_partlist ls)
{
	if (!ls)
		return;

	free_parttables(ls);

	if (ls->next_partno) {
		/* already initialized - reset */
		int tmp_nparts = ls->nparts_max;
		blkid_partition tmp_parts = ls->parts;

		memset(ls, 0, sizeof(struct blkid_struct_partlist));

		ls->nparts_max = tmp_nparts;
		ls->parts = tmp_parts;
	}

	ls->nparts = 0;
	ls->next_partno = 1;
	INIT_LIST_HEAD(&ls->l_tabs);

	DBG(DEBUG_LOWPROBE, printf("partlist reseted\n"));
}

static blkid_partlist partitions_init_data(struct blkid_chain *chn)
{
	blkid_partlist ls;

	if (chn->data)
		ls = (blkid_partlist) chn->data;
	else {
		/* allocate the new list of partitions */
		ls = calloc(1, sizeof(struct blkid_struct_partlist));
		if (!ls)
			return NULL;
		chn->data = (void *) ls;
	}

	reset_partlist(ls);

	DBG(DEBUG_LOWPROBE,
		printf("parts: initialized partitions list (%p, size=%d)\n",
		ls, ls->nparts_max));
	return ls;
}

static void partitions_free_data(blkid_probe pr __attribute__((__unused__)),
				 void *data)
{
	blkid_partlist ls = (blkid_partlist) data;

	if (!ls)
		return;

	free_parttables(ls);

	/* deallocate partitions and partlist */
	free(ls->parts);
	free(ls);
}

blkid_parttable blkid_partlist_new_parttable(blkid_partlist ls,
				const char *type, blkid_loff_t offset)
{
	blkid_parttable tab;

	tab = calloc(1, sizeof(struct blkid_struct_parttable));
	if (!tab)
		return NULL;
	tab->type = type;
	tab->offset = offset;
	tab->parent = ls->next_parent;

	INIT_LIST_HEAD(&tab->t_tabs);
	list_add_tail(&tab->t_tabs, &ls->l_tabs);

	DBG(DEBUG_LOWPROBE,
		printf("parts: create a new partition table "
		       "(%p, type=%s, offset=%"PRId64")\n", tab, type, offset));
	return tab;
}

static blkid_partition new_partition(blkid_partlist ls, blkid_parttable tab)
{
	blkid_partition par;

	if (ls->nparts + 1 > ls->nparts_max) {
		/* Linux kernel has DISK_MAX_PARTS=256, but it's too much for
		 * generic Linux machine -- let start with 32 partititions.
		 */
		ls->parts = realloc(ls->parts, (ls->nparts_max + 32) *
					sizeof(struct blkid_struct_partition));
		if (!ls->parts)
			return NULL;
		ls->nparts_max += 32;
	}

	par = &ls->parts[ls->nparts++];
	memset(par, 0, sizeof(struct blkid_struct_partition));

	ref_parttable(tab);
	par->tab = tab;
	par->partno = blkid_partlist_increment_partno(ls);

	return par;
}

blkid_partition blkid_partlist_add_partition(blkid_partlist ls,
					blkid_parttable tab,
					blkid_loff_t start, blkid_loff_t size)
{
	blkid_partition par = new_partition(ls, tab);

	if (!par)
		return NULL;

	par->start = start;
	par->size = size;

	DBG(DEBUG_LOWPROBE,
		printf("parts: add partition (%p start=%"
		PRId64 ", size=%" PRId64 ", table=%p)\n",
		par, par->start, par->size, tab));
	return par;
}

/* allows to modify used partitions numbers (for example for logical partitions) */
int blkid_partlist_set_partno(blkid_partlist ls, int partno)
{
	if (!ls)
		return -1;
	ls->next_partno = partno;
	return 0;
}

int blkid_partlist_increment_partno(blkid_partlist ls)
{
	return ls ? ls->next_partno++ : -1;
}

/* allows to set "parent" for the next nested partition */
int blkid_partlist_set_parent(blkid_partlist ls, blkid_partition par)
{
	if (!ls)
		return -1;
	ls->next_parent = par;
	return 0;
}

blkid_partition blkid_partlist_get_parent(blkid_partlist ls)
{
	if (!ls)
		return NULL;
	return ls->next_parent;
}

int blkid_partitions_need_typeonly(blkid_probe pr)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	return chn && chn->data && chn->binary ? FALSE : TRUE;
}

/* get private chain flags */
int blkid_partitions_get_flags(blkid_probe pr)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	return chn ? chn->flags : 0;
}

/* check if @start and @size are within @par partition */
int blkid_is_nested_dimension(blkid_partition par,
			blkid_loff_t start, blkid_loff_t size)
{
	blkid_loff_t pstart;
	blkid_loff_t psize;

	if (!par)
		return 0;

	pstart = blkid_partition_get_start(par);
	psize = blkid_partition_get_size(par);

	if (start < pstart || start + size > pstart + psize)
		return 0;

	return 1;
}

static int idinfo_probe(blkid_probe pr, const struct blkid_idinfo *id)
{
	const struct blkid_idmag *mag;
	int rc = 1;		/* = nothing detected */

	if (pr->size <= 0 || (id->minsz && id->minsz > pr->size))
		goto nothing;	/* the device is too small */

	if (blkid_probe_get_idmag(pr, id, NULL, &mag))
		goto nothing;

	/* final check by probing function */
	if (id->probefunc) {
		DBG(DEBUG_LOWPROBE, printf(
			"%s: ---> call probefunc()\n", id->name));
		rc = id->probefunc(pr, mag);
	        if (rc == -1) {
			/* reset after error */
			reset_partlist(blkid_probe_get_partlist(pr));
			DBG(DEBUG_LOWPROBE, printf(
				"%s probefunc failed\n", id->name));
		}
		DBG(DEBUG_LOWPROBE, printf(
			"%s: <--- (rc = %d)\n", id->name, rc));
	}

nothing:
	return rc;
}

/*
 * The blkid_do_probe() backend.
 */
static int partitions_probe(blkid_probe pr, struct blkid_chain *chn)
{
	int rc = 1;
	size_t i;

	if (!pr || chn->idx < -1)
		return -1;
	blkid_probe_chain_reset_vals(pr, chn);

	if (chn->binary)
		partitions_init_data(chn);

	if (!pr->wipe_size && (pr->prob_flags & BLKID_PROBE_FL_IGNORE_PT))
		goto details_only;

	DBG(DEBUG_LOWPROBE,
		printf("--> starting probing loop [PARTS idx=%d]\n",
		chn->idx));

	i = chn->idx < 0 ? 0 : chn->idx + 1U;

	for ( ; i < ARRAY_SIZE(idinfos); i++) {
		const char *name;

		chn->idx = i;

		/* apply filter */
		if (chn->fltr && blkid_bmp_get_item(chn->fltr, i))
			continue;

		/* apply checks from idinfo */
		if (idinfo_probe(pr, idinfos[i]) != 0)
			continue;

		name = idinfos[i]->name;

		/* all checks passed */
		if (!chn->binary)
			blkid_probe_set_value(pr, "PTTYPE",
						(unsigned char *) name,
						strlen(name) + 1);
		DBG(DEBUG_LOWPROBE,
			printf("<-- leaving probing loop (type=%s) [PARTS idx=%d]\n",
			name, chn->idx));
		rc = 0;
		break;
	}

	if (rc == 1) {
		DBG(DEBUG_LOWPROBE,
			printf("<-- leaving probing loop (failed) [PARTS idx=%d]\n",
			chn->idx));
	}

details_only:
	/*
	 * Gather PART_ENTRY_* values if the current device is a partition.
	 */
	if (!chn->binary &&
	    (blkid_partitions_get_flags(pr) & BLKID_PARTS_ENTRY_DETAILS)) {

		if (!blkid_partitions_probe_partition(pr))
			rc = 0;
	}

	return rc;
}

/* Probe for nested partition table within the parental partition */
int blkid_partitions_do_subprobe(blkid_probe pr, blkid_partition parent,
		const struct blkid_idinfo *id)
{
	blkid_probe prc;
	int rc = 1;
	blkid_partlist ls;
	blkid_loff_t sz, off;

	DBG(DEBUG_LOWPROBE, printf(
		"parts: ----> %s subprobe requested (parent=%p)\n",
		id->name, parent));

	if (!pr || !parent || !parent->size)
		return -1;

	/* range defined by parent */
	sz = ((blkid_loff_t) parent->size) << 9;
	off = ((blkid_loff_t) parent->start) << 9;

	if (off < pr->off || pr->off + pr->size < off + sz) {
		DBG(DEBUG_LOWPROBE, printf(
			"ERROR: parts: <---- '%s' subprobe: overflow detected.\n",
			id->name));
		return -1;
	}

	/* create private prober */
	prc = blkid_clone_probe(pr);
	if (!prc)
		return -1;

	blkid_probe_set_dimension(prc, off, sz);

	/* clone is always with reseted chain, fix it */
	prc->cur_chain = blkid_probe_get_chain(pr);

	/*
	 * Set 'parent' to the current list of the partitions and use the list
	 * in cloned prober (so the cloned prober will extend the current list
	 * of partitions rather than create a new).
	 */
	ls = blkid_probe_get_partlist(pr);
	blkid_partlist_set_parent(ls, parent);

	blkid_probe_set_partlist(prc, ls);

	rc = idinfo_probe(prc, id);

	blkid_probe_set_partlist(prc, NULL);
	blkid_partlist_set_parent(ls, NULL);

	blkid_free_probe(prc);	/* free cloned prober */

	DBG(DEBUG_LOWPROBE, printf(
		"parts: <---- %s subprobe done (parent=%p, rc=%d)\n",
		id->name, parent, rc));

	return rc;
}

static int blkid_partitions_probe_partition(blkid_probe pr)
{
	int rc = 1;
	blkid_probe disk_pr = NULL;
	blkid_partlist ls;
	blkid_partition par;
	dev_t devno;

	devno = blkid_probe_get_devno(pr);
	if (!devno)
		goto nothing;

	disk_pr = blkid_probe_get_wholedisk_probe(pr);
	if (!disk_pr)
		goto nothing;

	/* parse PT */
	ls = blkid_probe_get_partitions(disk_pr);
	if (!ls)
		goto nothing;

	par = blkid_partlist_devno_to_partition(ls, devno);
	if (par) {
		const char *v;
		blkid_parttable tab = blkid_partition_get_table(par);
		dev_t disk = blkid_probe_get_devno(disk_pr);

		if (tab) {
			v = blkid_parttable_get_type(tab);
			if (v)
				blkid_probe_set_value(pr, "PART_ENTRY_SCHEME",
					(unsigned char *) v, strlen(v) + 1);
		}

		v = blkid_partition_get_name(par);
		if (v)
			blkid_probe_set_value(pr, "PART_ENTRY_NAME",
				(unsigned char *) v, strlen(v) + 1);

		v = blkid_partition_get_uuid(par);
		if (v)
			blkid_probe_set_value(pr, "PART_ENTRY_UUID",
				(unsigned char *) v, strlen(v) + 1);

		/* type */
		v = blkid_partition_get_type_string(par);
		if (v)
			blkid_probe_set_value(pr, "PART_ENTRY_TYPE",
				(unsigned char *) v, strlen(v) + 1);
		else
			blkid_probe_sprintf_value(pr, "PART_ENTRY_TYPE",
				"0x%x", blkid_partition_get_type(par));

		if (blkid_partition_get_flags(par))
			blkid_probe_sprintf_value(pr, "PART_ENTRY_FLAGS",
				"0x%llx", blkid_partition_get_flags(par));

		blkid_probe_sprintf_value(pr, "PART_ENTRY_NUMBER",
				"%d", blkid_partition_get_partno(par));

		blkid_probe_sprintf_value(pr, "PART_ENTRY_OFFSET", "%jd",
				blkid_partition_get_start(par));
		blkid_probe_sprintf_value(pr, "PART_ENTRY_SIZE", "%jd",
				blkid_partition_get_size(par));

		blkid_probe_sprintf_value(pr, "PART_ENTRY_DISK", "%u:%u",
				major(disk), minor(disk));
	}
	rc = 0;
nothing:
	return rc;
}

/*
 * Returns 1 if the device is whole-disk and the area specified by @offset and
 * @size is covered by any partition.
 */
int blkid_probe_is_covered_by_pt(blkid_probe pr,
				 blkid_loff_t offset, blkid_loff_t size)
{
	blkid_probe prc;
	blkid_partlist ls = NULL;
	blkid_loff_t start, end;
	int nparts, i, rc = 0;

	DBG(DEBUG_LOWPROBE, printf(
		"=> checking if off=%jd size=%jd covered by PT\n",
		offset, size));

	prc = blkid_clone_probe(pr);
	if (!prc)
		goto done;

	ls = blkid_probe_get_partitions(prc);
	if (!ls)
		goto done;

	nparts = blkid_partlist_numof_partitions(ls);
	if (!nparts)
		goto done;

	end = (offset + size) >> 9;
	start = offset >> 9;

	/* check if the partition table fits into the device */
	for (i = 0; i < nparts; i++) {
		blkid_partition par = &ls->parts[i];

		if (par->start + par->size > (pr->size >> 9)) {
			DBG(DEBUG_LOWPROBE, printf("partition #%d overflows "
				"device (off=%" PRId64 " size=%" PRId64 ")\n",
				par->partno, par->start, par->size));
			goto done;
		}
	}

	/* check if the requested area is covered by PT */
	for (i = 0; i < nparts; i++) {
		blkid_partition par = &ls->parts[i];

		if (start >= par->start && end <= par->start + par->size) {
			rc = 1;
			break;
		}
	}
done:
	blkid_free_probe(prc);

	DBG(DEBUG_LOWPROBE, printf("<= %s covered by PT\n", rc ? "IS" : "NOT"));
	return rc;
}

/**
 * blkid_known_pttype:
 * @pttype: partiton name
 *
 * Returns: 1 for known or 0 for unknown partition type.
 */
int blkid_known_pttype(const char *pttype)
{
	size_t i;

	if (!pttype)
		return 0;

	for (i = 0; i < ARRAY_SIZE(idinfos); i++) {
		const struct blkid_idinfo *id = idinfos[i];
		if (strcmp(id->name, pttype) == 0)
			return 1;
	}
	return 0;
}

/**
 * blkid_partlist_numof_partitions:
 * @ls: partitions list
 *
 * Returns: number of partitions in the list or -1 in case of error.
 */
int blkid_partlist_numof_partitions(blkid_partlist ls)
{
	return ls ? ls->nparts : -1;
}

/**
 * blkid_partlist_get_table:
 * @ls: partitions list
 *
 * Returns: top-level partition table or NULL of there is not a partition table
 * on the device.
 */
blkid_parttable blkid_partlist_get_table(blkid_partlist ls)
{
	if (!ls || list_empty(&ls->l_tabs))
		return NULL;

	return list_entry(ls->l_tabs.next,
			struct blkid_struct_parttable, t_tabs);
}


/**
 * blkid_partlist_get_partition:
 * @ls: partitions list
 * @n: partition number in range 0..N, where 'N' is blkid_partlist_numof_partitions().
 *
 * It's possible that the list of partitions is *empty*, but there is a valid
 * partition table on the disk. This happen when on-disk details about
 * partitions are unknown or the partition table is empty.
 *
 * See also blkid_partlist_get_table().
 *
 * Returns: partition object or NULL in case or error.
 */
blkid_partition blkid_partlist_get_partition(blkid_partlist ls, int n)
{
	if (!ls || n < 0 || n >= ls->nparts)
		return NULL;

	return &ls->parts[n];
}

/**
 * blkid_partlist_devno_to_partition:
 * @ls: partitions list
 * @devno: requested partition
 *
 * This function tries to get start and size for @devno from sysfs and
 * returns a partition from @ls which matches with the values from sysfs.
 *
 * This funtion is necessary when you want to make a relation between an entry
 * in the partition table (@ls) and block devices in your system.
 *
 * Returns: partition object or NULL in case or error.
 */
blkid_partition blkid_partlist_devno_to_partition(blkid_partlist ls, dev_t devno)
{
	struct sysfs_cxt sysfs;
	uint64_t start, size;
	int i, rc, partno = 0;

	DBG(DEBUG_LOWPROBE,
		printf("triyng to convert devno 0x%llx to partition\n",
			(long long) devno));

	if (sysfs_init(&sysfs, devno, NULL)) {
		DBG(DEBUG_LOWPROBE, printf("failed t init sysfs context\n"));
		return NULL;
	}
	rc = sysfs_read_u64(&sysfs, "size", &size);
	if (!rc) {
		rc = sysfs_read_u64(&sysfs, "start", &start);
		if (rc) {
			/* try to get partition number from DM uuid.
			 */
			char *uuid = sysfs_strdup(&sysfs, "dm/uuid");
			char *tmp = uuid;
			char *prefix = uuid ? strsep(&tmp, "-") : NULL;

			if (prefix && strncasecmp(prefix, "part", 4) == 0) {
				char *end = NULL;

				partno = strtol(prefix + 4, &end, 10);
				if (prefix == end || (end && *end))
					partno = 0;
				else
					rc = 0;		/* success */
			}
			free(uuid);
		}
	}

	sysfs_deinit(&sysfs);

	if (rc)
		return NULL;

	if (partno) {
		DBG(DEBUG_LOWPROBE, printf("mapped by DM, using partno %d\n", partno));

		/*
		 * Partition mapped by kpartx does not provide "start" offset
		 * in /sys, but if we know partno and size of the partition
		 * that we can probably make the releation bettween the device
		 * and an entry in partition table.
		 */
		 for (i = 0; i < ls->nparts; i++) {
			 blkid_partition par = &ls->parts[i];

			 if (partno != blkid_partition_get_partno(par))
				 continue;

			 if ((blkid_loff_t) size == blkid_partition_get_size(par) ||
			     (blkid_partition_is_extended(par) && size <= 1024))
				 return par;

		 }
		 return NULL;
	}

	DBG(DEBUG_LOWPROBE, printf("searching by offset/size\n"));

	for (i = 0; i < ls->nparts; i++) {
		blkid_partition par = &ls->parts[i];

		if (blkid_partition_get_start(par) == (blkid_loff_t) start &&
		    blkid_partition_get_size(par) == (blkid_loff_t) size)
			return par;

		/* exception for extended dos partitions */
		if (blkid_partition_get_start(par) == (blkid_loff_t) start &&
		    blkid_partition_is_extended(par) && size <= 1024)
			return par;

	}

	DBG(DEBUG_LOWPROBE, printf("not found partition for device\n"));
	return NULL;
}

int blkid_partition_set_type(blkid_partition par, int type)
{
	if (!par)
		return -1;
	par->type = type;
	return 0;
}

/**
 * blkid_parttable_get_type:
 * @tab: partition table
 *
 * Returns: partition table type (type name, e.g. "dos", "gpt", ...)
 */
const char *blkid_parttable_get_type(blkid_parttable tab)
{
	return tab ? tab->type : NULL;
}

/**
 * blkid_parttable_get_parent:
 * @tab: partition table
 *
 * Returns: parent for nexted partitition tables or NULL.
 */
blkid_partition blkid_parttable_get_parent(blkid_parttable tab)
{
	return tab ? tab->parent : NULL;
}

/**
 * blkid_parttable_get_offset:
 * @tab: partition table
 *
 * Note the position is relative to begin of the device as defined by
 * blkid_probe_set_device() for primary partition table, and relative
 * to parental partition for nested patition tables.
 *
 * <informalexample>
 *   <programlisting>
 * off_t offset;
 * blkid_partition parent = blkid_parttable_get_parent(tab);
 *
 * offset = blkid_parttable_get_offset(tab);
 *
 * if (parent)
 *      / * 'tab' is nested partition table * /
 *	offset += blkid_partition_get_start(parent);
 *   </programlisting>
 * </informalexample>

 * Returns: position (in bytes) of the partition table or -1 in case of error.
 *
 */
blkid_loff_t blkid_parttable_get_offset(blkid_parttable tab)
{
	return tab ? tab->offset : -1;
}

/**
 * blkid_partition_get_table:
 * @par: partition
 *
 * The "parttable" describes partition table. The table is usually the same for
 * all partitions -- except nested partition tables.
 *
 * For example bsd, solaris, etc. use a nested partition table within
 * standard primary dos partition:
 *
 * <informalexample>
 *   <programlisting>
 *
 *  -- dos partition table
 *  0: sda1     dos primary partition
 *  1: sda2     dos primary partition
 *     -- bsd partition table (with in sda2)
 *  2:    sda5  bds partition
 *  3:    sda6  bds partition
 *
 *   </programlisting>
 * </informalexample>
 *
 * The library does not to use a separate partition table object for dos logical
 * partitions (partitions within extended partition). It's possible to
 * differentiate between logical, extended and primary partitions by
 *
 *	blkid_partition_is_{extended,primary,logical}().
 *
 * Returns: partition table object or NULL in case of error.
 */
blkid_parttable blkid_partition_get_table(blkid_partition par)
{
	return par ? par->tab : NULL;
}

static int partition_get_logical_type(blkid_partition par)
{
	blkid_parttable tab;

	if (!par)
		return -1;

	tab = blkid_partition_get_table(par);
	if (!tab || !tab->type)
		return -1;

	if (tab->parent)
		return 'L';  /* report nested partitions as logical */

	if (!strcmp(tab->type, "dos")) {
		if (par->partno > 4)
			return 'L';	/* logical */

	        if(par->type == BLKID_DOS_EXTENDED_PARTITION ||
                   par->type == BLKID_W95_EXTENDED_PARTITION ||
		   par->type == BLKID_LINUX_EXTENDED_PARTITION)
			return 'E';
	}
	return 'P';
}

/**
 * blkid_partition_is_primary:
 * @par: partition
 *
 * Note, this function returns FALSE for DOS extended partitions and
 * all partitions in nested partition tables.
 *
 * Returns: 1 if the partitions is primary partition or 0 if not.
 */
int blkid_partition_is_primary(blkid_partition par)
{
	return partition_get_logical_type(par) == 'P' ? TRUE : FALSE;
}

/**
 * blkid_partition_is_extended:
 * @par: partition
 *
 * Returns: 1 if the partitions is extended (dos, windows or linux)
 * partition or 0 if not.
 */
int blkid_partition_is_extended(blkid_partition par)
{
	return partition_get_logical_type(par) == 'E' ? TRUE : FALSE;
}

/**
 * blkid_partition_is_logical:
 * @par: partition
 *
 * Note that this function returns TRUE for all partitions in all
 * nested partition tables (e.g. BSD labels).
 *
 * Returns: 1 if the partitions is logical partition or 0 if not.
 */
int blkid_partition_is_logical(blkid_partition par)
{
	return partition_get_logical_type(par) == 'L' ? TRUE : FALSE;
}

static void set_string(unsigned char *item, size_t max,
				const unsigned char *data, size_t len)
{
	if (len >= max)
		len = max - 1;

	memcpy(item, data, len);
	item[len] = '\0';

	blkid_rtrim_whitespace(item);
}

int blkid_partition_set_name(blkid_partition par,
		const unsigned char *name, size_t len)
{
	if (!par)
		return -1;

	set_string(par->name, sizeof(par->name), name, len);
	return 0;
}

int blkid_partition_set_utf8name(blkid_partition par, const unsigned char *name,
		size_t len, int enc)
{
	if (!par)
		return -1;

	blkid_encode_to_utf8(enc, par->name, sizeof(par->name), name, len);
	blkid_rtrim_whitespace(par->name);
	return 0;
}

int blkid_partition_set_uuid(blkid_partition par, const unsigned char *uuid)
{
	if (!par)
		return -1;

	blkid_unparse_uuid(uuid, par->uuid, sizeof(par->uuid));
	return 0;
}

/**
 * blkid_partition_get_name:
 * @par: partition
 *
 * Returns: partition name string if supported by PT (e.g. Mac) or NULL.
 */
const char *blkid_partition_get_name(blkid_partition par)
{
	return par && *par->name ? (char *) par->name : NULL;
}

/**
 * blkid_partition_get_uuid:
 * @par: partition
 *
 * Returns: partition UUID string if supported by PT (e.g. GPT) or NULL.
 */
const char *blkid_partition_get_uuid(blkid_partition par)
{
	return par && *par->uuid ? par->uuid : NULL;
}

/**
 * blkid_partition_get_partno:
 * @par: partition
 *
 * Returns: proposed partitin number (e.g. 'N' from sda'N') or -1 in case of
 * error. Note that the number is generate by library independenly on your OS.
 */
int blkid_partition_get_partno(blkid_partition par)
{
	return par ? par->partno : -1;
}

/**
 * blkid_partition_get_start:
 * @par: partition
 *
 * Be careful if you _not_ probe whole disk:
 *
 * 1) the offset is usully relative to begin of the disk -- but if you probe a
 *    fragment of the disk only -- then the offset could be still relative to
 *    the begin of the disk rather that relative to the fragment.
 *
 * 2) the offset for nested partitions could be releative to parent (e.g. Solaris)
 *    _or_ relative to the begin of the whole disk (e.g. bsd).
 *
 * You don't have to care about such details if you proble whole disk. In such
 * a case libblkid always returns the offset relative to the begin of the disk.
 *
 * Returns: start of the partition (in 512-sectors).
 */
blkid_loff_t blkid_partition_get_start(blkid_partition par)
{
	return par ? par->start : -1;
}

/**
 * blkid_partition_get_size:
 * @par: partition
 *
 * WARNING: be very careful when you work with MS-DOS extended partitions. The
 *          library always returns full size of the partition. If you want add
 *          the partition to the Linux system (BLKPG_ADD_PARTITION ioctl) you
 *          need to reduce the size of the partition to 1 or 2 blocks. The
 *          rest of the partition has to be unaccessible for mkfs or mkswap
 *          programs, we need a small space for boot loaders only.
 *
 *          For some unknown reason this (safe) practice is not to used for
 *          nested BSD, Solaris, ..., partition tables in Linux kernel.
 *
 * Returns: size of the partition (in 512-sectors).
 */
blkid_loff_t blkid_partition_get_size(blkid_partition par)
{
	return par ? par->size : -1;
}

/**
 * blkid_partition_get_type:
 * @par: partition
 *
 * Returns: partition type.
 */
int blkid_partition_get_type(blkid_partition par)
{
	return par ? par->type : 0;
}

/* Sets partition 'type' for PT where the type is defined by string rather
 * than by number
 */
int blkid_partition_set_type_string(blkid_partition par,
		const unsigned char *type, size_t len)
{
	if (!par)
		return -1;

	set_string((unsigned char *) par->typestr,
			sizeof(par->typestr), type, len);
	return 0;
}

/* Sets partition 'type' for PT where the type is defined by UUIDrather
 * than by number
 */
int blkid_partition_set_type_uuid(blkid_partition par, const unsigned char *uuid)
{
	if (!par)
		return -1;

	blkid_unparse_uuid(uuid, par->typestr, sizeof(par->typestr));
	return 0;
}

/**
 * blkid_partition_get_type_string:
 * @par: partition
 *
 * The type string is supported by a small subset of partition tables (e.g Mac
 * and EFI GPT).  Note that GPT uses type UUID and this function returns this
 * UUID as string.
 *
 * Returns: partition type string or NULL.
 */
const char *blkid_partition_get_type_string(blkid_partition par)
{
	return par && *par->typestr ? par->typestr : NULL;
}


int blkid_partition_set_flags(blkid_partition par, unsigned long long flags)
{
	if (!par)
		return -1;
	par->flags = flags;
	return 0;
}

/**
 * blkid_partition_get_flags
 * @par: partition
 *
 * Returns: partition flags (or attributes for gpt).
 */
unsigned long long blkid_partition_get_flags(blkid_partition par)
{
	return par ? par->flags : 0;
}

