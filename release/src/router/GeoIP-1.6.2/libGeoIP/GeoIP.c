/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/* GeoIP.c
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
#include "GeoIP_internal.h"

static geoipv6_t IPV6_NULL;

#if !defined(_WIN32)
#include <unistd.h>
#include <netdb.h>
#include <sys/mman.h>
#endif /* !defined(_WIN32) */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h> /* for fstat */
#include <sys/stat.h>  /* for fstat */

#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h> /* for gettimeofday */
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>     /* For uint32_t */
#endif

#if defined(_WIN32)
#include "pread.h"
#endif

#ifdef _UNUSED
#elif defined(__GNUC__)
#define _UNUSED __attribute__ ((unused))
#else
#define _UNUSED
#endif

#ifndef        INADDR_NONE
#define        INADDR_NONE     -1
#endif

#define COUNTRY_BEGIN 16776960
#define LARGE_COUNTRY_BEGIN 16515072
#define STATE_BEGIN_REV0 16700000
#define STATE_BEGIN_REV1 16000000
#define STRUCTURE_INFO_MAX_SIZE 20
#define DATABASE_INFO_MAX_SIZE 100
#define MAX_ORG_RECORD_LENGTH 300
#define US_OFFSET 1
#define CANADA_OFFSET 677
#define WORLD_OFFSET 1353
#define FIPS_RANGE 360

#define CHECK_ERR(err, msg)                              \
    {                                                    \
        if (err != Z_OK) {                               \
            fprintf(stderr, "%s error: %d\n", msg, err); \
            exit(1);                                     \
        }                                                \
    }

#ifndef HAVE_PREAD
#define pread(fd, buf, count, offset)           \
    (                                           \
        lseek(fd, offset, SEEK_SET) == offset ? \
        read(fd, buf, count) :                  \
        -1                                      \
    )
#endif /* HAVE_PREAD */



const char GeoIP_country_code[256][3] =
{ "--", "AP", "EU", "AD", "AE", "AF",
  "AG", "AI", "AL", "AM", "CW",
  "AO", "AQ", "AR", "AS", "AT", "AU",
  "AW", "AZ", "BA", "BB",
  "BD", "BE", "BF", "BG", "BH", "BI",
  "BJ", "BM", "BN", "BO",
  "BR", "BS", "BT", "BV", "BW", "BY",
  "BZ", "CA", "CC", "CD",
  "CF", "CG", "CH", "CI", "CK", "CL",
  "CM", "CN", "CO", "CR",
  "CU", "CV", "CX", "CY", "CZ", "DE",
  "DJ", "DK", "DM", "DO",
  "DZ", "EC", "EE", "EG", "EH", "ER",
  "ES", "ET", "FI", "FJ",
  "FK", "FM", "FO", "FR", "SX", "GA",
  "GB", "GD", "GE", "GF",
  "GH", "GI", "GL", "GM", "GN", "GP",
  "GQ", "GR", "GS", "GT",
  "GU", "GW", "GY", "HK", "HM", "HN",
  "HR", "HT", "HU", "ID",
  "IE", "IL", "IN", "IO", "IQ", "IR",
  "IS", "IT", "JM", "JO",
  "JP", "KE", "KG", "KH", "KI", "KM",
  "KN", "KP", "KR", "KW",
  "KY", "KZ", "LA", "LB", "LC", "LI",
  "LK", "LR", "LS", "LT",
  "LU", "LV", "LY", "MA", "MC", "MD",
  "MG", "MH", "MK", "ML",
  "MM", "MN", "MO", "MP", "MQ", "MR",
  "MS", "MT", "MU", "MV",
  "MW", "MX", "MY", "MZ", "NA", "NC",
  "NE", "NF", "NG", "NI",
  "NL", "NO", "NP", "NR", "NU", "NZ",
  "OM", "PA", "PE", "PF",
  "PG", "PH", "PK", "PL", "PM", "PN",
  "PR", "PS", "PT", "PW",
  "PY", "QA", "RE", "RO", "RU", "RW",
  "SA", "SB", "SC", "SD",
  "SE", "SG", "SH", "SI", "SJ", "SK",
  "SL", "SM", "SN", "SO",
  "SR", "ST", "SV", "SY", "SZ", "TC",
  "TD", "TF", "TG", "TH",
  "TJ", "TK", "TM", "TN", "TO", "TL",
  "TR", "TT", "TV", "TW",
  "TZ", "UA", "UG", "UM", "US", "UY",
  "UZ", "VA", "VC", "VE",
  "VG", "VI", "VN", "VU", "WF", "WS",
  "YE", "YT", "RS", "ZA",
  "ZM", "ME", "ZW", "A1", "A2", "O1",
  "AX", "GG", "IM", "JE",
  "BL", "MF", "BQ", "SS", "O1" };

static const unsigned num_GeoIP_countries =
    (unsigned)(sizeof(GeoIP_country_code) / sizeof(GeoIP_country_code[0]));

const char GeoIP_country_code3[256][4] =
{ "--",  "AP",  "EU",  "AND",
  "ARE",
  "AFG", "ATG", "AIA", "ALB","ARM",  "CUW",
  "AGO", "ATA", "ARG", "ASM","AUT",
  "AUS", "ABW", "AZE", "BIH","BRB",
  "BGD", "BEL", "BFA", "BGR","BHR",
  "BDI", "BEN", "BMU", "BRN","BOL",
  "BRA", "BHS", "BTN", "BVT","BWA",
  "BLR", "BLZ", "CAN", "CCK","COD",
  "CAF", "COG", "CHE", "CIV","COK",
  "CHL", "CMR", "CHN", "COL","CRI",
  "CUB", "CPV", "CXR", "CYP","CZE",
  "DEU", "DJI", "DNK", "DMA","DOM",
  "DZA", "ECU", "EST", "EGY","ESH",
  "ERI", "ESP", "ETH", "FIN","FJI",
  "FLK", "FSM", "FRO", "FRA","SXM",
  "GAB", "GBR", "GRD", "GEO","GUF",
  "GHA", "GIB", "GRL", "GMB","GIN",
  "GLP", "GNQ", "GRC", "SGS","GTM",
  "GUM", "GNB", "GUY", "HKG","HMD",
  "HND", "HRV", "HTI", "HUN","IDN",
  "IRL", "ISR", "IND", "IOT","IRQ",
  "IRN", "ISL", "ITA", "JAM","JOR",
  "JPN", "KEN", "KGZ", "KHM","KIR",
  "COM", "KNA", "PRK", "KOR","KWT",
  "CYM", "KAZ", "LAO", "LBN","LCA",
  "LIE", "LKA", "LBR", "LSO","LTU",
  "LUX", "LVA", "LBY", "MAR","MCO",
  "MDA", "MDG", "MHL", "MKD","MLI",
  "MMR", "MNG", "MAC", "MNP","MTQ",
  "MRT", "MSR", "MLT", "MUS","MDV",
  "MWI", "MEX", "MYS", "MOZ","NAM",
  "NCL", "NER", "NFK", "NGA","NIC",
  "NLD", "NOR", "NPL", "NRU","NIU",
  "NZL", "OMN", "PAN", "PER","PYF",
  "PNG", "PHL", "PAK", "POL","SPM",
  "PCN", "PRI", "PSE", "PRT","PLW",
  "PRY", "QAT", "REU", "ROU","RUS",
  "RWA", "SAU", "SLB", "SYC","SDN",
  "SWE", "SGP", "SHN", "SVN","SJM",
  "SVK", "SLE", "SMR", "SEN","SOM",
  "SUR", "STP", "SLV", "SYR","SWZ",
  "TCA", "TCD", "ATF", "TGO","THA",
  "TJK", "TKL", "TKM", "TUN","TON",
  "TLS", "TUR", "TTO", "TUV","TWN",
  "TZA", "UKR", "UGA", "UMI","USA",
  "URY", "UZB", "VAT", "VCT","VEN",
  "VGB", "VIR", "VNM", "VUT","WLF",
  "WSM", "YEM", "MYT", "SRB","ZAF",
  "ZMB", "MNE", "ZWE", "A1", "A2",
  "O1",  "ALA", "GGY", "IMN","JEY",
  "BLM", "MAF", "BES", "SSD","O1" };

