
/*
 * GeoIPCity.c
 *
 * Copyright (C) 2006 MaxMind LLC
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "GeoIP.h"
#include "GeoIP_internal.h"
#include "GeoIPCity.h"
#if !defined(_WIN32)
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h> /* For ntohl */
#else
#include <windows.h>
#include <winsock.h>
#endif
#include <sys/types.h>  /* For uint32_t */
#ifdef HAVE_STDINT_H
#include <stdint.h>     /* For uint32_t */
#endif

#if defined(_WIN32)
#include "pread.h"
#endif

#ifndef HAVE_PREAD
#define pread(fd, buf, count, offset)           \
    (                                           \
        lseek(fd, offset, SEEK_SET) == offset ? \
        read(fd, buf, count) :                  \
        -1                                      \
    )
#endif /* HAVE_PREAD */


static
const int FULL_RECORD_LENGTH = 50;


static
GeoIPRecord *
_extract_record(GeoIP * gi, unsigned int seek_record, int *next_record_ptr)
{
    int record_pointer;
    unsigned char *record_buf = NULL;
    unsigned char *begin_record_buf = NULL;
    GeoIPRecord *record;
    int str_length = 0;
    int j;
    double latitude = 0, longitude = 0;
    int metroarea_combo = 0;
    int bytes_read = 0;
    if (seek_record == gi->databaseSegments[0]) {
        return NULL;
    }

    record = malloc(sizeof(GeoIPRecord));
    memset(record, 0, sizeof(GeoIPRecord));
    record->charset = gi->charset;

    record_pointer = seek_record +
                     (2 * gi->record_length - 1) * gi->databaseSegments[0];

    if (gi->cache == NULL) {
        begin_record_buf = record_buf = malloc(
                               sizeof(char) * FULL_RECORD_LENGTH);
        bytes_read = pread(fileno(
                               gi->GeoIPDatabase), record_buf,
                           FULL_RECORD_LENGTH, record_pointer);
        if (bytes_read == 0) {
            /* eof or other error */
            free(begin_record_buf);
            free(record);
            return NULL;
        }
    }else {
        record_buf = gi->cache + (long)record_pointer;
    }

    /* get country */
    record->continent_code = (char *)GeoIP_country_continent[record_buf[0]];
    record->country_code = (char *)GeoIP_country_code[record_buf[0]];
    record->country_code3 = (char *)GeoIP_country_code3[record_buf[0]];
    record->country_name = (char *)GeoIP_country_name_by_id(gi, record_buf[0]);
    record_buf++;

    /* get region */
    while (record_buf[str_length] != '\0') {
        str_length++;
    }
    if (str_length > 0) {
        record->region = malloc(str_length + 1);
        strncpy(record->region, (char *)record_buf, str_length + 1);
    }
    record_buf += str_length + 1;
    str_length = 0;

    /* get city */
    while (record_buf[str_length] != '\0') {
        str_length++;
    }
    if (str_length > 0) {
        if (gi->charset == GEOIP_CHARSET_UTF8) {
            record->city = _GeoIP_iso_8859_1__utf8((const char *)record_buf);
        }else {
            record->city = malloc(str_length + 1);
            strncpy(record->city, (const char *)record_buf, str_length + 1);
        }
    }
    record_buf += (str_length + 1);
    str_length = 0;

    /* get postal code */
    while (record_buf[str_length] != '\0') {
        str_length++;
    }
    if (str_length > 0) {
        record->postal_code = malloc(str_length + 1);
        strncpy(record->postal_code, (char *)record_buf, str_length + 1);
    }
    record_buf += (str_length + 1);

    /* get latitude */
    for (j = 0; j < 3; ++j) {
        latitude += (record_buf[j] << (j * 8));
    }
    record->latitude = latitude / 10000 - 180;
    record_buf += 3;

    /* get longitude */
    for (j = 0; j < 3; ++j) {
        longitude += (record_buf[j] << (j * 8));
    }
    record->longitude = longitude / 10000 - 180;

    /*
     * get area code and metro code for post April 2002 databases and for US
     * locations
     */
    if (GEOIP_CITY_EDITION_REV1 == gi->databaseType) {
        if (!strcmp(record->country_code, "US")) {
            record_buf += 3;
            for (j = 0; j < 3; ++j) {
                metroarea_combo += (record_buf[j] << (j * 8));
            }
            record->metro_code = metroarea_combo / 1000;
            record->area_code = metroarea_combo % 1000;
        }
    }

    if (gi->cache == NULL) {
        free(begin_record_buf);
    }

    /* Used for GeoIP_next_record */
    if (next_record_ptr != NULL) {
        *next_record_ptr = seek_record + record_buf - begin_record_buf + 3;
    }

    return record;
}

static
GeoIPRecord *
_get_record_gl(GeoIP * gi, unsigned long ipnum, GeoIPLookup * gl)
{
    unsigned int seek_record;
    GeoIPRecord * r;
    if (gi->databaseType != GEOIP_CITY_EDITION_REV0
        && gi->databaseType != GEOIP_CITY_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               GeoIPDBDescription[(int)gi->databaseType],
               GeoIPDBDescription[GEOIP_CITY_EDITION_REV1]);
        return NULL;
    }

    seek_record = _GeoIP_seek_record_gl(gi, ipnum, gl);
    r = _extract_record(gi, seek_record, NULL);
    if (r) {
        r->netmask = gl->netmask;
    }
    return r;
}

