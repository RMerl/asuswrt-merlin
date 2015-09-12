/*
 * +----------------------------------------------------------------------------
 * HND Forwarder between GMAC and "HW switching capable WLAN interfaces".
 *
 * Northstar router includes 3 GMAC ports from the integrated switch to the Host
 * (single or dual core) CPU. The integrated switch provides advanced ethernet
 * hardware switching functions, similar to that of a Linux software bridge.
 * In Northstar, this integrated HW switch is responsible for bridging packets
 * between the 4 LAN ports. All LAN ports are seen by the ethernet network
 * device driver as a single "vlan1" interface. This single vlan1 interface
 * represents the collection of physical LAN ports on the switch, without
 * having to create a seperate interface per LAN port and  adding each one of
 * these LAN interfaces to the default Linux software LAN bridge "br0".
 * The hardware switch allows the LAN ports to be segregated into multiple
 * bridges using VLAN (Independent VLAN Learning Mode). Again, each subset of
 * physical LAN ports are represented by a single interface, namely "vlanX".
 *
 * The 3 GMAC configuration treats the primary WLAN interface as just another
 * LAN interface (albeit with a WLAN 802.11 MAC as opposed to an Ethernet 802.3
 * MAC). Two of the three GMACs are dedicated for binding the primary WLAN
 * interfaces to the HW switch which performs the LAN to WLAN bridging function.
 * These two GMACs are referred to as Forwarding GMACs.
 * The third GMAC is used to connect the switch to the Linux router network
 * stack, by making all LAN and WLAN ports appear as a single vlan1 interface
 * that is added to the software Linux bridge "br0". This GMAC is referred to
 * as the Networking GMAC.
 *
 * Similar to LAN to WAN routing, where LAN originated packets would be flooded
 * to the WAN port via the br0, likewise, WLAN originated packets would re-enter
 * Linux network stack via the 3rd GMAC. Software Cut-Through-Forwarding CTF
 * will accelerate WLAN <-> WAN traffic. When the hardware Flow Accelerator is
 * enabled, WLAN <-> WAN traffic need not re-enter the host CPU, other than the
 * first few packets that are needed to establish the flows in the FA, post
 * DPI or Security related flow classification functions.
 *
 * +----------------------------------------------------------------------------
 *
 * A typical GMAC configuration is:
 *   GMAC#0 - port#5 - fwd0 <---> wl0/eth1 (radio 0) on CPU core0
 *   GMAC#1 - port#7 - fwd1 <---> wl1/eth2 (radio 1) on CPU core1
 *
 *   GMAC#2 - port#8 - eth0 <--- vlan1 ---> br0
 *
 * Here, analogous to all physical LAN ports are managed by vlan1, likewise the
 * port#5 and port#7 are also treated as "LAN" ports hosting a "WiFI" MAC.
 *
 * In such a configuration, the primary wl interfaces wl0 and wl1 (i.e. eth1 and
 * eth2) are not added to the Linux bridge br0, as the integrated HW switch will
 * handle all LAN <---> WLAN bridging functions. All LAN ports and the WLAN
 * interface are represented by vlan1. vlan1 is added to the Linux bridge br0.
 *
 * The switch port#8, is IMP port capable (Integrated Management Port). All
 * routed LAN and WAN traffic are directed to this port, where the eth0 network
 * interface is responsible for directing to the network stack. The eth0 network
 * interface uses the presence of a VLAN=1 to differentiate from WAN
 * originating packets.
 *
 * The same ethernet driver is instantiated per GMAC, and is flavored as a
 * Forwarder (GMAC#0 and GMAC#1) or a Network interface (GMAC#2). The Forwarder
 * GMAC(s) are not added to any Linux SW bridge, and are responsible to
 * receive/transmit packets from/to the integrated switch on behalf of the
 * HW switching capable WLAN interfaces.
 *
 * hndfwd.h provides the API for the GMAC Fwder device driver and wl driver
 * to register their respective transmit bypass handlers.
 *
 * An upstream forwarder object forwards traffic received from a GMAC to a
 * WLAN intreface for transmission. Likewise a downstream forwarder object
 * forwards traffic received from a WLAN interface to the GMAC. The upstream
 * and downstream forwarder, together act as a bi-directional socket.
 *
 * In a single core Northstar (47081), a pair of bidirectional forwarder objects
 * are maintained, indexed by the GMAC and WLAN radio fwder_unit number.
 *
 * In a dual core SMP Northstar, the bidirectional forwarder objects are defined
 * as per CPU instances. Each GMAC forwarder driver and mated WLAN interface are
 * tied to the appropriate CPU core using their respective fwder_unit number
 * and they access the per CPU core forwarder object by using the fwder_unit
 * number to retrieve the per CPU instance.
 *
 * In Atlas, where multiple radios may share the same IRQ, the two radios must
 * be hosted on the same CPU core. A "fwder_cpumap" nvram variable defines
 * the mode, channel, band, irq, and cpucore assignment, for all (3) radios.
 * The nvram variable is parsed and a unique fwder_unit number is assigned to
 * each radio such that radios that shared IRQ will use the same GMAC fwder.
 *
 * MultiBSS Radio:
 * Packets forwarded by the integrated switch only identify a port on which a
 * radio resides. In a Multi BSS configuration, a radio may host multipe virtual
 * interfaces that may avail of HW switching ("br0"). Each primary or virtual
 * interface registers an explicit Linux netdevice and WLAN interface in which
 * context the transmission occurs. In such a MBSS configuration, a packet
 * forwarded by a GMAC for WLAN transmission needs to first identify the virtual
 * interface, detected by the Ethernet MAC DA.
 *
 * +----------------------------------------------------------------------------
 *
 * WLAN Offload Forwarding Assist (WOFA)
 * HND Forwarder provides a Mac address to interface mapping table using an
 * abstraction of a dictionary of symbols. A symbol is a 6 Byte Ethernet MAC
 * address. A single dictionary is instantiated per GMAC forwarder pair.
 *
 * In this use case, the dictionary is managed by the WLAN driver. Symbols are
 * added or deleted on Association/Reassociation and Deassociation events
 * respectively, representing the address associated with the station.
 * When a symbol is added to the WOFA dictionary an opaque metadata is also
 * added. In the NIC mode, this metadata is the network device associated with
 * the WLAN interface. In the DGL mode, the metadata may encode the flow ring.
 * The metadata may be used to locate the wl_info/dhd_info, the interface and
 * any other information such as the queue associated with the flow ring.
 *
 * In the GMAC to WLAN direction, a lookup of the MAC DstAddress is performed
 * within wl_forward(bound bypass WLAN forwarding handler). The lookup returns
 * the WLAN interface (from which the registered Linux net_device and wl_info
 * object is derived).
 *
 * In the wl_sendup, path a lookup is also performed to determine whether the
 * packet has to be locally bridged. WOFA's bloom filter is used to quickly
 * terminate the lookup, and proceed with forwarding to the GMAC fwder.
 *
 * Broadcast packets would be flooded to all bound wl interfaces.
 *
 * If deassociation events may get dropped, a periodic synchronization of the
 * dictionary with the list of associated stations needs to be performed.
 *
 * WOFA's uses a 3 step lookup:
 * Stage 1. WOFA caches the most recent hit entry. If this entry matches, then
 *          the hash table is directly looked up.
 * Stage 2. A single hash based bloom filter is looked up. A negative filter
 *          avoids the need to further search the hash table.
 * Stage 3. A search of the hash table using a 4bin per bucket search is used.
 *
 * In devices where the source port number can be determined (BRCM tag), the
 * stage 1 lookup can benefit from back-to-back packets arriving for the same
 * Mac DstAddress.
 *
 * The bloom filter uses a simple multi word bitmap and a single hash function
 * for filtering. The bloom filter mwbmap assumes a 32bit architecture. As the
 * bloom filter can have false positives, an exact match by searching the hash
 * table is performed. The bloom-filter serves in quickly determining whether
 * a further exact match lookup is necessary. This is done to limit CPU cycles
 * for packets that are not destined to WOFA managed interfaces.
 *
 * +----------------------------------------------------------------------------
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * +----------------------------------------------------------------------------
 */