const char * GeoIP_utf8_country_name[256] =
{ "N/A",
  "Asia/Pacific Region",                         "Europe",
  "Andorra",                                     "United Arab Emirates",
  "Afghanistan",                                 "Antigua and Barbuda",
  "Anguilla",
  "Albania",                                     "Armenia",
  "Cura" "\xc3\xa7" "ao",
  "Angola",
  "Antarctica",                                  "Argentina",
  "American Samoa",                              "Austria",
  "Australia",                                   "Aruba",
  "Azerbaijan",
  "Bosnia and Herzegovina",                      "Barbados",
  "Bangladesh",
  "Belgium",                                     "Burkina Faso",
  "Bulgaria",                                    "Bahrain",
  "Burundi",                                     "Benin",
  "Bermuda",
  "Brunei Darussalam",                           "Bolivia",
  "Brazil",
  "Bahamas",                                     "Bhutan",
  "Bouvet Island",                               "Botswana",
  "Belarus",                                     "Belize",
  "Canada",
  "Cocos (Keeling) Islands",
  "Congo, The Democratic Republic of the",
  "Central African Republic",
  "Congo",                                       "Switzerland",
  "Cote D'Ivoire",                               "Cook Islands",
  "Chile",                                       "Cameroon",
  "China",
  "Colombia",                                    "Costa Rica",
  "Cuba",
  "Cape Verde",                                  "Christmas Island",
  "Cyprus",                                      "Czech Republic",
  "Germany",                                     "Djibouti",
  "Denmark",
  "Dominica",                                    "Dominican Republic",
  "Algeria",
  "Ecuador",                                     "Estonia",
  "Egypt",                                       "Western Sahara",
  "Eritrea",                                     "Spain",
  "Ethiopia",
  "Finland",                                     "Fiji",
  "Falkland Islands (Malvinas)",
  "Micronesia, Federated States of",             "Faroe Islands",
  "France",                                      "Sint Maarten (Dutch part)",
  "Gabon",                                       "United Kingdom",
  "Grenada",
  "Georgia",                                     "French Guiana",
  "Ghana",
  "Gibraltar",                                   "Greenland",
  "Gambia",                                      "Guinea",
  "Guadeloupe",                                  "Equatorial Guinea",
  "Greece",
  "South Georgia and the South Sandwich Islands","Guatemala",
  "Guam",
  "Guinea-Bissau",                               "Guyana",
  "Hong Kong",
  "Heard Island and McDonald Islands",
  "Honduras",                                    "Croatia",
  "Haiti",
  "Hungary",                                     "Indonesia",
  "Ireland",
  "Israel",                                      "India",
  "British Indian Ocean Territory",              "Iraq",
  "Iran, Islamic Republic of",                   "Iceland",
  "Italy",
  "Jamaica",                                     "Jordan",
  "Japan",
  "Kenya",                                       "Kyrgyzstan",
  "Cambodia",                                    "Kiribati",
  "Comoros",                                     "Saint Kitts and Nevis",
  "Korea, Democratic People's Republic of",      "Korea, Republic of",
  "Kuwait",
  "Cayman Islands",
  "Kazakhstan",
  "Lao People's Democratic Republic",
  "Lebanon",                                     "Saint Lucia",
  "Liechtenstein",                               "Sri Lanka",
  "Liberia",
  "Lesotho",                                     "Lithuania",
  "Luxembourg",
  "Latvia",                                      "Libya",
  "Morocco",                                     "Monaco",
  "Moldova, Republic of",                        "Madagascar",
  "Marshall Islands",                            "Macedonia",
  "Mali",
  "Myanmar",
  "Mongolia",                                    "Macau",
  "Northern Mariana Islands",                    "Martinique",
  "Mauritania",                                  "Montserrat",
  "Malta",
  "Mauritius",                                   "Maldives",
  "Malawi",
  "Mexico",                                      "Malaysia",
  "Mozambique",                                  "Namibia",
  "New Caledonia",                               "Niger",
  "Norfolk Island",
  "Nigeria",                                     "Nicaragua",
  "Netherlands",
  "Norway",                                      "Nepal",
  "Nauru",                                       "Niue",
  "New Zealand",                                 "Oman",
  "Panama",
  "Peru",                                        "French Polynesia",
  "Papua New Guinea",
  "Philippines",                                 "Pakistan",
  "Poland",                                      "Saint Pierre and Miquelon",
  "Pitcairn Islands",                            "Puerto Rico",
  "Palestinian Territory",                       "Portugal",
  "Palau",
  "Paraguay",
  "Qatar",                                       "Reunion",
  "Romania",                                     "Russian Federation",
  "Rwanda",                                      "Saudi Arabia",
  "Solomon Islands",                             "Seychelles",
  "Sudan",
  "Sweden",
  "Singapore",                                   "Saint Helena",
  "Slovenia",                                    "Svalbard and Jan Mayen",
  "Slovakia",                                    "Sierra Leone",
  "San Marino",
  "Senegal",                                     "Somalia",
  "Suriname",
  "Sao Tome and Principe",
  "El Salvador",                                 "Syrian Arab Republic",
  "Swaziland",                                   "Turks and Caicos Islands",
  "Chad",                                        "French Southern Territories",
  "Togo",
  "Thailand",
  "Tajikistan",
  "Tokelau",                                     "Turkmenistan",
  "Tunisia",                                     "Tonga",
  "Timor-Leste",                                 "Turkey",
  "Trinidad and Tobago",                         "Tuvalu",
  "Taiwan",
  "Tanzania, United Republic of",
  "Ukraine",                                     "Uganda",
  "United States Minor Outlying Islands",        "United States",
  "Uruguay",                                     "Uzbekistan",
  "Holy See (Vatican City State)",
  "Saint Vincent and the Grenadines",
  "Venezuela",
  "Virgin Islands, British",
  "Virgin Islands, U.S.",                        "Vietnam",
  "Vanuatu",                                     "Wallis and Futuna",
  "Samoa",                                       "Yemen",
  "Mayotte",
  "Serbia",                                      "South Africa",
  "Zambia",
  "Montenegro",                                  "Zimbabwe",
  "Anonymous Proxy",                             "Satellite Provider",
  "Other",                                       "Aland Islands",
  "Guernsey",
  "Isle of Man",                                 "Jersey",
  "Saint Barthelemy",
  "Saint Martin",
  "Bonaire, Saint Eustatius and Saba",
  "South Sudan",                                 "Other" };

const char * GeoIP_country_name[256] =
{ "N/A",
  "Asia/Pacific Region",                         "Europe",
  "Andorra",                                     "United Arab Emirates",
  "Afghanistan",                                 "Antigua and Barbuda",
  "Anguilla",
  "Albania",                                     "Armenia",
  "Curacao",
  "Angola",
  "Antarctica",                                  "Argentina",
  "American Samoa",                              "Austria",
  "Australia",                                   "Aruba",
  "Azerbaijan",
  "Bosnia and Herzegovina",                      "Barbados",
  "Bangladesh",
  "Belgium",                                     "Burkina Faso",
  "Bulgaria",                                    "Bahrain",
  "Burundi",                                     "Benin",
  "Bermuda",
  "Brunei Darussalam",                           "Bolivia",
  "Brazil",
  "Bahamas",                                     "Bhutan",
  "Bouvet Island",                               "Botswana",
  "Belarus",                                     "Belize",
  "Canada",
  "Cocos (Keeling) Islands",
  "Congo, The Democratic Republic of the",
  "Central African Republic",
  "Congo",                                       "Switzerland",
  "Cote D'Ivoire",                               "Cook Islands",
  "Chile",                                       "Cameroon",
  "China",
  "Colombia",                                    "Costa Rica",
  "Cuba",
  "Cape Verde",                                  "Christmas Island",
  "Cyprus",                                      "Czech Republic",
  "Germany",                                     "Djibouti",
  "Denmark",
  "Dominica",                                    "Dominican Republic",
  "Algeria",
  "Ecuador",                                     "Estonia",
  "Egypt",                                       "Western Sahara",
  "Eritrea",                                     "Spain",
  "Ethiopia",
  "Finland",                                     "Fiji",
  "Falkland Islands (Malvinas)",
  "Micronesia, Federated States of",             "Faroe Islands",
  "France",                                      "Sint Maarten (Dutch part)",
  "Gabon",                                       "United Kingdom",
  "Grenada",
  "Georgia",                                     "French Guiana",
  "Ghana",
  "Gibraltar",                                   "Greenland",
  "Gambia",                                      "Guinea",
  "Guadeloupe",                                  "Equatorial Guinea",
  "Greece",
  "South Georgia and the South Sandwich Islands","Guatemala",
  "Guam",
  "Guinea-Bissau",                               "Guyana",
  "Hong Kong",
  "Heard Island and McDonald Islands",
  "Honduras",                                    "Croatia",
  "Haiti",
  "Hungary",                                     "Indonesia",
  "Ireland",
  "Israel",                                      "India",
  "British Indian Ocean Territory",              "Iraq",
  "Iran, Islamic Republic of",                   "Iceland",
  "Italy",
  "Jamaica",                                     "Jordan",
  "Japan",
  "Kenya",                                       "Kyrgyzstan",
  "Cambodia",                                    "Kiribati",
  "Comoros",                                     "Saint Kitts and Nevis",
  "Korea, Democratic People's Republic of",      "Korea, Republic of",
  "Kuwait",
  "Cayman Islands",
  "Kazakhstan",
  "Lao People's Democratic Republic",
  "Lebanon",                                     "Saint Lucia",
  "Liechtenstein",                               "Sri Lanka",
  "Liberia",
  "Lesotho",                                     "Lithuania",
  "Luxembourg",
  "Latvia",                                      "Libya",
  "Morocco",                                     "Monaco",
  "Moldova, Republic of",                        "Madagascar",
  "Marshall Islands",                            "Macedonia",
  "Mali",
  "Myanmar",
  "Mongolia",                                    "Macau",
  "Northern Mariana Islands",                    "Martinique",
  "Mauritania",                                  "Montserrat",
  "Malta",
  "Mauritius",                                   "Maldives",
  "Malawi",
  "Mexico",                                      "Malaysia",
  "Mozambique",                                  "Namibia",
  "New Caledonia",                               "Niger",
  "Norfolk Island",
  "Nigeria",                                     "Nicaragua",
  "Netherlands",
  "Norway",                                      "Nepal",
  "Nauru",                                       "Niue",
  "New Zealand",                                 "Oman",
  "Panama",
  "Peru",                                        "French Polynesia",
  "Papua New Guinea",
  "Philippines",                                 "Pakistan",
  "Poland",                                      "Saint Pierre and Miquelon",
  "Pitcairn Islands",                            "Puerto Rico",
  "Palestinian Territory",                       "Portugal",
  "Palau",
  "Paraguay",
  "Qatar",                                       "Reunion",
  "Romania",                                     "Russian Federation",
  "Rwanda",                                      "Saudi Arabia",
  "Solomon Islands",                             "Seychelles",
  "Sudan",
  "Sweden",
  "Singapore",                                   "Saint Helena",
  "Slovenia",                                    "Svalbard and Jan Mayen",
  "Slovakia",                                    "Sierra Leone",
  "San Marino",
  "Senegal",                                     "Somalia",
  "Suriname",
  "Sao Tome and Principe",
  "El Salvador",                                 "Syrian Arab Republic",
  "Swaziland",                                   "Turks and Caicos Islands",
  "Chad",                                        "French Southern Territories",
  "Togo",
  "Thailand",
  "Tajikistan",
  "Tokelau",                                     "Turkmenistan",
  "Tunisia",                                     "Tonga",
  "Timor-Leste",                                 "Turkey",
  "Trinidad and Tobago",                         "Tuvalu",
  "Taiwan",
  "Tanzania, United Republic of",
  "Ukraine",                                     "Uganda",
  "United States Minor Outlying Islands",        "United States",
  "Uruguay",                                     "Uzbekistan",
  "Holy See (Vatican City State)",
  "Saint Vincent and the Grenadines",
  "Venezuela",
  "Virgin Islands, British",
  "Virgin Islands, U.S.",                        "Vietnam",
  "Vanuatu",                                     "Wallis and Futuna",
  "Samoa",                                       "Yemen",
  "Mayotte",
  "Serbia",                                      "South Africa",
  "Zambia",
  "Montenegro",                                  "Zimbabwe",
  "Anonymous Proxy",                             "Satellite Provider",
  "Other",                                       "Aland Islands",
  "Guernsey",
  "Isle of Man",                                 "Jersey",
  "Saint Barthelemy",
  "Saint Martin",
  "Bonaire, Saint Eustatius and Saba",
  "South Sudan",                                 "Other" };

/* Possible continent codes are AF, AS, EU, NA, OC, SA for Africa, Asia, Europe, North America, Oceania
   and South America. */

