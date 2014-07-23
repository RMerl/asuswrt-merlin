#include <stdio.h>
#include "GeoIP.h"
#include "GeoIPCity.h"
#if !defined(_WIN32)
#include <sys/time.h>
#endif                          /* !defined(_WIN32) */

char *ipstring[4] = { "24.24.24.24",  "80.24.24.80",
                      "200.24.24.40", "68.24.24.46" };

int numipstrings = 4;

#if !defined(_WIN32)
struct timeval timer_t1;
struct timeval timer_t2;
#else                           /* !defined(_WIN32) */
FILETIME timer_t1;              /* 100 ns */
FILETIME timer_t2;
#endif                          /* !defined(_WIN32) */

#if !defined(_WIN32)
void timerstart()
{
    gettimeofday(&timer_t1, NULL);
}

double timerstop()
{
    int a1 = 0;
    int a2 = 0;
    double r = 0;
    gettimeofday(&timer_t2, NULL);
    a1 = timer_t2.tv_sec - timer_t1.tv_sec;
    a2 = timer_t2.tv_usec - timer_t1.tv_usec;
    if (a1 < 0) {
        a1 = a1 - 1;
        a2 = a2 + 1000000;
    }
    r = (((double)a1) + (((double)a2) / 1000000));
    return r;
}
#else                           /* !defined(_WIN32) */
void timerstart()
{
    GetSystemTimeAsFileTime(&timer_t1);
}

double timerstop()
{
    __int64 delta;              /* VC6 can't convert an unsigned int64 to to double */
    GetSystemTimeAsFileTime(&timer_t2);
    delta = FILETIME_TO_USEC(timer_t2) - FILETIME_TO_USEC(timer_t2);
    return delta;
}
#endif                          /* !defined(_WIN32) */

void testgeoipcountry(int flags, const char *msg, int numlookups)
{
    const char *str = NULL;
    double t = 0;
    int i4 = 0;
    int i2 = 0;
    GeoIP *i = NULL;
    i = GeoIP_open("/usr/local/share/GeoIP/GeoIP.dat", flags);
    if (i == NULL) {
        printf("error: GeoIP.dat does not exist\n");
        return;
    }
    timerstart();
    for (i2 = 0; i2 < numlookups; i2++) {
        str = GeoIP_country_name_by_addr(i, ipstring[i4]);
        i4 = (i4 + 1) % numipstrings;
    }
    t = timerstop();
    printf("%s\n", msg);
    printf("%d lookups made in %f seconds \n", numlookups, t);
    GeoIP_delete(i);
}

void testgeoiporg(int flags, const char *msg, int numlookups)
{
    GeoIP *i = NULL;
    char *i3 = NULL;
    int i4 = 0;
    int i2 = 0;
    double t = 0;
    i = GeoIP_open("/usr/local/share/GeoIP/GeoIPOrg.dat", flags);
    if (i == NULL) {
        printf("error: GeoIPOrg.dat does not exist\n");
        return;
    }
    timerstart();
    for (i2 = 0; i2 < numlookups; i2++) {
        i3 = GeoIP_name_by_addr(i, ipstring[i4]);
        i4 = (i4 + 1) % numipstrings;
    }
    t = timerstop();
    printf("%s\n", msg);
    printf("%d lookups made in %f seconds \n", numlookups, t);
    GeoIP_delete(i);
}

void testgeoipregion(int flags, const char *msg, int numlookups)
{
    GeoIP *i = NULL;
    GeoIPRegion *i3 = NULL;
    int i4 = 0;
    int i2 = 0;
    double t = 0;
    i = GeoIP_open("/usr/local/share/GeoIP/GeoIPRegion.dat", flags);
    if (i == NULL) {
        printf("error: GeoIPRegion.dat does not exist\n");
        return;
    }
    timerstart();
    for (i2 = 0; i2 < numlookups; i2++) {
        i3 = GeoIP_region_by_addr(i, ipstring[i4]);
        GeoIPRegion_delete(i3);
        i4 = (i4 + 1) % numipstrings;
    }
    t = timerstop();
    printf("%s\n", msg);
    printf("%d lookups made in %f seconds \n", numlookups, t);
    GeoIP_delete(i);
}

void testgeoipcity(int flags, const char *msg, int numlookups)
{
    GeoIP *i = NULL;
    GeoIPRecord *i3 = NULL;
    int i4 = 0;
    int i2 = 0;
    double t = 0;
    i = GeoIP_open("/usr/local/share/GeoIP/GeoIPCity.dat", flags);
    if (i == NULL) {
        printf("error: GeoLiteCity.dat does not exist\n");
        return;
    }
    timerstart();
    for (i2 = 0; i2 < numlookups; i2++) {
        i3 = GeoIP_record_by_addr(i, ipstring[i4]);
        GeoIPRecord_delete(i3);
        i4 = (i4 + 1) % numipstrings;
    }
    t = timerstop();
    printf("%s\n", msg);
    printf("%d lookups made in %f seconds \n", numlookups, t);
    GeoIP_delete(i);
}

int main()
{
    int time = 300 * numipstrings;
    testgeoipcountry(0, "GeoIP Country", 100 * time);
    testgeoipcountry(GEOIP_CHECK_CACHE, "GeoIP Country with GEOIP_CHECK_CACHE",
                     100 * time);
    testgeoipcountry(GEOIP_MEMORY_CACHE,
                     "GeoIP Country with GEOIP_MEMORY_CACHE", 1000 * time);
    testgeoipcountry(
        GEOIP_MEMORY_CACHE | GEOIP_CHECK_CACHE,
        "GeoIP Country with GEOIP_MEMORY_CACHE and GEOIP_CHECK_CACHE",
        1000 * time);

    testgeoipregion(0, "GeoIP Region", 100 * time);
    testgeoipregion(GEOIP_CHECK_CACHE, "GeoIP Region with GEOIP_CHECK_CACHE",
                    100 * time);
    testgeoipregion(GEOIP_MEMORY_CACHE, "GeoIP Region with GEOIP_MEMORY_CACHE",
                    1000 * time);
    testgeoipregion(
        GEOIP_MEMORY_CACHE | GEOIP_CHECK_CACHE,
        "GeoIP Region with GEOIP_MEMORY_CACHE and GEOIP_CHECK_CACHE",
        1000 * time);

    testgeoiporg(0, "GeoIP Org", 50 * time);
    testgeoiporg(GEOIP_INDEX_CACHE, "GeoIP Org with GEOIP_INDEX_CACHE",
                 200 * time);
    testgeoiporg(GEOIP_INDEX_CACHE | GEOIP_CHECK_CACHE,
                 "GeoIP Org with GEOIP_INDEX_CACHE and GEOIP_CHECK_CACHE",
                 200 * time);
    testgeoiporg(GEOIP_MEMORY_CACHE, "GeoIP Org with GEOIP_MEMORY_CACHE",
                 500 * time);

    testgeoipcity(0, "GeoIP City", 50 * time);
    testgeoipcity(GEOIP_INDEX_CACHE, "GeoIP City with GEOIP_INDEX_CACHE",
                  200 * time);
    testgeoipcity(GEOIP_INDEX_CACHE | GEOIP_CHECK_CACHE,
                  "GeoIP City with GEOIP_INDEX_CACHE and GEOIP_CHECK_CACHE",
                  200 * time);
    testgeoipcity(GEOIP_MEMORY_CACHE, "GeoIP City with GEOIP_MEMORY_CACHE",
                  500 * time);
    return 0;
}
