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

#include <zebra.h>
#include "if.h"

#include "babeld.h"
#include "util.h"
#include "net.h"
#include "babel_interface.h"
#include "source.h"
#include "neighbour.h"
#include "route.h"
#include "xroute.h"
#include "resend.h"
#include "message.h"
#include "kernel.h"

unsigned char packet_header[4] = {42, 2};

int split_horizon = 1;

unsigned short myseqno = 0;
struct timeval seqno_time = {0, 0};

#define UNICAST_BUFSIZE 1024
int unicast_buffered = 0;
unsigned char *unicast_buffer = NULL;
struct neighbour *unicast_neighbour = NULL;
struct timeval unicast_flush_timeout = {0, 0};

static const unsigned char v4prefix[16] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };

/* Parse a network prefix, encoded in the somewhat baroque compressed
   representation used by Babel.  Return the number of bytes parsed. */
static int
network_prefix(int ae, int plen, unsigned int omitted,
               const unsigned char *p, const unsigned char *dp,
               unsigned int len, unsigned char *p_r)
{
    unsigned pb;
    unsigned char prefix[16];
    int ret = -1;

    if(plen >= 0)
        pb = (plen + 7) / 8;
    else if(ae == 1)
        pb = 4;
    else
        pb = 16;

    if(pb > 16)
        return -1;

    memset(prefix, 0, 16);

    switch(ae) {
    case 0:
        ret = 0;
        break;
    case 1:
        if(omitted > 4 || pb > 4 || (pb > omitted && len < pb - omitted))
            return -1;
        memcpy(prefix, v4prefix, 12);
        if(omitted) {
            if (dp == NULL || !v4mapped(dp)) return -1;
            memcpy(prefix, dp, 12 + omitted);
        }
        if(pb > omitted) memcpy(prefix + 12 + omitted, p, pb - omitted);
        ret = pb - omitted;
        break;
    case 2:
        if(omitted > 16 || (pb > omitted && len < pb - omitted)) return -1;
        if(omitted) {
            if (dp == NULL || v4mapped(dp)) return -1;
            memcpy(prefix, dp, omitted);
        }
        if(pb > omitted) memcpy(prefix + omitted, p, pb - omitted);
        ret = pb - omitted;
        break;
    case 3:
        if(pb > 8 && len < pb - 8) return -1;
        prefix[0] = 0xfe;
        prefix[1] = 0x80;
        if(pb > 8) memcpy(prefix + 8, p, pb - 8);
        ret = pb - 8;
        break;
    default:
        return -1;
    }

    mask_prefix(p_r, prefix, plen < 0 ? 128 : ae == 1 ? plen + 96 : plen);
    return ret;
}

static void
parse_route_attributes(const unsigned char *a, int alen,
                       unsigned char *channels)
{
    int type, len, i = 0;

    while(i < alen) {
        type = a[i];
        if(type == 0) {
            i++;
            continue;
        }

        if(i + 1 > alen) {
            fprintf(stderr, "Received truncated attributes.\n");
            return;
        }
        len = a[i + 1];
        if(i + len > alen) {
            fprintf(stderr, "Received truncated attributes.\n");
            return;
        }

        if(type == 1) {
            /* Nothing. */
        } else if(type == 2) {
            if(len > DIVERSITY_HOPS) {
                fprintf(stderr,
                        "Received overlong channel information (%d > %d).\n",
                        len, DIVERSITY_HOPS);
                len = DIVERSITY_HOPS;
            }
            if(memchr(a + i + 2, 0, len) != NULL) {
                /* 0 is reserved. */
                fprintf(stderr, "Channel information contains 0!");
                return;
            }
            memset(channels, 0, DIVERSITY_HOPS);
            memcpy(channels, a + i + 2, len);
        } else {
            fprintf(stderr, "Received unknown route attribute %d.\n", type);
        }

        i += len + 2;
    }
}

static int
network_address(int ae, const unsigned char *a, unsigned int len,
                unsigned char *a_r)
{
    return network_prefix(ae, -1, 0, a, NULL, len, a_r);
}

static int
channels_len(unsigned char *channels)
{
    unsigned char *p = memchr(channels, 0, DIVERSITY_HOPS);
    return p ? (p - channels) : DIVERSITY_HOPS;
}

