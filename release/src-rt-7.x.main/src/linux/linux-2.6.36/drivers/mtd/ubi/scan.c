/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Artem Bityutskiy (Битюцкий Артём)
 */

/*
 * UBI scanning sub-system.
 *
 * This sub-system is responsible for scanning the flash media, checking UBI
 * headers and providing complete information about the UBI flash image.
 *
 * The scanning information is represented by a &struct ubi_scan_info' object.
 * Information about found volumes is represented by &struct ubi_scan_volume
 * objects which are kept in volume RB-tree with root at the @volumes field.
 * The RB-tree is indexed by the volume ID.
 *
 * Found logical eraseblocks are represented by &struct ubi_scan_leb objects.
 * These objects are kept in per-volume RB-trees with the root at the
 * corresponding &struct ubi_scan_volume object. To put it differently, we keep
 * an RB-tree of per-volume objects and each of these objects is the root of
 * RB-tree of per-eraseblock objects.
 *
 * Corrupted physical eraseblocks are put to the @corr list, free physical
 * eraseblocks are put to the @free list and the physical eraseblock to be
 * erased are put to the @erase list.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/math64.h>
#include <linux/random.h>
#include "ubi.h"

#ifdef CONFIG_MTD_UBI_DEBUG_PARANOID
static int paranoid_check_si(struct ubi_device *ubi, struct ubi_scan_info *si);
#else
#define paranoid_check_si(ubi, si) 0
#endif

/* Temporary variables used during scanning */
static struct ubi_ec_hdr *ech;
static struct ubi_vid_hdr *vidh;

/**
 * add_to_list - add physical eraseblock to a list.
 * @si: scanning information
 * @pnum: physical eraseblock number to add
 * @ec: erase counter of the physical eraseblock
 * @list: the list to add to
 *
 * This function adds physical eraseblock @pnum to free, erase, corrupted or
 * alien lists. Returns zero in case of success and a negative error code in
 * case of failure.
 */
static int add_to_list(struct ubi_scan_info *si, int pnum, int ec,
		       struct list_head *list)
{
	struct ubi_scan_leb *seb;

	if (list == &si->free) {
		dbg_bld("add to free: PEB %d, EC %d", pnum, ec);
		si->free_peb_count += 1;
	} else if (list == &si->erase) {
		dbg_bld("add to erase: PEB %d, EC %d", pnum, ec);
		si->erase_peb_count += 1;
	} else if (list == &si->corr) {
		dbg_bld("add to corrupted: PEB %d, EC %d", pnum, ec);
		si->corr_peb_count += 1;
	} else if (list == &si->alien) {
		dbg_bld("add to alien: PEB %d, EC %d", pnum, ec);
		si->alien_peb_count += 1;
	} else
		BUG();

	seb = kmalloc(sizeof(struct ubi_scan_leb), GFP_KERNEL);
	if (!seb)
		return -ENOMEM;

	seb->pnum = pnum;
	seb->ec = ec;
	list_add_tail(&seb->u.list, list);
	return 0;
}

/**
 * validate_vid_hdr - check volume identifier header.
 * @vid_hdr: the volume identifier header to check
 * @sv: information about the volume this logical eraseblock belongs to
 * @pnum: physical eraseblock number the VID header came from
 *
 * This function checks that data stored in @vid_hdr is consistent. Returns
 * non-zero if an inconsistency was found and zero if not.
 *
 * Note, UBI does sanity check of everything it reads from the flash media.
 * Most of the checks are done in the I/O sub-system. Here we check that the
 * information in the VID header is consistent to the information in other VID
 * headers of the same volume.
 */
static int validate_vid_hdr(const struct ubi_vid_hdr *vid_hdr,
			    const struct ubi_scan_volume *sv, int pnum)
{
	int vol_type = vid_hdr->vol_type;
	int vol_id = be32_to_cpu(vid_hdr->vol_id);
	int used_ebs = be32_to_cpu(vid_hdr->used_ebs);
	int data_pad = be32_to_cpu(vid_hdr->data_pad);

	if (sv->leb_count != 0) {
		int sv_vol_type;

		/*
		 * This is not the first logical eraseblock belonging to this
		 * volume. Ensure that the data in its VID header is consistent
		 * to the data in previous logical eraseblock headers.
		 */

		if (vol_id != sv->vol_id) {
			dbg_err("inconsistent vol_id");
			goto bad;
		}

		if (sv->vol_type == UBI_STATIC_VOLUME)
			sv_vol_type = UBI_VID_STATIC;
		else
			sv_vol_type = UBI_VID_DYNAMIC;

		if (vol_type != sv_vol_type) {
			dbg_err("inconsistent vol_type");
			goto bad;
		}

		if (used_ebs != sv->used_ebs) {
			dbg_err("inconsistent used_ebs");
			goto bad;
		}

		if (data_pad != sv->data_pad) {
			dbg_err("inconsistent data_pad");
			goto bad;
		}
	}

	return 0;

bad:
	ubi_err("inconsistent VID header at PEB %d", pnum);
	ubi_dbg_dump_vid_hdr(vid_hdr);
	ubi_dbg_dump_sv(sv);
	return -EINVAL;
}

/**
 * add_volume - add volume to the scanning information.
 * @si: scanning information
 * @vol_id: ID of the volume to add
 * @pnum: physical eraseblock number
 * @vid_hdr: volume identifier header
 *
 * If the volume corresponding to the @vid_hdr logical eraseblock is already
 * present in the scanning information, this function does nothing. Otherwise
 * it adds corresponding volume to the scanning information. Returns a pointer
 * to the scanning volume object in case of success and a negative error code
 * in case of failure.
 */
