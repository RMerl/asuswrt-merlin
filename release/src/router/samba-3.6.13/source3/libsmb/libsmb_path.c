/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008

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

#include "includes.h"
#include "libsmbclient.h"
#include "libsmb_internal.h"


/* Used by urldecode_talloc() */
static int 
hex2int( unsigned int _char )
{
        if ( _char >= 'A' && _char <='F')
                return _char - 'A' + 10;
        if ( _char >= 'a' && _char <='f')
                return _char - 'a' + 10;
        if ( _char >= '0' && _char <='9')
                return _char - '0';
        return -1;
}

/*
 * smbc_urldecode()
 * and urldecode_talloc() (internal fn.)
 *
 * Convert strings of %xx to their single character equivalent.  Each 'x' must
 * be a valid hexadecimal digit, or that % sequence is left undecoded.
 *
 * dest may, but need not be, the same pointer as src.
 *
 * Returns the number of % sequences which could not be converted due to lack
 * of two following hexadecimal digits.
 */
static int
urldecode_talloc(TALLOC_CTX *ctx, char **pp_dest, const char *src)
{
	int old_length = strlen(src);
	int i = 0;
	int err_count = 0;
	size_t newlen = 1;
	char *p, *dest;

	if (old_length == 0) {
		return 0;
	}

	*pp_dest = NULL;
	for (i = 0; i < old_length; ) {
		unsigned char character = src[i++];

		if (character == '%') {
			int a = i+1 < old_length ? hex2int(src[i]) : -1;
			int b = i+1 < old_length ? hex2int(src[i+1]) : -1;

			/* Replace valid sequence */
			if (a != -1 && b != -1) {
				/* Replace valid %xx sequence with %dd */
				character = (a * 16) + b;
				if (character == '\0') {
					break; /* Stop at %00 */
				}
				i += 2;
			} else {
				err_count++;
			}
		}
		newlen++;
	}

	dest = TALLOC_ARRAY(ctx, char, newlen);
	if (!dest) {
		return err_count;
	}

	err_count = 0;
	for (p = dest, i = 0; i < old_length; ) {
                unsigned char character = src[i++];

                if (character == '%') {
                        int a = i+1 < old_length ? hex2int(src[i]) : -1;
                        int b = i+1 < old_length ? hex2int(src[i+1]) : -1;

                        /* Replace valid sequence */
                        if (a != -1 && b != -1) {
                                /* Replace valid %xx sequence with %dd */
                                character = (a * 16) + b;
                                if (character == '\0') {
                                        break; /* Stop at %00 */
                                }
                                i += 2;
                        } else {
                                err_count++;
                        }
                }
                *p++ = character;
        }

        *p = '\0';
	*pp_dest = dest;
        return err_count;
}

int
smbc_urldecode(char *dest,
               char *src,
               size_t max_dest_len)
{
	TALLOC_CTX *frame = talloc_stackframe();
	char *pdest;
	int ret = urldecode_talloc(frame, &pdest, src);

	if (pdest) {
		strlcpy(dest, pdest, max_dest_len);
	}
	TALLOC_FREE(frame);
	return ret;
}

/*
 * smbc_urlencode()
 *
 * Convert any characters not specifically allowed in a URL into their %xx
 * equivalent.
 *
 * Returns the remaining buffer length.
 */
int
smbc_urlencode(char *dest,
               char *src,
               int max_dest_len)
{
        char hex[] = "0123456789ABCDEF";

        for (; *src != '\0' && max_dest_len >= 3; src++) {

                if ((*src < '0' &&
                     *src != '-' &&
                     *src != '.') ||
                    (*src > '9' &&
                     *src < 'A') ||
                    (*src > 'Z' &&
                     *src < 'a' &&
                     *src != '_') ||
                    (*src > 'z')) {
                        *dest++ = '%';
                        *dest++ = hex[(*src >> 4) & 0x0f];
                        *dest++ = hex[*src & 0x0f];
                        max_dest_len -= 3;
                } else {
                        *dest++ = *src;
                        max_dest_len--;
                }
        }

        *dest++ = '\0';
        max_dest_len--;

        return max_dest_len;
}

/*
 * Function to parse a path and turn it into components
 *
 * The general format of an SMB URI is explain in Christopher Hertel's CIFS
 * book, at http://ubiqx.org/cifs/Appendix-D.html.  We accept a subset of the
 * general format ("smb:" only; we do not look for "cifs:").
 *
 *
 * We accept:
 *  smb://[[[domain;]user[:password]@]server[/share[/path[/file]]]][?options]
 *
 * Meaning of URLs:
 *
 * smb://           Show all workgroups.
 *
 *                  The method of locating the list of workgroups varies
 *                  depending upon the setting of the context variable
 *                  context->options.browse_max_lmb_count.  This value
 *                  determines the maximum number of local master browsers to
 *                  query for the list of workgroups.  In order to ensure that
 *                  a complete list of workgroups is obtained, all master
 *                  browsers must be queried, but if there are many
 *                  workgroups, the time spent querying can begin to add up.
 *                  For small networks (not many workgroups), it is suggested
 *                  that this variable be set to 0, indicating query all local
 *                  master browsers.  When the network has many workgroups, a
 *                  reasonable setting for this variable might be around 3.
 *
 * smb://name/      if name<1D> or name<1B> exists, list servers in
 *                  workgroup, else, if name<20> exists, list all shares
 *                  for server ...
 *
 * If "options" are provided, this function returns the entire option list as a
 * string, for later parsing by the caller.  Note that currently, no options
 * are supported.
 */

