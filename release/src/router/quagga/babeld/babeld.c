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
#include "command.h"
#include "prefix.h"
#include "memory.h"
#include "memtypes.h"
#include "table.h"
#include "distribute.h"
#include "prefix.h"
#include "filter.h"
#include "plist.h"

#include "babel_main.h"
#include "babeld.h"
#include "util.h"
#include "net.h"
#include "kernel.h"
#include "babel_interface.h"
#include "neighbour.h"
#include "route.h"
#include "message.h"
#include "resend.h"
#include "babel_filter.h"
#include "babel_zebra.h"


static int babel_init_routing_process(struct thread *thread);
static void babel_get_myid(void);
static void babel_initial_noise(void);
static int babel_read_protocol (struct thread *thread);
static int babel_main_loop(struct thread *thread);
static void babel_set_timer(struct timeval *timeout);
static void babel_fill_with_next_timeout(struct timeval *tv);


/* Informations relative to the babel running daemon. */
static struct babel *babel_routing_process = NULL;
static unsigned char *receive_buffer = NULL;
static int receive_buffer_size = 0;

/* timeouts */
struct timeval check_neighbours_timeout;
static time_t expiry_time;
static time_t source_expiry_time;

/* Babel node structure. */
static struct cmd_node cmd_babel_node =
{
    .node   = BABEL_NODE,
    .prompt = "%s(config-router)# ",
    .vtysh  = 1,
};

/* print current babel configuration on vty */
static int
babel_config_write (struct vty *vty)
{
    int lines = 0;
    int i;

    /* list enabled debug modes */
    lines += debug_babel_config_write (vty);

    if (!babel_routing_process)
        return lines;
    vty_out (vty, "router babel%s", VTY_NEWLINE);
    if (resend_delay != BABEL_DEFAULT_RESEND_DELAY)
    {
        vty_out (vty, " babel resend-delay %u%s", resend_delay, VTY_NEWLINE);
        lines++;
    }
    /* list enabled interfaces */
    lines = 1 + babel_enable_if_config_write (vty);
    /* list redistributed protocols */
    for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
        if (i != zclient->redist_default && zclient->redist[i])
        {
            vty_out (vty, " redistribute %s%s", zebra_route_string (i), VTY_NEWLINE);
            lines++;
        }

    return lines;
}


static int
babel_create_routing_process (void)
{
    assert (babel_routing_process == NULL);

    /* Allocaste Babel instance. */
    babel_routing_process = XCALLOC (MTYPE_BABEL, sizeof (struct babel));

    /* Initialize timeouts */
    gettime(&babel_now);
    expiry_time = babel_now.tv_sec + roughly(30);
    source_expiry_time = babel_now.tv_sec + roughly(300);

    /* Make socket for Babel protocol. */
    protocol_socket = babel_socket(protocol_port);
    if (protocol_socket < 0) {
        zlog_err("Couldn't create link local socket: %s", safe_strerror(errno));
        goto fail;
    }

    /* Threads. */
    babel_routing_process->t_read =
    thread_add_read(master, &babel_read_protocol, NULL, protocol_socket);
    /* wait a little: zebra will announce interfaces, addresses, routes... */
    babel_routing_process->t_update =
    thread_add_timer_msec(master, &babel_init_routing_process, NULL, 200L);
    return 0;

fail:
    XFREE(MTYPE_BABEL, babel_routing_process);
    babel_routing_process = NULL;
    return -1;
}

/* thread reading entries form others babel daemons */
static int
babel_read_protocol (struct thread *thread)
{
    int rc;
    struct interface *ifp = NULL;
    struct sockaddr_in6 sin6;
    struct listnode *linklist_node = NULL;

    assert(babel_routing_process != NULL);
    assert(protocol_socket >= 0);

    rc = babel_recv(protocol_socket,
                    receive_buffer, receive_buffer_size,
                    (struct sockaddr*)&sin6, sizeof(sin6));
    if(rc < 0) {
        if(errno != EAGAIN && errno != EINTR) {
            zlog_err("recv: %s", safe_strerror(errno));
        }
    } else {
        FOR_ALL_INTERFACES(ifp, linklist_node) {
            if(!if_up(ifp))
                continue;
            if(ifp->ifindex == sin6.sin6_scope_id) {
                parse_packet((unsigned char*)&sin6.sin6_addr, ifp,
                             receive_buffer, rc);
                break;
            }
        }
    }

    /* re-add thread */
    babel_routing_process->t_read =
    thread_add_read(master, &babel_read_protocol, NULL, protocol_socket);
    return 0;
}