const char GeoIP_country_continent[256][3] = {
    "--", "AS", "EU", "EU", "AS", "AS", "NA", "NA", "EU", "AS", "NA",
    "AF", "AN", "SA", "OC", "EU", "OC", "NA", "AS", "EU", "NA",
    "AS", "EU", "AF", "EU", "AS", "AF", "AF", "NA", "AS", "SA",
    "SA", "NA", "AS", "AN", "AF", "EU", "NA", "NA", "AS", "AF",
    "AF", "AF", "EU", "AF", "OC", "SA", "AF", "AS", "SA", "NA",
    "NA", "AF", "AS", "AS", "EU", "EU", "AF", "EU", "NA", "NA",
    "AF", "SA", "EU", "AF", "AF", "AF", "EU", "AF", "EU", "OC",
    "SA", "OC", "EU", "EU", "NA", "AF", "EU", "NA", "AS", "SA",
    "AF", "EU", "NA", "AF", "AF", "NA", "AF", "EU", "AN", "NA",
    "OC", "AF", "SA", "AS", "AN", "NA", "EU", "NA", "EU", "AS",
    "EU", "AS", "AS", "AS", "AS", "AS", "EU", "EU", "NA", "AS",
    "AS", "AF", "AS", "AS", "OC", "AF", "NA", "AS", "AS", "AS",
    "NA", "AS", "AS", "AS", "NA", "EU", "AS", "AF", "AF", "EU",
    "EU", "EU", "AF", "AF", "EU", "EU", "AF", "OC", "EU", "AF",
    "AS", "AS", "AS", "OC", "NA", "AF", "NA", "EU", "AF", "AS",
    "AF", "NA", "AS", "AF", "AF", "OC", "AF", "OC", "AF", "NA",
    "EU", "EU", "AS", "OC", "OC", "OC", "AS", "NA", "SA", "OC",
    "OC", "AS", "AS", "EU", "NA", "OC", "NA", "AS", "EU", "OC",
    "SA", "AS", "AF", "EU", "EU", "AF", "AS", "OC", "AF", "AF",
    "EU", "AS", "AF", "EU", "EU", "EU", "AF", "EU", "AF", "AF",
    "SA", "AF", "NA", "AS", "AF", "NA", "AF", "AN", "AF", "AS",
    "AS", "OC", "AS", "AF", "OC", "AS", "EU", "NA", "OC", "AS",
    "AF", "EU", "AF", "OC", "NA", "SA", "AS", "EU", "NA", "SA",
    "NA", "NA", "AS", "OC", "OC", "OC", "AS", "AF", "EU", "AF",
    "AF", "EU", "AF", "--", "--", "--", "EU", "EU", "EU", "EU",
    "NA", "NA", "NA", "AF", "--"
};

static const char * get_db_description(int dbtype)
{
    const char * ptr;
    if (dbtype >= NUM_DB_TYPES || dbtype < 0) {
        return "Unknown";
    }
    ptr = GeoIPDBDescription[dbtype];
    return ptr == NULL ? "Unknown" : ptr;
}

geoipv6_t _GeoIP_lookupaddress_v6(const char *host);

#if defined(_WIN32)
/* http://www.mail-archive.com/users@ipv6.org/msg02107.html */
static const char * _GeoIP_inet_ntop(int af, const void *src, char *dst,
                                     socklen_t cnt)
{
    if (af == AF_INET) {
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        memcpy(&in.sin_addr, src, sizeof(struct in_addr));
        getnameinfo((struct sockaddr *)&in, sizeof(struct
                                                   sockaddr_in), dst, cnt, NULL,
                    0,
                    NI_NUMERICHOST);
        return dst;
    }else if (af == AF_INET6) {
        struct sockaddr_in6 in;
        memset(&in, 0, sizeof(in));
        in.sin6_family = AF_INET6;
        memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
        getnameinfo((struct sockaddr *)&in, sizeof(struct
                                                   sockaddr_in6), dst, cnt,
                    NULL, 0,
                    NI_NUMERICHOST);
        return dst;
    }
    return NULL;
}

static int _GeoIP_inet_pton(int af, const char *src, void *dst)
{
    struct addrinfo hints, *res, *ressave;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = af;

    if (getaddrinfo(src, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Couldn't resolve host %s\n", src);
        return -1;
    }

    ressave = res;

    while (res) {
        memcpy(dst, res->ai_addr, res->ai_addrlen);
        res = res->ai_next;
    }

    freeaddrinfo(ressave);
    return 0;
}
#else
static int _GeoIP_inet_pton(int af, const char *src, void *dst)
{
    return inet_pton(af, src, dst);
}
static const char * _GeoIP_inet_ntop(int af, const void *src, char *dst,
                                     socklen_t cnt)
{
    return inet_ntop(af, src, dst, cnt);
}

#endif /* defined(_WIN32) */


int __GEOIP_V6_IS_NULL(geoipv6_t v6)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (v6.s6_addr[i]) {
            return 0;
        }
    }
    return 1;
}

void __GEOIP_PREPARE_TEREDO(geoipv6_t * v6)
{
    int i;
    if ((v6->s6_addr[0]) != 0x20) {
        return;
    }
    if ((v6->s6_addr[1]) != 0x01) {
        return;
    }
    if ((v6->s6_addr[2]) != 0x00) {
        return;
    }
    if ((v6->s6_addr[3]) != 0x00) {
        return;
    }

    for (i = 0; i < 12; i++) {
        v6->s6_addr[i] = 0;
    }
    for (; i < 16; i++) {
        v6->s6_addr[i] ^= 0xff;
    }
}

const char * GeoIPDBDescription[NUM_DB_TYPES] = {
    NULL,
    "GeoIP Country Edition",
    "GeoIP City Edition, Rev 1",
    "GeoIP Region Edition, Rev 1",
    "GeoIP ISP Edition",
    "GeoIP Organization Edition",
    "GeoIP City Edition, Rev 0",
    "GeoIP Region Edition, Rev 0",
    "GeoIP Proxy Edition",
    "GeoIP ASNum Edition",
    "GeoIP Netspeed Edition",
    "GeoIP Domain Name Edition",
    "GeoIP Country V6 Edition",
    "GeoIP LocationID ASCII Edition",
    "GeoIP Accuracy Radius Edition",
    NULL,
    NULL,
    "GeoIP Large Country Edition",
    "GeoIP Large Country V6 Edition",
    NULL,
    "GeoIP CCM Edition",
    "GeoIP ASNum V6 Edition",
    "GeoIP ISP V6 Edition",
    "GeoIP Organization V6 Edition",
    "GeoIP Domain Name V6 Edition",
    "GeoIP LocationID ASCII V6 Edition",
    "GeoIP Registrar Edition",
    "GeoIP Registrar V6 Edition",
    "GeoIP UserType Edition",
    "GeoIP UserType V6 Edition",
    "GeoIP City Edition V6, Rev 1",
    "GeoIP City Edition V6, Rev 0",
    "GeoIP Netspeed Edition, Rev 1",
    "GeoIP Netspeed Edition V6, Rev1",
    "GeoIP Country Confidence Edition",
    "GeoIP City Confidence Edition",
    "GeoIP Region Confidence Edition",
    "GeoIP Postal Confidence Edition",
    "GeoIP Accuracy Radius Edition V6"
};

char * GeoIP_custom_directory = NULL;

void GeoIP_setup_custom_directory(char * dir)
{
    GeoIP_custom_directory = dir;
}

char *_GeoIP_full_path_to(const char *file_name)
{
    int len;
    char *path = malloc(sizeof(char) * 1024);

    if (GeoIP_custom_directory == NULL) {
#if !defined(_WIN32)
        memset(path, 0, sizeof(char) * 1024);
        snprintf(path, sizeof(char) * 1024 - 1, "%s/%s", GEOIPDATADIR,
                 file_name);
#else
        char buf[MAX_PATH], *p, *q = NULL;
        memset(buf, 0, sizeof(buf));
        len = GetModuleFileNameA(GetModuleHandle(NULL), buf, sizeof(buf) - 1);
        for (p = buf + len; p > buf; p--) {
            if (*p == '\\') {
                if (!q) {
                    q = p;
                } else{
                    *p = '/';
                }
            }
        }
        *q = 0;
        memset(path, 0, sizeof(char) * 1024);
        snprintf(path, sizeof(char) * 1024 - 1, "%s/%s", buf, file_name);
#endif
    } else {
        len = strlen(GeoIP_custom_directory);
        if (GeoIP_custom_directory[len - 1] != '/') {
            snprintf(path, sizeof(char) * 1024 - 1, "%s/%s",
                     GeoIP_custom_directory,
                     file_name);
        } else {
            snprintf(path, sizeof(char) * 1024 - 1, "%s%s",
                     GeoIP_custom_directory,
                     file_name);
        }
    }
    return path;
}

char ** GeoIPDBFileName = NULL;