void
parse_packet(const unsigned char *from, struct interface *ifp,
             const unsigned char *packet, int packetlen)
{
    int i;
    const unsigned char *message;
    unsigned char type, len;
    int bodylen;
    struct neighbour *neigh;
    int have_router_id = 0, have_v4_prefix = 0, have_v6_prefix = 0,
        have_v4_nh = 0, have_v6_nh = 0;
    unsigned char router_id[8], v4_prefix[16], v6_prefix[16],
        v4_nh[16], v6_nh[16];

    if(!linklocal(from)) {
        zlog_err("Received packet from non-local address %s.",
                 format_address(from));
        return;
    }

    if(packet[0] != 42) {
        zlog_err("Received malformed packet on %s from %s.",
                 ifp->name, format_address(from));
        return;
    }

    if(packet[1] != 2) {
        zlog_err("Received packet with unknown version %d on %s from %s.",
                 packet[1], ifp->name, format_address(from));
        return;
    }

    neigh = find_neighbour(from, ifp);
    if(neigh == NULL) {
        zlog_err("Couldn't allocate neighbour.");
        return;
    }

    DO_NTOHS(bodylen, packet + 2);

    if(bodylen + 4 > packetlen) {
        zlog_err("Received truncated packet (%d + 4 > %d).",
                 bodylen, packetlen);
        bodylen = packetlen - 4;
    }

    i = 0;
    while(i < bodylen) {
        message = packet + 4 + i;
        type = message[0];
        if(type == MESSAGE_PAD1) {
            debugf(BABEL_DEBUG_COMMON,"Received pad1 from %s on %s.",
                   format_address(from), ifp->name);
            i++;
            continue;
        }
        if(i + 1 > bodylen) {
            zlog_err("Received truncated message.");
            break;
        }
        len = message[1];
        if(i + len > bodylen) {
            zlog_err("Received truncated message.");
            break;
        }

        if(type == MESSAGE_PADN) {
            debugf(BABEL_DEBUG_COMMON,"Received pad%d from %s on %s.",
                   len, format_address(from), ifp->name);
        } else if(type == MESSAGE_ACK_REQ) {
            unsigned short nonce, interval;
            if(len < 6) goto fail;
            DO_NTOHS(nonce, message + 4);
            DO_NTOHS(interval, message + 6);
            debugf(BABEL_DEBUG_COMMON,"Received ack-req (%04X %d) from %s on %s.",
                   nonce, interval, format_address(from), ifp->name);
            send_ack(neigh, nonce, interval);
        } else if(type == MESSAGE_ACK) {
            debugf(BABEL_DEBUG_COMMON,"Received ack from %s on %s.",
                   format_address(from), ifp->name);
            /* Nothing right now */
        } else if(type == MESSAGE_HELLO) {
            unsigned short seqno, interval;
            int changed;
            if(len < 6) goto fail;
            DO_NTOHS(seqno, message + 4);
            DO_NTOHS(interval, message + 6);
            debugf(BABEL_DEBUG_COMMON,"Received hello %d (%d) from %s on %s.",
                   seqno, interval,
                   format_address(from), ifp->name);
            changed = update_neighbour(neigh, seqno, interval);
            update_neighbour_metric(neigh, changed);
            if(interval > 0)
                schedule_neighbours_check(interval * 10, 0);
        } else if(type == MESSAGE_IHU) {
            unsigned short txcost, interval;
            unsigned char address[16];
            int rc;
            if(len < 6) goto fail;
            DO_NTOHS(txcost, message + 4);
            DO_NTOHS(interval, message + 6);
            rc = network_address(message[2], message + 8, len - 6, address);
            if(rc < 0) goto fail;
            debugf(BABEL_DEBUG_COMMON,"Received ihu %d (%d) from %s on %s for %s.",
                   txcost, interval,
                   format_address(from), ifp->name,
                   format_address(address));
            if(message[2] == 0 || is_interface_ll_address(ifp, address)) {
                int changed = txcost != neigh->txcost;
                neigh->txcost = txcost;
                neigh->ihu_time = babel_now;
                neigh->ihu_interval = interval;
                update_neighbour_metric(neigh, changed);
                if(interval > 0)
                    schedule_neighbours_check(interval * 10 * 3, 0);
            }
        } else if(type == MESSAGE_ROUTER_ID) {
            if(len < 10) {
                have_router_id = 0;
                goto fail;
            }
            memcpy(router_id, message + 4, 8);
            have_router_id = 1;
            debugf(BABEL_DEBUG_COMMON,"Received router-id %s from %s on %s.",
                   format_eui64(router_id), format_address(from), ifp->name);
        } else if(type == MESSAGE_NH) {
            unsigned char nh[16];
            int rc;
            if(len < 2) {
                have_v4_nh = 0;
                have_v6_nh = 0;
                goto fail;
            }
            rc = network_address(message[2], message + 4, len - 2,
                                 nh);
            if(rc < 0) {
                have_v4_nh = 0;
                have_v6_nh = 0;
                goto fail;
            }
            debugf(BABEL_DEBUG_COMMON,"Received nh %s (%d) from %s on %s.",
                   format_address(nh), message[2],
                   format_address(from), ifp->name);
            if(message[2] == 1) {
                memcpy(v4_nh, nh, 16);
                have_v4_nh = 1;
            } else {
                memcpy(v6_nh, nh, 16);
                have_v6_nh = 1;
            }
        } else if(type == MESSAGE_UPDATE) {
            unsigned char prefix[16], *nh;
            unsigned char plen;
            unsigned char channels[DIVERSITY_HOPS];
            unsigned short interval, seqno, metric;
            int rc, parsed_len;
            if(len < 10) {
                if(len < 2 || message[3] & 0x80)
                    have_v4_prefix = have_v6_prefix = 0;
                goto fail;
            }
            DO_NTOHS(interval, message + 6);
            DO_NTOHS(seqno, message + 8);
            DO_NTOHS(metric, message + 10);
            if(message[5] == 0 ||
               (message[3] == 1 ? have_v4_prefix : have_v6_prefix))
                rc = network_prefix(message[2], message[4], message[5],
                                    message + 12,
                                    message[2] == 1 ? v4_prefix : v6_prefix,
                                    len - 10, prefix);
            else
                rc = -1;
            if(rc < 0) {
                if(message[3] & 0x80)
                    have_v4_prefix = have_v6_prefix = 0;
                goto fail;
            }
            parsed_len = 10 + rc;

            plen = message[4] + (message[2] == 1 ? 96 : 0);

            if(message[3] & 0x80) {
                if(message[2] == 1) {
                    memcpy(v4_prefix, prefix, 16);
                    have_v4_prefix = 1;
                } else {
                    memcpy(v6_prefix, prefix, 16);
                    have_v6_prefix = 1;
                }
            }
            if(message[3] & 0x40) {
                if(message[2] == 1) {
                    memset(router_id, 0, 4);
                    memcpy(router_id + 4, prefix + 12, 4);
                } else {
                    memcpy(router_id, prefix + 8, 8);
                }
                have_router_id = 1;
            }
            if(!have_router_id && message[2] != 0) {
                zlog_err("Received prefix with no router id.");
                goto fail;
            }
            debugf(BABEL_DEBUG_COMMON,"Received update%s%s for %s from %s on %s.",
                   (message[3] & 0x80) ? "/prefix" : "",
                   (message[3] & 0x40) ? "/id" : "",
                   format_prefix(prefix, plen),
                   format_address(from), ifp->name);

            if(message[2] == 0) {
                if(metric < 0xFFFF) {
                    zlog_err("Received wildcard update with finite metric.");
                    goto done;
                }
                retract_neighbour_routes(neigh);
                goto done;
            } else if(message[2] == 1) {
                if(!have_v4_nh)
                    goto fail;
                nh = v4_nh;
            } else if(have_v6_nh) {
                nh = v6_nh;
            } else {
                nh = neigh->address;
            }

            if(message[2] == 1) {
                if(!babel_get_if_nfo(ifp)->ipv4)
                    goto done;
            }

            if((ifp->flags & BABEL_IF_FARAWAY)) {
                channels[0] = 0;
            } else {
                /* This will be overwritten by parse_route_attributes below. */
                if(metric < 256) {
                    /* Assume non-interfering (wired) link. */
                    channels[0] = 0;
                } else {
                    /* Assume interfering. */
                    channels[0] = BABEL_IF_CHANNEL_INTERFERING;
                    channels[1] = 0;
                }

                if(parsed_len < len)
                    parse_route_attributes(message + 2 + parsed_len,
                                           len - parsed_len, channels);
            }

            update_route(router_id, prefix, plen, seqno, metric, interval,
                         neigh, nh,
                         channels, channels_len(channels));
        } else if(type == MESSAGE_REQUEST) {
            unsigned char prefix[16], plen;
            int rc;
            if(len < 2) goto fail;
            rc = network_prefix(message[2], message[3], 0,
                                message + 4, NULL, len - 2, prefix);
            if(rc < 0) goto fail;
            plen = message[3] + (message[2] == 1 ? 96 : 0);
            debugf(BABEL_DEBUG_COMMON,"Received request for %s from %s on %s.",
                   message[2] == 0 ? "any" : format_prefix(prefix, plen),
                   format_address(from), ifp->name);
            if(message[2] == 0) {
                struct babel_interface *babel_ifp =babel_get_if_nfo(neigh->ifp);
                /* If a neighbour is requesting a full route dump from us,
                   we might as well send it an IHU. */
                send_ihu(neigh, NULL);
                /* Since nodes send wildcard requests on boot, booting
                   a large number of nodes at the same time may cause an
                   update storm.  Ignore a wildcard request that happens
                   shortly after we sent a full update. */
                if(babel_ifp->last_update_time <
                   (time_t)(babel_now.tv_sec -
                            MAX(babel_ifp->hello_interval / 100, 1)))
                    send_update(neigh->ifp, 0, NULL, 0);
            } else {
                send_update(neigh->ifp, 0, prefix, plen);
            }
        } else if(type == MESSAGE_MH_REQUEST) {
            unsigned char prefix[16], plen;
            unsigned short seqno;
            int rc;
            if(len < 14) goto fail;
            DO_NTOHS(seqno, message + 4);
            rc = network_prefix(message[2], message[3], 0,
                                message + 16, NULL, len - 14, prefix);
            if(rc < 0) goto fail;
            plen = message[3] + (message[2] == 1 ? 96 : 0);
            debugf(BABEL_DEBUG_COMMON,"Received request (%d) for %s from %s on %s (%s, %d).",
                   message[6],
                   format_prefix(prefix, plen),
                   format_address(from), ifp->name,
                   format_eui64(message + 8), seqno);
            handle_request(neigh, prefix, plen, message[6],
                           seqno, message + 8);
        } else {
            debugf(BABEL_DEBUG_COMMON,"Received unknown packet type %d from %s on %s.",
                   type, format_address(from), ifp->name);
        }
    done:
        i += len + 2;
        continue;

    fail:
        zlog_err("Couldn't parse packet (%d, %d) from %s on %s.",
                 message[0], message[1], format_address(from), ifp->name);
        goto done;
    }
    return;
}

