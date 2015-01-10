/*
 * mkswap.c - set up a linux swap device
 *
 * (C) 1991 Linus Torvalds. This file may be redistributed as per
 * the Linux copyright.
 */

/*
 * 20.12.91  -	time began. Got VM working yesterday by doing this by hand.
 *
 * Usage: mkswap [-c] [-vN] [-f] device [size-in-blocks]
 *
 *	-c   for readability checking. (Use it unless you are SURE!)
 *	-vN  for swap areas version N. (Only N=0,1 known today.)
 *      -f   for forcing swap creation even if it would smash partition table.
 *
 * The device may be a block device or an image of one, but this isn't
 * enforced (but it's not much fun on a character device :-).
 *
 * Patches from jaggy@purplet.demon.co.uk (Mike Jagdis) to make the
 * size-in-blocks parameter optional added Wed Feb  8 10:33:43 1995.
 *
 * Version 1 swap area code (for kernel 2.1.117), aeb, 981010.
 *
 * Sparc fixes, jj@ultra.linux.cz (Jakub Jelinek), 981201 - mangled by aeb.
 * V1_MAX_PAGES fixes, jj, 990325.
 * sparc64 fixes, jj, 000219.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <mntent.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#include "linux_version.h"
#include "swapheader.h"
#include "strutils.h"
#include "nls.h"
#include "blkdev.h"
#include "pathnames.h"
#include "wholedisk.h"
#include "writeall.h"
#include "xalloc.h"
#include "c.h"

#ifdef HAVE_LIBUUID
# include <uuid.h>
#endif

#ifdef HAVE_LIBBLKID
# include <blkid.h>
#endif

static char *device_name = NULL;
static int DEV = -1;
static unsigned long long PAGES = 0;
static unsigned long badpages = 0;
static int check = 0;

#define SELINUX_SWAPFILE_TYPE	"swapfile_t"

#ifdef __sparc__
# ifdef __arch64__
#  define is_sparc64() 1
#  define is_be64() 1
# else /* sparc32 */
static int
is_sparc64(void)
{
	struct utsname un;
	static int sparc64 = -1;

	if (sparc64 != -1)
		return sparc64;
	sparc64 = 0;

	if (uname(&un) < 0)
		return 0;
	if (! strcmp(un.machine, "sparc64")) {
		sparc64 = 1;
		return 1;
	}
	if (strcmp(un.machine, "sparc"))
		return 0; /* Should not happen */

#ifdef HAVE_PERSONALITY
	{
		extern int personality(unsigned long);
		int oldpers;
#define PERS_LINUX          0x00000000
#define PERS_LINUX_32BIT    0x00800000
#define PERS_LINUX32        0x00000008

		oldpers = personality(PERS_LINUX_32BIT);
		if (oldpers != -1) {
			if (personality(PERS_LINUX) != -1) {
				uname(&un);
				if (! strcmp(un.machine, "sparc64")) {
					sparc64 = 1;
					oldpers = PERS_LINUX32;
				}
			}
			personality(oldpers);
		}
	}
#endif

	return sparc64;
}
#  define is_be64() is_sparc64()
# endif /* sparc32 */
#else /* !sparc */
# define is_be64() 0
#endif

/*
 * The definition of the union swap_header uses the kernel constant PAGE_SIZE.
 * Unfortunately, on some architectures this depends on the hardware model, and
 * can only be found at run time -- we use getpagesize(), so that we do not
 * need separate binaries e.g. for sun4, sun4c/d/m and sun4u.
 *
 * Even more unfortunately, getpagesize() does not always return the right
 * information. For example, libc4, libc5 and glibc 2.0 do not use the system
 * call but invent a value themselves (EXEC_PAGESIZE or NBPG * CLSIZE or NBPC),
 * and thus it may happen that e.g. on a sparc kernel PAGE_SIZE=4096 and
 * getpagesize() returns 8192.
 *
 * What to do? Let us allow the user to specify the pagesize explicitly.
 *
 */
static long user_pagesize;
static int pagesize;
static unsigned long *signature_page = NULL;

static void
init_signature_page(void)
{

	int kernel_pagesize = pagesize = getpagesize();

	if (user_pagesize) {
		if ((user_pagesize & (user_pagesize - 1)) ||
		    user_pagesize < (long) sizeof(struct swap_header_v1_2) + 10)
			errx(EXIT_FAILURE,
				_("Bad user-specified page size %lu"),
				user_pagesize);
		pagesize = user_pagesize;
	}

	if (user_pagesize && user_pagesize != kernel_pagesize)
		warnx(_("Using user-specified page size %d, "
				"instead of the system value %d"),
				pagesize, kernel_pagesize);

	signature_page = (unsigned long *) xcalloc(1, pagesize);
}