void _GeoIP_setup_dbfilename()
{
    if (NULL == GeoIPDBFileName) {
        GeoIPDBFileName = malloc(sizeof(char *) * NUM_DB_TYPES);
        memset(GeoIPDBFileName, 0, sizeof(char *) * NUM_DB_TYPES);

        GeoIPDBFileName[GEOIP_COUNTRY_EDITION] = _GeoIP_full_path_to(
            "GeoIP.dat");
        GeoIPDBFileName[GEOIP_REGION_EDITION_REV0] = _GeoIP_full_path_to(
            "GeoIPRegion.dat");
        GeoIPDBFileName[GEOIP_REGION_EDITION_REV1] = _GeoIP_full_path_to(
            "GeoIPRegion.dat");
        GeoIPDBFileName[GEOIP_CITY_EDITION_REV0] = _GeoIP_full_path_to(
            "GeoIPCity.dat");
        GeoIPDBFileName[GEOIP_CITY_EDITION_REV1] = _GeoIP_full_path_to(
            "GeoIPCity.dat");
        GeoIPDBFileName[GEOIP_ISP_EDITION] = _GeoIP_full_path_to("GeoIPISP.dat");
        GeoIPDBFileName[GEOIP_ORG_EDITION] = _GeoIP_full_path_to("GeoIPOrg.dat");
        GeoIPDBFileName[GEOIP_PROXY_EDITION] = _GeoIP_full_path_to(
            "GeoIPProxy.dat");
        GeoIPDBFileName[GEOIP_ASNUM_EDITION] = _GeoIP_full_path_to(
            "GeoIPASNum.dat");
        GeoIPDBFileName[GEOIP_NETSPEED_EDITION] = _GeoIP_full_path_to(
            "GeoIPNetSpeed.dat");
        GeoIPDBFileName[GEOIP_DOMAIN_EDITION] = _GeoIP_full_path_to(
            "GeoIPDomain.dat");
        GeoIPDBFileName[GEOIP_COUNTRY_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPv6.dat");
        GeoIPDBFileName[GEOIP_LOCATIONA_EDITION] = _GeoIP_full_path_to(
            "GeoIPLocA.dat");
        GeoIPDBFileName[GEOIP_ACCURACYRADIUS_EDITION] = _GeoIP_full_path_to(
            "GeoIPDistance.dat");
        GeoIPDBFileName[GEOIP_LARGE_COUNTRY_EDITION] = _GeoIP_full_path_to(
            "GeoIP.dat");
        GeoIPDBFileName[GEOIP_LARGE_COUNTRY_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPv6.dat");
        GeoIPDBFileName[GEOIP_ASNUM_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPASNumv6.dat");
        GeoIPDBFileName[GEOIP_ISP_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPISPv6.dat");
        GeoIPDBFileName[GEOIP_ORG_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPOrgv6.dat");
        GeoIPDBFileName[GEOIP_DOMAIN_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPDomainv6.dat");
        GeoIPDBFileName[GEOIP_LOCATIONA_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPLocAv6.dat");
        GeoIPDBFileName[GEOIP_REGISTRAR_EDITION] = _GeoIP_full_path_to(
            "GeoIPRegistrar.dat");
        GeoIPDBFileName[GEOIP_REGISTRAR_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPRegistrarv6.dat");
        GeoIPDBFileName[GEOIP_USERTYPE_EDITION] = _GeoIP_full_path_to(
            "GeoIPUserType.dat");
        GeoIPDBFileName[GEOIP_USERTYPE_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPUserTypev6.dat");
        GeoIPDBFileName[GEOIP_CITY_EDITION_REV0_V6] = _GeoIP_full_path_to(
            "GeoIPCityv6.dat");
        GeoIPDBFileName[GEOIP_CITY_EDITION_REV1_V6] = _GeoIP_full_path_to(
            "GeoIPCityv6.dat");
        GeoIPDBFileName[GEOIP_NETSPEED_EDITION_REV1] = _GeoIP_full_path_to(
            "GeoIPNetSpeedCell.dat");
        GeoIPDBFileName[GEOIP_NETSPEED_EDITION_REV1_V6] = _GeoIP_full_path_to(
            "GeoIPNetSpeedCellv6.dat");
        GeoIPDBFileName[GEOIP_COUNTRYCONF_EDITION] = _GeoIP_full_path_to(
            "GeoIPCountryConf.dat");
        GeoIPDBFileName[GEOIP_CITYCONF_EDITION] = _GeoIP_full_path_to(
            "GeoIPCityConf.dat");
        GeoIPDBFileName[GEOIP_REGIONCONF_EDITION] = _GeoIP_full_path_to(
            "GeoIPRegionConf.dat");
        GeoIPDBFileName[GEOIP_POSTALCONF_EDITION] = _GeoIP_full_path_to(
            "GeoIPPostalConf.dat");
        GeoIPDBFileName[GEOIP_ACCURACYRADIUS_EDITION_V6] = _GeoIP_full_path_to(
            "GeoIPDistancev6.dat");
    }
}

static
int _file_exists(const char *file_name)
{
    struct stat file_stat;
    return (stat(file_name, &file_stat) == 0) ? 1 : 0;
}

char * _GeoIP_iso_8859_1__utf8(const char * iso)
{
    signed char c;
    char k;
    char * p;
    char * t = (char *)iso;
    int len = 0;
    while ( ( c = *t++) ) {
        if (c < 0) {
            len++;
        }
    }
    len += t - iso;
    t = p = malloc( len );

    if (p) {
        while ( ( c = *iso++ ) ) {
            if (c < 0) {
                k = 0xc2;
                if (c >= -64) {
                    k++;
                }
                *t++ = k;
                c &= ~0x40;
            }
            *t++ = c;
        }
        *t++ = 0x00;
    }
    return p;
}

int GeoIP_is_private_ipnum_v4( unsigned long ipnum )
{
    return ((ipnum >= 167772160U && ipnum <= 184549375U)
            || (ipnum >= 2851995648U && ipnum <= 2852061183U)
            || (ipnum >= 2886729728U && ipnum <= 2887778303U)
            || (ipnum >= 3232235520U && ipnum <= 3232301055U)
            || (ipnum >= 2130706432U && ipnum <= 2147483647U)) ? 1 : 0;
}

int GeoIP_is_private_v4( const char * addr )
{
    unsigned long ipnum = GeoIP_addr_to_num(addr);
    return GeoIP_is_private_ipnum_v4(ipnum);
}

int GeoIP_db_avail(int type)
{
    const char * filePath;
    if (type < 0 || type >= NUM_DB_TYPES) {
        return 0;
    }
    _GeoIP_setup_dbfilename();
    filePath = GeoIPDBFileName[type];
    if (NULL == filePath) {
        return 0;
    }
    return _file_exists(filePath);
}

static int _database_has_content( int database_type )
{
    return (database_type != GEOIP_COUNTRY_EDITION
            && database_type != GEOIP_PROXY_EDITION
            && database_type != GEOIP_NETSPEED_EDITION
            && database_type != GEOIP_COUNTRY_EDITION_V6
            && database_type != GEOIP_LARGE_COUNTRY_EDITION
            && database_type != GEOIP_LARGE_COUNTRY_EDITION_V6
            && database_type != GEOIP_REGION_EDITION_REV0
            && database_type != GEOIP_REGION_EDITION_REV1) ? 1 : 0;
}

static
void _setup_segments(GeoIP * gi)
{
    int i, j, segment_record_length;
    unsigned char delim[3];
    unsigned char buf[LARGE_SEGMENT_RECORD_LENGTH];
    ssize_t silence _UNUSED;
    int fno = fileno(gi->GeoIPDatabase);

    gi->databaseSegments = NULL;

    /* default to GeoIP Country Edition */
    gi->databaseType = GEOIP_COUNTRY_EDITION;
    gi->record_length = STANDARD_RECORD_LENGTH;
    lseek(fno, -3l, SEEK_END);
    for (i = 0; i < STRUCTURE_INFO_MAX_SIZE; i++) {
        silence = read(fno, delim, 3);
        if (delim[0] == 255 && delim[1] == 255 && delim[2] == 255) {
            silence = read(fno, &gi->databaseType, 1 );
            if (gi->databaseType >= 106) {
                /* backwards compatibility with databases from April 2003 and earlier */
                gi->databaseType -= 105;
            }

            if (gi->databaseType == GEOIP_REGION_EDITION_REV0) {
                /* Region Edition, pre June 2003 */
                gi->databaseSegments = malloc(sizeof(int));
                gi->databaseSegments[0] = STATE_BEGIN_REV0;
            } else if (gi->databaseType == GEOIP_REGION_EDITION_REV1) {
                /* Region Edition, post June 2003 */
                gi->databaseSegments = malloc(sizeof(int));
                gi->databaseSegments[0] = STATE_BEGIN_REV1;
            } else if (gi->databaseType == GEOIP_CITY_EDITION_REV0 ||
                       gi->databaseType == GEOIP_CITY_EDITION_REV1 ||
                       gi->databaseType == GEOIP_ORG_EDITION ||
                       gi->databaseType == GEOIP_ORG_EDITION_V6 ||
                       gi->databaseType == GEOIP_DOMAIN_EDITION ||
                       gi->databaseType == GEOIP_DOMAIN_EDITION_V6 ||
                       gi->databaseType == GEOIP_ISP_EDITION ||
                       gi->databaseType == GEOIP_ISP_EDITION_V6 ||
                       gi->databaseType == GEOIP_REGISTRAR_EDITION ||
                       gi->databaseType == GEOIP_REGISTRAR_EDITION_V6 ||
                       gi->databaseType == GEOIP_USERTYPE_EDITION ||
                       gi->databaseType == GEOIP_USERTYPE_EDITION_V6 ||
                       gi->databaseType == GEOIP_ASNUM_EDITION ||
                       gi->databaseType == GEOIP_ASNUM_EDITION_V6 ||
                       gi->databaseType == GEOIP_NETSPEED_EDITION_REV1 ||
                       gi->databaseType == GEOIP_NETSPEED_EDITION_REV1_V6 ||
                       gi->databaseType == GEOIP_LOCATIONA_EDITION ||
                       gi->databaseType == GEOIP_ACCURACYRADIUS_EDITION ||
                       gi->databaseType == GEOIP_ACCURACYRADIUS_EDITION_V6 ||
                       gi->databaseType == GEOIP_CITY_EDITION_REV0_V6 ||
                       gi->databaseType == GEOIP_CITY_EDITION_REV1_V6 ||
                       gi->databaseType == GEOIP_CITYCONF_EDITION ||
                       gi->databaseType == GEOIP_COUNTRYCONF_EDITION ||
                       gi->databaseType == GEOIP_REGIONCONF_EDITION ||
                       gi->databaseType == GEOIP_POSTALCONF_EDITION
                       ) {
                /* City/Org Editions have two segments, read offset of second segment */
                gi->databaseSegments = malloc(sizeof(int));
                gi->databaseSegments[0] = 0;

                segment_record_length = SEGMENT_RECORD_LENGTH;

                silence = read(fno, buf, segment_record_length );
                for (j = 0; j < segment_record_length; j++) {
                    gi->databaseSegments[0] += (buf[j] << (j * 8));
                }

                /* the record_length must be correct from here on */
                if (gi->databaseType == GEOIP_ORG_EDITION ||
                    gi->databaseType == GEOIP_ORG_EDITION_V6 ||
                    gi->databaseType == GEOIP_DOMAIN_EDITION ||
                    gi->databaseType == GEOIP_DOMAIN_EDITION_V6 ||
                    gi->databaseType == GEOIP_ISP_EDITION ||
                    gi->databaseType == GEOIP_ISP_EDITION_V6) {
                    gi->record_length = ORG_RECORD_LENGTH;
                }
            }
            break;
        } else {
            lseek(fno, -4l, SEEK_CUR);
        }
    }
    if (gi->databaseType == GEOIP_COUNTRY_EDITION ||
        gi->databaseType == GEOIP_PROXY_EDITION ||
        gi->databaseType == GEOIP_NETSPEED_EDITION ||
        gi->databaseType == GEOIP_COUNTRY_EDITION_V6) {
        gi->databaseSegments = malloc(sizeof(int));
        gi->databaseSegments[0] = COUNTRY_BEGIN;
    }else if (gi->databaseType == GEOIP_LARGE_COUNTRY_EDITION ||
              gi->databaseType == GEOIP_LARGE_COUNTRY_EDITION_V6) {
        gi->databaseSegments = malloc(sizeof(int));
        gi->databaseSegments[0] = LARGE_COUNTRY_BEGIN;
    }

}

static
int _check_mtime(GeoIP *gi)
{
    struct stat buf;
    unsigned int idx_size;

#if !defined(_WIN32)
    struct timeval t;
#else /* !defined(_WIN32) */
    FILETIME ft;
    ULONGLONG t;
#endif /* !defined(_WIN32) */

    if (gi->flags & GEOIP_CHECK_CACHE) {

#if !defined(_WIN32)
        /* stat only has second granularity, so don't
         * call it more than once a second */
        gettimeofday(&t, NULL);
        if (t.tv_sec == gi->last_mtime_check) {
            return 0;
        }
        gi->last_mtime_check = t.tv_sec;

#else   /* !defined(_WIN32) */

        /* stat only has second granularity, so don't
           call it more than once a second */
        GetSystemTimeAsFileTime(&ft);
        t = FILETIME_TO_USEC(ft) / 1000 / 1000;
        if (t == gi->last_mtime_check) {
            return 0;
        }
        gi->last_mtime_check = t;

#endif  /* !defined(_WIN32) */

        if (stat(gi->file_path, &buf) != -1) {
            /* make sure that the database file is at least 60
             * seconds untouched. Otherwise we might load the
             * database only partly and crash
             */
            if (buf.st_mtime != gi->mtime &&
                ( buf.st_mtime + 60 < gi->last_mtime_check  ) ) {
                /* GeoIP Database file updated */
                if (gi->flags & (GEOIP_MEMORY_CACHE | GEOIP_MMAP_CACHE)) {
                    if (gi->flags & GEOIP_MMAP_CACHE) {
#if !defined(_WIN32)
                        /* MMAP is only avail on UNIX */
                        munmap(gi->cache, gi->size);
                        gi->cache = NULL;
#endif
                    } else {
                        /* reload database into memory cache */
                        if ((gi->cache =
                                 (unsigned char *)realloc(gi->cache,
                                                          buf.st_size)) ==
                            NULL) {
                            fprintf(stderr, "Out of memory when reloading %s\n",
                                    gi->file_path);
                            return -1;
                        }
                    }
                }
                /* refresh filehandle */
                fclose(gi->GeoIPDatabase);
                gi->GeoIPDatabase = fopen(gi->file_path, "rb");
                if (gi->GeoIPDatabase == NULL) {
                    fprintf(stderr, "Error Opening file %s when reloading\n",
                            gi->file_path);
                    return -1;
                }
                gi->mtime = buf.st_mtime;
                gi->size = buf.st_size;

                if (gi->flags & GEOIP_MMAP_CACHE) {
#if defined(_WIN32)
                    fprintf(stderr,
                            "GEOIP_MMAP_CACHE is not supported on WIN32\n");
                    gi->cache = 0;
                    return -1;
#else
                    gi->cache =
                        mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fileno(
                                 gi->GeoIPDatabase), 0);
                    if (gi->cache == MAP_FAILED) {

                        fprintf(stderr,
                                "Error remapping file %s when reloading\n",
                                gi->file_path);

                        gi->cache = NULL;
                        return -1;
                    }
#endif
                } else if (gi->flags & GEOIP_MEMORY_CACHE) {
                    if (pread(fileno(gi->GeoIPDatabase), gi->cache, buf.st_size,
                              0) != (ssize_t)buf.st_size) {
                        fprintf(stderr,
                                "Error reading file %s when reloading\n",
                                gi->file_path);
                        return -1;
                    }
                }

                if (gi->databaseSegments != NULL) {
                    free(gi->databaseSegments);
                    gi->databaseSegments = NULL;
                }
                _setup_segments(gi);
                if (gi->databaseSegments == NULL) {
                    fprintf(stderr, "Error reading file %s -- corrupt\n",
                            gi->file_path);
                    return -1;
                }

                /* make sure the index is <= file size
                 * This test makes sense for all modes - not
                 * only index
                 */
                idx_size =
                    _database_has_content(gi->databaseType) ? gi->
                    databaseSegments[0
                    ] * (long)gi->record_length * 2 :  buf.st_size;
                if (idx_size > buf.st_size) {
                    fprintf(stderr, "Error file %s -- corrupt\n", gi->file_path);
                    return -1;
                }

                if (gi->flags & GEOIP_INDEX_CACHE) {
                    gi->index_cache = (unsigned char *)realloc(
                        gi->index_cache, sizeof(unsigned char) * idx_size );
                    if (gi->index_cache != NULL) {
                        if (pread(fileno(gi->GeoIPDatabase), gi->index_cache,
                                  idx_size, 0 ) != (ssize_t)idx_size) {
                            fprintf(stderr,
                                    "Error reading file %s where reloading\n",
                                    gi->file_path);
                            return -1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

#define ADDR_STR_LEN (8 * 4 + 7 + 1)
unsigned int _GeoIP_seek_record_v6_gl(GeoIP *gi, geoipv6_t ipnum,
                                      GeoIPLookup * gl)
{
    int depth;
    char paddr[ADDR_STR_LEN];
    unsigned int x;
    unsigned char stack_buffer[2 * MAX_RECORD_LENGTH];
    const unsigned char *buf = (gi->cache == NULL) ? stack_buffer : NULL;
    unsigned int offset = 0;

    const unsigned char * p;
    int j;
    ssize_t silence _UNUSED;
    int fno = fileno(gi->GeoIPDatabase);
    _check_mtime(gi);
    if (GeoIP_teredo(gi) ) {
        __GEOIP_PREPARE_TEREDO(&ipnum);
    }
    for (depth = 127; depth >= 0; depth--) {
        if (gi->cache == NULL && gi->index_cache == NULL) {
            /* read from disk */
            silence = pread(fno, stack_buffer, gi->record_length * 2,
                            (long)gi->record_length * 2 * offset );
        } else if (gi->index_cache == NULL) {
            /* simply point to record in memory */
            buf = gi->cache + (long)gi->record_length * 2 * offset;
        } else {
            buf = gi->index_cache + (long)gi->record_length * 2 * offset;
        }

        if (GEOIP_CHKBIT_V6(depth, ipnum.s6_addr )) {
            /* Take the right-hand branch */
            if (gi->record_length == 3) {
                /* Most common case is completely unrolled and uses constants. */
                x = (buf[3 * 1 + 0] << (0 * 8))
                    + (buf[3 * 1 + 1] << (1 * 8))
                    + (buf[3 * 1 + 2] << (2 * 8));

            } else {
                /* General case */
                j = gi->record_length;
                p = &buf[2 * j];
                x = 0;
                do {
                    x <<= 8;
                    x += *(--p);
                } while (--j);
            }

        } else {
            /* Take the left-hand branch */
            if (gi->record_length == 3) {
                /* Most common case is completely unrolled and uses constants. */
                x = (buf[3 * 0 + 0] << (0 * 8))
                    + (buf[3 * 0 + 1] << (1 * 8))
                    + (buf[3 * 0 + 2] << (2 * 8));
            } else {
                /* General case */
                j = gi->record_length;
                p = &buf[1 * j];
                x = 0;
                do {
                    x <<= 8;
                    x += *(--p);
                } while (--j);
            }
        }

        if (x >= gi->databaseSegments[0]) {
            gi->netmask = gl->netmask = 128 - depth;
            return x;
        }
        offset = x;
    }

    /* shouldn't reach here */
    _GeoIP_inet_ntop(AF_INET6, &ipnum.s6_addr[0], paddr, ADDR_STR_LEN);
    fprintf(
        stderr,
        "Error Traversing Database for ipnum = %s - Perhaps database is corrupt?\n",
        paddr);
    return 0;
}

geoipv6_t
_GeoIP_addr_to_num_v6(const char *addr)
{
    geoipv6_t ipnum;
    if (1 == _GeoIP_inet_pton(AF_INET6, addr, &ipnum.s6_addr[0] ) ) {
        return ipnum;
    }
    return IPV6_NULL;
}


unsigned int _GeoIP_seek_record_gl(GeoIP *gi, unsigned long ipnum,
                                   GeoIPLookup *gl)
{
    int depth;
    unsigned int x;
    unsigned char stack_buffer[2 * MAX_RECORD_LENGTH];
    const unsigned char *buf = (gi->cache == NULL) ? stack_buffer : NULL;
    unsigned int offset = 0;
    ssize_t silence _UNUSED;

    const unsigned char * p;
    int j;
    int fno = fileno(gi->GeoIPDatabase);
    _check_mtime(gi);
    for (depth = 31; depth >= 0; depth--) {
        if (gi->cache == NULL && gi->index_cache == NULL) {
            /* read from disk */
            silence = pread(fno, stack_buffer, gi->record_length * 2,
                            gi->record_length * 2 * offset);
        } else if (gi->index_cache == NULL) {
            /* simply point to record in memory */
            buf = gi->cache + (long)gi->record_length * 2 * offset;
        } else {
            buf = gi->index_cache + (long)gi->record_length * 2 * offset;
        }

        if (ipnum & (1 << depth)) {
            /* Take the right-hand branch */
            if (gi->record_length == 3) {
                /* Most common case is completely unrolled and uses constants. */
                x = (buf[3 * 1 + 0] << (0 * 8))
                    + (buf[3 * 1 + 1] << (1 * 8))
                    + (buf[3 * 1 + 2] << (2 * 8));

            } else {
                /* General case */
                j = gi->record_length;
                p = &buf[2 * j];
                x = 0;
                do {
                    x <<= 8;
                    x += *(--p);
                } while (--j);
            }

        } else {
            /* Take the left-hand branch */
            if (gi->record_length == 3) {
                /* Most common case is completely unrolled and uses constants. */
                x = (buf[3 * 0 + 0] << (0 * 8))
                    + (buf[3 * 0 + 1] << (1 * 8))
                    + (buf[3 * 0 + 2] << (2 * 8));
            } else {
                /* General case */
                j = gi->record_length;
                p = &buf[1 * j];
                x = 0;
                do {
                    x <<= 8;
                    x += *(--p);
                } while (--j);
            }
        }

        if (x >= gi->databaseSegments[0]) {
            gi->netmask = gl->netmask = 32 - depth;
            return x;
        }
        offset = x;
    }
    /* shouldn't reach here */
    fprintf(
        stderr,
        "Error Traversing Database for ipnum = %lu - Perhaps database is corrupt?\n",
        ipnum);
    return 0;
}

unsigned long
GeoIP_addr_to_num(const char *addr)
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

GeoIP * GeoIP_open_type(int type, int flags)
{
    GeoIP * gi;
    const char * filePath;
    if (type < 0 || type >= NUM_DB_TYPES) {
        printf("Invalid database type %d\n", type);
        return NULL;
    }
    _GeoIP_setup_dbfilename();
    filePath = GeoIPDBFileName[type];
    if (filePath == NULL) {
        printf("Invalid database type %d\n", type);
        return NULL;
    }
    gi = GeoIP_open(filePath, flags);

    if (gi) {
        /* make sure this is the requested database type */
        int database_type = gi->databaseType;
        if (database_type > 105) {
            database_type -= 105;
        }
        /*  type must match, but we accept org and asnum,
         *  since domain and *conf database have always the wrong type
         *  for historical reason. Maybe we fix it at some point.
         */
        if (database_type == type
            || database_type == GEOIP_ASNUM_EDITION
            || database_type == GEOIP_ORG_EDITION) {
            return gi;
        }
        GeoIP_delete(gi);
    }

    return NULL;
}

GeoIP * GeoIP_new(int flags)
{
    GeoIP * gi;
    _GeoIP_setup_dbfilename();
    gi = GeoIP_open(GeoIPDBFileName[GEOIP_COUNTRY_EDITION], flags);
    return gi;
}

GeoIP * GeoIP_open(const char * filename, int flags)
{
    struct stat buf;
    unsigned int idx_size;
    GeoIP * gi;
    size_t len;

    gi = (GeoIP *)malloc(sizeof(GeoIP));
    if (gi == NULL) {
        return NULL;
    }
    len = sizeof(char) * (strlen(filename) + 1);
    gi->file_path = malloc(len);
    if (gi->file_path == NULL) {
        free(gi);
        return NULL;
    }
    strncpy(gi->file_path, filename, len);
    gi->GeoIPDatabase = fopen(filename, "rb");
    if (gi->GeoIPDatabase == NULL) {
        fprintf(stderr, "Error Opening file %s\n", filename);
        free(gi->file_path);
        free(gi);
        return NULL;
    } else {
        if (fstat(fileno(gi->GeoIPDatabase), &buf) == -1) {
            fprintf(stderr, "Error stating file %s\n", filename);
            free(gi->file_path);
            free(gi);
            return NULL;
        }
        if (flags & (GEOIP_MEMORY_CACHE | GEOIP_MMAP_CACHE) ) {
            gi->mtime = buf.st_mtime;
            gi->size = buf.st_size;

            /* MMAP added my Peter Shipley */
            if (flags & GEOIP_MMAP_CACHE) {
#if !defined(_WIN32)
                gi->cache =
                    mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fileno(
                             gi->GeoIPDatabase), 0);
                if (gi->cache == MAP_FAILED) {
                    fprintf(stderr, "Error mmaping file %s\n", filename);
                    free(gi->file_path);
                    free(gi);
                    return NULL;
                }
#endif
            } else {
                gi->cache = (unsigned char *)malloc(
                    sizeof(unsigned char) * buf.st_size);

                if (gi->cache != NULL) {
                    if (pread(fileno(gi->GeoIPDatabase), gi->cache, buf.st_size,
                              0) != (ssize_t)buf.st_size) {
                        fprintf(stderr, "Error reading file %s\n", filename);
                        free(gi->cache);
                        free(gi->file_path);
                        free(gi);
                        return NULL;
                    }
                }
            }
        } else {
            if (flags & GEOIP_CHECK_CACHE) {
                if (fstat(fileno(gi->GeoIPDatabase), &buf) == -1) {
                    fprintf(stderr, "Error stating file %s\n", filename);
                    free(gi->file_path);
                    free(gi);
                    return NULL;
                }
                gi->mtime = buf.st_mtime;
            }
            gi->cache = NULL;
        }
        gi->flags = flags;
        gi->charset = GEOIP_CHARSET_ISO_8859_1;
        gi->ext_flags = 1U << GEOIP_TEREDO_BIT;
        _setup_segments(gi);

        idx_size =
            _database_has_content(gi->databaseType) ? gi->databaseSegments[0] *
            (long)gi->record_length * 2 :  buf.st_size;

        /* make sure the index is <= file size */
        if (idx_size > buf.st_size) {
            fprintf(stderr, "Error file %s -- corrupt\n", gi->file_path);
            if (flags & GEOIP_MEMORY_CACHE) {
                free(gi->cache);
            }
#if !defined(_WIN32)
            else if (flags & GEOIP_MMAP_CACHE) {
                /* MMAP is only avail on UNIX */
                munmap(gi->cache, gi->size);
                gi->cache = NULL;
            }
#endif
            free(gi->file_path);
            free(gi);
            return NULL;
        }

        if (flags & GEOIP_INDEX_CACHE) {
            gi->index_cache = (unsigned char *)malloc(
                sizeof(unsigned char) * idx_size);
            if (gi->index_cache != NULL) {
                if (pread(fileno(gi->GeoIPDatabase), gi->index_cache, idx_size,
                          0) != idx_size) {
                    fprintf(stderr, "Error reading file %s\n", filename);
                    free(gi->databaseSegments);
                    free(gi->index_cache);
                    free(gi);
                    return NULL;
                }
            }
        } else {
            gi->index_cache = NULL;
        }
        return gi;
    }
}

void GeoIP_delete(GeoIP *gi)
{
    if (gi == NULL) {
        return;
    }
    if (gi->GeoIPDatabase != NULL) {
        fclose(gi->GeoIPDatabase);
    }
    if (gi->cache != NULL) {
        if (gi->flags & GEOIP_MMAP_CACHE) {
#if !defined(_WIN32)
            munmap(gi->cache, gi->size);
#endif
        } else {
            free(gi->cache);
        }
        gi->cache = NULL;
    }
    if (gi->index_cache != NULL) {
        free(gi->index_cache);
    }
    if (gi->file_path != NULL) {
        free(gi->file_path);
    }
    if (gi->databaseSegments != NULL) {
        free(gi->databaseSegments);
    }
    free(gi);
}

const char *GeoIP_country_code_by_name_v6_gl(GeoIP * gi, const char *name,
                                             GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_v6_gl(gi, name, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}

const char *GeoIP_country_code_by_name_gl(GeoIP * gi, const char *name,
                                          GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_gl(gi, name, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}

const char *GeoIP_country_code3_by_name_v6_gl(GeoIP * gi, const char *name,
                                              GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_v6_gl(gi, name, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

const char *GeoIP_country_code3_by_name_gl(GeoIP * gi, const char *name,
                                           GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_gl(gi, name, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

const char *GeoIP_country_name_by_name_v6_gl(GeoIP * gi, const char *name,
                                             GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_v6_gl(gi, name, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

const char *GeoIP_country_name_by_name_gl(GeoIP * gi, const char *name,
                                          GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_name_gl(gi, name, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

unsigned long _GeoIP_lookupaddress(const char *host)
{
    unsigned long addr = inet_addr(host);
    struct hostent phe2;
    struct hostent * phe = &phe2;
    char *buf = NULL;
#ifdef HAVE_GETHOSTBYNAME_R
    int buflength = 16384;
    int herr = 0;
#endif
    int result = 0;
#ifdef HAVE_GETHOSTBYNAME_R
    buf = malloc(buflength);
#endif
    if (addr == INADDR_NONE) {
#ifdef HAVE_GETHOSTBYNAME_R
        while (1) {
            /* we use gethostbyname_r here because it is thread-safe and gethostbyname is not */
#ifdef GETHOSTBYNAME_R_RETURNS_INT
            result = gethostbyname_r(host, &phe2, buf, buflength, &phe, &herr);
#else
            phe = gethostbyname_r(host, &phe2, buf, buflength, &herr);
#endif
            if (herr != ERANGE) {
                break;
            }
            if (result == 0) {
                break;
            }
            /* double the buffer if the buffer is too small */
            buflength = buflength * 2;
            buf = realloc(buf, buflength);
        }
#else
        /* Some systems do not support gethostbyname_r, such as Mac OS X */
        phe = gethostbyname(host);
#endif
        if (!phe || result != 0) {
            free(buf);
            return 0;
        }
#if !defined(_WIN32)
        addr = *((in_addr_t *)phe->h_addr_list[0]);
#else
        addr = ((IN_ADDR *)phe->h_addr_list[0])->S_un.S_addr;
#endif
    }
#ifdef HAVE_GETHOSTBYNAME_R
    free(buf);
#endif
    return ntohl(addr);
}

geoipv6_t
_GeoIP_lookupaddress_v6(const char *host)
{
    geoipv6_t ipnum;
    int gaierr;
    struct addrinfo hints, *aifirst;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    /* hints.ai_flags = AI_V4MAPPED; */
    hints.ai_socktype = SOCK_STREAM;

    if ((gaierr = getaddrinfo(host, NULL, &hints, &aifirst)) != 0) {
        /* fprintf(stderr, "Err: %s (%d %s)\n", host, gaierr, gai_strerror(gaierr)); */
        return IPV6_NULL;
    }
    memcpy(ipnum.s6_addr,
           ((struct sockaddr_in6 *)aifirst->ai_addr)->sin6_addr.s6_addr,
           sizeof(geoipv6_t));
    freeaddrinfo(aifirst);
    /* inet_pton(AF_INET6, host, ipnum.s6_addr); */

    return ipnum;
}

int GeoIP_id_by_name_gl(GeoIP * gi, const char *name, GeoIPLookup *gl )
{
    unsigned long ipnum;
    int ret;
    if (name == NULL) {
        return 0;
    }
    if (gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION && gi->databaseType !=
        GEOIP_COUNTRY_EDITION && gi->databaseType != GEOIP_PROXY_EDITION &&
        gi->databaseType != GEOIP_NETSPEED_EDITION) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION));
        return 0;
    }
    if (!(ipnum = _GeoIP_lookupaddress(name))) {
        return 0;
    }
    ret = _GeoIP_seek_record_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}

int GeoIP_id_by_name_v6_gl(GeoIP * gi, const char *name, GeoIPLookup * gl)
{
    geoipv6_t ipnum;
    int ret;
    if (name == NULL) {
        return 0;
    }
    if (gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION_V6 &&
        gi->databaseType != GEOIP_COUNTRY_EDITION_V6) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION_V6));
        return 0;
    }
    ipnum = _GeoIP_lookupaddress_v6(name);
    if (__GEOIP_V6_IS_NULL(ipnum)) {
        return 0;
    }

    ret = _GeoIP_seek_record_v6_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}

const char *GeoIP_country_code_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                             GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_addr_v6_gl(gi, addr, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}


const char *GeoIP_country_code_by_addr_gl(GeoIP * gi, const char *addr,
                                          GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_addr_gl(gi, addr, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}
const char *GeoIP_country_code3_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                              GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_addr_v6_gl(gi, addr, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

const char *GeoIP_country_code3_by_addr_gl(GeoIP * gi, const char *addr,
                                           GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_addr_gl(gi, addr, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

const char *GeoIP_country_name_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                             GeoIPLookup * gl )
{
    int country_id;
    country_id = GeoIP_id_by_addr_v6_gl(gi, addr, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

const char *GeoIP_country_name_by_addr_gl(GeoIP * gi, const char *addr,
                                          GeoIPLookup * gl )
{
    int country_id;
    country_id = GeoIP_id_by_addr_gl(gi, addr, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

const char *GeoIP_country_name_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                           GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_gl(gi, ipnum, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

const char *GeoIP_country_name_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                              GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_v6_gl(gi, ipnum, gl);
    return GeoIP_country_name_by_id(gi, country_id );
}

const char *GeoIP_country_code_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                           GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_gl(gi, ipnum, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}

const char *GeoIP_country_code_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                              GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_v6_gl(gi, ipnum, gl);
    return (country_id > 0) ? GeoIP_country_code[country_id] : NULL;
}

const char *GeoIP_country_code3_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                            GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_gl(gi, ipnum, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

const char *GeoIP_country_code3_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                               GeoIPLookup * gl)
{
    int country_id;
    country_id = GeoIP_id_by_ipnum_v6_gl(gi, ipnum, gl);
    return (country_id > 0) ? GeoIP_country_code3[country_id] : NULL;
}

int GeoIP_country_id_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                   GeoIPLookup * gl)
{
    GeoIPLookup n;
    return GeoIP_id_by_addr_v6_gl(gi, addr, &n);
}

int GeoIP_country_id_by_addr_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    return GeoIP_id_by_addr_gl(gi, addr, gl);
}

int GeoIP_country_id_by_name_v6_gl(GeoIP * gi, const char *host,
                                   GeoIPLookup * gl)
{
    return GeoIP_id_by_name_v6_gl(gi, host, gl);
}

int GeoIP_country_id_by_name_gl(GeoIP * gi, const char *host, GeoIPLookup * gl)
{
    return GeoIP_id_by_name_gl(gi, host, gl);
}

int GeoIP_id_by_addr_v6_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    geoipv6_t ipnum;
    int ret;
    if (addr == NULL) {
        return 0;
    }
    if (gi->databaseType != GEOIP_COUNTRY_EDITION_V6
        && gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION_V6) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION_V6));
        return 0;
    }
    ipnum = _GeoIP_addr_to_num_v6(addr);
    ret = _GeoIP_seek_record_v6_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}


int GeoIP_id_by_addr_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    unsigned long ipnum;
    int ret;
    if (addr == NULL) {
        return 0;
    }
    if (gi->databaseType != GEOIP_COUNTRY_EDITION &&
        gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION &&
        gi->databaseType != GEOIP_PROXY_EDITION &&
        gi->databaseType != GEOIP_NETSPEED_EDITION) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION));
        return 0;
    }
    ipnum = GeoIP_addr_to_num(addr);
    ret = _GeoIP_seek_record_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}

int GeoIP_id_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum, GeoIPLookup * gl)
{
    int ret;
    if (gi->databaseType != GEOIP_COUNTRY_EDITION_V6
        && gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION_V6) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION_V6));
        return 0;
    }
    ret = _GeoIP_seek_record_v6_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}

int GeoIP_id_by_ipnum_gl(GeoIP * gi, unsigned long ipnum, GeoIPLookup * gl)
{
    int ret;
    if (ipnum == 0) {
        return 0;
    }
    if (gi->databaseType != GEOIP_COUNTRY_EDITION &&
        gi->databaseType != GEOIP_LARGE_COUNTRY_EDITION &&
        gi->databaseType != GEOIP_PROXY_EDITION &&
        gi->databaseType != GEOIP_NETSPEED_EDITION) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_COUNTRY_EDITION));
        return 0;
    }
    ret = _GeoIP_seek_record_gl(gi, ipnum, gl) - gi->databaseSegments[0];
    return ret;
}

char *GeoIP_database_info(GeoIP * gi)
{
    int i;
    unsigned char buf[3];
    char *retval;
    int hasStructureInfo = 0;
    ssize_t silence _UNUSED;
    int fno;

    if (gi == NULL) {
        return NULL;
    }

    fno = fileno(gi->GeoIPDatabase);

    _check_mtime(gi);
    lseek(fno, -3l, SEEK_END);

    /* first get past the database structure information */
    for (i = 0; i < STRUCTURE_INFO_MAX_SIZE; i++) {
        silence = read(fno, buf, 3 );
        if (buf[0] == 255 && buf[1] == 255 && buf[2] == 255) {
            hasStructureInfo = 1;
            break;
        }
        lseek(fno, -4l, SEEK_CUR);
    }
    if (hasStructureInfo == 1) {
        lseek(fno, -6l, SEEK_CUR);
    } else {
        /* no structure info, must be pre Sep 2002 database, go back to end */
        lseek(fno, -3l, SEEK_END);
    }

    for (i = 0; i < DATABASE_INFO_MAX_SIZE; i++) {
        silence = read(fno, buf, 3 );
        if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0) {
            retval = malloc(sizeof(char) * (i + 1));
            if (retval == NULL) {
                return NULL;
            }
            silence = read(fno, retval, i);
            retval[i] = '\0';
            return retval;
        }
        lseek(fno, -4l, SEEK_CUR);
    }
    return NULL;
}

/* GeoIP Region Edition functions */

void GeoIP_assign_region_by_inetaddr_gl(GeoIP * gi, unsigned long inetaddr,
                                        GeoIPRegion *region,
                                        GeoIPLookup * gl)
{
    unsigned int seek_region;

    /* This also writes in the terminating NULs (if you decide to
     * keep them) and clear any fields that are not set. */
    memset(region, 0, sizeof(GeoIPRegion));

    seek_region = _GeoIP_seek_record_gl(gi, ntohl(inetaddr), gl);

    if (gi->databaseType == GEOIP_REGION_EDITION_REV0) {
        /* Region Edition, pre June 2003 */
        seek_region -= STATE_BEGIN_REV0;
        if (seek_region >= 1000) {
            region->country_code[0] = 'U';
            region->country_code[1] = 'S';
            region->region[0] = (char)((seek_region - 1000) / 26 + 65);
            region->region[1] = (char)((seek_region - 1000) % 26 + 65);
        } else {
            memcpy(region->country_code, GeoIP_country_code[seek_region], 2);
        }
    } else if (gi->databaseType == GEOIP_REGION_EDITION_REV1) {
        /* Region Edition, post June 2003 */
        seek_region -= STATE_BEGIN_REV1;
        if (seek_region < US_OFFSET) {
            /* Unknown */
            /* we don't need to do anything here b/c we memset region to 0 */
        } else if (seek_region < CANADA_OFFSET) {
            /* USA State */
            region->country_code[0] = 'U';
            region->country_code[1] = 'S';
            region->region[0] = (char)((seek_region - US_OFFSET) / 26 + 65);
            region->region[1] = (char)((seek_region - US_OFFSET) % 26 + 65);
        } else if (seek_region < WORLD_OFFSET) {
            /* Canada Province */
            region->country_code[0] = 'C';
            region->country_code[1] = 'A';
            region->region[0] = (char)((seek_region - CANADA_OFFSET) / 26 + 65);
            region->region[1] = (char)((seek_region - CANADA_OFFSET) % 26 + 65);
        } else {
            /* Not US or Canada */
            memcpy(
                region->country_code,
                GeoIP_country_code[(seek_region -
                                    WORLD_OFFSET) / FIPS_RANGE], 2);
        }
    }
}

void GeoIP_assign_region_by_inetaddr_v6_gl(GeoIP * gi, geoipv6_t inetaddr,
                                           GeoIPRegion *region,
                                           GeoIPLookup * gl)
{
    unsigned int seek_region;

    /* This also writes in the terminating NULs (if you decide to
     * keep them) and clear any fields that are not set. */
    memset(region, 0, sizeof(GeoIPRegion));

    seek_region = _GeoIP_seek_record_v6_gl(gi, inetaddr, gl);

    if (gi->databaseType == GEOIP_REGION_EDITION_REV0) {
        /* Region Edition, pre June 2003 */
        seek_region -= STATE_BEGIN_REV0;
        if (seek_region >= 1000) {
            region->country_code[0] = 'U';
            region->country_code[1] = 'S';
            region->region[0] = (char)((seek_region - 1000) / 26 + 65);
            region->region[1] = (char)((seek_region - 1000) % 26 + 65);
        } else {
            memcpy(region->country_code, GeoIP_country_code[seek_region], 2);
        }
    } else if (gi->databaseType == GEOIP_REGION_EDITION_REV1) {
        /* Region Edition, post June 2003 */
        seek_region -= STATE_BEGIN_REV1;
        if (seek_region < US_OFFSET) {
            /* Unknown */
            /* we don't need to do anything here b/c we memset region to 0 */
        } else if (seek_region < CANADA_OFFSET) {
            /* USA State */
            region->country_code[0] = 'U';
            region->country_code[1] = 'S';
            region->region[0] = (char)((seek_region - US_OFFSET) / 26 + 65);
            region->region[1] = (char)((seek_region - US_OFFSET) % 26 + 65);
        } else if (seek_region < WORLD_OFFSET) {
            /* Canada Province */
            region->country_code[0] = 'C';
            region->country_code[1] = 'A';
            region->region[0] = (char)((seek_region - CANADA_OFFSET) / 26 + 65);
            region->region[1] = (char)((seek_region - CANADA_OFFSET) % 26 + 65);
        } else {
            /* Not US or Canada */
            memcpy(
                region->country_code,
                GeoIP_country_code[(seek_region -
                                    WORLD_OFFSET) / FIPS_RANGE], 2);
        }
    }
}

static
GeoIPRegion * _get_region_gl(GeoIP * gi, unsigned long ipnum, GeoIPLookup * gl)
{
    GeoIPRegion * region;

    region = malloc(sizeof(GeoIPRegion));
    if (region) {
        GeoIP_assign_region_by_inetaddr_gl(gi, htonl(ipnum), region, gl);
    }
    return region;
}

static
GeoIPRegion * _get_region_v6_gl(GeoIP * gi, geoipv6_t ipnum, GeoIPLookup * gl)
{
    GeoIPRegion * region;

    region = malloc(sizeof(GeoIPRegion));
    if (region) {
        GeoIP_assign_region_by_inetaddr_v6_gl(gi, ipnum, region, gl);
    }
    return region;
}

GeoIPRegion * GeoIP_region_by_addr_gl(GeoIP * gi, const char *addr,
                                      GeoIPLookup * gl)
{
    unsigned long ipnum;
    if (addr == NULL) {
        return NULL;
    }
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }
    ipnum = GeoIP_addr_to_num(addr);
    return _get_region_gl(gi, ipnum, gl);
}

GeoIPRegion * GeoIP_region_by_addr_v6_gl(GeoIP * gi, const char *addr,
                                         GeoIPLookup * gl)
{
    geoipv6_t ipnum;
    if (addr == NULL) {
        return NULL;
    }
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }
    ipnum = _GeoIP_addr_to_num_v6(addr);
    return _get_region_v6_gl(gi, ipnum, gl);
}

GeoIPRegion * GeoIP_region_by_name_gl(GeoIP * gi, const char *name,
                                      GeoIPLookup * gl)
{
    unsigned long ipnum;
    if (name == NULL) {
        return NULL;
    }
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }
    if (!(ipnum = _GeoIP_lookupaddress(name))) {
        return NULL;
    }
    return _get_region_gl(gi, ipnum, gl);
}

GeoIPRegion * GeoIP_region_by_name_v6_gl(GeoIP * gi, const char *name,
                                         GeoIPLookup * gl)
{
    geoipv6_t ipnum;
    if (name == NULL) {
        return NULL;
    }
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }

    ipnum = _GeoIP_lookupaddress_v6(name);
    if (__GEOIP_V6_IS_NULL(ipnum)) {
        return NULL;
    }
    return _get_region_v6_gl(gi, ipnum, gl);
}

GeoIPRegion * GeoIP_region_by_ipnum_gl(GeoIP * gi, unsigned long ipnum,
                                       GeoIPLookup * gl)
{
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }
    return _get_region_gl(gi, ipnum, gl);
}

GeoIPRegion * GeoIP_region_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum,
                                          GeoIPLookup * gl)
{
    if (gi->databaseType != GEOIP_REGION_EDITION_REV0 &&
        gi->databaseType != GEOIP_REGION_EDITION_REV1) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_REGION_EDITION_REV1));
        return NULL;
    }
    return _get_region_v6_gl(gi, ipnum, gl);
}

