/*
 * Low-level libblkid probing API
 *
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: lowprobe
 * @title: Low-level probing
 * @short_description: low-level prober initialization
 *
 * The low-level probing routines always and directly read information from
 * the selected (see blkid_probe_set_device()) device.
 *
 * The probing routines are grouped together into separate chains. Currently,
 * the library provides superblocks, partitions and topology chains.
 *
 * The probing routines is possible to filter (enable/disable) by type (e.g.
 * fstype "vfat" or partype "gpt") or by usage flags (e.g. BLKID_USAGE_RAID).
 * These filters are per-chain. Note that always when you touch the chain
 * filter the current probing position is reseted and probing starts from
 * scratch.  It means that the chain filter should not be modified during
 * probing, for example in loop where you call blkid_do_probe().
 *
 * For more details see the chain specific documentation.
 *
 * The low-level API provides two ways how access to probing results.
 *
 *   1. The NAME=value (tag) interface. This interface is older and returns all data
 *      as strings. This interface is generic for all chains.
 *
 *   2. The binary interfaces. These interfaces return data in the native formats.
 *      The interface is always specific to the probing chain.
 *
 *  Note that the previous probing result (binary or NAME=value) is always
 *  zeroized when a chain probing function is called. For example
 *
 * <informalexample>
 *   <programlisting>
 *     blkid_probe_enable_partitions(pr, TRUE);
 *     blkid_probe_enable_superblocks(pr, FALSE);
 *
 *     blkid_do_safeprobe(pr);
 *   </programlisting>
 * </informalexample>
 *
 * overwrites the previous probing result for the partitions chain, the superblocks
 * result is not modified.
 */

/**
 * SECTION: lowprobe-tags
 * @title: Low-level tags
 * @short_description: generic NAME=value interface.
 *
 * The probing routines inside the chain are mutually exclusive by default --
 * only few probing routines are marked as "tolerant". The "tolerant" probing
 * routines are used for filesystem which can share the same device with any
 * other filesystem. The blkid_do_safeprobe() checks for the "tolerant" flag.
 *
 * The SUPERBLOCKS chain is enabled by default. The all others chains is
 * necessary to enable by blkid_probe_enable_'CHAINNAME'(). See chains specific
 * documentation.
 *
 * The blkid_do_probe() function returns a result from only one probing
 * routine, and the next call from the next probing routine. It means you need
 * to call the function in loop, for example:
 *
 * <informalexample>
 *   <programlisting>
 *	while((blkid_do_probe(pr) == 0)
 *		... use result ...
 *   </programlisting>
 * </informalexample>
 *
 * The blkid_do_safeprobe() is the same as blkid_do_probe(), but returns only
 * first probing result for every enabled chain. This function checks for
 * ambivalent results (e.g. more "intolerant" filesystems superblocks on the
 * device).
 *
 * The probing result is set of NAME=value pairs (the NAME is always unique).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef HAVE_LIBUUID
# include <uuid.h>
#endif

#include "blkidP.h"

/* chains */
extern const struct blkid_chaindrv superblocks_drv;
extern const struct blkid_chaindrv topology_drv;
extern const struct blkid_chaindrv partitions_drv;

/*
 * All supported chains
 */
static const struct blkid_chaindrv *chains_drvs[] = {
	[BLKID_CHAIN_SUBLKS] = &superblocks_drv,
	[BLKID_CHAIN_TOPLGY] = &topology_drv,
	[BLKID_CHAIN_PARTS] = &partitions_drv
};

static void blkid_probe_reset_vals(blkid_probe pr);
static void blkid_probe_reset_buffer(blkid_probe pr);

/**
 * blkid_new_probe:
 *
 * Returns: a pointer to the newly allocated probe struct.
 */
blkid_probe blkid_new_probe(void)
{
	int i;
	blkid_probe pr;

	blkid_init_debug(0);
	pr = calloc(1, sizeof(struct blkid_struct_probe));
	if (!pr)
		return NULL;

	DBG(DEBUG_LOWPROBE, printf("allocate a new probe %p\n", pr));

	/* initialize chains */
	for (i = 0; i < BLKID_NCHAINS; i++) {
		pr->chains[i].driver = chains_drvs[i];
		pr->chains[i].flags = chains_drvs[i]->dflt_flags;
		pr->chains[i].enabled = chains_drvs[i]->dflt_enabled;
	}
	INIT_LIST_HEAD(&pr->buffers);
	return pr;
}

/*
 * Clone @parent, the new clone shares all, but except:
 *
 *	- probing result
 *	- bufferes if another device (or offset) is set to the prober
 */
blkid_probe blkid_clone_probe(blkid_probe parent)
{
	blkid_probe pr;

	if (!parent)
		return NULL;

	DBG(DEBUG_LOWPROBE, printf("allocate a probe clone\n"));

	pr = blkid_new_probe();
	if (!pr)
		return NULL;

	pr->fd = parent->fd;
	pr->off = parent->off;
	pr->size = parent->size;
	pr->devno = parent->devno;
	pr->disk_devno = parent->disk_devno;
	pr->blkssz = parent->blkssz;
	pr->flags = parent->flags;
	pr->parent = parent;

	return pr;
}