#ifndef _hndfwd_h_
#define _hndfwd_h_

/* Standalone WOFA (mac address dictionary) */
struct wofa;                    /* WOFA Address Resolution Logic */
#define WOFA_NOOP               do { /* noop */ } while (0)

#define WOFA_NULL               ((struct wofa *)NULL)
#define WOFA_FAILURE            (-1)
#define WOFA_SUCCESS            (0)

#define WOFA_DATA_INVALID        ((uintptr_t)(~0)) /* symbol to data mapping */


struct fwder;                   /* Forwarder object */
#define FWDER_NOOP              do { /* noop */ } while (0)

#define FWDER_NULL              (NULL)
#define FWDER_FAILURE           (-1)
#define FWDER_SUCCESS           (0)

#define FWDER_WOFA_INVALID		(WOFA_DATA_INVALID) /* uintptr_t ~0 */

#if defined(BCM_GMAC3)

/** WOFA Dictionary and Symbol dimensioning */
#define WOFA_DICT_SYMBOL_SIZE   (6) /* Size of a Mac Address */
#define WOFA_DICT_BKT_MAX       (1 << NBBY)
#define WOFA_DICT_BIN_MAX       (4) /* collision list */
#define WOFA_DICT_SYMBOLS_MAX   (WOFA_DICT_BKT_MAX * WOFA_DICT_BIN_MAX)

