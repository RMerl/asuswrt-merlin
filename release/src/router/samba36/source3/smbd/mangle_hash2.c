/* 
   Unix SMB/CIFS implementation.
   new hash based name mangling implementation
   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Simo Sorce 2002
   
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
  this mangling scheme uses the following format

  Annnn~n.AAA

  where nnnnn is a base 36 hash, and A represents characters from the original string

  The hash is taken of the leading part of the long filename, in uppercase

  for simplicity, we only allow ascii characters in 8.3 names
 */

 /* hash alghorithm changed to FNV1 by idra@samba.org (Simo Sorce).
  * see http://www.isthe.com/chongo/tech/comp/fnv/index.html for a
  * discussion on Fowler / Noll / Vo (FNV) Hash by one of it's authors
  */

/*
  ===============================================================================
  NOTE NOTE NOTE!!!

  This file deliberately uses non-multibyte string functions in many places. This
  is *not* a mistake. This code is multi-byte safe, but it gets this property
  through some very subtle knowledge of the way multi-byte strings are encoded 
  and the fact that this mangling algorithm only supports ascii characters in
  8.3 names.

  please don't convert this file to use the *_m() functions!!
  ===============================================================================
*/

/*
 * ============================================================================
 * Whenever you change anything in the FLAG_ or other fields,
 * re-initialize the tables char_flags and base_reverse by running the
 * init_tables() routine once and dump its results. To do this, a
 * single smbd run with
 *
 * #define DYNAMIC_MANGLE_TABLES 1
 *
 * and debug level 10 should be sufficient.
 * ============================================================================
 */


#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "memcache.h"
#include "mangle.h"

#if 1
#define M_DEBUG(level, x) DEBUG(level, x)
#else
#define M_DEBUG(level, x)
#endif

/* these flags are used to mark characters in as having particular
   properties */
#define FLAG_BASECHAR 1
#define FLAG_ASCII 2
#define FLAG_ILLEGAL 4
#define FLAG_WILDCARD 8

/* the "possible" flags are used as a fast way to find possible DOS
   reserved filenames */
#define FLAG_POSSIBLE1 16
#define FLAG_POSSIBLE2 32
#define FLAG_POSSIBLE3 64
#define FLAG_POSSIBLE4 128

/* by default have a max of 4096 entries in the cache. */
#ifndef MANGLE_CACHE_SIZE
#define MANGLE_CACHE_SIZE 4096
#endif

#define FNV1_PRIME 0x01000193
/*the following number is a fnv1 of the string: idra@samba.org 2002 */
#define FNV1_INIT  0xa6b93095

#define FLAG_CHECK(c, flag) (char_flags[(unsigned char)(c)] & (flag))