/* Zebra will give some information, especially about interfaces. This function
 must be call with a litte timeout wich may give zebra the time to do his job,
 making these inits have sense. */
static int
babel_init_routing_process(struct thread *thread)
{
    myseqno = (random() & 0xFFFF);
    babel_get_myid();
    babel_load_state_file();
    debugf(BABEL_DEBUG_COMMON, "My ID is : %s.", format_eui64(myid));
    babel_initial_noise();
    babel_main_loop(thread);/* this function self-add to the t_update thread */
    return 0;
}

/* fill "myid" with an unique id (only if myid != {0}). */
static void
babel_get_myid(void)
{
    struct interface *ifp = NULL;
    struct listnode *linklist_node = NULL;
    int rc;
    int i;

    /* if we already have an id (from state file), we return. */
    if (memcmp(myid, zeroes, 8) != 0) {
        return;
    }

    FOR_ALL_INTERFACES(ifp, linklist_node) {
        /* ifp->ifindex is not necessarily valid at this point */
        int ifindex = if_nametoindex(ifp->name);
        if(ifindex > 0) {
            unsigned char eui[8];
            rc = if_eui64(ifp->name, ifindex, eui);
            if(rc < 0)
                continue;
            memcpy(myid, eui, 8);
            return;
        }
    }

    /* We failed to get a global EUI64 from the interfaces we were given.
     Let's try to find an interface with a MAC address. */
    for(i = 1; i < 256; i++) {
        char buf[IF_NAMESIZE], *ifname;
        unsigned char eui[8];
        ifname = if_indextoname(i, buf);
        if(ifname == NULL)
            continue;
        rc = if_eui64(ifname, i, eui);
        if(rc < 0)
            continue;
        memcpy(myid, eui, 8);
        return;
    }

    zlog_err("Warning: couldn't find router id -- using random value.");

    rc = read_random_bytes(myid, 8);
    if(rc < 0) {
        zlog_err("read(random): %s (cannot assign an ID)",safe_strerror(errno));
        exit(1);
    }
    /* Clear group and global bits */
    myid[0] &= ~3;
}

/* Make some noise so that others notice us, and send retractions in
 case we were restarted recently */
static void
babel_initial_noise(void)
{
    struct interface *ifp = NULL;
    struct listnode *linklist_node = NULL;

    FOR_ALL_INTERFACES(ifp, linklist_node) {
        if(!if_up(ifp))
            continue;
        /* Apply jitter before we send the first message. */
        usleep(roughly(10000));
        gettime(&babel_now);
        send_hello(ifp);
        send_wildcard_retraction(ifp);
    }

    FOR_ALL_INTERFACES(ifp, linklist_node) {
        if(!if_up(ifp))
            continue;
        usleep(roughly(10000));
        gettime(&babel_now);
        send_hello(ifp);
        send_wildcard_retraction(ifp);
        send_self_update(ifp);
        send_request(ifp, NULL, 0);
        flushupdates(ifp);
        flushbuf(ifp);
    }
}

/* Delete all the added babel routes, make babeld only speak to zebra. */
static void
babel_clean_routing_process()
{
    flush_all_routes();
    babel_interface_close_all();

    /* cancel threads */
    if (babel_routing_process->t_read != NULL) {
        thread_cancel(babel_routing_process->t_read);
    }
    if (babel_routing_process->t_update != NULL) {
        thread_cancel(babel_routing_process->t_update);
    }

    XFREE(MTYPE_BABEL, babel_routing_process);
    babel_routing_process = NULL;
}

