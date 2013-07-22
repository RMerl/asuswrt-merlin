/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2008 Nokia Corporation
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Generating UBI images.
 *
 * Authors: Oliver Lohmann
 *          Artem Bityutskiy
 */

#define PROGRAM_NAME "libubigen"

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <mtd/ubi-media.h>
#include <mtd_swab.h>
#include <libubigen.h>
#include <crc32.h>
#include "common.h"

void ubigen_info_init(struct ubigen_info *ui, int peb_size, int min_io_size,
		      int subpage_size, int vid_hdr_offs, int ubi_ver,
		      uint32_t image_seq)
{
	if (!vid_hdr_offs) {
		vid_hdr_offs = UBI_EC_HDR_SIZE + subpage_size - 1;
		vid_hdr_offs /= subpage_size;
		vid_hdr_offs *= subpage_size;
	}

	ui->peb_size = peb_size;
	ui->min_io_size = min_io_size;
	ui->vid_hdr_offs = vid_hdr_offs;
	ui->data_offs = vid_hdr_offs + UBI_VID_HDR_SIZE + min_io_size - 1;
	ui->data_offs /= min_io_size;
	ui->data_offs *= min_io_size;
	ui->leb_size = peb_size - ui->data_offs;
	ui->ubi_ver = ubi_ver;
	ui->image_seq = image_seq;

	ui->max_volumes = ui->leb_size / UBI_VTBL_RECORD_SIZE;
	if (ui->max_volumes > UBI_MAX_VOLUMES)
		ui->max_volumes = UBI_MAX_VOLUMES;
	ui->vtbl_size = ui->max_volumes * UBI_VTBL_RECORD_SIZE;
}

struct ubi_vtbl_record *ubigen_create_empty_vtbl(const struct ubigen_info *ui)
{
	struct ubi_vtbl_record *vtbl;
	int i;

	vtbl = calloc(1, ui->vtbl_size);
	if (!vtbl) {
		sys_errmsg("cannot allocate %d bytes of memory", ui->vtbl_size);
		return NULL;
	}

	for (i = 0; i < ui->max_volumes; i++) {
		uint32_t crc = mtd_crc32(UBI_CRC32_INIT, &vtbl[i],
				     UBI_VTBL_RECORD_SIZE_CRC);
		vtbl[i].crc = cpu_to_be32(crc);
	}

	return vtbl;
}

int ubigen_add_volume(const struct ubigen_info *ui,
		      const struct ubigen_vol_info *vi,
		      struct ubi_vtbl_record *vtbl)
{
	struct ubi_vtbl_record *vtbl_rec = &vtbl[vi->id];
	uint32_t tmp;

	if (vi->id >= ui->max_volumes) {
		errmsg("too high volume id %d, max. volumes is %d",
		       vi->id, ui->max_volumes);
		errno = EINVAL;
		return -1;
	}

	if (vi->alignment >= ui->leb_size) {
		errmsg("too large alignment %d, max is %d (LEB size)",
		       vi->alignment, ui->leb_size);
		errno = EINVAL;
		return -1;
	}

	memset(vtbl_rec, 0, sizeof(struct ubi_vtbl_record));
	tmp = (vi->bytes + ui->leb_size - 1) / ui->leb_size;
	vtbl_rec->reserved_pebs = cpu_to_be32(tmp);
	vtbl_rec->alignment = cpu_to_be32(vi->alignment);
	vtbl_rec->vol_type = vi->type;
	tmp = ui->leb_size % vi->alignment;
	vtbl_rec->data_pad = cpu_to_be32(tmp);
	vtbl_rec->flags = vi->flags;

	memcpy(vtbl_rec->name, vi->name, vi->name_len);
	vtbl_rec->name[vi->name_len] = '\0';
	vtbl_rec->name_len = cpu_to_be16(vi->name_len);

	tmp = mtd_crc32(UBI_CRC32_INIT, vtbl_rec, UBI_VTBL_RECORD_SIZE_CRC);
	vtbl_rec->crc =	 cpu_to_be32(tmp);
	return 0;
}

void ubigen_init_ec_hdr(const struct ubigen_info *ui,
		        struct ubi_ec_hdr *hdr, long long ec)
{
	uint32_t crc;

	memset(hdr, 0, sizeof(struct ubi_ec_hdr));

	hdr->magic = cpu_to_be32(UBI_EC_HDR_MAGIC);
	hdr->version = ui->ubi_ver;
	hdr->ec = cpu_to_be64(ec);
	hdr->vid_hdr_offset = cpu_to_be32(ui->vid_hdr_offs);
	hdr->data_offset = cpu_to_be32(ui->data_offs);
	hdr->image_seq = cpu_to_be32(ui->image_seq);

	crc = mtd_crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
	hdr->hdr_crc = cpu_to_be32(crc);
}