static struct ubi_scan_volume *add_volume(struct ubi_scan_info *si, int vol_id,
					  int pnum,
					  const struct ubi_vid_hdr *vid_hdr)
{
	struct ubi_scan_volume *sv;
	struct rb_node **p = &si->volumes.rb_node, *parent = NULL;

	ubi_assert(vol_id == be32_to_cpu(vid_hdr->vol_id));

	/* Walk the volume RB-tree to look if this volume is already present */
	while (*p) {
		parent = *p;
		sv = rb_entry(parent, struct ubi_scan_volume, rb);

		if (vol_id == sv->vol_id)
			return sv;

		if (vol_id > sv->vol_id)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	/* The volume is absent - add it */
	sv = kmalloc(sizeof(struct ubi_scan_volume), GFP_KERNEL);
	if (!sv)
		return ERR_PTR(-ENOMEM);

	sv->highest_lnum = sv->leb_count = 0;
	sv->vol_id = vol_id;
	sv->root = RB_ROOT;
	sv->used_ebs = be32_to_cpu(vid_hdr->used_ebs);
	sv->data_pad = be32_to_cpu(vid_hdr->data_pad);
	sv->compat = vid_hdr->compat;
	sv->vol_type = vid_hdr->vol_type == UBI_VID_DYNAMIC ? UBI_DYNAMIC_VOLUME
							    : UBI_STATIC_VOLUME;
	if (vol_id > si->highest_vol_id)
		si->highest_vol_id = vol_id;

	rb_link_node(&sv->rb, parent, p);
	rb_insert_color(&sv->rb, &si->volumes);
	si->vols_found += 1;
	dbg_bld("added volume %d", vol_id);
	return sv;
}

/**
 * compare_lebs - find out which logical eraseblock is newer.
 * @ubi: UBI device description object
 * @seb: first logical eraseblock to compare
 * @pnum: physical eraseblock number of the second logical eraseblock to
 * compare
 * @vid_hdr: volume identifier header of the second logical eraseblock
 *
 * This function compares 2 copies of a LEB and informs which one is newer. In
 * case of success this function returns a positive value, in case of failure, a
 * negative error code is returned. The success return codes use the following
 * bits:
 *     o bit 0 is cleared: the first PEB (described by @seb) is newer than the
 *       second PEB (described by @pnum and @vid_hdr);
 *     o bit 0 is set: the second PEB is newer;
 *     o bit 1 is cleared: no bit-flips were detected in the newer LEB;
 *     o bit 1 is set: bit-flips were detected in the newer LEB;
 *     o bit 2 is cleared: the older LEB is not corrupted;
 *     o bit 2 is set: the older LEB is corrupted.
 */
static int compare_lebs(struct ubi_device *ubi, const struct ubi_scan_leb *seb,
			int pnum, const struct ubi_vid_hdr *vid_hdr)
{
	void *buf;
	int len, err, second_is_newer, bitflips = 0, corrupted = 0;
	uint32_t data_crc, crc;
	struct ubi_vid_hdr *vh = NULL;
	unsigned long long sqnum2 = be64_to_cpu(vid_hdr->sqnum);

	if (sqnum2 == seb->sqnum) {
		/*
		 * This must be a really ancient UBI image which has been
		 * created before sequence numbers support has been added. At
		 * that times we used 32-bit LEB versions stored in logical
		 * eraseblocks. That was before UBI got into mainline. We do not
		 * support these images anymore. Well, those images will work
		 * still work, but only if no unclean reboots happened.
		 */
		ubi_err("unsupported on-flash UBI format\n");
		return -EINVAL;
	}

	/* Obviously the LEB with lower sequence counter is older */
	second_is_newer = !!(sqnum2 > seb->sqnum);

	/*
	 * Now we know which copy is newer. If the copy flag of the PEB with
	 * newer version is not set, then we just return, otherwise we have to
	 * check data CRC. For the second PEB we already have the VID header,
	 * for the first one - we'll need to re-read it from flash.
	 *
	 * Note: this may be optimized so that we wouldn't read twice.
	 */

	if (second_is_newer) {
		if (!vid_hdr->copy_flag) {
			/* It is not a copy, so it is newer */
			dbg_bld("second PEB %d is newer, copy_flag is unset",
				pnum);
			return 1;
		}
	} else {
		pnum = seb->pnum;

		vh = ubi_zalloc_vid_hdr(ubi, GFP_KERNEL);
		if (!vh)
			return -ENOMEM;

		err = ubi_io_read_vid_hdr(ubi, pnum, vh, 0);
		if (err) {
			if (err == UBI_IO_BITFLIPS)
				bitflips = 1;
			else {
				dbg_err("VID of PEB %d header is bad, but it "
					"was OK earlier", pnum);
				if (err > 0)
					err = -EIO;

				goto out_free_vidh;
			}
		}

		if (!vh->copy_flag) {
			/* It is not a copy, so it is newer */
			dbg_bld("first PEB %d is newer, copy_flag is unset",
				pnum);
			err = bitflips << 1;
			goto out_free_vidh;
		}

		vid_hdr = vh;
	}

	/* Read the data of the copy and check the CRC */

	len = be32_to_cpu(vid_hdr->data_size);
	buf = vmalloc(len);
	if (!buf) {
		err = -ENOMEM;
		goto out_free_vidh;
	}

	err = ubi_io_read_data(ubi, buf, pnum, 0, len);
	if (err && err != UBI_IO_BITFLIPS && err != -EBADMSG)
		goto out_free_buf;

	data_crc = be32_to_cpu(vid_hdr->data_crc);
	crc = crc32(UBI_CRC32_INIT, buf, len);
	if (crc != data_crc) {
		dbg_bld("PEB %d CRC error: calculated %#08x, must be %#08x",
			pnum, crc, data_crc);
		corrupted = 1;
		bitflips = 0;
		second_is_newer = !second_is_newer;
	} else {
		dbg_bld("PEB %d CRC is OK", pnum);
		bitflips = !!err;
	}

	vfree(buf);
	ubi_free_vid_hdr(ubi, vh);

	if (second_is_newer)
		dbg_bld("second PEB %d is newer, copy_flag is set", pnum);
	else
		dbg_bld("first PEB %d is newer, copy_flag is set", pnum);

	return second_is_newer | (bitflips << 1) | (corrupted << 2);

out_free_buf:
	vfree(buf);
out_free_vidh:
	ubi_free_vid_hdr(ubi, vh);
	return err;
}

/**
 * ubi_scan_add_used - add physical eraseblock to the scanning information.
 * @ubi: UBI device description object
 * @si: scanning information
 * @pnum: the physical eraseblock number
 * @ec: erase counter
 * @vid_hdr: the volume identifier header
 * @bitflips: if bit-flips were detected when this physical eraseblock was read
 *
 * This function adds information about a used physical eraseblock to the
 * 'used' tree of the corresponding volume. The function is rather complex
 * because it has to handle cases when this is not the first physical
 * eraseblock belonging to the same logical eraseblock, and the newer one has
 * to be picked, while the older one has to be dropped. This function returns
 * zero in case of success and a negative error code in case of failure.
 */
int ubi_scan_add_used(struct ubi_device *ubi, struct ubi_scan_info *si,
		      int pnum, int ec, const struct ubi_vid_hdr *vid_hdr,
		      int bitflips)
{
	int err, vol_id, lnum;
	unsigned long long sqnum;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb;
	struct rb_node **p, *parent = NULL;

	vol_id = be32_to_cpu(vid_hdr->vol_id);
	lnum = be32_to_cpu(vid_hdr->lnum);
	sqnum = be64_to_cpu(vid_hdr->sqnum);

	dbg_bld("PEB %d, LEB %d:%d, EC %d, sqnum %llu, bitflips %d",
		pnum, vol_id, lnum, ec, sqnum, bitflips);

	sv = add_volume(si, vol_id, pnum, vid_hdr);
	if (IS_ERR(sv))
		return PTR_ERR(sv);

	if (si->max_sqnum < sqnum)
		si->max_sqnum = sqnum;

	/*
	 * Walk the RB-tree of logical eraseblocks of volume @vol_id to look
	 * if this is the first instance of this logical eraseblock or not.
	 */
	p = &sv->root.rb_node;
	while (*p) {
		int cmp_res;

		parent = *p;
		seb = rb_entry(parent, struct ubi_scan_leb, u.rb);
		if (lnum != seb->lnum) {
			if (lnum < seb->lnum)
				p = &(*p)->rb_left;
			else
				p = &(*p)->rb_right;
			continue;
		}

		/*
		 * There is already a physical eraseblock describing the same
		 * logical eraseblock present.
		 */

		dbg_bld("this LEB already exists: PEB %d, sqnum %llu, "
			"EC %d", seb->pnum, seb->sqnum, seb->ec);

		/*
		 * Make sure that the logical eraseblocks have different
		 * sequence numbers. Otherwise the image is bad.
		 *
		 * However, if the sequence number is zero, we assume it must
		 * be an ancient UBI image from the era when UBI did not have
		 * sequence numbers. We still can attach these images, unless
		 * there is a need to distinguish between old and new
		 * eraseblocks, in which case we'll refuse the image in
		 * 'compare_lebs()'. In other words, we attach old clean
		 * images, but refuse attaching old images with duplicated
		 * logical eraseblocks because there was an unclean reboot.
		 */
		if (seb->sqnum == sqnum && sqnum != 0) {
			ubi_err("two LEBs with same sequence number %llu",
				sqnum);
			ubi_dbg_dump_seb(seb, 0);
			ubi_dbg_dump_vid_hdr(vid_hdr);
			return -EINVAL;
		}

		/*
		 * Now we have to drop the older one and preserve the newer
		 * one.
		 */
		cmp_res = compare_lebs(ubi, seb, pnum, vid_hdr);
		if (cmp_res < 0)
			return cmp_res;

		if (cmp_res & 1) {
			/*
			 * This logical eraseblock is newer than the one
			 * found earlier.
			 */
			err = validate_vid_hdr(vid_hdr, sv, pnum);
			if (err)
				return err;

			if (cmp_res & 4)
				err = add_to_list(si, seb->pnum, seb->ec,
						  &si->corr);
			else
				err = add_to_list(si, seb->pnum, seb->ec,
						  &si->erase);
			if (err)
				return err;

			seb->ec = ec;
			seb->pnum = pnum;
			seb->scrub = ((cmp_res & 2) || bitflips);
			seb->sqnum = sqnum;

			if (sv->highest_lnum == lnum)
				sv->last_data_size =
					be32_to_cpu(vid_hdr->data_size);

			return 0;
		} else {
			/*
			 * This logical eraseblock is older than the one found
			 * previously.
			 */
			if (cmp_res & 4)
				return add_to_list(si, pnum, ec, &si->corr);
			else
				return add_to_list(si, pnum, ec, &si->erase);
		}
	}

	/*
	 * We've met this logical eraseblock for the first time, add it to the
	 * scanning information.
	 */

	err = validate_vid_hdr(vid_hdr, sv, pnum);
	if (err)
		return err;

	seb = kmalloc(sizeof(struct ubi_scan_leb), GFP_KERNEL);
	if (!seb)
		return -ENOMEM;

	seb->ec = ec;
	seb->pnum = pnum;
	seb->lnum = lnum;
	seb->sqnum = sqnum;
	seb->scrub = bitflips;

	if (sv->highest_lnum <= lnum) {
		sv->highest_lnum = lnum;
		sv->last_data_size = be32_to_cpu(vid_hdr->data_size);
	}

	sv->leb_count += 1;
	rb_link_node(&seb->u.rb, parent, p);
	rb_insert_color(&seb->u.rb, &sv->root);
	si->used_peb_count += 1;
	return 0;
}

/**
 * ubi_scan_find_sv - find volume in the scanning information.
 * @si: scanning information
 * @vol_id: the requested volume ID
 *
 * This function returns a pointer to the volume description or %NULL if there
 * are no data about this volume in the scanning information.
 */
struct ubi_scan_volume *ubi_scan_find_sv(const struct ubi_scan_info *si,
					 int vol_id)
{
	struct ubi_scan_volume *sv;
	struct rb_node *p = si->volumes.rb_node;

	while (p) {
		sv = rb_entry(p, struct ubi_scan_volume, rb);

		if (vol_id == sv->vol_id)
			return sv;

		if (vol_id > sv->vol_id)
			p = p->rb_left;
		else
			p = p->rb_right;
	}

	return NULL;
}

/**
 * ubi_scan_find_seb - find LEB in the volume scanning information.
 * @sv: a pointer to the volume scanning information
 * @lnum: the requested logical eraseblock
 *
 * This function returns a pointer to the scanning logical eraseblock or %NULL
 * if there are no data about it in the scanning volume information.
 */
struct ubi_scan_leb *ubi_scan_find_seb(const struct ubi_scan_volume *sv,
				       int lnum)
{
	struct ubi_scan_leb *seb;
	struct rb_node *p = sv->root.rb_node;

	while (p) {
		seb = rb_entry(p, struct ubi_scan_leb, u.rb);

		if (lnum == seb->lnum)
			return seb;

		if (lnum > seb->lnum)
			p = p->rb_left;
		else
			p = p->rb_right;
	}

	return NULL;
}

/**
 * ubi_scan_rm_volume - delete scanning information about a volume.
 * @si: scanning information
 * @sv: the volume scanning information to delete
 */
void ubi_scan_rm_volume(struct ubi_scan_info *si, struct ubi_scan_volume *sv)
{
	struct rb_node *rb;
	struct ubi_scan_leb *seb;

	dbg_bld("remove scanning information about volume %d", sv->vol_id);

	while ((rb = rb_first(&sv->root))) {
		seb = rb_entry(rb, struct ubi_scan_leb, u.rb);
		rb_erase(&seb->u.rb, &sv->root);
		list_add_tail(&seb->u.list, &si->erase);
	}

	rb_erase(&sv->rb, &si->volumes);
	kfree(sv);
	si->vols_found -= 1;
}

/**
 * ubi_scan_erase_peb - erase a physical eraseblock.
 * @ubi: UBI device description object
 * @si: scanning information
 * @pnum: physical eraseblock number to erase;
 * @ec: erase counter value to write (%UBI_SCAN_UNKNOWN_EC if it is unknown)
 *
 * This function erases physical eraseblock 'pnum', and writes the erase
 * counter header to it. This function should only be used on UBI device
 * initialization stages, when the EBA sub-system had not been yet initialized.
 * This function returns zero in case of success and a negative error code in
 * case of failure.
 */
int ubi_scan_erase_peb(struct ubi_device *ubi, const struct ubi_scan_info *si,
		       int pnum, int ec)
{
	int err;
	struct ubi_ec_hdr *ec_hdr;

	if ((long long)ec >= UBI_MAX_ERASECOUNTER) {
		/*
		 * Erase counter overflow. Upgrade UBI and use 64-bit
		 * erase counters internally.
		 */
		ubi_err("erase counter overflow at PEB %d, EC %d", pnum, ec);
		return -EINVAL;
	}

	ec_hdr = kzalloc(ubi->ec_hdr_alsize, GFP_KERNEL);
	if (!ec_hdr)
		return -ENOMEM;

	ec_hdr->ec = cpu_to_be64(ec);

	err = ubi_io_sync_erase(ubi, pnum, 0);
	if (err < 0)
		goto out_free;

	err = ubi_io_write_ec_hdr(ubi, pnum, ec_hdr);

out_free:
	kfree(ec_hdr);
	return err;
}

/**
 * ubi_scan_get_free_peb - get a free physical eraseblock.
 * @ubi: UBI device description object
 * @si: scanning information
 *
 * This function returns a free physical eraseblock. It is supposed to be
 * called on the UBI initialization stages when the wear-leveling sub-system is
 * not initialized yet. This function picks a physical eraseblocks from one of
 * the lists, writes the EC header if it is needed, and removes it from the
 * list.
 *
 * This function returns scanning physical eraseblock information in case of
 * success and an error code in case of failure.
 */
struct ubi_scan_leb *ubi_scan_get_free_peb(struct ubi_device *ubi,
					   struct ubi_scan_info *si)
{
	int err = 0, i;
	struct ubi_scan_leb *seb;

	if (!list_empty(&si->free)) {
		seb = list_entry(si->free.next, struct ubi_scan_leb, u.list);
		list_del(&seb->u.list);
		dbg_bld("return free PEB %d, EC %d", seb->pnum, seb->ec);
		return seb;
	}

	for (i = 0; i < 2; i++) {
		struct list_head *head;
		struct ubi_scan_leb *tmp_seb;

		if (i == 0)
			head = &si->erase;
		else
			head = &si->corr;

		/*
		 * We try to erase the first physical eraseblock from the @head
		 * list and pick it if we succeed, or try to erase the
		 * next one if not. And so forth. We don't want to take care
		 * about bad eraseblocks here - they'll be handled later.
		 */
		list_for_each_entry_safe(seb, tmp_seb, head, u.list) {
			if (seb->ec == UBI_SCAN_UNKNOWN_EC)
				seb->ec = si->mean_ec;

			err = ubi_scan_erase_peb(ubi, si, seb->pnum, seb->ec+1);
			if (err)
				continue;

			seb->ec += 1;
			list_del(&seb->u.list);
			dbg_bld("return PEB %d, EC %d", seb->pnum, seb->ec);
			return seb;
		}
	}

	ubi_err("no eraseblocks found");
	return ERR_PTR(-ENOSPC);
}

/**
 * process_eb - read, check UBI headers, and add them to scanning information.
 * @ubi: UBI device description object
 * @si: scanning information
 * @pnum: the physical eraseblock number
 *
 * This function returns a zero if the physical eraseblock was successfully
 * handled and a negative error code in case of failure.
 */
static int process_eb(struct ubi_device *ubi, struct ubi_scan_info *si,
		      int pnum)
{
	long long uninitialized_var(ec);
	int err, bitflips = 0, vol_id, ec_corr = 0;

	dbg_bld("scan PEB %d", pnum);

	/* Skip bad physical eraseblocks */
	err = ubi_io_is_bad(ubi, pnum);
	if (err < 0)
		return err;
	else if (err) {
		si->bad_peb_count += 1;
		return 0;
	}

	err = ubi_io_read_ec_hdr(ubi, pnum, ech, 0);
	if (err < 0)
		return err;
	else if (err == UBI_IO_BITFLIPS)
		bitflips = 1;
	else if (err == UBI_IO_PEB_EMPTY)
		return add_to_list(si, pnum, UBI_SCAN_UNKNOWN_EC, &si->erase);
	else if (err == UBI_IO_BAD_HDR_READ || err == UBI_IO_BAD_HDR) {
		/*
		 * We have to also look at the VID header, possibly it is not
		 * corrupted. Set %bitflips flag in order to make this PEB be
		 * moved and EC be re-created.
		 */
		ec_corr = err;
		ec = UBI_SCAN_UNKNOWN_EC;
		bitflips = 1;
	}

	if (!ec_corr) {
		int image_seq;

		/* Make sure UBI version is OK */
		if (ech->version != UBI_VERSION) {
			ubi_err("this UBI version is %d, image version is %d",
				UBI_VERSION, (int)ech->version);
			return -EINVAL;
		}

		ec = be64_to_cpu(ech->ec);
		if (ec > UBI_MAX_ERASECOUNTER) {
			/*
			 * Erase counter overflow. The EC headers have 64 bits
			 * reserved, but we anyway make use of only 31 bit
			 * values, as this seems to be enough for any existing
			 * flash. Upgrade UBI and use 64-bit erase counters
			 * internally.
			 */
			ubi_err("erase counter overflow, max is %d",
				UBI_MAX_ERASECOUNTER);
			ubi_dbg_dump_ec_hdr(ech);
			return -EINVAL;
		}

		/*
		 * Make sure that all PEBs have the same image sequence number.
		 * This allows us to detect situations when users flash UBI
		 * images incorrectly, so that the flash has the new UBI image
		 * and leftovers from the old one. This feature was added
		 * relatively recently, and the sequence number was always
		 * zero, because old UBI implementations always set it to zero.
		 * For this reasons, we do not panic if some PEBs have zero
		 * sequence number, while other PEBs have non-zero sequence
		 * number.
		 */
		image_seq = be32_to_cpu(ech->image_seq);
		if (!ubi->image_seq && image_seq)
			ubi->image_seq = image_seq;
		if (ubi->image_seq && image_seq &&
		    ubi->image_seq != image_seq) {
			ubi_err("bad image sequence number %d in PEB %d, "
				"expected %d", image_seq, pnum, ubi->image_seq);
			ubi_dbg_dump_ec_hdr(ech);
			return -EINVAL;
		}
	}

	/* OK, we've done with the EC header, let's look at the VID header */

	err = ubi_io_read_vid_hdr(ubi, pnum, vidh, 0);
	if (err < 0)
		return err;
	else if (err == UBI_IO_BITFLIPS)
		bitflips = 1;
	else if (err == UBI_IO_BAD_HDR_READ || err == UBI_IO_BAD_HDR ||
		 (err == UBI_IO_PEB_FREE && ec_corr)) {
		/* VID header is corrupted */
		if (err == UBI_IO_BAD_HDR_READ ||
		    ec_corr == UBI_IO_BAD_HDR_READ)
			si->read_err_count += 1;
		err = add_to_list(si, pnum, ec, &si->corr);
		if (err)
			return err;
		goto adjust_mean_ec;
	} else if (err == UBI_IO_PEB_FREE) {
		/* No VID header - the physical eraseblock is free */
		err = add_to_list(si, pnum, ec, &si->free);
		if (err)
			return err;
		goto adjust_mean_ec;
	}

	vol_id = be32_to_cpu(vidh->vol_id);
	if (vol_id > UBI_MAX_VOLUMES && vol_id != UBI_LAYOUT_VOLUME_ID) {
		int lnum = be32_to_cpu(vidh->lnum);

		/* Unsupported internal volume */
		switch (vidh->compat) {
		case UBI_COMPAT_DELETE:
			ubi_msg("\"delete\" compatible internal volume %d:%d"
				" found, will remove it", vol_id, lnum);
			err = add_to_list(si, pnum, ec, &si->erase);
			if (err)
				return err;
			return 0;

		case UBI_COMPAT_RO:
			ubi_msg("read-only compatible internal volume %d:%d"
				" found, switch to read-only mode",
				vol_id, lnum);
			ubi->ro_mode = 1;
			break;

		case UBI_COMPAT_PRESERVE:
			ubi_msg("\"preserve\" compatible internal volume %d:%d"
				" found", vol_id, lnum);
			err = add_to_list(si, pnum, ec, &si->alien);
			if (err)
				return err;
			return 0;

		case UBI_COMPAT_REJECT:
			ubi_err("incompatible internal volume %d:%d found",
				vol_id, lnum);
			return -EINVAL;
		}
	}

	if (ec_corr)
		ubi_warn("valid VID header but corrupted EC header at PEB %d",
			 pnum);
	err = ubi_scan_add_used(ubi, si, pnum, ec, vidh, bitflips);
	if (err)
		return err;

adjust_mean_ec:
	if (!ec_corr) {
		si->ec_sum += ec;
		si->ec_count += 1;
		if (ec > si->max_ec)
			si->max_ec = ec;
		if (ec < si->min_ec)
			si->min_ec = ec;
	}

	return 0;
}

/**
 * check_what_we_have - check what PEB were found by scanning.
 * @ubi: UBI device description object
 * @si: scanning information
 *
 * This is a helper function which takes a look what PEBs were found by
 * scanning, and decides whether the flash is empty and should be formatted and
 * whether there are too many corrupted PEBs and we should not attach this
 * MTD device. Returns zero if we should proceed with attaching the MTD device,
 * and %-EINVAL if we should not.
 */
static int check_what_we_have(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	struct ubi_scan_leb *seb;
	int max_corr;

	max_corr = ubi->peb_count - si->bad_peb_count - si->alien_peb_count;
	max_corr = max_corr / 20 ?: 8;

	/*
	 * Few corrupted PEBs are not a problem and may be just a result of
	 * unclean reboots. However, many of them may indicate some problems
	 * with the flash HW or driver.
	 */
	if (si->corr_peb_count >= 8) {
		ubi_warn("%d PEBs are corrupted", si->corr_peb_count);
		printk(KERN_WARNING "corrupted PEBs are:");
		list_for_each_entry(seb, &si->corr, u.list)
			printk(KERN_CONT " %d", seb->pnum);
		printk(KERN_CONT "\n");

		/*
		 * If too many PEBs are corrupted, we refuse attaching,
		 * otherwise, only print a warning.
		 */
		if (si->corr_peb_count >= max_corr) {
			ubi_err("too many corrupted PEBs, refusing this device");
			return -EINVAL;
		}
	}

	if (si->free_peb_count + si->used_peb_count +
	    si->alien_peb_count == 0) {
		/* No UBI-formatted eraseblocks were found */
		if (si->corr_peb_count == si->read_err_count &&
		    si->corr_peb_count < 8) {
			/* No or just few corrupted PEBs, and all of them had a
			 * read error. We assume that those are bad PEBs, which
			 * were just not marked as bad so far.
			 *
			 * This piece of code basically tries to distinguish
			 * between the following 2 situations:
			 *
			 * 1. Flash is empty, but there are few bad PEBs, which
			 *    are not marked as bad so far, and which were read
			 *    with error. We want to go ahead and format this
			 *    flash. While formating, the faulty PEBs will
			 *    probably be marked as bad.
			 *
			 * 2. Flash probably contains non-UBI data and we do
			 * not want to format it and destroy possibly needed
			 * data (e.g., consider the case when the bootloader
			 * MTD partition was accidentally fed to UBI).
			 */
			si->is_empty = 1;
			ubi_msg("empty MTD device detected");
			get_random_bytes(&ubi->image_seq, sizeof(ubi->image_seq));
		} else {
			ubi_err("MTD device possibly contains non-UBI data, "
				"refusing it");
			return -EINVAL;
		}
	}

	if (si->corr_peb_count > 0)
		ubi_msg("corrupted PEBs will be formatted");
	return 0;
}

/**
 * ubi_scan - scan an MTD device.
 * @ubi: UBI device description object
 *
 * This function does full scanning of an MTD device and returns complete
 * information about it. In case of failure, an error code is returned.
 */
struct ubi_scan_info *ubi_scan(struct ubi_device *ubi)
{
	int err, pnum;
	struct rb_node *rb1, *rb2;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb;
	struct ubi_scan_info *si;

	si = kzalloc(sizeof(struct ubi_scan_info), GFP_KERNEL);
	if (!si)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&si->corr);
	INIT_LIST_HEAD(&si->free);
	INIT_LIST_HEAD(&si->erase);
	INIT_LIST_HEAD(&si->alien);
	si->volumes = RB_ROOT;

	err = -ENOMEM;
	ech = kzalloc(ubi->ec_hdr_alsize, GFP_KERNEL);
	if (!ech)
		goto out_si;

	vidh = ubi_zalloc_vid_hdr(ubi, GFP_KERNEL);
	if (!vidh)
		goto out_ech;

	for (pnum = 0; pnum < ubi->peb_count; pnum++) {
		cond_resched();

		dbg_gen("process PEB %d", pnum);
		err = process_eb(ubi, si, pnum);
		if (err < 0)
			goto out_vidh;
	}

	dbg_msg("scanning is finished");

	/* Calculate mean erase counter */
	if (si->ec_count)
		si->mean_ec = div_u64(si->ec_sum, si->ec_count);

	err = check_what_we_have(ubi, si);
	if (err)
		goto out_vidh;

	/*
	 * In case of unknown erase counter we use the mean erase counter
	 * value.
	 */
	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb)
			if (seb->ec == UBI_SCAN_UNKNOWN_EC)
				seb->ec = si->mean_ec;
	}

	list_for_each_entry(seb, &si->free, u.list) {
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;
	}

	list_for_each_entry(seb, &si->corr, u.list)
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;

	list_for_each_entry(seb, &si->erase, u.list)
		if (seb->ec == UBI_SCAN_UNKNOWN_EC)
			seb->ec = si->mean_ec;

	err = paranoid_check_si(ubi, si);
	if (err)
		goto out_vidh;

	ubi_free_vid_hdr(ubi, vidh);
	kfree(ech);

	return si;

