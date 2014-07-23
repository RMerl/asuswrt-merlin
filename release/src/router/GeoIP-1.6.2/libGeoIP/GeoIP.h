/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/* GeoIP.h
 *
 * Copyright (C) 2006 MaxMind LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef GEOIP_H
#define GEOIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else /* !defined(_WIN32) */
#include <winsock2.h>
#include <ws2tcpip.h>
#define snprintf _snprintf
#define FILETIME_TO_USEC(ft)                      \
    (((unsigned __int64)ft.dwHighDateTime << 32 | \
      ft.dwLowDateTime) / 10)
#endif /* !defined(_WIN32) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for fstat */
#include <sys/stat.h>  /* for fstat */

#define SEGMENT_RECORD_LENGTH 3
#define LARGE_SEGMENT_RECORD_LENGTH 4
#define STANDARD_RECORD_LENGTH 3
#define ORG_RECORD_LENGTH 4
#define MAX_RECORD_LENGTH 4
#define NUM_DB_TYPES (38 + 1)

/* 128 bit address in network order */
typedef struct in6_addr geoipv6_t;

#define GEOIP_CHKBIT_V6(bit,                \
                        ptr) (ptr[((127UL - \
                                    bit) >> 3)] & (1UL << (~(127 - bit) & 7)))

typedef struct GeoIPTag {
    FILE *GeoIPDatabase;
    char *file_path;
    unsigned char *cache;
    unsigned char *index_cache;
    unsigned int *databaseSegments;
    char databaseType;
    time_t mtime;
    int flags;
    off_t size;
    char record_length;
    int charset;            /* 0 iso-8859-1 1 utf8 */
    int record_iter;        /* used in GeoIP_next_record */
    int netmask;            /* netmask of last lookup - set using depth in _GeoIP_seek_record */
    time_t last_mtime_check;
    off_t dyn_seg_size;     /* currently only used by the cityconfidence database */
    unsigned int ext_flags; /* bit 0 teredo support enabled */
} GeoIP;

typedef struct GeoIPLookup {
    int netmask;
} GeoIPLookup;

typedef enum {
    GEOIP_TEREDO_BIT = 0
} GeoIPExtFlags;

typedef enum {
    GEOIP_CHARSET_ISO_8859_1 = 0,
    GEOIP_CHARSET_UTF8 = 1
} GeoIPCharset;

typedef struct GeoIPRegionTag {
    char country_code[3];
    char region[3];
} GeoIPRegion;

typedef enum {
    GEOIP_STANDARD = 0,
    GEOIP_MEMORY_CACHE = 1,
    GEOIP_CHECK_CACHE = 2,
    GEOIP_INDEX_CACHE = 4,
    GEOIP_MMAP_CACHE = 8,
} GeoIPOptions;

typedef enum {
    GEOIP_COUNTRY_EDITION = 1,
    GEOIP_REGION_EDITION_REV0 = 7,
    GEOIP_CITY_EDITION_REV0 = 6,
    GEOIP_ORG_EDITION = 5,
    GEOIP_ISP_EDITION = 4,
    GEOIP_CITY_EDITION_REV1 = 2,
    GEOIP_REGION_EDITION_REV1 = 3,
    GEOIP_PROXY_EDITION = 8,
    GEOIP_ASNUM_EDITION = 9,
    GEOIP_NETSPEED_EDITION = 10,
    GEOIP_DOMAIN_EDITION = 11,
    GEOIP_COUNTRY_EDITION_V6 = 12,
    GEOIP_LOCATIONA_EDITION = 13,
    GEOIP_ACCURACYRADIUS_EDITION = 14,
    GEOIP_CITYCONFIDENCE_EDITION = 15,           /* unsupported */
    GEOIP_CITYCONFIDENCEDIST_EDITION = 16,       /* unsupported */
    GEOIP_LARGE_COUNTRY_EDITION = 17,
    GEOIP_LARGE_COUNTRY_EDITION_V6 = 18,
    GEOIP_CITYCONFIDENCEDIST_ISP_ORG_EDITION = 19, /* unsued, but gaps are not allowed */
    GEOIP_CCM_COUNTRY_EDITION = 20,                /* unsued, but gaps are not allowed */
    GEOIP_ASNUM_EDITION_V6 = 21,
    GEOIP_ISP_EDITION_V6 = 22,
    GEOIP_ORG_EDITION_V6 = 23,
    GEOIP_DOMAIN_EDITION_V6 = 24,
    GEOIP_LOCATIONA_EDITION_V6 = 25,
    GEOIP_REGISTRAR_EDITION = 26,
    GEOIP_REGISTRAR_EDITION_V6 = 27,
    GEOIP_USERTYPE_EDITION = 28,
    GEOIP_USERTYPE_EDITION_V6 = 29,
    GEOIP_CITY_EDITION_REV1_V6 = 30,
    GEOIP_CITY_EDITION_REV0_V6 = 31,
    GEOIP_NETSPEED_EDITION_REV1 = 32,
    GEOIP_NETSPEED_EDITION_REV1_V6 = 33,
    GEOIP_COUNTRYCONF_EDITION = 34,
    GEOIP_CITYCONF_EDITION = 35,
    GEOIP_REGIONCONF_EDITION = 36,
    GEOIP_POSTALCONF_EDITION = 37,
    GEOIP_ACCURACYRADIUS_EDITION_V6 = 38
} GeoIPDBTypes;

