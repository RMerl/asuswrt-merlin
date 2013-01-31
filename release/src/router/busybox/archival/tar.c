/* vi: set sw=4 ts=4: */
/*
 * Mini tar implementation for busybox
 *
 * Modified to use common extraction code used by ar, cpio, dpkg-deb, dpkg
 *  by Glenn McGrath
 *
 * Note, that as of BusyBox-0.43, tar has been completely rewritten from the
 * ground up.  It still has remnants of the old code lying about, but it is
 * very different now (i.e., cleaner, less global variables, etc.)
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Based in part in the tar implementation in sash
 *  Copyright (c) 1999 by David I. Bell
 *  Permission is granted to use, distribute, or modify this source,
 *  provided that this copyright notice remains intact.
 *  Permission to distribute sash derived code under GPL has been granted.
 *
 * Based in part on the tar implementation from busybox-0.28
 *  Copyright (C) 1995 Bruce Perens
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* TODO: security with -C DESTDIR option can be enhanced.
 * Consider tar file created via:
 * $ tar cvf bug.tar anything.txt
 * $ ln -s /tmp symlink
 * $ tar --append -f bug.tar symlink
 * $ rm symlink
 * $ mkdir symlink
 * $ tar --append -f bug.tar symlink/evil.py
 *
 * This will result in an archive which contains:
 * $ tar --list -f bug.tar
 * anything.txt
 * symlink
 * symlink/evil.py
 *
 * Untarring it puts evil.py in '/tmp' even if the -C DESTDIR is given.
 * This doesn't feel right, and IIRC GNU tar doesn't do that.
 */

#include <fnmatch.h>
#include "libbb.h"
#include "bb_archive.h"
/* FIXME: Stop using this non-standard feature */
#ifndef FNM_LEADING_DIR
# define FNM_LEADING_DIR 0
#endif


//#define DBG(fmt, ...) bb_error_msg("%s: " fmt, __func__, ## __VA_ARGS__)
#define DBG(...) ((void)0)


#define block_buf bb_common_bufsiz1


#if !ENABLE_FEATURE_SEAMLESS_GZ && !ENABLE_FEATURE_SEAMLESS_BZ2
/* Do not pass gzip flag to writeTarFile() */
#define writeTarFile(tar_fd, verboseFlag, dereferenceFlag, include, exclude, gzip) \
	writeTarFile(tar_fd, verboseFlag, dereferenceFlag, include, exclude)
#endif


#if ENABLE_FEATURE_TAR_CREATE

/*
** writeTarFile(), writeFileToTarball(), and writeTarHeader() are
** the only functions that deal with the HardLinkInfo structure.
** Even these functions use the xxxHardLinkInfo() functions.
*/
typedef struct HardLinkInfo {
	struct HardLinkInfo *next; /* Next entry in list */
	dev_t dev;                 /* Device number */
	ino_t ino;                 /* Inode number */
//	short linkCount;           /* (Hard) Link Count */
	char name[1];              /* Start of filename (must be last) */
} HardLinkInfo;

/* Some info to be carried along when creating a new tarball */
typedef struct TarBallInfo {
	int tarFd;                      /* Open-for-write file descriptor
	                                 * for the tarball */
	int verboseFlag;                /* Whether to print extra stuff or not */
	const llist_t *excludeList;     /* List of files to not include */
	HardLinkInfo *hlInfoHead;       /* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;           /* Hard Link Info for the current file */
//TODO: save only st_dev + st_ino
	struct stat tarFileStatBuf;     /* Stat info for the tarball, letting
	                                 * us know the inode and device that the
	                                 * tarball lives, so we can avoid trying
	                                 * to include the tarball into itself */
} TarBallInfo;

/* A nice enum with all the possible tar file content types */
enum {
	REGTYPE = '0',		/* regular file */
	REGTYPE0 = '\0',	/* regular file (ancient bug compat) */
	LNKTYPE = '1',		/* hard link */
	SYMTYPE = '2',		/* symbolic link */
	CHRTYPE = '3',		/* character special */
	BLKTYPE = '4',		/* block special */
	DIRTYPE = '5',		/* directory */
	FIFOTYPE = '6',		/* FIFO special */
	CONTTYPE = '7',		/* reserved */
	GNULONGLINK = 'K',	/* GNU long (>100 chars) link name */
	GNULONGNAME = 'L',	/* GNU long (>100 chars) file name */
};

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static void addHardLinkInfo(HardLinkInfo **hlInfoHeadPtr,
					struct stat *statbuf,
					const char *fileName)
{
	/* Note: hlInfoHeadPtr can never be NULL! */
	HardLinkInfo *hlInfo;

	hlInfo = xmalloc(sizeof(HardLinkInfo) + strlen(fileName));
	hlInfo->next = *hlInfoHeadPtr;
	*hlInfoHeadPtr = hlInfo;
	hlInfo->dev = statbuf->st_dev;
	hlInfo->ino = statbuf->st_ino;
//	hlInfo->linkCount = statbuf->st_nlink;
	strcpy(hlInfo->name, fileName);
}

static void freeHardLinkInfo(HardLinkInfo **hlInfoHeadPtr)
{
	HardLinkInfo *hlInfo;
	HardLinkInfo *hlInfoNext;

	if (hlInfoHeadPtr) {
		hlInfo = *hlInfoHeadPtr;
		while (hlInfo) {
			hlInfoNext = hlInfo->next;
			free(hlInfo);
			hlInfo = hlInfoNext;
		}
		*hlInfoHeadPtr = NULL;
	}
}