/**
 * blkid_new_probe_from_filename:
 * @filename: device or regular file
 *
 * This function is same as call open(filename), blkid_new_probe() and
 * blkid_probe_set_device(pr, fd, 0, 0).
 *
 * The @filename is closed by blkid_free_probe() or by the
 * blkid_probe_set_device() call.
 *
 * Returns: a pointer to the newly allocated probe struct or NULL in case of
 * error.
 */
blkid_probe blkid_new_probe_from_filename(const char *filename)
{
	int fd = -1;
	blkid_probe pr = NULL;

	if (!filename)
		return NULL;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	pr = blkid_new_probe();
	if (!pr)
		goto err;

	if (blkid_probe_set_device(pr, fd, 0, 0))
		goto err;

	pr->flags |= BLKID_FL_PRIVATE_FD;
	return pr;
err:
	if (fd >= 0)
		close(fd);
	blkid_free_probe(pr);
	return NULL;
}

/**
 * blkid_free_probe:
 * @pr: probe
 *
 * Deallocates the probe struct, buffers and all allocated
 * data that are associated with this probing control struct.
 */
void blkid_free_probe(blkid_probe pr)
{
	int i;

	if (!pr)
		return;

	for (i = 0; i < BLKID_NCHAINS; i++) {
		struct blkid_chain *ch = &pr->chains[i];

		if (ch->driver->free_data)
			ch->driver->free_data(pr, ch->data);
		free(ch->fltr);
	}

	if ((pr->flags & BLKID_FL_PRIVATE_FD) && pr->fd >= 0)
		close(pr->fd);
	blkid_probe_reset_buffer(pr);
	blkid_free_probe(pr->disk_probe);

	DBG(DEBUG_LOWPROBE, printf("free probe %p\n", pr));
	free(pr);
}


/*
 * Removes chain values from probing result.
 */
void blkid_probe_chain_reset_vals(blkid_probe pr, struct blkid_chain *chn)
{
	int nvals = pr->nvals;
	int i, x;

	for (x = 0, i = 0; i < pr->nvals; i++) {
		struct blkid_prval *v = &pr->vals[i];

		if (v->chain != chn && x == i) {
			x++;
			continue;
		}
		if (v->chain == chn) {
			--nvals;
			continue;
		}
		memcpy(&pr->vals[x++], v, sizeof(struct blkid_prval));
	}
	pr->nvals = nvals;
}

static void blkid_probe_chain_reset_position(struct blkid_chain *chn)
{
	if (chn)
		chn->idx = -1;
}

/*
 * Copies chain values from probing result to @vals, the max size of @vals is
 * @nvals and returns real number of values.
 */
int blkid_probe_chain_copy_vals(blkid_probe pr, struct blkid_chain *chn,
		struct blkid_prval *vals, int nvals)
{
	int i, x;

	for (x = 0, i = 0; i < pr->nvals && x < nvals; i++) {
		struct blkid_prval *v = &pr->vals[i];

		if (v->chain != chn)
			continue;
		memcpy(&vals[x++], v, sizeof(struct blkid_prval));
	}
	return x;
}

/*
 * Appends values from @vals to the probing result
 */
void blkid_probe_append_vals(blkid_probe pr, struct blkid_prval *vals, int nvals)
{
	int i = 0;

	while (i < nvals && pr->nvals < BLKID_NVALS) {
		memcpy(&pr->vals[pr->nvals++], &vals[i++],
				sizeof(struct blkid_prval));
	}
}

static void blkid_probe_reset_vals(blkid_probe pr)
{
	memset(pr->vals, 0, sizeof(pr->vals));
	pr->nvals = 0;
}

struct blkid_chain *blkid_probe_get_chain(blkid_probe pr)
{
	return pr->cur_chain;
}

void *blkid_probe_get_binary_data(blkid_probe pr, struct blkid_chain *chn)
{
	int rc, org_prob_flags;
	struct blkid_chain *org_chn;

	if (!pr || !chn)
		return NULL;

	/* save the current setting -- the binary API has to be completely
	 * independent on the current probing status
	 */
	org_chn = pr->cur_chain;
	org_prob_flags = pr->prob_flags;

	pr->cur_chain = chn;
	pr->prob_flags = 0;
	chn->binary = TRUE;
	blkid_probe_chain_reset_position(chn);

	rc = chn->driver->probe(pr, chn);

	chn->binary = FALSE;
	blkid_probe_chain_reset_position(chn);

	/* restore the original setting
	 */
	pr->cur_chain = org_chn;
	pr->prob_flags = org_prob_flags;

	if (rc != 0)
		return NULL;

	DBG(DEBUG_LOWPROBE,
		printf("returning %s binary data\n", chn->driver->name));
	return chn->data;
}


/**
 * blkid_reset_probe:
 * @pr: probe
 *
 * Zeroize probing results and resets the current probing (this has impact to
 * blkid_do_probe() only). This function does not touch probing filters and
 * keeps assigned device.
 */
void blkid_reset_probe(blkid_probe pr)
{
	int i;

	if (!pr)
		return;

	blkid_probe_reset_vals(pr);

	pr->cur_chain = NULL;

	for (i = 0; i < BLKID_NCHAINS; i++)
		blkid_probe_chain_reset_position(&pr->chains[i]);
}