static void
write_signature(char *sig)
{
	char *sp = (char *) signature_page;

	strncpy(sp + pagesize - 10, sig, 10);
}

static void
write_uuid_and_label(unsigned char *uuid, char *volume_name)
{
	struct swap_header_v1_2 *h;

	/* Sanity check */
	if (sizeof(struct swap_header_v1) !=
	    sizeof(struct swap_header_v1_2)) {
		warnx(_("Bad swap header size, no label written."));
		return;
	}

	h = (struct swap_header_v1_2 *) signature_page;
	if (uuid)
		memcpy(h->uuid, uuid, sizeof(h->uuid));
	if (volume_name) {
		xstrncpy(h->volume_name, volume_name, sizeof(h->volume_name));
		if (strlen(volume_name) > strlen(h->volume_name))
			warnx(_("Label was truncated."));
	}
	if (uuid || volume_name) {
		if (volume_name)
			printf("LABEL=%s, ", h->volume_name);
		else
			printf(_("no label, "));
#ifdef HAVE_LIBUUID
		if (uuid) {
			char uuid_string[37];
			uuid_unparse(uuid, uuid_string);
			printf("UUID=%s\n", uuid_string);
		} else
#endif
			printf(_("no uuid\n"));
	}
}

/*
 * Find out what the maximum amount of swap space is that the kernel will
 * handle.  This wouldn't matter if the kernel just used as much of the
 * swap space as it can handle, but until 2.3.4 it would return an error
 * to swapon() if the swapspace was too large.
 */
/* Before 2.2.0pre9 */
#define V1_OLD_MAX_PAGES	((0x7fffffff / pagesize) - 1)
/* Since 2.2.0pre9, before 2.3.4:
   error if nr of pages >= SWP_OFFSET(SWP_ENTRY(0,~0UL))
   with variations on
	#define SWP_ENTRY(type,offset) (((type) << 1) | ((offset) << 8))
	#define SWP_OFFSET(entry) ((entry) >> 8)
   on the various architectures. Below the result - yuk.

   Machine	pagesize	SWP_ENTRY	SWP_OFFSET	bound+1	oldbound+2
   i386		2^12		o<<8		e>>8		1<<24	1<<19
   mips		2^12		o<<15		e>>15		1<<17	1<<19
   alpha	2^13		o<<40		e>>40		1<<24	1<<18
   m68k		2^12		o<<12		e>>12		1<<20	1<<19
   sparc	2^{12,13}	(o&0x3ffff)<<9	(e>>9)&0x3ffff	1<<18	1<<{19,18}
   sparc64	2^13		o<<13		e>>13		1<<51	1<<18
   ppc		2^12		o<<8		e>>8		1<<24	1<<19
   armo		2^{13,14,15}	o<<8		e>>8		1<<24	1<<{18,17,16}
   armv		2^12		o<<9		e>>9		1<<23	1<<19

   assuming that longs have 64 bits on alpha and sparc64 and 32 bits elsewhere.

   The bad part is that we need to know this since the kernel will
   refuse a swap space if it is too large.
*/
/* patch from jj - why does this differ from the above? */
/* 32bit kernels have a second limitation of 2GB, sparc64 is limited by
   the size of virtual address space allocation for vmalloc */
#if defined(__alpha__)
#define V1_MAX_PAGES           ((1 << 24) - 1)
#elif defined(__mips__)
#define V1_MAX_PAGES           ((1 << 17) - 1)
#elif defined(__sparc__)
#define V1_MAX_PAGES           (is_sparc64() ? ((3 << 29) - 1) : ((1 << 18) - 1))
#elif defined(__ia64__)
/*
 * The actual size will depend on the amount of virtual address space
 * available to vmalloc the swap map.
 */
#define V1_MAX_PAGES          ((1UL << 54) - 1)
#else
#define V1_MAX_PAGES           V1_OLD_MAX_PAGES
#endif
/* man page now says:
The maximum useful size of a swap area now depends on the architecture.
It is roughly 2GB on i386, PPC, m68k, ARM, 1GB on sparc, 512MB on mips,
128GB on alpha and 3TB on sparc64.
*/

