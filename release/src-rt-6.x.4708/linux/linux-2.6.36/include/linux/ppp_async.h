#ifndef	 LINUX_PPP_ASYNC_H
#define  LINUX_PPP_ASYNC_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/tty.h>
#include <linux/ppp_async.h>
#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/crc-ccitt.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/ppp_channel.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/string.h>

#define OBUFSIZE        4096

/* Structure for storing local state. */
struct asyncppp {
        struct tty_struct *tty;
        unsigned int    flags;
        unsigned int    state;
        unsigned int    rbits;
        int             mru;
        spinlock_t      xmit_lock;
        spinlock_t      recv_lock;
        unsigned long   xmit_flags;
        u32             xaccm[8];
        u32             raccm;
        unsigned int    bytes_sent;
        unsigned int    bytes_rcvd;

        struct sk_buff  *tpkt;
        int             tpkt_pos;
        u16             tfcs;
        unsigned char   *optr;
        unsigned char   *olim;
        unsigned long   last_xmit;

        struct sk_buff  *rpkt;
        int             lcp_fcs;
        struct sk_buff_head rqueue;

        struct tasklet_struct tsk;

        atomic_t        refcnt;
        struct semaphore dead_sem;
        struct ppp_channel chan;        /* interface to generic ppp layer */
        unsigned char   obuf[OBUFSIZE];
};

#endif