/** WOFA Bloom Filter sizing. */
#define WOFA_BLOOMFILTER_BITS   (64 * 1024) /* 16bit hash index */
#define WOFA_BLOOMFILTER_WORDS  (WOFA_BLOOMFILTER_BITS / 32)

/** WOFA Cached recent hit per LAN port|WLAN Interface. */
#define WOFA_MAX_PORTS          (16 + 1)


/** Enable/Disable debugging of the WOFA */
#if defined(BCMDBG)
#define WOFA_DEBUG              2
#else
#define WOFA_DEBUG              0
#endif

#define WOFA_ERROR(args)        printk args

/* Formatted Etherned Mac Address display */
#define __EFMT                  "%02X:%02X:%02X:%02X:%02X:%02X "
#define __EVAL(e)               *((e) + 0), *((e) + 1), *((e) + 2), \
	                            *((e) + 3), *((e) + 4), *((e) + 5)
#if (WOFA_DEBUG >= 1)
#define WOFA_ASSERT(exp)        ASSERT(exp)
#define WOFA_WARN(args)         printk args
#else
#define WOFA_ASSERT(exp)        WOFA_NOOP
#define WOFA_WARN(args)         WOFA_NOOP
#endif /* WOFA_DEBUG < 1 */

#if (WOFA_DEBUG >= 2)
#define WOFA_TRACE(args)        printk args
#else
#define WOFA_TRACE(args)        WOFA_NOOP
#endif /* WOFA_DEBUG < 2 */

#if (WOFA_DEBUG >= 3)
#define WOFA_PTRACE(args)       printk args
#else
#define WOFA_PTRACE(args)       WOFA_NOOP
#endif /* WOFA_DEBUG < 3 */

#define WOFA_ALIGN16(sym)       WOFA_ASSERT((((uintptr)sym) & 1) == 0)

/** Enable/Disable statistics in HND WOFA . */
/* #define WOFA_STATS */

#if defined(WOFA_STATS)
#define WOFA_STATS_CLR(x)       (x) = 0U
#define WOFA_STATS_ADD(x, c)    (x) = (x) + (c)
#define WOFA_STATS_INCR(x)      (x) = (x) + 1
#else  /* ! WOFA_STATS */
#define WOFA_STATS_CLR(x)       WOFA_NOOP
#define WOFA_STATS_ADD(x, c)    WOFA_NOOP
#define WOFA_STATS_INCR(x)      WOFA_NOOP
#endif /* ! WOFA_STATS */

struct wofa * wofa_init(void);
void wofa_fini(struct wofa * wofa);
int wofa_add(struct wofa * wofa, uint16 * symbol, uintptr_t data);
int wofa_del(struct wofa * wofa, uint16 * symbol, uintptr_t data);
int wofa_clr(struct wofa * wofa, uintptr_t data);
uintptr_t wofa_lkup(struct wofa * wofa, uint16 * symbol, const int port);

struct bcmstrbuf;
void wofa_dump(struct bcmstrbuf *b, struct wofa * wofa);


