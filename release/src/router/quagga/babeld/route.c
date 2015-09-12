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

#include "babeld.h"
#include "util.h"
#include "kernel.h"
#include "babel_interface.h"
#include "source.h"
#include "neighbour.h"
#include "route.h"
#include "xroute.h"
#include "message.h"
#include "resend.h"

static void consider_route(struct babel_route *route);

struct babel_route **routes = NULL;
static int route_slots = 0, max_route_slots = 0;
int kernel_metric = 0;
int allow_duplicates = -1;
int diversity_kind = DIVERSITY_NONE;
int diversity_factor = 256;     /* in units of 1/256 */
int keep_unfeasible = 0;

/* We maintain a list of "slots", ordered by prefix.  Every slot
   contains a linked list of the routes to this prefix, with the
   installed route, if any, at the head of the list. */

static int
route_compare(const unsigned char *prefix, unsigned char plen,
               struct babel_route *route)
{
    int i = memcmp(prefix, route->src->prefix, 16);
    if(i != 0)
        return i;

    if(plen < route->src->plen)
        return -1;
    else if(plen > route->src->plen)
        return 1;
    else
        return 0;
}

/* Performs binary search, returns -1 in case of failure.  In the latter
   case, new_return is the place where to insert the new element. */

static int
find_route_slot(const unsigned char *prefix, unsigned char plen,
                int *new_return)
{
    int p, m, g, c;

    if(route_slots < 1) {
        if(new_return)
            *new_return = 0;
        return -1;
    }

    p = 0; g = route_slots - 1;

    do {
        m = (p + g) / 2;
        c = route_compare(prefix, plen, routes[m]);
        if(c == 0)
            return m;
        else if(c < 0)
            g = m - 1;
        else
            p = m + 1;
    } while(p <= g);

    if(new_return)
        *new_return = p;

    return -1;
}

struct babel_route *
find_route(const unsigned char *prefix, unsigned char plen,
           struct neighbour *neigh, const unsigned char *nexthop)
{
    struct babel_route *route;
    int i = find_route_slot(prefix, plen, NULL);

    if(i < 0)
        return NULL;

    route = routes[i];

    while(route) {
        if(route->neigh == neigh && memcmp(route->nexthop, nexthop, 16) == 0)
            return route;
        route = route->next;
    }

    return NULL;
}

struct babel_route *
find_installed_route(const unsigned char *prefix, unsigned char plen)
{
    int i = find_route_slot(prefix, plen, NULL);

    if(i >= 0 && routes[i]->installed)
        return routes[i];

    return NULL;
}

/* Returns an overestimate of the number of installed routes. */
int
installed_routes_estimate(void)
{
    return route_slots;
}

static int
resize_route_table(int new_slots)
{
    struct babel_route **new_routes;
    assert(new_slots >= route_slots);

    if(new_slots == 0) {
        new_routes = NULL;
        free(routes);
    } else {
        new_routes = realloc(routes, new_slots * sizeof(struct babel_route*));
        if(new_routes == NULL)
            return -1;
    }

    max_route_slots = new_slots;
    routes = new_routes;
    return 1;
}

/* Insert a route into the table.  If successful, retains the route.
   On failure, caller must free the route. */
static struct babel_route *
insert_route(struct babel_route *route)
{
    int i, n;

    assert(!route->installed);

    i = find_route_slot(route->src->prefix, route->src->plen, &n);

    if(i < 0) {
        if(route_slots >= max_route_slots)
            resize_route_table(max_route_slots < 1 ? 8 : 2 * max_route_slots);
        if(route_slots >= max_route_slots)
            return NULL;
        route->next = NULL;
        if(n < route_slots)
            memmove(routes + n + 1, routes + n,
                    (route_slots - n) * sizeof(struct babel_route*));
        route_slots++;
        routes[n] = route;
    } else {
        struct babel_route *r;
        r = routes[i];
        while(r->next)
            r = r->next;
        r->next = route;
        route->next = NULL;
    }

    return route;
}