/* Might be faster (and bigger) if the dev/ino were stored in numeric order ;) */
static HardLinkInfo *findHardLinkInfo(HardLinkInfo *hlInfo, struct stat *statbuf)
{
	while (hlInfo) {
		if (statbuf->st_ino == hlInfo->ino
		 && statbuf->st_dev == hlInfo->dev
		) {
			DBG("found hardlink:'%s'", hlInfo->name);
			break;
		}
		hlInfo = hlInfo->next;
	}
	return hlInfo;
}

/* Put an octal string into the specified buffer.
 * The number is zero padded and possibly null terminated.
 * Stores low-order bits only if whole value does not fit. */
static void putOctal(char *cp, int len, off_t value)
{
	char tempBuffer[sizeof(off_t)*3 + 1];
	char *tempString = tempBuffer;
	int width;

	width = sprintf(tempBuffer, "%0*"OFF_FMT"o", len, value);
	tempString += (width - len);

	/* If string has leading zeroes, we can drop one */
	/* and field will have trailing '\0' */
	/* (increases chances of compat with other tars) */
	if (tempString[0] == '0')
		tempString++;

	/* Copy the string to the field */
	memcpy(cp, tempString, len);
}
#define PUT_OCTAL(a, b) putOctal((a), sizeof(a), (b))

static void chksum_and_xwrite(int fd, struct tar_header_t* hp)
{
	/* POSIX says that checksum is done on unsigned bytes
	 * (Sun and HP-UX gets it wrong... more details in
	 * GNU tar source) */
	const unsigned char *cp;
	int chksum, size;

	strcpy(hp->magic, "ustar  ");

	/* Calculate and store the checksum (i.e., the sum of all of the bytes of
	 * the header).  The checksum field must be filled with blanks for the
	 * calculation.  The checksum field is formatted differently from the
	 * other fields: it has 6 digits, a null, then a space -- rather than
	 * digits, followed by a null like the other fields... */
	memset(hp->chksum, ' ', sizeof(hp->chksum));
	cp = (const unsigned char *) hp;
	chksum = 0;
	size = sizeof(*hp);
	do { chksum += *cp++; } while (--size);
	putOctal(hp->chksum, sizeof(hp->chksum)-1, chksum);

	/* Now write the header out to disk */
	xwrite(fd, hp, sizeof(*hp));
}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
static void writeLongname(int fd, int type, const char *name, int dir)
{
	static const struct {
		char mode[8];             /* 100-107 */
		char uid[8];              /* 108-115 */
		char gid[8];              /* 116-123 */
		char size[12];            /* 124-135 */
		char mtime[12];           /* 136-147 */
	} prefilled = {
		"0000000",
		"0000000",
		"0000000",
		"00000000000",
		"00000000000",
	};
	struct tar_header_t header;
	int size;

	dir = !!dir; /* normalize: 0/1 */
	size = strlen(name) + 1 + dir; /* GNU tar uses strlen+1 */
	/* + dir: account for possible '/' */

	memset(&header, 0, sizeof(header));
	strcpy(header.name, "././@LongLink");
	memcpy(header.mode, prefilled.mode, sizeof(prefilled));
	PUT_OCTAL(header.size, size);
	header.typeflag = type;
	chksum_and_xwrite(fd, &header);

	/* Write filename[/] and pad the block. */
	/* dir=0: writes 'name<NUL>', pads */
	/* dir=1: writes 'name', writes '/<NUL>', pads */
	dir *= 2;
	xwrite(fd, name, size - dir);
	xwrite(fd, "/", dir);
	size = (-size) & (TAR_BLOCK_SIZE-1);
	memset(&header, 0, size);
	xwrite(fd, &header, size);
}
#endif

