/*  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
Copyright (c) 2007, 2008 by Juliusz Chroboczek
Copyright 2011 by Matthieu Boutier and Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "babel_main.h"
#include "babeld.h"
#include "util.h"

unsigned
roughly(unsigned value)
{
    return value * 3 / 4 + random() % (value / 2);
}

/* d = s1 - s2 */
void
timeval_minus(struct timeval *d,
              const struct timeval *s1, const struct timeval *s2)
{
    if(s1->tv_usec >= s2->tv_usec) {
        d->tv_usec = s1->tv_usec - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec;
    } else {
        d->tv_usec = s1->tv_usec + 1000000 - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec - 1;
    }
}

unsigned
timeval_minus_msec(const struct timeval *s1, const struct timeval *s2)
{
    if(s1->tv_sec < s2->tv_sec)
        return 0;

    /* Avoid overflow. */
    if(s1->tv_sec - s2->tv_sec > 2000000)
        return 2000000000;

    if(s1->tv_sec > s2->tv_sec)
        return
            (unsigned)((unsigned)(s1->tv_sec - s2->tv_sec) * 1000 +
                       ((int)s1->tv_usec - s2->tv_usec) / 1000);

    if(s1->tv_usec <= s2->tv_usec)
        return 0;

    return (unsigned)(s1->tv_usec - s2->tv_usec) / 1000u;
}

/* d = s + msecs */
void
timeval_add_msec(struct timeval *d, const struct timeval *s, const int msecs)
{
    int usecs;
    d->tv_sec = s->tv_sec + msecs / 1000;
    usecs = s->tv_usec + (msecs % 1000) * 1000;
    if(usecs < 1000000) {
        d->tv_usec = usecs;
    } else {
        d->tv_usec = usecs - 1000000;
        d->tv_sec++;
    }
}

void
set_timeout(struct timeval *timeout, int msecs)
{
    timeval_add_msec(timeout, &babel_now, roughly(msecs));
}

/* returns <0 if "s1" < "s2", etc. */
int
timeval_compare(const struct timeval *s1, const struct timeval *s2)
{
    if(s1->tv_sec < s2->tv_sec)
        return -1;
    else if(s1->tv_sec > s2->tv_sec)
        return 1;
    else if(s1->tv_usec < s2->tv_usec)
        return -1;
    else if(s1->tv_usec > s2->tv_usec)
        return 1;
    else
        return 0;
}

/* set d at min(d, s) */
/* {0, 0} represents infinity */
void
timeval_min(struct timeval *d, const struct timeval *s)
{
    if(s->tv_sec == 0)
        return;

    if(d->tv_sec == 0 || timeval_compare(d, s) > 0) {
        *d = *s;
    }
}

/* set d to min(d, x) with x in [secs, secs+1] */
void
timeval_min_sec(struct timeval *d, time_t secs)
{
    if(d->tv_sec == 0 || d->tv_sec > secs) {
        d->tv_sec = secs;
        d->tv_usec = random() % 1000000;
    }
}

/* parse a float value in second and return the corresponding mili-seconds.
 For example:
 parse_msec("12.342345") returns 12342 */
int
parse_msec(const char *string)
{
    unsigned int in, fl;
    int i, j;

    in = fl = 0;
    i = 0;
    while(string[i] == ' ' || string[i] == '\t')
        i++;
    while(string[i] >= '0' && string[i] <= '9') {
        in = in * 10 + string[i] - '0';
        i++;
    }
    if(string[i] == '.') {
        i++;
        j = 0;
        while(string[i] >= '0' && string[i] <= '9') {
            fl = fl * 10 + string[i] - '0';
            i++;
            j++;
        }

        while(j > 3) {
            fl /= 10;
            j--;
        }
        while(j < 3) {
            fl *= 10;
            j++;
        }
    }

    while(string[i] == ' ' || string[i] == '\t')
        i++;

    if(string[i] == '\0')
        return in * 1000 + fl;

    return -1;
}

int
in_prefix(const unsigned char *restrict address,
          const unsigned char *restrict prefix, unsigned char plen)
{
    unsigned char m;

    if(plen > 128)
        plen = 128;

    if(memcmp(address, prefix, plen / 8) != 0)
        return 0;

    if(plen % 8 == 0)
        return 1;

    m = 0xFF << (8 - (plen % 8));

    return ((address[plen / 8] & m) == (prefix[plen / 8] & m));
}

unsigned char *
mask_prefix(unsigned char *restrict ret,
            const unsigned char *restrict prefix, unsigned char plen)
{
    if(plen >= 128) {
        memcpy(ret, prefix, 16);
        return ret;
    }

    memset(ret, 0, 16);
    memcpy(ret, prefix, plen / 8);
    if(plen % 8 != 0)
        ret[plen / 8] =
            (prefix[plen / 8] & ((0xFF << (8 - (plen % 8))) & 0xFF));
    return ret;
}

static const unsigned char v4prefix[16] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };

static const unsigned char llprefix[16] =
    {0xFE, 0x80};