void GeoIPRegion_delete(GeoIPRegion *gir)
{
    free(gir);
}

/* GeoIP Organization, ISP and AS Number Edition private method */
static
char *_get_name_gl(GeoIP * gi, unsigned long ipnum, GeoIPLookup * gl)
{
    int seek_org;
    char buf[MAX_ORG_RECORD_LENGTH];
    char * org_buf, * buf_pointer;
    int record_pointer;
    size_t len;
    ssize_t silence _UNUSED;

    if (gi->databaseType != GEOIP_ORG_EDITION &&
        gi->databaseType != GEOIP_ISP_EDITION &&
        gi->databaseType != GEOIP_DOMAIN_EDITION &&
        gi->databaseType != GEOIP_ASNUM_EDITION &&
        gi->databaseType != GEOIP_ACCURACYRADIUS_EDITION &&
        gi->databaseType != GEOIP_NETSPEED_EDITION_REV1 &&
        gi->databaseType != GEOIP_USERTYPE_EDITION &&
        gi->databaseType != GEOIP_REGISTRAR_EDITION &&
        gi->databaseType != GEOIP_LOCATIONA_EDITION &&
        gi->databaseType != GEOIP_CITYCONF_EDITION &&
        gi->databaseType != GEOIP_COUNTRYCONF_EDITION &&
        gi->databaseType != GEOIP_REGIONCONF_EDITION &&
        gi->databaseType != GEOIP_POSTALCONF_EDITION
        ) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_ORG_EDITION));
        return NULL;
    }

    seek_org = _GeoIP_seek_record_gl(gi, ipnum, gl);
    if (seek_org == gi->databaseSegments[0]) {
        return NULL;
    }

    record_pointer = seek_org +
                     (2 * gi->record_length - 1) * gi->databaseSegments[0];

    if (gi->cache == NULL) {
        silence = pread(fileno(
                            gi->GeoIPDatabase), buf, MAX_ORG_RECORD_LENGTH,
                        record_pointer);
        if (gi->charset == GEOIP_CHARSET_UTF8) {
            org_buf = _GeoIP_iso_8859_1__utf8( (const char * )buf );
        } else {
            len = sizeof(char) * (strlen(buf) + 1);
            org_buf = malloc(len);
            strncpy(org_buf, buf, len);
        }
    } else {
        buf_pointer = (char *)(gi->cache + (long)record_pointer);
        if (gi->charset == GEOIP_CHARSET_UTF8) {
            org_buf = _GeoIP_iso_8859_1__utf8( (const char * )buf_pointer );
        } else {
            len = sizeof(char) * (strlen(buf_pointer) + 1);
            org_buf = malloc(len);
            strncpy(org_buf, buf_pointer, len);
        }
    }
    return org_buf;
}

