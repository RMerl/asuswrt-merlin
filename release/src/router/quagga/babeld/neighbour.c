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
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <zebra.h>
#include "if.h"

#include "babel_main.h"
#include "babeld.h"
#include "util.h"
#include "babel_interface.h"
#include "neighbour.h"
#include "source.h"
#include "route.h"
#include "message.h"
#include "resend.h"

struct neighbour *neighs = NULL;

static struct neighbour *
find_neighbour_nocreate(const unsigned char *address, struct interface *ifp)
{
    struct neighbour *neigh;
    FOR_ALL_NEIGHBOURS(neigh) {
        if(memcmp(address, neigh->address, 16) == 0 &&
           neigh->ifp == ifp)
            return neigh;
    }
    return NULL;
}

void
flush_neighbour(struct neighbour *neigh)
{
    flush_neighbour_routes(neigh);
    if(unicast_neighbour == neigh)
        flush_unicast(1);
    flush_resends(neigh);

    if(neighs == neigh) {
        neighs = neigh->next;
    } else {
        struct neighbour *previous = neighs;
        while(previous->next != neigh)
            previous = previous->next;
        previous->next = neigh->next;
    }
    free(neigh);
}

struct neighbour *
find_neighbour(const unsigned char *address, struct interface *ifp)
{
    struct neighbour *neigh;
    const struct timeval zero = {0, 0};

    neigh = find_neighbour_nocreate(address, ifp);
    if(neigh)
        return neigh;

    debugf(BABEL_DEBUG_COMMON,"Creating neighbour %s on %s.",
           format_address(address), ifp->name);

    neigh = malloc(sizeof(struct neighbour));
    if(neigh == NULL) {
        zlog_err("malloc(neighbour): %s", safe_strerror(errno));
        return NULL;
    }

    neigh->hello_seqno = -1;
    memcpy(neigh->address, address, 16);
    neigh->reach = 0;
    neigh->txcost = INFINITY;
    neigh->ihu_time = babel_now;
    neigh->hello_time = zero;
    neigh->hello_interval = 0;
    neigh->ihu_interval = 0;
    neigh->ifp = ifp;
    neigh->next = neighs;
    neighs = neigh;
    send_hello(ifp);
    return neigh;
}

/* Recompute a neighbour's rxcost.  Return true if anything changed.
   This does not call local_notify_neighbour, see update_neighbour_metric. */
int
update_neighbour(struct neighbour *neigh, int hello, int hello_interval)
{
    int missed_hellos;
    int rc = 0;

    if(hello < 0) {
        if(neigh->hello_interval <= 0)
            return rc;
        missed_hellos =
            ((int)timeval_minus_msec(&babel_now, &neigh->hello_time) -
             neigh->hello_interval * 7) /
            (neigh->hello_interval * 10);
        if(missed_hellos <= 0)
            return rc;
        timeval_add_msec(&neigh->hello_time, &neigh->hello_time,
                          missed_hellos * neigh->hello_interval * 10);
    } else {
        if(neigh->hello_seqno >= 0 && neigh->reach > 0) {
            missed_hellos = seqno_minus(hello, neigh->hello_seqno) - 1;
            if(missed_hellos < -8) {
                /* Probably a neighbour that rebooted and lost its seqno.
                   Reboot the universe. */
                neigh->reach = 0;
                missed_hellos = 0;
                rc = 1;
            } else if(missed_hellos < 0) {
                if(hello_interval > neigh->hello_interval) {
                    /* This neighbour has increased its hello interval,
                       and we didn't notice. */
                    neigh->reach <<= -missed_hellos;
                    missed_hellos = 0;
                } else {
                    /* Late hello.  Probably due to the link layer buffering
                       packets during a link outage.  Ignore it, but reset
                       the expected seqno. */
                    neigh->hello_seqno = hello;
                    hello = -1;
                    missed_hellos = 0;
                }
                rc = 1;
            }
        } else {
            missed_hellos = 0;
        }
        neigh->hello_time = babel_now;
        neigh->hello_interval = hello_interval;
    }

    if(missed_hellos > 0) {
        neigh->reach >>= missed_hellos;
        neigh->hello_seqno = seqno_plus(neigh->hello_seqno, missed_hellos);
        missed_hellos = 0;
        rc = 1;
    }

    if(hello >= 0) {
        neigh->hello_seqno = hello;
        neigh->reach >>= 1;
        neigh->reach |= 0x8000;
        if((neigh->reach & 0xFC00) != 0xFC00)
            rc = 1;
    }

    /* Make sure to give neighbours some feedback early after association */
    if((neigh->reach & 0xBF00) == 0x8000) {
        /* A new neighbour */
        send_hello(neigh->ifp);
    } else {
        /* Don't send hellos, in order to avoid a positive feedback loop. */
        int a = (neigh->reach & 0xC000);
        int b = (neigh->reach & 0x3000);
        if((a == 0xC000 && b == 0) || (a == 0 && b == 0x3000)) {
            /* Reachability is either 1100 or 0011 */
            send_self_update(neigh->ifp);
        }
    }

    if((neigh->reach & 0xFC00) == 0xC000) {
        /* This is a newish neighbour, let's request a full route dump.
           We ought to avoid this when the network is dense */
        send_unicast_request(neigh, NULL, 0);
        send_ihu(neigh, NULL);
    }
    return rc;
}