const char *
format_address(const unsigned char *address)
{
    static char buf[4][INET6_ADDRSTRLEN];
    static int i = 0;
    i = (i + 1) % 4;
    if(v4mapped(address))
       inet_ntop(AF_INET, address + 12, buf[i], INET6_ADDRSTRLEN);
    else
       inet_ntop(AF_INET6, address, buf[i], INET6_ADDRSTRLEN);
    return buf[i];
}

const char *
format_prefix(const unsigned char *prefix, unsigned char plen)
{
    static char buf[4][INET6_ADDRSTRLEN + 4];
    static int i = 0;
    int n;
    i = (i + 1) % 4;
    if(plen >= 96 && v4mapped(prefix)) {
        inet_ntop(AF_INET, prefix + 12, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen - 96);
    } else {
        inet_ntop(AF_INET6, prefix, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
    }
    return buf[i];
}

const char *
format_eui64(const unsigned char *eui)
{
    static char buf[4][28];
    static int i = 0;
    i = (i + 1) % 4;
    snprintf(buf[i], 28, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             eui[0], eui[1], eui[2], eui[3],
             eui[4], eui[5], eui[6], eui[7]);
    return buf[i];
}

const char *format_bool(const int b) {
    return b ? "true" : "false";
}

int
parse_address(const char *address, unsigned char *addr_r, int *af_r)
{
    struct in_addr ina;
    struct in6_addr ina6;
    int rc;

    rc = inet_pton(AF_INET, address, &ina);
    if(rc > 0) {
        memcpy(addr_r, v4prefix, 12);
        memcpy(addr_r + 12, &ina, 4);
        if(af_r) *af_r = AF_INET;
        return 0;
    }

    rc = inet_pton(AF_INET6, address, &ina6);
    if(rc > 0) {
        memcpy(addr_r, &ina6, 16);
        if(af_r) *af_r = AF_INET6;
        return 0;
    }

    return -1;
}

int
parse_eui64(const char *eui, unsigned char *eui_r)
{
    int n;
    n = sscanf(eui, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &eui_r[0], &eui_r[1], &eui_r[2], &eui_r[3],
               &eui_r[4], &eui_r[5], &eui_r[6], &eui_r[7]);
    if(n == 8)
        return 0;

    n = sscanf(eui, "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx",
               &eui_r[0], &eui_r[1], &eui_r[2], &eui_r[3],
               &eui_r[4], &eui_r[5], &eui_r[6], &eui_r[7]);
    if(n == 8)
        return 0;

    n = sscanf(eui, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &eui_r[0], &eui_r[1], &eui_r[2],
               &eui_r[5], &eui_r[6], &eui_r[7]);
    if(n == 6) {
        eui_r[3] = 0xFF;
        eui_r[4] = 0xFE;
        return 0;
    }
    return -1;
}

int
wait_for_fd(int direction, int fd, int msecs)
{
    fd_set fds;
    int rc;
    struct timeval tv;

    tv.tv_sec = msecs / 1000;
    tv.tv_usec = (msecs % 1000) * 1000;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if(direction)
        rc = select(fd + 1, NULL, &fds, NULL, &tv);
    else
        rc = select(fd + 1, &fds, NULL, NULL, &tv);

    return rc;
}

int
martian_prefix(const unsigned char *prefix, int plen)
{
    return
        (plen >= 8 && prefix[0] == 0xFF) ||
        (plen >= 10 && prefix[0] == 0xFE && (prefix[1] & 0xC0) == 0x80) ||
        (plen >= 128 && memcmp(prefix, zeroes, 15) == 0 &&
         (prefix[15] == 0 || prefix[15] == 1)) ||
        (plen >= 96 && v4mapped(prefix) &&
         ((plen >= 104 && (prefix[12] == 127 || prefix[12] == 0)) ||
          (plen >= 100 && (prefix[12] & 0xE0) == 0xE0)));
}

int
linklocal(const unsigned char *address)
{
    return memcmp(address, llprefix, 8) == 0;
}

int
v4mapped(const unsigned char *address)
{
    return memcmp(address, v4prefix, 12) == 0;
}

void
v4tov6(unsigned char *dst, const unsigned char *src)
{
    memcpy(dst, v4prefix, 12);
    memcpy(dst + 12, src, 4);
}

void
inaddr_to_uchar(unsigned char *dest, const struct in_addr *src)
{
    memcpy(dest, v4prefix, 12);
    memcpy(dest + 12, src, 4);
    assert(v4mapped(dest));
}

void
uchar_to_inaddr(struct in_addr *dest, const unsigned char *src)
{
    assert(v4mapped(src));
    memcpy(dest, src + 12, 4);
}

void
in6addr_to_uchar(unsigned char *dest, const struct in6_addr *src)
{
    memcpy(dest, src, 16);
}

void
uchar_to_in6addr(struct in6_addr *dest, const unsigned char *src)
{
    memcpy(dest, src, 16);
}

int
daemonise()
{
    int rc;

    fflush(stdout);
    fflush(stderr);

    rc = fork();
    if(rc < 0)
        return -1;

    if(rc > 0)
        exit(0);

    rc = setsid();
    if(rc < 0)
        return -1;

    return 1;
}