/* Function used with timeout. */
static int
babel_main_loop(struct thread *thread)
{
    struct timeval tv;
    struct interface *ifp = NULL;
    struct listnode *linklist_node = NULL;

    while(1) {
        gettime(&babel_now);

        /* timeouts --------------------------------------------------------- */
        /* get the next timeout */
        babel_fill_with_next_timeout(&tv);
        /* if there is no timeout, we must wait. */
        if(timeval_compare(&tv, &babel_now) > 0) {
            timeval_minus(&tv, &tv, &babel_now);
            debugf(BABEL_DEBUG_TIMEOUT, "babel main loop : timeout: %ld msecs",
                   tv.tv_sec * 1000 + tv.tv_usec / 1000);
            /* it happens often to have less than 1 ms, it's bad. */
            timeval_add_msec(&tv, &tv, 300);
            babel_set_timer(&tv);
            return 0;
        }

        gettime(&babel_now);

        /* update database -------------------------------------------------- */
        if(timeval_compare(&check_neighbours_timeout, &babel_now) < 0) {
            int msecs;
            msecs = check_neighbours();
            msecs = MAX(msecs, 10);
            schedule_neighbours_check(msecs, 1);
        }

        if(babel_now.tv_sec >= expiry_time) {
            expire_routes();
            expire_resend();
            expiry_time = babel_now.tv_sec + roughly(30);
        }

        if(babel_now.tv_sec >= source_expiry_time) {
            expire_sources();
            source_expiry_time = babel_now.tv_sec + roughly(300);
        }

        FOR_ALL_INTERFACES(ifp, linklist_node) {
            babel_interface_nfo *babel_ifp = NULL;
            if(!if_up(ifp))
                continue;
            babel_ifp = babel_get_if_nfo(ifp);
            if(timeval_compare(&babel_now, &babel_ifp->hello_timeout) >= 0)
                send_hello(ifp);
            if(timeval_compare(&babel_now, &babel_ifp->update_timeout) >= 0)
                send_update(ifp, 0, NULL, 0);
            if(timeval_compare(&babel_now,
                               &babel_ifp->update_flush_timeout) >= 0)
                flushupdates(ifp);
        }

        if(resend_time.tv_sec != 0) {
            if(timeval_compare(&babel_now, &resend_time) >= 0)
                do_resend();
        }

        if(unicast_flush_timeout.tv_sec != 0) {
            if(timeval_compare(&babel_now, &unicast_flush_timeout) >= 0)
                flush_unicast(1);
        }

        FOR_ALL_INTERFACES(ifp, linklist_node) {
            babel_interface_nfo *babel_ifp = NULL;
            if(!if_up(ifp))
                continue;
            babel_ifp = babel_get_if_nfo(ifp);
            if(babel_ifp->flush_timeout.tv_sec != 0) {
                if(timeval_compare(&babel_now, &babel_ifp->flush_timeout) >= 0)
                    flushbuf(ifp);
            }
        }
    }

    assert(0); /* this line should never be reach */
}

static void
printIfMin(struct timeval *tv, int cmd, const char *tag, const char *ifname)
{
    static struct timeval curr_tv;
    static char buffer[200];
    static const char *curr_tag = NULL;

    switch (cmd) {
        case 0: /* reset timeval */
            curr_tv = *tv;
            if(ifname != NULL) {
                snprintf(buffer, 200L, "interface: %s; %s", ifname, tag);
                curr_tag = buffer;
            } else {
                curr_tag = tag;
            }
            break;
        case 1: /* take the min */
            if (tv->tv_sec == 0 && tv->tv_usec == 0) { /* if (tv == ∞) */
                break;
            }
            if (tv->tv_sec < curr_tv.tv_sec ||(tv->tv_sec == curr_tv.tv_sec &&
                                               tv->tv_usec < curr_tv.tv_usec)) {
                curr_tv = *tv;
                if(ifname != NULL) {
                    snprintf(buffer, 200L, "interface: %s; %s", ifname, tag);
                    curr_tag = buffer;
                } else {
                    curr_tag = tag;
                }
            }
            break;
        case 2: /* print message */
            debugf(BABEL_DEBUG_TIMEOUT, "next timeout due to: %s", curr_tag);
            break;
        default:
            break;
    }
}