static int
reset_txcost(struct neighbour *neigh)
{
    unsigned delay;

    delay = timeval_minus_msec(&babel_now, &neigh->ihu_time);

    if(neigh->ihu_interval > 0 && delay < neigh->ihu_interval * 10U * 3U)
        return 0;

    /* If we're losing a lot of packets, we probably lost an IHU too */
    if(delay >= 180000 || (neigh->reach & 0xFFF0) == 0 ||
       (neigh->ihu_interval > 0 &&
        delay >= neigh->ihu_interval * 10U * 10U)) {
        neigh->txcost = INFINITY;
        neigh->ihu_time = babel_now;
        return 1;
    }

    return 0;
}

unsigned
neighbour_txcost(struct neighbour *neigh)
{
    return neigh->txcost;
}

unsigned
check_neighbours()
{
    struct neighbour *neigh;
    int changed, rc;
    unsigned msecs = 50000;

    debugf(BABEL_DEBUG_COMMON,"Checking neighbours.");

    neigh = neighs;
    while(neigh) {
        changed = update_neighbour(neigh, -1, 0);

        if(neigh->reach == 0 ||
           neigh->hello_time.tv_sec > babel_now.tv_sec || /* clock stepped */
           timeval_minus_msec(&babel_now, &neigh->hello_time) > 300000) {
            struct neighbour *old = neigh;
            neigh = neigh->next;
            flush_neighbour(old);
            continue;
        }

        rc = reset_txcost(neigh);
        changed = changed || rc;

        update_neighbour_metric(neigh, changed);

        if(neigh->hello_interval > 0)
            msecs = MIN(msecs, neigh->hello_interval * 10U);
        if(neigh->ihu_interval > 0)
            msecs = MIN(msecs, neigh->ihu_interval * 10U);
        neigh = neigh->next;
    }

    return msecs;
}

unsigned
neighbour_rxcost(struct neighbour *neigh)
{
    unsigned delay;
    unsigned short reach = neigh->reach;

    delay = timeval_minus_msec(&babel_now, &neigh->hello_time);

    if((reach & 0xFFF0) == 0 || delay >= 180000) {
        return INFINITY;
    } else if(babel_get_if_nfo(neigh->ifp)->flags & BABEL_IF_LQ) {
        int sreach =
            ((reach & 0x8000) >> 2) +
            ((reach & 0x4000) >> 1) +
            (reach & 0x3FFF);
        /* 0 <= sreach <= 0x7FFF */
        int cost = (0x8000 * babel_get_if_nfo(neigh->ifp)->cost) / (sreach + 1);
        /* cost >= interface->cost */
        if(delay >= 40000)
            cost = (cost * (delay - 20000) + 10000) / 20000;
        return MIN(cost, INFINITY);
    } else {
        /* To lose one hello is a misfortune, to lose two is carelessness. */
        if((reach & 0xC000) == 0xC000)
            return babel_get_if_nfo(neigh->ifp)->cost;
        else if((reach & 0xC000) == 0)
            return INFINITY;
        else if((reach & 0x2000))
            return babel_get_if_nfo(neigh->ifp)->cost;
        else
            return INFINITY;
    }
}

unsigned
neighbour_cost(struct neighbour *neigh)
{
    unsigned a, b;

    if(!if_up(neigh->ifp))
        return INFINITY;

    a = neighbour_txcost(neigh);

    if(a >= INFINITY)
        return INFINITY;

    b = neighbour_rxcost(neigh);
    if(b >= INFINITY)
        return INFINITY;

    if(!(babel_get_if_nfo(neigh->ifp)->flags & BABEL_IF_LQ)
       || (a < 256 && b < 256)) {
        return a;
    } else {
        /* a = 256/alpha, b = 256/beta, where alpha and beta are the expected
           probabilities of a packet getting through in the direct and reverse
           directions. */
        a = MAX(a, 256);
        b = MAX(b, 256);
        /* 1/(alpha * beta), which is just plain ETX. */
        /* Since a and b are capped to 16 bits, overflow is impossible. */
        return (a * b + 128) >> 8;
    }
}