static
GeoIPRecord *
_get_record(GeoIP * gi, unsigned long ipnum)
{
    GeoIPLookup gl;
    return _get_record_gl(gi, ipnum, &gl);
}

static
GeoIPRecord *
_get_record_v6_gl(GeoIP * gi, geoipv6_t ipnum, GeoIPLookup * gl)
{
    GeoIPRecord * r;
    unsigned int seek_record;
    if (gi->databaseType != GEOIP_CITY_EDITION_REV0_V6 &&
        gi->databaseType != GEOIP_CITY_EDITION_REV1_V6) {
        printf("Invalid database type %s, expected %s\n",
               GeoIPDBDescription[(int)gi->databaseType],
               GeoIPDBDescription[GEOIP_CITY_EDITION_REV1_V6]);
        return NULL;
    }

    seek_record = _GeoIP_seek_record_v6_gl(gi, ipnum, gl);
    r = _extract_record(gi, seek_record, NULL);
    if (r) {
        r->netmask = gl->netmask;
    }
    return r;
}

static
GeoIPRecord *
_get_record_v6(GeoIP * gi, geoipv6_t ipnum)
{
    GeoIPLookup gl;
    return _get_record_v6_gl(gi, ipnum, &gl);
}

GeoIPRecord *
GeoIP_record_by_ipnum(GeoIP * gi, unsigned long ipnum)
{
    return _get_record(gi, ipnum);
}

GeoIPRecord *
GeoIP_record_by_ipnum_v6(GeoIP * gi, geoipv6_t ipnum)
{
    return _get_record_v6(gi, ipnum);
}

GeoIPRecord *
GeoIP_record_by_addr(GeoIP * gi, const char *addr)
{
    unsigned long ipnum;
    GeoIPLookup gl;
    if (addr == NULL) {
        return 0;
    }
    ipnum = GeoIP_addr_to_num(addr);
    return _get_record_gl(gi, ipnum, &gl);
}

GeoIPRecord *
GeoIP_record_by_addr_v6(GeoIP * gi, const char *addr)
{
    geoipv6_t ipnum;
    if (addr == NULL) {
        return 0;
    }
    ipnum = _GeoIP_addr_to_num_v6(addr);
    return _get_record_v6(gi, ipnum);
}

GeoIPRecord *
GeoIP_record_by_name(GeoIP * gi, const char *name)
{
    unsigned long ipnum;
    if (name == NULL) {
        return 0;
    }
    ipnum = _GeoIP_lookupaddress(name);
    return _get_record(gi, ipnum);
}

GeoIPRecord *
GeoIP_record_by_name_v6(GeoIP * gi, const char *name)
{
    geoipv6_t ipnum;
    if (name == NULL) {
        return 0;
    }
    ipnum = _GeoIP_lookupaddress_v6(name);
    return _get_record_v6(gi, ipnum);
}

int
GeoIP_record_id_by_addr(GeoIP * gi, const char *addr)
{
    unsigned long ipnum;
    if (gi->databaseType != GEOIP_CITY_EDITION_REV0 &&
        gi->databaseType != GEOIP_CITY_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               GeoIPDBDescription[(int)gi->databaseType],
               GeoIPDBDescription[GEOIP_CITY_EDITION_REV1]);
        return 0;
    }
    if (addr == NULL) {
        return 0;
    }
    ipnum = GeoIP_addr_to_num(addr);
    return _GeoIP_seek_record(gi, ipnum);
}

int
GeoIP_record_id_by_addr_v6(GeoIP * gi, const char *addr)
{
    geoipv6_t ipnum;
    if (gi->databaseType != GEOIP_CITY_EDITION_REV0_V6 &&
        gi->databaseType != GEOIP_CITY_EDITION_REV1_V6) {
        printf("Invalid database type %s, expected %s\n",
               GeoIPDBDescription[(int)gi->databaseType],
               GeoIPDBDescription[GEOIP_CITY_EDITION_REV1]);
        return 0;
    }
    if (addr == NULL) {
        return 0;
    }
    ipnum = _GeoIP_addr_to_num_v6(addr);
    return _GeoIP_seek_record_v6(gi, ipnum);
}

int
GeoIP_init_record_iter(GeoIP * gi)
{
    return gi->databaseSegments[0] + 1;
}

int
GeoIP_next_record(GeoIP * gi, GeoIPRecord ** gir, int *record_iter)
{
    if (gi->cache != NULL) {
        printf("GeoIP_next_record not supported in memory cache mode\n");
        return 1;
    }
    *gir = _extract_record(gi, *record_iter, record_iter);
    return 0;
}

void
GeoIPRecord_delete(GeoIPRecord * gir)
{
    free(gir->region);
    free(gir->city);
    free(gir->postal_code);
    free(gir);
}