/** Enable/Disable debugging of the Hnd Forwarder. */
#if defined(BCMDBG)
#define FWDER_DEBUG             2
#else
#define FWDER_DEBUG             0
#endif

#define FWDER_ERROR(args)       printk args

#if (FWDER_DEBUG >= 1)
#define FWDER_ASSERT(exp)       ASSERT(exp)
#define FWDER_WARN(args)        printk args
#else
#define FWDER_ASSERT(exp)       FWDER_NOOP
#define FWDER_WARN(args)        FWDER_NOOP
#endif /* FWDER_DEBUG < 1 */

#if (FWDER_DEBUG >= 2)
#define FWDER_TRACE(args)       printk args
#else
#define FWDER_TRACE(args)       FWDER_NOOP
#endif /* FWDER_DEBUG < 2 */

#if (FWDER_DEBUG >= 3)
#define FWDER_PTRACE(args)      printk args
#else
#define FWDER_PTRACE(args)      FWDER_NOOP
#endif /* FWDER_DEBUG < 3 */

/** Enable/Disable statistics in HND Forwarder. */
/* #define FWDER_STATS */

#if defined(FWDER_STATS)
#define FWDER_STATS_CLR(x)     (x) = 0U
#define FWDER_STATS_ADD(x, c)  (x) = (x) + (c)
#define FWDER_STATS_INCR(x)    (x) = (x) + 1
#else  /* ! FWDER_STATS */
#define FWDER_STATS_CLR(x)     FWDER_NOOP
#define FWDER_STATS_ADD(x, c)  FWDER_NOOP
#define FWDER_STATS_INCR(x)    FWDER_NOOP
#endif /* ! FWDER_STATS */

/** Nvram configuration used by Forwarder. */
#define FWDER_WLIFS_NVAR       "fwd_wlandevs" /* fwder_bind() */

/** Nvram radio to CPU mapping specification for up to FWDER_MAX_RADIO radios */
#define FWDER_CPUMAP_NVAR      "fwd_cpumap"


/* Default Radio CPU Map for ATLAS BCM4709acdcrh (Config #1) */
#define BCM4709ACDCRH_CPUMAP    "d:l:5:163:0 d:x:2:163:0 d:u:5:169:1"
#define FWDER_CPUMAP_DEFAULT    BCM4709ACDCRH_CPUMAP

/* Number of Forwarder (sockets) */
#define FWDER_MAX_UNIT          (2)

/* Total number of WLAN radios */
#define FWDER_MAX_RADIO         (4)

/* Total number of virtual devices per radio */
#define FWDER_MAX_IF            (16)

/** Mapping radios to GMAC forwarders.
 * All radios with even unit numbers (begining from unit#0) are hosted on fwd0,
 * and radios with odd unit numbers (beginning from unit#1) are hosted in fwd1.
 * Virtual interfaces are identified by the subunit [0..FWDER_MAX_IF).
 */
#define FWDER_MAX_DEV           (FWDER_MAX_RADIO * FWDER_MAX_IF)

/** WOFA uses a cached lookup scheme. Packets originating from a WLAN interface
 * use the WLAN interface's subunit number [0..FWDER_MAX_IF). Packets
 * originating from a GMAC use the FWDER_GMAC_PORT as the rx port number.
 */
#define FWDER_GMAC_PORT         (FWDER_MAX_IF)

/** HW switching capability of a WLAN interface.
 * Each HW switching capable wlif maintains a pointer to the upstream forwarder.
 * Macro WLIF_FWDER() is provided in wl_linux.h.
 */

/* forward declaration */
struct fwder;                   /* Forwarder object */
struct sk_buff;
struct net_device;

/** Direction of a Hnd Forwarder object.
 * Directions are defined with respect to WLAN MAC processing:
 * - WLAN MAC RX => Upstream
 * - WLAN MAC TX => Downstream
 */
typedef enum fwder_dir {
	FWDER_UPSTREAM,             /* WL##(RX) -> GMAC(TX) Forwarding Handler */
	FWDER_DNSTREAM,             /* GMAC(RX) -> Wl##(TX) Forwarding Handler */
	FWDER_MAX_DIR
} fwder_dir_t;

