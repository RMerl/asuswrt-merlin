/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/* geoiplookup.c
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

#include "GeoIP.h"
#include "GeoIPCity.h"
#include "GeoIP_internal.h"

#if defined(_WIN32)
# ifndef uint32_t
typedef unsigned int uint32_t;
# endif
#endif

void geoiplookup(GeoIP * gi, char *hostname, int i);

void usage()
{
    fprintf(
        stderr,
        "Usage: geoiplookup [-h] [-?] [-d custom_dir] [-f custom_file] [-v] [-i] [-l] <ipaddress|hostname>\n");
}

/* extra info used in _say_range_ip */
int info_flag = 0;

int main(int argc, char *argv[])
{
    char * hostname = NULL;
    char * db_info;
    GeoIP * gi;
    int i;
    char *custom_directory = NULL;
    char *custom_file = NULL;
    int version_flag = 0;
    int charset = GEOIP_CHARSET_UTF8;

    if (argc < 2) {
        usage();
        exit(1);
    }
    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            version_flag = 1;
        } else if (strcmp(argv[i], "-l") == 0) {
            charset = GEOIP_CHARSET_ISO_8859_1;
        } else if (strcmp(argv[i], "-i") == 0) {
            info_flag = 1;
        } else if (( strcmp(argv[i], "-?" ) == 0 )
                   || ( strcmp(argv[i], "-h" ) == 0 )) {
            usage();
            exit(0);
        } else if (strcmp(argv[i], "-f") == 0) {
            if ((i + 1) < argc) {
                i++;
                custom_file = argv[i];
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            if ((i + 1) < argc) {
                i++;
                custom_directory = argv[i];
            }
        } else {
            hostname = argv[i];
        }
        i++;
    }
    if (hostname == NULL) {
        usage();
        exit(1);
    }

    if (custom_directory != NULL) {
        GeoIP_setup_custom_directory(custom_directory);
    }
    _GeoIP_setup_dbfilename();

    if (custom_file != NULL) {
        gi = GeoIP_open(custom_file, GEOIP_STANDARD);

        if (NULL == gi) {
            printf("%s not available, skipping...\n", custom_file);
        } else {
            gi->charset = charset;
            i = GeoIP_database_edition(gi);
            if (version_flag == 1) {
                db_info = GeoIP_database_info(gi);
                printf("%s: %s\n", GeoIPDBDescription[i],
                       db_info == NULL ? "" : db_info );
                free(db_info);
            } else {
                geoiplookup(gi, hostname, i);
            }
        }
        GeoIP_delete(gi);
    } else {
        /* iterate through different database types */
        for (i = 0; i < NUM_DB_TYPES; ++i) {
            if (GeoIP_db_avail(i)) {
                gi = GeoIP_open_type(i, GEOIP_STANDARD);
                if (NULL == gi) {
                    /* Ignore these errors. It's possible
                     * to use the same database name for
                     * different databases.
                     */
                    ;
                } else {
                    gi->charset = charset;
                    if (version_flag == 1) {
                        db_info = GeoIP_database_info(gi);
                        printf("%s: %s\n", GeoIPDBDescription[i],
                               db_info == NULL ? "" : db_info );
                        free(db_info);
                    } else {
                        geoiplookup(gi, hostname, i);
                    }
                }
                GeoIP_delete(gi);
            }
        }
    }
    return 0;
}

static const char * _mk_NA( const char * p )
{
    return p ? p : "N/A";
}

static unsigned long
__addr_to_num(const char *addr)
{
    unsigned int c, octet, t;
    unsigned long ipnum;
    int i = 3;

    octet = ipnum = 0;
    while ((c = *addr++)) {
        if (c == '.') {
            if (octet > 255) {
                return 0;
            }
            ipnum <<= 8;
            ipnum += octet;
            i--;
            octet = 0;
        } else {
            t = octet;
            octet <<= 3;
            octet += t;
            octet += t;
            c -= '0';
            if (c > 9) {
                return 0;
            }
            octet += c;
        }
    }
    if ((octet > 255) || (i != 0)) {
        return 0;
    }
    ipnum <<= 8;
    return ipnum + octet;
}



/* ptr must be a memory area with at least 16 bytes */
static char *__num_to_addr_r(unsigned long ipnum, char * ptr)
{
    char *cur_str;
    int octet[4];
    int num_chars_written, i;

    cur_str = ptr;

    for (i = 0; i < 4; i++) {
        octet[3 - i] = ipnum % 256;
        ipnum >>= 8;
    }

    for (i = 0; i < 4; i++) {
        num_chars_written = sprintf(cur_str, "%d", octet[i]);
        cur_str += num_chars_written;

        if (i < 3) {
            cur_str[0] = '.';
            cur_str++;
        }
    }

    return ptr;
}

void _say_range_by_ip(GeoIP * gi, uint32_t ipnum )
{
    unsigned long last_nm, mask, low, hi;
    char ipaddr[16];
    char tmp[16];
    char ** range;

    if (info_flag == 0) {
        return; /* noop unless extra information is requested */

    }
    range = GeoIP_range_by_ip( gi, __num_to_addr_r( ipnum, ipaddr ) );
    if (range == NULL) {
        return;
    }

    printf( "  ipaddr: %s\n", ipaddr );

    printf( "  range_by_ip:  %s - %s\n", range[0], range[1] );
    last_nm = GeoIP_last_netmask(gi);
    mask = 0xffffffff << ( 32 - last_nm );
    low = ipnum & mask;
    hi = low + ( 0xffffffff & ~mask );
    printf( "  network:      %s - %s ::%ld\n",
            __num_to_addr_r( low, ipaddr ),
            __num_to_addr_r( hi, tmp ),
            last_nm
            );
    printf( "  ipnum: %u\n", ipnum );
    printf( "  range_by_num: %lu - %lu\n", __addr_to_num(
                range[0]), __addr_to_num(range[1]) );
    printf( "  network num:  %lu - %lu ::%lu\n", low, hi, last_nm );

    GeoIP_range_by_ip_delete(range);
}