static
char *_get_name_v6_gl(GeoIP * gi, geoipv6_t ipnum, GeoIPLookup * gl)
{
    int seek_org;
    char buf[MAX_ORG_RECORD_LENGTH];
    char * org_buf, * buf_pointer;
    int record_pointer;
    size_t len;
    ssize_t silence _UNUSED;

    if (
        gi->databaseType != GEOIP_ORG_EDITION_V6 &&
        gi->databaseType != GEOIP_ISP_EDITION_V6 &&
        gi->databaseType != GEOIP_DOMAIN_EDITION_V6 &&
        gi->databaseType != GEOIP_ASNUM_EDITION_V6 &&
        gi->databaseType != GEOIP_ACCURACYRADIUS_EDITION_V6 &&
        gi->databaseType != GEOIP_NETSPEED_EDITION_REV1_V6 &&
        gi->databaseType != GEOIP_USERTYPE_EDITION_V6 &&
        gi->databaseType != GEOIP_REGISTRAR_EDITION_V6 &&
        gi->databaseType != GEOIP_LOCATIONA_EDITION_V6
        ) {
        printf("Invalid database type %s, expected %s\n",
               get_db_description(gi->databaseType),
               get_db_description(GEOIP_ORG_EDITION));
        return NULL;
    }

    seek_org = _GeoIP_seek_record_v6_gl(gi, ipnum, gl);
    if (seek_org == gi->databaseSegments[0]) {
        return NULL;
    }

    record_pointer = seek_org +
                     (2 * gi->record_length - 1) * gi->databaseSegments[0];

    if (gi->cache == NULL) {
        silence = pread(fileno(
                            gi->GeoIPDatabase), buf, MAX_ORG_RECORD_LENGTH,
                        record_pointer);
        if (gi->charset == GEOIP_CHARSET_UTF8) {
            org_buf = _GeoIP_iso_8859_1__utf8( (const char * )buf );
        } else {
            len = sizeof(char) * (strlen(buf) + 1);
            org_buf = malloc(len);
            strncpy(org_buf, buf, len);
        }
    } else {
        buf_pointer = (char *)(gi->cache + (long)record_pointer);
        if (gi->charset == GEOIP_CHARSET_UTF8) {
            org_buf = _GeoIP_iso_8859_1__utf8( (const char * )buf_pointer );
        } else {
            len = sizeof(char) * (strlen(buf_pointer) + 1);
            org_buf = malloc(len);
            strncpy(org_buf, buf_pointer, len);
        }
    }
    return org_buf;
}