#define MAX_BADPAGES	((pagesize-1024-128*sizeof(int)-10)/sizeof(int))
#define MIN_GOODPAGES	10

static void __attribute__ ((__noreturn__)) usage(FILE *out)
{
	fprintf(out,
		_("\nUsage:\n"
		  " %s [options] device [size]\n"),
		program_invocation_short_name);

	fprintf(out, _(
		"\nOptions:\n"
		" -c, --check               check bad blocks before creating the swap area\n"
		" -f, --force               allow swap size area be larger than device\n"
		" -p, --pagesize SIZE       specify page size in bytes\n"
		" -L, --label LABEL         specify label\n"
		" -v, --swapversion NUM     specify swap-space version number\n"
		" -U, --uuid UUID           specify the uuid to use\n"
		" -V, --version             output version information and exit\n"
		" -h, --help                display this help and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
page_bad(int page)
{
	struct swap_header_v1_2 *p = (struct swap_header_v1_2 *) signature_page;

	if (badpages == MAX_BADPAGES)
		errx(EXIT_FAILURE, _("too many bad pages"));
	p->badpages[badpages] = page;
	badpages++;
}

static void
check_blocks(void)
{
	unsigned int current_page;
	int do_seek = 1;
	char *buffer;

	buffer = xmalloc(pagesize);
	current_page = 0;
	while (current_page < PAGES) {
		if (do_seek && lseek(DEV,current_page*pagesize,SEEK_SET) !=
		    current_page*pagesize)
			errx(EXIT_FAILURE, _("seek failed in check_blocks"));
		if ((do_seek = (pagesize != read(DEV, buffer, pagesize))))
			page_bad(current_page);
		current_page++;
	}
	if (badpages == 1)
		printf(_("one bad page\n"));
	else if (badpages > 1)
		printf(_("%lu bad pages\n"), badpages);
	free(buffer);
}

/* return size in pages */
static unsigned long long
get_size(const char *file)
{
	int fd;
	unsigned long long size;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		perror(file);
		exit(EXIT_FAILURE);
	}
	if (blkdev_get_size(fd, &size) == 0)
		size /= pagesize;

	close(fd);
	return size;
}

/*
 * Check to make certain that our new filesystem won't be created on
 * an already mounted partition.  Code adapted from mke2fs, Copyright
 * (C) 1994 Theodore Ts'o.  Also licensed under GPL.
 * (C) 2006 Karel Zak -- port to mkswap
 */
static int
check_mount(void)
{
	FILE *f;
	struct mntent *mnt;

	if ((f = setmntent (_PATH_MOUNTED, "r")) == NULL)
		return 0;
	while ((mnt = getmntent (f)) != NULL)
		if (strcmp (device_name, mnt->mnt_fsname) == 0)
			break;
	endmntent (f);
	if (!mnt)
		return 0;
	return 1;
}

#ifdef HAVE_LIBBLKID
static blkid_probe
new_prober(int fd)
{
	blkid_probe pr = blkid_new_probe();
	if (!pr)
		errx(EXIT_FAILURE, _("unable to alloc new libblkid probe"));
	if (blkid_probe_set_device(pr, fd, 0, 0))
		errx(EXIT_FAILURE, _("unable to assign device to libblkid probe"));
	return pr;
}
#endif

static void
wipe_device(int fd, const char *devname, int force, int is_blkdev)
{
	char *type = NULL;
	int whole = 0;
	int zap = 1;
#ifdef HAVE_LIBBLKID
	blkid_probe pr = NULL;
#endif
	if (!force) {
		if (lseek(fd, 0, SEEK_SET) != 0)
			errx(EXIT_FAILURE, _("unable to rewind swap-device"));

		if (is_blkdev && is_whole_disk_fd(fd, devname)) {
			/* don't zap bootbits on whole disk -- we know nothing
			 * about bootloaders on the device */
			whole = 1;
			zap = 0;
		} else {
#ifdef HAVE_LIBBLKID
			pr = new_prober(fd);
			blkid_probe_enable_partitions(pr, 1);
			blkid_probe_enable_superblocks(pr, 0);

			if (blkid_do_fullprobe(pr) == 0 &&
			    blkid_probe_lookup_value(pr, "PTTYPE",
					(const char **) &type, NULL) == 0 &&
			    type) {
				type = xstrdup(type);
				zap = 0;
			}
#else
			/* don't zap if compiled without libblkid */
			zap = 0;
#endif
		}
	}

	if (zap) {
		/*
		 * Wipe boodbits
		 */
		char buf[1024];

		if (lseek(fd, 0, SEEK_SET) != 0)
			errx(EXIT_FAILURE, _("unable to rewind swap-device"));

		memset(buf, 0, sizeof(buf));
		if (write_all(fd, buf, sizeof(buf)))
			errx(EXIT_FAILURE, _("unable to erase bootbits sectors"));
#ifdef HAVE_LIBBLKID
		/*
		 * Wipe rest of the device
		 */
		if (!pr)
			pr = new_prober(fd);

		blkid_probe_enable_superblocks(pr, 1);
		blkid_probe_enable_partitions(pr, 0);
		blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC);

		while (blkid_do_probe(pr) == 0)
			blkid_do_wipe(pr, 0);
#endif
	} else {
		warnx(_("%s: warning: don't erase bootbits sectors"),
			devname);
		if (type)
			fprintf(stderr, _("        (%s partition table detected). "), type);
		else if (whole)
			fprintf(stderr, _("        on whole disk. "));
		else
			fprintf(stderr, _("        (compiled without libblkid). "));
		fprintf(stderr, "Use -f to force.\n");
	}
#ifdef HAVE_LIBBLKID
	blkid_free_probe(pr);
#endif
}

int
main(int argc, char **argv) {
	struct stat statbuf;
	struct swap_header_v1_2 *hdr;
	int c;
	unsigned long long maxpages;
	unsigned long long goodpages;
	unsigned long long sz;
	off_t offset;
	int force = 0;
	long version = 1;
	char *block_count = 0;
	char *opt_label = NULL;
	unsigned char *uuid = NULL;
#ifdef HAVE_LIBUUID
	const char *opt_uuid = NULL;
	uuid_t uuid_dat;
#endif
	static const struct option longopts[] = {
		{ "check",       no_argument,       0, 'c' },
		{ "force",       no_argument,       0, 'f' },
		{ "pagesize",    required_argument, 0, 'p' },
		{ "label",       required_argument, 0, 'L' },
		{ "swapversion", required_argument, 0, 'v' },
		{ "uuid",        required_argument, 0, 'U' },
		{ "version",     no_argument,       0, 'V' },
		{ "help",        no_argument,       0, 'h' },
		{ NULL,          0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while((c = getopt_long(argc, argv, "cfp:L:v:U:Vh", longopts, NULL)) != -1) {
		switch (c) {
		case 'c':
			check=1;
			break;
		case 'f':
			force=1;
			break;
		case 'p':
			user_pagesize = strtol_or_err(optarg, _("parse page size failed"));
			break;
		case 'L':
			opt_label = optarg;
			break;
		case 'v':
			version = strtol_or_err(optarg, _("parse version number failed"));
			break;
		case 'U':
#ifdef HAVE_LIBUUID
			opt_uuid = optarg;
#else
			warnx(_("warning: ignore -U (UUIDs are unsupported by %s)"),
				program_invocation_short_name);
#endif
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}
	}
	if (optind < argc)
		device_name = argv[optind++];
	if (optind < argc)
		block_count = argv[optind++];
	if (optind != argc) {
		warnx(("only one device as argument is currently supported."));
		usage(stderr);
	}

	if (version != 1)
		errx(EXIT_FAILURE,
			_("does not support swapspace version %lu."),
			version);

#ifdef HAVE_LIBUUID
	if(opt_uuid) {
		if (uuid_parse(opt_uuid, uuid_dat) != 0)
			errx(EXIT_FAILURE, _("error: UUID parsing failed"));
	} else
		uuid_generate(uuid_dat);
	uuid = uuid_dat;
#endif

	init_signature_page();	/* get pagesize */

	if (!device_name) {
		warnx(_("error: Nowhere to set up swap on?"));
		usage(stderr);
	}
	if (block_count) {
		/* this silly user specified the number of blocks explicitly */
		long long blks;

		blks = strtoll_or_err(block_count, "parse block count failed");
		if (blks < 0)
			usage(stderr);

		PAGES = blks / (pagesize / 1024);
	}
	sz = get_size(device_name);
	if (!PAGES)
		PAGES = sz;
	else if (PAGES > sz && !force) {
		errx(EXIT_FAILURE,
			_("error: "
			  "size %llu KiB is larger than device size %llu KiB"),
			PAGES*(pagesize/1024), sz*(pagesize/1024));
	}

	if (PAGES < MIN_GOODPAGES) {
		warnx(_("error: swap area needs to be at least %ld KiB"),
			(long)(MIN_GOODPAGES * pagesize/1024));
		usage(stderr);
	}

#ifdef __linux__
	if (get_linux_version() >= KERNEL_VERSION(2,3,4))
		maxpages = UINT_MAX + 1ULL;
	else if (get_linux_version() >= KERNEL_VERSION(2,2,1))
		maxpages = V1_MAX_PAGES;
	else
#endif
		maxpages = V1_OLD_MAX_PAGES;

	if (PAGES > maxpages) {
		PAGES = maxpages;
		warnx(_("warning: truncating swap area to %llu KiB"),
			PAGES * pagesize / 1024);
	}

	if (stat(device_name, &statbuf) < 0) {
		perror(device_name);
		exit(EXIT_FAILURE);
	}
	if (S_ISBLK(statbuf.st_mode))
		DEV = open(device_name, O_RDWR | O_EXCL);
	else
		DEV = open(device_name, O_RDWR);

	if (DEV < 0) {
		perror(device_name);
		exit(EXIT_FAILURE);
	}

	/* Want a block device. Probably not /dev/hda or /dev/hdb. */
	if (!S_ISBLK(statbuf.st_mode))
		check=0;
	else if (statbuf.st_rdev == 0x0300 || statbuf.st_rdev == 0x0340)
		errx(EXIT_FAILURE, _("error: "
			"will not try to make swapdevice on '%s'"),
			device_name);
	else if (check_mount())
		errx(EXIT_FAILURE, _("error: "
			"%s is mounted; will not make swapspace."),
			device_name);
	else if (blkdev_is_misaligned(DEV))
		warnx(_("warning: %s is misaligned"), device_name);

	if (check)
		check_blocks();

	wipe_device(DEV, device_name, force, S_ISBLK(statbuf.st_mode));

	hdr = (struct swap_header_v1_2 *) signature_page;
	hdr->version = 1;
	hdr->last_page = PAGES - 1;
	hdr->nr_badpages = badpages;

	if (badpages > PAGES - MIN_GOODPAGES)
		errx(EXIT_FAILURE, _("Unable to set up swap-space: unreadable"));

	goodpages = PAGES - badpages - 1;
	printf(_("Setting up swapspace version 1, size = %llu KiB\n"),
		goodpages * pagesize / 1024);

	write_signature("SWAPSPACE2");
	write_uuid_and_label(uuid, opt_label);

	offset = 1024;
	if (lseek(DEV, offset, SEEK_SET) != offset)
		errx(EXIT_FAILURE, _("unable to rewind swap-device"));
	if (write_all(DEV, (char *) signature_page + offset,
				    pagesize - offset) == -1)
		err(EXIT_FAILURE,
			_("%s: unable to write signature page"),
			device_name);

	/*
	 * A subsequent swapon() will fail if the signature
	 * is not actually on disk. (This is a kernel bug.)
	 */
#ifdef HAVE_FSYNC
	if (fsync(DEV))
		errx(EXIT_FAILURE, _("fsync failed"));
#endif

#ifdef HAVE_LIBSELINUX
	if (S_ISREG(statbuf.st_mode) && is_selinux_enabled() > 0) {
		security_context_t context_string;
		security_context_t oldcontext;
		context_t newcontext;

		if (fgetfilecon(DEV, &oldcontext) < 0) {
			if (errno != ENODATA)
				err(EXIT_FAILURE,
					_("%s: unable to obtain selinux file label"),
					device_name);
			if (matchpathcon(device_name, statbuf.st_mode, &oldcontext))
				errx(EXIT_FAILURE, _("unable to matchpathcon()"));
		}
		if (!(newcontext = context_new(oldcontext)))
			errx(EXIT_FAILURE, _("unable to create new selinux context"));
		if (context_type_set(newcontext, SELINUX_SWAPFILE_TYPE))
			errx(EXIT_FAILURE, _("couldn't compute selinux context"));

		context_string = context_str(newcontext);

		if (strcmp(context_string, oldcontext)!=0) {
			if (fsetfilecon(DEV, context_string))
				err(EXIT_FAILURE, _("unable to relabel %s to %s"),
						device_name, context_string);
		}
		context_free(newcontext);
		freecon(oldcontext);
	}
#endif
	return EXIT_SUCCESS;
}
