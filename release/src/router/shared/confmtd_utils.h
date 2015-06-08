/*
 * Shell-like utility functions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: $
 */

#ifndef _CONFMTD_UTILS_H_
#define _CONFMTD_UTILS_H_


/* CONFMTD ramfs directories */
#define RAMFS_CONFMTD_DIR		"/tmp/confmtd"
#define NAND_DIR			"/tmp/media/nand"
#define NAND_FILE			NAND_DIR"/config.tgz"
#define CONFMTD_TGZ_TMP_FILE		"/tmp/config.tgz"

/* CONFMTD partition magic number: "confmtd" */
#define CONFMTD_MAGIC			"confmtd"

typedef struct {
	char magic[8];
	unsigned int len;
	unsigned short checksum;
} confmtd_hdr_t;

/*
 * Write a file to an MTD device
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int confmtd_backup();

/*
 * Read MTD device to file
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int confmtd_restore();

#endif /* _CONFMTD_UTILS_H_ */