char * GeoIP_num_to_addr(unsigned long ipnum)
{
    char *ret_str;
    char *cur_str;
    int octet[4];
    int num_chars_written, i;

    ret_str = malloc(sizeof(char) * 16);
    cur_str = ret_str;

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

    return ret_str;
}

char **GeoIP_range_by_ip_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    unsigned long ipnum;
    unsigned long left_seek;
    unsigned long right_seek;
    unsigned long mask;
    int orig_netmask;
    int target_value;
    char **ret;
    GeoIPLookup t;

    if (addr == NULL) {
        return NULL;
    }

    ret = malloc(sizeof(char *) * 2);

    ipnum = GeoIP_addr_to_num(addr);
    target_value = _GeoIP_seek_record_gl(gi, ipnum, gl);
    orig_netmask = gl->netmask;
    mask = 0xffffffff << ( 32 - orig_netmask );
    left_seek = ipnum & mask;
    right_seek = left_seek + ( 0xffffffff & ~mask );

    while (left_seek != 0
           && target_value == _GeoIP_seek_record_gl(gi, left_seek - 1, &t) ) {

        /* Go to beginning of netblock defined by netmask */
        mask = 0xffffffff << ( 32 - t.netmask );
        left_seek = ( left_seek - 1 ) & mask;
    }
    ret[0] = GeoIP_num_to_addr(left_seek);

    while (right_seek != 0xffffffff
           && target_value == _GeoIP_seek_record_gl(gi, right_seek + 1, &t) ) {

        /* Go to end of netblock defined by netmask */
        mask = 0xffffffff << ( 32 - t.netmask );
        right_seek = ( right_seek + 1 ) & mask;
        right_seek += 0xffffffff & ~mask;
    }
    ret[1] = GeoIP_num_to_addr(right_seek);

    gi->netmask = orig_netmask;

    return ret;
}
void GeoIP_range_by_ip_delete( char ** ptr )
{
    if (ptr) {
        if (ptr[0]) {
            free(ptr[0]);
        }
        if (ptr[1]) {
            free(ptr[1]);
        }
        free(ptr);
    }
}