#define SMBC_PREFIX "smb:"

int
SMBC_parse_path(TALLOC_CTX *ctx,
		SMBCCTX *context,
                const char *fname,
                char **pp_workgroup,
                char **pp_server,
                char **pp_share,
                char **pp_path,
		char **pp_user,
                char **pp_password,
                char **pp_options)
{
	char *s;
	const char *p;
	char *q, *r;
	char *workgroup = NULL;
	int len;

	/* Ensure these returns are at least valid pointers. */
	*pp_server = talloc_strdup(ctx, "");
	*pp_share = talloc_strdup(ctx, "");
	*pp_path = talloc_strdup(ctx, "");
	*pp_user = talloc_strdup(ctx, "");
	*pp_password = talloc_strdup(ctx, "");

	if (!*pp_server || !*pp_share || !*pp_path ||
            !*pp_user || !*pp_password) {
		return -1;
	}

        /*
         * Assume we wont find an authentication domain to parse, so default
         * to the workgroup in the provided context.
         */
	if (pp_workgroup != NULL) {
		*pp_workgroup =
                        talloc_strdup(ctx, smbc_getWorkgroup(context));
	}

	if (pp_options) {
		*pp_options = talloc_strdup(ctx, "");
	}
	s = talloc_strdup(ctx, fname);

	/* see if it has the right prefix */
	len = strlen(SMBC_PREFIX);
	if (strncmp(s,SMBC_PREFIX,len) || (s[len] != '/' && s[len] != 0)) {
                return -1; /* What about no smb: ? */
        }

	p = s + len;

	/* Watch the test below, we are testing to see if we should exit */

	if (strncmp(p, "//", 2) && strncmp(p, "\\\\", 2)) {
                DEBUG(1, ("Invalid path (does not begin with smb://"));
		return -1;
	}

	p += 2;  /* Skip the double slash */

        /* See if any options were specified */
        if ((q = strrchr(p, '?')) != NULL ) {
                /* There are options.  Null terminate here and point to them */
                *q++ = '\0';

                DEBUG(4, ("Found options '%s'", q));

		/* Copy the options */
		if (pp_options && *pp_options != NULL) {
			TALLOC_FREE(*pp_options);
			*pp_options = talloc_strdup(ctx, q);
		}
	}

	if (*p == '\0') {
		goto decoding;
	}

	if (*p == '/') {
		int wl = strlen(smbc_getWorkgroup(context));

		if (wl > 16) {
			wl = 16;
		}

		*pp_server = talloc_strdup(ctx, smbc_getWorkgroup(context));
		if (!*pp_server) {
			return -1;
		}
		(*pp_server)[wl] = '\0';
		return 0;
	}

	/*
	 * ok, its for us. Now parse out the server, share etc.
	 *
	 * However, we want to parse out [[domain;]user[:password]@] if it
	 * exists ...
	 */

	/* check that '@' occurs before '/', if '/' exists at all */
	q = strchr_m(p, '@');
	r = strchr_m(p, '/');
	if (q && (!r || q < r)) {
		char *userinfo = NULL;
		const char *u;

		next_token_no_ltrim_talloc(ctx, &p, &userinfo, "@");
		if (!userinfo) {
			return -1;
		}
		u = userinfo;

		if (strchr_m(u, ';')) {
			next_token_no_ltrim_talloc(ctx, &u, &workgroup, ";");
			if (!workgroup) {
				return -1;
			}
			if (pp_workgroup) {
				*pp_workgroup = workgroup;
			}
		}

		if (strchr_m(u, ':')) {
			next_token_no_ltrim_talloc(ctx, &u, pp_user, ":");
			if (!*pp_user) {
				return -1;
			}
			*pp_password = talloc_strdup(ctx, u);
			if (!*pp_password) {
				return -1;
			}
		} else {
			*pp_user = talloc_strdup(ctx, u);
			if (!*pp_user) {
				return -1;
			}
		}
	}

	if (!next_token_talloc(ctx, &p, pp_server, "/")) {
		return -1;
	}

	if (*p == (char)0) {
		goto decoding;  /* That's it ... */
	}

	if (!next_token_talloc(ctx, &p, pp_share, "/")) {
		return -1;
	}

        /*
         * Prepend a leading slash if there's a file path, as required by
         * NetApp filers.
         */
        if (*p != '\0') {
		*pp_path = talloc_asprintf(ctx,
                                           "\\%s",
                                           p);
        } else {
		*pp_path = talloc_strdup(ctx, "");
	}
	if (!*pp_path) {
		return -1;
	}
	string_replace(*pp_path, '/', '\\');

decoding:
	(void) urldecode_talloc(ctx, pp_path, *pp_path);
	(void) urldecode_talloc(ctx, pp_server, *pp_server);
	(void) urldecode_talloc(ctx, pp_share, *pp_share);
	(void) urldecode_talloc(ctx, pp_user, *pp_user);
	(void) urldecode_talloc(ctx, pp_password, *pp_password);

	if (!workgroup) {
		workgroup = talloc_strdup(ctx, smbc_getWorkgroup(context));
	}
	if (!workgroup) {
		return -1;
	}

	/* set the credentials to make DFS work */
	smbc_set_credentials_with_fallback(context,
				    	   workgroup,
				    	   *pp_user,
				    	   *pp_password);
	return 0;
}