static void
babel_fill_with_next_timeout(struct timeval *tv)
{
#if (defined NO_DEBUG)
#define printIfMin(a,b,c,d)
#else
#define printIfMin(a,b,c,d) \
  if (UNLIKELY(debug & BABEL_DEBUG_TIMEOUT)) {printIfMin(a,b,c,d);}

    struct interface *ifp = NULL;
    struct listnode *linklist_node = NULL;

    *tv = check_neighbours_timeout;
    printIfMin(tv, 0, "check_neighbours_timeout", NULL);
    timeval_min_sec(tv, expiry_time);
    printIfMin(tv, 1, "expiry_time", NULL);
    timeval_min_sec(tv, source_expiry_time);
    printIfMin(tv, 1, "source_expiry_time", NULL);
    timeval_min(tv, &resend_time);
    printIfMin(tv, 1, "resend_time", NULL);
    FOR_ALL_INTERFACES(ifp, linklist_node) {
        babel_interface_nfo *babel_ifp = NULL;
        if(!if_up(ifp))
            continue;
        babel_ifp = babel_get_if_nfo(ifp);
        timeval_min(tv, &babel_ifp->flush_timeout);
        printIfMin(tv, 1, "flush_timeout", ifp->name);
        timeval_min(tv, &babel_ifp->hello_timeout);
        printIfMin(tv, 1, "hello_timeout", ifp->name);
        timeval_min(tv, &babel_ifp->update_timeout);
        printIfMin(tv, 1, "update_timeout", ifp->name);
        timeval_min(tv, &babel_ifp->update_flush_timeout);
        printIfMin(tv, 1, "update_flush_timeout",ifp->name);
    }
    timeval_min(tv, &unicast_flush_timeout);
    printIfMin(tv, 1, "unicast_flush_timeout", NULL);
    printIfMin(tv, 2, NULL, NULL);
#undef printIfMin
#endif
}

/* set the t_update thread of the babel routing process to be launch in
 'timeout' (approximate at the milisecond) */
static void
babel_set_timer(struct timeval *timeout)
{
    long msecs = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    if (babel_routing_process->t_update != NULL) {
        thread_cancel(babel_routing_process->t_update);
    }
    babel_routing_process->t_update =
    thread_add_timer_msec(master, &babel_main_loop, NULL, msecs);
}

/* Schedule a neighbours check after roughly 3/2 times msecs have elapsed. */
void
schedule_neighbours_check(int msecs, int override)
{
    struct timeval timeout;

    timeval_add_msec(&timeout, &babel_now, roughly(msecs * 3 / 2));
    if(override)
        check_neighbours_timeout = timeout;
    else
        timeval_min(&check_neighbours_timeout, &timeout);
}

int
resize_receive_buffer(int size)
{
    if(size <= receive_buffer_size)
        return 0;

    if(receive_buffer == NULL) {
        receive_buffer = malloc(size);
        if(receive_buffer == NULL) {
            zlog_err("malloc(receive_buffer): %s", safe_strerror(errno));
            return -1;
        }
        receive_buffer_size = size;
    } else {
        unsigned char *new;
        new = realloc(receive_buffer, size);
        if(new == NULL) {
            zlog_err("realloc(receive_buffer): %s", safe_strerror(errno));
            return -1;
        }
        receive_buffer = new;
        receive_buffer_size = size;
    }
    return 1;
}

static void
babel_distribute_update (struct distribute *dist)
{
    struct interface *ifp;
    babel_interface_nfo *babel_ifp;
    struct access_list *alist;
    struct prefix_list *plist;

    if (! dist->ifname)
        return;

    ifp = if_lookup_by_name (dist->ifname);
    if (ifp == NULL)
        return;

    babel_ifp = babel_get_if_nfo(ifp);

    if (dist->list[DISTRIBUTE_IN]) {
        alist = access_list_lookup (AFI_IP6, dist->list[DISTRIBUTE_IN]);
        if (alist)
            babel_ifp->list[BABEL_FILTER_IN] = alist;
        else
            babel_ifp->list[BABEL_FILTER_IN] = NULL;
    } else {
        babel_ifp->list[BABEL_FILTER_IN] = NULL;
    }

    if (dist->list[DISTRIBUTE_OUT]) {
        alist = access_list_lookup (AFI_IP6, dist->list[DISTRIBUTE_OUT]);
        if (alist)
            babel_ifp->list[BABEL_FILTER_OUT] = alist;
        else
            babel_ifp->list[BABEL_FILTER_OUT] = NULL;
    } else {
        babel_ifp->list[BABEL_FILTER_OUT] = NULL;
    }

    if (dist->prefix[DISTRIBUTE_IN]) {
        plist = prefix_list_lookup (AFI_IP6, dist->prefix[DISTRIBUTE_IN]);
        if (plist)
            babel_ifp->prefix[BABEL_FILTER_IN] = plist;
        else
            babel_ifp->prefix[BABEL_FILTER_IN] = NULL;
    } else {
        babel_ifp->prefix[BABEL_FILTER_IN] = NULL;
    }

    if (dist->prefix[DISTRIBUTE_OUT]) {
        plist = prefix_list_lookup (AFI_IP6, dist->prefix[DISTRIBUTE_OUT]);
        if (plist)
            babel_ifp->prefix[BABEL_FILTER_OUT] = plist;
        else
            babel_ifp->prefix[BABEL_FILTER_OUT] = NULL;
    } else {
        babel_ifp->prefix[BABEL_FILTER_OUT] = NULL;
    }
}