/* Under normal circumstances, there are enough moderation mechanisms
   elsewhere in the protocol to make sure that this last-ditch check
   should never trigger.  But I'm superstitious. */

static int
check_bucket(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    if(babel_ifp->bucket <= 0) {
        int seconds = babel_now.tv_sec - babel_ifp->bucket_time;
        if(seconds > 0) {
            babel_ifp->bucket = MIN(BUCKET_TOKENS_MAX,
                              seconds * BUCKET_TOKENS_PER_SEC);
        }
        /* Reset bucket time unconditionally, in case clock is stepped. */
        babel_ifp->bucket_time = babel_now.tv_sec;
    }

    if(babel_ifp->bucket > 0) {
        babel_ifp->bucket--;
        return 1;
    } else {
        return 0;
    }
}

void
flushbuf(struct interface *ifp)
{
    int rc;
    struct sockaddr_in6 sin6;
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);

    assert(babel_ifp->buffered <= babel_ifp->bufsize);

    flushupdates(ifp);

    if(babel_ifp->buffered > 0) {
        debugf(BABEL_DEBUG_COMMON,"  (flushing %d buffered bytes on %s)",
               babel_ifp->buffered, ifp->name);
        if(check_bucket(ifp)) {
            memset(&sin6, 0, sizeof(sin6));
            sin6.sin6_family = AF_INET6;
            memcpy(&sin6.sin6_addr, protocol_group, 16);
            sin6.sin6_port = htons(protocol_port);
            sin6.sin6_scope_id = ifp->ifindex;
            DO_HTONS(packet_header + 2, babel_ifp->buffered);
            rc = babel_send(protocol_socket,
                            packet_header, sizeof(packet_header),
                            babel_ifp->sendbuf, babel_ifp->buffered,
                            (struct sockaddr*)&sin6, sizeof(sin6));
            if(rc < 0)
                zlog_err("send: %s", safe_strerror(errno));
        } else {
            zlog_err("Warning: bucket full, dropping packet to %s.",
                     ifp->name);
        }
    }
    VALGRIND_MAKE_MEM_UNDEFINED(babel_ifp->sendbuf, babel_ifp->bufsize);
    babel_ifp->buffered = 0;
    babel_ifp->have_buffered_hello = 0;
    babel_ifp->have_buffered_id = 0;
    babel_ifp->have_buffered_nh = 0;
    babel_ifp->have_buffered_prefix = 0;
    babel_ifp->flush_timeout.tv_sec = 0;
    babel_ifp->flush_timeout.tv_usec = 0;
}

static void
schedule_flush(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    unsigned msecs = jitter(babel_ifp, 0);
    if(babel_ifp->flush_timeout.tv_sec != 0 &&
       timeval_minus_msec(&babel_ifp->flush_timeout, &babel_now) < msecs)
        return;
    set_timeout(&babel_ifp->flush_timeout, msecs);
}

static void
schedule_flush_now(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    /* Almost now */
    unsigned msecs = roughly(10);
    if(babel_ifp->flush_timeout.tv_sec != 0 &&
       timeval_minus_msec(&babel_ifp->flush_timeout, &babel_now) < msecs)
        return;
    set_timeout(&babel_ifp->flush_timeout, msecs);
}

static void
schedule_unicast_flush(unsigned msecs)
{
    if(!unicast_neighbour)
        return;
    if(unicast_flush_timeout.tv_sec != 0 &&
       timeval_minus_msec(&unicast_flush_timeout, &babel_now) < msecs)
        return;
    unicast_flush_timeout.tv_usec = (babel_now.tv_usec + msecs * 1000) %1000000;
    unicast_flush_timeout.tv_sec =
        babel_now.tv_sec + (babel_now.tv_usec / 1000 + msecs) / 1000;
}