/* these are the characters we use in the 8.3 hash. Must be 36 chars long */
static const char basechars[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
#define base_forward(v) basechars[v]

/* the list of reserved dos names - all of these are illegal */
static const char * const reserved_names[] =
{ "AUX", "LOCK$", "CON", "COM1", "COM2", "COM3", "COM4",
  "LPT1", "LPT2", "LPT3", "NUL", "PRN", NULL };

#define DYNAMIC_MANGLE_TABLES 0

#if DYNAMIC_MANGLE_TABLES

/* these tables are used to provide fast tests for characters */
static unsigned char char_flags[256];
static unsigned char base_reverse[256];

/* initialise the flags table

  we allow only a very restricted set of characters as 'ascii' in this
  mangling backend. This isn't a significant problem as modern clients
  use the 'long' filenames anyway, and those don't have these
  restrictions.
*/
static void init_tables(void)
{
	int i;

	memset(char_flags, 0, sizeof(char_flags));

	for (i=1;i<128;i++) {
		if (i <= 0x1f) {
			/* Control characters. */
			char_flags[i] |= FLAG_ILLEGAL;
		}

		if ((i >= '0' && i <= '9') ||
		    (i >= 'a' && i <= 'z') ||
		    (i >= 'A' && i <= 'Z')) {
			char_flags[i] |=  (FLAG_ASCII | FLAG_BASECHAR);
		}
		if (strchr("_-$~", i)) {
			char_flags[i] |= FLAG_ASCII;
		}

		if (strchr("*\\/?<>|\":", i)) {
			char_flags[i] |= FLAG_ILLEGAL;
		}

		if (strchr("*?\"<>", i)) {
			char_flags[i] |= FLAG_WILDCARD;
		}
	}

	memset(base_reverse, 0, sizeof(base_reverse));
	for (i=0;i<36;i++) {
		base_reverse[(unsigned char)base_forward(i)] = i;
	}

	/* fill in the reserved names flags. These are used as a very
	   fast filter for finding possible DOS reserved filenames */
	for (i=0; reserved_names[i]; i++) {
		unsigned char c1, c2, c3, c4;

		c1 = (unsigned char)reserved_names[i][0];
		c2 = (unsigned char)reserved_names[i][1];
		c3 = (unsigned char)reserved_names[i][2];
		c4 = (unsigned char)reserved_names[i][3];

		char_flags[c1] |= FLAG_POSSIBLE1;
		char_flags[c2] |= FLAG_POSSIBLE2;
		char_flags[c3] |= FLAG_POSSIBLE3;
		char_flags[c4] |= FLAG_POSSIBLE4;
		char_flags[tolower_m(c1)] |= FLAG_POSSIBLE1;
		char_flags[tolower_m(c2)] |= FLAG_POSSIBLE2;
		char_flags[tolower_m(c3)] |= FLAG_POSSIBLE3;
		char_flags[tolower_m(c4)] |= FLAG_POSSIBLE4;

		char_flags[(unsigned char)'.'] |= FLAG_POSSIBLE4;
	}

#if 0
	DEBUG(10, ("char_flags\n"));
	dump_data(10, char_flags, sizeof(char_flags));

	DEBUG(10, ("base_reverse\n"));
	dump_data(10, base_reverse, sizeof(base_reverse));
#endif
}

#else

/*
 * These tables were initialized by a single run of the above
 * init_tables() routine, dumping the tables and a simple emacs macro.
 *
 * Technically we could leave out the 0's at the end of the array
 * initializers, but I'll leave it in: less surprise.
 */

static uint8_t char_flags[256] = {
	0x80, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x00, 0x00, 0x0C, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0C, 0x00, 0x00, 0x02, 0x80, 0x04,
	0x03, 0x83, 0x83, 0x83, 0x83, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x04, 0x00, 0x0C, 0x00, 0x0C, 0x0C,
	0x00, 0x13, 0x03, 0x53, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x83, 0x53, 0x43, 0x53, 0x23,
	0x33, 0x03, 0x23, 0x03, 0x43, 0x23, 0x03, 0x03,
	0x43, 0x03, 0x03, 0x00, 0x04, 0x00, 0x00, 0x02,
	0x00, 0x13, 0x03, 0x53, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x83, 0x53, 0x43, 0x53, 0x23,
	0x33, 0x03, 0x23, 0x03, 0x43, 0x23, 0x03, 0x03,
	0x43, 0x03, 0x03, 0x00, 0x04, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t base_reverse[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif

/* 
   hash a string of the specified length. The string does not need to be
   null terminated 

   this hash needs to be fast with a low collision rate (what hash doesn't?)
*/
static unsigned int mangle_hash(const char *key, unsigned int length)
{
	unsigned int value;
	unsigned int   i;
	fstring str;

	/* we have to uppercase here to ensure that the mangled name
	   doesn't depend on the case of the long name. Note that this
	   is the only place where we need to use a multi-byte string
	   function */
	length = MIN(length,sizeof(fstring)-1);
	strncpy(str, key, length);
	str[length] = 0;
	strupper_m(str);

	/* the length of a multi-byte string can change after a strupper_m */
	length = strlen(str);

	/* Set the initial value from the key size. */
	for (value = FNV1_INIT, i=0; i < length; i++) {
                value *= (unsigned int)FNV1_PRIME;
                value ^= (unsigned int)(str[i]);
        }

	/* note that we force it to a 31 bit hash, to keep within the limits
	   of the 36^6 mangle space */
	return value & ~0x80000000;  
}

/*
  insert an entry into the prefix cache. The string might not be null
  terminated */
static void cache_insert(const char *prefix, int length, unsigned int hash)
{
	char *str = SMB_STRNDUP(prefix, length);

	if (str == NULL) {
		return;
	}

	memcache_add(smbd_memcache(), MANGLE_HASH2_CACHE,
		     data_blob_const(&hash, sizeof(hash)),
		     data_blob_const(str, length+1));
	SAFE_FREE(str);
}

/*
  lookup an entry in the prefix cache. Return NULL if not found.
*/
static char *cache_lookup(TALLOC_CTX *mem_ctx, unsigned int hash)
{
	DATA_BLOB value;

	if (!memcache_lookup(smbd_memcache(), MANGLE_HASH2_CACHE,
			     data_blob_const(&hash, sizeof(hash)), &value)) {
		return NULL;
	}

	SMB_ASSERT((value.length > 0)
		   && (value.data[value.length-1] == '\0'));

	return talloc_strdup(mem_ctx, (char *)value.data);
}


/* 
   determine if a string is possibly in a mangled format, ignoring
   case 

   In this algorithm, mangled names use only pure ascii characters (no
   multi-byte) so we can avoid doing a UCS2 conversion 
 */
static bool is_mangled_component(const char *name, size_t len)
{
	unsigned int i;

	M_DEBUG(10,("is_mangled_component %s (len %lu) ?\n", name, (unsigned long)len));

	/* check the length */
	if (len > 12 || len < 8)
		return False;

	/* the best distinguishing characteristic is the ~ */
	if (name[6] != '~')
		return False;

	/* check extension */
	if (len > 8) {
		if (name[8] != '.')
			return False;
		for (i=9; name[i] && i < len; i++) {
			if (! FLAG_CHECK(name[i], FLAG_ASCII)) {
				return False;
			}
		}
	}
	
	/* check lead characters */
	for (i=0;i<mangle_prefix;i++) {
		if (! FLAG_CHECK(name[i], FLAG_ASCII)) {
			return False;
		}
	}
	
	/* check rest of hash */
	if (! FLAG_CHECK(name[7], FLAG_BASECHAR)) {
		return False;
	}
	for (i=mangle_prefix;i<6;i++) {
		if (! FLAG_CHECK(name[i], FLAG_BASECHAR)) {
			return False;
		}
	}

	M_DEBUG(10,("is_mangled_component %s (len %lu) -> yes\n", name, (unsigned long)len));

	return True;
}



/* 
   determine if a string is possibly in a mangled format, ignoring
   case 

   In this algorithm, mangled names use only pure ascii characters (no
   multi-byte) so we can avoid doing a UCS2 conversion 

   NOTE! This interface must be able to handle a path with unix
   directory separators. It should return true if any component is
   mangled
 */
static bool is_mangled(const char *name, const struct share_params *parm)
{
	const char *p;
	const char *s;

	M_DEBUG(10,("is_mangled %s ?\n", name));

	for (s=name; (p=strchr(s, '/')); s=p+1) {
		if (is_mangled_component(s, PTR_DIFF(p, s))) {
			return True;
		}
	}
	
	/* and the last part ... */
	return is_mangled_component(s,strlen(s));
}


/* 
   see if a filename is an allowable 8.3 name to return to the client.
   Note this is not testing if this is a valid Samba mangled name, so
   the rules are different for is_mangled.

   we are only going to allow ascii characters in 8.3 names, as this
   simplifies things greatly (it means that we know the string won't
   get larger when converted from UNIX to DOS formats)
*/

static char force_shortname_chars[] = " +,[];=";

static bool is_8_3(const char *name, bool check_case, bool allow_wildcards, const struct share_params *p)
{
	int len, i;
	char *dot_p;

	/* as a special case, the names '.' and '..' are allowable 8.3 names */
	if (name[0] == '.') {
		if (!name[1] || (name[1] == '.' && !name[2])) {
			return True;
		}
	}

	/* the simplest test is on the overall length of the
	 filename. Note that we deliberately use the ascii string
	 length (not the multi-byte one) as it is faster, and gives us
	 the result we need in this case. Using strlen_m would not
	 only be slower, it would be incorrect */
	len = strlen(name);
	if (len > 12)
		return False;

	/* find the '.'. Note that once again we use the non-multibyte
           function */
	dot_p = strchr(name, '.');

	if (!dot_p) {
		/* if the name doesn't contain a '.' then its length
                   must be less than 8 */
		if (len > 8) {
			return False;
		}
	} else {
		int prefix_len, suffix_len;

		/* if it does contain a dot then the prefix must be <=
		   8 and the suffix <= 3 in length */
		prefix_len = PTR_DIFF(dot_p, name);
		suffix_len = len - (prefix_len+1);

		if (prefix_len > 8 || suffix_len > 3 || suffix_len == 0) {
			return False;
		}

		/* a 8.3 name cannot contain more than 1 '.' */
		if (strchr(dot_p+1, '.')) {
			return False;
		}
	}

	/* the length are all OK. Now check to see if the characters themselves are OK */
	for (i=0; name[i]; i++) {
		if (FLAG_CHECK(name[i], FLAG_ILLEGAL)) {
			return false;
		}
		/* note that we may allow wildcard petterns! */
		if (!allow_wildcards && FLAG_CHECK(name[i], FLAG_WILDCARD)) {
			return false;
		}
		if (((unsigned char)name[i]) > 0x7e) {
			return false;
		}
		if (strchr(force_shortname_chars, name[i])) {
			return false;
		}
	}

	/* it is a good 8.3 name */
	return True;
}


/*
  reset the mangling cache on a smb.conf reload. This only really makes sense for
  mangling backends that have parameters in smb.conf, and as this backend doesn't
  this is a NULL operation
*/
static void mangle_reset(void)
{
	/* noop */
}


/*
  try to find a 8.3 name in the cache, and if found then
  replace the string with the original long name.
*/
static bool lookup_name_from_8_3(TALLOC_CTX *ctx,
			const char *name,
			char **pp_out, /* talloced on the given context. */
			const struct share_params *p)
{
	unsigned int hash, multiplier;
	unsigned int i;
	char *prefix;
	char extension[4];

	*pp_out = NULL;

	/* make sure that this is a mangled name from this cache */
	if (!is_mangled(name, p)) {
		M_DEBUG(10,("lookup_name_from_8_3: %s -> not mangled\n", name));
		return False;
	}

	/* we need to extract the hash from the 8.3 name */
	hash = base_reverse[(unsigned char)name[7]];
	for (multiplier=36, i=5;i>=mangle_prefix;i--) {
		unsigned int v = base_reverse[(unsigned char)name[i]];
		hash += multiplier * v;
		multiplier *= 36;
	}

	/* now look in the prefix cache for that hash */
	prefix = cache_lookup(ctx, hash);
	if (!prefix) {
		M_DEBUG(10,("lookup_name_from_8_3: %s -> %08X -> not found\n",
					name, hash));
		return False;
	}

	/* we found it - construct the full name */
	if (name[8] == '.') {
		strncpy(extension, name+9, 3);
		extension[3] = 0;
	} else {
		extension[0] = 0;
	}

	if (extension[0]) {
		M_DEBUG(10,("lookup_name_from_8_3: %s -> %s.%s\n",
					name, prefix, extension));
		*pp_out = talloc_asprintf(ctx, "%s.%s", prefix, extension);
	} else {
		M_DEBUG(10,("lookup_name_from_8_3: %s -> %s\n", name, prefix));
		*pp_out = talloc_strdup(ctx, prefix);
	}

	TALLOC_FREE(prefix);

	if (!*pp_out) {
		M_DEBUG(0,("talloc_fail"));
		return False;
	}

	return True;
}

/*
  look for a DOS reserved name
*/
static bool is_reserved_name(const char *name)
{
	if (FLAG_CHECK(name[0], FLAG_POSSIBLE1) &&
	    FLAG_CHECK(name[1], FLAG_POSSIBLE2) &&
	    FLAG_CHECK(name[2], FLAG_POSSIBLE3) &&
	    FLAG_CHECK(name[3], FLAG_POSSIBLE4)) {
		/* a likely match, scan the lot */
		int i;
		for (i=0; reserved_names[i]; i++) {
			int len = strlen(reserved_names[i]);
			/* note that we match on COM1 as well as COM1.foo */
			if (strnequal(name, reserved_names[i], len) &&
			    (name[len] == '.' || name[len] == 0)) {
				return True;
			}
		}
	}

	return False;
}

/*
 See if a filename is a legal long filename.
 A filename ending in a '.' is not legal unless it's "." or "..". JRA.
 A filename ending in ' ' is not legal either. See bug id #2769.
*/

static bool is_legal_name(const char *name)
{
	const char *dot_pos = NULL;
	bool alldots = True;
	size_t numdots = 0;

	while (*name) {
		if (((unsigned int)name[0]) > 128 && (name[1] != 0)) {
			/* Possible start of mb character. */
			size_t size = 0;
			(void)next_codepoint(name, &size);
			/*
			 * Note that if CH_UNIX is utf8 a string may be 3
			 * bytes, but this is ok as mb utf8 characters don't
			 * contain embedded ascii bytes. We are really checking
			 * for mb UNIX asian characters like Japanese (SJIS) here.
			 * JRA.
			 */
			if (size > 1) {
				/* Was a mb string. */
				name += size;
				continue;
			}
		}

		if (FLAG_CHECK(name[0], FLAG_ILLEGAL)) {
			return False;
		}
		if (name[0] == '.') {
			dot_pos = name;
			numdots++;
		} else {
			alldots = False;
		}
		if ((name[0] == ' ') && (name[1] == '\0')) {
			/* Can't end in ' ' */
			return False;
		}
		name++;
	}

	if (dot_pos) {
		if (alldots && (numdots == 1 || numdots == 2))
			return True; /* . or .. is a valid name */

		/* A valid long name cannot end in '.' */
		if (dot_pos[1] == '\0')
			return False;
	}
	return True;
}

static bool must_mangle(const char *name,
			const struct share_params *p)
{
	if (is_reserved_name(name)) {
		return True;
	}
	return !is_legal_name(name);
}

/*
  the main forward mapping function, which converts a long filename to 
  a 8.3 name

  if cache83 is not set then we don't cache the result

*/
static bool hash2_name_to_8_3(const char *name,
			char new_name[13],
			bool cache83,
			int default_case,
			const struct share_params *p)
{
	char *dot_p;
	char lead_chars[7];
	char extension[4];
	unsigned int extension_length, i;
	unsigned int prefix_len;
	unsigned int hash, v;

	/* reserved names are handled specially */
	if (!is_reserved_name(name)) {
		/* if the name is already a valid 8.3 name then we don't need to
		 * change anything */
		if (is_legal_name(name) && is_8_3(name, False, False, p)) {
			safe_strcpy(new_name, name, 12);
			return True;
		}
	}

	/* find the '.' if any */
	dot_p = strrchr(name, '.');

	if (dot_p) {
		/* if the extension contains any illegal characters or
		   is too long or zero length then we treat it as part
		   of the prefix */
		for (i=0; i<4 && dot_p[i+1]; i++) {
			if (! FLAG_CHECK(dot_p[i+1], FLAG_ASCII)) {
				dot_p = NULL;
				break;
			}
		}
		if (i == 0 || i == 4) {
			dot_p = NULL;
		}
	}

	/* the leading characters in the mangled name is taken from
	   the first characters of the name, if they are ascii otherwise
	   '_' is used
	*/
	for (i=0;i<mangle_prefix && name[i];i++) {
		lead_chars[i] = name[i];
		if (! FLAG_CHECK(lead_chars[i], FLAG_ASCII)) {
			lead_chars[i] = '_';
		}
		lead_chars[i] = toupper_m(lead_chars[i]);
	}
	for (;i<mangle_prefix;i++) {
		lead_chars[i] = '_';
	}

	/* the prefix is anything up to the first dot */
	if (dot_p) {
		prefix_len = PTR_DIFF(dot_p, name);
	} else {
		prefix_len = strlen(name);
	}

	/* the extension of the mangled name is taken from the first 3
	   ascii chars after the dot */
	extension_length = 0;
	if (dot_p) {
		for (i=1; extension_length < 3 && dot_p[i]; i++) {
			char c = dot_p[i];
			if (FLAG_CHECK(c, FLAG_ASCII)) {
				extension[extension_length++] =
					toupper_m(c);
			}
		}
	}

	/* find the hash for this prefix */
	v = hash = mangle_hash(name, prefix_len);

	/* now form the mangled name. */
	for (i=0;i<mangle_prefix;i++) {
		new_name[i] = lead_chars[i];
	}
	new_name[7] = base_forward(v % 36);
	new_name[6] = '~';
	for (i=5; i>=mangle_prefix; i--) {
		v = v / 36;
		new_name[i] = base_forward(v % 36);
	}

	/* add the extension */
	if (extension_length) {
		new_name[8] = '.';
		memcpy(&new_name[9], extension, extension_length);
		new_name[9+extension_length] = 0;
	} else {
		new_name[8] = 0;
	}

	if (cache83) {
		/* put it in the cache */
		cache_insert(name, prefix_len, hash);
	}

	M_DEBUG(10,("hash2_name_to_8_3: %s -> %08X -> %s (cache=%d)\n",
		   name, hash, new_name, cache83));

	return True;
}

/*
  the following provides the abstraction layer to make it easier
  to drop in an alternative mangling implementation */
static const struct mangle_fns mangle_hash2_fns = {
	mangle_reset,
	is_mangled,
	must_mangle,
	is_8_3,
	lookup_name_from_8_3,
	hash2_name_to_8_3
};

/* return the methods for this mangling implementation */
const struct mangle_fns *mangle_hash2_init(void)
{
	/* the mangle prefix can only be in the mange 1 to 6 */
	mangle_prefix = lp_mangle_prefix();
	if (mangle_prefix > 6) {
		mangle_prefix = 6;
	}
	if (mangle_prefix < 1) {
		mangle_prefix = 1;
	}

#if DYNAMIC_MANGLE_TABLES
	init_tables();
#endif
	mangle_reset();

	return &mangle_hash2_fns;
}

static void posix_mangle_reset(void)
{;}

static bool posix_is_mangled(const char *s, const struct share_params *p)
{
	return False;
}

static bool posix_must_mangle(const char *s, const struct share_params *p)
{
	return False;
}

static bool posix_is_8_3(const char *fname,
			bool check_case,
			bool allow_wildcards,
			const struct share_params *p)
{
	return False;
}

static bool posix_lookup_name_from_8_3(TALLOC_CTX *ctx,
				const char *in,
				char **out, /* talloced on the given context. */
				const struct share_params *p)
{
	return False;
}

static bool posix_name_to_8_3(const char *in,
				char out[13],
				bool cache83,
				int default_case,
				const struct share_params *p)
{
	memset(out, '\0', 13);
	return True;
}

/* POSIX paths backend - no mangle. */
static const struct mangle_fns posix_mangle_fns = {
	posix_mangle_reset,
	posix_is_mangled,
	posix_must_mangle,
	posix_is_8_3,
	posix_lookup_name_from_8_3,
	posix_name_to_8_3
};

const struct mangle_fns *posix_mangle_init(void)
{
	return &posix_mangle_fns;
}
