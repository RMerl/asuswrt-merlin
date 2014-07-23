/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/* test-geoip-region.c
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
#include <sys/types.h>          /* For uint32_t */
#ifdef HAVE_STDINT_H
#include <stdint.h>             /* For uint32_t */
#endif
#if !defined(_WIN32)
#include <netdb.h>              /* For gethostbyname */
#include <netinet/in.h>         /* For ntohl */
#else
#include <windows.h>
#include <winsock.h>
#endif
#include <assert.h>

unsigned long inetaddr(const char *name)
{
    struct hostent *host;
    struct in_addr inaddr;

    host = gethostbyname(name);
    assert(host);
    inaddr.s_addr = *((uint32_t *)host->h_addr_list[0]);
    return inaddr.s_addr;
}

static const char *_mk_NA(const char *p)
{
    return p ? p : "N/A";
}

int main()
{
    GeoIP *gi;
    GeoIPRegion *gir, giRegion;

    FILE *f;
    char ipAddress[30];
    char expectedCountry[3];
    char expectedCountry3[4];
    const char *time_zone;

    gi = GeoIP_open("../data/GeoIPRegion.dat", GEOIP_MEMORY_CACHE);

    if (gi == NULL) {
        fprintf(stderr, "Error opening database\n");
        exit(1);
    }

    f = fopen("region_test.txt", "r");

    if (f == NULL) {
        fprintf(stderr, "Error opening region_test.txt\n");
        exit(1);
    }

    gir = GeoIP_region_by_addr(gi, "10.0.0.0");
    if (gir != NULL) {
        printf("lookup of private IP address: country = %s, region = %s\n",
               gir->country_code, gir->region);
    }

    while (fscanf(f, "%s%s%s", ipAddress, expectedCountry, expectedCountry3) !=
           EOF) {
        printf("ip = %s\n", ipAddress);

        gir = GeoIP_region_by_name(gi, ipAddress);
        if (gir != NULL) {
            time_zone =
                GeoIP_time_zone_by_country_and_region(gir->country_code,
                                                      gir->region);
            printf("%s, %s, %s, %s\n",
                   gir->country_code,
                   (!gir->region[0]) ? "N/A" : gir->region,
                   _mk_NA(GeoIP_region_name_by_code
                              (gir->country_code,
                              gir->region)), _mk_NA(time_zone));
        } else {
            printf("NULL!\n");
        }

        GeoIP_assign_region_by_inetaddr(gi, inetaddr(ipAddress), &giRegion);
        if (gir != NULL) {
            assert(giRegion.country_code[0]);
            assert(!strcmp(gir->country_code, giRegion.country_code));
            if (gir->region[0]) {
                assert(giRegion.region[0]);
                assert(!strcmp(gir->region, giRegion.region));
            } else {
                assert(!giRegion.region[0]);
            }
        } else {
            assert(!giRegion.country_code[0]);
        }

        if (gir != NULL) {
            GeoIPRegion_delete(gir);
        }
    }

    GeoIP_delete(gi);
    fclose(f);
    return 0;
}