/* Write out a tar header for the specified file/directory/whatever */
static int writeTarHeader(struct TarBallInfo *tbInfo,
		const char *header_name, const char *fileName, struct stat *statbuf)
{
	struct tar_header_t header;

	memset(&header, 0, sizeof(header));

	strncpy(header.name, header_name, sizeof(header.name));

	/* POSIX says to mask mode with 07777. */
	PUT_OCTAL(header.mode, statbuf->st_mode & 07777);
	PUT_OCTAL(header.uid, statbuf->st_uid);
	PUT_OCTAL(header.gid, statbuf->st_gid);
	memset(header.size, '0', sizeof(header.size)-1); /* Regular file size is handled later */
	/* users report that files with negative st_mtime cause trouble, so: */
	PUT_OCTAL(header.mtime, statbuf->st_mtime >= 0 ? statbuf->st_mtime : 0);

	/* Enter the user and group names */
	safe_strncpy(header.uname, get_cached_username(statbuf->st_uid), sizeof(header.uname));
	safe_strncpy(header.gname, get_cached_groupname(statbuf->st_gid), sizeof(header.gname));

	if (tbInfo->hlInfo) {
		/* This is a hard link */
		header.typeflag = LNKTYPE;
		strncpy(header.linkname, tbInfo->hlInfo->name,
				sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK,
					tbInfo->hlInfo->name, 0);
#endif
	} else if (S_ISLNK(statbuf->st_mode)) {
		char *lpath = xmalloc_readlink_or_warn(fileName);
		if (!lpath)
			return FALSE;
		header.typeflag = SYMTYPE;
		strncpy(header.linkname, lpath, sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK, lpath, 0);
#else
		/* If it is larger than 100 bytes, bail out */
		if (header.linkname[sizeof(header.linkname)-1]) {
			free(lpath);
			bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
			return FALSE;
		}
#endif
		free(lpath);
	} else if (S_ISDIR(statbuf->st_mode)) {
		header.typeflag = DIRTYPE;
		/* Append '/' only if there is a space for it */
		if (!header.name[sizeof(header.name)-1])
			header.name[strlen(header.name)] = '/';
	} else if (S_ISCHR(statbuf->st_mode)) {
		header.typeflag = CHRTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISBLK(statbuf->st_mode)) {
		header.typeflag = BLKTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISFIFO(statbuf->st_mode)) {
		header.typeflag = FIFOTYPE;
	} else if (S_ISREG(statbuf->st_mode)) {
		/* header.size field is 12 bytes long */
		/* Does octal-encoded size fit? */
		uoff_t filesize = statbuf->st_size;
		if (sizeof(filesize) <= 4
		 || filesize <= (uoff_t)0777777777777LL
		) {
			PUT_OCTAL(header.size, filesize);
		}
		/* Does base256-encoded size fit?
		 * It always does unless off_t is wider than 64 bits.
		 */
		else if (ENABLE_FEATURE_TAR_GNU_EXTENSIONS
#if ULLONG_MAX > 0xffffffffffffffffLL /* 2^64-1 */
		 && (filesize <= 0x3fffffffffffffffffffffffLL)
#endif
		) {
	                /* GNU tar uses "base-256 encoding" for very large numbers.
	                 * Encoding is binary, with highest bit always set as a marker
	                 * and sign in next-highest bit:
	                 * 80 00 .. 00 - zero
	                 * bf ff .. ff - largest positive number
	                 * ff ff .. ff - minus 1
	                 * c0 00 .. 00 - smallest negative number
			 */
			char *p8 = header.size + sizeof(header.size);
			do {
				*--p8 = (uint8_t)filesize;
				filesize >>= 8;
			} while (p8 != header.size);
			*p8 |= 0x80;
		} else {
			bb_error_msg_and_die("can't store file '%s' "
				"of size %"OFF_FMT"u, aborting",
				fileName, statbuf->st_size);
		}
		header.typeflag = REGTYPE;
	} else {
		bb_error_msg("%s: unknown file type", fileName);
		return FALSE;
	}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	/* Write out long name if needed */
	/* (we, like GNU tar, output long linkname *before* long name) */
	if (header.name[sizeof(header.name)-1])
		writeLongname(tbInfo->tarFd, GNULONGNAME,
				header_name, S_ISDIR(statbuf->st_mode));
#endif

	/* Now write the header out to disk */
	chksum_and_xwrite(tbInfo->tarFd, &header);

	/* Now do the verbose thing (or not) */
	if (tbInfo->verboseFlag) {
		FILE *vbFd = stdout;

		/* If archive goes to stdout, verbose goes to stderr */
		if (tbInfo->tarFd == STDOUT_FILENO)
			vbFd = stderr;
		/* GNU "tar cvvf" prints "extended" listing a-la "ls -l" */
		/* We don't have such excesses here: for us "v" == "vv" */
		/* '/' is probably a GNUism */
		fprintf(vbFd, "%s%s\n", header_name,
				S_ISDIR(statbuf->st_mode) ? "/" : "");
	}

	return TRUE;
}

#if ENABLE_FEATURE_TAR_FROM
static int exclude_file(const llist_t *excluded_files, const char *file)
{
	while (excluded_files) {
		if (excluded_files->data[0] == '/') {
			if (fnmatch(excluded_files->data, file,
					FNM_PATHNAME | FNM_LEADING_DIR) == 0)
				return 1;
		} else {
			const char *p;

			for (p = file; p[0] != '\0'; p++) {
				if ((p == file || p[-1] == '/')
				 && p[0] != '/'
				 && fnmatch(excluded_files->data, p,
						FNM_PATHNAME | FNM_LEADING_DIR) == 0
				) {
					return 1;
				}
			}
		}
		excluded_files = excluded_files->link;
	}

	return 0;
}
#else
# define exclude_file(excluded_files, file) 0
#endif

static int FAST_FUNC writeFileToTarball(const char *fileName, struct stat *statbuf,
			void *userData, int depth UNUSED_PARAM)
{
	struct TarBallInfo *tbInfo = (struct TarBallInfo *) userData;
	const char *header_name;
	int inputFileFd = -1;

	DBG("writeFileToTarball('%s')", fileName);

	/* Strip leading '/' and such (must be before memorizing hardlink's name) */
	header_name = strip_unsafe_prefix(fileName);

	if (header_name[0] == '\0')
		return TRUE;

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		bb_error_msg("%s: socket ignored", fileName);
		return TRUE;
	}

	/*
	 * Check to see if we are dealing with a hard link.
	 * If so -
	 * Treat the first occurance of a given dev/inode as a file while
	 * treating any additional occurances as hard links.  This is done
	 * by adding the file information to the HardLinkInfo linked list.
	 */
	tbInfo->hlInfo = NULL;
	if (!S_ISDIR(statbuf->st_mode) && statbuf->st_nlink > 1) {
		DBG("'%s': st_nlink > 1", header_name);
		tbInfo->hlInfo = findHardLinkInfo(tbInfo->hlInfoHead, statbuf);
		if (tbInfo->hlInfo == NULL) {
			DBG("'%s': addHardLinkInfo", header_name);
			addHardLinkInfo(&tbInfo->hlInfoHead, statbuf, header_name);
		}
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->tarFileStatBuf.st_dev == statbuf->st_dev
	 && tbInfo->tarFileStatBuf.st_ino == statbuf->st_ino
	) {
		bb_error_msg("%s: file is the archive; skipping", fileName);
		return TRUE;
	}

	if (exclude_file(tbInfo->excludeList, header_name))
		return SKIP;

#if !ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (strlen(header_name) >= NAME_SIZE) {
		bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
		return TRUE;
	}
