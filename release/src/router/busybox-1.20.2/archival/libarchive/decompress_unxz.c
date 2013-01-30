/*
 * This file uses XZ Embedded library code which is written
 * by Lasse Collin <lasse.collin@tukaani.org>
 * and Igor Pavlov <http://7-zip.org/>
 *
 * See README file in unxz/ directory for more information.
 *
 * This file is:
 * Copyright (C) 2010 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

#define XZ_FUNC FAST_FUNC
#define XZ_EXTERN static

#define XZ_DEC_DYNALLOC

/* Skip check (rather than fail) of unsupported hash functions */
#define XZ_DEC_ANY_CHECK  1

/* We use our own crc32 function */
#define XZ_INTERNAL_CRC32 0
static uint32_t xz_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
	return ~crc32_block_endian0(~crc, buf, size, global_crc32_table);
}

/* We use arch-optimized unaligned accessors */
#define get_unaligned_le32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_LE32(v); })
#define get_unaligned_be32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_BE32(v); })
#define put_unaligned_le32(val, buf) move_to_unaligned16(buf, SWAP_LE32(val))
#define put_unaligned_be32(val, buf) move_to_unaligned16(buf, SWAP_BE32(val))

#include "unxz/xz_dec_bcj.c"
#include "unxz/xz_dec_lzma2.c"
#include "unxz/xz_dec_stream.c"

IF_DESKTOP(long long) int FAST_FUNC
unpack_xz_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd)
{
	struct xz_buf iobuf;
	struct xz_dec *state;
	unsigned char *membuf;
	IF_DESKTOP(long long) int total = 0;

	if (!global_crc32_table)
		global_crc32_table = crc32_filltable(NULL, /*endian:*/ 0);

	memset(&iobuf, 0, sizeof(iobuf));
	membuf = xmalloc(2 * BUFSIZ);
	iobuf.in = membuf;
	iobuf.out = membuf + BUFSIZ;
	iobuf.out_size = BUFSIZ;

	if (!aux || aux->check_signature == 0) {
		/* Preload XZ file signature */
		strcpy((char*)membuf, HEADER_MAGIC);
		iobuf.in_size = HEADER_MAGIC_SIZE;
	} /* else: let xz code read & check it */

	/* Limit memory usage to about 64 MiB. */
	state = xz_dec_init(XZ_DYNALLOC, 64*1024*1024);

	while (1) {
		enum xz_ret r;

		if (iobuf.in_pos == iobuf.in_size) {
			int rd = safe_read(src_fd, membuf, BUFSIZ);
			if (rd < 0) {
				bb_error_msg(bb_msg_read_error);
				total = -1;
				break;
			}
			iobuf.in_size = rd;
			iobuf.in_pos = 0;
		}
//		bb_error_msg(">in pos:%d size:%d out pos:%d size:%d",
//				iobuf.in_pos, iobuf.in_size, iobuf.out_pos, iobuf.out_size);
		r = xz_dec_run(state, &iobuf);
//		bb_error_msg("<in pos:%d size:%d out pos:%d size:%d r:%d",
//				iobuf.in_pos, iobuf.in_size, iobuf.out_pos, iobuf.out_size, r);
		if (iobuf.out_pos) {
			xwrite(dst_fd, iobuf.out, iobuf.out_pos);
			IF_DESKTOP(total += iobuf.out_pos;)
			iobuf.out_pos = 0;
		}
		if (r == XZ_STREAM_END) {
			break;
		}
		if (r != XZ_OK && r != XZ_UNSUPPORTED_CHECK) {
			bb_error_msg("corrupted data");
			total = -1;
			break;
		}
	}
	xz_dec_end(state);
	free(membuf);

	return total;
}