void ubigen_init_vid_hdr(const struct ubigen_info *ui,
			 const struct ubigen_vol_info *vi,
			 struct ubi_vid_hdr *hdr, int lnum,
			 const void *data, int data_size)
{
	uint32_t crc;

	memset(hdr, 0, sizeof(struct ubi_vid_hdr));

	hdr->magic = cpu_to_be32(UBI_VID_HDR_MAGIC);
	hdr->version = ui->ubi_ver;
	hdr->vol_type = vi->type;
	hdr->vol_id = cpu_to_be32(vi->id);
	hdr->lnum = cpu_to_be32(lnum);
	hdr->data_pad = cpu_to_be32(vi->data_pad);
	hdr->compat = vi->compat;

	if (vi->type == UBI_VID_STATIC) {
		hdr->data_size = cpu_to_be32(data_size);
		hdr->used_ebs = cpu_to_be32(vi->used_ebs);
		crc = mtd_crc32(UBI_CRC32_INIT, data, data_size);
		hdr->data_crc = cpu_to_be32(crc);
	}

	crc = mtd_crc32(UBI_CRC32_INIT, hdr, UBI_VID_HDR_SIZE_CRC);
	hdr->hdr_crc = cpu_to_be32(crc);
}

int ubigen_write_volume(const struct ubigen_info *ui,
			const struct ubigen_vol_info *vi, long long ec,
			long long bytes, int in, int out)
{
	int len = vi->usable_leb_size, rd, lnum = 0;
	char *inbuf, *outbuf;

	if (vi->id >= ui->max_volumes) {
		errmsg("too high volume id %d, max. volumes is %d",
		       vi->id, ui->max_volumes);
		errno = EINVAL;
		return -1;
	}

	if (vi->alignment >= ui->leb_size) {
		errmsg("too large alignment %d, max is %d (LEB size)",
		       vi->alignment, ui->leb_size);
		errno = EINVAL;
		return -1;
	}

	inbuf = malloc(ui->leb_size);
	if (!inbuf)
		return sys_errmsg("cannot allocate %d bytes of memory",
				  ui->leb_size);
	outbuf = malloc(ui->peb_size);
	if (!outbuf) {
		sys_errmsg("cannot allocate %d bytes of memory", ui->peb_size);
		goto out_free;
	}

	memset(outbuf, 0xFF, ui->data_offs);
	ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf, ec);

	while (bytes) {
		int l;
		struct ubi_vid_hdr *vid_hdr;

		if (bytes < len)
			len = bytes;
		bytes -= len;

		l = len;
		do {
			rd = read(in, inbuf + len - l, l);
			if (rd != l) {
				sys_errmsg("cannot read %d bytes from the input file", l);
				goto out_free1;
			}

			l -= rd;
		} while (l);

		vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
		ubigen_init_vid_hdr(ui, vi, vid_hdr, lnum, inbuf, len);

		memcpy(outbuf + ui->data_offs, inbuf, len);
		memset(outbuf + ui->data_offs + len, 0xFF,
		       ui->peb_size - ui->data_offs - len);

		if (write(out, outbuf, ui->peb_size) != ui->peb_size) {
			sys_errmsg("cannot write %d bytes to the output file", ui->peb_size);
			goto out_free1;
		}

		lnum += 1;
	}

	free(outbuf);
	free(inbuf);
	return 0;

out_free1:
	free(outbuf);
out_free:
	free(inbuf);
	return -1;
}

int ubigen_write_layout_vol(const struct ubigen_info *ui, int peb1, int peb2,
			    long long ec1, long long ec2,
			    struct ubi_vtbl_record *vtbl, int fd)
{
	int ret;
	struct ubigen_vol_info vi;
	char *outbuf;
	struct ubi_vid_hdr *vid_hdr;
	off_t seek;

	vi.bytes = ui->leb_size * UBI_LAYOUT_VOLUME_EBS;
	vi.id = UBI_LAYOUT_VOLUME_ID;
	vi.alignment = UBI_LAYOUT_VOLUME_ALIGN;
	vi.data_pad = ui->leb_size % UBI_LAYOUT_VOLUME_ALIGN;
	vi.usable_leb_size = ui->leb_size - vi.data_pad;
	vi.data_pad = ui->leb_size - vi.usable_leb_size;
	vi.type = UBI_LAYOUT_VOLUME_TYPE;
	vi.name = UBI_LAYOUT_VOLUME_NAME;
	vi.name_len = strlen(UBI_LAYOUT_VOLUME_NAME);
	vi.compat = UBI_LAYOUT_VOLUME_COMPAT;

	outbuf = malloc(ui->peb_size);
	if (!outbuf)
		return sys_errmsg("failed to allocate %d bytes",
				  ui->peb_size);

	memset(outbuf, 0xFF, ui->data_offs);
	vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
	memcpy(outbuf + ui->data_offs, vtbl, ui->vtbl_size);
	memset(outbuf + ui->data_offs + ui->vtbl_size, 0xFF,
	       ui->peb_size - ui->data_offs - ui->vtbl_size);

	seek = peb1 * ui->peb_size;
	if (lseek(fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek output file");
		goto out_free;
	}

	ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf, ec1);
	ubigen_init_vid_hdr(ui, &vi, vid_hdr, 0, NULL, 0);
	ret = write(fd, outbuf, ui->peb_size);
	if (ret != ui->peb_size) {
		sys_errmsg("cannot write %d bytes", ui->peb_size);
		goto out_free;
	}

	seek = peb2 * ui->peb_size;
	if (lseek(fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek output file");
		goto out_free;
	}
	ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf, ec2);
	ubigen_init_vid_hdr(ui, &vi, vid_hdr, 1, NULL, 0);
	ret = write(fd, outbuf, ui->peb_size);
	if (ret != ui->peb_size) {
		sys_errmsg("cannot write %d bytes", ui->peb_size);
		goto out_free;
	}

	free(outbuf);
	return 0;

out_free:
	free(outbuf);
	return -1;
}
