/* vi: set sw=4 ts=4: */
/* Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *  FIXME:
 *    In privileged mode if uname and gname map to a uid and gid then use the
 *    mapped value instead of the uid/gid values in tar header
 *
 *  References:
 *    GNU tar and star man pages,
 *    Opengroup's ustar interchange format,
 *	http://www.opengroup.org/onlinepubs/007904975/utilities/pax.html
 */

#include "libbb.h"
#include "unarchive.h"

typedef uint32_t aliased_uint32_t FIX_ALIASING;
typedef off_t    aliased_off_t    FIX_ALIASING;


/* NB: _DESTROYS_ str[len] character! */
static unsigned long long getOctal(char *str, int len)
{
	unsigned long long v;
	char *end;
	/* NB: leading spaces are allowed. Using strtoull to handle that.
	 * The downside is that we accept e.g. "-123" too :(
	 */
	str[len] = '\0';
	v = strtoull(str, &end, 8);
	/* std: "Each numeric field is terminated by one or more
	 * <space> or NUL characters". We must support ' '! */
	if (*end != '\0' && *end != ' ') {
		int8_t first = str[0];
		if (!(first & 0x80))
			bb_error_msg_and_die("corrupted octal value in tar header");
		/*
		 * GNU tar uses "base-256 encoding" for very large numbers.
		 * Encoding is binary, with highest bit always set as a marker
		 * and sign in next-highest bit:
		 * 80 00 .. 00 - zero
		 * bf ff .. ff - largest positive number
		 * ff ff .. ff - minus 1
		 * c0 00 .. 00 - smallest negative number
		 *
		 * Example of tar file with 8914993153 (0x213600001) byte file.
		 * Field starts at offset 7c:
		 * 00070  30 30 30 00 30 30 30 30  30 30 30 00 80 00 00 00  |000.0000000.....|
		 * 00080  00 00 00 02 13 60 00 01  31 31 31 32 30 33 33 36  |.....`..11120336|
		 *
		 * NB: tarballs with NEGATIVE unix times encoded that way were seen!
		 */
		v = first;
		/* Sign-extend using 6th bit: */
		v <<= sizeof(unsigned long long)*8 - 7;
		v = (long long)v >> (sizeof(unsigned long long)*8 - 7);
		while (--len != 0)
			v = (v << 8) + (unsigned char) *str++;
	}
	return v;
}
#define GET_OCTAL(a) getOctal((a), sizeof(a))

#if ENABLE_FEATURE_TAR_SELINUX
/* Scan a PAX header for SELinux contexts, via "RHT.security.selinux" keyword.
 * This is what Red Hat's patched version of tar uses.
 */
# define SELINUX_CONTEXT_KEYWORD "RHT.security.selinux"
static char *get_selinux_sctx_from_pax_hdr(archive_handle_t *archive_handle, unsigned sz)
{
	char *buf, *p;
	char *result;

	p = buf = xmalloc(sz + 1);
	/* prevent bb_strtou from running off the buffer */
	buf[sz] = '\0';
	xread(archive_handle->src_fd, buf, sz);
	archive_handle->offset += sz;

	result = NULL;
	while (sz != 0) {
		char *end, *value;
		unsigned len;

		/* Every record has this format: "LEN NAME=VALUE\n" */
		len = bb_strtou(p, &end, 10);
		/* expect errno to be EINVAL, because the character
		 * following the digits should be a space
		 */
		p += len;
		sz -= len;
		if ((int)sz < 0
		 || len == 0
		 || errno != EINVAL
		 || *end != ' '
		) {
			bb_error_msg("malformed extended header, skipped");
			// More verbose version:
			//bb_error_msg("malformed extended header at %"OFF_FMT"d, skipped",
			//		archive_handle->offset - (sz + len));
			break;
		}
		/* overwrite the terminating newline with NUL
		 * (we do not bother to check that it *was* a newline)
		 */
		p[-1] = '\0';
		/* Is it selinux security context? */
		value = end + 1;
		if (strncmp(value, SELINUX_CONTEXT_KEYWORD"=", sizeof(SELINUX_CONTEXT_KEYWORD"=") - 1) == 0) {
			value += sizeof(SELINUX_CONTEXT_KEYWORD"=") - 1;
			result = xstrdup(value);
			break;
		}
	}

	free(buf);
	return result;
}
#endif