/***
static int blkid_probe_dump_filter(blkid_probe pr, int chain)
{
	struct blkid_chain *chn;
	int i;

	if (!pr || chain < 0 || chain >= BLKID_NCHAINS)
		return -1;

	chn = &pr->chains[chain];

	if (!chn->fltr)
		return -1;

	for (i = 0; i < chn->driver->nidinfos; i++) {
		const struct blkid_idinfo *id = chn->driver->idinfos[i];

		DBG(DEBUG_LOWPROBE, printf("%d: %s: %s\n",
			i,
			id->name,
			blkid_bmp_get_item(chn->fltr, i)
				? "disabled" : "enabled <--"));
	}
	return 0;
}
***/

/*
 * Returns properly initialized chain filter
 */
unsigned long *blkid_probe_get_filter(blkid_probe pr, int chain, int create)
{
	struct blkid_chain *chn;

	if (!pr || chain < 0 || chain >= BLKID_NCHAINS)
		return NULL;

	chn = &pr->chains[chain];

	/* always when you touch the chain filter all indexes are reseted and
	 * probing starts from scratch
	 */
	blkid_probe_chain_reset_position(chn);
	pr->cur_chain = NULL;

	if (!chn->driver->has_fltr || (!chn->fltr && !create))
		return NULL;

	if (!chn->fltr)
		chn->fltr = calloc(1, blkid_bmp_nbytes(chn->driver->nidinfos));
	else
		memset(chn->fltr, 0, blkid_bmp_nbytes(chn->driver->nidinfos));

	/* blkid_probe_dump_filter(pr, chain); */
	return chn->fltr;
}

/*
 * Generic private functions for filter setting
 */
int __blkid_probe_invert_filter(blkid_probe pr, int chain)
{
	size_t i;
	struct blkid_chain *chn;

	chn = &pr->chains[chain];

	if (!chn->driver->has_fltr || !chn->fltr)
		return -1;

	for (i = 0; i < blkid_bmp_nwords(chn->driver->nidinfos); i++)
		chn->fltr[i] = ~chn->fltr[i];

	DBG(DEBUG_LOWPROBE, printf("probing filter inverted\n"));
	/* blkid_probe_dump_filter(pr, chain); */
	return 0;
}

int __blkid_probe_reset_filter(blkid_probe pr, int chain)
{
	return blkid_probe_get_filter(pr, chain, FALSE) ? 0 : -1;
}

int __blkid_probe_filter_types(blkid_probe pr, int chain, int flag, char *names[])
{
	unsigned long *fltr;
	struct blkid_chain *chn;
	size_t i;

	fltr = blkid_probe_get_filter(pr, chain, TRUE);
	if (!fltr)
		return -1;

	chn = &pr->chains[chain];

	for (i = 0; i < chn->driver->nidinfos; i++) {
		int has = 0;
		const struct blkid_idinfo *id = chn->driver->idinfos[i];
		char **n;

		for (n = names; *n; n++) {
			if (!strcmp(id->name, *n)) {
				has = 1;
				break;
			}
		}
		if (flag & BLKID_FLTR_ONLYIN) {
		       if (!has)
				blkid_bmp_set_item(fltr, i);
		} else if (flag & BLKID_FLTR_NOTIN) {
			if (has)
				blkid_bmp_set_item(fltr, i);
		}
	}

	DBG(DEBUG_LOWPROBE,
		printf("%s: a new probing type-filter initialized\n",
		chn->driver->name));
	/* blkid_probe_dump_filter(pr, chain); */
	return 0;
}

unsigned char *blkid_probe_get_buffer(blkid_probe pr,
				blkid_loff_t off, blkid_loff_t len)
{
	struct list_head *p;
	struct blkid_bufinfo *bf = NULL;

	if (pr->size <= 0)
		return NULL;

	if (pr->parent &&
	    pr->parent->devno == pr->devno &&
	    pr->parent->off <= pr->off &&
	    pr->parent->off + pr->parent->size >= pr->off + pr->size) {
		/*
		 * This is a cloned prober and points to the same area as
		 * parent. Let's use parent's buffers.
		 *
		 * Note that pr->off (and pr->parent->off) is always from the
		 * beginig of the device.
		 */
		return blkid_probe_get_buffer(pr->parent,
				pr->off + off - pr->parent->off, len);
	}

	list_for_each(p, &pr->buffers) {
		struct blkid_bufinfo *x =
				list_entry(p, struct blkid_bufinfo, bufs);

		if (x->off <= off && off + len <= x->off + x->len) {
			DBG(DEBUG_LOWPROBE,
				printf("\treuse buffer: off=%jd len=%jd pr=%p\n",
							x->off, x->len, pr));
			bf = x;
			break;
		}
	}
	if (!bf) {
		ssize_t ret;

		if (blkid_llseek(pr->fd, pr->off + off, SEEK_SET) < 0)
			return NULL;

		/* allocate info and space for data by why call */
		bf = calloc(1, sizeof(struct blkid_bufinfo) + len);
		if (!bf)
			return NULL;

		bf->data = ((unsigned char *) bf) + sizeof(struct blkid_bufinfo);
		bf->len = len;
		bf->off = off;
		INIT_LIST_HEAD(&bf->bufs);

		DBG(DEBUG_LOWPROBE,
			printf("\tbuffer read: off=%jd len=%jd pr=%p\n",
				off, len, pr));

		ret = read(pr->fd, bf->data, len);
		if (ret != (ssize_t) len) {
			free(bf);
			return NULL;
		}
		list_add_tail(&bf->bufs, &pr->buffers);
	}

	return off ? bf->data + (off - bf->off) : bf->data;
}


