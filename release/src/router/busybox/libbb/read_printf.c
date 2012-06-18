/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

#define ZIPPED (ENABLE_FEATURE_SEAMLESS_LZMA \
	|| ENABLE_FEATURE_SEAMLESS_BZ2 \
	|| ENABLE_FEATURE_SEAMLESS_GZ \
	/* || ENABLE_FEATURE_SEAMLESS_Z */ \
)

#if ZIPPED
# include "unarchive.h"
#endif


/* Suppose that you are a shell. You start child processes.
 * They work and eventually exit. You want to get user input.
 * You read stdin. But what happens if last child switched
 * its stdin into O_NONBLOCK mode?
 *
 * *** SURPRISE! It will affect the parent too! ***
 * *** BIG SURPRISE! It stays even after child exits! ***
 *
 * This is a design bug in UNIX API.
 *      fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
 * will set nonblocking mode not only on _your_ stdin, but
 * also on stdin of your parent, etc.
 *
 * In general,
 *      fd2 = dup(fd1);
 *      fcntl(fd2, F_SETFL, fcntl(fd2, F_GETFL) | O_NONBLOCK);
 * sets both fd1 and fd2 to O_NONBLOCK. This includes cases
 * where duping is done implicitly by fork() etc.
 *
 * We need
 *      fcntl(fd2, F_SETFD, fcntl(fd2, F_GETFD) | O_NONBLOCK);
 * (note SETFD, not SETFL!) but such thing doesn't exist.
 *
 * Alternatively, we need nonblocking_read(fd, ...) which doesn't
 * require O_NONBLOCK dance at all. Actually, it exists:
 *      n = recv(fd, buf, len, MSG_DONTWAIT);
 *      "MSG_DONTWAIT:
 *      Enables non-blocking operation; if the operation
 *      would block, EAGAIN is returned."
 * but recv() works only for sockets!
 *
 * So far I don't see any good solution, I can only propose
 * that affected readers should be careful and use this routine,
 * which detects EAGAIN and uses poll() to wait on the fd.
 * Thankfully, poll() doesn't care about O_NONBLOCK flag.
 */
ssize_t FAST_FUNC nonblock_safe_read(int fd, void *buf, size_t count)
{
	struct pollfd pfd[1];
	ssize_t n;

	while (1) {
		n = safe_read(fd, buf, count);
		if (n >= 0 || errno != EAGAIN)
			return n;
		/* fd is in O_NONBLOCK mode. Wait using poll and repeat */
		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		safe_poll(pfd, 1, -1); /* note: this pulls in printf */
	}
}

// Reads one line a-la fgets (but doesn't save terminating '\n').
// Reads byte-by-byte. Useful when it is important to not read ahead.
// Bytes are appended to pfx (which must be malloced, or NULL).
char* FAST_FUNC xmalloc_reads(int fd, char *buf, size_t *maxsz_p)
{
	char *p;
	size_t sz = buf ? strlen(buf) : 0;
	size_t maxsz = maxsz_p ? *maxsz_p : (INT_MAX - 4095);

	goto jump_in;
	while (sz < maxsz) {
		if ((size_t)(p - buf) == sz) {
 jump_in:
			buf = xrealloc(buf, sz + 128);
			p = buf + sz;
			sz += 128;
		}
		/* nonblock_safe_read() because we are used by e.g. shells */
		if (nonblock_safe_read(fd, p, 1) != 1) { /* EOF/error */
			if (p == buf) { /* we read nothing */
				free(buf);
				return NULL;
			}
			break;
		}
		if (*p == '\n')
			break;
		p++;
	}
	*p = '\0';
	if (maxsz_p)
		*maxsz_p  = p - buf;
	p++;
	return xrealloc(buf, p - buf);
}