/** Mode of operation of a HW switching capable interface.
 * When a HW switching capable interface is made operational, it will
 * register it's mode of operation (NIC or DGL) with the forwarder.
 */
typedef enum fwder_mode {
	FWDER_NIC_MODE,             /* NIC Mode WLAN interface */
	FWDER_DNG_MODE,             /* Offload Mode Dongle Host Driver interface */
	FWDER_MAX_MODE
} fwder_mode_t;

/** Radio channel */
typedef enum fwder_chan {
    FWDER_UPPER_CHAN,
    FWDER_LOWER_CHAN,
    FWDER_UNDEF_CHAN,
    FWDER_MAX_CHAN
} fwder_chan_t;

/** Forwarding bypass function handler registered during attach time.
 * This bypass handler will direct the packet to the appropriate interface.
 * Depending on the mode of operation, bypass handler will invoke:
 * - NIC Mode: wl hard_start_xmit handler, given a packet or a packet chain.
 * - DGL Mode: DHD flowring enqueue handler, given an array of pointers to pkts.
 * The number of virtual devices that have bound to the forwarder is passed. A
 * WLAN bypass handler is responsible to locate the actual network device by
 * looking up the MAC to virtual interface's device table.
 * The GMAC driver registers et_forward() which invokes the et_start().
 */
typedef int (*fwder_bypass_fn_t)(struct fwder * fwder,
                                 struct sk_buff * skbs, int skb_cnt,
                                 struct net_device * rx_dev);

typedef int (*fwder_flood_fn_t)(struct sk_buff *skb, struct net_device *net, bool in_lock);

/** HND Forwarder object. */
typedef struct fwder {
#if defined(CONFIG_SMP)
	spinlock_t        lock;         /* LOCK : spinlock */
#endif  /*  CONFIG_SMP */
	unsigned long     lflags;       /* LOCK : local irq flags */
	struct fwder      * mate;       /* Forwarder in the reverse direction */
	fwder_bypass_fn_t bypass_fn;    /* Transmit bypass handler */
	struct net_device * dev_def;    /* Device to use, when devs_cnt = 1 */
	int               devs_cnt;     /* Number of HW fwding capable interfaces */
	dll_t             devs_dll;     /* List of HW switching capable WL ifs */
	struct wofa       * wofa;       /* WOFA: Software ARL for IntraBSS fwding */
	fwder_mode_t      mode;         /* NIC or DGL operational mode */
	uint16            dataoff;      /* Offset of start of ethernet header */
	void              * osh;        /* OS Handler */
	uint32            transmit;     /* Count of fwder_transmit calls */
	uint32            dropped;      /* Count of packets dropped (!delivered) */
	uint32            flooded;      /* Count of packets flooded */
	int               unit;         /* Instance identifier */
	char              * name;
} fwder_t;


#if defined(CONFIG_SMP)
/** fwder_upstream and fwder_dnstream form a bi-directional pair per CPU[unit].
 * GMAC attaches against "upstream" direction and gets a pointer to "dnstream"
 * WLAN attaches against "dnstream" direction and gets a pointer to "upstream"
 *
 * The returned pointer to a fwder will carry the bypass handler and context for
 * and will be used to forward a packet via the peer.
 */
DECLARE_PER_CPU(struct fwder, fwder_upstream_g); /* Per Core GMAC Transmit */
DECLARE_PER_CPU(struct fwder, fwder_dnstream_g); /* Per Core Wl## Transmit */
#else  /* ! CONFIG_SMP */
/** fwder_upstream[unit], fwder_dnstream[unit] form a bi-directional socket. */
extern struct fwder fwder_upstream_g[FWDER_MAX_UNIT];
extern struct fwder fwder_dnstream_g[FWDER_MAX_UNIT];
#endif /* ! CONFIG_SMP */


/** HND forwarder construction and destruction. */
extern int fwder_init(void); /* Invoked on insmod of et module */
extern int fwder_exit(void); /* Invoked on rmmod of et module */

/** Assign a fwder_unit number: [0 .. FWDER_MAX_RADIO) */
extern int fwder_assign(fwder_mode_t mode, int radio_unit);

/* Assign IRQ affinity of a radio identified by its fwder_unit */
extern int fwder_affinity(fwder_dir_t dir, int fwder_unit, int irq);

