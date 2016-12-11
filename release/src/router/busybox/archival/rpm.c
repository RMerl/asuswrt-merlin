/* vi: set sw=4 ts=4: */
/*
 * Mini rpm applet for busybox
 *
 * Copyright (C) 2001,2002 by Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//config:config RPM
//config:	bool "rpm"
//config:	default y
//config:	help
//config:	  Mini RPM applet - queries and extracts RPM packages.

//applet:IF_RPM(APPLET(rpm, BB_DIR_BIN, BB_SUID_DROP))
//kbuild:lib-$(CONFIG_RPM) += rpm.o

//usage:#define rpm_trivial_usage
//usage:       "-i PACKAGE.rpm; rpm -qp[ildc] PACKAGE.rpm"
//usage:#define rpm_full_usage "\n\n"
//usage:       "Manipulate RPM packages\n"
//usage:     "\nCommands:"
//usage:     "\n	-i	Install package"
//usage:     "\n	-qp	Query package"
//usage:     "\n	-qpi	Show information"
//usage:     "\n	-qpl	List contents"
//usage:     "\n	-qpd	List documents"
//usage:     "\n	-qpc	List config files"

#include "libbb.h"
#include "common_bufsiz.h"
#include "bb_archive.h"
#include "rpm.h"

#define RPM_CHAR_TYPE           1
#define RPM_INT8_TYPE           2
#define RPM_INT16_TYPE          3
#define RPM_INT32_TYPE          4
/* #define RPM_INT64_TYPE       5   ---- These aren't supported (yet) */
#define RPM_STRING_TYPE         6
#define RPM_BIN_TYPE            7
#define RPM_STRING_ARRAY_TYPE   8
#define RPM_I18NSTRING_TYPE     9

#define TAG_NAME                1000
#define TAG_VERSION             1001
#define TAG_RELEASE             1002
#define TAG_SUMMARY             1004
#define TAG_DESCRIPTION         1005
#define TAG_BUILDTIME           1006
#define TAG_BUILDHOST           1007
#define TAG_SIZE                1009
#define TAG_VENDOR              1011
#define TAG_LICENSE             1014
#define TAG_PACKAGER            1015
#define TAG_GROUP               1016
#define TAG_URL                 1020
#define TAG_PREIN               1023
#define TAG_POSTIN              1024
#define TAG_FILEFLAGS           1037
#define TAG_FILEUSERNAME        1039
#define TAG_FILEGROUPNAME       1040
#define TAG_SOURCERPM           1044
#define TAG_PREINPROG           1085
#define TAG_POSTINPROG          1086
#define TAG_PREFIXS             1098
#define TAG_DIRINDEXES          1116
#define TAG_BASENAMES           1117
#define TAG_DIRNAMES            1118

#define RPMFILE_CONFIG          (1 << 0)
#define RPMFILE_DOC             (1 << 1)

enum rpm_functions_e {
	rpm_query = 1,
	rpm_install = 2,
	rpm_query_info = 4,
	rpm_query_package = 8,
	rpm_query_list = 16,
	rpm_query_list_doc = 32,
	rpm_query_list_config = 64
};

typedef struct {
	uint32_t tag; /* 4 byte tag */
	uint32_t type; /* 4 byte type */
	uint32_t offset; /* 4 byte offset */
	uint32_t count; /* 4 byte count */
} rpm_index;