static void blkid_probe_reset_buffer(blkid_probe pr)
{
	uint64_t read_ct = 0, len_ct = 0;

	if (!pr || list_empty(&pr->buffers))
		return;

	DBG(DEBUG_LOWPROBE, printf("reseting probing buffers pr=%p\n", pr));

	while (!list_empty(&pr->buffers)) {
		struct blkid_bufinfo *bf = list_entry(pr->buffers.next,
						struct blkid_bufinfo, bufs);
		read_ct++;
		len_ct += bf->len;
		list_del(&bf->bufs);
		free(bf);
	}

	DBG(DEBUG_LOWPROBE,
		printf("buffers summary: %"PRIu64" bytes "
			"by %"PRIu64" read() call(s)\n",
			len_ct, read_ct));

	INIT_LIST_HEAD(&pr->buffers);
}

/*
 * Small devices need a special care.
 */
int blkid_probe_is_tiny(blkid_probe pr)
{
	return pr && (pr->flags & BLKID_FL_TINY_DEV);
}

/*
 * CDROMs may fail when probed for RAID (last sector problem)
 */
int blkid_probe_is_cdrom(blkid_probe pr)
{
	return pr && (pr->flags & BLKID_FL_CDROM_DEV);
}

/**
 * blkid_probe_set_device:
 * @pr: probe
 * @fd: device file descriptor
 * @off: begin of probing area
 * @size: size of probing area (zero means whole device/file)
 *
 * Assigns the device to probe control struct, resets internal buffers and
 * resets the current probing.
 *
 * Returns: -1 in case of failure, or 0 on success.
 */
int blkid_probe_set_device(blkid_probe pr, int fd,
		blkid_loff_t off, blkid_loff_t size)
{
	struct stat sb;

	if (!pr)
		return -1;

	blkid_reset_probe(pr);
	blkid_probe_reset_buffer(pr);

	if ((pr->flags & BLKID_FL_PRIVATE_FD) && pr->fd >= 0)
		close(pr->fd);

	pr->flags &= ~BLKID_FL_PRIVATE_FD;
	pr->flags &= ~BLKID_FL_TINY_DEV;
	pr->flags &= ~BLKID_FL_CDROM_DEV;
	pr->prob_flags = 0;
	pr->fd = fd;
	pr->off = off;
	pr->size = 0;
	pr->devno = 0;
	pr->disk_devno = 0;
	pr->mode = 0;
	pr->blkssz = 0;
	pr->wipe_off = 0;
	pr->wipe_size = 0;
	pr->wipe_chain = NULL;

#if defined(POSIX_FADV_RANDOM) && defined(HAVE_POSIX_FADVISE)
	/* Disable read-ahead */
	posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
#endif
	if (fstat(fd, &sb))
		goto err;

	if (!S_ISBLK(sb.st_mode) && !S_ISCHR(sb.st_mode) && !S_ISREG(sb.st_mode))
		goto err;

	pr->mode = sb.st_mode;
	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode))
		pr->devno = sb.st_rdev;

	if (size)
		pr->size = size;
	else {
		if (S_ISBLK(sb.st_mode)) {
			if (blkdev_get_size(fd, (unsigned long long *) &pr->size)) {
				DBG(DEBUG_LOWPROBE, printf(
					"failed to get device size\n"));
				goto err;
			}
		} else if (S_ISCHR(sb.st_mode))
			pr->size = 1;		/* UBI devices are char... */
		else if (S_ISREG(sb.st_mode))
			pr->size = sb.st_size;	/* regular file */

		if (pr->off > pr->size)
			goto err;

		/* The probing area cannot be larger than whole device, pr->off
		 * is offset within the device */
		pr->size -= pr->off;
	}

	if (pr->size <= 1440 * 1024 && !S_ISCHR(sb.st_mode))
		pr->flags |= BLKID_FL_TINY_DEV;

#ifdef CDROM_GET_CAPABILITY
	if (S_ISBLK(sb.st_mode) && ioctl(fd, CDROM_GET_CAPABILITY, NULL) >= 0)
		pr->flags |= BLKID_FL_CDROM_DEV;
#endif

	DBG(DEBUG_LOWPROBE, printf("ready for low-probing, offset=%jd, size=%jd\n",
				pr->off, pr->size));
	DBG(DEBUG_LOWPROBE, printf("whole-disk: %s, regfile: %s\n",
		blkid_probe_is_wholedisk(pr) ?"YES" : "NO",
		S_ISREG(pr->mode) ? "YES" : "NO"));

	return 0;
err:
	DBG(DEBUG_LOWPROBE,
		printf("failed to prepare a device for low-probing\n"));
	return -1;

}

int blkid_probe_get_dimension(blkid_probe pr,
		blkid_loff_t *off, blkid_loff_t *size)
{
	if (!pr)
		return -1;

	*off = pr->off;
	*size = pr->size;
	return 0;
}