out_vidh:
	ubi_free_vid_hdr(ubi, vidh);
out_ech:
	kfree(ech);
out_si:
	ubi_scan_destroy_si(si);
	return ERR_PTR(err);
}

/**
 * destroy_sv - free the scanning volume information
 * @sv: scanning volume information
 *
 * This function destroys the volume RB-tree (@sv->root) and the scanning
 * volume information.
 */
static void destroy_sv(struct ubi_scan_volume *sv)
{
	struct ubi_scan_leb *seb;
	struct rb_node *this = sv->root.rb_node;

	while (this) {
		if (this->rb_left)
			this = this->rb_left;
		else if (this->rb_right)
			this = this->rb_right;
		else {
			seb = rb_entry(this, struct ubi_scan_leb, u.rb);
			this = rb_parent(this);
			if (this) {
				if (this->rb_left == &seb->u.rb)
					this->rb_left = NULL;
				else
					this->rb_right = NULL;
			}

			kfree(seb);
		}
	}
	kfree(sv);
}

/**
 * ubi_scan_destroy_si - destroy scanning information.
 * @si: scanning information
 */
void ubi_scan_destroy_si(struct ubi_scan_info *si)
{
	struct ubi_scan_leb *seb, *seb_tmp;
	struct ubi_scan_volume *sv;
	struct rb_node *rb;

	list_for_each_entry_safe(seb, seb_tmp, &si->alien, u.list) {
		list_del(&seb->u.list);
		kfree(seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->erase, u.list) {
		list_del(&seb->u.list);
		kfree(seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->corr, u.list) {
		list_del(&seb->u.list);
		kfree(seb);
	}
	list_for_each_entry_safe(seb, seb_tmp, &si->free, u.list) {
		list_del(&seb->u.list);
		kfree(seb);
	}

	/* Destroy the volume RB-tree */
	rb = si->volumes.rb_node;
	while (rb) {
		if (rb->rb_left)
			rb = rb->rb_left;
		else if (rb->rb_right)
			rb = rb->rb_right;
		else {
			sv = rb_entry(rb, struct ubi_scan_volume, rb);

			rb = rb_parent(rb);
			if (rb) {
				if (rb->rb_left == &sv->rb)
					rb->rb_left = NULL;
				else
					rb->rb_right = NULL;
			}

			destroy_sv(sv);
		}
	}

	kfree(si);
}

#ifdef CONFIG_MTD_UBI_DEBUG_PARANOID

/**
 * paranoid_check_si - check the scanning information.
 * @ubi: UBI device description object
 * @si: scanning information
 *
 * This function returns zero if the scanning information is all right, and a
 * negative error code if not or if an error occurred.
 */
static int paranoid_check_si(struct ubi_device *ubi, struct ubi_scan_info *si)
{
	int pnum, err, vols_found = 0;
	struct rb_node *rb1, *rb2;
	struct ubi_scan_volume *sv;
	struct ubi_scan_leb *seb, *last_seb;
	uint8_t *buf;

	/*
	 * At first, check that scanning information is OK.
	 */
	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		int leb_count = 0;

		cond_resched();

		vols_found += 1;

		if (si->is_empty) {
			ubi_err("bad is_empty flag");
			goto bad_sv;
		}

		if (sv->vol_id < 0 || sv->highest_lnum < 0 ||
		    sv->leb_count < 0 || sv->vol_type < 0 || sv->used_ebs < 0 ||
		    sv->data_pad < 0 || sv->last_data_size < 0) {
			ubi_err("negative values");
			goto bad_sv;
		}

		if (sv->vol_id >= UBI_MAX_VOLUMES &&
		    sv->vol_id < UBI_INTERNAL_VOL_START) {
			ubi_err("bad vol_id");
			goto bad_sv;
		}

		if (sv->vol_id > si->highest_vol_id) {
			ubi_err("highest_vol_id is %d, but vol_id %d is there",
				si->highest_vol_id, sv->vol_id);
			goto out;
		}

		if (sv->vol_type != UBI_DYNAMIC_VOLUME &&
		    sv->vol_type != UBI_STATIC_VOLUME) {
			ubi_err("bad vol_type");
			goto bad_sv;
		}

		if (sv->data_pad > ubi->leb_size / 2) {
			ubi_err("bad data_pad");
			goto bad_sv;
		}

		last_seb = NULL;
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb) {
			cond_resched();

			last_seb = seb;
			leb_count += 1;

			if (seb->pnum < 0 || seb->ec < 0) {
				ubi_err("negative values");
				goto bad_seb;
			}

			if (seb->ec < si->min_ec) {
				ubi_err("bad si->min_ec (%d), %d found",
					si->min_ec, seb->ec);
				goto bad_seb;
			}

			if (seb->ec > si->max_ec) {
				ubi_err("bad si->max_ec (%d), %d found",
					si->max_ec, seb->ec);
				goto bad_seb;
			}

			if (seb->pnum >= ubi->peb_count) {
				ubi_err("too high PEB number %d, total PEBs %d",
					seb->pnum, ubi->peb_count);
				goto bad_seb;
			}

			if (sv->vol_type == UBI_STATIC_VOLUME) {
				if (seb->lnum >= sv->used_ebs) {
					ubi_err("bad lnum or used_ebs");
					goto bad_seb;
				}
			} else {
				if (sv->used_ebs != 0) {
					ubi_err("non-zero used_ebs");
					goto bad_seb;
				}
			}

			if (seb->lnum > sv->highest_lnum) {
				ubi_err("incorrect highest_lnum or lnum");
				goto bad_seb;
			}
		}

		if (sv->leb_count != leb_count) {
			ubi_err("bad leb_count, %d objects in the tree",
				leb_count);
			goto bad_sv;
		}

		if (!last_seb)
			continue;

		seb = last_seb;

		if (seb->lnum != sv->highest_lnum) {
			ubi_err("bad highest_lnum");
			goto bad_seb;
		}
	}

	if (vols_found != si->vols_found) {
		ubi_err("bad si->vols_found %d, should be %d",
			si->vols_found, vols_found);
		goto out;
	}

	/* Check that scanning information is correct */
	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb) {
		last_seb = NULL;
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb) {
			int vol_type;

			cond_resched();

			last_seb = seb;

			err = ubi_io_read_vid_hdr(ubi, seb->pnum, vidh, 1);
			if (err && err != UBI_IO_BITFLIPS) {
				ubi_err("VID header is not OK (%d)", err);
				if (err > 0)
					err = -EIO;
				return err;
			}

			vol_type = vidh->vol_type == UBI_VID_DYNAMIC ?
				   UBI_DYNAMIC_VOLUME : UBI_STATIC_VOLUME;
			if (sv->vol_type != vol_type) {
				ubi_err("bad vol_type");
				goto bad_vid_hdr;
			}

			if (seb->sqnum != be64_to_cpu(vidh->sqnum)) {
				ubi_err("bad sqnum %llu", seb->sqnum);
				goto bad_vid_hdr;
			}

			if (sv->vol_id != be32_to_cpu(vidh->vol_id)) {
				ubi_err("bad vol_id %d", sv->vol_id);
				goto bad_vid_hdr;
			}

			if (sv->compat != vidh->compat) {
				ubi_err("bad compat %d", vidh->compat);
				goto bad_vid_hdr;
			}

			if (seb->lnum != be32_to_cpu(vidh->lnum)) {
				ubi_err("bad lnum %d", seb->lnum);
				goto bad_vid_hdr;
			}

			if (sv->used_ebs != be32_to_cpu(vidh->used_ebs)) {
				ubi_err("bad used_ebs %d", sv->used_ebs);
				goto bad_vid_hdr;
			}

			if (sv->data_pad != be32_to_cpu(vidh->data_pad)) {
				ubi_err("bad data_pad %d", sv->data_pad);
				goto bad_vid_hdr;
			}
		}

		if (!last_seb)
			continue;

		if (sv->highest_lnum != be32_to_cpu(vidh->lnum)) {
			ubi_err("bad highest_lnum %d", sv->highest_lnum);
			goto bad_vid_hdr;
		}

		if (sv->last_data_size != be32_to_cpu(vidh->data_size)) {
			ubi_err("bad last_data_size %d", sv->last_data_size);
			goto bad_vid_hdr;
		}
	}

	/*
	 * Make sure that all the physical eraseblocks are in one of the lists
	 * or trees.
	 */
	buf = kzalloc(ubi->peb_count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (pnum = 0; pnum < ubi->peb_count; pnum++) {
		err = ubi_io_is_bad(ubi, pnum);
		if (err < 0) {
			kfree(buf);
			return err;
		} else if (err)
			buf[pnum] = 1;
	}

	ubi_rb_for_each_entry(rb1, sv, &si->volumes, rb)
		ubi_rb_for_each_entry(rb2, seb, &sv->root, u.rb)
			buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->free, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->corr, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->erase, u.list)
		buf[seb->pnum] = 1;

	list_for_each_entry(seb, &si->alien, u.list)
		buf[seb->pnum] = 1;

	err = 0;
	for (pnum = 0; pnum < ubi->peb_count; pnum++)
		if (!buf[pnum]) {
			ubi_err("PEB %d is not referred", pnum);
			err = 1;
		}

	kfree(buf);
	if (err)
		goto out;
	return 0;

bad_seb:
	ubi_err("bad scanning information about LEB %d", seb->lnum);
	ubi_dbg_dump_seb(seb, 0);
	ubi_dbg_dump_sv(sv);
	goto out;

bad_sv:
	ubi_err("bad scanning information about volume %d", sv->vol_id);
	ubi_dbg_dump_sv(sv);
	goto out;

bad_vid_hdr:
	ubi_err("bad scanning information about volume %d", sv->vol_id);
	ubi_dbg_dump_sv(sv);
	ubi_dbg_dump_vid_hdr(vidh);

out:
	ubi_dbg_dump_stack();
	return -EINVAL;
}

#endif /* CONFIG_MTD_UBI_DEBUG_PARANOID */