/** Register a bypass handler and return the reverse dir forwarder object. */
extern fwder_t * fwder_attach(fwder_dir_t dir, int unit, fwder_mode_t mode,
            fwder_bypass_fn_t bypass_fn, struct net_device * dev, void *osh);

/** Dettach a interface from the forwarder by dettaching the bypass handler. */
extern fwder_t * fwder_dettach(fwder_t * fwder, fwder_dir_t dir, int unit);

/** Bind a default net_device to a fwder: fwder handle is an upstream fwder */
extern int fwder_register(fwder_t * fwder, struct net_device * dev);

/** Fetch the default interface in the downstream forwarder. */
extern struct net_device * fwder_default(fwder_t * fwder);

/** On open/close of an interface, bind/unbind the interface's device to the
 * forwarder, if it is HW switching capable (nvram "fwd_wlandevs").
 */
extern fwder_t * fwder_bind(fwder_t * fwder, int unit, int subunit,
                            struct net_device * dev, bool attach);

/** Add a station to Upstream Forwarder's WOFA on association/reassociation.
 * Add the wofa metadata against the symbol to be returned on a lookup/
 */
extern int fwder_reassoc(fwder_t * fwder, uint16 * symbol, uintptr_t data);

/** Delete a station from Upstream Forwarder's WOFA on deassociation.  */
extern int fwder_deassoc(fwder_t * fwder, uint16 * symbol, uintptr_t data);

/** Flush all entries that have a reference to wofa metadata. */
extern int fwder_flush(fwder_t * fwder, uintptr_t data);

/** Lookup for a station assocatied with a forwarder.
 * If not found, returns FWDER_WOFA_INVALID, else return added wofa metadata.
 */
extern uintptr_t fwder_lookup(fwder_t * fwder, uint16 * symbol, const int port);

/** Given a downstream forwarder, flood a packet to all bound devices except to
 * the device in the skb. If clone is TRUE, then the original skb is returned.
 */
extern int fwder_flood(fwder_t * fwder, struct sk_buff *skb, void * osh,
                       bool clone, fwder_flood_fn_t dev_start_xmit);

/** Fixup a GMAC forwarded packet, by deleting data offset, popping VLAN tag,
 * and stripping the CRC.
 */
extern void fwder_fixup(fwder_t * fwder, struct sk_buff * skb);

/** Request a GMAC forwarder to discard a packet. */
extern void fwder_discard(fwder_t * fwder, struct sk_buff * skb);

/** Debug dump of forwarder object state. */
struct bcmstrbuf;
extern void fwder_dump(struct bcmstrbuf *b);

/** Hnd Forwarder LOCKand UNLOCK primitives in SMP and UP modes. */

/** HND Fwder and wofa Lock primitive: SMP/UP modes */
static inline void
_FWDER_LOCK(fwder_t * fwder)
{
#if defined(CONFIG_SMP)
	spin_lock_bh(&(fwder)->lock);
	/* spin_lock_irqsave(&fwder->lock, fwder->lflags); */
#else  /* ! CONFIG_SMP */
	local_irq_save(fwder->lflags);
#endif /* ! CONFIG_SMP */
}

/** HND Fwder and wofa UnLock primitive: SMP/UP modes */
static inline void
_FWDER_UNLOCK(fwder_t * fwder)
{
#if defined(CONFIG_SMP)
	spin_unlock_bh(&(fwder)->lock);
	/* spin_unlock_irqrestore(&fwder->lock, fwder->lflags); */
#else  /* ! CONFIG_SMP */
	local_irq_restore(fwder->lflags);
#endif /* ! CONFIG_SMP */
}

static inline bool
fwder_is_mode(const struct fwder * fwder, const fwder_mode_t mode)
{
	return (fwder->mode == mode);
}

/** Deliver packet(s) to an egress bypass handler.
 * Caller's responsibility to free packet(s), in case of failure.
 * In NIC mode, a packet chain may be sent.
 * In DGL mode, an array of pointers to packets are sent.
 *
 * Packets are transferred to the bypass handler along with the fwder object.
 * In the case where multiple interfaces avail of the same GMAC, then the
 * bound bypass handler is responsible to determine the appropriate net_device,
 * by accessing the WOFA (Mac Address Resolution Logic).
 *
 * In the upstream direction, there may be only one net_device (namely fwd##),
 * if the packet is directed to one of the LAN ports belonging to default br0.
 * However, if the packet is to another WLAN interface, then the WOFA ARL will
 * be used to locate the appropriate WLAN interface.
 */
