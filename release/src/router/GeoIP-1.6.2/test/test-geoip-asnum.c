/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/* test-geoip-asnum.c
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

static const char *_mk_NA(const char *p)
{
    return p ? p : "N/A";
}

int main(int argc, char *argv[])
{
    FILE *f;
    GeoIP *gi;
    char *org;
    int generate = 0;
    char host[50];

    if (argc == 2) {
        if (!strcmp(argv[1], "gen")) {
            generate = 1;
        }
    }

    gi = GeoIP_open("../data/GeoIPASNum.dat", GEOIP_STANDARD);

    if (gi == NULL) {
        fprintf(stderr, "Error opening database\n");
        exit(1);
    }

    f = fopen("asnum_test.txt", "r");

    if (f == NULL) {
        fprintf(stderr, "Error opening asnum_test.txt\n");
        exit(1);
    }

    while (fscanf(f, "%s", host) != EOF) {
        org = GeoIP_org_by_name(gi, (const char *)host);

        if (org != NULL) {
            printf("%s\t%s\n", host, _mk_NA(org));
        }
    }

    GeoIP_delete(gi);
    fclose(f);
    return 0;
}
