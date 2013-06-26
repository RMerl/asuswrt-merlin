/*
 * Copyright (C) 2010 Andrew Tridgell
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
  This header provides a minimal set of defines and typedefs needed
  for building an external DLZ module for bind9. When creating a new
  external DLZ driver, please copy this header into your own source
  tree.
 */
typedef unsigned int isc_result_t;
typedef bool isc_boolean_t;
typedef uint32_t dns_ttl_t;

#define DLZ_DLOPEN_VERSION 1

/* return this in flags to dlz_version() if thread safe */
#define DNS_SDLZFLAG_THREADSAFE		0x00000001U

/* result codes */
#define ISC_R_SUCCESS			0
#define ISC_R_NOMEMORY			1
#define ISC_R_NOTFOUND			23
#define ISC_R_FAILURE			25

/* log levels */
#define ISC_LOG_INFO		(-1)
#define ISC_LOG_NOTICE		(-2)
#define ISC_LOG_WARNING 	(-3)
#define ISC_LOG_ERROR		(-4)
#define ISC_LOG_CRITICAL	(-5)

/* some opaque structures */
typedef void *dns_sdlzlookup_t;
typedef void *dns_sdlzallnodes_t;
typedef void *dns_view_t;

/*
 * prototypes for the functions you can include in your driver
 */


/*
 * dlz_version() is required for all DLZ external drivers. It should
 * return DLZ_DLOPEN_VERSION
 */
int dlz_version(unsigned int *flags);

/*
 * dlz_create() is required for all DLZ external drivers.
 */
isc_result_t dlz_create(const char *dlzname, unsigned int argc, char *argv[], void **dbdata, ...);

/*
 * dlz_destroy() is optional, and will be called when the driver is
 * unloaded if supplied
 */
void dlz_destroy(void *dbdata);

/*
  dlz_findzonedb is required for all DLZ external drivers
 */
isc_result_t dlz_findzonedb(void *dbdata, const char *name);

/*
  dlz_lookup is required for all DLZ external drivers
 */
isc_result_t dlz_lookup(const char *zone, const char *name,
			void *dbdata, dns_sdlzlookup_t *lookup);

/*
  dlz_allowzonexfr() is optional, and should be supplied if you want
  to support zone transfers
 */
isc_result_t dlz_allowzonexfr(void *dbdata, const char *name, const char *client);


/*
  dlz_allnodes() is optional, but must be supplied if supply a
  dlz_allowzonexfr() function
 */
isc_result_t dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes);

/*
  dlz_newversion() is optional. It should be supplied if you want to
  support dynamic updates.
 */
isc_result_t dlz_newversion(const char *zone, void *dbdata, void **versionp);

/*
   dlz_closeversion() is optional, but must be supplied if you supply
   a dlz_newversion() function
 */
void dlz_closeversion(const char *zone, isc_boolean_t commit, void *dbdata, void **versionp);

/*
  dlz_configure() is optional, but must be supplied if you want to
  support dynamic updates
 */
isc_result_t dlz_configure(dns_view_t *view, void *dbdata);

/*
  dlz_ssumatch() is optional, but must be supplied if you want to
  support dynamic updates
 */
isc_boolean_t dlz_ssumatch(const char *signer, const char *name, const char *tcpaddr,
			   const char *type, const char *key, uint32_t keydatalen, uint8_t *keydata,
			   void *dbdata);

/*
  dlz_addrdataset() is optional, but must be supplied if you want to
  support dynamic updates
 */
isc_result_t dlz_addrdataset(const char *name, const char *rdatastr, void *dbdata, void *version);

/*
  dlz_subrdataset() is optional, but must be supplied if you want to
  support dynamic updates
 */
isc_result_t dlz_subrdataset(const char *name, const char *rdatastr, void *dbdata, void *version);

/*
  dlz_delrdataset() is optional, but must be supplied if you want to
  support dynamic updates
 */
isc_result_t dlz_delrdataset(const char *name, const char *type, void *dbdata, void *version);