typedef enum {
    GEOIP_ANON_PROXY = 1,
    GEOIP_HTTP_X_FORWARDED_FOR_PROXY = 2,
    GEOIP_HTTP_CLIENT_IP_PROXY = 3,
} GeoIPProxyTypes;

typedef enum {
    GEOIP_UNKNOWN_SPEED = 0,
    GEOIP_DIALUP_SPEED = 1,
    GEOIP_CABLEDSL_SPEED = 2,
    GEOIP_CORPORATE_SPEED = 3,
} GeoIPNetspeedValues;


#if defined(_WIN32)
#ifdef GEOIP_EXPORTS
#define GEOIP_API __declspec(dllexport)
#define GEOIP_DATA __declspec(dllexport)
#else
#define GEOIP_DATA __declspec(dllimport)
#define GEOIP_API
#endif  /* GEOIP_EXPORTS */
#else
#define GEOIP_API
#define GEOIP_DATA
#endif

extern char **GeoIPDBFileName;
extern GEOIP_DATA const char * GeoIPDBDescription[NUM_DB_TYPES];
extern GEOIP_DATA const char *GeoIPCountryDBFileName;
extern GEOIP_DATA const char *GeoIPRegionDBFileName;
extern GEOIP_DATA const char *GeoIPCityDBFileName;
extern GEOIP_DATA const char *GeoIPOrgDBFileName;
extern GEOIP_DATA const char *GeoIPISPDBFileName;
extern GEOIP_DATA const char *GeoIPLocationADBFileName;
extern GEOIP_DATA const char *GeoIPAccuracyRadiusFileName;
extern GEOIP_DATA const char *GeoIPCityConfidenceFileName;
extern char * GeoIP_custom_directory;

/* Warning: do not use those arrays as doing so may break your
 * program with newer GeoIP versions */
extern GEOIP_DATA const char GeoIP_country_code[256][3];
extern GEOIP_DATA const char GeoIP_country_code3[256][4];
extern GEOIP_DATA const char * GeoIP_country_name[256];
extern GEOIP_DATA const char * GeoIP_utf8_country_name[256];
extern GEOIP_DATA const char GeoIP_country_continent[256][3];

GEOIP_API void GeoIP_setup_custom_directory(char *dir);
GEOIP_API GeoIP * GeoIP_open_type(int type, int flags);
GEOIP_API GeoIP * GeoIP_new(int flags);
GEOIP_API GeoIP * GeoIP_open(const char * filename, int flags);
/*
 * WARNING: GeoIP_db_avail() checks for the existence of a database
 * file but does not check that it has the requested database revision.
 * For database types which have more than one revision (including Region,
 * City, and Cityv6), this can lead to unexpected results. Check the
 * return value of GeoIP_open_type() to find out whether a particular
 * database type is really available.
 */
GEOIP_API int GeoIP_db_avail(int type);
GEOIP_API void GeoIP_delete(GeoIP * gi);