static void
ensure_space(struct interface *ifp, int space)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    if(babel_ifp->bufsize - babel_ifp->buffered < space)
        flushbuf(ifp);
}

static void
start_message(struct interface *ifp, int type, int len)
{
  babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    if(babel_ifp->bufsize - babel_ifp->buffered < len + 2)
        flushbuf(ifp);
    babel_ifp->sendbuf[babel_ifp->buffered++] = type;
    babel_ifp->sendbuf[babel_ifp->buffered++] = len;
}

static void
end_message(struct interface *ifp, int type, int bytes)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    assert(babel_ifp->buffered >= bytes + 2 &&
           babel_ifp->sendbuf[babel_ifp->buffered - bytes - 2] == type &&
           babel_ifp->sendbuf[babel_ifp->buffered - bytes - 1] == bytes);
    schedule_flush(ifp);
}

static void
accumulate_byte(struct interface *ifp, unsigned char value)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    babel_ifp->sendbuf[babel_ifp->buffered++] = value;
}

static void
accumulate_short(struct interface *ifp, unsigned short value)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    DO_HTONS(babel_ifp->sendbuf + babel_ifp->buffered, value);
    babel_ifp->buffered += 2;
}

static void
accumulate_bytes(struct interface *ifp,
                 const unsigned char *value, unsigned len)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    memcpy(babel_ifp->sendbuf + babel_ifp->buffered, value, len);
    babel_ifp->buffered += len;
}

static int
start_unicast_message(struct neighbour *neigh, int type, int len)
{
    if(unicast_neighbour) {
        if(neigh != unicast_neighbour ||
           unicast_buffered + len + 2 >=
           MIN(UNICAST_BUFSIZE, babel_get_if_nfo(neigh->ifp)->bufsize))
            flush_unicast(0);
    }
    if(!unicast_buffer)
        unicast_buffer = malloc(UNICAST_BUFSIZE);
    if(!unicast_buffer) {
        zlog_err("malloc(unicast_buffer): %s", safe_strerror(errno));
        return -1;
    }

    unicast_neighbour = neigh;

    unicast_buffer[unicast_buffered++] = type;
    unicast_buffer[unicast_buffered++] = len;
    return 1;
}

static void
end_unicast_message(struct neighbour *neigh, int type, int bytes)
{
    assert(unicast_neighbour == neigh && unicast_buffered >= bytes + 2 &&
           unicast_buffer[unicast_buffered - bytes - 2] == type &&
           unicast_buffer[unicast_buffered - bytes - 1] == bytes);
    schedule_unicast_flush(jitter(babel_get_if_nfo(neigh->ifp), 0));
}

static void
accumulate_unicast_byte(struct neighbour *neigh, unsigned char value)
{
    unicast_buffer[unicast_buffered++] = value;
}

static void
accumulate_unicast_short(struct neighbour *neigh, unsigned short value)
{
    DO_HTONS(unicast_buffer + unicast_buffered, value);
    unicast_buffered += 2;
}

static void
accumulate_unicast_bytes(struct neighbour *neigh,
                         const unsigned char *value, unsigned len)
{
    memcpy(unicast_buffer + unicast_buffered, value, len);
    unicast_buffered += len;
}

void
send_ack(struct neighbour *neigh, unsigned short nonce, unsigned short interval)
{
    int rc;
    debugf(BABEL_DEBUG_COMMON,"Sending ack (%04x) to %s on %s.",
           nonce, format_address(neigh->address), neigh->ifp->name);
    rc = start_unicast_message(neigh, MESSAGE_ACK, 2); if(rc < 0) return;
    accumulate_unicast_short(neigh, nonce);
    end_unicast_message(neigh, MESSAGE_ACK, 2);
    /* Roughly yields a value no larger than 3/2, so this meets the deadline */
    schedule_unicast_flush(roughly(interval * 6));
}

void
send_hello_noupdate(struct interface *ifp, unsigned interval)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    /* This avoids sending multiple hellos in a single packet, which breaks
       link quality estimation. */
    if(babel_ifp->have_buffered_hello)
        flushbuf(ifp);

    babel_ifp->hello_seqno = seqno_plus(babel_ifp->hello_seqno, 1);
    set_timeout(&babel_ifp->hello_timeout, babel_ifp->hello_interval);

    if(!if_up(ifp))
        return;

    debugf(BABEL_DEBUG_COMMON,"Sending hello %d (%d) to %s.",
           babel_ifp->hello_seqno, interval, ifp->name);

    start_message(ifp, MESSAGE_HELLO, 6);
    accumulate_short(ifp, 0);
    accumulate_short(ifp, babel_ifp->hello_seqno);
    accumulate_short(ifp, interval > 0xFFFF ? 0xFFFF : interval);
    end_message(ifp, MESSAGE_HELLO, 6);
    babel_ifp->have_buffered_hello = 1;
}

void
send_hello(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    send_hello_noupdate(ifp, (babel_ifp->hello_interval + 9) / 10);
    /* Send full IHU every 3 hellos, and marginal IHU each time */
    if(babel_ifp->hello_seqno % 3 == 0)
        send_ihu(NULL, ifp);
    else
        send_marginal_ihu(ifp);
}

void
flush_unicast(int dofree)
{
    struct sockaddr_in6 sin6;
    int rc;

    if(unicast_buffered == 0)
        goto done;

    if(!if_up(unicast_neighbour->ifp))
        goto done;

    /* Preserve ordering of messages */
    flushbuf(unicast_neighbour->ifp);

    if(check_bucket(unicast_neighbour->ifp)) {
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        memcpy(&sin6.sin6_addr, unicast_neighbour->address, 16);
        sin6.sin6_port = htons(protocol_port);
        sin6.sin6_scope_id = unicast_neighbour->ifp->ifindex;
        DO_HTONS(packet_header + 2, unicast_buffered);
        rc = babel_send(protocol_socket,
                        packet_header, sizeof(packet_header),
                        unicast_buffer, unicast_buffered,
                        (struct sockaddr*)&sin6, sizeof(sin6));
        if(rc < 0)
            zlog_err("send(unicast): %s", safe_strerror(errno));
    } else {
        zlog_err("Warning: bucket full, dropping unicast packet to %s if %s.",
                 format_address(unicast_neighbour->address),
                 unicast_neighbour->ifp->name);
    }

 done:
    VALGRIND_MAKE_MEM_UNDEFINED(unicast_buffer, UNICAST_BUFSIZE);
    unicast_buffered = 0;
    if(dofree && unicast_buffer) {
        free(unicast_buffer);
        unicast_buffer = NULL;
    }
    unicast_neighbour = NULL;
    unicast_flush_timeout.tv_sec = 0;
    unicast_flush_timeout.tv_usec = 0;
}