static inline int
fwder_transmit(struct fwder * fwder, struct sk_buff * skbs, int skb_cnt,
               struct net_device * rx_dev)
{
	int ret;

	ASSERT(fwder != (struct fwder*)NULL);

	_FWDER_LOCK(fwder);                                    /* ++LOCK */

	FWDER_PTRACE(("%s skbs<%p> skb_cnt<%d> rx_dev<%p,%s> fwder<%p,%d,%s> %pS\n",
	              __FUNCTION__, skbs, skb_cnt, rx_dev,
	              (rx_dev) ? rx_dev->name : "null",
	              fwder, fwder->unit, fwder->name, fwder->bypass_fn));

	/* Use fwder to deliver to peer's bypass handler, for transmission. */
	ret = fwder->bypass_fn(fwder, skbs, skb_cnt, rx_dev);

	FWDER_STATS_ADD(fwder->transmit, skb_cnt);

	_FWDER_UNLOCK(fwder);                                  /* --LOCK */

	return ret;
}


#else  /* ! BCM_GMAC3 */

/** Non GMAC3:  WOFA not supported - stubs */
#define wofa_init()             ({  (WOFA_NULL); })
#define wofa_fini(wofa)			WOFA_NOOP
#define wofa_add(wofa, symbol, data)                                           \
	({ BCM_REFERENCE(wofa); BCM_REFERENCE(symbol); BCM_REFERENCE(data);        \
	   (WOFA_FAILURE); })
#define wofa_del(wofa, symbol, data)                                           \
	({ BCM_REFERENCE(wofa); BCM_REFERENCE(symbol); BCM_REFERENCE(data);        \
	   (WOFA_FAILURE); })
#define wofa_clr(wofa, data)                                                   \
	({ BCM_REFERENCE(wofa); BCM_REFERENCE(data); (WOFA_FAILURE); })
#define wofa_lkup(wofa, symbol, port)                                          \
	({ BCM_REFERENCE(wofa); BCM_REFERENCE(symbol); BCM_REFERENCE(port);        \
	   (WOFA_DATA_INVALID); })


/** Non GMAC3:  FWDER not supported - stubs */
#define fwder_init()            FWDER_NOOP
#define fwder_exit()            FWDER_NOOP

#define fwder_attach(dir, unit, mode, bypass_fn, dev, osh)                     \
	({ BCM_REFERENCE(dir); BCM_REFERENCE(unit); BCM_REFERENCE(mode);           \
	   BCM_REFERENCE(bypass_fn); BCM_REFERENCE(dev); BCM_REFERENCE(osh);       \
	   (FWDER_NULL); })

#define fwder_dettach(fwder, dir, unit)                                        \
	({ BCM_REFERENCE(fwder); (FWDER_NULL); })

#define fwder_bind(dir, unit, subunit, dev, attach)                            \
	({ BCM_REFERENCE(dir); BCM_REFERENCE(unit); BCM_REFERENCE(subunit);        \
	   BCM_REFERENCE(attach); (FWDER_NULL); })

#define fwder_transmit(skbs, skb_cnt, rxdev, fwder)                            \
	({ BCM_REFERENCE(skbs); BCM_REFERENCE(skb_cnt); BCM_REFERENCE(rxdev);      \
	   BCM_REFERENCE(fwder); (FWDER_FAILURE); })

#define fwder_flood(fwder, pkt, osh, clone)                                    \
	({ BCM_REFERENCE(fwder); BCM_REFERENCE(pkt);                               \
	   BCM_REFERENCE(osh); BCM_REFERENCE(clone); })

#define fwder_fixup(fwder, pkt)                                                \
	({ BCM_REFERENCE(fwder); BCM_REFERENCE(pkt); })

#define fwder_discard(fwder, pkt)                                              \
	({ BCM_REFERENCE(fwder); BCM_REFERENCE(pkt); })

#endif /* ! BCM_GMAC3 */

#endif /* _hndfwd_h_ */