char *GeoIP_name_by_ipnum_gl(GeoIP * gi, unsigned long ipnum, GeoIPLookup * gl)
{
    return _get_name_gl(gi, ipnum, gl);
}

char *GeoIP_name_by_ipnum_v6_gl(GeoIP * gi, geoipv6_t ipnum, GeoIPLookup * gl)
{
    return _get_name_v6_gl(gi, ipnum, gl);
}

char *GeoIP_name_by_addr_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    unsigned long ipnum;
    if (addr == NULL) {
        return NULL;
    }
    ipnum = GeoIP_addr_to_num(addr);
    return _get_name_gl(gi, ipnum, gl);
}

char *GeoIP_name_by_addr_v6_gl(GeoIP * gi, const char *addr, GeoIPLookup * gl)
{
    geoipv6_t ipnum;
    if (addr == NULL) {
        return NULL;
    }
    ipnum = _GeoIP_addr_to_num_v6(addr);
    return _get_name_v6_gl(gi, ipnum, gl);
}

char *GeoIP_name_by_name_gl(GeoIP * gi, const char *name, GeoIPLookup * gl)
{
    unsigned long ipnum;
    if (name == NULL) {
        return NULL;
    }
    if (!(ipnum = _GeoIP_lookupaddress(name))) {
        return NULL;
    }
    return _get_name_gl(gi, ipnum, gl);
}

char *GeoIP_name_by_name_v6_gl(GeoIP * gi, const char *name, GeoIPLookup *gl)
{
    geoipv6_t ipnum;
    if (name == NULL) {
        return NULL;
    }
    ipnum = _GeoIP_lookupaddress_v6(name);
    if (__GEOIP_V6_IS_NULL(ipnum)) {
        return NULL;
    }
    return _get_name_v6_gl(gi, ipnum, gl);
}

unsigned char GeoIP_database_edition(GeoIP * gi)
{
    return gi->databaseType;
}

int GeoIP_enable_teredo(GeoIP * gi, int true_false)
{
    unsigned int mask = ( 1U << GEOIP_TEREDO_BIT );
    int b = ( gi->ext_flags & mask ) ? 1 : 0;
    gi->ext_flags &= ~mask;
    if (true_false) {
        gi->ext_flags |= true_false;
    }
    return b;
}

int GeoIP_teredo( GeoIP * gi )
{
    unsigned int mask = ( 1U << GEOIP_TEREDO_BIT );
    return ( gi->ext_flags & mask ) ? 1 : 0;
}

int GeoIP_charset( GeoIP * gi)
{
    return gi->charset;
}

int GeoIP_set_charset(  GeoIP * gi, int charset )
{
    int old_charset = gi->charset;
    gi->charset = charset;
    return old_charset;
}

/** return two letter country code */
const char * GeoIP_code_by_id(int id)
{
    if (id < 0 || id >= (int)num_GeoIP_countries) {
        return NULL;
    }

    return GeoIP_country_code[id];
}

/** return three letter country code */
const char * GeoIP_code3_by_id(int id)
{
    if (id < 0 || id >= (int)num_GeoIP_countries) {
        return NULL;
    }

    return GeoIP_country_code3[id];
}


/** return full name of country in utf8 or iso-8859-1 */
const char * GeoIP_country_name_by_id(GeoIP * gi, int id)
{
    /* return NULL also even for index 0 for backward compatibility */
    if (id <= 0 || id >= (int)num_GeoIP_countries) {
        return NULL;
    }
    return (gi->charset == GEOIP_CHARSET_UTF8)
           ? GeoIP_utf8_country_name[id]
           : GeoIP_country_name[id];
}

/** return full name of country in iso-8859-1 */
const char * GeoIP_name_by_id(int id)
{
    if (id < 0 || id >= (int)num_GeoIP_countries) {
        return NULL;
    }

    return GeoIP_country_name[id];
}

/** return continent of country */
const char * GeoIP_continent_by_id(int id)
{
    if (id < 0 || id >= (int)num_GeoIP_countries) {
        return NULL;
    }

    return GeoIP_country_continent[id];
}

/** return id by country code **/
int GeoIP_id_by_code(const char *country)
{
    unsigned i;

    for (i = 0; i < num_GeoIP_countries; ++i) {
        if (strcmp(country, GeoIP_country_code[i]) == 0) {
            return i;
        }
    }

    return 0;
}

unsigned GeoIP_num_countries(void)
{
    return num_GeoIP_countries;
}

const char * GeoIP_lib_version(void)
{
    return PACKAGE_VERSION;
}

int GeoIP_cleanup(void)
{
    int i, result = 0;
    if (GeoIPDBFileName) {

        for (i = 0; i < NUM_DB_TYPES; i++) {
            if (GeoIPDBFileName[i]) {
                free(GeoIPDBFileName[i]);
            }
        }

        free(GeoIPDBFileName);
        GeoIPDBFileName = NULL;
        result = 1;
    }

    return result;
}