int blkid_probe_set_dimension(blkid_probe pr,
		blkid_loff_t off, blkid_loff_t size)
{
	if (!pr)
		return -1;

	DBG(DEBUG_LOWPROBE, printf(
		"changing probing area pr=%p: size=%llu, off=%llu "
		"-to-> size=%llu, off=%llu\n",
		pr,
		(unsigned long long) pr->size,
		(unsigned long long) pr->off,
		(unsigned long long) size,
		(unsigned long long) off));

	pr->off = off;
	pr->size = size;
	pr->flags &= ~BLKID_FL_TINY_DEV;

	if (pr->size <= 1440 * 1024 && !S_ISCHR(pr->mode))
		pr->flags |= BLKID_FL_TINY_DEV;

	blkid_probe_reset_buffer(pr);

	return 0;
}

int blkid_probe_get_idmag(blkid_probe pr, const struct blkid_idinfo *id,
			blkid_loff_t *offset, const struct blkid_idmag **res)
{
	const struct blkid_idmag *mag = NULL;
	blkid_loff_t off = 0;

	if (id)
		mag = id->magics ? &id->magics[0] : NULL;
	if (res)
		*res = NULL;

	/* try to detect by magic string */
	while(mag && mag->magic) {
		unsigned char *buf;

		off = (mag->kboff + (mag->sboff >> 10)) << 10;
		buf = blkid_probe_get_buffer(pr, off, 1024);

		if (buf && !memcmp(mag->magic,
				buf + (mag->sboff & 0x3ff), mag->len)) {
			DBG(DEBUG_LOWPROBE, printf(
				"\tmagic sboff=%u, kboff=%ld\n",
				mag->sboff, mag->kboff));
			if (offset)
				*offset = off + (mag->sboff & 0x3ff);
			if (res)
				*res = mag;
			return 0;
		}
		mag++;
	}

	if (id->magics && id->magics[0].magic)
		/* magic string(s) defined, but not found */
		return 1;

	return 0;
}

static inline void blkid_probe_start(blkid_probe pr)
{
	if (pr) {
		pr->cur_chain = NULL;
		pr->prob_flags = 0;
		blkid_probe_set_wiper(pr, 0, 0);
	}
}

static inline void blkid_probe_end(blkid_probe pr)
{
	if (pr) {
		pr->cur_chain = NULL;
		pr->prob_flags = 0;
		blkid_probe_set_wiper(pr, 0, 0);
	}
}

/**
 * blkid_do_probe:
 * @pr: prober
 *
 * Calls probing functions in all enabled chains. The superblocks chain is
 * enabled by default. The blkid_do_probe() stores result from only one
 * probing function. It's necessary to call this routine in a loop to get
 * results from all probing functions in all chains. The probing is reseted
 * by blkid_reset_probe() or by filter functions.
 *
 * This is string-based NAME=value interface only.
 *
 * <example>
 *   <title>basic case - use the first result only</title>
 *   <programlisting>
 *
 *	if (blkid_do_probe(pr) == 0) {
 *		int nvals = blkid_probe_numof_values(pr);
 *		for (n = 0; n < nvals; n++) {
 *			if (blkid_probe_get_value(pr, n, &name, &data, &len) == 0)
 *				printf("%s = %s\n", name, data);
 *		}
 *	}
 *  </programlisting>
 * </example>
 *
 * <example>
 *   <title>advanced case - probe for all signatures</title>
 *   <programlisting>
 *
 *	while (blkid_do_probe(pr) == 0) {
 *		int nvals = blkid_probe_numof_values(pr);
 *		...
 *	}
 *  </programlisting>
 * </example>
 *
 * See also blkid_reset_probe().
 *
 * Returns: 0 on success, 1 when probing is done and -1 in case of error.
 */
int blkid_do_probe(blkid_probe pr)
{
	int rc = 1;

	if (!pr)
		return -1;

	do {
		struct blkid_chain *chn = pr->cur_chain;

		if (!chn) {
			blkid_probe_start(pr);
			chn = pr->cur_chain = &pr->chains[0];
		}
		/* we go to the next chain only when the previous probing
		 * result was nothing (rc == 1) and when the current chain is
		 * disabled or we are at end of the current chain (chain->idx +
		 * 1 == sizeof chain) or the current chain bailed out right at
		 * the start (chain->idx == -1)
		 */
		else if (rc == 1 && (chn->enabled == FALSE ||
				     chn->idx + 1 == (int) chn->driver->nidinfos ||
				     chn->idx == -1)) {

			size_t idx = chn->driver->id + 1;

			if (idx < BLKID_NCHAINS)
				chn = pr->cur_chain = &pr->chains[idx];
			else {
				blkid_probe_end(pr);
				return 1;	/* all chains already probed */
			}
		}

		chn->binary = FALSE;		/* for sure... */

		DBG(DEBUG_LOWPROBE, printf("chain probe %s %s (idx=%d)\n",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED",
				chn->idx));

		if (!chn->enabled)
			continue;

		/* rc: -1 = error, 0 = success, 1 = no result */
		rc = chn->driver->probe(pr, chn);

	} while (rc == 1);

	return rc;
}