#endif

	/* Is this a regular file? */
	if (tbInfo->hlInfo == NULL && S_ISREG(statbuf->st_mode)) {
		/* open the file we want to archive, and make sure all is well */
		inputFileFd = open_or_warn(fileName, O_RDONLY);
		if (inputFileFd < 0) {
			return FALSE;
		}
	}

	/* Add an entry to the tarball */
	if (writeTarHeader(tbInfo, header_name, fileName, statbuf) == FALSE) {
		return FALSE;
	}

	/* If it was a regular file, write out the body */
	if (inputFileFd >= 0) {
		size_t readSize;
		/* Write the file to the archive. */
		/* We record size into header first, */
		/* and then write out file. If file shrinks in between, */
		/* tar will be corrupted. So we don't allow for that. */
		/* NB: GNU tar 1.16 warns and pads with zeroes */
		/* or even seeks back and updates header */
		bb_copyfd_exact_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////off_t readSize;
		////readSize = bb_copyfd_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////if (readSize != statbuf->st_size && readSize >= 0) {
		////	bb_error_msg_and_die("short read from %s, aborting", fileName);
		////}

		/* Check that file did not grow in between? */
		/* if (safe_read(inputFileFd, 1) == 1) warn but continue? */

		close(inputFileFd);

		/* Pad the file up to the tar block size */
		/* (a few tricks here in the name of code size) */
		readSize = (-(int)statbuf->st_size) & (TAR_BLOCK_SIZE-1);
		memset(block_buf, 0, readSize);
		xwrite(tbInfo->tarFd, block_buf, readSize);
	}

	return TRUE;
}

#if ENABLE_FEATURE_SEAMLESS_GZ || ENABLE_FEATURE_SEAMLESS_BZ2
# if !(ENABLE_FEATURE_SEAMLESS_GZ && ENABLE_FEATURE_SEAMLESS_BZ2)
#  define vfork_compressor(tar_fd, gzip) vfork_compressor(tar_fd)
# endif
/* Don't inline: vfork scares gcc and pessimizes code */
static void NOINLINE vfork_compressor(int tar_fd, int gzip)
{
	pid_t gzipPid;
# if ENABLE_FEATURE_SEAMLESS_GZ && ENABLE_FEATURE_SEAMLESS_BZ2
	const char *zip_exec = (gzip == 1) ? "gzip" : "bzip2";
# elif ENABLE_FEATURE_SEAMLESS_GZ
	const char *zip_exec = "gzip";
# else /* only ENABLE_FEATURE_SEAMLESS_BZ2 */
	const char *zip_exec = "bzip2";
# endif
	// On Linux, vfork never unpauses parent early, although standard
	// allows for that. Do we want to waste bytes checking for it?
# define WAIT_FOR_CHILD 0
	volatile int vfork_exec_errno = 0;
	struct fd_pair gzipDataPipe;
# if WAIT_FOR_CHILD
	struct fd_pair gzipStatusPipe;
	xpiped_pair(gzipStatusPipe);
# endif
	xpiped_pair(gzipDataPipe);

	signal(SIGPIPE, SIG_IGN); /* we only want EPIPE on errors */

# if defined(__GNUC__) && __GNUC__
	/* Avoid vfork clobbering */
	(void) &zip_exec;
# endif

	gzipPid = xvfork();

	if (gzipPid == 0) {
		/* child */
		/* NB: close _first_, then move fds! */
		close(gzipDataPipe.wr);
# if WAIT_FOR_CHILD
		close(gzipStatusPipe.rd);
		/* gzipStatusPipe.wr will close only on exec -
		 * parent waits for this close to happen */
		fcntl(gzipStatusPipe.wr, F_SETFD, FD_CLOEXEC);
# endif
		xmove_fd(gzipDataPipe.rd, 0);
		xmove_fd(tar_fd, 1);
		/* exec gzip/bzip2 program/applet */
		BB_EXECLP(zip_exec, zip_exec, "-f", NULL);
		vfork_exec_errno = errno;
		_exit(EXIT_FAILURE);
	}

	/* parent */
	xmove_fd(gzipDataPipe.wr, tar_fd);
	close(gzipDataPipe.rd);
# if WAIT_FOR_CHILD
	close(gzipStatusPipe.wr);
	while (1) {
		char buf;
		int n;

		/* Wait until child execs (or fails to) */
		n = full_read(gzipStatusPipe.rd, &buf, 1);
		if (n < 0 /* && errno == EAGAIN */)
			continue;	/* try it again */
	}
	close(gzipStatusPipe.rd);
# endif
	if (vfork_exec_errno) {
		errno = vfork_exec_errno;
		bb_perror_msg_and_die("can't execute '%s'", zip_exec);
	}
}
#endif /* ENABLE_FEATURE_SEAMLESS_GZ || ENABLE_FEATURE_SEAMLESS_BZ2 */


