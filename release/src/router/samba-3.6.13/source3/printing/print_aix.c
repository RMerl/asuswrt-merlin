/* 
   AIX-specific printcap loading
   Copyright (C) Jean-Pierre.Boulard@univ-rennes1.fr      1996

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
 * This module implements AIX-specific printcap loading.  Most of the code
 * here was originally provided by Jean-Pierre.Boulard@univ-rennes1.fr in
 * the Samba 1.9.14 release, and was formerly contained in pcap.c.  It has
 * been moved here and condensed as part of a larger effort to clean up and
 * simplify the printcap code.    -- Rob Foehl, 2004/12/06
 */

#include "includes.h"
#include "system/filesys.h"
#include "printing/pcap.h"

#ifdef AIX
bool aix_cache_reload(void)
{
	int iEtat;
	XFILE *pfile;
	char *line = NULL, *p;
	char *name = NULL;
	TALLOC_CTX *ctx = talloc_init("aix_cache_reload");

	if (!ctx) {
		return false;
	}

	DEBUG(5, ("reloading aix printcap cache\n"));

	if ((pfile = x_fopen(lp_printcapname(), O_RDONLY, 0)) == NULL) {
		DEBUG(0,( "Unable to open qconfig file %s for read!\n", lp_printcapname()));
		TALLOC_FREE(ctx);
		return false;
	}

	iEtat = 0;
	/* scan qconfig file for searching <printername>:	*/
	for (;(line = fgets_slash(NULL, 1024, pfile)); free(line)) {
		if (*line == '*' || *line == 0)
			continue;

		switch (iEtat) {
		case 0: /* locate an entry */
			if (*line == '\t' || *line == ' ')
				continue;

			if ((p = strchr_m(line, ':'))) {
				char *saveptr;
				*p = '\0';
				p = strtok_r(line, ":", &saveptr);
				if (strcmp(p, "bsh") != 0) {
					name = talloc_strdup(ctx, p);
					if (!name) {
						SAFE_FREE(line);
						x_fclose(pfile);
						TALLOC_FREE(ctx);
						return false;
					}
					iEtat = 1;
					continue;
				}
			 }
			 break;

		case 1: /* scanning device stanza */
			if (*line == '*' || *line == 0)
				continue;

			if (*line != '\t' && *line != ' ') {
				/* name is found without stanza device  */
				/* probably a good printer ???		*/
				iEtat = 0;
				if (!pcap_cache_add(name, NULL, NULL)) {
					SAFE_FREE(line);
					x_fclose(pfile);
					TALLOC_FREE(ctx);
					return false;
				}
				continue;
			}

			if (strstr_m(line, "backend")) {
				/* it's a device, not a virtual printer */
				iEtat = 0;
			} else if (strstr_m(line, "device")) {
				/* it's a good virtual printer */
				iEtat = 0;
				if (!pcap_cache_add(name, NULL, NULL)) {
					SAFE_FREE(line);
					x_fclose(pfile);
					TALLOC_FREE(ctx);
					return false;
				}
				continue;
			}
			break;
		}
	}

	x_fclose(pfile);
	TALLOC_FREE(ctx);
	return true;
}

#else
/* this keeps fussy compilers happy */
 void print_aix_dummy(void);
 void print_aix_dummy(void) {}
#endif /* AIX */
