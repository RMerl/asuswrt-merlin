#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "blkdev.h"
#include "pathnames.h"
#include "common.h"

/*
 * return partition name - uses static storage unless buf is supplied
 */
char *
partname(char *dev, int pno, int lth) {
	static char bufp[80];
	char *p;
	int w, wp;

	w = strlen(dev);
	p = "";

	if (isdigit(dev[w-1]))
		p = "p";

	/* devfs kludge - note: fdisk partition names are not supposed
	   to equal kernel names, so there is no reason to do this */
	if (strcmp (dev + w - 4, "disc") == 0) {
		w -= 4;
		p = "part";
	}

	/* udev names partitions by appending -partN
	   e.g. ata-SAMSUNG_SV8004H_0357J1FT712448-part1 */
	if ((strncmp(dev, _PATH_DEV_BYID, strlen(_PATH_DEV_BYID)) == 0) ||
	     strncmp(dev, _PATH_DEV_BYPATH, strlen(_PATH_DEV_BYPATH)) == 0) {
	       p = "-part";
	}

	wp = strlen(p);

	if (lth) {
		snprintf(bufp, sizeof(bufp), "%*.*s%s%-2u",
			 lth-wp-2, w, dev, p, pno);
	} else {
		snprintf(bufp, sizeof(bufp), "%.*s%s%-2u", w, dev, p, pno);
	}
	return bufp;
}