struct globals {
	void *map;
	rpm_index **mytags;
	int tagcount;
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { setup_common_bufsiz(); } while (0)

static void extract_cpio(int fd, const char *source_rpm)
{
	archive_handle_t *archive_handle;

	if (source_rpm != NULL) {
		/* Binary rpm (it was built from some SRPM), install to root */
		xchdir("/");
	} /* else: SRPM, install to current dir */

	/* Initialize */
	archive_handle = init_handle();
	archive_handle->seek = seek_by_read;
	archive_handle->action_data = data_extract_all;
#if 0 /* For testing (rpm -i only lists the files in internal cpio): */
	archive_handle->action_header = header_list;
	archive_handle->action_data = data_skip;
#endif
	archive_handle->ah_flags = ARCHIVE_RESTORE_DATE | ARCHIVE_CREATE_LEADING_DIRS
		/* compat: overwrite existing files.
		 * try "rpm -i foo.src.rpm" few times in a row -
		 * standard rpm will not complain.
		 */
		| ARCHIVE_REPLACE_VIA_RENAME;
	archive_handle->src_fd = fd;
	/*archive_handle->offset = 0; - init_handle() did it */

	setup_unzip_on_fd(archive_handle->src_fd, /*fail_if_not_compressed:*/ 1);
	while (get_header_cpio(archive_handle) == EXIT_SUCCESS)
		continue;
}

static rpm_index **rpm_gettags(int fd, int *num_tags)
{
	/* We should never need more than 200 (shrink via realloc later) */
	rpm_index **tags = xzalloc(200 * sizeof(tags[0]));
	int pass, tagindex = 0;

	xlseek(fd, 96, SEEK_CUR); /* Seek past the unused lead */

	/* 1st pass is the signature headers, 2nd is the main stuff */
	for (pass = 0; pass < 2; pass++) {
		struct rpm_header header;
		rpm_index *tmpindex;
		int storepos;

		xread(fd, &header, sizeof(header));
		if (header.magic_and_ver != htonl(RPM_HEADER_MAGICnVER))
			return NULL; /* Invalid magic, or not version 1 */
		header.size = ntohl(header.size);
		header.entries = ntohl(header.entries);
		storepos = xlseek(fd, 0, SEEK_CUR) + header.entries * 16;

		while (header.entries--) {
			tmpindex = tags[tagindex++] = xmalloc(sizeof(*tmpindex));
			xread(fd, tmpindex, sizeof(*tmpindex));
			tmpindex->tag = ntohl(tmpindex->tag);
			tmpindex->type = ntohl(tmpindex->type);
			tmpindex->count = ntohl(tmpindex->count);
			tmpindex->offset = storepos + ntohl(tmpindex->offset);
			if (pass == 0)
				tmpindex->tag -= 743;
		}
		storepos = xlseek(fd, header.size, SEEK_CUR); /* Seek past store */
		/* Skip padding to 8 byte boundary after reading signature headers */
		if (pass == 0)
			xlseek(fd, (-storepos) & 0x7, SEEK_CUR);
	}
	/* realloc tags to save space */
	tags = xrealloc(tags, tagindex * sizeof(tags[0]));
	*num_tags = tagindex;
	/* All done, leave the file at the start of the gzipped cpio archive */
	return tags;
}

static int bsearch_rpmtag(const void *key, const void *item)
{
	int *tag = (int *)key;
	rpm_index **tmp = (rpm_index **) item;
	return (*tag - tmp[0]->tag);
}

static int rpm_getcount(int tag)
{
	rpm_index **found;
	found = bsearch(&tag, G.mytags, G.tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found)
		return 0;
	return found[0]->count;
}

static char *rpm_getstr(int tag, int itemindex)
{
	rpm_index **found;
	found = bsearch(&tag, G.mytags, G.tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found || itemindex >= found[0]->count)
		return NULL;
	if (found[0]->type == RPM_STRING_TYPE
	 || found[0]->type == RPM_I18NSTRING_TYPE
	 || found[0]->type == RPM_STRING_ARRAY_TYPE
	) {
		int n;
		char *tmpstr = (char *) G.map + found[0]->offset;
		for (n = 0; n < itemindex; n++)
			tmpstr = tmpstr + strlen(tmpstr) + 1;
		return tmpstr;
	}
	return NULL;
}

static int rpm_getint(int tag, int itemindex)
{
	rpm_index **found;
	char *tmpint;

	/* gcc throws warnings here when sizeof(void*)!=sizeof(int) ...
	 * it's ok to ignore it because tag won't be used as a pointer */
	found = bsearch(&tag, G.mytags, G.tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found || itemindex >= found[0]->count)
		return -1;

	tmpint = (char *) G.map + found[0]->offset;
	if (found[0]->type == RPM_INT32_TYPE) {
		tmpint += itemindex*4;
		return ntohl(*(int32_t*)tmpint);
	}
	if (found[0]->type == RPM_INT16_TYPE) {
		tmpint += itemindex*2;
		return ntohs(*(int16_t*)tmpint);
	}
	if (found[0]->type == RPM_INT8_TYPE) {
		tmpint += itemindex;
		return *(int8_t*)tmpint;
	}
	return -1;
}

static void fileaction_dobackup(char *filename, int fileref)
{
	struct stat oldfile;
	int stat_res;
	char *newname;
	if (rpm_getint(TAG_FILEFLAGS, fileref) & RPMFILE_CONFIG) {
		/* Only need to backup config files */
		stat_res = lstat(filename, &oldfile);
		if (stat_res == 0 && S_ISREG(oldfile.st_mode)) {
			/* File already exists  - really should check MD5's etc to see if different */
			newname = xasprintf("%s.rpmorig", filename);
			copy_file(filename, newname, FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS);
			remove_file(filename, FILEUTILS_RECUR | FILEUTILS_FORCE);
			free(newname);
		}
	}
}

static void fileaction_setowngrp(char *filename, int fileref)
{
	/* real rpm warns: "user foo does not exist - using <you>" */
	struct passwd *pw = getpwnam(rpm_getstr(TAG_FILEUSERNAME, fileref));
	int uid = pw ? pw->pw_uid : getuid(); /* or euid? */
	struct group *gr = getgrnam(rpm_getstr(TAG_FILEGROUPNAME, fileref));
	int gid = gr ? gr->gr_gid : getgid();
	chown(filename, uid, gid);
}

static void loop_through_files(int filetag, void (*fileaction)(char *filename, int fileref))
{
	int count = 0;
	while (rpm_getstr(filetag, count)) {
		char* filename = xasprintf("%s%s",
			rpm_getstr(TAG_DIRNAMES, rpm_getint(TAG_DIRINDEXES, count)),
			rpm_getstr(TAG_BASENAMES, count));
		fileaction(filename, count++);
		free(filename);
	}
}

int rpm_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rpm_main(int argc, char **argv)
{
	int opt, func = 0;
	const unsigned pagesize = getpagesize();

	while ((opt = getopt(argc, argv, "iqpldc")) != -1) {
		switch (opt) {
		case 'i': /* First arg: Install mode, with q: Information */
			if (!func) func = rpm_install;
			else func |= rpm_query_info;
			break;
		case 'q': /* First arg: Query mode */
			if (func) bb_show_usage();
			func = rpm_query;
			break;
		case 'p': /* Query a package */
			func |= rpm_query_package;
			break;
		case 'l': /* List files in a package */
			func |= rpm_query_list;
			break;
		case 'd': /* List doc files in a package (implies list) */
			func |= rpm_query_list;
			func |= rpm_query_list_doc;
			break;
		case 'c': /* List config files in a package (implies list) */
			func |= rpm_query_list;
			func |= rpm_query_list_config;
			break;
		default:
			bb_show_usage();
		}
	}
	argv += optind;
	//argc -= optind;
	if (!argv[0]) {
		bb_show_usage();
	}

	while (*argv) {
		int rpm_fd;
		unsigned mapsize;
		const char *source_rpm;

		rpm_fd = xopen(*argv++, O_RDONLY);
		G.mytags = rpm_gettags(rpm_fd, &G.tagcount);
		if (!G.mytags)
			bb_error_msg_and_die("error reading rpm header");
		mapsize = xlseek(rpm_fd, 0, SEEK_CUR);
		mapsize = (mapsize + pagesize) & -(int)pagesize;
		/* Some NOMMU systems prefer MAP_PRIVATE over MAP_SHARED */
		G.map = mmap(0, mapsize, PROT_READ, MAP_PRIVATE, rpm_fd, 0);
//FIXME: error check?

		source_rpm = rpm_getstr(TAG_SOURCERPM, 0);

		if (func & rpm_install) {
			/* Backup any config files */
			loop_through_files(TAG_BASENAMES, fileaction_dobackup);
			/* Extact the archive */
			extract_cpio(rpm_fd, source_rpm);
			/* Set the correct file uid/gid's */
			loop_through_files(TAG_BASENAMES, fileaction_setowngrp);
		}
		else if ((func & (rpm_query|rpm_query_package)) == (rpm_query|rpm_query_package)) {
			if (!(func & (rpm_query_info|rpm_query_list))) {
				/* If just a straight query, just give package name */
				printf("%s-%s-%s\n", rpm_getstr(TAG_NAME, 0), rpm_getstr(TAG_VERSION, 0), rpm_getstr(TAG_RELEASE, 0));
			}
			if (func & rpm_query_info) {
				/* Do the nice printout */
				time_t bdate_time;
				struct tm *bdate_ptm;
				char bdatestring[50];
				const char *p;

				printf("%-12s: %s\n", "Name"        , rpm_getstr(TAG_NAME, 0));
				/* TODO compat: add "Epoch" here */
				printf("%-12s: %s\n", "Version"     , rpm_getstr(TAG_VERSION, 0));
				printf("%-12s: %s\n", "Release"     , rpm_getstr(TAG_RELEASE, 0));
				/* add "Architecture" */
				printf("%-12s: %s\n", "Install Date", "(not installed)");
				printf("%-12s: %s\n", "Group"       , rpm_getstr(TAG_GROUP, 0));
				printf("%-12s: %d\n", "Size"        , rpm_getint(TAG_SIZE, 0));
				printf("%-12s: %s\n", "License"     , rpm_getstr(TAG_LICENSE, 0));
				/* add "Signature" */
				printf("%-12s: %s\n", "Source RPM"  , source_rpm ? source_rpm : "(none)");
				bdate_time = rpm_getint(TAG_BUILDTIME, 0);
				bdate_ptm = localtime(&bdate_time);
				strftime(bdatestring, 50, "%a %d %b %Y %T %Z", bdate_ptm);
				printf("%-12s: %s\n", "Build Date"  , bdatestring);
				printf("%-12s: %s\n", "Build Host"  , rpm_getstr(TAG_BUILDHOST, 0));
				p = rpm_getstr(TAG_PREFIXS, 0);
				printf("%-12s: %s\n", "Relocations" , p ? p : "(not relocatable)");
				/* add "Packager" */
				p = rpm_getstr(TAG_VENDOR, 0);
				printf("%-12s: %s\n", "Vendor"      , p ? p : "(none)");
				printf("%-12s: %s\n", "URL"         , rpm_getstr(TAG_URL, 0));
				printf("%-12s: %s\n", "Summary"     , rpm_getstr(TAG_SUMMARY, 0));
				printf("Description :\n%s\n", rpm_getstr(TAG_DESCRIPTION, 0));
			}
			if (func & rpm_query_list) {
				int count, it, flags;
				count = rpm_getcount(TAG_BASENAMES);
				for (it = 0; it < count; it++) {
					flags = rpm_getint(TAG_FILEFLAGS, it);
					switch (func & (rpm_query_list_doc|rpm_query_list_config)) {
					case rpm_query_list_doc:
						if (!(flags & RPMFILE_DOC)) continue;
						break;
					case rpm_query_list_config:
						if (!(flags & RPMFILE_CONFIG)) continue;
						break;
					case rpm_query_list_doc|rpm_query_list_config:
						if (!(flags & (RPMFILE_CONFIG|RPMFILE_DOC))) continue;
						break;
					}
					printf("%s%s\n",
						rpm_getstr(TAG_DIRNAMES, rpm_getint(TAG_DIRINDEXES, it)),
						rpm_getstr(TAG_BASENAMES, it));
				}
			}
		}
		munmap(G.map, mapsize);
		free(G.mytags);
		close(rpm_fd);
	}
	return 0;
}
