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

#include <zebra.h>
#include "if.h"
#include "log.h"

#include "babeld.h"
#include "kernel.h"
#include "neighbour.h"
#include "message.h"
#include "route.h"
#include "xroute.h"
#include "util.h"
#include "babel_interface.h"

static int xroute_add_new_route(unsigned char prefix[16], unsigned char plen,
                                unsigned short metric, unsigned int ifindex,
                                int proto, int send_updates);

static struct xroute *xroutes;
static int numxroutes = 0, maxxroutes = 0;

/* Add redistributed route to Babel table. */
int
babel_ipv4_route_add (struct zapi_ipv4 *api, struct prefix_ipv4 *prefix,
                      unsigned int ifindex, struct in_addr *nexthop)
{
    unsigned char uchar_prefix[16];

    inaddr_to_uchar(uchar_prefix, &prefix->prefix);
    debugf(BABEL_DEBUG_ROUTE, "Adding new ipv4 route comming from Zebra.");
    xroute_add_new_route(uchar_prefix, prefix->prefixlen + 96,
                         api->metric, ifindex, 0, 1);
    return 0;
}

/* Remove redistributed route from Babel table. */
int
babel_ipv4_route_delete (struct zapi_ipv4 *api, struct prefix_ipv4 *prefix,
                         unsigned int ifindex)
{
    unsigned char uchar_prefix[16];
    struct xroute *xroute = NULL;

    inaddr_to_uchar(uchar_prefix, &prefix->prefix);
    xroute = find_xroute(uchar_prefix, prefix->prefixlen + 96);
    if (xroute != NULL) {
        debugf(BABEL_DEBUG_ROUTE, "Removing ipv4 route (from zebra).");
        flush_xroute(xroute);
    }
    return 0;
}

/* Add redistributed route to Babel table. */
int
babel_ipv6_route_add (struct zapi_ipv6 *api, struct prefix_ipv6 *prefix,
                      unsigned int ifindex, struct in6_addr *nexthop)
{
    unsigned char uchar_prefix[16];

    in6addr_to_uchar(uchar_prefix, &prefix->prefix);
    debugf(BABEL_DEBUG_ROUTE, "Adding new route comming from Zebra.");
    xroute_add_new_route(uchar_prefix, prefix->prefixlen, api->metric, ifindex,
                         0, 1);
    return 0;
}

/* Remove redistributed route from Babel table. */
int
babel_ipv6_route_delete (struct zapi_ipv6 *api, struct prefix_ipv6 *prefix,
                         unsigned int ifindex)
{
    unsigned char uchar_prefix[16];
    struct xroute *xroute = NULL;

    in6addr_to_uchar(uchar_prefix, &prefix->prefix);
    xroute = find_xroute(uchar_prefix, prefix->prefixlen);
    if (xroute != NULL) {
        debugf(BABEL_DEBUG_ROUTE, "Removing route (from zebra).");
        flush_xroute(xroute);
    }
    return 0;
}

struct xroute *
find_xroute(const unsigned char *prefix, unsigned char plen)
{
    int i;
    for(i = 0; i < numxroutes; i++) {
        if(xroutes[i].plen == plen &&
           memcmp(xroutes[i].prefix, prefix, 16) == 0)
            return &xroutes[i];
    }
    return NULL;
}

void
flush_xroute(struct xroute *xroute)
{
    int i;

    i = xroute - xroutes;
    assert(i >= 0 && i < numxroutes);

    if(i != numxroutes - 1)
        memcpy(xroutes + i, xroutes + numxroutes - 1, sizeof(struct xroute));
    numxroutes--;
    VALGRIND_MAKE_MEM_UNDEFINED(xroutes + numxroutes, sizeof(struct xroute));

    if(numxroutes == 0) {
        free(xroutes);
        xroutes = NULL;
        maxxroutes = 0;
    } else if(maxxroutes > 8 && numxroutes < maxxroutes / 4) {
        struct xroute *new_xroutes;
        int n = maxxroutes / 2;
        new_xroutes = realloc(xroutes, n * sizeof(struct xroute));
        if(new_xroutes == NULL)
            return;
        xroutes = new_xroutes;
        maxxroutes = n;
    }
}

static int
add_xroute(unsigned char prefix[16], unsigned char plen,
           unsigned short metric, unsigned int ifindex, int proto)
{
    struct xroute *xroute = find_xroute(prefix, plen);
    if(xroute) {
        if(xroute->metric <= metric)
            return 0;
        xroute->metric = metric;
        return 1;
    }

    if(numxroutes >= maxxroutes) {
        struct xroute *new_xroutes;
        int n = maxxroutes < 1 ? 8 : 2 * maxxroutes;
        new_xroutes = xroutes == NULL ?
            malloc(n * sizeof(struct xroute)) :
            realloc(xroutes, n * sizeof(struct xroute));
        if(new_xroutes == NULL)
            return -1;
        maxxroutes = n;
        xroutes = new_xroutes;
    }

    memcpy(xroutes[numxroutes].prefix, prefix, 16);
    xroutes[numxroutes].plen = plen;
    xroutes[numxroutes].metric = metric;
    xroutes[numxroutes].ifindex = ifindex;
    xroutes[numxroutes].proto = proto;
    numxroutes++;
    return 1;
}

/* Returns an overestimate of the number of xroutes. */
int
xroutes_estimate()
{
    return numxroutes;
}

void
for_all_xroutes(void (*f)(struct xroute*, void*), void *closure)
{
    int i;

    for(i = 0; i < numxroutes; i++)
        (*f)(&xroutes[i], closure);
}

/* add an xroute, verifying some conditions; return 0 if there is no changes */
static int
xroute_add_new_route(unsigned char prefix[16], unsigned char plen,
                     unsigned short metric, unsigned int ifindex,
                     int proto, int send_updates)
{
    int rc;
    if(martian_prefix(prefix, plen))
        return 0;
    metric = redistribute_filter(prefix, plen, ifindex, proto);
    if(metric < INFINITY) {
        rc = add_xroute(prefix, plen, metric, ifindex, proto);
        if(rc > 0) {
            struct babel_route *route;
            route = find_installed_route(prefix, plen);
            if(route) {
                if(allow_duplicates < 0 ||
                   metric < allow_duplicates)
                    uninstall_route(route);
            }
            if(send_updates)
                send_update(NULL, 0, prefix, plen);
            return 1;
        }
    }
    return 0;
}