char FAST_FUNC get_header_tar(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	struct tar_header_t tar;
	char *cp;
	int i, sum_u, sum;
#if ENABLE_FEATURE_TAR_OLDSUN_COMPATIBILITY
	int sum_s;
#endif
	int parse_names;

	/* Our "private data" */
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
# define p_longname (archive_handle->tar__longname)
# define p_linkname (archive_handle->tar__linkname)
#else
# define p_longname 0
# define p_linkname 0
#endif

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS || ENABLE_FEATURE_TAR_SELINUX
 again:
#endif
	/* Align header */
	data_align(archive_handle, 512);

 again_after_align:

#if ENABLE_DESKTOP || ENABLE_FEATURE_TAR_AUTODETECT
	/* to prevent misdetection of bz2 sig */
	*(aliased_uint32_t*)&tar = 0;
	i = full_read(archive_handle->src_fd, &tar, 512);
	/* If GNU tar sees EOF in above read, it says:
	 * "tar: A lone zero block at N", where N = kilobyte
	 * where EOF was met (not EOF block, actual EOF!),
	 * and exits with EXIT_SUCCESS.
	 * We will mimic exit(EXIT_SUCCESS), although we will not mimic
	 * the message and we don't check whether we indeed
	 * saw zero block directly before this. */
	if (i == 0) {
		xfunc_error_retval = 0;
 short_read:
		bb_error_msg_and_die("short read");
	}
	if (i != 512) {
		IF_FEATURE_TAR_AUTODETECT(goto autodetect;)
		goto short_read;
	}

#else
	i = 512;
	xread(archive_handle->src_fd, &tar, i);
#endif
	archive_handle->offset += i;

	/* If there is no filename its an empty header */
	if (tar.name[0] == 0 && tar.prefix[0] == 0) {
		if (archive_handle->tar__end) {
			/* Second consecutive empty header - end of archive.
			 * Read until the end to empty the pipe from gz or bz2
			 */
			while (full_read(archive_handle->src_fd, &tar, 512) == 512)
				continue;
			return EXIT_FAILURE;
		}
		archive_handle->tar__end = 1;
		return EXIT_SUCCESS;
	}
	archive_handle->tar__end = 0;

	/* Check header has valid magic, "ustar" is for the proper tar,
	 * five NULs are for the old tar format  */
	if (strncmp(tar.magic, "ustar", 5) != 0
	 && (!ENABLE_FEATURE_TAR_OLDGNU_COMPATIBILITY
	     || memcmp(tar.magic, "\0\0\0\0", 5) != 0)
	) {
#if ENABLE_FEATURE_TAR_AUTODETECT
		char FAST_FUNC (*get_header_ptr)(archive_handle_t *);
		uint16_t magic2;

 autodetect:
		magic2 = *(uint16_t*)tar.name;
		/* tar gz/bz autodetect: check for gz/bz2 magic.
		 * If we see the magic, and it is the very first block,
		 * we can switch to get_header_tar_gz/bz2/lzma().
		 * Needs seekable fd. I wish recv(MSG_PEEK) works
		 * on any fd... */
# if ENABLE_FEATURE_SEAMLESS_GZ
		if (magic2 == GZIP_MAGIC) {
			get_header_ptr = get_header_tar_gz;
		} else
# endif
# if ENABLE_FEATURE_SEAMLESS_BZ2
		if (magic2 == BZIP2_MAGIC
		 && tar.name[2] == 'h' && isdigit(tar.name[3])
		) { /* bzip2 */
			get_header_ptr = get_header_tar_bz2;
		} else
# endif
# if ENABLE_FEATURE_SEAMLESS_XZ
		//TODO: if (magic2 == XZ_MAGIC1)...
		//else
# endif
			goto err;
		/* Two different causes for lseek() != 0:
		 * unseekable fd (would like to support that too, but...),
		 * or not first block (false positive, it's not .gz/.bz2!) */
		if (lseek(archive_handle->src_fd, -i, SEEK_CUR) != 0)
			goto err;
		while (get_header_ptr(archive_handle) == EXIT_SUCCESS)
			continue;
		return EXIT_FAILURE;
 err:
#endif /* FEATURE_TAR_AUTODETECT */
		bb_error_msg_and_die("invalid tar magic");
	}

	/* Do checksum on headers.
	 * POSIX says that checksum is done on unsigned bytes, but
	 * Sun and HP-UX gets it wrong... more details in
	 * GNU tar source. */
#if ENABLE_FEATURE_TAR_OLDSUN_COMPATIBILITY
	sum_s = ' ' * sizeof(tar.chksum);
#endif
	sum_u = ' ' * sizeof(tar.chksum);
	for (i = 0; i < 148; i++) {
		sum_u += ((unsigned char*)&tar)[i];
#if ENABLE_FEATURE_TAR_OLDSUN_COMPATIBILITY
		sum_s += ((signed char*)&tar)[i];
#endif
	}
	for (i = 156; i < 512; i++) {
		sum_u += ((unsigned char*)&tar)[i];
#if ENABLE_FEATURE_TAR_OLDSUN_COMPATIBILITY
		sum_s += ((signed char*)&tar)[i];
#endif
	}
	/* This field does not need special treatment (getOctal) */
	{
		char *endp; /* gcc likes temp var for &endp */
		sum = strtoul(tar.chksum, &endp, 8);
		if ((*endp != '\0' && *endp != ' ')
		 || (sum_u != sum IF_FEATURE_TAR_OLDSUN_COMPATIBILITY(&& sum_s != sum))
		) {
			bb_error_msg_and_die("invalid tar header checksum");
		}
	}
	/* don't use xstrtoul, tar.chksum may have leading spaces */
	sum = strtoul(tar.chksum, NULL, 8);
	if (sum_u != sum IF_FEATURE_TAR_OLDSUN_COMPATIBILITY(&& sum_s != sum)) {
		bb_error_msg_and_die("invalid tar header checksum");
	}

	/* 0 is reserved for high perf file, treat as normal file */
	if (!tar.typeflag) tar.typeflag = '0';
	parse_names = (tar.typeflag >= '0' && tar.typeflag <= '7');

	/* getOctal trashes subsequent field, therefore we call it
	 * on fields in reverse order */
	if (tar.devmajor[0]) {
		char t = tar.prefix[0];
		/* we trash prefix[0] here, but we DO need it later! */
		unsigned minor = GET_OCTAL(tar.devminor);
		unsigned major = GET_OCTAL(tar.devmajor);
		file_header->device = makedev(major, minor);
		tar.prefix[0] = t;
	}
	file_header->link_target = NULL;
	if (!p_linkname && parse_names && tar.linkname[0]) {
		file_header->link_target = xstrndup(tar.linkname, sizeof(tar.linkname));
		/* FIXME: what if we have non-link object with link_target? */
		/* Will link_target be free()ed? */
	}
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	file_header->tar__uname = tar.uname[0] ? xstrndup(tar.uname, sizeof(tar.uname)) : NULL;
	file_header->tar__gname = tar.gname[0] ? xstrndup(tar.gname, sizeof(tar.gname)) : NULL;
#endif
	file_header->mtime = GET_OCTAL(tar.mtime);
	file_header->size = GET_OCTAL(tar.size);
	file_header->gid = GET_OCTAL(tar.gid);
	file_header->uid = GET_OCTAL(tar.uid);
	/* Set bits 0-11 of the files mode */
	file_header->mode = 07777 & GET_OCTAL(tar.mode);

	file_header->name = NULL;
	if (!p_longname && parse_names) {
		/* we trash mode[0] here, it's ok */
		//tar.name[sizeof(tar.name)] = '\0'; - gcc 4.3.0 would complain
		tar.mode[0] = '\0';
		if (tar.prefix[0]) {
			/* and padding[0] */
			//tar.prefix[sizeof(tar.prefix)] = '\0'; - gcc 4.3.0 would complain
			tar.padding[0] = '\0';
			file_header->name = concat_path_file(tar.prefix, tar.name);
		} else
			file_header->name = xstrdup(tar.name);
	}

	/* Set bits 12-15 of the files mode */
	/* (typeflag was not trashed because chksum does not use getOctal) */
	switch (tar.typeflag) {
	/* busybox identifies hard links as being regular files with 0 size and a link name */
	case '1':
		file_header->mode |= S_IFREG;
		break;
	case '7':
	/* case 0: */
	case '0':
#if ENABLE_FEATURE_TAR_OLDGNU_COMPATIBILITY
		if (last_char_is(file_header->name, '/')) {
			goto set_dir;
		}
#endif
		file_header->mode |= S_IFREG;
		break;
	case '2':
		file_header->mode |= S_IFLNK;
		/* have seen tarballs with size field containing
		 * the size of the link target's name */
 size0:
		file_header->size = 0;
		break;
	case '3':
		file_header->mode |= S_IFCHR;
		goto size0; /* paranoia */
	case '4':
		file_header->mode |= S_IFBLK;
		goto size0;
	case '5':
 IF_FEATURE_TAR_OLDGNU_COMPATIBILITY(set_dir:)
		file_header->mode |= S_IFDIR;
		goto size0;
	case '6':
		file_header->mode |= S_IFIFO;
		goto size0;
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	case 'L':
		/* free: paranoia: tar with several consecutive longnames */
		free(p_longname);
		/* For paranoia reasons we allocate extra NUL char */
		p_longname = xzalloc(file_header->size + 1);
		/* We read ASCIZ string, including NUL */
		xread(archive_handle->src_fd, p_longname, file_header->size);
		archive_handle->offset += file_header->size;
		/* return get_header_tar(archive_handle); */
		/* gcc 4.1.1 didn't optimize it into jump */
		/* so we will do it ourself, this also saves stack */
		goto again;
	case 'K':
		free(p_linkname);
		p_linkname = xzalloc(file_header->size + 1);
		xread(archive_handle->src_fd, p_linkname, file_header->size);
		archive_handle->offset += file_header->size;
		/* return get_header_tar(archive_handle); */
		goto again;
	case 'D':	/* GNU dump dir */
	case 'M':	/* Continuation of multi volume archive */
	case 'N':	/* Old GNU for names > 100 characters */
	case 'S':	/* Sparse file */
	case 'V':	/* Volume header */
#endif
#if !ENABLE_FEATURE_TAR_SELINUX
	case 'g':	/* pax global header */
	case 'x':	/* pax extended header */
#else
 skip_ext_hdr:
#endif
	{
		off_t sz;
		bb_error_msg("warning: skipping header '%c'", tar.typeflag);
		sz = (file_header->size + 511) & ~(off_t)511;
		archive_handle->offset += sz;
		sz >>= 9; /* sz /= 512 but w/o contortions for signed div */
		while (sz--)
			xread(archive_handle->src_fd, &tar, 512);
		/* return get_header_tar(archive_handle); */
		goto again_after_align;
	}
#if ENABLE_FEATURE_TAR_SELINUX
	case 'g':	/* pax global header */
	case 'x': {	/* pax extended header */
		char **pp;
		if ((uoff_t)file_header->size > 0xfffff) /* paranoia */
			goto skip_ext_hdr;
		pp = (tar.typeflag == 'g') ? &archive_handle->tar__global_sctx : &archive_handle->tar__next_file_sctx;
		free(*pp);
		*pp = get_selinux_sctx_from_pax_hdr(archive_handle, file_header->size);
		goto again;
	}
#endif
	default:
		bb_error_msg_and_die("unknown typeflag: 0x%x", tar.typeflag);
	}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (p_longname) {
		file_header->name = p_longname;
		p_longname = NULL;
	}
	if (p_linkname) {
		file_header->link_target = p_linkname;
		p_linkname = NULL;
	}
#endif
	if (strncmp(file_header->name, "/../"+1, 3) == 0
	 || strstr(file_header->name, "/../")
	) {
		bb_error_msg_and_die("name with '..' encountered: '%s'",
				file_header->name);
	}

	/* Strip trailing '/' in directories */
	/* Must be done after mode is set as '/' is used to check if it's a directory */
	cp = last_char_is(file_header->name, '/');

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_header(/*archive_handle->*/ file_header);
		/* Note that we kill the '/' only after action_header() */
		/* (like GNU tar 1.15.1: verbose mode outputs "dir/dir/") */
		if (cp)
			*cp = '\0';
		archive_handle->action_data(archive_handle);
		if (archive_handle->accept || archive_handle->reject)
			llist_add_to(&archive_handle->passed, file_header->name);
		else /* Caller isn't interested in list of unpacked files */
			free(file_header->name);
	} else {
		data_skip(archive_handle);
		free(file_header->name);
	}
	archive_handle->offset += file_header->size;

	free(file_header->link_target);
	/* Do not free(file_header->name)!
	 * It might be inserted in archive_handle->passed - see above */
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	free(file_header->tar__uname);
	free(file_header->tar__gname);
#endif
	return EXIT_SUCCESS;
}
