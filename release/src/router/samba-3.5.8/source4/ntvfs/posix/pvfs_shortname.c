/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - 8.3 name routines

   Copyright (C) Andrew Tridgell 2004

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
#include "system/locale.h"
#include "vfs_posix.h"
#include "param/param.h"

/*
  this mangling scheme uses the following format

  Annnn~n.AAA

  where nnnnn is a base 36 hash, and A represents characters from the original string

  The hash is taken of the leading part of the long filename, in uppercase

  for simplicity, we only allow ascii characters in 8.3 names
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

#define DEFAULT_MANGLE_PREFIX 4

#define MANGLE_BASECHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#define FLAG_CHECK(c, flag) (ctx->char_flags[(uint8_t)(c)] & (flag))

static const char *reserved_names[] = 
{ "AUX", "CON", "COM1", "COM2", "COM3", "COM4",
  "LPT1", "LPT2", "LPT3", "NUL", "PRN", NULL };


struct pvfs_mangle_context {
	uint8_t char_flags[256];
	/*
	  this determines how many characters are used from the original
	  filename in the 8.3 mangled name. A larger value leads to a weaker
	  hash and more collisions.  The largest possible value is 6.
	*/
	int mangle_prefix;
	uint32_t mangle_modulus;

	/* we will use a very simple direct mapped prefix cache. The big
	   advantage of this cache structure is speed and low memory usage 

	   The cache is indexed by the low-order bits of the hash, and confirmed by
	   hashing the resulting cache entry to match the known hash
	*/
	uint32_t cache_size;
	char **prefix_cache;
	uint32_t *prefix_cache_hashes;

	/* this is used to reverse the base 36 mapping */
	unsigned char base_reverse[256];

	struct smb_iconv_convenience *iconv_convenience;
};


/* 
   hash a string of the specified length. The string does not need to be
   null terminated 

   this hash needs to be fast with a low collision rate (what hash doesn't?)
*/
static uint32_t mangle_hash(struct pvfs_mangle_context *ctx,
			    const char *key, size_t length)
{
	return pvfs_name_hash(key, length) % ctx->mangle_modulus;
}

/*
  insert an entry into the prefix cache. The string might not be null
  terminated */
static void cache_insert(struct pvfs_mangle_context *ctx,
			 const char *prefix, int length, uint32_t hash)
{
	int i = hash % ctx->cache_size;

	if (ctx->prefix_cache[i]) {
		talloc_free(ctx->prefix_cache[i]);
	}

	ctx->prefix_cache[i] = talloc_strndup(ctx->prefix_cache, prefix, length);
	ctx->prefix_cache_hashes[i] = hash;
}

/*
  lookup an entry in the prefix cache. Return NULL if not found.
*/
static const char *cache_lookup(struct pvfs_mangle_context *ctx, uint32_t hash)
{
	int i = hash % ctx->cache_size;


	if (!ctx->prefix_cache[i] || hash != ctx->prefix_cache_hashes[i]) {
		return NULL;
	}

	/* yep, it matched */
	return ctx->prefix_cache[i];
}


/* 
   determine if a string is possibly in a mangled format, ignoring
   case 

   In this algorithm, mangled names use only pure ascii characters (no
   multi-byte) so we can avoid doing a UCS2 conversion 
 */