void
flush_route(struct babel_route *route)
{
    int i;
    struct source *src;
    unsigned oldmetric;
    int lost = 0;

    oldmetric = route_metric(route);
    src = route->src;

    if(route->installed) {
        uninstall_route(route);
        lost = 1;
    }

    i = find_route_slot(route->src->prefix, route->src->plen, NULL);
    assert(i >= 0 && i < route_slots);

    if(route == routes[i]) {
        routes[i] = route->next;
        route->next = NULL;
        free(route);

        if(routes[i] == NULL) {
            if(i < route_slots - 1)
                memmove(routes + i, routes + i + 1,
                        (route_slots - i - 1) * sizeof(struct babel_route*));
            routes[route_slots - 1] = NULL;
            route_slots--;
        }

        if(route_slots == 0)
            resize_route_table(0);
        else if(max_route_slots > 8 && route_slots < max_route_slots / 4)
            resize_route_table(max_route_slots / 2);
    } else {
        struct babel_route *r = routes[i];
        while(r->next != route)
            r = r->next;
        r->next = route->next;
        route->next = NULL;
        free(route);
    }

    if(lost)
        route_lost(src, oldmetric);

    release_source(src);
}

void
flush_all_routes()
{
    int i;

    /* Start from the end, to avoid shifting the table. */
    i = route_slots - 1;
    while(i >= 0) {
        while(i < route_slots) {
        /* Uninstall first, to avoid calling route_lost. */
            if(routes[i]->installed)
                uninstall_route(routes[0]);
            flush_route(routes[i]);
        }
        i--;
    }

    check_sources_released();
}

void
flush_neighbour_routes(struct neighbour *neigh)
{
    int i;

    i = 0;
    while(i < route_slots) {
        struct babel_route *r;
        r = routes[i];
        while(r) {
            if(r->neigh == neigh) {
                flush_route(r);
                goto again;
            }
            r = r->next;
        }
        i++;
    again:
        ;
    }
}

void
flush_interface_routes(struct interface *ifp, int v4only)
{
    int i;

    i = 0;
    while(i < route_slots) {
        struct babel_route *r;
        r = routes[i];
        while(r) {
            if(r->neigh->ifp == ifp &&
               (!v4only || v4mapped(r->nexthop))) {
                flush_route(r);
                goto again;
            }
            r = r->next;
        }
        i++;
    again:
        ;
    }
}

/* Iterate a function over all routes. */
void
for_all_routes(void (*f)(struct babel_route*, void*), void *closure)
{
    int i;

    for(i = 0; i < route_slots; i++) {
        struct babel_route *r = routes[i];
        while(r) {
            (*f)(r, closure);
            r = r->next;
        }
    }
}

void
for_all_installed_routes(void (*f)(struct babel_route*, void*), void *closure)
{
    int i;

    for(i = 0; i < route_slots; i++) {
        if(routes[i]->installed)
            (*f)(routes[i], closure);
    }
}

static int
metric_to_kernel(int metric)
{
    return metric < INFINITY ? kernel_metric : KERNEL_INFINITY;
}

/* This is used to maintain the invariant that the installed route is at
   the head of the list. */
static void
move_installed_route(struct babel_route *route, int i)
{
    assert(i >= 0 && i < route_slots);
    assert(route->installed);

    if(route != routes[i]) {
        struct babel_route *r = routes[i];
        while(r->next != route)
            r = r->next;
        r->next = route->next;
        route->next = routes[i];
        routes[i] = route;
    }
}

void
install_route(struct babel_route *route)
{
    int i, rc;

    if(route->installed)
        return;

    if(!route_feasible(route))
        zlog_err("WARNING: installing unfeasible route "
                 "(this shouldn't happen).");

    i = find_route_slot(route->src->prefix, route->src->plen, NULL);
    assert(i >= 0 && i < route_slots);

    if(routes[i] != route && routes[i]->installed) {
        fprintf(stderr, "WARNING: attempting to install duplicate route "
                "(this shouldn't happen).");
        return;
    }

    rc = kernel_route(ROUTE_ADD, route->src->prefix, route->src->plen,
                      route->nexthop,
                      route->neigh->ifp->ifindex,
                      metric_to_kernel(route_metric(route)), NULL, 0, 0);
    if(rc < 0) {
        int save = errno;
        zlog_err("kernel_route(ADD): %s", safe_strerror(errno));
        if(save != EEXIST)
            return;
    }
    route->installed = 1;
    move_installed_route(route, i);

}

