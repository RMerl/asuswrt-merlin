/* vi: set sw=4 ts=4: */
/*
 * dpkg-deb packs, unpacks and provides information about Debian archives.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//config:config DPKG_DEB
//config:	bool "dpkg_deb"
//config:	default n
//config:	select FEATURE_SEAMLESS_GZ
//config:	help
//config:	  dpkg-deb unpacks and provides information about Debian archives.
//config:
//config:	  This implementation of dpkg-deb cannot pack archives.
//config:
//config:	  Unless you have a specific application which requires dpkg-deb,
//config:	  say N here.
//config:
//config:config FEATURE_DPKG_DEB_EXTRACT_ONLY
//config:	bool "Extract only (-x)"
//config:	default n
//config:	depends on DPKG_DEB
//config:	help
//config:	  This reduces dpkg-deb to the equivalent of
//config:	  "ar -p <deb> data.tar.gz | tar -zx". However it saves space as none
//config:	  of the extra dpkg-deb, ar or tar options are needed, they are linked
//config:	  to internally.

//applet:IF_DPKG_DEB(APPLET_ODDNAME(dpkg-deb, dpkg_deb, BB_DIR_USR_BIN, BB_SUID_DROP, dpkg_deb))
//kbuild:lib-$(CONFIG_DPKG_DEB) += dpkg_deb.o

//usage:#define dpkg_deb_trivial_usage
//usage:       "[-cefxX] FILE [argument]"
//usage:#define dpkg_deb_full_usage "\n\n"
//usage:       "Perform actions on Debian packages (.debs)\n"
//usage:     "\n	-c	List contents of filesystem tree"
//usage:     "\n	-e	Extract control files to [argument] directory"
//usage:     "\n	-f	Display control field name starting with [argument]"
//usage:     "\n	-x	Extract packages filesystem tree to directory"
//usage:     "\n	-X	Verbose extract"
//usage:
//usage:#define dpkg_deb_example_usage
//usage:       "$ dpkg-deb -X ./busybox_0.48-1_i386.deb /tmp\n"

#include "libbb.h"
#include "bb_archive.h"

#define DPKG_DEB_OPT_CONTENTS         1
#define DPKG_DEB_OPT_CONTROL          2
#define DPKG_DEB_OPT_FIELD            4
#define DPKG_DEB_OPT_EXTRACT          8
#define DPKG_DEB_OPT_EXTRACT_VERBOSE 16

int dpkg_deb_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dpkg_deb_main(int argc, char **argv)
{
	archive_handle_t *ar_archive;
	archive_handle_t *tar_archive;
	llist_t *control_tar_llist = NULL;
	unsigned opt;
	const char *extract_dir;
	int need_args;

	/* Setup the tar archive handle */
	tar_archive = init_handle();

	/* Setup an ar archive handle that refers to the gzip sub archive */
	ar_archive = init_handle();
	ar_archive->dpkg__sub_archive = tar_archive;
	ar_archive->filter = filter_accept_list_reassign;

	llist_add_to(&ar_archive->accept, (char*)"data.tar");
	llist_add_to(&control_tar_llist, (char*)"control.tar");
#if ENABLE_FEATURE_SEAMLESS_GZ
	llist_add_to(&ar_archive->accept, (char*)"data.tar.gz");
	llist_add_to(&control_tar_llist, (char*)"control.tar.gz");
#endif
#if ENABLE_FEATURE_SEAMLESS_BZ2
	llist_add_to(&ar_archive->accept, (char*)"data.tar.bz2");
	llist_add_to(&control_tar_llist, (char*)"control.tar.bz2");
#endif
#if ENABLE_FEATURE_SEAMLESS_LZMA
	llist_add_to(&ar_archive->accept, (char*)"data.tar.lzma");
	llist_add_to(&control_tar_llist, (char*)"control.tar.lzma");
#endif
#if ENABLE_FEATURE_SEAMLESS_XZ
	llist_add_to(&ar_archive->accept, (char*)"data.tar.xz");
	llist_add_to(&control_tar_llist, (char*)"control.tar.xz");
#endif

	opt_complementary = "c--efXx:e--cfXx:f--ceXx:X--cefx:x--cefX";
	opt = getopt32(argv, "cefXx");
	argv += optind;
	argc -= optind;

	if (opt & DPKG_DEB_OPT_CONTENTS) {
		tar_archive->action_header = header_verbose_list;
	}
	extract_dir = NULL;
	need_args = 1;
	if (opt & DPKG_DEB_OPT_CONTROL) {
		ar_archive->accept = control_tar_llist;
		tar_archive->action_data = data_extract_all;
		if (1 == argc) {
			extract_dir = "./DEBIAN";
		} else {
			need_args++;
		}
	}
	if (opt & DPKG_DEB_OPT_FIELD) {
		/* Print the entire control file
		 * it should accept a second argument which specifies a
		 * specific field to print */
		ar_archive->accept = control_tar_llist;
		llist_add_to(&(tar_archive->accept), (char*)"./control");
		tar_archive->filter = filter_accept_list;
		tar_archive->action_data = data_extract_to_stdout;
	}
	if (opt & DPKG_DEB_OPT_EXTRACT) {
		tar_archive->action_header = header_list;
	}
	if (opt & (DPKG_DEB_OPT_EXTRACT_VERBOSE | DPKG_DEB_OPT_EXTRACT)) {
		tar_archive->action_data = data_extract_all;
		need_args = 2;
	}

	if (need_args != argc) {
		bb_show_usage();
	}

	tar_archive->src_fd = ar_archive->src_fd = xopen(argv[0], O_RDONLY);

	/* Work out where to extract the files */
	/* 2nd argument is a dir name */
	if (argv[1]) {
		extract_dir = argv[1];
	}
	if (extract_dir) {
		mkdir(extract_dir, 0777); /* bb_make_directory(extract_dir, 0777, 0) */
		xchdir(extract_dir);
	}

	/* Do it */
	unpack_ar_archive(ar_archive);

	/* Cleanup */
	if (ENABLE_FEATURE_CLEAN_UP)
		close(ar_archive->src_fd);

	return EXIT_SUCCESS;
}