static void
really_send_update(struct interface *ifp,
                   const unsigned char *id,
                   const unsigned char *prefix, unsigned char plen,
                   unsigned short seqno, unsigned short metric,
                   unsigned char *channels, int channels_len)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    int add_metric, v4, real_plen, omit = 0;
    const unsigned char *real_prefix;
    unsigned short flags = 0;
    int channels_size;

    if(diversity_kind != DIVERSITY_CHANNEL)
        channels_len = -1;

    channels_size = channels_len >= 0 ? channels_len + 2 : 0;

    if(!if_up(ifp))
        return;

    add_metric = output_filter(id, prefix, plen, ifp->ifindex);
    if(add_metric >= INFINITY)
        return;

    metric = MIN(metric + add_metric, INFINITY);
    /* Worst case */
    ensure_space(ifp, 20 + 12 + 28);

    v4 = plen >= 96 && v4mapped(prefix);

    if(v4) {
        if(!babel_ifp->ipv4)
            return;
        if(!babel_ifp->have_buffered_nh ||
           memcmp(babel_ifp->buffered_nh, babel_ifp->ipv4, 4) != 0) {
            start_message(ifp, MESSAGE_NH, 6);
            accumulate_byte(ifp, 1);
            accumulate_byte(ifp, 0);
            accumulate_bytes(ifp, babel_ifp->ipv4, 4);
            end_message(ifp, MESSAGE_NH, 6);
            memcpy(babel_ifp->buffered_nh, babel_ifp->ipv4, 4);
            babel_ifp->have_buffered_nh = 1;
        }

        real_prefix = prefix + 12;
        real_plen = plen - 96;
    } else {
        if(babel_ifp->have_buffered_prefix) {
            while(omit < plen / 8 &&
                  babel_ifp->buffered_prefix[omit] == prefix[omit])
                omit++;
        }
        if(!babel_ifp->have_buffered_prefix || plen >= 48)
            flags |= 0x80;
        real_prefix = prefix;
        real_plen = plen;
    }

    if(!babel_ifp->have_buffered_id
       || memcmp(id, babel_ifp->buffered_id, 8) != 0) {
        if(real_plen == 128 && memcmp(real_prefix + 8, id, 8) == 0) {
            flags |= 0x40;
        } else {
            start_message(ifp, MESSAGE_ROUTER_ID, 10);
            accumulate_short(ifp, 0);
            accumulate_bytes(ifp, id, 8);
            end_message(ifp, MESSAGE_ROUTER_ID, 10);
        }
        memcpy(babel_ifp->buffered_id, id, 16);
        babel_ifp->have_buffered_id = 1;
    }

    start_message(ifp, MESSAGE_UPDATE, 10 + (real_plen + 7) / 8 - omit +
                  channels_size);
    accumulate_byte(ifp, v4 ? 1 : 2);
    accumulate_byte(ifp, flags);
    accumulate_byte(ifp, real_plen);
    accumulate_byte(ifp, omit);
    accumulate_short(ifp, (babel_ifp->update_interval + 5) / 10);
    accumulate_short(ifp, seqno);
    accumulate_short(ifp, metric);
    accumulate_bytes(ifp, real_prefix + omit, (real_plen + 7) / 8 - omit);
    /* Note that an empty channels TLV is different from no such TLV. */
    if(channels_len >= 0) {
        accumulate_byte(ifp, 2);
        accumulate_byte(ifp, channels_len);
        accumulate_bytes(ifp, channels, channels_len);
    }
    end_message(ifp, MESSAGE_UPDATE, 10 + (real_plen + 7) / 8 - omit +
                channels_size);

    if(flags & 0x80) {
        memcpy(babel_ifp->buffered_prefix, prefix, 16);
        babel_ifp->have_buffered_prefix = 1;
    }
}

static int
compare_buffered_updates(const void *av, const void *bv)
{
    const struct buffered_update *a = av, *b = bv;
    int rc, v4a, v4b, ma, mb;

    rc = memcmp(a->id, b->id, 8);
    if(rc != 0)
        return rc;

    v4a = (a->plen >= 96 && v4mapped(a->prefix));
    v4b = (b->plen >= 96 && v4mapped(b->prefix));

    if(v4a > v4b)
        return 1;
    else if(v4a < v4b)
        return -1;

    ma = (!v4a && a->plen == 128 && memcmp(a->prefix + 8, a->id, 8) == 0);
    mb = (!v4b && b->plen == 128 && memcmp(b->prefix + 8, b->id, 8) == 0);

    if(ma > mb)
        return -1;
    else if(mb > ma)
        return 1;

    if(a->plen < b->plen)
        return 1;
    else if(a->plen > b->plen)
        return -1;

    return memcmp(a->prefix, b->prefix, 16);
}