void
uninstall_route(struct babel_route *route)
{
    int rc;

    if(!route->installed)
        return;

    rc = kernel_route(ROUTE_FLUSH, route->src->prefix, route->src->plen,
                      route->nexthop,
                      route->neigh->ifp->ifindex,
                      metric_to_kernel(route_metric(route)), NULL, 0, 0);
    if(rc < 0)
        zlog_err("kernel_route(FLUSH): %s", safe_strerror(errno));

    route->installed = 0;
}

/* This is equivalent to uninstall_route followed with install_route,
   but without the race condition.  The destination of both routes
   must be the same. */

static void
switch_routes(struct babel_route *old, struct babel_route *new)
{
    int rc;

    if(!old) {
        install_route(new);
        return;
    }

    if(!old->installed)
        return;

    if(!route_feasible(new))
        zlog_err("WARNING: switching to unfeasible route "
                 "(this shouldn't happen).");

    rc = kernel_route(ROUTE_MODIFY, old->src->prefix, old->src->plen,
                      old->nexthop, old->neigh->ifp->ifindex,
                      metric_to_kernel(route_metric(old)),
                      new->nexthop, new->neigh->ifp->ifindex,
                      metric_to_kernel(route_metric(new)));
    if(rc < 0) {
        zlog_err("kernel_route(MODIFY): %s", safe_strerror(errno));
        return;
    }

    old->installed = 0;
    new->installed = 1;
    move_installed_route(new, find_route_slot(new->src->prefix, new->src->plen,
                                              NULL));
}

static void
change_route_metric(struct babel_route *route,
                    unsigned refmetric, unsigned cost, unsigned add)
{
    int old, new;
    int newmetric = MIN(refmetric + cost + add, INFINITY);

    old = metric_to_kernel(route_metric(route));
    new = metric_to_kernel(newmetric);

    if(route->installed && old != new) {
        int rc;
        rc = kernel_route(ROUTE_MODIFY, route->src->prefix, route->src->plen,
                          route->nexthop, route->neigh->ifp->ifindex,
                          old,
                          route->nexthop, route->neigh->ifp->ifindex,
                          new);
        if(rc < 0) {
            zlog_err("kernel_route(MODIFY metric): %s", safe_strerror(errno));
            return;
        }
    }

    route->refmetric = refmetric;
    route->cost = cost;
    route->add_metric = add;
}

static void
retract_route(struct babel_route *route)
{
    change_route_metric(route, INFINITY, INFINITY, 0);
}

int
route_feasible(struct babel_route *route)
{
    return update_feasible(route->src, route->seqno, route->refmetric);
}

int
route_old(struct babel_route *route)
{
    return route->time < babel_now.tv_sec - route->hold_time * 7 / 8;
}

int
route_expired(struct babel_route *route)
{
    return route->time < babel_now.tv_sec - route->hold_time;
}

static int
channels_interfere(int ch1, int ch2)
{
    if(ch1 == BABEL_IF_CHANNEL_NONINTERFERING
       || ch2 == BABEL_IF_CHANNEL_NONINTERFERING)
        return 0;
    if(ch1 == BABEL_IF_CHANNEL_INTERFERING
       || ch2 == BABEL_IF_CHANNEL_INTERFERING)
        return 1;
    return ch1 == ch2;
}

int
route_interferes(struct babel_route *route, struct interface *ifp)
{
    struct babel_interface *babel_ifp = NULL;
    switch(diversity_kind) {
    case DIVERSITY_NONE:
        return 1;
    case DIVERSITY_INTERFACE_1:
        return route->neigh->ifp == ifp;
    case DIVERSITY_CHANNEL_1:
    case DIVERSITY_CHANNEL:
        if(route->neigh->ifp == ifp)
            return 1;
        babel_ifp = babel_get_if_nfo(ifp);
        if(channels_interfere(babel_ifp->channel,
                              babel_get_if_nfo(route->neigh->ifp)->channel))
            return 1;
        if(diversity_kind == DIVERSITY_CHANNEL) {
            int i;
            for(i = 0; i < DIVERSITY_HOPS; i++) {
                if(route->channels[i] == 0)
                    break;
                if(channels_interfere(babel_ifp->channel, route->channels[i]))
                    return 1;
            }
        }
        return 0;
    default:
        fprintf(stderr, "Unknown kind of diversity.\n");
        return 1;
    }
}