GEOIP_API const char *GeoIP_country_code_by_addr_gl(GeoIP * gi,
                                                    const char *addr,
                                                    GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code_by_name_gl(GeoIP * gi,
                                                    const char *host,
                                                    GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_addr_gl(GeoIP * gi,
                                                     const char *addr,
                                                     GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_name_gl(GeoIP * gi,
                                                     const char *host,
                                                     GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_name_by_addr_gl(GeoIP * gi,
                                                    const char *addr,
                                                    GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_name_by_name_gl(GeoIP * gi,
                                                    const char *host,
                                                    GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_name_by_ipnum_gl(GeoIP * gi,
                                                     unsigned long ipnum,
                                                     GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code_by_ipnum_gl(GeoIP * gi,
                                                     unsigned long ipnum,
                                                     GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_ipnum_gl(GeoIP * gi,
                                                      unsigned long ipnum,
                                                      GeoIPLookup * gl);

/* */
GEOIP_API const char *GeoIP_country_name_by_ipnum_v6_gl(GeoIP * gi,
                                                        geoipv6_t ipnum,
                                                        GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code_by_ipnum_v6_gl(GeoIP * gi,
                                                        geoipv6_t ipnum,
                                                        GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_ipnum_v6_gl(GeoIP * gi,
                                                         geoipv6_t ipnum,
                                                         GeoIPLookup * gl);

GEOIP_API const char *GeoIP_country_code_by_addr_v6_gl(GeoIP * gi,
                                                       const char *addr,
                                                       GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code_by_name_v6_gl(GeoIP * gi,
                                                       const char *host,
                                                       GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_addr_v6_gl(GeoIP * gi,
                                                        const char *addr,
                                                        GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_code3_by_name_v6_gl(GeoIP * gi,
                                                        const char *host,
                                                        GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_name_by_addr_v6_gl(GeoIP * gi,
                                                       const char *addr,
                                                       GeoIPLookup * gl);
GEOIP_API const char *GeoIP_country_name_by_name_v6_gl(GeoIP * gi,
                                                       const char *host,
                                                       GeoIPLookup * gl);

GEOIP_API int GeoIP_id_by_addr_gl(GeoIP * gi, const char *addr,
                                  GeoIPLookup * gl);
GEOIP_API int GeoIP_id_by_name_gl(GeoIP * gi, const char *host,
                                  GeoIPLookup * gl);
GEOIP_API int GeoIP_id_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                   GeoIPLookup * gl);

GEOIP_API int GeoIP_id_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                     GeoIPLookup * gl);
GEOIP_API int GeoIP_id_by_name_v6_gl(GeoIP * gi, const char *host,
                                     GeoIPLookup * gl);
GEOIP_API int GeoIP_id_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                      GeoIPLookup * gl);

GEOIP_API GeoIPRegion * GeoIP_region_by_addr_gl(GeoIP * gi, const char *addr,
                                                GeoIPLookup * gl);
GEOIP_API GeoIPRegion * GeoIP_region_by_name_gl(GeoIP * gi, const char *host,
                                                GeoIPLookup * gl);
GEOIP_API GeoIPRegion * GeoIP_region_by_ipnum_gl(GeoIP *gi, unsigned long ipnum,
                                                 GeoIPLookup * gl);

GEOIP_API GeoIPRegion * GeoIP_region_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                                   GeoIPLookup * gl);
GEOIP_API GeoIPRegion * GeoIP_region_by_name_v6_gl(GeoIP * gi, const char *host,
                                                   GeoIPLookup * gl);
GEOIP_API GeoIPRegion * GeoIP_region_by_ipnum_v6_gl(GeoIP *gi, geoipv6_t ipnum,
                                                    GeoIPLookup * gl);

/* Warning - don't call this after GeoIP_assign_region_by_inetaddr calls */
GEOIP_API void GeoIPRegion_delete(GeoIPRegion *gir);

GEOIP_API void GeoIP_assign_region_by_inetaddr_gl(GeoIP * gi,
                                                  unsigned long inetaddr,
                                                  GeoIPRegion *gir,
                                                  GeoIPLookup * gl);
GEOIP_API void GeoIP_assign_region_by_inetaddr_v6_gl(GeoIP * gi,
                                                     geoipv6_t inetaddr,
                                                     GeoIPRegion *gir,
                                                     GeoIPLookup * gl);

/* Used to query GeoIP Organization, ISP and AS Number databases */
GEOIP_API char *GeoIP_name_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                       GeoIPLookup * gl);
GEOIP_API char *GeoIP_name_by_addr_gl(GeoIP * gi, const char *addr,
                                      GeoIPLookup * gl);
GEOIP_API char *GeoIP_name_by_name_gl(GeoIP * gi, const char *host,
                                      GeoIPLookup * gl);

GEOIP_API char *GeoIP_name_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                          GeoIPLookup * gl);
GEOIP_API char *GeoIP_name_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                         GeoIPLookup * gl);
GEOIP_API char *GeoIP_name_by_name_v6_gl(GeoIP * gi, const char *name,
                                         GeoIPLookup * gl);

/** return two letter country code */
GEOIP_API const char * GeoIP_code_by_id(int id);

/** return three letter country code */
GEOIP_API const char * GeoIP_code3_by_id(int id);

/** return full name of country in utf8 or iso-8859-1 */
GEOIP_API const char * GeoIP_country_name_by_id(GeoIP * gi, int id);

/** return full name of country */
GEOIP_API const char * GeoIP_name_by_id(int id);

/** return continent of country */
GEOIP_API const char * GeoIP_continent_by_id(int id);

/** return id by country code **/
GEOIP_API int GeoIP_id_by_code(const char *country);

/** return return number of known countries */
GEOIP_API unsigned GeoIP_num_countries(void);

GEOIP_API char *GeoIP_database_info(GeoIP * gi);
GEOIP_API unsigned char GeoIP_database_edition(GeoIP * gi);

GEOIP_API int GeoIP_charset(GeoIP * gi);
GEOIP_API int GeoIP_set_charset(GeoIP * gi, int charset);
GEOIP_API int GeoIP_enable_teredo(GeoIP * gi, int true_false );
GEOIP_API int GeoIP_teredo(GeoIP * gi );

GEOIP_API char **GeoIP_range_by_ip_gl(GeoIP * gi, const char *addr,
                                      GeoIPLookup * gl);
GEOIP_API void GeoIP_range_by_ip_delete(char **ptr);

/* Convert region code to region name */
GEOIP_API const char * GeoIP_region_name_by_code(const char *country_code,
                                                 const char *region_code);

/* Get timezone from country and region code */
GEOIP_API const char * GeoIP_time_zone_by_country_and_region(
    const char *country_code, const char *region_code);

/* some v4 helper functions as of 1.4.7 exported to the public API */
GEOIP_API unsigned long GeoIP_addr_to_num(const char *addr);
GEOIP_API char *        GeoIP_num_to_addr(unsigned long ipnum);

/* Internal function -- convert iso to utf8; return a malloced utf8 string. */
char * _GeoIP_iso_8859_1__utf8(const char * iso);

/* Cleans up memory used to hold file name paths. Returns 1 if successful; otherwise 0.
 * */
GEOIP_API int GeoIP_cleanup(void);

/* Returns the library version in use. Helpful if your loading dynamically. */
GEOIP_API const char * GeoIP_lib_version(void);


/* deprecated */
GEOIP_API const char *GeoIP_country_code_by_addr(GeoIP * gi, const char *addr);
GEOIP_API const char *GeoIP_country_code_by_name(GeoIP * gi, const char *host);
GEOIP_API const char *GeoIP_country_code3_by_addr(GeoIP * gi, const char *addr);
GEOIP_API const char *GeoIP_country_code3_by_name(GeoIP * gi, const char *host);
GEOIP_API const char *GeoIP_country_name_by_addr(GeoIP * gi, const char *addr);
GEOIP_API const char *GeoIP_country_name_by_name(GeoIP * gi, const char *host);
GEOIP_API const char *GeoIP_country_name_by_ipnum(GeoIP * gi,
                                                  unsigned long ipnum);
GEOIP_API const char *GeoIP_country_code_by_ipnum(GeoIP * gi,
                                                  unsigned long ipnum);
GEOIP_API const char *GeoIP_country_code3_by_ipnum(GeoIP * gi,
                                                   unsigned long ipnum);

GEOIP_API const char *GeoIP_country_name_by_ipnum_v6(GeoIP * gi,
                                                     geoipv6_t ipnum);
GEOIP_API const char *GeoIP_country_code_by_ipnum_v6(GeoIP * gi,
                                                     geoipv6_t ipnum);
GEOIP_API const char *GeoIP_country_code3_by_ipnum_v6(GeoIP * gi,
                                                      geoipv6_t ipnum);

GEOIP_API const char *GeoIP_country_code_by_addr_v6(GeoIP * gi,
                                                    const char *addr);
GEOIP_API const char *GeoIP_country_code_by_name_v6(GeoIP * gi,
                                                    const char *host);
GEOIP_API const char *GeoIP_country_code3_by_addr_v6(GeoIP * gi,
                                                     const char *addr);
GEOIP_API const char *GeoIP_country_code3_by_name_v6(GeoIP * gi,
                                                     const char *host);
GEOIP_API const char *GeoIP_country_name_by_addr_v6(GeoIP * gi,
                                                    const char *addr);
GEOIP_API const char *GeoIP_country_name_by_name_v6(GeoIP * gi,
                                                    const char *host);

GEOIP_API int GeoIP_id_by_addr(GeoIP * gi, const char *addr);
GEOIP_API int GeoIP_id_by_name(GeoIP * gi, const char *host);
GEOIP_API int GeoIP_id_by_ipnum(GeoIP * gi, unsigned long ipnum);

GEOIP_API int GeoIP_id_by_addr_v6(GeoIP * gi, const char *addr);
GEOIP_API int GeoIP_id_by_name_v6(GeoIP * gi, const char *host);
GEOIP_API int GeoIP_id_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum);

GEOIP_API GeoIPRegion * GeoIP_region_by_addr(GeoIP * gi, const char *addr);
GEOIP_API GeoIPRegion * GeoIP_region_by_name(GeoIP * gi, const char *host);
GEOIP_API GeoIPRegion * GeoIP_region_by_ipnum(GeoIP *gi, unsigned long ipnum);

GEOIP_API GeoIPRegion * GeoIP_region_by_addr_v6(GeoIP * gi, const char *addr);
GEOIP_API GeoIPRegion * GeoIP_region_by_name_v6(GeoIP * gi, const char *host);
GEOIP_API GeoIPRegion * GeoIP_region_by_ipnum_v6(GeoIP *gi, geoipv6_t ipnum);

GEOIP_API void GeoIP_assign_region_by_inetaddr(GeoIP * gi,
                                               unsigned long inetaddr,
                                               GeoIPRegion *gir);
GEOIP_API void GeoIP_assign_region_by_inetaddr_v6(GeoIP * gi,
                                                  geoipv6_t inetaddr,
                                                  GeoIPRegion *gir);

GEOIP_API char *GeoIP_name_by_ipnum(GeoIP * gi, unsigned long ipnum);
GEOIP_API char *GeoIP_name_by_addr(GeoIP * gi, const char *addr);
GEOIP_API char *GeoIP_name_by_name(GeoIP * gi, const char *host);

GEOIP_API char *GeoIP_name_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum);
GEOIP_API char *GeoIP_name_by_addr_v6(GeoIP * gi, const char *addr);
GEOIP_API char *GeoIP_name_by_name_v6(GeoIP * gi, const char *name);


/** GeoIP_last_netmask is deprecated - it is not thread safe */
GEOIP_API int GeoIP_last_netmask(GeoIP * gi);
GEOIP_API char **GeoIP_range_by_ip(GeoIP * gi, const char *addr);

/* Deprecated - for backwards compatibility only */
GEOIP_API int GeoIP_country_id_by_addr(GeoIP * gi, const char *addr);
GEOIP_API int GeoIP_country_id_by_name(GeoIP * gi, const char *host);
GEOIP_API char *GeoIP_org_by_addr(GeoIP * gi, const char *addr);
GEOIP_API char *GeoIP_org_by_name(GeoIP * gi, const char *host);
GEOIP_API char *GeoIP_org_by_ipnum(GeoIP * gi, unsigned long ipnum);

GEOIP_API int GeoIP_country_id_by_addr_v6(GeoIP * gi, const char *addr);
GEOIP_API char *GeoIP_org_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum);
GEOIP_API char *GeoIP_org_by_addr_v6(GeoIP * gi, const char *addr);
GEOIP_API char *GeoIP_org_by_name_v6(GeoIP * gi, const char *name);

/* End deprecated */



#
#ifdef __cplusplus
}
#endif

#endif /* GEOIP_H */
