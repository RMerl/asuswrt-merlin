/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

char FAST_FUNC get_header_tar_bz2(archive_handle_t *archive_handle)
{
	/* Can't lseek over pipes */
	archive_handle->seek = seek_by_read;

	open_transformer(archive_handle->src_fd, unpack_bz2_stream_prime, "bunzip2");
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS)
		continue;

	/* Can only do one file at a time */
	return EXIT_FAILURE;
}