void
flushupdates(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = NULL;
    struct xroute *xroute;
    struct babel_route *route;
    const unsigned char *last_prefix = NULL;
    unsigned char last_plen = 0xFF;
    int i;

    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node)
            flushupdates(ifp_aux);
        return;
    }

    babel_ifp = babel_get_if_nfo(ifp);
    if(babel_ifp->num_buffered_updates > 0) {
        struct buffered_update *b = babel_ifp->buffered_updates;
        int n = babel_ifp->num_buffered_updates;

        babel_ifp->buffered_updates = NULL;
        babel_ifp->update_bufsize = 0;
        babel_ifp->num_buffered_updates = 0;

        if(!if_up(ifp))
            goto done;

        debugf(BABEL_DEBUG_COMMON,"  (flushing %d buffered updates on %s (%d))",
               n, ifp->name, ifp->ifindex);

        /* In order to send fewer update messages, we want to send updates
           with the same router-id together, with IPv6 going out before IPv4. */

        for(i = 0; i < n; i++) {
            route = find_installed_route(b[i].prefix, b[i].plen);
            if(route)
                memcpy(b[i].id, route->src->id, 8);
            else
                memcpy(b[i].id, myid, 8);
        }

        qsort(b, n, sizeof(struct buffered_update), compare_buffered_updates);

        for(i = 0; i < n; i++) {
            /* The same update may be scheduled multiple times before it is
               sent out.  Since our buffer is now sorted, it is enough to
               compare with the previous update. */

            if(last_prefix) {
                if(b[i].plen == last_plen &&
                   memcmp(b[i].prefix, last_prefix, 16) == 0)
                    continue;
            }

            xroute = find_xroute(b[i].prefix, b[i].plen);
            route = find_installed_route(b[i].prefix, b[i].plen);

            if(xroute && (!route || xroute->metric <= kernel_metric)) {
                really_send_update(ifp, myid,
                                   xroute->prefix, xroute->plen,
                                   myseqno, xroute->metric,
                                   NULL, 0);
                last_prefix = xroute->prefix;
                last_plen = xroute->plen;
            } else if(route) {
                unsigned char channels[DIVERSITY_HOPS];
                int chlen;
                struct interface *route_ifp = route->neigh->ifp;
                struct babel_interface *babel_route_ifp = NULL;
                unsigned short metric;
                unsigned short seqno;

                seqno = route->seqno;
                metric =
                    route_interferes(route, ifp) ?
                    route_metric(route) :
                    route_metric_noninterfering(route);

                if(metric < INFINITY)
                    satisfy_request(route->src->prefix, route->src->plen,
                                    seqno, route->src->id, ifp);
                if((babel_ifp->flags & BABEL_IF_SPLIT_HORIZON) &&
                   route->neigh->ifp == ifp)
                    continue;

                babel_route_ifp = babel_get_if_nfo(route_ifp);
                if(babel_route_ifp->channel ==BABEL_IF_CHANNEL_NONINTERFERING) {
                    memcpy(channels, route->channels, DIVERSITY_HOPS);
                } else {
                    if(babel_route_ifp->channel == BABEL_IF_CHANNEL_UNKNOWN)
                        channels[0] = BABEL_IF_CHANNEL_INTERFERING;
                    else {
                        assert(babel_route_ifp->channel > 0 &&
                               babel_route_ifp->channel <= 255);
                        channels[0] = babel_route_ifp->channel;
                    }
                    memcpy(channels + 1, route->channels, DIVERSITY_HOPS - 1);
                }

                chlen = channels_len(channels);
                really_send_update(ifp, route->src->id,
                                   route->src->prefix,
                                   route->src->plen,
                                   seqno, metric,
                                   channels, chlen);
                update_source(route->src, seqno, metric);
                last_prefix = route->src->prefix;
                last_plen = route->src->plen;
            } else {
            /* There's no route for this prefix.  This can happen shortly
               after an xroute has been retracted, so send a retraction. */
                really_send_update(ifp, myid, b[i].prefix, b[i].plen,
                                   myseqno, INFINITY, NULL, -1);
            }
        }
        schedule_flush_now(ifp);
    done:
        free(b);
    }
    babel_ifp->update_flush_timeout.tv_sec = 0;
    babel_ifp->update_flush_timeout.tv_usec = 0;
}

static void
schedule_update_flush(struct interface *ifp, int urgent)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    unsigned msecs;
    msecs = update_jitter(babel_ifp, urgent);
    if(babel_ifp->update_flush_timeout.tv_sec != 0 &&
       timeval_minus_msec(&babel_ifp->update_flush_timeout, &babel_now) < msecs)
        return;
    set_timeout(&babel_ifp->update_flush_timeout, msecs);
}

static void
buffer_update(struct interface *ifp,
              const unsigned char *prefix, unsigned char plen)
{
    babel_interface_nfo *babel_ifp = babel_get_if_nfo(ifp);
    if(babel_ifp->num_buffered_updates > 0 &&
       babel_ifp->num_buffered_updates >= babel_ifp->update_bufsize)
        flushupdates(ifp);

    if(babel_ifp->update_bufsize == 0) {
        int n;
        assert(babel_ifp->buffered_updates == NULL);
        /* Allocate enough space to hold a full update.  Since the
           number of installed routes will grow over time, make sure we
           have enough space to send a full-ish frame. */
        n = installed_routes_estimate() + xroutes_estimate() + 4;
        n = MAX(n, babel_ifp->bufsize / 16);
    again:
        babel_ifp->buffered_updates = malloc(n *sizeof(struct buffered_update));
        if(babel_ifp->buffered_updates == NULL) {
            zlog_err("malloc(buffered_updates): %s", safe_strerror(errno));
            if(n > 4) {
                /* Try again with a tiny buffer. */
                n = 4;
                goto again;
            }
            return;
        }
        babel_ifp->update_bufsize = n;
        babel_ifp->num_buffered_updates = 0;
    }

    memcpy(babel_ifp->buffered_updates[babel_ifp->num_buffered_updates].prefix,
           prefix, 16);
    babel_ifp->buffered_updates[babel_ifp->num_buffered_updates].plen = plen;
    babel_ifp->num_buffered_updates++;
}

static void
buffer_update_callback(struct babel_route *route, void *closure)
{
    buffer_update((struct interface*)closure,
                  route->src->prefix, route->src->plen);
}

void
send_update(struct interface *ifp, int urgent,
            const unsigned char *prefix, unsigned char plen)
{
    babel_interface_nfo *babel_ifp = NULL;

    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        struct babel_route *route;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node)
            send_update(ifp_aux, urgent, prefix, plen);
        if(prefix) {
            /* Since flushupdates only deals with non-wildcard interfaces, we
               need to do this now. */
            route = find_installed_route(prefix, plen);
            if(route && route_metric(route) < INFINITY)
                satisfy_request(prefix, plen, route->src->seqno, route->src->id,
                                NULL);
        }
        return;
    }

    if(!if_up(ifp))
        return;

    babel_ifp = babel_get_if_nfo(ifp);
    if(prefix) {
        debugf(BABEL_DEBUG_COMMON,"Sending update to %s for %s.",
               ifp->name, format_prefix(prefix, plen));
        buffer_update(ifp, prefix, plen);
    } else {
        send_self_update(ifp);
        debugf(BABEL_DEBUG_COMMON,"Sending update to %s for any.", ifp->name);
        for_all_installed_routes(buffer_update_callback, ifp);
        set_timeout(&babel_ifp->update_timeout, babel_ifp->update_interval);
        babel_ifp->last_update_time = babel_now.tv_sec;
    }
    schedule_update_flush(ifp, urgent);
}