// Read (potentially big) files in one go. File size is estimated
// by stat. Extra '\0' byte is appended.
void* FAST_FUNC xmalloc_read(int fd, size_t *maxsz_p)
{
	char *buf;
	size_t size, rd_size, total;
	size_t to_read;
	struct stat st;

	to_read = maxsz_p ? *maxsz_p : (INT_MAX - 4095); /* max to read */

	/* Estimate file size */
	st.st_size = 0; /* in case fstat fails, assume 0 */
	fstat(fd, &st);
	/* /proc/N/stat files report st_size 0 */
	/* In order to make such files readable, we add small const */
	size = (st.st_size | 0x3ff) + 1;

	total = 0;
	buf = NULL;
	while (1) {
		if (to_read < size)
			size = to_read;
		buf = xrealloc(buf, total + size + 1);
		rd_size = full_read(fd, buf + total, size);
		if ((ssize_t)rd_size == (ssize_t)(-1)) { /* error */
			free(buf);
			return NULL;
		}
		total += rd_size;
		if (rd_size < size) /* EOF */
			break;
		if (to_read <= rd_size)
			break;
		to_read -= rd_size;
		/* grow by 1/8, but in [1k..64k] bounds */
		size = ((total / 8) | 0x3ff) + 1;
		if (size > 64*1024)
			size = 64*1024;
	}
	buf = xrealloc(buf, total + 1);
	buf[total] = '\0';

	if (maxsz_p)
		*maxsz_p = total;
	return buf;
}

#ifdef USING_LSEEK_TO_GET_SIZE
/* Alternatively, file size can be obtained by lseek to the end.
 * The code is slightly bigger. Retained in case fstat approach
 * will not work for some weird cases (/proc, block devices, etc).
 * (NB: lseek also can fail to work for some weird files) */

// Read (potentially big) files in one go. File size is estimated by
// lseek to end.
void* FAST_FUNC xmalloc_open_read_close(const char *filename, size_t *maxsz_p)
{
	char *buf;
	size_t size;
	int fd;
	off_t len;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	/* /proc/N/stat files report len 0 here */
	/* In order to make such files readable, we add small const */
	size = 0x3ff; /* read only 1k on unseekable files */
	len = lseek(fd, 0, SEEK_END) | 0x3ff; /* + up to 1k */
	if (len != (off_t)-1) {
		xlseek(fd, 0, SEEK_SET);
		size = maxsz_p ? *maxsz_p : (INT_MAX - 4095);
		if (len < size)
			size = len;
	}

	buf = xmalloc(size + 1);
	size = read_close(fd, buf, size);
	if ((ssize_t)size < 0) {
		free(buf);
		return NULL;
	}
	buf = xrealloc(buf, size + 1);
	buf[size] = '\0';

	if (maxsz_p)
		*maxsz_p = size;
	return buf;
}
#endif

// Read (potentially big) files in one go. File size is estimated
// by stat.
void* FAST_FUNC xmalloc_open_read_close(const char *filename, size_t *maxsz_p)
{
	char *buf;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	buf = xmalloc_read(fd, maxsz_p);
	close(fd);
	return buf;
}

/* Die with an error message if we can't read the entire buffer. */
void FAST_FUNC xread(int fd, void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_read(fd, buf, count);
		if ((size_t)size != count)
			bb_error_msg_and_die("short read");
	}
}

/* Die with an error message if we can't read one character. */
unsigned char FAST_FUNC xread_char(int fd)
{
	char tmp;
	xread(fd, &tmp, 1);
	return tmp;
}

void* FAST_FUNC xmalloc_xopen_read_close(const char *filename, size_t *maxsz_p)
{
	void *buf = xmalloc_open_read_close(filename, maxsz_p);
	if (!buf)
		bb_perror_msg_and_die("can't read '%s'", filename);
	return buf;
}

/* Used by e.g. rpm which gives us a fd without filename,
 * thus we can't guess the format from filename's extension.
 */