static void
babel_distribute_update_interface (struct interface *ifp)
{
    struct distribute *dist;

    dist = distribute_lookup (ifp->name);
    if (dist)
        babel_distribute_update (dist);
}

/* Update all interface's distribute list. */
static void
babel_distribute_update_all (struct prefix_list *notused)
{
    struct interface *ifp;
    struct listnode *node;

    for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
        babel_distribute_update_interface (ifp);
}

static void
babel_distribute_update_all_wrapper (struct access_list *notused)
{
    babel_distribute_update_all(NULL);
}


/* [Command] */
DEFUN (router_babel,
       router_babel_cmd,
       "router babel",
       "Enable a routing process\n"
       "Make Babel instance command\n"
       "No attributes\n")
{
    int ret;

    vty->node = BABEL_NODE;

    if (!babel_routing_process) {
        ret = babel_create_routing_process ();

        /* Notice to user we couldn't create Babel. */
        if (ret < 0) {
            zlog_warn ("can't create Babel");
            return CMD_WARNING;
        }
    }

    return CMD_SUCCESS;
}

/* [Command] */
DEFUN (no_router_babel,
       no_router_babel_cmd,
       "no router babel",
       NO_STR
       "Disable a routing process\n"
       "Remove Babel instance command\n"
       "No attributes\n")
{
    if(babel_routing_process)
        babel_clean_routing_process();
    return CMD_SUCCESS;
}

/* [Babel Command] */
DEFUN (babel_set_resend_delay,
       babel_set_resend_delay_cmd,
       "babel resend-delay <20-655340>",
       "Babel commands\n"
       "Time before resending a message\n"
       "Milliseconds\n")
{
    int interval;

    VTY_GET_INTEGER_RANGE("milliseconds", interval, argv[0], 20, 10 * 0xFFFE);

    resend_delay = interval;
    return CMD_SUCCESS;
}

void
babeld_quagga_init(void)
{

    install_node(&cmd_babel_node, &babel_config_write);

    install_element(CONFIG_NODE, &router_babel_cmd);
    install_element(CONFIG_NODE, &no_router_babel_cmd);

    install_default(BABEL_NODE);
    install_element(BABEL_NODE, &babel_set_resend_delay_cmd);

    babel_if_init();

    /* Access list install. */
    access_list_init ();
    access_list_add_hook (babel_distribute_update_all_wrapper);
    access_list_delete_hook (babel_distribute_update_all_wrapper);

    /* Prefix list initialize.*/
    prefix_list_init ();
    prefix_list_add_hook (babel_distribute_update_all);
    prefix_list_delete_hook (babel_distribute_update_all);

    /* Distribute list install. */
    distribute_list_init (BABEL_NODE);
    distribute_list_add_hook (babel_distribute_update);
    distribute_list_delete_hook (babel_distribute_update);
}

/* Stubs to adapt Babel's filtering calls to Quagga's infrastructure. */

int
input_filter(const unsigned char *id,
             const unsigned char *prefix, unsigned short plen,
             const unsigned char *neigh, unsigned int ifindex)
{
    return babel_filter(0, prefix, plen, ifindex);
}

int
output_filter(const unsigned char *id, const unsigned char *prefix,
              unsigned short plen, unsigned int ifindex)
{
    return babel_filter(1, prefix, plen, ifindex);
}

/* There's no redistribute filter in Quagga -- the zebra daemon does its
   own filtering. */
int
redistribute_filter(const unsigned char *prefix, unsigned short plen,
                    unsigned int ifindex, int proto)
{
    return 0;
}