void
send_update_resend(struct interface *ifp,
                   const unsigned char *prefix, unsigned char plen)
{
    assert(prefix != NULL);

    send_update(ifp, 1, prefix, plen);
    record_resend(RESEND_UPDATE, prefix, plen, 0, 0, NULL, resend_delay);
}

void
send_wildcard_retraction(struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = NULL;
    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node)
            send_wildcard_retraction(ifp_aux);
        return;
    }

    if(!if_up(ifp))
        return;

    babel_ifp = babel_get_if_nfo(ifp);
    start_message(ifp, MESSAGE_UPDATE, 10);
    accumulate_byte(ifp, 0);
    accumulate_byte(ifp, 0x40);
    accumulate_byte(ifp, 0);
    accumulate_byte(ifp, 0);
    accumulate_short(ifp, 0xFFFF);
    accumulate_short(ifp, myseqno);
    accumulate_short(ifp, 0xFFFF);
    end_message(ifp, MESSAGE_UPDATE, 10);

    babel_ifp->have_buffered_id = 0;
}

void
update_myseqno()
{
    myseqno = seqno_plus(myseqno, 1);
    seqno_time = babel_now;
}

static void
send_xroute_update_callback(struct xroute *xroute, void *closure)
{
    struct interface *ifp = (struct interface*)closure;
    send_update(ifp, 0, xroute->prefix, xroute->plen);
}

void
send_self_update(struct interface *ifp)
{
    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node) {
            if(!if_up(ifp_aux))
                continue;
            send_self_update(ifp_aux);
        }
        return;
    }

    debugf(BABEL_DEBUG_COMMON,"Sending self update to %s.", ifp->name);
    for_all_xroutes(send_xroute_update_callback, ifp);
}

void
send_ihu(struct neighbour *neigh, struct interface *ifp)
{
    babel_interface_nfo *babel_ifp = NULL;
    int rxcost, interval;
    int ll;

    if(neigh == NULL && ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node) {
            if(if_up(ifp_aux))
                continue;
            send_ihu(NULL, ifp_aux);
        }
        return;
    }

    if(neigh == NULL) {
        struct neighbour *ngh;
        FOR_ALL_NEIGHBOURS(ngh) {
            if(ngh->ifp == ifp)
                send_ihu(ngh, ifp);
        }
        return;
    }


    if(ifp && neigh->ifp != ifp)
        return;

    ifp = neigh->ifp;
    babel_ifp = babel_get_if_nfo(ifp);
    if(!if_up(ifp))
        return;

    rxcost = neighbour_rxcost(neigh);
    interval = (babel_ifp->hello_interval * 3 + 9) / 10;

    /* Conceptually, an IHU is a unicast message.  We usually send them as
       multicast, since this allows aggregation into a single packet and
       avoids an ARP exchange.  If we already have a unicast message queued
       for this neighbour, however, we might as well piggyback the IHU. */
    debugf(BABEL_DEBUG_COMMON,"Sending %sihu %d on %s to %s.",
           unicast_neighbour == neigh ? "unicast " : "",
           rxcost,
           neigh->ifp->name,
           format_address(neigh->address));

    ll = linklocal(neigh->address);

    if(unicast_neighbour != neigh) {
        start_message(ifp, MESSAGE_IHU, ll ? 14 : 22);
        accumulate_byte(ifp, ll ? 3 : 2);
        accumulate_byte(ifp, 0);
        accumulate_short(ifp, rxcost);
        accumulate_short(ifp, interval);
        if(ll)
            accumulate_bytes(ifp, neigh->address + 8, 8);
        else
            accumulate_bytes(ifp, neigh->address, 16);
        end_message(ifp, MESSAGE_IHU, ll ? 14 : 22);
    } else {
        int rc;
        rc = start_unicast_message(neigh, MESSAGE_IHU, ll ? 14 : 22);
        if(rc < 0) return;
        accumulate_unicast_byte(neigh, ll ? 3 : 2);
        accumulate_unicast_byte(neigh, 0);
        accumulate_unicast_short(neigh, rxcost);
        accumulate_unicast_short(neigh, interval);
        if(ll)
            accumulate_unicast_bytes(neigh, neigh->address + 8, 8);
        else
            accumulate_unicast_bytes(neigh, neigh->address, 16);
        end_unicast_message(neigh, MESSAGE_IHU, ll ? 14 : 22);
    }
}

/* Send IHUs to all marginal neighbours */
void
send_marginal_ihu(struct interface *ifp)
{
    struct neighbour *neigh;
    FOR_ALL_NEIGHBOURS(neigh) {
        if(ifp && neigh->ifp != ifp)
            continue;
        if(neigh->txcost >= 384 || (neigh->reach & 0xF000) != 0xF000)
            send_ihu(neigh, ifp);
    }
}

void
send_request(struct interface *ifp,
             const unsigned char *prefix, unsigned char plen)
{
    int v4, len;

    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node) {
            if(if_up(ifp_aux))
                continue;
            send_request(ifp_aux, prefix, plen);
        }
        return;
    }

    /* make sure any buffered updates go out before this request. */
    flushupdates(ifp);

    if(!if_up(ifp))
        return;

    debugf(BABEL_DEBUG_COMMON,"sending request to %s for %s.",
           ifp->name, prefix ? format_prefix(prefix, plen) : "any");
    v4 = plen >= 96 && v4mapped(prefix);
    len = !prefix ? 2 : v4 ? 6 : 18;

    start_message(ifp, MESSAGE_REQUEST, len);
    accumulate_byte(ifp, !prefix ? 0 : v4 ? 1 : 2);
    accumulate_byte(ifp, !prefix ? 0 : v4 ? plen - 96 : plen);
    if(prefix) {
        if(v4)
            accumulate_bytes(ifp, prefix + 12, 4);
        else
            accumulate_bytes(ifp, prefix, 16);
    }
    end_message(ifp, MESSAGE_REQUEST, len);
}