/* gcc 4.2.1 inlines it, making code bigger */
static NOINLINE int writeTarFile(int tar_fd, int verboseFlag,
	int dereferenceFlag, const llist_t *include,
	const llist_t *exclude, int gzip)
{
	int errorFlag = FALSE;
	struct TarBallInfo tbInfo;

	tbInfo.hlInfoHead = NULL;
	tbInfo.tarFd = tar_fd;
	tbInfo.verboseFlag = verboseFlag;

	/* Store the stat info for the tarball's file, so
	 * can avoid including the tarball into itself....  */
	xfstat(tbInfo.tarFd, &tbInfo.tarFileStatBuf, "can't stat tar file");

#if ENABLE_FEATURE_SEAMLESS_GZ || ENABLE_FEATURE_SEAMLESS_BZ2
	if (gzip)
		vfork_compressor(tbInfo.tarFd, gzip);
#endif

	tbInfo.excludeList = exclude;

	/* Read the directory/files and iterate over them one at a time */
	while (include) {
		if (!recursive_action(include->data, ACTION_RECURSE |
				(dereferenceFlag ? ACTION_FOLLOWLINKS : 0),
				writeFileToTarball, writeFileToTarball, &tbInfo, 0)
		) {
			errorFlag = TRUE;
		}
		include = include->link;
	}
	/* Write two empty blocks to the end of the archive */
	memset(block_buf, 0, 2*TAR_BLOCK_SIZE);
	xwrite(tbInfo.tarFd, block_buf, 2*TAR_BLOCK_SIZE);

	/* To be pedantically correct, we would check if the tarball
	 * is smaller than 20 tar blocks, and pad it if it was smaller,
	 * but that isn't necessary for GNU tar interoperability, and
	 * so is considered a waste of space */

	/* Close so the child process (if any) will exit */
	close(tbInfo.tarFd);

	/* Hang up the tools, close up shop, head home */
	if (ENABLE_FEATURE_CLEAN_UP)
		freeHardLinkInfo(&tbInfo.hlInfoHead);

	if (errorFlag)
		bb_error_msg("error exit delayed from previous errors");

#if ENABLE_FEATURE_SEAMLESS_GZ || ENABLE_FEATURE_SEAMLESS_BZ2
	if (gzip) {
		int status;
		if (safe_waitpid(-1, &status, 0) == -1)
			bb_perror_msg("waitpid");
		else if (!WIFEXITED(status) || WEXITSTATUS(status))
			/* gzip was killed or has exited with nonzero! */
			errorFlag = TRUE;
	}
#endif
	return errorFlag;
}
#else
int writeTarFile(int tar_fd, int verboseFlag,
	int dereferenceFlag, const llist_t *include,
	const llist_t *exclude, int gzip);
#endif /* FEATURE_TAR_CREATE */

#if ENABLE_FEATURE_TAR_FROM
static llist_t *append_file_list_to_list(llist_t *list)
{
	FILE *src_stream;
	char *line;
	llist_t *newlist = NULL;

	while (list) {
		src_stream = xfopen_stdin(llist_pop(&list));
		while ((line = xmalloc_fgetline(src_stream)) != NULL) {
			/* kill trailing '/' unless the string is just "/" */
			char *cp = last_char_is(line, '/');
			if (cp > line)
				*cp = '\0';
			llist_add_to(&newlist, line);
		}
		fclose(src_stream);
	}
	return newlist;
}
#else
# define append_file_list_to_list(x) 0
#endif

//usage:#define tar_trivial_usage
//usage:	"-[" IF_FEATURE_TAR_CREATE("c") "xt"
//usage:	IF_FEATURE_SEAMLESS_Z("Z")
//usage:	IF_FEATURE_SEAMLESS_GZ("z")
//usage:	IF_FEATURE_SEAMLESS_BZ2("j")
//usage:	IF_FEATURE_SEAMLESS_LZMA("a")
//usage:	IF_FEATURE_TAR_CREATE("h")
//usage:	IF_FEATURE_TAR_NOPRESERVE_TIME("m")
//usage:	"vO] "
//usage:	IF_FEATURE_TAR_FROM("[-X FILE] [-T FILE] ")
//usage:	"[-f TARFILE] [-C DIR] [FILE]..."
//usage:#define tar_full_usage "\n\n"
//usage:	IF_FEATURE_TAR_CREATE("Create, extract, ")
//usage:	IF_NOT_FEATURE_TAR_CREATE("Extract ")
//usage:	"or list files from a tar file\n"
//usage:     "\nOperation:"
//usage:	IF_FEATURE_TAR_CREATE(
//usage:     "\n	c	Create"
//usage:	)
//usage:     "\n	x	Extract"
//usage:     "\n	t	List"
//usage:     "\n	f	Name of TARFILE ('-' for stdin/out)"
//usage:     "\n	C	Change to DIR before operation"
//usage:     "\n	v	Verbose"
//usage:	IF_FEATURE_SEAMLESS_Z(
//usage:     "\n	Z	(De)compress using compress"
//usage:	)
//usage:	IF_FEATURE_SEAMLESS_GZ(
//usage:     "\n	z	(De)compress using gzip"
//usage:	)
//usage:	IF_FEATURE_SEAMLESS_BZ2(
//usage:     "\n	j	(De)compress using bzip2"
//usage:	)
//usage:	IF_FEATURE_SEAMLESS_LZMA(
//usage:     "\n	a	(De)compress using lzma"
//usage:	)
//usage:     "\n	O	Extract to stdout"
//usage:	IF_FEATURE_TAR_CREATE(
//usage:     "\n	h	Follow symlinks"
//usage:	)
//usage:	IF_FEATURE_TAR_NOPRESERVE_TIME(
//usage:     "\n	m	Don't restore mtime"
//usage:	)
//usage:	IF_FEATURE_TAR_FROM(
//usage:	IF_FEATURE_TAR_LONG_OPTIONS(
//usage:     "\n	exclude	File to exclude"
//usage:	)
//usage:     "\n	X	File with names to exclude"
//usage:     "\n	T	File with names to include"
//usage:	)
//usage:
//usage:#define tar_example_usage
//usage:       "$ zcat /tmp/tarball.tar.gz | tar -xf -\n"
//usage:       "$ tar -cf /tmp/tarball.tar /usr/local\n"