int
update_feasible(struct source *src,
                unsigned short seqno, unsigned short refmetric)
{
    if(src == NULL)
        return 1;

    if(src->time < babel_now.tv_sec - SOURCE_GC_TIME)
        /* Never mind what is probably stale data */
        return 1;

    if(refmetric >= INFINITY)
        /* Retractions are always feasible */
        return 1;

    return (seqno_compare(seqno, src->seqno) > 0 ||
            (src->seqno == seqno && refmetric < src->metric));
}

/* This returns the feasible route with the smallest metric. */
struct babel_route *
find_best_route(const unsigned char *prefix, unsigned char plen, int feasible,
                struct neighbour *exclude)
{
    struct babel_route *route = NULL, *r = NULL;
    int i = find_route_slot(prefix, plen, NULL);

    if(i < 0)
        return NULL;

    route = routes[i];

    r = route->next;
    while(r) {
        if(!route_expired(r) &&
           (!feasible || route_feasible(r)) &&
           (!exclude || r->neigh != exclude) &&
           (route_metric(r) < route_metric(route)))
            route = r;
        r = r->next;
    }
    return route;
}

void
update_route_metric(struct babel_route *route)
{
    int oldmetric = route_metric(route);

    if(route_expired(route)) {
        if(route->refmetric < INFINITY) {
            route->seqno = seqno_plus(route->src->seqno, 1);
            retract_route(route);
            if(oldmetric < INFINITY)
                route_changed(route, route->src, oldmetric);
        }
    } else {
        struct neighbour *neigh = route->neigh;
        int add_metric = input_filter(route->src->id,
                                      route->src->prefix, route->src->plen,
                                      neigh->address,
                                      neigh->ifp->ifindex);
        change_route_metric(route, route->refmetric,
                            neighbour_cost(route->neigh), add_metric);
        if(route_metric(route) != oldmetric)
            route_changed(route, route->src, oldmetric);
    }
}

/* Called whenever a neighbour's cost changes, to update the metric of
   all routes through that neighbour.  Calls local_notify_neighbour. */
void
update_neighbour_metric(struct neighbour *neigh, int changed)
{

    if(changed) {
        int i;

        for(i = 0; i < route_slots; i++) {
            struct babel_route *r = routes[i];
            while(r) {
                if(r->neigh == neigh)
                    update_route_metric(r);
                r = r->next;
            }
        }
    }
}

void
update_interface_metric(struct interface *ifp)
{
    int i;

    for(i = 0; i < route_slots; i++) {
        struct babel_route *r = routes[i];
        while(r) {
            if(r->neigh->ifp == ifp)
                update_route_metric(r);
            r = r->next;
        }
    }
}