static bool is_mangled_component(struct pvfs_mangle_context *ctx,
				 const char *name, size_t len)
{
	unsigned int i;

	M_DEBUG(10,("is_mangled_component %s (len %u) ?\n", name, (unsigned int)len));

	/* check the length */
	if (len > 12 || len < 8)
		return false;

	/* the best distinguishing characteristic is the ~ */
	if (name[6] != '~')
		return false;

	/* check extension */
	if (len > 8) {
		if (name[8] != '.')
			return false;
		for (i=9; name[i] && i < len; i++) {
			if (! FLAG_CHECK(name[i], FLAG_ASCII)) {
				return false;
			}
		}
	}
	
	/* check lead characters */
	for (i=0;i<ctx->mangle_prefix;i++) {
		if (! FLAG_CHECK(name[i], FLAG_ASCII)) {
			return false;
		}
	}
	
	/* check rest of hash */
	if (! FLAG_CHECK(name[7], FLAG_BASECHAR)) {
		return false;
	}
	for (i=ctx->mangle_prefix;i<6;i++) {
		if (! FLAG_CHECK(name[i], FLAG_BASECHAR)) {
			return false;
		}
	}

	M_DEBUG(10,("is_mangled_component %s (len %u) -> yes\n", name, (unsigned int)len));

	return true;
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
static bool is_mangled(struct pvfs_mangle_context *ctx, const char *name)
{
	const char *p;
	const char *s;

	M_DEBUG(10,("is_mangled %s ?\n", name));

	for (s=name; (p=strchr(s, '/')); s=p+1) {
		if (is_mangled_component(ctx, s, PTR_DIFF(p, s))) {
			return true;
		}
	}
	
	/* and the last part ... */
	return is_mangled_component(ctx, s, strlen(s));
}


/* 
   see if a filename is an allowable 8.3 name.

   we are only going to allow ascii characters in 8.3 names, as this
   simplifies things greatly (it means that we know the string won't
   get larger when converted from UNIX to DOS formats)
*/
static bool is_8_3(struct pvfs_mangle_context *ctx,
		   const char *name, bool check_case, bool allow_wildcards)
{
	int len, i;
	char *dot_p;

	/* as a special case, the names '.' and '..' are allowable 8.3 names */
	if (name[0] == '.') {
		if (!name[1] || (name[1] == '.' && !name[2])) {
			return true;
		}
	}

	/* the simplest test is on the overall length of the
	 filename. Note that we deliberately use the ascii string
	 length (not the multi-byte one) as it is faster, and gives us
	 the result we need in this case. Using strlen_m would not
	 only be slower, it would be incorrect */
	len = strlen(name);
	if (len > 12)
		return false;

	/* find the '.'. Note that once again we use the non-multibyte
           function */
	dot_p = strchr(name, '.');

	if (!dot_p) {
		/* if the name doesn't contain a '.' then its length
                   must be less than 8 */
		if (len > 8) {
			return false;
		}
	} else {
		int prefix_len, suffix_len;

		/* if it does contain a dot then the prefix must be <=
		   8 and the suffix <= 3 in length */
		prefix_len = PTR_DIFF(dot_p, name);
		suffix_len = len - (prefix_len+1);

		if (prefix_len > 8 || suffix_len > 3 || suffix_len == 0) {
			return false;
		}

		/* a 8.3 name cannot contain more than 1 '.' */
		if (strchr(dot_p+1, '.')) {
			return false;
		}
	}

	/* the length are all OK. Now check to see if the characters themselves are OK */
	for (i=0; name[i]; i++) {
		/* note that we may allow wildcard petterns! */
		if (!FLAG_CHECK(name[i], FLAG_ASCII|(allow_wildcards ? FLAG_WILDCARD : 0)) && 
		    name[i] != '.') {
			return false;
		}
	}

	/* it is a good 8.3 name */
	return true;
}


/*
  try to find a 8.3 name in the cache, and if found then
  return the original long name. 
*/
static char *check_cache(struct pvfs_mangle_context *ctx, 
			 TALLOC_CTX *mem_ctx, const char *name)
{
	uint32_t hash, multiplier;
	unsigned int i;
	const char *prefix;
	char extension[4];

	/* make sure that this is a mangled name from this cache */
	if (!is_mangled(ctx, name)) {
		M_DEBUG(10,("check_cache: %s -> not mangled\n", name));
		return NULL;
	}

	/* we need to extract the hash from the 8.3 name */
	hash = ctx->base_reverse[(unsigned char)name[7]];
	for (multiplier=36, i=5;i>=ctx->mangle_prefix;i--) {
		uint32_t v = ctx->base_reverse[(unsigned char)name[i]];
		hash += multiplier * v;
		multiplier *= 36;
	}

	/* now look in the prefix cache for that hash */
	prefix = cache_lookup(ctx, hash);
	if (!prefix) {
		M_DEBUG(10,("check_cache: %s -> %08X -> not found\n", name, hash));
		return NULL;
	}

	/* we found it - construct the full name */
	if (name[8] == '.') {
		strncpy(extension, name+9, 3);
		extension[3] = 0;
	} else {
		extension[0] = 0;
	}

	if (extension[0]) {
		return talloc_asprintf(mem_ctx, "%s.%s", prefix, extension);
	}

	return talloc_strdup(mem_ctx, prefix);
}


/*
  look for a DOS reserved name
*/
static bool is_reserved_name(struct pvfs_mangle_context *ctx, const char *name)
{
	if (FLAG_CHECK(name[0], FLAG_POSSIBLE1) &&
	    FLAG_CHECK(name[1], FLAG_POSSIBLE2) &&
	    FLAG_CHECK(name[2], FLAG_POSSIBLE3) &&
	    FLAG_CHECK(name[3], FLAG_POSSIBLE4)) {
		/* a likely match, scan the lot */
		int i;
		for (i=0; reserved_names[i]; i++) {
			if (strcasecmp(name, reserved_names[i]) == 0) {
				return true;
			}
		}
	}

	return false;
}


/*
 See if a filename is a legal long filename.
 A filename ending in a '.' is not legal unless it's "." or "..". JRA.
*/
static bool is_legal_name(struct pvfs_mangle_context *ctx, const char *name)
{
	while (*name) {
		size_t c_size;
		codepoint_t c = next_codepoint_convenience(ctx->iconv_convenience, name, &c_size);
		if (c == INVALID_CODEPOINT) {
			return false;
		}
		/* all high chars are OK */
		if (c >= 128) {
			name += c_size;
			continue;
		}
		if (FLAG_CHECK(c, FLAG_ILLEGAL)) {
			return false;
		}
		name += c_size;
	}

	return true;
}

/*
  the main forward mapping function, which converts a long filename to 
  a 8.3 name

  if need83 is not set then we only do the mangling if the name is illegal
  as a long name

  if cache83 is not set then we don't cache the result

  return NULL if we don't need to do any conversion
*/
static char *name_map(struct pvfs_mangle_context *ctx,
		      const char *name, bool need83, bool cache83)
{
	char *dot_p;
	char lead_chars[7];
	char extension[4];
	unsigned int extension_length, i;
	unsigned int prefix_len;
	uint32_t hash, v;
	char *new_name;
	const char *basechars = MANGLE_BASECHARS;

	/* reserved names are handled specially */
	if (!is_reserved_name(ctx, name)) {
		/* if the name is already a valid 8.3 name then we don't need to 
		   do anything */
		if (is_8_3(ctx, name, false, false)) {
			return NULL;
		}

		/* if the caller doesn't strictly need 8.3 then just check for illegal 
		   filenames */
		if (!need83 && is_legal_name(ctx, name)) {
			return NULL;
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
		if (i == 0 || i == 4) dot_p = NULL;
	}

	/* the leading characters in the mangled name is taken from
	   the first characters of the name, if they are ascii otherwise
	   '_' is used
	*/
	for (i=0;i<ctx->mangle_prefix && name[i];i++) {
		lead_chars[i] = name[i];
		if (! FLAG_CHECK(lead_chars[i], FLAG_ASCII)) {
			lead_chars[i] = '_';
		}
		lead_chars[i] = toupper((unsigned char)lead_chars[i]);
	}
	for (;i<ctx->mangle_prefix;i++) {
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
			unsigned char c = dot_p[i];
			if (FLAG_CHECK(c, FLAG_ASCII)) {
				extension[extension_length++] = toupper(c);
			}
		}
	}
	   
	/* find the hash for this prefix */
	v = hash = mangle_hash(ctx, name, prefix_len);

	new_name = talloc_array(ctx, char, 13);
	if (new_name == NULL) {
		return NULL;
	}

	/* now form the mangled name. */
	for (i=0;i<ctx->mangle_prefix;i++) {
		new_name[i] = lead_chars[i];
	}
	new_name[7] = basechars[v % 36];
	new_name[6] = '~';	
	for (i=5; i>=ctx->mangle_prefix; i--) {
		v = v / 36;
		new_name[i] = basechars[v % 36];
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
		cache_insert(ctx, name, prefix_len, hash);
	}

	M_DEBUG(10,("name_map: %s -> %08X -> %s (cache=%d)\n", 
		   name, hash, new_name, cache83));

	return new_name;
}


/* initialise the flags table 

  we allow only a very restricted set of characters as 'ascii' in this
  mangling backend. This isn't a significant problem as modern clients
  use the 'long' filenames anyway, and those don't have these
  restrictions. 
*/
static void init_tables(struct pvfs_mangle_context *ctx)
{
	const char *basechars = MANGLE_BASECHARS;
	int i;
	/* the list of reserved dos names - all of these are illegal */

	ZERO_STRUCT(ctx->char_flags);

	for (i=1;i<128;i++) {
		if ((i >= '0' && i <= '9') || 
		    (i >= 'a' && i <= 'z') || 
		    (i >= 'A' && i <= 'Z')) {
			ctx->char_flags[i] |=  (FLAG_ASCII | FLAG_BASECHAR);
		}
		if (strchr("_-$~", i)) {
			ctx->char_flags[i] |= FLAG_ASCII;
		}

		if (strchr("*\\/?<>|\":", i)) {
			ctx->char_flags[i] |= FLAG_ILLEGAL;
		}

		if (strchr("*?\"<>", i)) {
			ctx->char_flags[i] |= FLAG_WILDCARD;
		}
	}

	ZERO_STRUCT(ctx->base_reverse);
	for (i=0;i<36;i++) {
		ctx->base_reverse[(uint8_t)basechars[i]] = i;
	}	

	/* fill in the reserved names flags. These are used as a very
	   fast filter for finding possible DOS reserved filenames */
	for (i=0; reserved_names[i]; i++) {
		unsigned char c1, c2, c3, c4;

		c1 = (unsigned char)reserved_names[i][0];
		c2 = (unsigned char)reserved_names[i][1];
		c3 = (unsigned char)reserved_names[i][2];
		c4 = (unsigned char)reserved_names[i][3];

		ctx->char_flags[c1] |= FLAG_POSSIBLE1;
		ctx->char_flags[c2] |= FLAG_POSSIBLE2;
		ctx->char_flags[c3] |= FLAG_POSSIBLE3;
		ctx->char_flags[c4] |= FLAG_POSSIBLE4;
		ctx->char_flags[tolower(c1)] |= FLAG_POSSIBLE1;
		ctx->char_flags[tolower(c2)] |= FLAG_POSSIBLE2;
		ctx->char_flags[tolower(c3)] |= FLAG_POSSIBLE3;
		ctx->char_flags[tolower(c4)] |= FLAG_POSSIBLE4;

		ctx->char_flags[(unsigned char)'.'] |= FLAG_POSSIBLE4;
	}

	ctx->mangle_modulus = 1;
	for (i=0;i<(7-ctx->mangle_prefix);i++) {
		ctx->mangle_modulus *= 36;
	}
}

/* 
   initialise the mangling code 
 */
NTSTATUS pvfs_mangle_init(struct pvfs_state *pvfs)
{
	struct pvfs_mangle_context *ctx;

	ctx = talloc(pvfs, struct pvfs_mangle_context);
	if (ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ctx->iconv_convenience = lp_iconv_convenience(pvfs->ntvfs->ctx->lp_ctx);

	/* by default have a max of 512 entries in the cache. */
	ctx->cache_size = lp_parm_int(pvfs->ntvfs->ctx->lp_ctx, NULL, "mangle", "cachesize", 512);

	ctx->prefix_cache = talloc_array(ctx, char *, ctx->cache_size);
	if (ctx->prefix_cache == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	ctx->prefix_cache_hashes = talloc_array(ctx, uint32_t, ctx->cache_size);
	if (ctx->prefix_cache_hashes == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	memset(ctx->prefix_cache, 0, sizeof(char *) * ctx->cache_size);
	memset(ctx->prefix_cache_hashes, 0, sizeof(uint32_t) * ctx->cache_size);

	ctx->mangle_prefix = lp_parm_int(pvfs->ntvfs->ctx->lp_ctx, NULL, "mangle", "prefix", -1);
	if (ctx->mangle_prefix < 0 || ctx->mangle_prefix > 6) {
		ctx->mangle_prefix = DEFAULT_MANGLE_PREFIX;
	}

	init_tables(ctx);

	pvfs->mangle_ctx = ctx;

	return NT_STATUS_OK;
}


/*
  return the short name for a component of a full name
*/
char *pvfs_short_name_component(struct pvfs_state *pvfs, const char *name)
{
	return name_map(pvfs->mangle_ctx, name, true, true);
}


/*
  return the short name for a given entry in a directory
*/
const char *pvfs_short_name(struct pvfs_state *pvfs, TALLOC_CTX *mem_ctx, 
			    struct pvfs_filename *name)
{
	char *p = strrchr(name->full_name, '/');
	char *ret = pvfs_short_name_component(pvfs, p+1);
	if (ret == NULL) {
		return p+1;
	}
	talloc_steal(mem_ctx, ret);
	return ret;
}

/*
  lookup a mangled name, returning the original long name if present
  in the cache
*/
char *pvfs_mangled_lookup(struct pvfs_state *pvfs, TALLOC_CTX *mem_ctx, 
			  const char *name)
{
	return check_cache(pvfs->mangle_ctx, mem_ctx, name);
}


/*
  look for a DOS reserved name
*/
bool pvfs_is_reserved_name(struct pvfs_state *pvfs, const char *name)
{
	return is_reserved_name(pvfs->mangle_ctx, name);
}


/*
  see if a component of a filename could be a mangled name from our
  mangling code
*/
bool pvfs_is_mangled_component(struct pvfs_state *pvfs, const char *name)
{
	return is_mangled_component(pvfs->mangle_ctx, name, strlen(name));
}
