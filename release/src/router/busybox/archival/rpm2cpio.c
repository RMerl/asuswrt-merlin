/* vi: set sw=4 ts=4: */
/*
 * Mini rpm2cpio implementation for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "unarchive.h"
#include "rpm.h"

enum { rpm_fd = STDIN_FILENO };

static unsigned skip_header(void)
{
	struct rpm_header header;
	unsigned len;

	xread(rpm_fd, &header, sizeof(header));
//	if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC_STR, 3) != 0) {
//		bb_error_msg_and_die("invalid RPM header magic");
//	}
//	if (header.version != 1) {
//		bb_error_msg_and_die("unsupported RPM header version");
//	}
	if (header.magic_and_ver != htonl(RPM_HEADER_MAGICnVER)) {
		bb_error_msg_and_die("invalid RPM header magic or unsupported version");
		// ": %x != %x", header.magic_and_ver, htonl(RPM_HEADER_MAGICnVER));
	}

	/* Seek past index entries, and past store */
	len = 16 * ntohl(header.entries) + ntohl(header.size);
	seek_by_jump(rpm_fd, len);

	return sizeof(header) + len;
}

/* No getopt required */
int rpm2cpio_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rpm2cpio_main(int argc UNUSED_PARAM, char **argv)
{
	struct rpm_lead lead;
	unsigned pos;

	if (argv[1]) {
		xmove_fd(xopen(argv[1], O_RDONLY), rpm_fd);
	}
	xread(rpm_fd, &lead, sizeof(lead));

	/* Just check the magic, the rest is irrelevant */
	if (lead.magic != htonl(RPM_LEAD_MAGIC)) {
		bb_error_msg_and_die("invalid RPM magic");
	}

	/* Skip the signature header, align to 8 bytes */
	pos = skip_header();
	seek_by_jump(rpm_fd, (-(int)pos) & 7);

	/* Skip the main header */
	skip_header();

#if 0
	/* This works, but doesn't report uncompress errors (they happen in child) */
	setup_unzip_on_fd(rpm_fd /*fail_if_not_detected: 1*/);
	if (bb_copyfd_eof(rpm_fd, STDOUT_FILENO) < 0)
		bb_error_msg_and_die("error unpacking");
#else
	/* BLOAT */
	{
		union {
			uint8_t b[4];
			uint16_t b16[2];
			uint32_t b32[1];
		} magic;
		IF_DESKTOP(long long) int FAST_FUNC (*unpack)(int src_fd, int dst_fd);

		xread(rpm_fd, magic.b16, sizeof(magic.b16[0]));
		if (magic.b16[0] == GZIP_MAGIC) {
			unpack = unpack_gz_stream;
		} else
		if (ENABLE_FEATURE_SEAMLESS_BZ2
		 && magic.b16[0] == BZIP2_MAGIC
		) {
			unpack = unpack_bz2_stream;
		} else
		if (ENABLE_FEATURE_SEAMLESS_XZ
		 && magic.b16[0] == XZ_MAGIC1
		) {
			xread(rpm_fd, magic.b32, sizeof(magic.b32[0]));
			if (magic.b32[0] != XZ_MAGIC2)
				goto no_magic;
			/* unpack_xz_stream wants fd at position 6, no need to seek */
			//xlseek(rpm_fd, -6, SEEK_CUR);
			unpack = unpack_xz_stream;
		} else {
 no_magic:
			bb_error_msg_and_die("no gzip"
					IF_FEATURE_SEAMLESS_BZ2("/bzip2")
					IF_FEATURE_SEAMLESS_XZ("/xz")
					" magic");
		}
		if (unpack(rpm_fd, STDOUT_FILENO) < 0)
			bb_error_msg_and_die("error unpacking");
	}
#endif

	if (ENABLE_FEATURE_CLEAN_UP) {
		close(rpm_fd);
	}

	return 0;
}
