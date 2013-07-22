/*
 * swaplabel.c - Print or change the label / UUID of a swap partition
 *
 * Copyright (C) 2010 Jason Borden <jborden@bluehost.com>
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * Usage: swaplabel [-L label] [-U UUID] device
 *
 * This file may be redistributed under the terms of the GNU Public License
 * version 2 or later.
 *
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <blkid.h>
#include <getopt.h>

#ifdef HAVE_LIBUUID
# include <uuid.h>
#endif

#include "c.h"
#include "writeall.h"
#include "swapheader.h"
#include "strutils.h"
#include "nls.h"

#define SWAP_UUID_OFFSET	(offsetof(struct swap_header_v1_2, uuid))
#define SWAP_LABEL_OFFSET	(offsetof(struct swap_header_v1_2, volume_name))

/*
 * Returns new libblkid prober. This function call exit() on error.
 */
static blkid_probe get_swap_prober(const char *devname)
{
	blkid_probe pr;
	int rc;
	const char *version = NULL;
	char *swap_filter[] = { "swap", NULL };

	pr = blkid_new_probe_from_filename(devname);
	if (!pr) {
		warn(_("%s: unable to probe device"), devname);
		return NULL;
	}

	blkid_probe_enable_superblocks(pr, TRUE);
	blkid_probe_set_superblocks_flags(pr,
			BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID |
			BLKID_SUBLKS_VERSION);

	blkid_probe_filter_superblocks_type(pr, BLKID_FLTR_ONLYIN, swap_filter);

	rc = blkid_do_safeprobe(pr);
	if (rc == -1)
		warn(_("%s: unable to probe device"), devname);
	else if (rc == -2)
		warnx(_("%s: ambivalent probing result, use wipefs(8)"), devname);
	else if (rc == 1)
		warnx(_("%s: not a valid swap partition"), devname);

	if (rc == 0) {
		/* supported is SWAPSPACE2 only */
		blkid_probe_lookup_value(pr, "VERSION", &version, NULL);
		if (strcmp(version, "2"))
			warnx(_("%s: unsupported swap version '%s'"),
						devname, version);
		else
			return pr;
	}

	blkid_free_probe(pr);
	return NULL;
}

/* Print the swap partition information */
static int print_info(blkid_probe pr)
{
	const char *data;

	if (!blkid_probe_lookup_value(pr, "LABEL", &data, NULL))
		printf("LABEL: %s\n", data);

	if (!blkid_probe_lookup_value(pr, "UUID", &data, NULL))
		printf("UUID:  %s\n", data);

	return 0;
}

/* Change the swap partition info */
static int change_info(const char *devname, const char *label, const char *uuid)
{
	int fd;

	fd = open(devname, O_RDWR);
	if (fd < 0) {
		warn(_("%s: failed to open"), devname);
		goto err;
	}
#ifdef HAVE_LIBUUID
	/* Write the uuid if it was provided */
	if (uuid) {
		uuid_t newuuid;

		if (uuid_parse(uuid, newuuid) == -1)
			warnx(_("failed to parse UUID: %s"), uuid);
		else {
			if (lseek(fd, SWAP_UUID_OFFSET, SEEK_SET) !=
							SWAP_UUID_OFFSET) {
				warn(_("%s: failed to seek to swap UUID"), devname);
				goto err;

			} else if (write_all(fd, newuuid, sizeof(newuuid))) {
				warn(_("%s: failed to write UUID"), devname);
				goto err;
			}
		}
	}
#endif
	/* Write the label if it was provided */
	if (label) {
		char newlabel[SWAP_LABEL_LENGTH];

		if (lseek(fd, SWAP_LABEL_OFFSET, SEEK_SET) != SWAP_LABEL_OFFSET) {
			warn(_("%s: failed to seek to swap label "), devname);
			goto err;
		}
		memset(newlabel, 0, sizeof(newlabel));
		xstrncpy(newlabel, label, sizeof(newlabel));

		if (strlen(label) > strlen(newlabel))
			warnx(_("label is too long. Truncating it to '%s'"),
					newlabel);
		if (write_all(fd, newlabel, sizeof(newlabel))) {
			warn(_("%s: failed to write label"), devname);
			goto err;
		}
	}

	close(fd);
	return 0;
err:
	if (fd >= 0)
		close(fd);
	return -1;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fprintf(out, _("Usage: %s [options] <device>\n\nOptions:\n"),
			program_invocation_short_name);

	fprintf(out, _(
		" -h, --help          this help\n"
		" -L, --label <label> specify a new label\n"
		" -U, --uuid <uuid>   specify a new uuid\n"));

	fprintf(out, _("\nFor more information see swaplabel(8).\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	blkid_probe pr = NULL;
	char *uuid = NULL, *label = NULL, *devname;
	int c, rc = -1;

	static const struct option longopts[] = {
	    { "help",      0, 0, 'h' },
	    { "label",     1, 0, 'L' },
	    { "uuid",      1, 0, 'U' },
	    { NULL,        0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "hL:U:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(stdout);
			break;
		case 'L':
			label = optarg;
			break;
		case 'U':
#ifdef HAVE_LIBUUID
			uuid = optarg;
#else
			warnx(_("ignore -U (UUIDs are unsupported)"));
#endif
			break;
		default:
			usage(stderr);
			break;
		}
	}

	if (optind == argc)
		usage(stderr);

	devname = argv[optind];
	pr = get_swap_prober(devname);
	if (pr) {
		if (uuid || label)
			rc = change_info(devname, label, uuid);
		else
			rc  = print_info(pr);
		blkid_free_probe(pr);
	}
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}