/* This is called whenever we receive an update. */
struct babel_route *
update_route(const unsigned char *router_id,
             const unsigned char *prefix, unsigned char plen,
             unsigned short seqno, unsigned short refmetric,
             unsigned short interval,
             struct neighbour *neigh, const unsigned char *nexthop,
             const unsigned char *channels, int channels_len)
{
    struct babel_route *route;
    struct source *src;
    int metric, feasible;
    int add_metric;
    int hold_time = MAX((4 * interval) / 100 + interval / 50, 15);

    if(memcmp(router_id, myid, 8) == 0)
        return NULL;

    if(martian_prefix(prefix, plen)) {
        zlog_err("Rejecting martian route to %s through %s.",
                 format_prefix(prefix, plen), format_address(router_id));
        return NULL;
    }

    add_metric = input_filter(router_id, prefix, plen,
                              neigh->address, neigh->ifp->ifindex);
    if(add_metric >= INFINITY)
        return NULL;

    route = find_route(prefix, plen, neigh, nexthop);

    if(route && memcmp(route->src->id, router_id, 8) == 0)
        /* Avoid scanning the source table. */
        src = route->src;
    else
        src = find_source(router_id, prefix, plen, 1, seqno);

    if(src == NULL)
        return NULL;

    feasible = update_feasible(src, seqno, refmetric);
    metric = MIN((int)refmetric + neighbour_cost(neigh) + add_metric, INFINITY);

    if(route) {
        struct source *oldsrc;
        unsigned short oldmetric;
        int lost = 0;

        oldsrc = route->src;
        oldmetric = route_metric(route);

        /* If a successor switches sources, we must accept his update even
           if it makes a route unfeasible in order to break any routing loops
           in a timely manner.  If the source remains the same, we ignore
           the update. */
        if(!feasible && route->installed) {
            debugf(BABEL_DEBUG_COMMON,"Unfeasible update for installed route to %s "
                   "(%s %d %d -> %s %d %d).",
                   format_prefix(src->prefix, src->plen),
                   format_address(route->src->id),
                   route->seqno, route->refmetric,
                   format_address(src->id), seqno, refmetric);
            if(src != route->src) {
                uninstall_route(route);
                lost = 1;
            }
        }

        route->src = retain_source(src);
        if((feasible || keep_unfeasible) && refmetric < INFINITY)
            route->time = babel_now.tv_sec;
        route->seqno = seqno;
        change_route_metric(route,
                            refmetric, neighbour_cost(neigh), add_metric);
        route->hold_time = hold_time;

        route_changed(route, oldsrc, oldmetric);
        if(lost)
            route_lost(oldsrc, oldmetric);

        if(!feasible)
            send_unfeasible_request(neigh, route->installed && route_old(route),
                                    seqno, metric, src);
        release_source(oldsrc);
    } else {
        struct babel_route *new_route;

        if(refmetric >= INFINITY)
            /* Somebody's retracting a route we never saw. */
            return NULL;
        if(!feasible) {
            send_unfeasible_request(neigh, 0, seqno, metric, src);
            if(!keep_unfeasible)
                return NULL;
        }

        route = malloc(sizeof(struct babel_route));
        if(route == NULL) {
            perror("malloc(route)");
            return NULL;
        }

        route->src = retain_source(src);
        route->refmetric = refmetric;
        route->cost = neighbour_cost(neigh);
        route->add_metric = add_metric;
        route->seqno = seqno;
        route->neigh = neigh;
        memcpy(route->nexthop, nexthop, 16);
        route->time = babel_now.tv_sec;
        route->hold_time = hold_time;
        route->installed = 0;
        memset(&route->channels, 0, sizeof(route->channels));
        if(channels_len > 0)
            memcpy(&route->channels, channels,
                   MIN(channels_len, DIVERSITY_HOPS));
        route->next = NULL;
        new_route = insert_route(route);
        if(new_route == NULL) {
            fprintf(stderr, "Couldn't insert route.\n");
            free(route);
            return NULL;
        }
        consider_route(route);
    }
    return route;
}

/* We just received an unfeasible update.  If it's any good, send
   a request for a new seqno. */
void
send_unfeasible_request(struct neighbour *neigh, int force,
                        unsigned short seqno, unsigned short metric,
                        struct source *src)
{
    struct babel_route *route = find_installed_route(src->prefix, src->plen);

    if(seqno_minus(src->seqno, seqno) > 100) {
        /* Probably a source that lost its seqno.  Let it time-out. */
        return;
    }

    if(force || !route || route_metric(route) >= metric + 512) {
        send_unicast_multihop_request(neigh, src->prefix, src->plen,
                                      src->metric >= INFINITY ?
                                      src->seqno :
                                      seqno_plus(src->seqno, 1),
                                      src->id, 127);
    }
}

/* This takes a feasible route and decides whether to install it. */
static void
consider_route(struct babel_route *route)
{
    struct babel_route *installed;
    struct xroute *xroute;

    if(route->installed)
        return;

    if(!route_feasible(route))
        return;

    xroute = find_xroute(route->src->prefix, route->src->plen);
    if(xroute && (allow_duplicates < 0 || xroute->metric >= allow_duplicates))
        return;

    installed = find_installed_route(route->src->prefix, route->src->plen);

    if(installed == NULL)
        goto install;

    if(route_metric(route) >= INFINITY)
        return;

    if(route_metric(installed) >= INFINITY)
        goto install;

    if(route_metric(installed) >= route_metric(route) + 64)
        goto install;

    return;

 install:
    switch_routes(installed, route);
    if(installed && route->installed)
        send_triggered_update(route, installed->src, route_metric(installed));
    else
        send_update(NULL, 1, route->src->prefix, route->src->plen);
    return;
}

void
retract_neighbour_routes(struct neighbour *neigh)
{
    int i;

    for(i = 0; i < route_slots; i++) {
        struct babel_route *r = routes[i];
        while(r) {
            if(r->neigh == neigh) {
                if(r->refmetric != INFINITY) {
                    unsigned short oldmetric = route_metric(r);
                    retract_route(r);
                    if(oldmetric != INFINITY)
                        route_changed(r, r->src, oldmetric);
                }
            }
            r = r->next;
        }
        i++;
    }
}

