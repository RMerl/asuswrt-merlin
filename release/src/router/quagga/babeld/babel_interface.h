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

#ifndef BABEL_INTERFACE_H
#define BABEL_INTERFACE_H

#include <zebra.h>
#include "zclient.h"
#include "vty.h"

#define CONFIG_DEFAULT 0
#define CONFIG_NO 1
#define CONFIG_YES 2

/* babeld interface informations */
struct babel_interface {
    unsigned short flags;                     /* see below */
    unsigned short cost;
    int channel;
    struct timeval hello_timeout;
    struct timeval update_timeout;
    struct timeval flush_timeout;
    struct timeval update_flush_timeout;
    unsigned char *ipv4;
    int buffered;
    int bufsize;
    char have_buffered_hello;
    char have_buffered_id;
    char have_buffered_nh;
    char have_buffered_prefix;
    unsigned char buffered_id[16];
    unsigned char buffered_nh[4];
    unsigned char buffered_prefix[16];
    unsigned char *sendbuf;
    struct buffered_update *buffered_updates;
    int num_buffered_updates;
    int update_bufsize;
    time_t bucket_time;
    unsigned int bucket;
    time_t last_update_time;
    unsigned short hello_seqno;
    unsigned hello_interval;
    unsigned update_interval;

    /* For filter type slot. */
#define BABEL_FILTER_IN  0
#define BABEL_FILTER_OUT 1
#define BABEL_FILTER_MAX 2
    struct access_list *list[BABEL_FILTER_MAX];               /* Access-list. */
    struct prefix_list *prefix[BABEL_FILTER_MAX];             /* Prefix-list. */
};

typedef struct babel_interface babel_interface_nfo;
static inline babel_interface_nfo* babel_get_if_nfo(struct interface *ifp)
{
    return ((babel_interface_nfo*) ifp->info);
}

/* babel_interface_nfo flags */
#define BABEL_IF_IS_UP         (1 << 0)
#define BABEL_IF_WIRED         (1 << 1)
#define BABEL_IF_SPLIT_HORIZON (1 << 2)
#define BABEL_IF_LQ            (1 << 3)
#define BABEL_IF_FARAWAY       (1 << 4)

/* Only INTERFERING can appear on the wire. */
#define BABEL_IF_CHANNEL_UNKNOWN 0
#define BABEL_IF_CHANNEL_INTERFERING 255
#define BABEL_IF_CHANNEL_NONINTERFERING -2

static inline int
if_up(struct interface *ifp)
{
    return (if_is_operative(ifp) &&
            ifp->connected != NULL &&
            (babel_get_if_nfo(ifp)->flags & BABEL_IF_IS_UP));
}

/* types:
 struct interface _ifp, struct listnode node */
#define FOR_ALL_INTERFACES(_ifp, _node)                                              \
    for(ALL_LIST_ELEMENTS_RO(iflist, _node, _ifp))

/* types:
 struct interface *ifp, struct connected *_connected, struct listnode *node */
#define FOR_ALL_INTERFACES_ADDRESSES(ifp, _connected, _node)                   \
    for(ALL_LIST_ELEMENTS_RO(ifp->connected, _node, _connected))

struct buffered_update {
    unsigned char id[8];
    unsigned char prefix[16];
    unsigned char plen;
    unsigned char pad[3];
};


/* init function */
void babel_if_init(void);

/* Callback functions for zebra client */
int babel_interface_up (int, struct zclient *, zebra_size_t);
int babel_interface_down (int, struct zclient *, zebra_size_t);
int babel_interface_add (int, struct zclient *, zebra_size_t);
int babel_interface_delete (int, struct zclient *, zebra_size_t);
int babel_interface_address_add (int, struct zclient *, zebra_size_t);
int babel_interface_address_delete (int, struct zclient *, zebra_size_t);

unsigned jitter(babel_interface_nfo *, int);
unsigned update_jitter(babel_interface_nfo *babel_ifp, int urgent);
/* return "true" if "address" is one of our ipv6 addresses */
int is_interface_ll_address(struct interface *ifp, const unsigned char *address);
/* Send retraction to all, and reset all interfaces statistics. */
void babel_interface_close_all(void);
extern int babel_enable_if_config_write (struct vty *);


#endif
