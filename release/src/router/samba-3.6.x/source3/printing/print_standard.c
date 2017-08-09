/*
   Unix SMB/CIFS implementation.
   printcap parsing
   Copyright (C) Karl Auer 1993-1998

   Re-working by Martin Kiff, 1994

   Re-written again by Andrew Tridgell

   Modified for SVID support by Norm Jacobs, 1997

   Modified for CUPS support by Michael Sweet, 1999

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  This module contains code to parse and cache printcap data, possibly
 *  in concert with the CUPS/SYSV/AIX-specific code found elsewhere.
 *
 *  The way this module looks at the printcap file is very simplistic.
 *  Only the local printcap file is inspected (no searching of NIS
 *  databases etc).
 *
 *  There are assumed to be one or more printer names per record, held
 *  as a set of sub-fields separated by vertical bar symbols ('|') in the
 *  first field of the record. The field separator is assumed to be a colon
 *  ':' and the record separator a newline.
 *
 *  Lines ending with a backspace '\' are assumed to flag that the following
 *  line is a continuation line so that a set of lines can be read as one
 *  printcap entry.
 *
 *  A line stating with a hash '#' is assumed to be a comment and is ignored
 *  Comments are discarded before the record is strung together from the
 *  set of continuation lines.
 *
 *  Opening a pipe for "lpc status" and reading that would probably
 *  be pretty effective. Code to do this already exists in the freely
 *  distributable PCNFS server code.
 */

/* printcap parsing specific code moved here from printing/pcap.c */


#include "includes.h"
#include "system/filesys.h"
#include "printing/pcap.h"

/* handle standard printcap - moved from pcap_printer_fn() */
bool std_pcap_cache_reload(const char *pcap_name)
{
	XFILE *pcap_file;
	char *pcap_line;

	if ((pcap_file = x_fopen(pcap_name, O_RDONLY, 0)) == NULL) {
		DEBUG(0, ("Unable to open printcap file %s for read!\n", pcap_name));
		return false;
	}

	for (; (pcap_line = fgets_slash(NULL, 1024, pcap_file)) != NULL; free(pcap_line)) {
		char name[MAXPRINTERLEN+1];
		char comment[62];
		char *p, *q;

		if (*pcap_line == '#' || *pcap_line == 0)
			continue;

		/* now we have a real printer line - cut at the first : */
		if ((p = strchr_m(pcap_line, ':')) != NULL)
			*p = 0;

		/*
		 * now find the most likely printer name and comment
		 * this is pure guesswork, but it's better than nothing
		 */
		for (*name = *comment = 0, p = pcap_line; p != NULL; p = q) {
			bool has_punctuation;

			if ((q = strchr_m(p, '|')) != NULL)
				*q++ = 0;

			has_punctuation = (strchr_m(p, ' ') ||
			                   strchr_m(p, '\t') ||
					   strchr_m(p, '"') ||
					   strchr_m(p, '\'') ||
					   strchr_m(p, ';') ||
					   strchr_m(p, ',') ||
			                   strchr_m(p, '(') ||
			                   strchr_m(p, ')'));

			if (strlen(p) > strlen(comment) && has_punctuation) {
				strlcpy(comment, p, sizeof(comment));
				continue;
			}

			if (strlen(p) <= MAXPRINTERLEN && *name == '\0' && !has_punctuation) {
				strlcpy(name, p, sizeof(name));
				continue;
			}

			if (!strchr_m(comment, ' ') &&
			    strlen(p) > strlen(comment)) {
				strlcpy(comment, p, sizeof(comment));
				continue;
			}
		}

		if (*name && !pcap_cache_add(name, comment, NULL)) {
			x_fclose(pcap_file);
			return false;
		}
	}

	x_fclose(pcap_file);
	return true;
}