void
send_triggered_update(struct babel_route *route, struct source *oldsrc,
                      unsigned oldmetric)
{
    unsigned newmetric, diff;
    /* 1 means send speedily, 2 means resend */
    int urgent;

    if(!route->installed)
        return;

    newmetric = route_metric(route);
    diff =
        newmetric >= oldmetric ? newmetric - oldmetric : oldmetric - newmetric;

    if(route->src != oldsrc || (oldmetric < INFINITY && newmetric >= INFINITY))
        /* Switching sources can cause transient routing loops.
           Retractions can cause blackholes. */
        urgent = 2;
    else if(newmetric > oldmetric && oldmetric < 6 * 256 && diff >= 512)
        /* Route getting significantly worse */
        urgent = 1;
    else if(unsatisfied_request(route->src->prefix, route->src->plen,
                                route->seqno, route->src->id))
        /* Make sure that requests are satisfied speedily */
        urgent = 1;
    else if(oldmetric >= INFINITY && newmetric < INFINITY)
        /* New route */
        urgent = 0;
    else if(newmetric < oldmetric && diff < 1024)
        /* Route getting better.  This may be a transient fluctuation, so
           don't advertise it to avoid making routes unfeasible later on. */
        return;
    else if(diff < 384)
        /* Don't fret about trivialities */
        return;
    else
        urgent = 0;

    if(urgent >= 2)
        send_update_resend(NULL, route->src->prefix, route->src->plen);
    else
        send_update(NULL, urgent, route->src->prefix, route->src->plen);

    if(oldmetric < INFINITY) {
        if(newmetric >= oldmetric + 512) {
            send_request_resend(NULL, route->src->prefix, route->src->plen,
                                route->src->metric >= INFINITY ?
                                route->src->seqno :
                                seqno_plus(route->src->seqno, 1),
                                route->src->id);
        } else if(newmetric >= oldmetric + 288) {
            send_request(NULL, route->src->prefix, route->src->plen);
        }
    }
}

/* A route has just changed.  Decide whether to switch to a different route or
   send an update. */
void
route_changed(struct babel_route *route,
              struct source *oldsrc, unsigned short oldmetric)
{
    if(route->installed) {
        if(route_metric(route) > oldmetric) {
            struct babel_route *better_route;
            better_route =
                find_best_route(route->src->prefix, route->src->plen, 1, NULL);
            if(better_route &&
               route_metric(better_route) <= route_metric(route) - 96)
                consider_route(better_route);
        }

        if(route->installed)
            /* We didn't change routes after all. */
            send_triggered_update(route, oldsrc, oldmetric);
    } else {
        /* Reconsider routes even when their metric didn't decrease,
           they may not have been feasible before. */
        consider_route(route);
    }
}

/* We just lost the installed route to a given destination. */
void
route_lost(struct source *src, unsigned oldmetric)
{
    struct babel_route *new_route;
    new_route = find_best_route(src->prefix, src->plen, 1, NULL);
    if(new_route) {
        consider_route(new_route);
    } else if(oldmetric < INFINITY) {
        /* Complain loudly. */
        send_update_resend(NULL, src->prefix, src->plen);
        send_request_resend(NULL, src->prefix, src->plen,
                            src->metric >= INFINITY ?
                            src->seqno : seqno_plus(src->seqno, 1),
                            src->id);
    }
}

/* This is called periodically to flush old routes.  It will also send
   requests for routes that are about to expire. */
void
expire_routes(void)
{
    struct babel_route *r;
    int i;

    debugf(BABEL_DEBUG_COMMON,"Expiring old routes.");

    i = 0;
    while(i < route_slots) {
        r = routes[i];
        while(r) {
            /* Protect against clock being stepped. */
            if(r->time > babel_now.tv_sec || route_old(r)) {
                flush_route(r);
                goto again;
            }

            update_route_metric(r);

            if(r->installed && r->refmetric < INFINITY) {
                if(route_old(r))
                    /* Route about to expire, send a request. */
                    send_unicast_request(r->neigh,
                                         r->src->prefix, r->src->plen);
            }
            r = r->next;
        }
        i++;
    again:
        ;
    }
}
