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

#include "babeld.h"
#include "babel_main.h"
#include "log.h"

#if defined(i386) || defined(__mc68020__) || defined(__x86_64__)
#define DO_NTOHS(_d, _s) do{ _d = ntohs(*(const unsigned short*)(_s)); }while(0)
#define DO_NTOHL(_d, _s) do{ _d = ntohl(*(const unsigned*)(_s)); } while(0)
#define DO_HTONS(_d, _s) do{ *(unsigned short*)(_d) = htons(_s); } while(0)
#define DO_HTONL(_d, _s) do{ *(unsigned*)(_d) = htonl(_s); } while(0)
/* Some versions of gcc seem to be buggy, and ignore the packed attribute.
   Disable this code until the issue is clarified. */
/* #elif defined __GNUC__*/
#elif 0
struct __us { unsigned short x __attribute__((packed)); };
#define DO_NTOHS(_d, _s) \
    do { _d = ntohs(((const struct __us*)(_s))->x); } while(0)
#define DO_HTONS(_d, _s) \
    do { ((struct __us*)(_d))->x = htons(_s); } while(0)
#else
#define DO_NTOHS(_d, _s) \
    do { short _dd; \
         memcpy(&(_dd), (_s), 2); \
         _d = ntohs(_dd); } while(0)
#define DO_HTONS(_d, _s) \
    do { unsigned short _dd; \
         _dd = htons(_s); \
         memcpy((_d), &(_dd), 2); } while(0)
#endif

static inline int
seqno_compare(unsigned short s1, unsigned short s2)
{
    if(s1 == s2)
        return 0;
    else
        return ((s2 - s1) & 0x8000) ? 1 : -1;
}

static inline short
seqno_minus(unsigned short s1, unsigned short s2)
{
    return (short)((s1 - s2) & 0xFFFF);
}

static inline unsigned short
seqno_plus(unsigned short s, int plus)
{
    return ((s + plus) & 0xFFFF);
}

unsigned roughly(unsigned value);
void timeval_minus(struct timeval *d,
                   const struct timeval *s1, const struct timeval *s2);
unsigned timeval_minus_msec(const struct timeval *s1, const struct timeval *s2)
    ATTRIBUTE ((pure));
void timeval_add_msec(struct timeval *d,
                      const struct timeval *s, const int msecs);
void set_timeout (struct timeval *timeout, int msecs);
int timeval_compare(const struct timeval *s1, const struct timeval *s2)
    ATTRIBUTE ((pure));
void timeval_min(struct timeval *d, const struct timeval *s);
void timeval_min_sec(struct timeval *d, time_t secs);
int parse_msec(const char *string) ATTRIBUTE ((pure));
int in_prefix(const unsigned char *restrict address,
              const unsigned char *restrict prefix, unsigned char plen)
    ATTRIBUTE ((pure));
unsigned char *mask_prefix(unsigned char *restrict ret,
                           const unsigned char *restrict prefix,
                           unsigned char plen);
const char *format_address(const unsigned char *address);
const char *format_prefix(const unsigned char *address, unsigned char prefix);
const char *format_eui64(const unsigned char *eui);
const char *format_bool(const int b);
int parse_address(const char *address, unsigned char *addr_r, int *af_r);
int parse_eui64(const char *eui, unsigned char *eui_r);
int wait_for_fd(int direction, int fd, int msecs);
int martian_prefix(const unsigned char *prefix, int plen) ATTRIBUTE ((pure));
int linklocal(const unsigned char *address) ATTRIBUTE ((pure));
int v4mapped(const unsigned char *address) ATTRIBUTE ((pure));
void v4tov6(unsigned char *dst, const unsigned char *src);
void inaddr_to_uchar(unsigned char *dest, const struct in_addr *src);
void uchar_to_inaddr(struct in_addr *dest, const unsigned char *src);
void in6addr_to_uchar(unsigned char *dest, const struct in6_addr *src);
void uchar_to_in6addr(struct in6_addr *dest, const unsigned char *src);
int daemonise(void);

/* If debugging is disabled, we want to avoid calling format_address
   for every omitted debugging message.  So debug is a macro.  But
   vararg macros are not portable. */
#if defined NO_DEBUG

#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#define debugf(...) do {} while(0)
#elif defined __GNUC__
#define debugf(_args...) do {} while(0)
#else
static inline void debugf(int level, const char *format, ...) { return; }
#endif

#else /* NO_DEBUG */

/* some levels */
#define BABEL_DEBUG_COMMON      (1 << 0)
#define BABEL_DEBUG_KERNEL      (1 << 1)
#define BABEL_DEBUG_FILTER      (1 << 2)
#define BABEL_DEBUG_TIMEOUT     (1 << 3)
#define BABEL_DEBUG_IF          (1 << 4)
#define BABEL_DEBUG_ROUTE       (1 << 5)
#define BABEL_DEBUG_ALL         (0xFFFF)

#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#define debugf(level, ...) \
do { \
if(UNLIKELY(debug & level)) zlog_debug(__VA_ARGS__);     \
} while(0)
#elif defined __GNUC__
#define debugf(level, _args...) \
do { \
if(UNLIKELY(debug & level)) zlog_debug(_args);   \
} while(0)
#else
static inline void debugf(int level, const char *format, ...) { return; }
#endif

#endif /* NO_DEBUG */