// Supported but aren't in --help:
//	o	no-same-owner
//	p	same-permissions
//	k	keep-old
//	numeric-owner
//	no-same-permissions
//	overwrite
//IF_FEATURE_TAR_TO_COMMAND(
//	to-command
//)

enum {
	OPTBIT_KEEP_OLD = 8,
	IF_FEATURE_TAR_CREATE(   OPTBIT_CREATE      ,)
	IF_FEATURE_TAR_CREATE(   OPTBIT_DEREFERENCE ,)
	IF_FEATURE_SEAMLESS_BZ2( OPTBIT_BZIP2       ,)
	IF_FEATURE_SEAMLESS_LZMA(OPTBIT_LZMA        ,)
	IF_FEATURE_TAR_FROM(     OPTBIT_INCLUDE_FROM,)
	IF_FEATURE_TAR_FROM(     OPTBIT_EXCLUDE_FROM,)
	IF_FEATURE_SEAMLESS_GZ(  OPTBIT_GZIP        ,)
	IF_FEATURE_SEAMLESS_Z(   OPTBIT_COMPRESS    ,) // 16th bit
	IF_FEATURE_TAR_NOPRESERVE_TIME(OPTBIT_NOPRESERVE_TIME,)
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
	IF_FEATURE_TAR_TO_COMMAND(OPTBIT_2COMMAND   ,)
	OPTBIT_NUMERIC_OWNER,
	OPTBIT_NOPRESERVE_PERM,
	OPTBIT_OVERWRITE,
#endif
	OPT_TEST         = 1 << 0, // t
	OPT_EXTRACT      = 1 << 1, // x
	OPT_BASEDIR      = 1 << 2, // C
	OPT_TARNAME      = 1 << 3, // f
	OPT_2STDOUT      = 1 << 4, // O
	OPT_NOPRESERVE_OWNER = 1 << 5, // o == no-same-owner
	OPT_P            = 1 << 6, // p
	OPT_VERBOSE      = 1 << 7, // v
	OPT_KEEP_OLD     = 1 << 8, // k
	OPT_CREATE       = IF_FEATURE_TAR_CREATE(   (1 << OPTBIT_CREATE      )) + 0, // c
	OPT_DEREFERENCE  = IF_FEATURE_TAR_CREATE(   (1 << OPTBIT_DEREFERENCE )) + 0, // h
	OPT_BZIP2        = IF_FEATURE_SEAMLESS_BZ2( (1 << OPTBIT_BZIP2       )) + 0, // j
	OPT_LZMA         = IF_FEATURE_SEAMLESS_LZMA((1 << OPTBIT_LZMA        )) + 0, // a
	OPT_INCLUDE_FROM = IF_FEATURE_TAR_FROM(     (1 << OPTBIT_INCLUDE_FROM)) + 0, // T
	OPT_EXCLUDE_FROM = IF_FEATURE_TAR_FROM(     (1 << OPTBIT_EXCLUDE_FROM)) + 0, // X
	OPT_GZIP         = IF_FEATURE_SEAMLESS_GZ(  (1 << OPTBIT_GZIP        )) + 0, // z
	OPT_COMPRESS     = IF_FEATURE_SEAMLESS_Z(   (1 << OPTBIT_COMPRESS    )) + 0, // Z
	OPT_NOPRESERVE_TIME = IF_FEATURE_TAR_NOPRESERVE_TIME((1 << OPTBIT_NOPRESERVE_TIME)) + 0, // m
	OPT_2COMMAND        = IF_FEATURE_TAR_TO_COMMAND(  (1 << OPTBIT_2COMMAND       )) + 0, // to-command
	OPT_NUMERIC_OWNER   = IF_FEATURE_TAR_LONG_OPTIONS((1 << OPTBIT_NUMERIC_OWNER  )) + 0, // numeric-owner
	OPT_NOPRESERVE_PERM = IF_FEATURE_TAR_LONG_OPTIONS((1 << OPTBIT_NOPRESERVE_PERM)) + 0, // no-same-permissions
	OPT_OVERWRITE       = IF_FEATURE_TAR_LONG_OPTIONS((1 << OPTBIT_OVERWRITE      )) + 0, // overwrite

	OPT_ANY_COMPRESS = (OPT_BZIP2 | OPT_LZMA | OPT_GZIP | OPT_COMPRESS),
};
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
static const char tar_longopts[] ALIGN1 =
	"list\0"                No_argument       "t"
	"extract\0"             No_argument       "x"
	"directory\0"           Required_argument "C"
	"file\0"                Required_argument "f"
	"to-stdout\0"           No_argument       "O"
	/* do not restore owner */
	/* Note: GNU tar handles 'o' as no-same-owner only on extract,
	 * on create, 'o' is --old-archive. We do not support --old-archive. */
	"no-same-owner\0"       No_argument       "o"
	"same-permissions\0"    No_argument       "p"
	"verbose\0"             No_argument       "v"
	"keep-old\0"            No_argument       "k"
# if ENABLE_FEATURE_TAR_CREATE
	"create\0"              No_argument       "c"
	"dereference\0"         No_argument       "h"
# endif
# if ENABLE_FEATURE_SEAMLESS_BZ2
	"bzip2\0"               No_argument       "j"
# endif
# if ENABLE_FEATURE_SEAMLESS_LZMA
	"lzma\0"                No_argument       "a"
# endif
# if ENABLE_FEATURE_TAR_FROM
	"files-from\0"          Required_argument "T"
	"exclude-from\0"        Required_argument "X"
# endif
# if ENABLE_FEATURE_SEAMLESS_GZ
	"gzip\0"                No_argument       "z"
# endif
# if ENABLE_FEATURE_SEAMLESS_Z
	"compress\0"            No_argument       "Z"
