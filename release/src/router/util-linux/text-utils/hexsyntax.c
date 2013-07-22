/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include "hexdump.h"
#include "nls.h"
#include "strutils.h"

off_t skip;				/* bytes to skip */


void
newsyntax(int argc, char ***argvp)
{
	int ch;
	char *p, **argv;

	argv = *argvp;
	while ((ch = getopt(argc, argv, "bcCde:f:n:os:vxV")) != -1) {
		switch (ch) {
		case 'b':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 16/1 \"%03o \" \"\\n\"");
			break;
		case 'c':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\"");
			break;
		case 'C':
			add("\"%08.8_Ax\n\"");
			add("\"%08.8_ax  \" 8/1 \"%02x \" \"  \" 8/1 \"%02x \" ");
			add("\"  |\" 16/1 \"%_p\" \"|\\n\"");
			break;
		case 'd':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\"");
			break;
		case 'e':
			add(optarg);
			break;
		case 'f':
			addfile(optarg);
			break;
		case 'n':
		        length = strtol_or_err(optarg, _("bad length value"));
			break;
		case 'o':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \" %06o \" \"\\n\"");
			break;
		case 's':
			if ((skip = strtol(optarg, &p, 0)) < 0)
				err(EXIT_FAILURE, _("bad skip value"));
			switch(*p) {
			case 'b':
				skip *= 512;
				break;
			case 'k':
				skip *= 1024;
				break;
			case 'm':
				skip *= 1048576;
				break;
			}
			break;
		case 'v':
			vflag = ALL;
			break;
		case 'x':
			add("\"%07.7_Ax\n\"");
			add("\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\"");
			break;
		case 'V':
			printf(_("%s from %s\n"),
					program_invocation_short_name,
					PACKAGE_STRING);
			exit(EXIT_SUCCESS);
			break;
		default:
			usage(stderr);
		}
	}

	if (!fshead) {
		add("\"%07.7_Ax\n\"");
		add("\"%07.7_ax \" 8/2 \"%04x \" \"\\n\"");
	}

	*argvp += optind;
}

void __attribute__((__noreturn__)) usage(FILE *out)
{
	fprintf(out, _("\nUsage:\n"
		       " %s [options] file...\n"),
		       program_invocation_short_name);
	fprintf(out, _(
		       "\nOptions:\n"
		       " -b              one-byte octal display\n"
		       " -c              one-byte character display\n"
		       " -C              canonical hex+ASCII display\n"
		       " -d              two-byte decimal display\n"
		       " -o              two-byte octal display\n"
		       " -x              two-byte hexadecimal display\n"
		       " -e format       format string to be used for displaying data\n"
		       " -f format_file  file that contains format strings\n"
		       " -n length       interpret only length bytes of input\n"
		       " -s offset       skip offset bytes from the beginning\n"
		       " -v              display without squeezing similar lines\n"
		       " -V              output version information and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}
