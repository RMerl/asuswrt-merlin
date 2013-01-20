/* vi: set sw=4 ts=4: */
/* Copyright 2001 Glenn McGrath.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "bb_archive.h"
#include "ar.h"

static unsigned read_num(const char *str, int base)
{
	/* This code works because
	 * on misformatted numbers bb_strtou returns all-ones */
	int err = bb_strtou(str, NULL, base);
	if (err == -1)
		bb_error_msg_and_die("invalid ar header");
	return err;
}

char FAST_FUNC get_header_ar(archive_handle_t *archive_handle)
{
	file_header_t *typed = archive_handle->file_header;
	unsigned size;
	union {
		char raw[60];
		struct ar_header formatted;
	} ar;
#if ENABLE_FEATURE_AR_LONG_FILENAMES
	static char *ar_long_names;
	static unsigned ar_long_name_size;
#endif

	/* dont use xread as we want to handle the error ourself */
	if (read(archive_handle->src_fd, ar.raw, 60) != 60) {
		/* End Of File */
		return EXIT_FAILURE;
	}

	/* ar header starts on an even byte (2 byte aligned)
	 * '\n' is used for padding
	 */
	if (ar.raw[0] == '\n') {
		/* fix up the header, we started reading 1 byte too early */
		memmove(ar.raw, &ar.raw[1], 59);
		ar.raw[59] = xread_char(archive_handle->src_fd);
		archive_handle->offset++;
	}
	archive_handle->offset += 60;

	if (ar.formatted.magic[0] != '`' || ar.formatted.magic[1] != '\n')
		bb_error_msg_and_die("invalid ar header");

	/* FIXME: more thorough routine would be in order here
	 * (we have something like that in tar)
	 * but for now we are lax. */
	ar.formatted.magic[0] = '\0'; /* else 4G-2 file will have size="4294967294`\n..." */
	typed->size = size = read_num(ar.formatted.size, 10);

	/* special filenames have '/' as the first character */
	if (ar.formatted.name[0] == '/') {
		if (ar.formatted.name[1] == ' ') {
			/* This is the index of symbols in the file for compilers */
			data_skip(archive_handle);
			archive_handle->offset += size;
			return get_header_ar(archive_handle); /* Return next header */
		}
#if ENABLE_FEATURE_AR_LONG_FILENAMES
		if (ar.formatted.name[1] == '/') {
			/* If the second char is a '/' then this entries data section
			 * stores long filename for multiple entries, they are stored
			 * in static variable long_names for use in future entries
			 */
			ar_long_name_size = size;
			free(ar_long_names);
			ar_long_names = xmalloc(size);
			xread(archive_handle->src_fd, ar_long_names, size);
			archive_handle->offset += size;
			/* Return next header */
			return get_header_ar(archive_handle);
		}
#else
		bb_error_msg_and_die("long filenames not supported");
#endif
	}
	/* Only size is always present, the rest may be missing in
	 * long filename pseudo file. Thus we decode the rest
	 * after dealing with long filename pseudo file.
	 */
	typed->mode = read_num(ar.formatted.mode, 8);
	typed->mtime = read_num(ar.formatted.date, 10);
	typed->uid = read_num(ar.formatted.uid, 10);
	typed->gid = read_num(ar.formatted.gid, 10);

#if ENABLE_FEATURE_AR_LONG_FILENAMES
	if (ar.formatted.name[0] == '/') {
		unsigned long_offset;

		/* The number after the '/' indicates the offset in the ar data section
		 * (saved in ar_long_names) that conatains the real filename */
		long_offset = read_num(&ar.formatted.name[1], 10);
		if (long_offset >= ar_long_name_size) {
			bb_error_msg_and_die("can't resolve long filename");
		}
		typed->name = xstrdup(ar_long_names + long_offset);
	} else
#endif
	{
		/* short filenames */
		typed->name = xstrndup(ar.formatted.name, 16);
	}

	typed->name[strcspn(typed->name, " /")] = '\0';

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_header(typed);
#if ENABLE_DPKG || ENABLE_DPKG_DEB
		if (archive_handle->dpkg__sub_archive) {
			while (archive_handle->dpkg__action_data_subarchive(archive_handle->dpkg__sub_archive) == EXIT_SUCCESS)
				continue;
		} else
#endif
			archive_handle->action_data(archive_handle);
	} else {
		data_skip(archive_handle);
	}

	archive_handle->offset += typed->size;
	/* Set the file pointer to the correct spot, we may have been reading a compressed file */
	lseek(archive_handle->src_fd, archive_handle->offset, SEEK_SET);

	return EXIT_SUCCESS;
}