# endif
# if ENABLE_FEATURE_TAR_NOPRESERVE_TIME
	"touch\0"               No_argument       "m"
# endif
# if ENABLE_FEATURE_TAR_TO_COMMAND
	"to-command\0"		Required_argument "\xfb"
# endif
	/* use numeric uid/gid from tar header, not textual */
	"numeric-owner\0"       No_argument       "\xfc"
	/* do not restore mode */
	"no-same-permissions\0" No_argument       "\xfd"
	/* on unpack, open with O_TRUNC and !O_EXCL */
	"overwrite\0"           No_argument       "\xfe"
	/* --exclude takes next bit position in option mask, */
	/* therefore we have to put it _after_ --no-same-permissions */
# if ENABLE_FEATURE_TAR_FROM
	"exclude\0"             Required_argument "\xff"
# endif
	;
#endif

int tar_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tar_main(int argc UNUSED_PARAM, char **argv)
{
	archive_handle_t *tar_handle;
	char *base_dir = NULL;
	const char *tar_filename = "-";
	unsigned opt;
	int verboseFlag = 0;
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
	llist_t *excludes = NULL;
#endif

	/* Initialise default values */
	tar_handle = init_handle();
	tar_handle->ah_flags = ARCHIVE_CREATE_LEADING_DIRS
	                     | ARCHIVE_RESTORE_DATE
	                     | ARCHIVE_UNLINK_OLD;

	/* Apparently only root's tar preserves perms (see bug 3844) */
	if (getuid() != 0)
		tar_handle->ah_flags |= ARCHIVE_DONT_RESTORE_PERM;

	/* Prepend '-' to the first argument if required */
	opt_complementary = "--:" // first arg is options
		"tt:vv:" // count -t,-v
		IF_FEATURE_TAR_FROM("X::T::") // cumulative lists
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
		"\xff::" // cumulative lists for --exclude
#endif
		IF_FEATURE_TAR_CREATE("c:") "t:x:" // at least one of these is reqd
		IF_FEATURE_TAR_CREATE("c--tx:t--cx:x--ct") // mutually exclusive
		IF_NOT_FEATURE_TAR_CREATE("t--x:x--t"); // mutually exclusive
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
	applet_long_options = tar_longopts;
#endif
#if ENABLE_DESKTOP
	if (argv[1] && argv[1][0] != '-') {
		/* Compat:
		 * 1st argument without dash handles options with parameters
		 * differently from dashed one: it takes *next argv[i]*
		 * as paramenter even if there are more chars in 1st argument:
		 *  "tar fx TARFILE" - "x" is not taken as f's param
		 *  but is interpreted as -x option
		 *  "tar -xf TARFILE" - dashed equivalent of the above
		 *  "tar -fx ..." - "x" is taken as f's param
		 * getopt32 wouldn't handle 1st command correctly.
		 * Unfortunately, people do use such commands.
		 * We massage argv[1] to work around it by moving 'f'
		 * to the end of the string.
		 * More contrived "tar fCx TARFILE DIR" still fails,
		 * but such commands are much less likely to be used.
		 */
		char *f = strchr(argv[1], 'f');
		if (f) {
			while (f[1] != '\0') {
				*f = f[1];
				f++;
			}
			*f = 'f';
		}
	}
#endif
	opt = getopt32(argv,
		"txC:f:Oopvk"
		IF_FEATURE_TAR_CREATE(   "ch"  )
		IF_FEATURE_SEAMLESS_BZ2( "j"   )
		IF_FEATURE_SEAMLESS_LZMA("a"   )
		IF_FEATURE_TAR_FROM(     "T:X:")
		IF_FEATURE_SEAMLESS_GZ(  "z"   )
		IF_FEATURE_SEAMLESS_Z(   "Z"   )
		IF_FEATURE_TAR_NOPRESERVE_TIME("m")
		, &base_dir // -C dir
		, &tar_filename // -f filename
		IF_FEATURE_TAR_FROM(, &(tar_handle->accept)) // T
		IF_FEATURE_TAR_FROM(, &(tar_handle->reject)) // X
		IF_FEATURE_TAR_TO_COMMAND(, &(tar_handle->tar__to_command)) // --to-command
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
		, &excludes // --exclude
#endif
		, &verboseFlag // combined count for -t and -v
		, &verboseFlag // combined count for -t and -v
		);
	//bb_error_msg("opt:%08x", opt);
	argv += optind;

	if (verboseFlag) tar_handle->action_header = header_verbose_list;
	if (verboseFlag == 1) tar_handle->action_header = header_list;

	if (opt & OPT_EXTRACT)
		tar_handle->action_data = data_extract_all;

	if (opt & OPT_2STDOUT)
		tar_handle->action_data = data_extract_to_stdout;

	if (opt & OPT_2COMMAND) {
		putenv((char*)"TAR_FILETYPE=f");
		signal(SIGPIPE, SIG_IGN);
		tar_handle->action_data = data_extract_to_command;
		IF_FEATURE_TAR_TO_COMMAND(tar_handle->tar__to_command_shell = xstrdup(get_shell_name());)
	}

	if (opt & OPT_KEEP_OLD)
		tar_handle->ah_flags &= ~ARCHIVE_UNLINK_OLD;

	if (opt & OPT_NUMERIC_OWNER)
		tar_handle->ah_flags |= ARCHIVE_NUMERIC_OWNER;

	if (opt & OPT_NOPRESERVE_OWNER)
		tar_handle->ah_flags |= ARCHIVE_DONT_RESTORE_OWNER;

	if (opt & OPT_NOPRESERVE_PERM)
		tar_handle->ah_flags |= ARCHIVE_DONT_RESTORE_PERM;

	if (opt & OPT_OVERWRITE) {
		tar_handle->ah_flags &= ~ARCHIVE_UNLINK_OLD;
		tar_handle->ah_flags |= ARCHIVE_O_TRUNC;
	}

	if (opt & OPT_NOPRESERVE_TIME)
		tar_handle->ah_flags &= ~ARCHIVE_RESTORE_DATE;

#if ENABLE_FEATURE_TAR_FROM
	tar_handle->reject = append_file_list_to_list(tar_handle->reject);
# if ENABLE_FEATURE_TAR_LONG_OPTIONS
	/* Append excludes to reject */
	while (excludes) {
		llist_t *next = excludes->link;
		excludes->link = tar_handle->reject;
		tar_handle->reject = excludes;
		excludes = next;
	}
# endif
	tar_handle->accept = append_file_list_to_list(tar_handle->accept);
#endif

	/* Setup an array of filenames to work with */
	/* TODO: This is the same as in ar, make a separate function? */
	while (*argv) {
		/* kill trailing '/' unless the string is just "/" */
		char *cp = last_char_is(*argv, '/');
		if (cp > *argv)
			*cp = '\0';
		llist_add_to_end(&tar_handle->accept, *argv);
		argv++;
	}

	if (tar_handle->accept || tar_handle->reject)
		tar_handle->filter = filter_accept_reject_list;

	/* Open the tar file */
	{
		int tar_fd = STDIN_FILENO;
		int flags = O_RDONLY;

		if (opt & OPT_CREATE) {
			/* Make sure there is at least one file to tar up */
			if (tar_handle->accept == NULL)
				bb_error_msg_and_die("empty archive");

			tar_fd = STDOUT_FILENO;
			/* Mimicking GNU tar 1.15.1: */
			flags = O_WRONLY | O_CREAT | O_TRUNC;
		}

		if (LONE_DASH(tar_filename)) {
			tar_handle->src_fd = tar_fd;
			tar_handle->seek = seek_by_read;
		} else {
			if (ENABLE_FEATURE_TAR_AUTODETECT
			 && flags == O_RDONLY
			 && !(opt & OPT_ANY_COMPRESS)
			) {
				tar_handle->src_fd = open_zipped(tar_filename);
				if (tar_handle->src_fd < 0)
					bb_perror_msg_and_die("can't open '%s'", tar_filename);
			} else {
				tar_handle->src_fd = xopen(tar_filename, flags);
			}
		}
	}

	if (base_dir)
		xchdir(base_dir);

	//if (SEAMLESS_COMPRESSION || OPT_COMPRESS)
	//	/* We need to know whether child (gzip/bzip/etc) exits abnormally */
	//	signal(SIGCHLD, check_errors_in_children);

	/* Create an archive */
	if (opt & OPT_CREATE) {
#if ENABLE_FEATURE_SEAMLESS_GZ || ENABLE_FEATURE_SEAMLESS_BZ2
		int zipMode = 0;
		if (ENABLE_FEATURE_SEAMLESS_GZ && (opt & OPT_GZIP))
			zipMode = 1;
		if (ENABLE_FEATURE_SEAMLESS_BZ2 && (opt & OPT_BZIP2))
			zipMode = 2;
#endif
		/* NB: writeTarFile() closes tar_handle->src_fd */
		return writeTarFile(tar_handle->src_fd, verboseFlag, opt & OPT_DEREFERENCE,
				tar_handle->accept,
				tar_handle->reject, zipMode);
	}

	if (opt & OPT_ANY_COMPRESS) {
		USE_FOR_MMU(IF_DESKTOP(long long) int FAST_FUNC (*xformer)(transformer_aux_data_t *aux, int src_fd, int dst_fd);)
		USE_FOR_NOMMU(const char *xformer_prog;)

		if (opt & OPT_COMPRESS)
			USE_FOR_MMU(xformer = unpack_Z_stream;)
			USE_FOR_NOMMU(xformer_prog = "uncompress";)
		if (opt & OPT_GZIP)
			USE_FOR_MMU(xformer = unpack_gz_stream;)
			USE_FOR_NOMMU(xformer_prog = "gunzip";)
		if (opt & OPT_BZIP2)
			USE_FOR_MMU(xformer = unpack_bz2_stream;)
			USE_FOR_NOMMU(xformer_prog = "bunzip2";)
		if (opt & OPT_LZMA)
			USE_FOR_MMU(xformer = unpack_lzma_stream;)
			USE_FOR_NOMMU(xformer_prog = "unlzma";)

		open_transformer_with_sig(tar_handle->src_fd, xformer, xformer_prog);
		/* Can't lseek over pipes */
		tar_handle->seek = seek_by_read;
		/*tar_handle->offset = 0; - already is */
	}

	while (get_header_tar(tar_handle) == EXIT_SUCCESS)
		continue;

	/* Check that every file that should have been extracted was */
	while (tar_handle->accept) {
		if (!find_list_entry(tar_handle->reject, tar_handle->accept->data)
		 && !find_list_entry(tar_handle->passed, tar_handle->accept->data)
		) {
			bb_error_msg_and_die("%s: not found in archive",
				tar_handle->accept->data);
		}
		tar_handle->accept = tar_handle->accept->link;
	}
	if (ENABLE_FEATURE_CLEAN_UP /* && tar_handle->src_fd != STDIN_FILENO */)
		close(tar_handle->src_fd);

	if (SEAMLESS_COMPRESSION || OPT_COMPRESS) {
		check_errors_in_children(0);
		return bb_got_signal;
	}
	return EXIT_SUCCESS;
}