void
geoiplookup(GeoIP * gi, char *hostname, int i)
{
    const char *country_code;
    const char *country_name;
    const char *domain_name;
    const char *asnum_name;
    int netspeed;
    int country_id;
    GeoIPRegion *region;
    GeoIPRecord *gir;
    const char *org;
    uint32_t ipnum;

    ipnum = _GeoIP_lookupaddress(hostname);
    if (ipnum == 0) {
        printf("%s: can't resolve hostname ( %s )\n", GeoIPDBDescription[i],
               hostname);

    }else {

        if (GEOIP_DOMAIN_EDITION == i) {
            domain_name = GeoIP_name_by_ipnum(gi, ipnum);
            if (domain_name == NULL) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s\n", GeoIPDBDescription[i], domain_name);
                _say_range_by_ip(gi, ipnum);
            }
        }else if (GEOIP_LOCATIONA_EDITION == i ||
                  GEOIP_ACCURACYRADIUS_EDITION == i
                  || GEOIP_ASNUM_EDITION == i || GEOIP_USERTYPE_EDITION == i
                  || GEOIP_REGISTRAR_EDITION == i ||
                  GEOIP_NETSPEED_EDITION_REV1 == i
                  || GEOIP_COUNTRYCONF_EDITION == i ||
                  GEOIP_CITYCONF_EDITION == i
                  || GEOIP_REGIONCONF_EDITION == i ||
                  GEOIP_POSTALCONF_EDITION == i) {
            asnum_name = GeoIP_name_by_ipnum(gi, ipnum);
            if (asnum_name == NULL) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s\n", GeoIPDBDescription[i], asnum_name);
                _say_range_by_ip(gi, ipnum);
            }
        }else if (GEOIP_COUNTRY_EDITION == i) {
            country_id = GeoIP_id_by_ipnum(gi, ipnum);
            country_code = GeoIP_country_code[country_id];
            country_name = GeoIP_country_name[country_id];
            if (country_id == 0) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s, %s\n", GeoIPDBDescription[i], country_code,
                       country_name);
                _say_range_by_ip(gi, ipnum);
            }
        }else if (GEOIP_REGION_EDITION_REV0 == i ||
                  GEOIP_REGION_EDITION_REV1 == i) {
            region = GeoIP_region_by_ipnum(gi, ipnum);
            if (NULL == region || region->country_code[0] == '\0') {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s, %s\n", GeoIPDBDescription[i],
                       region->country_code,
                       region->region);
                _say_range_by_ip(gi, ipnum);
            }
            if (region) {
                GeoIPRegion_delete(region);
            }
        }else if (GEOIP_CITY_EDITION_REV0 == i) {
            gir = GeoIP_record_by_ipnum(gi, ipnum);
            if (NULL == gir) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s, %s, %s, %s, %s, %f, %f\n",
                       GeoIPDBDescription[i], gir->country_code, _mk_NA(
                           gir->region),
                       _mk_NA(GeoIP_region_name_by_code(gir->country_code,
                                                        gir->region)),
                       _mk_NA(gir->city), _mk_NA(
                           gir->postal_code), gir->latitude, gir->longitude);
                _say_range_by_ip(gi, ipnum);
                GeoIPRecord_delete(gir);
            }
        }else if (GEOIP_CITY_EDITION_REV1 == i) {
            gir = GeoIP_record_by_ipnum(gi, ipnum);
            if (NULL == gir) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s, %s, %s, %s, %s, %f, %f, %d, %d\n",
                       GeoIPDBDescription[i], gir->country_code, _mk_NA(
                           gir->region),
                       _mk_NA(GeoIP_region_name_by_code(gir->country_code,
                                                        gir->region)),
                       _mk_NA(gir->city), _mk_NA(
                           gir->postal_code),
                       gir->latitude, gir->longitude, gir->metro_code,
                       gir->area_code);
                _say_range_by_ip(gi, ipnum);
                GeoIPRecord_delete(gir);
            }
        }else if (GEOIP_ORG_EDITION == i || GEOIP_ISP_EDITION == i) {
            org = GeoIP_org_by_ipnum(gi, ipnum);
            if (org == NULL) {
                printf("%s: IP Address not found\n", GeoIPDBDescription[i]);
            }else {
                printf("%s: %s\n", GeoIPDBDescription[i], org);
                _say_range_by_ip(gi, ipnum);
            }
        }else if (GEOIP_NETSPEED_EDITION == i) {
            netspeed = GeoIP_id_by_ipnum(gi, ipnum);
            if (netspeed == GEOIP_UNKNOWN_SPEED) {
                printf("%s: Unknown\n", GeoIPDBDescription[i]);
            }else if (netspeed == GEOIP_DIALUP_SPEED) {
                printf("%s: Dialup\n", GeoIPDBDescription[i]);
            }else if (netspeed == GEOIP_CABLEDSL_SPEED) {
                printf("%s: Cable/DSL\n", GeoIPDBDescription[i]);
            }else if (netspeed == GEOIP_CORPORATE_SPEED) {
                printf("%s: Corporate\n", GeoIPDBDescription[i]);
            }
            _say_range_by_ip(gi, ipnum);
        }else {

            /*
             * Silent ignore IPv6 databases. Otherwise we get annoying
             * messages whenever we have a mixed environment IPv4 and
             *  IPv6
             */

            /*
             * printf("Can not handle database type -- try geoiplookup6\n");
             */
            ;
        }
    }
}