#if ZIPPED
void FAST_FUNC setup_unzip_on_fd(int fd /*, int fail_if_not_detected*/)
{
	const int fail_if_not_detected = 1;
	union {
		uint8_t b[4];
		uint16_t b16[2];
		uint32_t b32[1];
	} magic;
	int offset = -2;
# if BB_MMU
	IF_DESKTOP(long long) int FAST_FUNC (*xformer)(int src_fd, int dst_fd);
	enum { xformer_prog = 0 };
# else
	enum { xformer = 0 };
	const char *xformer_prog;
# endif

	/* .gz and .bz2 both have 2-byte signature, and their
	 * unpack_XXX_stream wants this header skipped. */
	xread(fd, magic.b16, sizeof(magic.b16[0]));
	if (ENABLE_FEATURE_SEAMLESS_GZ
	 && magic.b16[0] == GZIP_MAGIC
	) {
# if BB_MMU
		xformer = unpack_gz_stream;
# else
		xformer_prog = "gunzip";
# endif
		goto found_magic;
	}
	if (ENABLE_FEATURE_SEAMLESS_BZ2
	 && magic.b16[0] == BZIP2_MAGIC
	) {
# if BB_MMU
		xformer = unpack_bz2_stream;
# else
		xformer_prog = "bunzip2";
# endif
		goto found_magic;
	}
	if (ENABLE_FEATURE_SEAMLESS_XZ
	 && magic.b16[0] == XZ_MAGIC1
	) {
		offset = -6;
		xread(fd, magic.b32, sizeof(magic.b32[0]));
		if (magic.b32[0] == XZ_MAGIC2) {
# if BB_MMU
			xformer = unpack_xz_stream;
			/* unpack_xz_stream wants fd at position 6, no need to seek */
			//xlseek(fd, offset, SEEK_CUR);
# else
			xformer_prog = "unxz";
# endif
			goto found_magic;
		}
	}

	/* No known magic seen */
	if (fail_if_not_detected)
		bb_error_msg_and_die("no gzip"
			IF_FEATURE_SEAMLESS_BZ2("/bzip2")
			IF_FEATURE_SEAMLESS_XZ("/xz")
			" magic");
	xlseek(fd, offset, SEEK_CUR);
	return;

 found_magic:
# if !BB_MMU
	/* NOMMU version of open_transformer execs
	 * an external unzipper that wants
	 * file position at the start of the file */
	xlseek(fd, offset, SEEK_CUR);
# endif
	open_transformer(fd, xformer, xformer_prog);
}
#endif /* ZIPPED */

int FAST_FUNC open_zipped(const char *fname)
{
#if !ZIPPED
	return open(fname, O_RDONLY);
#else
	char *sfx;
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		return fd;

	sfx = strrchr(fname, '.');
	if (sfx) {
		sfx++;
		if (ENABLE_FEATURE_SEAMLESS_LZMA && strcmp(sfx, "lzma") == 0)
			/* .lzma has no header/signature, just trust it */
			open_transformer(fd, unpack_lzma_stream, "unlzma");
		else
		if ((ENABLE_FEATURE_SEAMLESS_GZ && strcmp(sfx, "gz") == 0)
		 || (ENABLE_FEATURE_SEAMLESS_BZ2 && strcmp(sfx, "bz2") == 0)
		 || (ENABLE_FEATURE_SEAMLESS_XZ && strcmp(sfx, "xz") == 0)
		) {
			setup_unzip_on_fd(fd /*, fail_if_not_detected: 1*/);
		}
	}

	return fd;
#endif
}

void* FAST_FUNC xmalloc_open_zipped_read_close(const char *fname, size_t *maxsz_p)
{
	int fd;
	char *image;

	fd = open_zipped(fname);
	if (fd < 0)
		return NULL;

	image = xmalloc_read(fd, maxsz_p);
	if (!image)
		bb_perror_msg("read error from '%s'", fname);
	close(fd);

	return image;
}