void
send_unicast_request(struct neighbour *neigh,
                     const unsigned char *prefix, unsigned char plen)
{
    int rc, v4, len;

    /* make sure any buffered updates go out before this request. */
    flushupdates(neigh->ifp);

    debugf(BABEL_DEBUG_COMMON,"sending unicast request to %s for %s.",
           format_address(neigh->address),
           prefix ? format_prefix(prefix, plen) : "any");
    v4 = plen >= 96 && v4mapped(prefix);
    len = !prefix ? 2 : v4 ? 6 : 18;

    rc = start_unicast_message(neigh, MESSAGE_REQUEST, len);
    if(rc < 0) return;
    accumulate_unicast_byte(neigh, !prefix ? 0 : v4 ? 1 : 2);
    accumulate_unicast_byte(neigh, !prefix ? 0 : v4 ? plen - 96 : plen);
    if(prefix) {
        if(v4)
            accumulate_unicast_bytes(neigh, prefix + 12, 4);
        else
            accumulate_unicast_bytes(neigh, prefix, 16);
    }
    end_unicast_message(neigh, MESSAGE_REQUEST, len);
}

void
send_multihop_request(struct interface *ifp,
                      const unsigned char *prefix, unsigned char plen,
                      unsigned short seqno, const unsigned char *id,
                      unsigned short hop_count)
{
    int v4, pb, len;

    /* Make sure any buffered updates go out before this request. */
    flushupdates(ifp);

    if(ifp == NULL) {
      struct interface *ifp_aux;
      struct listnode *linklist_node = NULL;
        FOR_ALL_INTERFACES(ifp_aux, linklist_node) {
            if(!if_up(ifp_aux))
                continue;
            send_multihop_request(ifp_aux, prefix, plen, seqno, id, hop_count);
        }
        return;
    }

    if(!if_up(ifp))
        return;

    debugf(BABEL_DEBUG_COMMON,"Sending request (%d) on %s for %s.",
           hop_count, ifp->name, format_prefix(prefix, plen));
    v4 = plen >= 96 && v4mapped(prefix);
    pb = v4 ? ((plen - 96) + 7) / 8 : (plen + 7) / 8;
    len = 6 + 8 + pb;

    start_message(ifp, MESSAGE_MH_REQUEST, len);
    accumulate_byte(ifp, v4 ? 1 : 2);
    accumulate_byte(ifp, v4 ? plen - 96 : plen);
    accumulate_short(ifp, seqno);
    accumulate_byte(ifp, hop_count);
    accumulate_byte(ifp, 0);
    accumulate_bytes(ifp, id, 8);
    if(prefix) {
        if(v4)
            accumulate_bytes(ifp, prefix + 12, pb);
        else
            accumulate_bytes(ifp, prefix, pb);
    }
    end_message(ifp, MESSAGE_MH_REQUEST, len);
}

void
send_unicast_multihop_request(struct neighbour *neigh,
                              const unsigned char *prefix, unsigned char plen,
                              unsigned short seqno, const unsigned char *id,
                              unsigned short hop_count)
{
    int rc, v4, pb, len;

    /* Make sure any buffered updates go out before this request. */
    flushupdates(neigh->ifp);

    debugf(BABEL_DEBUG_COMMON,"Sending multi-hop request to %s for %s (%d hops).",
           format_address(neigh->address),
           format_prefix(prefix, plen), hop_count);
    v4 = plen >= 96 && v4mapped(prefix);
    pb = v4 ? ((plen - 96) + 7) / 8 : (plen + 7) / 8;
    len = 6 + 8 + pb;

    rc = start_unicast_message(neigh, MESSAGE_MH_REQUEST, len);
    if(rc < 0) return;
    accumulate_unicast_byte(neigh, v4 ? 1 : 2);
    accumulate_unicast_byte(neigh, v4 ? plen - 96 : plen);
    accumulate_unicast_short(neigh, seqno);
    accumulate_unicast_byte(neigh, hop_count);
    accumulate_unicast_byte(neigh, 0);
    accumulate_unicast_bytes(neigh, id, 8);
    if(prefix) {
        if(v4)
            accumulate_unicast_bytes(neigh, prefix + 12, pb);
        else
            accumulate_unicast_bytes(neigh, prefix, pb);
    }
    end_unicast_message(neigh, MESSAGE_MH_REQUEST, len);
}

void
send_request_resend(struct neighbour *neigh,
                    const unsigned char *prefix, unsigned char plen,
                    unsigned short seqno, unsigned char *id)
{
    if(neigh)
        send_unicast_multihop_request(neigh, prefix, plen, seqno, id, 127);
    else
        send_multihop_request(NULL, prefix, plen, seqno, id, 127);

    record_resend(RESEND_REQUEST, prefix, plen, seqno, id,
                  neigh ? neigh->ifp : NULL, resend_delay);
}

void
handle_request(struct neighbour *neigh, const unsigned char *prefix,
               unsigned char plen, unsigned char hop_count,
               unsigned short seqno, const unsigned char *id)
{
    struct xroute *xroute;
    struct babel_route *route;
    struct neighbour *successor = NULL;

    xroute = find_xroute(prefix, plen);
    route = find_installed_route(prefix, plen);

    if(xroute && (!route || xroute->metric <= kernel_metric)) {
        if(hop_count > 0 && memcmp(id, myid, 8) == 0) {
            if(seqno_compare(seqno, myseqno) > 0) {
                if(seqno_minus(seqno, myseqno) > 100) {
                    /* Hopelessly out-of-date request */
                    return;
                }
                update_myseqno();
            }
        }
        send_update(neigh->ifp, 1, prefix, plen);
        return;
    }

    if(route &&
       (memcmp(id, route->src->id, 8) != 0 ||
        seqno_compare(seqno, route->seqno) <= 0)) {
        send_update(neigh->ifp, 1, prefix, plen);
        return;
    }

    if(hop_count <= 1)
        return;

    if(route && memcmp(id, route->src->id, 8) == 0 &&
       seqno_minus(seqno, route->seqno) > 100) {
        /* Hopelessly out-of-date */
        return;
    }

    if(request_redundant(neigh->ifp, prefix, plen, seqno, id))
        return;

    /* Let's try to forward this request. */
    if(route && route_metric(route) < INFINITY)
        successor = route->neigh;

    if(!successor || successor == neigh) {
        /* We were about to forward a request to its requestor.  Try to
           find a different neighbour to forward the request to. */
        struct babel_route *other_route;

        other_route = find_best_route(prefix, plen, 0, neigh);
        if(other_route && route_metric(other_route) < INFINITY)
            successor = other_route->neigh;
    }

    if(!successor || successor == neigh)
        /* Give up */
        return;

    send_unicast_multihop_request(successor, prefix, plen, seqno, id,
                                  hop_count - 1);
    record_resend(RESEND_REQUEST, prefix, plen, seqno, id,
                  neigh->ifp, 0);
}