/**
 * blkid_do_safeprobe:
 * @pr: prober
 *
 * This function gathers probing results from all enabled chains and checks
 * for ambivalent results (e.g. more filesystems on the device).
 *
 * This is string-based NAME=value interface only.
 *
 * Note about suberblocks chain -- the function does not check for filesystems
 * when a RAID signature is detected.  The function also does not check for
 * collision between RAIDs. The first detected RAID is returned. The function
 * checks for collision between partition table and RAID signature -- it's
 * recommended to enable partitions chain together with superblocks chain.
 *
 * Returns: 0 on success, 1 if nothing is detected, -2 if ambivalen result is
 * detected and -1 on case of error.
 */
int blkid_do_safeprobe(blkid_probe pr)
{
	int i, count = 0, rc = 0;

	if (!pr)
		return -1;

	blkid_probe_start(pr);

	for (i = 0; i < BLKID_NCHAINS; i++) {
		struct blkid_chain *chn;

		chn = pr->cur_chain = &pr->chains[i];
		chn->binary = FALSE;		/* for sure... */

		DBG(DEBUG_LOWPROBE, printf("chain safeprobe %s %s\n",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED"));

		if (!chn->enabled)
			continue;

		blkid_probe_chain_reset_position(chn);

		rc = chn->driver->safeprobe(pr, chn);

		blkid_probe_chain_reset_position(chn);

		/* rc: -2 ambivalent, -1 = error, 0 = success, 1 = no result */
		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;	/* success */
	}

done:
	blkid_probe_end(pr);
	if (rc < 0)
		return rc;
	return count ? 0 : 1;
}

/**
 * blkid_do_fullprobe:
 * @pr: prober
 *
 * This function gathers probing results from all enabled chains. Same as
 * blkid_do_safeprobe() but does not check for collision between probing
 * result.
 *
 * This is string-based NAME=value interface only.
 *
 * Returns: 0 on success, 1 if nothing is detected or -1 on case of error.
 */
int blkid_do_fullprobe(blkid_probe pr)
{
	int i, count = 0, rc = 0;

	if (!pr)
		return -1;

	blkid_probe_start(pr);

	for (i = 0; i < BLKID_NCHAINS; i++) {
		int rc;
		struct blkid_chain *chn;

		chn = pr->cur_chain = &pr->chains[i];
		chn->binary = FALSE;		/* for sure... */

		DBG(DEBUG_LOWPROBE, printf("chain fullprobe %s: %s\n",
				chn->driver->name,
				chn->enabled? "ENABLED" : "DISABLED"));

		if (!chn->enabled)
			continue;

		blkid_probe_chain_reset_position(chn);

		rc = chn->driver->probe(pr, chn);

		blkid_probe_chain_reset_position(chn);

		/* rc: -1 = error, 0 = success, 1 = no result */
		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;	/* success */
	}

done:
	blkid_probe_end(pr);
	if (rc < 0)
		return rc;
	return count ? 0 : 1;
}

/* same sa blkid_probe_get_buffer() but works with 512-sectors */
unsigned char *blkid_probe_get_sector(blkid_probe pr, unsigned int sector)
{
	return pr ? blkid_probe_get_buffer(pr,
			((blkid_loff_t) sector) << 9, 0x200) : NULL;
}

struct blkid_prval *blkid_probe_assign_value(
			blkid_probe pr, const char *name)
{
	struct blkid_prval *v;

	if (!name)
		return NULL;
	if (pr->nvals >= BLKID_NVALS)
		return NULL;

	v = &pr->vals[pr->nvals];
	v->name = name;
	v->chain = pr->cur_chain;
	pr->nvals++;

	DBG(DEBUG_LOWPROBE,
		printf("assigning %s [%s]\n", name, v->chain->driver->name));
	return v;
}

int blkid_probe_reset_last_value(blkid_probe pr)
{
	struct blkid_prval *v;

	if (pr == NULL || pr->nvals == 0)
		return -1;

	v = &pr->vals[pr->nvals - 1];

	DBG(DEBUG_LOWPROBE,
		printf("un-assigning %s [%s]\n", v->name, v->chain->driver->name));

	memset(v, 0, sizeof(struct blkid_prval));
	pr->nvals--;

	return 0;

}

int blkid_probe_set_value(blkid_probe pr, const char *name,
		unsigned char *data, size_t len)
{
	struct blkid_prval *v;

	if (len > BLKID_PROBVAL_BUFSIZ)
		len = BLKID_PROBVAL_BUFSIZ;

	v = blkid_probe_assign_value(pr, name);
	if (!v)
		return -1;

	memcpy(v->data, data, len);
	v->len = len;
	return 0;
}

int blkid_probe_vsprintf_value(blkid_probe pr, const char *name,
		const char *fmt, va_list ap)
{
	struct blkid_prval *v;
	size_t len;

	v = blkid_probe_assign_value(pr, name);
	if (!v)
		return -1;

	len = vsnprintf((char *) v->data, sizeof(v->data), fmt, ap);

	if (len <= 0) {
		blkid_probe_reset_last_value(pr);
		return -1;
	}
	v->len = len + 1;
	return 0;
}

int blkid_probe_sprintf_value(blkid_probe pr, const char *name,
		const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start(ap, fmt);
	rc = blkid_probe_vsprintf_value(pr, name, fmt, ap);
	va_end(ap);

	return rc;
}

/**
 * blkid_probe_get_devno:
 * @pr: probe
 *
 * Returns: block device number, or 0 for regilar files.
 */
dev_t blkid_probe_get_devno(blkid_probe pr)
{
	return pr->devno;
}

/**
 * blkid_probe_get_wholedisk_devno:
 * @pr: probe
 *
 * Returns: device number of the wholedisk, or 0 for regilar files.
 */
dev_t blkid_probe_get_wholedisk_devno(blkid_probe pr)
{
	if (!pr->disk_devno) {
		dev_t devno, disk_devno = 0;

		devno = blkid_probe_get_devno(pr);
		if (!devno)
			return 0;

		 if (blkid_devno_to_wholedisk(devno, NULL, 0, &disk_devno) == 0)
			pr->disk_devno = disk_devno;
	}
	return pr->disk_devno;
}

/**
 * blkid_probe_is_wholedisk:
 * @pr: probe
 *
 * Returns: 1 if the device is whole-disk or 0.
 */
int blkid_probe_is_wholedisk(blkid_probe pr)
{
	dev_t devno, disk_devno;

	devno = blkid_probe_get_devno(pr);
	if (!devno)
		return 0;

	disk_devno = blkid_probe_get_wholedisk_devno(pr);
	if (!disk_devno)
		return 0;

	return devno == disk_devno;
}

blkid_probe blkid_probe_get_wholedisk_probe(blkid_probe pr)
{
	dev_t disk;

	if (blkid_probe_is_wholedisk(pr))
		return NULL;			/* this is not partition */

	if (pr->parent)
		/* this is cloned blkid_probe, use parent's stuff */
		return blkid_probe_get_wholedisk_probe(pr->parent);

	disk = blkid_probe_get_wholedisk_devno(pr);

	if (pr->disk_probe && pr->disk_probe->devno != disk) {
		/* we have disk prober, but for another disk... close it */
		blkid_free_probe(pr->disk_probe);
		pr->disk_probe = NULL;
	}

	if (!pr->disk_probe) {
		/* Open a new disk prober */
		char *disk_path = blkid_devno_to_devname(disk);

		if (!disk_path)
			return NULL;

		DBG(DEBUG_LOWPROBE, printf("allocate a wholedisk probe\n"));

		pr->disk_probe = blkid_new_probe_from_filename(disk_path);
		if (!pr->disk_probe)
			return NULL;	/* ENOMEM? */
	}

	return pr->disk_probe;
}

/**
 * blkid_probe_get_size:
 * @pr: probe
 *
 * This function returns size of probing area as defined by blkid_probe_set_device().
 * If the size of the probing area is unrestricted then this function returns
 * the real size of device. See also blkid_get_dev_size().
 *
 * Returns: size in bytes or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_size(blkid_probe pr)
{
	return pr ? pr->size : -1;
}

/**
 * blkid_probe_get_offset:
 * @pr: probe
 *
 * This function returns offset of probing area as defined by blkid_probe_set_device().
 *
 * Returns: offset in bytes or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_offset(blkid_probe pr)
{
	return pr ? pr->off : -1;
}

/**
 * blkid_probe_get_fd:
 * @pr: probe
 *
 * Returns: file descriptor for assigned device/file.
 */
int blkid_probe_get_fd(blkid_probe pr)
{
	return pr ? pr->fd : -1;
}

/**
 * blkid_probe_get_sectorsize:
 * @pr: probe or NULL (for NULL returns 512)
 *
 * Returns: block device logical sector size (BLKSSZGET ioctl, default 512).
 */
unsigned int blkid_probe_get_sectorsize(blkid_probe pr)
{
	if (!pr)
		return DEFAULT_SECTOR_SIZE;  /*... and good luck! */

	if (pr->blkssz)
		return pr->blkssz;

	if (S_ISBLK(pr->mode) &&
	    blkdev_get_sector_size(pr->fd, (int *) &pr->blkssz) == 0)
		return pr->blkssz;

	pr->blkssz = DEFAULT_SECTOR_SIZE;
	return pr->blkssz;
}

/**
 * blkid_probe_get_sectors:
 * @pr: probe
 *
 * Returns: 512-byte sector count or -1 in case of error.
 */
blkid_loff_t blkid_probe_get_sectors(blkid_probe pr)
{
	return pr ? pr->size >> 9 : -1;
}

/**
 * blkid_probe_numof_values:
 * @pr: probe
 *
 * Returns: number of values in probing result or -1 in case of error.
 */
int blkid_probe_numof_values(blkid_probe pr)
{
	if (!pr)
		return -1;
	return pr->nvals;
}

/**
 * blkid_probe_get_value:
 * @pr: probe
 * @num: wanted value in range 0..N, where N is blkid_probe_numof_values() - 1
 * @name: pointer to return value name or NULL
 * @data: pointer to return value data or NULL
 * @len: pointer to return value length or NULL
 *
 * Note, the @len returns length of the @data, including the terminating
 * '\0' character.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_get_value(blkid_probe pr, int num, const char **name,
			const char **data, size_t *len)
{
	struct blkid_prval *v = __blkid_probe_get_value(pr, num);

	if (!v)
		return -1;
	if (name)
		*name = v->name;
	if (data)
		*data = (char *) v->data;
	if (len)
		*len = v->len;

	DBG(DEBUG_LOWPROBE, printf("returning %s value\n", v->name));
	return 0;
}

/**
 * blkid_probe_lookup_value:
 * @pr: probe
 * @name: name of value
 * @data: pointer to return value data or NULL
 * @len: pointer to return value length or NULL
 *
 * Note, the @len returns length of the @data, including the terminating
 * '\0' character.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_lookup_value(blkid_probe pr, const char *name,
			const char **data, size_t *len)
{
	struct blkid_prval *v = __blkid_probe_lookup_value(pr, name);

	if (!v)
		return -1;
	if (data)
		*data = (char *) v->data;
	if (len)
		*len = v->len;
	return 0;
}

/**
 * blkid_probe_has_value:
 * @pr: probe
 * @name: name of value
 *
 * Returns: 1 if value exist in probing result, otherwise 0.
 */
int blkid_probe_has_value(blkid_probe pr, const char *name)
{
	if (blkid_probe_lookup_value(pr, name, NULL, NULL) == 0)
		return 1;
	return 0;
}

struct blkid_prval *__blkid_probe_get_value(blkid_probe pr, int num)
{
	if (pr == NULL || num < 0 || num >= pr->nvals)
		return NULL;

	return &pr->vals[num];
}

struct blkid_prval *__blkid_probe_lookup_value(blkid_probe pr, const char *name)
{
	int i;

	if (pr == NULL || pr->nvals == 0 || name == NULL)
		return NULL;

	for (i = 0; i < pr->nvals; i++) {
		struct blkid_prval *v = &pr->vals[i];

		if (v->name && strcmp(name, v->name) == 0) {
			DBG(DEBUG_LOWPROBE, printf("returning %s value\n", v->name));
			return v;
		}
	}
	return NULL;
}


/* converts DCE UUID (uuid[16]) to human readable string
 * - the @len should be always 37 */
#ifdef HAVE_LIBUUID
void blkid_unparse_uuid(const unsigned char *uuid, char *str,
			size_t len __attribute__((__unused__)))
{
	uuid_unparse(uuid, str);
}
#else
void blkid_unparse_uuid(const unsigned char *uuid, char *str, size_t len)
{
	snprintf(str, len,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5],
		uuid[6], uuid[7],
		uuid[8], uuid[9],
		uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],uuid[15]);
}
#endif


