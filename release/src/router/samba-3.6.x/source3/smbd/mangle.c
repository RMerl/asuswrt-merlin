/*
   Unix SMB/CIFS implementation.
   Name mangling interface
   Copyright (C) Andrew Tridgell 2002

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "mangle.h"

/* this allows us to add more mangling backends */
static const struct {
	const char *name;
	const struct mangle_fns *(*init_fn)(void);
} mangle_backends[] = {
	{ "hash", mangle_hash_init },
	{ "hash2", mangle_hash2_init },
	{ "posix", posix_mangle_init },
	/*{ "tdb", mangle_tdb_init }, */
	{ NULL, NULL }
};

/*
  initialise the mangling subsystem
*/
static void mangle_init(void)
{
	int i;
	const char *method;

	if (mangle_fns)
		return;

	method = lp_mangling_method();

	/* find the first mangling method that manages to initialise and
	   matches the "mangling method" parameter */
	for (i=0; mangle_backends[i].name && !mangle_fns; i++) {
		if (!method || !*method || strcmp(method, mangle_backends[i].name) == 0) {
			mangle_fns = mangle_backends[i].init_fn();
		}
	}

	if (!mangle_fns) {
		DEBUG(0,("Failed to initialise mangling system '%s'\n", method));
		exit_server("mangling init failed");
	}
}


/*
  reset the cache. This is called when smb.conf has been reloaded
*/
void mangle_reset_cache(void)
{
	mangle_init();
	mangle_fns->reset();
}

void mangle_change_to_posix(void)
{
	mangle_fns = NULL;
	lp_set_mangling_method("posix");
	mangle_reset_cache();
}

/*
  see if a filename has come out of our mangling code
*/
bool mangle_is_mangled(const char *s, const struct share_params *p)
{
	return mangle_fns->is_mangled(s, p);
}

/*
  see if a filename matches the rules of a 8.3 filename
*/
bool mangle_is_8_3(const char *fname, bool check_case,
		   const struct share_params *p)
{
	return mangle_fns->is_8_3(fname, check_case, False, p);
}

bool mangle_is_8_3_wildcards(const char *fname, bool check_case,
			     const struct share_params *p)
{
	return mangle_fns->is_8_3(fname, check_case, True, p);
}

bool mangle_must_mangle(const char *fname,
		   const struct share_params *p)
{
	if (!lp_manglednames(p)) {
		return False;
	}
	return mangle_fns->must_mangle(fname, p);
}

/*
  try to reverse map a 8.3 name to the original filename. This doesn't have to
  always succeed, as the directory handling code in smbd will scan the directory
  looking for a matching name if it doesn't. It should succeed most of the time
  or there will be a huge performance penalty
*/
bool mangle_lookup_name_from_8_3(TALLOC_CTX *ctx,
			const char *in,
			char **out, /* talloced on the given context. */
			const struct share_params *p)
{
	return mangle_fns->lookup_name_from_8_3(ctx, in, out, p);
}

/*
   mangle a long filename to a 8.3 name.
   Return True if we did mangle the name (ie. out is filled in).
   False on error.
   JRA.
 */

bool name_to_8_3(const char *in,
		char out[13],
		bool cache83,
		const struct share_params *p)
{
	memset(out,'\0',13);

	/* name mangling can be disabled for speed, in which case
	   we just truncate the string */
	if (!lp_manglednames(p)) {
		strlcpy(out, in, 13);
		return True;
	}

	return mangle_fns->name_to_8_3(in,
				out,
				cache83,
				lp_defaultcase(p->service),
				p);
}
