/* dosfslabel.c - User interface

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
   Copyright (C) 2007 Red Hat, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.

   On Debian systems, the complete text of the GNU General Public License
   can be found in /usr/share/common-licenses/GPL-3 file.
*/

#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "common.h"
#include "dosfsck.h"
#include "io.h"
#include "boot.h"
#include "fat.h"
#include "file.h"
#include "check.h"

int interactive = 0, rw = 0, list = 0, test = 0, verbose = 0, write_immed = 0;
int atari_format = 0;
unsigned n_files = 0;
void *mem_queue = NULL;

static void usage(int error)
{
    FILE *f = error ? stderr : stdout;
    int status = error ? 1 : 0;

    fprintf(f, "usage: dosfslabel device [label]\n");
    exit(status);
}

/*
 * ++roman: On m68k, check if this is an Atari; if yes, turn on Atari variant
 * of MS-DOS filesystem by default.
 */
static void check_atari(void)
{
#ifdef __mc68000__
    FILE *f;
    char line[128], *p;

    if (!(f = fopen("/proc/hardware", "r"))) {
	perror("/proc/hardware");
	return;
    }

    while (fgets(line, sizeof(line), f)) {
	if (strncmp(line, "Model:", 6) == 0) {
	    p = line + 6;
	    p += strspn(p, " \t");
	    if (strncmp(p, "Atari ", 6) == 0)
		atari_format = 1;
	    break;
	}
    }
    fclose(f);
#endif
}

int main(int argc, char *argv[])
{
    DOS_FS fs;
    rw = 0;

    char *device = NULL;
    char *label = NULL;

    check_atari();

    if (argc < 2 || argc > 3)
	usage(1);

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
	usage(0);
    else if (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version")) {
	printf("dosfslabel " VERSION ", " VERSION_DATE ", FAT32, LFN\n");
	exit(0);
    }

    device = argv[1];
    if (argc == 3) {
	label = argv[2];
	if (strlen(label) > 11) {
	    fprintf(stderr,
		    "dosfslabel: labels can be no longer than 11 characters\n");
	    exit(1);
	}
	rw = 1;
    }

    fs_open(device, rw);
    read_boot(&fs);
    if (!rw) {
	fprintf(stdout, "%s\n", fs.label);
	exit(0);
    }

    write_label(&fs, label);
    fs_close(rw);
    return 0;
}