/* Removes whitespace from the right-hand side of a string (trailing
 * whitespace).
 *
 * Returns size of the new string (without \0).
 */
size_t blkid_rtrim_whitespace(unsigned char *str)
{
	size_t i = strlen((char *) str);

	while (i--) {
		if (!isspace(str[i]))
			break;
	}
	str[++i] = '\0';
	return i;
}

/*
 * Some mkfs-like utils wipe some parts (usually begin) of the device.
 * For example LVM (pvcreate) or mkswap(8). This information could be used
 * for later resolution to conflicts between superblocks.
 *
 * For example we found valid LVM superblock, LVM wipes 8KiB at the begin of
 * the device. If we found another signature (for example MBR) this wiped area
 * then the signature has been added later and LVM superblock should be ignore.
 *
 * Note that this heuristic is not 100% reliable, for example "pvcreate --zero
 * n" allows to keep the begin of the device unmodified. It's probably better
 * to use this heuristic for conflicts between superblocks and partition tables
 * than for conflicts between filesystem superblocks -- existence of unwanted
 * partition table is very unusual, because PT is pretty visible (parsed and
 * interpreted by kernel).
 */
void blkid_probe_set_wiper(blkid_probe pr, blkid_loff_t off, blkid_loff_t size)
{
	struct blkid_chain *chn;

	if (!pr)
		return;

	if (!size) {
		DBG(DEBUG_LOWPROBE, printf("zeroize wiper\n"));
		pr->wipe_size = pr->wipe_off = 0;
		pr->wipe_chain = NULL;
		return;
	}

	chn = pr->cur_chain;

	if (!chn || !chn->driver ||
	    chn->idx < 0 || (size_t) chn->idx >= chn->driver->nidinfos)
		return;

	pr->wipe_size = size;
	pr->wipe_off = off;
	pr->wipe_chain = chn;

	DBG(DEBUG_LOWPROBE,
		printf("wiper set to %s::%s off=%jd size=%jd\n",
			chn->driver->name,
			chn->driver->idinfos[chn->idx]->name,
			pr->wipe_off, pr->wipe_size));
	return;
}

/*
 * Returns 1 if the <@off,@size> area was wiped
 */
int blkid_probe_is_wiped(blkid_probe pr, struct blkid_chain **chn,
		     blkid_loff_t off, blkid_loff_t size)
{
	if (!pr || !size)
		return 0;

	if (pr->wipe_off <= off && off + size <= pr->wipe_off + pr->wipe_size) {
		if (chn)
			*chn = pr->wipe_chain;
		return 1;
	}
	return 0;
}

void blkid_probe_use_wiper(blkid_probe pr, blkid_loff_t off, blkid_loff_t size)
{
	struct blkid_chain *chn = NULL;

	if (blkid_probe_is_wiped(pr, &chn, off, size) && chn) {
		DBG(DEBUG_LOWPROBE, printf("wiped area detected -- ignore previous results\n"));
		blkid_probe_set_wiper(pr, 0, 0);
		blkid_probe_chain_reset_vals(pr, chn);
	}
}

