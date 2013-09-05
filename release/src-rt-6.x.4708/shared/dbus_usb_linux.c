/*
 * Dongle BUS interface
 * USB Linux Implementation
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: dbus_usb_linux.c 412260 2013-07-12 12:52:04Z $
 */

#include <typedefs.h>
#include <osl.h>

/**
 * DBUS_LINUX_RXDPC is created for router platform performance tuning. A separate thread is created
 * to handle USB RX and avoid the call chain getting too long and enhance cache hit rate.
 *
 * DBUS_LINUX_RXDPC setting is in wlconfig file.
 */

/*
 * If DBUS_LINUX_RXDPC is off, spin_lock_bh() for CTFPOOL in
 * linux_osl.c has to be changed to spin_lock_irqsave() because
 * PKTGET/PKTFREE are no longer in bottom half.
 *
 * Right now we have another queue rpcq in wl_linux.c. Maybe we
 * can eliminate that one to reduce the overhead.
 *
 * Enabling 2nd EP and DBUS_LINUX_RXDPC causing traffic from
 * both EP's to be queued in the same rx queue. If we want
 * RXDPC to work with 2nd EP. The EP for RPC call return
 * should bypass the dpc and go directly up.
 */

/* #define DBUS_LINUX_RXDPC */

/* Dbus histogram for ntxq, nrxq, dpc parameter tuning */
/* #define DBUS_LINUX_HIST */

#include <usbrdl.h>
#include <bcmendian.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <dbus.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <linux/usb.h>
#include <usbrdl.h>
#include <linux/firmware.h>
#ifdef DBUS_LINUX_RXDPC
#include <linux/sched.h>
#endif
#ifdef USBOS_THREAD
/**
 * The usb-thread is designed to provide currency on multiprocessors and SMP linux kernels. On the
 * dual cores platform, the WLAN driver, without threads, executed only on CPU0. The driver consumed
 * almost of 100% on CPU0, while CPU1 remained idle. The behavior was observed on Broadcom's STB.
 *
 * The WLAN driver consumed most of CPU0 and not CPU1 because tasklets/queues, software irq, and
 * hardware irq are executing from CPU0, only. CPU0 became the system's bottle-neck. TPUT is lower
 * and system's responsiveness is slower.
 *
 * To improve system responsiveness and TPUT usb-thread was implemented. The system's threads could
 * be scheduled to run on any core. One core could be processing data in the usb-layer and the other
 * core could be processing data in the wl-layer.
 *
 * For further info see [WlThreadAndUsbThread] Twiki.
 */

#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/hardirq.h>
#include <linux/list.h>
#include <linux_osl.h>
#endif /* USBOS_THREAD */


#ifdef DBUS_LINUX_RXDPC
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25))
#define RESCHED()   _cond_resched()
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#define RESCHED()   cond_resched()
#else
#define RESCHED()   __cond_resched()
#endif /* LINUX_VERSION_CODE  */
#endif	/* DBUS_LINUX_RXDPC */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#define KERNEL26

#define USB_ALLOC_URB()      usb_alloc_urb(0, GFP_ATOMIC)
#define USB_SUBMIT_URB(urb)  usb_submit_urb(urb, GFP_ATOMIC)
#define USB_UNLINK_URB(urb)  (usb_kill_urb(urb))
#define USB_FREE_URB(urb)    (usb_free_urb(urb))
#define USB_REGISTER()       usb_register(&dbus_usbdev)
#define USB_DEREGISTER()     usb_deregister(&dbus_usbdev)
#if defined(CONFIG_USB_SUSPEND)
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && (LINUX_VERSION_CODE < \
	KERNEL_VERSION(2, 6, 33)))
#define USB_AUTOPM_SET_INTERFACE(intf)  usb_autopm_set_interface(intf)
#else
#define USB_ENABLE_AUTOSUSPEND(udev)    usb_enable_autosuspend(udev)
#endif  /* ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) .....  */
#endif  /* defined(CONFIG_USB_SUSPEND) */

#define USB_CONTROL_MSG(dev, pipe, request, requesttype, value, index, data, size, timeout) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), \
	(data), (size), (timeout))
#define USB_BUFFER_ALLOC(dev, size, mem, dma)  usb_buffer_alloc(dev, size, mem, dma)
#define USB_BUFFER_FREE(dev, size, data, dma)  usb_buffer_free(dev, size, data, dma)

#ifdef WL_URB_ZPKT
#define URB_QUEUE_BULK   URB_ZERO_PACKET
#else
#define URB_QUEUE_BULK   0
#endif /* WL_URB_ZPKT */

#define CALLBACK_ARGS      struct urb *urb, struct pt_regs *regs
#define CONFIGDESC(usb)    (&((usb)->actconfig)->desc)
#define IFPTR(usb, idx)    ((usb)->actconfig->interface[idx])
#define IFALTS(usb, idx)   (IFPTR((usb), (idx))->altsetting[0])
#define IFDESC(usb, idx)   IFALTS((usb), (idx)).desc
#define IFEPDESC(usb, idx, ep) (IFALTS((usb), (idx)).endpoint[ep]).desc

#ifdef DBUS_LINUX_RXDPC
#define DAEMONIZE(a)   daemonize(a); allow_signal(SIGKILL); allow_signal(SIGTERM);
#define SET_NICE(n)    set_user_nice(current, n)
#endif

#else /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)) */

#define USB_ALLOC_URB()     usb_alloc_urb(0)
#define USB_SUBMIT_URB(urb) usb_submit_urb(urb)
#define USB_UNLINK_URB(urb) usb_unlink_urb(urb)
#define USB_FREE_URB(urb)   (usb_free_urb(urb))
#define USB_REGISTER()      usb_register(&dbus_usbdev)
#define USB_DEREGISTER()    usb_deregister(&dbus_usbdev)

#define USB_CONTROL_MSG(dev, pipe, request, requesttype, value, index, data, size, timeout) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), \
	(data), (size), (timeout))
#define USB_BUFFER_ALLOC(dev, size, mem, dma)  kmalloc(size, mem)
#define USB_BUFFER_FREE(dev, size, data, dma)  kfree(data)

#ifdef WL_URB_ZPKT
#define URB_QUEUE_BULK   USB_QUEUE_BULK|URB_ZERO_PACKET
#else
#define URB_QUEUE_BULK   0
#endif /*  WL_URB_ZPKT */

#define CALLBACK_ARGS     struct urb *urb
#define CONFIGDESC(usb)   ((usb)->actconfig)
#define IFPTR(usb, idx)   (&(usb)->actconfig->interface[idx])
#define IFALTS(usb, idx)  ((usb)->actconfig->interface[idx].altsetting[0])
#define IFDESC(usb, idx)  IFALTS((usb), (idx))
#define IFEPDESC(usb, idx, ep)  (IFALTS((usb), (idx)).endpoint[ep])

#ifdef DBUS_LINUX_RXDPC
#define DAEMONIZE(a)    daemonize();
#define SET_NICE(n)     do {current->nice = (n);} while (0)
#endif /* DBUS_LINUX_RXDPC */

#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31))
#define USB_SPEED_SUPER		5
#endif  /* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)) */

#define CONTROL_IF   0
#define BULK_IF      0

#ifdef BCMUSBDEV_COMPOSITE
#define USB_COMPIF_MAX       4

#define USB_CLASS_WIRELESS   0xe0
#define USB_CLASS_MISC       0xef
#define USB_SUBCLASS_COMMON  0x02
#define USB_PROTO_IAD        0x01
#define USB_PROTO_VENDOR	 0xff

#define USB_QUIRK_NO_SET_INTF   0x04 /* device does not support set_interface */
#endif /* BCMUSBDEV_COMPOSITE */

#define USB_SYNC_WAIT_TIMEOUT  300  /* ms */

/* Private data kept in skb */
#define SKB_PRIV(skb, idx)  (&((void **)skb->cb)[idx])
#define SKB_PRIV_URB(skb)   (*(struct urb **)SKB_PRIV(skb, 0))

#define BCM_USB_HOST_TOTALLEN_ZLP      512
#define BCM_USB_HOST_TOTALLEN_ZLP_PAD  8

enum usbos_suspend_state {
	USBOS_SUSPEND_STATE_DEVICE_ACTIVE = 0, /* Device is busy, won't allow suspend */
	USBOS_SUSPEND_STATE_SUSPEND_PENDING,   /* Device is idle, can be suspended */
	                                       /* Wating PM to suspend */
	USBOS_SUSPEND_STATE_SUSPENDED          /* Device suspended */
};

typedef struct {
	uint32 notification;
	uint32 reserved;
} intr_t;

typedef struct {
	dbus_pub_t *pub;

	void *cbarg;
	dbus_intf_callbacks_t *cbs;

	/* Imported */
	struct usb_device *usb;	/* USB device pointer from OS */
	struct urb *intr_urb; /* URB for interrupt endpoint */
	struct list_head req_freeq;
	struct list_head req_rxpostedq;	/* Posted down to USB driver for RX */
	struct list_head req_txpostedq;	/* Posted down to USB driver for TX */
	spinlock_t free_lock; /* Lock for free list */
	spinlock_t rxposted_lock; /* Lock for rx posted list */
	spinlock_t txposted_lock; /* Lock for tx posted list */
	uint rx_pipe, tx_pipe, intr_pipe, rx_pipe2; /* Pipe numbers for USB I/O */
	uint rxbuf_len;

	struct list_head req_rxpendingq; /* RXDPC: Pending for dpc to send up */
	spinlock_t rxpending_lock;	/* RXDPC: Lock for rx pending list */
	long dpc_pid;
	struct semaphore dpc_sem;
	struct completion dpc_exited;
	int rxpending;
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	int	dpc_cnt, dpc_pktcnt, dpc_maxpktcnt;
#endif

	struct urb *ctl_urb;
	int ctl_in_pipe, ctl_out_pipe;
	struct usb_ctrlrequest ctl_write;
	struct usb_ctrlrequest ctl_read;

	spinlock_t rxlock;      /* Lock for rxq management */
	spinlock_t txlock;      /* Lock for txq management */

	int intr_size;          /* Size of interrupt message */
	int interval;           /* Interrupt polling interval */
	intr_t intr;            /* Data buffer for interrupt endpoint */

	int maxps;
	int txposted;
	int rxposted;
	bool rxctl_deferrespok;	/* Get a response for setup from dongle */

	wait_queue_head_t wait;
	bool waitdone;
	int sync_urb_status;

	struct urb *blk_urb; /* Used for downloading embedded image */

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	int *txposted_hist;
	int *rxposted_hist;
#endif
#ifdef USBOS_THREAD
	spinlock_t              ctrl_lock;
	spinlock_t              usbos_list_lock;
	struct list_head        usbos_list;
	struct list_head        usbos_free_list;
	atomic_t                usbos_list_cnt;
	wait_queue_head_t       usbos_queue_head;
	struct task_struct      *usbos_kt;
#endif /* USBOS_THREAD */

	struct dma_pool *qtd_pool; /* QTD pool for USB optimization only */
	int tx_ep, rx_ep, rx2_ep;  /* EPs for USB optimization */
	struct usb_device *usb_device; /* USB device for optimization */
#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX) /** Linux USB AP related */
	spinlock_t fastpath_lock;
#endif
} usbos_info_t;

typedef struct urb_req {
	void         *pkt;
	int          buf_len;
	struct urb   *urb;
	void         *arg;
	usbos_info_t *usbinfo;
	struct list_head urb_list;
} urb_req_t;

#ifdef USBOS_THREAD
typedef struct usbos_list_entry {
	struct list_head    list;   /* must be first */
	void               *urb_context;
	int                 urb_length;
	int                 urb_status;
} usbos_list_entry_t;

static void* dbus_usbos_thread_init(usbos_info_t *usbos_info);
static void dbus_usbos_thread_deinit(usbos_info_t *usbos_info);
static void dbus_usbos_dispatch_schedule(CALLBACK_ARGS);
static int  dbus_usbos_thread_func(void *data);
static void dbus_usbos_recv_complete_handle(urb_req_t *req, int len, int status);
#endif /* USBOS_THREAD */

/* Shared Function prototypes */
bool dbus_usbos_dl_cmd(usbos_info_t *usbinfo, uint8 cmd, void *buffer, int buflen);
int dbus_usbos_wait(usbos_info_t *usbinfo, uint16 ms);
bool dbus_usbos_dl_send_bulk(usbos_info_t *usbinfo, void *buffer, int len);
int dbus_write_membytes(usbos_info_t *usbinfo, bool set, uint32 address, uint8 *data, uint size);

/* Local function prototypes */
static void dbus_usbos_send_complete(CALLBACK_ARGS);
#ifdef DBUS_LINUX_RXDPC
static void dbus_usbos_recv_dpc(usbos_info_t *usbos_info);
static int dbus_usbos_dpc_thread(void *data);
#endif /* DBUS_LINUX_RXDPC */
static void dbus_usbos_recv_complete(CALLBACK_ARGS);
static int  dbus_usbos_errhandler(void *bus, int err);
static int  dbus_usbos_state_change(void *bus, int state);
static void dbusos_stop(usbos_info_t *usbos_info);

#ifdef KERNEL26
static int dbus_usbos_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void dbus_usbos_disconnect(struct usb_interface *intf);
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) \
	&& (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38))
static int dbus_usbos_resume(struct usb_interface *intf);
static int dbus_usbos_suspend(struct usb_interface *intf, pm_message_t message);
static void dbus_usbos_sleep(usbos_info_t *usbos_info);
#endif
#else
static void *dbus_usbos_probe(struct usb_device *usb, unsigned int ifnum,
	const struct usb_device_id *id);
static void dbus_usbos_disconnect(struct usb_device *usb, void *ptr);
#endif /* KERNEL26 */

#ifdef USB_TRIGGER_DEBUG
static bool dbus_usbos_ctl_send_debugtrig(usbos_info_t *usbinfo);
#endif /* USB_TRIGGER_DEBUG */
static struct usb_device_id devid_table[] = {
	{ USB_DEVICE(BCM_DNGL_VID, 0x0000) }, /* Configurable via register() */
#if !defined(WLC_HIGH) || defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW)
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_4328) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_4322) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_4319) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_43236) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_43143) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_43242) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_4360) },
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BL_PID_4350) },
#endif
#ifdef EXTENDED_VID_PID
	EXTENDED_VID_PID,
#endif /* EXTENDED_VID_PID */
	{ USB_DEVICE(BCM_DNGL_VID, BCM_DNGL_BDC_PID) }, /* Default BDC */
	{ }
};

MODULE_DEVICE_TABLE(usb, devid_table);

/** functions called by the Linux kernel USB subsystem */
static struct usb_driver dbus_usbdev = {
	name:         "dbus_usbdev",
	probe:        dbus_usbos_probe,
	disconnect:   dbus_usbos_disconnect,
	id_table:     devid_table,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) \
	&& (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38))
	suspend:      dbus_usbos_suspend,
	resume:       dbus_usbos_resume,
	supports_autosuspend: 1
#endif
};

/** This stores USB info during Linux probe callback
 * since attach() is not called yet at this point
 */
typedef struct {
	void    *usbos_info;
	struct usb_device *usb; /* USB device pointer from OS */
	uint    rx_pipe;   /* Pipe numbers for USB I/O */
	uint    tx_pipe;   /* Pipe numbers for USB I/O */
	uint    intr_pipe; /* Pipe numbers for USB I/O */
	uint    rx_pipe2;  /* Pipe numbers for USB I/O */
	int     intr_size; /* Size of interrupt message */
	int     interval;  /* Interrupt polling interval */
	bool    dldone;
	int     vid;
	int     pid;
	bool    dereged;
	bool    disc_cb_done;
	DEVICE_SPEED    device_speed;
	enum usbos_suspend_state suspend_state;
	struct usb_interface     *intf;
} probe_info_t;

/*
 * USB Linux dbus_intf_t
 */
static void *dbus_usbos_intf_attach(dbus_pub_t *pub, void *cbarg, dbus_intf_callbacks_t *cbs);
static void dbus_usbos_intf_detach(dbus_pub_t *pub, void *info);
static int  dbus_usbos_intf_send_irb(void *bus, dbus_irb_tx_t *txirb);
static int  dbus_usbos_intf_recv_irb(void *bus, dbus_irb_rx_t *rxirb);
static int  dbus_usbos_intf_recv_irb_from_ep(void *bus, dbus_irb_rx_t *rxirb, uint32 ep_idx);
static int  dbus_usbos_intf_cancel_irb(void *bus, dbus_irb_tx_t *txirb);
static int  dbus_usbos_intf_send_ctl(void *bus, uint8 *buf, int len);
static int  dbus_usbos_intf_recv_ctl(void *bus, uint8 *buf, int len);
static int  dbus_usbos_intf_get_attrib(void *bus, dbus_attrib_t *attrib);
static int  dbus_usbos_intf_up(void *bus);
static int  dbus_usbos_intf_down(void *bus);
static int  dbus_usbos_intf_stop(void *bus);
static int  dbus_usbos_readreg(void *bus, uint32 regaddr, int datalen, uint32 *value);
int dbus_usbos_writereg(void *bus, uint32 regaddr, int datalen, uint32 data);
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
static void dbus_usbos_intf_dump(void *bus, struct bcmstrbuf *b);
#endif /* BCMDBG || DBUS_LINUX_HIST */
static int  dbus_usbos_intf_set_config(void *bus, dbus_config_t *config);
static bool dbus_usbos_intf_recv_needed(void *bus);
static void *dbus_usbos_intf_exec_rxlock(void *bus, exec_cb_t cb, struct exec_parms *args);
static void *dbus_usbos_intf_exec_txlock(void *bus, exec_cb_t cb, struct exec_parms *args);
static int dbus_usbos_intf_pnp(void *bus, int evnt);
#ifdef BCMUSBDEV_COMPOSITE
static int dbus_usbos_intf_wlan(struct usb_device *usb);
#endif /* BCMUSBDEV_COMPOSITE */

/** functions called by dbus_usb.c */
static dbus_intf_t dbus_usbos_intf = {
	dbus_usbos_intf_attach,
	dbus_usbos_intf_detach,
	dbus_usbos_intf_up,
	dbus_usbos_intf_down,
	dbus_usbos_intf_send_irb,
	dbus_usbos_intf_recv_irb,
	dbus_usbos_intf_cancel_irb,
	dbus_usbos_intf_send_ctl,
	dbus_usbos_intf_recv_ctl,
	NULL, /* get_stats */
	dbus_usbos_intf_get_attrib,
	dbus_usbos_intf_pnp, /* pnp */
	NULL, /* remove */
	NULL, /* resume */
	NULL, /* suspend */
	dbus_usbos_intf_stop,
	NULL, /* reset */
	NULL, /* pktget */
	NULL, /* pktfree */
	NULL, /* iovar_op */
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	dbus_usbos_intf_dump, /* dump */
#else
	NULL, /* dump */
#endif /* BCMDBG || DBUS_LINUX_HIST */
	dbus_usbos_intf_set_config, /* set_config */
	NULL, /* get_config */
	NULL, /* device_exists */
	NULL, /* dlneeded */
	NULL, /* dlstart */
	NULL, /* dlrun */
	dbus_usbos_intf_recv_needed,
	dbus_usbos_intf_exec_rxlock,
	dbus_usbos_intf_exec_txlock,

	NULL, /* tx_timer_init */
	NULL, /* tx_timer_start */
	NULL, /* tx_timer_stop */

	NULL, /* sched_dpc */
	NULL, /* lock */
	NULL, /* unlock */
	NULL, /* sched_probe_cb */

	NULL, /* shutdown */

	NULL, /* recv_stop */
	NULL, /* recv_resume */

	dbus_usbos_intf_recv_irb_from_ep,
	dbus_usbos_readreg
};

static probe_info_t    g_probe_info;
static probe_cb_t      probe_cb = NULL;
static disconnect_cb_t disconnect_cb = NULL;
static void            *probe_arg = NULL;
static void            *disc_arg = NULL;

#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
#define EHCI_PAGE_SIZE    4096

/* Copies of structures located elsewhere. */

typedef struct {
	dbus_pub_t *pub;

	void *cbarg;
	dbus_intf_callbacks_t *cbs;
	dbus_intf_t *drvintf;
	void *usbosl_info;
} usb_info_t;

/* This private structure dbus_info_t is also declared in dbus.c.
 * All the fields must be consistent in both declarations.
 */
typedef struct dbus_info {
	dbus_pub_t pub; /* MUST BE FIRST */

	void *cbarg;
	dbus_callbacks_t *cbs;
	void *bus_info;
	dbus_intf_t *drvintf;
	uint8 *fw;
	int fwlen;
	uint32 errmask;
	int rx_low_watermark;
	int tx_low_watermark;
	bool txoff;
	bool txoverride;
	bool rxoff;
	bool tx_timer_ticking;

	dbus_irbq_t *rx_q;
	dbus_irbq_t *tx_q;

#ifdef BCMDBG
	int *txpend_q_hist;
	int *rxpend_q_hist;
#endif /* BCMDBG */
#ifdef EHCI_FASTPATH_RX
	atomic_t rx_outstanding;
#endif
	uint8 *nvram;
	int	nvram_len;
	uint8 *image;	/* buffer for combine fw and nvram */
	int image_len;
	uint8 *orig_fw;
	int origfw_len;
	int decomp_memsize;
	dbus_extdl_t extdl;
	int nvram_nontxt;
} dbus_info_t;

static atomic_t s_tx_pending;

static int optimize_qtd_fill_with_data(const dbus_pub_t *pub, int epn, struct ehci_qtd *qtd,
	void *data,	int token, int len);
static void inline optimize_ehci_qtd_init(struct ehci_qtd *qtd, dma_addr_t dma);
static int optimize_init(usbos_info_t *usbos_info, struct usb_device *usb, int out, int in,
	int in2);
static int optimize_deinit(usbos_info_t *usbos_info, struct usb_device *usb);
#endif  /* #if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX) */


/**
 * multiple code paths in this file dequeue a URB request, this function makes sure that it happens
 * in a concurrency save manner. Don't call this from a sleepable process context.
 */
static urb_req_t * BCMFASTPATH
dbus_usbos_qdeq(struct list_head *urbreq_q, spinlock_t *lock)
{
	unsigned long flags;
	urb_req_t *req;

	ASSERT(urbreq_q != NULL);

	spin_lock_irqsave(lock, flags);

	if (list_empty(urbreq_q)) {
		req = NULL;
	} else {
		ASSERT(urbreq_q->next != NULL);
		ASSERT(urbreq_q->next != urbreq_q);

		req = list_entry(urbreq_q->next, urb_req_t, urb_list);
		list_del_init(&req->urb_list);
	}

	spin_unlock_irqrestore(lock, flags);

	return req;
}

static void BCMFASTPATH
dbus_usbos_qenq(struct list_head *urbreq_q, urb_req_t *req, spinlock_t *lock)
{
	unsigned long flags;

	spin_lock_irqsave(lock, flags);

	list_add_tail(&req->urb_list, urbreq_q);

	spin_unlock_irqrestore(lock, flags);

}

/**
 * multiple code paths in this file remove a URB request from a list, this function makes sure that
 * it happens in a concurrency save manner. Don't call this from a sleepable process context.
 * Is quite similar to dbus_usbos_qdeq(), I wonder why this function is needed.
 */
static void
dbus_usbos_req_del(urb_req_t *req, spinlock_t *lock)
{
	unsigned long flags;

	spin_lock_irqsave(lock, flags);

	list_del_init(&req->urb_list);

	spin_unlock_irqrestore(lock, flags);
}


/**
 * Driver requires a pool of URBs to operate. This function is called during initialization
 * (attach phase), allocates a number of URBs, and puts them on the free ('req_freeq') queue
 */
static int
dbus_usbos_urbreqs_alloc(usbos_info_t *usbos_info)
{
	int i;

	DBUSTRACE(("%s: allocating URBs. ntxq %d nrxq %d\n", __FUNCTION__,
		usbos_info->pub->ntxq, usbos_info->pub->nrxq));
	for (i = 0; i < (usbos_info->pub->nrxq + usbos_info->pub->ntxq); i++) {
		urb_req_t *req;

		req = MALLOC(usbos_info->pub->osh, sizeof(urb_req_t));
		if (req == NULL) {
			/* dbus_usbos_urbreqs_free() takes care of partial
			 * allocation
			 */
			DBUSERR(("%s: usb_alloc_urb failed\n", __FUNCTION__));
			return DBUS_ERR_NOMEM;
		}
		bzero(req, sizeof(urb_req_t));

		req->urb = USB_ALLOC_URB();
		if (req->urb == NULL) {
			DBUSERR(("%s: usb_alloc_urb failed\n", __FUNCTION__));
			return DBUS_ERR_NOMEM;
		}

		INIT_LIST_HEAD(&req->urb_list);

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		/* don't allocate now. Do it on demand */
		req->pkt = NULL;
#else
		/* pre-allocate  buffers never to be released */
		req->pkt = MALLOC(usbos_info->pub->osh, usbos_info->rxbuf_len);
		if (req->pkt == NULL) {
			DBUSERR(("%s: usb_alloc_urb failed\n", __FUNCTION__));
			return DBUS_ERR_NOMEM;
		}
#endif
		req->buf_len = usbos_info->rxbuf_len;
		dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
	}

	return DBUS_OK;
}

/** Typically called during detach or when attach failed. Don't call until all URBs unlinked */
static void
dbus_usbos_urbreqs_free(usbos_info_t *usbos_info)
{
	urb_req_t *req;

	while ((req = dbus_usbos_qdeq(&usbos_info->req_freeq,
		&usbos_info->free_lock)) != NULL) {

		if (req->pkt) {
			/* We do MFREE instead of PKTFREE because the pkt has been
			 * converted to native already
			 */
			MFREE(usbos_info->pub->osh, req->pkt, req->buf_len);
			req->pkt = NULL;
			req->buf_len = 0;
		}

		if (req->urb) {
			USB_FREE_URB(req->urb);
			req->urb = NULL;
		}
		MFREE(usbos_info->pub->osh, req, sizeof(urb_req_t));
	}
}

/**
 * called by Linux kernel on URB completion. Upper DBUS layer (dbus_usb.c) has to be notified of
 * send completion.
 */
void
dbus_usbos_send_complete(CALLBACK_ARGS)
{
	urb_req_t *req = urb->context;
	dbus_irb_tx_t *txirb = req->arg;
	usbos_info_t *usbos_info = req->usbinfo;
	unsigned long flags;
	int status = DBUS_OK;

	spin_lock_irqsave(&usbos_info->txlock, flags);
	dbus_usbos_req_del(req, &usbos_info->txposted_lock);
	usbos_info->txposted--;
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	if (usbos_info->txposted_hist) {
		usbos_info->txposted_hist[usbos_info->txposted]++;
	}
#endif /* BCMDBG || DBUS_LINUX_HIST */
	if (unlikely (usbos_info->txposted < 0)) {
		DBUSERR(("%s ERROR: txposted is negative!!\n", __FUNCTION__));
	}
	spin_unlock_irqrestore(&usbos_info->txlock, flags);

	if (unlikely (urb->status)) {
		status = DBUS_ERR_TXFAIL;
		DBUSTRACE(("txfail status %d\n", urb->status));
	}

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
	/* detach the packet from the req */
	req->pkt = NULL;
#endif
	if (txirb->send_buf) kfree(txirb->send_buf);
	if (likely (usbos_info->cbarg && usbos_info->cbs)) {
		if (likely (usbos_info->cbs->send_irb_complete != NULL))
			usbos_info->cbs->send_irb_complete(usbos_info->cbarg, txirb, status);
	}

	dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
}

/**
 * In order to receive USB traffic from the dongle, we need to supply the Linux kernel with a free
 * URB that is going to contain received data.
 */
static int BCMFASTPATH
dbus_usbos_recv_urb_submit(usbos_info_t *usbos_info, dbus_irb_rx_t *rxirb, uint32 ep_idx)
{
	urb_req_t *req;
	int ret = DBUS_OK;
	unsigned long flags;
	void *p;
	uint rx_pipe;

	if (!(req = dbus_usbos_qdeq(&usbos_info->req_freeq, &usbos_info->free_lock))) {
		DBUSERR(("%s No free URB!\n", __FUNCTION__));
		return DBUS_ERR_RXDROP;
	}

	spin_lock_irqsave(&usbos_info->rxlock, flags);

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
	req->pkt = rxirb->pkt = PKTGET(usbos_info->pub->osh, req->buf_len, FALSE);
	if (!rxirb->pkt) {
		DBUSERR(("%s: PKTGET failed\n", __FUNCTION__));
		dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
		ret = DBUS_ERR_RXDROP;
		goto fail;
	}
	/* consider the packet "native" so we don't count it as MALLOCED in the osl */
	PKTTONATIVE(usbos_info->pub->osh, req->pkt);
	rxirb->buf = NULL;
	p = PKTDATA(usbos_info->pub->osh, req->pkt);
#else
	rxirb->buf = req->pkt;
	p = rxirb->buf;
#endif /* defined(BCM_RPC_NOCOPY) */
	rxirb->buf_len = req->buf_len;
	req->usbinfo = usbos_info;
	req->arg = rxirb;
	if (ep_idx == 0) {
		rx_pipe = usbos_info->rx_pipe;
	} else {
		rx_pipe = usbos_info->rx_pipe2;
		ASSERT(usbos_info->rx_pipe2);
	}
	/* Prepare the URB */
	usb_fill_bulk_urb(req->urb, usbos_info->usb, rx_pipe,
		p,
		rxirb->buf_len,
		(usb_complete_t)dbus_usbos_recv_complete, req);
		req->urb->transfer_flags |= URB_QUEUE_BULK;

	if ((ret = USB_SUBMIT_URB(req->urb))) {
		DBUSERR(("%s USB_SUBMIT_URB failed. status %d\n", __FUNCTION__, ret));
		dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
		ret = DBUS_ERR_RXFAIL;
		goto fail;
	}
	usbos_info->rxposted++;
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	if (usbos_info->rxposted_hist) {
		usbos_info->rxposted_hist[usbos_info->rxposted]++;
	}
#endif /* BCMDBG || DBUS_LINUX_HIST */

	dbus_usbos_qenq(&usbos_info->req_rxpostedq, req, &usbos_info->rxposted_lock);
fail:
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);
	return ret;
}

#ifdef DBUS_LINUX_RXDPC
static void BCMFASTPATH
dbus_usbos_recv_dpc(usbos_info_t *usbos_info)
{
	urb_req_t *req = NULL;
	dbus_irb_rx_t *rxirb = NULL;
	int dbus_status = DBUS_OK;
	bool killed = (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) ? 1 : 0;

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	int cnt = 0;

	usbos_info->dpc_cnt++;
#endif /* BCMDBG || DBUS_LINUX_HIST */

	while ((req = dbus_usbos_qdeq(&usbos_info->req_rxpendingq,
		&usbos_info->rxpending_lock)) != NULL) {
		struct urb *urb = req->urb;
		rxirb = req->arg;

		/* Handle errors */
		if (urb->status) {
			/*
			 * Linux 2.4 disconnect: -ENOENT or -EILSEQ for CRC error; rmmod: -ENOENT
			 * Linux 2.6 disconnect: -EPROTO, rmmod: -ESHUTDOWN
			 */
			if ((urb->status == -ENOENT && (!killed)) || urb->status == -ESHUTDOWN) {
				/* NOTE: unlink() can not be called from URB callback().
				 * Do not call dbusos_stop() here.
				 */
				dbus_usbos_state_change(usbos_info, DBUS_STATE_DOWN);
			} else if (urb->status == -EPROTO) {
			} else {
				DBUSERR(("%s rx error %d\n", __FUNCTION__, urb->status));
				dbus_usbos_errhandler(usbos_info, DBUS_ERR_RXFAIL);
			}

			/* On error, don't submit more URBs yet */
			DBUSERR(("%s %d rx error %d\n", __FUNCTION__, __LINE__, urb->status));
			rxirb->buf = NULL;
			rxirb->actual_len = 0;
			dbus_status = DBUS_ERR_RXFAIL;
			goto fail;
		}

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		/* detach the packet from the req */
		req->pkt = NULL;
#endif
		/* Make the skb represent the received urb */
		rxirb->actual_len = urb->actual_length;

fail:
		usbos_info->rxpending--;
#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
		cnt++;
#endif /* BCMDBG || DBUS_LINUX_HIST */
		if (usbos_info->cbarg && usbos_info->cbs &&
			usbos_info->cbs->recv_irb_complete) {
			usbos_info->cbs->recv_irb_complete(usbos_info->cbarg, rxirb, dbus_status);
		}
		dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
	}

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	usbos_info->dpc_pktcnt += cnt;
	usbos_info->dpc_maxpktcnt = MAX(cnt, usbos_info->dpc_maxpktcnt);
#endif /* BCMDBG || DBUS_LINUX_HIST */
#ifdef DBUS_LINUX_HIST
	{
		static unsigned long last_dump = 0;

		/* dump every 20 sec */
		if (jiffies > (last_dump + 20*HZ)) {
			dbus_usbos_intf_dump(usbos_info, NULL);
			last_dump = jiffies;
		}
	}
#endif /* DBUS_LINUX_HIST */
}

static int BCMFASTPATH
dbus_usbos_dpc_thread(void *data)
{
	usbos_info_t *usbos_info = (usbos_info_t*)data;

	DAEMONIZE("dbus_rx_dpc");
	/* High priority for short response time. We will yield by ourselves. */
	/* SET_NICE(-10); */

	/* Run until signal received */
	while (1) {
		if (down_interruptible(&usbos_info->dpc_sem) == 0) {
			dbus_usbos_recv_dpc(usbos_info);
			RESCHED();
		} else
			break;
	}

	complete_and_exit(&usbos_info->dpc_exited, 0);
	return 0;
}
#endif /* DBUS_LINUX_RXDPC */

#ifdef USBOS_THREAD
static void
dbus_usbos_recv_complete(CALLBACK_ARGS)
{
	dbus_usbos_dispatch_schedule(urb, regs);
}

/** called by worked thread when a 'receive URB' completed */
static void BCMFASTPATH
dbus_usbos_recv_complete_handle(urb_req_t *req, int len, int status)
{
#ifdef DBUS_LINUX_RXDPC
	usbos_info_t *usbos_info = req->usbinfo;
	unsigned long flags;

	spin_lock_irqsave(&usbos_info->rxlock, flags);
	/* detach the packet from the queue */
	dbus_usbos_req_del(req, &usbos_info->rxposted_lock);
	usbos_info->rxposted--;

	/* Enqueue to rxpending queue */
	usbos_info->rxpending++;
	dbus_usbos_qenq(&usbos_info->req_rxpendingq, req, &usbos_info->rxpending_lock);
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);

	/* Wake up dpc for further processing */
	ASSERT(usbos_info->dpc_pid >= 0);
	up(&usbos_info->dpc_sem);
#else
	dbus_irb_rx_t *rxirb = req->arg;
	usbos_info_t *usbos_info = req->usbinfo;
	unsigned long flags;
	int dbus_status = DBUS_OK;
	bool killed = (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) ? 1 : 0;

	spin_lock_irqsave(&usbos_info->rxlock, flags);
	dbus_usbos_req_del(req, &usbos_info->rxposted_lock);
	usbos_info->rxposted--;
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);

	/* Handle errors */
	if (status) {
		/*
		 * Linux 2.4 disconnect: -ENOENT or -EILSEQ for CRC error; rmmod: -ENOENT
		 * Linux 2.6 disconnect: -EPROTO, rmmod: -ESHUTDOWN
		 */
		if ((status == -ENOENT && (!killed))|| status == -ESHUTDOWN) {
			/* NOTE: unlink() can not be called from URB callback().
			 * Do not call dbusos_stop() here.
			 */
			dbus_usbos_state_change(usbos_info, DBUS_STATE_DOWN);
		} else if (status == -EPROTO) {
		} else {
			DBUSTRACE(("%s rx error %d\n", __FUNCTION__, status));
			dbus_usbos_errhandler(usbos_info, DBUS_ERR_RXFAIL);
		}

		/* On error, don't submit more URBs yet */
		rxirb->buf = NULL;
		rxirb->actual_len = 0;
		dbus_status = DBUS_ERR_RXFAIL;
		goto fail;
	}

	/* Make the skb represent the received urb */
	rxirb->actual_len = len;
fail:
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
	/* detach the packet from the queue */
	req->pkt = NULL;
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */

	if (usbos_info->cbarg && usbos_info->cbs) {
		if (usbos_info->cbs->recv_irb_complete) {
			usbos_info->cbs->recv_irb_complete(usbos_info->cbarg, rxirb, dbus_status);
		}
	}

	dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
#endif /* DBUS_LINUX_RXDPC */
}


#else /*  !USBOS_THREAD */

/** called by Linux kernel when it returns a URB to this driver */
static void BCMFASTPATH
dbus_usbos_recv_complete(CALLBACK_ARGS)
{
#ifdef DBUS_LINUX_RXDPC
	urb_req_t *req = urb->context;
	usbos_info_t *usbos_info = req->usbinfo;
	unsigned long flags;

	spin_lock_irqsave(&usbos_info->rxlock, flags);
	/* detach the packet from the queue */
	dbus_usbos_req_del(req, &usbos_info->rxposted_lock);
	usbos_info->rxposted--;

	/* Enqueue to rxpending queue */
	usbos_info->rxpending++;
	dbus_usbos_qenq(&usbos_info->req_rxpendingq, req, &usbos_info->rxpending_lock);
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);

	/* Wake up dpc for further processing */
	ASSERT(usbos_info->dpc_pid >= 0);
	up(&usbos_info->dpc_sem);
#else
	urb_req_t *req = urb->context;
	dbus_irb_rx_t *rxirb = req->arg;
	usbos_info_t *usbos_info = req->usbinfo;
	unsigned long flags;
	int dbus_status = DBUS_OK;
	bool killed = (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) ? 1 : 0;

	spin_lock_irqsave(&usbos_info->rxlock, flags);
	dbus_usbos_req_del(req, &usbos_info->rxposted_lock);
	usbos_info->rxposted--;
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);

	/* Handle errors */
	if (urb->status) {
		/*
		 * Linux 2.4 disconnect: -ENOENT or -EILSEQ for CRC error; rmmod: -ENOENT
		 * Linux 2.6 disconnect: -EPROTO, rmmod: -ESHUTDOWN
		 */
		if ((urb->status == -ENOENT && (!killed))|| urb->status == -ESHUTDOWN) {
			/* NOTE: unlink() can not be called from URB callback().
			 * Do not call dbusos_stop() here.
			 */
			dbus_usbos_state_change(usbos_info, DBUS_STATE_DOWN);
		} else if (urb->status == -EPROTO) {
		} else {
			DBUSTRACE(("%s rx error %d\n", __FUNCTION__, urb->status));
			dbus_usbos_errhandler(usbos_info, DBUS_ERR_RXFAIL);
		}

		/* On error, don't submit more URBs yet */
		rxirb->buf = NULL;
		rxirb->actual_len = 0;
		dbus_status = DBUS_ERR_RXFAIL;
		goto fail;
	}

	/* Make the skb represent the received urb */
	rxirb->actual_len = urb->actual_length;
fail:
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
	/* detach the packet from the queue */
	req->pkt = NULL;
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */

	if (usbos_info->cbarg && usbos_info->cbs) {
		if (usbos_info->cbs->recv_irb_complete) {
			usbos_info->cbs->recv_irb_complete(usbos_info->cbarg, rxirb, dbus_status);
		}
	}
	dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
#endif /* DBUS_LINUX_RXDPC */
}
#endif /*  USBOS_THREAD */

#if (defined(WLC_HIGH) && defined(WLC_LOW))
/** called by Linux kernel on URB completion */
static void
dbus_usbos_intr_complete(CALLBACK_ARGS)
{
	usbos_info_t *usbos_info = (usbos_info_t *)urb->context;
#ifdef KERNEL26
	int ret;
#endif
	bool killed = (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) ? 1 : 0;

	if (usbos_info == NULL || usbos_info->pub == NULL)
		return;
	if ((urb->status == -ENOENT && (!killed)) || urb->status == -ESHUTDOWN ||
		urb->status == -ENODEV) {
		dbus_usbos_state_change(usbos_info, DBUS_STATE_DOWN);
	}

	if (usbos_info->pub->busstate == DBUS_STATE_DOWN) {
		DBUSERR(("%s: intr cb when DBUS down, ignoring\n", __FUNCTION__));
		return;
	}

#ifdef KERNEL26
	/* Resubmit every time (2.6 only) */
	if (((usbos_info->pub->busstate == DBUS_STATE_UP) &&
	     (ret = USB_SUBMIT_URB(usbos_info->intr_urb))))
		DBUSERR(("%s intr USB_SUBMIT_URB failed with status %d\n", __FUNCTION__, ret));
#endif
}
#endif	/* (defined(WLC_HIGH) && defined(WLC_LOW)) */

/**
 * If Linux notifies our driver that a control read or write URB has completed, we should notify
 * the DBUS layer above us (dbus_usb.c in this case).
 */
static void
dbus_usbos_ctl_complete(usbos_info_t *usbos_info, int type, int urbstatus)
{
	int status = DBUS_ERR;

	if (usbos_info == NULL)
		return;

	switch (urbstatus) {
		case 0:
			status = DBUS_OK;
		break;
		case -EINPROGRESS:
		case -ENOENT:
		default:
			DBUSERR(("%s: failed with status %d\n", __FUNCTION__, urbstatus));
			status = DBUS_ERR;
		break;
	}

	if (usbos_info->cbarg && usbos_info->cbs) {
		if (usbos_info->cbs->ctl_complete)
			usbos_info->cbs->ctl_complete(usbos_info->cbarg, type, status);
	}
}

/** called by Linux */
static void
dbus_usbos_ctlread_complete(CALLBACK_ARGS)
{
	usbos_info_t *usbos_info = (usbos_info_t *)urb->context;
#ifdef USBOS_THREAD
	unsigned long flags;

	spin_lock_irqsave(&usbos_info->ctrl_lock, flags);
#endif /* USBOS_THREAD */

	ASSERT(urb);
	usbos_info = (usbos_info_t *)urb->context;

	dbus_usbos_ctl_complete(usbos_info, DBUS_CBCTL_READ, urb->status);

#ifdef USBOS_THREAD
	if (usbos_info->rxctl_deferrespok) {
		usbos_info->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_CLASS |
		USB_RECIP_INTERFACE;
		usbos_info->ctl_read.bRequest = 1;
	}
	spin_unlock_irqrestore(&usbos_info->ctrl_lock, flags);
#endif
}

/** called by Linux */
static void
dbus_usbos_ctlwrite_complete(CALLBACK_ARGS)
{
	usbos_info_t *usbos_info = (usbos_info_t *)urb->context;
#ifdef USBOS_THREAD
	unsigned long flags;

	spin_lock_irqsave(&usbos_info->ctrl_lock, flags);
#endif /* USBOS_THREAD */

	ASSERT(urb);
	usbos_info = (usbos_info_t *)urb->context;

	dbus_usbos_ctl_complete(usbos_info, DBUS_CBCTL_WRITE, urb->status);

#ifdef USBOS_THREAD
	spin_unlock_irqrestore(&usbos_info->ctrl_lock, flags);
#endif
}

/**
 * when the bus is going to sleep or halt, the Linux kernel requires us to take ownership of our
 * URBs again. Multiple code paths in this file require a list of URBs to be cancelled in a
 * concurrency save manner.
 */
static void
dbus_usbos_unlink(struct list_head *urbreq_q, spinlock_t *lock)
{
	urb_req_t *req;

	/* dbus_usbos_recv_complete() adds req back to req_freeq */
	while ((req = dbus_usbos_qdeq(urbreq_q, lock)) != NULL) {
		ASSERT(req->urb != NULL);
		USB_UNLINK_URB(req->urb);
	}
}

/** multiple code paths in this file require the bus to stop */
static void
dbusos_stop(usbos_info_t *usbos_info)
{
	urb_req_t *req;
	req = NULL;
	BCM_REFERENCE(req);

	ASSERT(usbos_info);

#ifdef USB_TRIGGER_DEBUG
	dbus_usbos_ctl_send_debugtrig(usbos_info);
#endif /* USB_TRIGGER_DEBUG */
	dbus_usbos_state_change(usbos_info, DBUS_STATE_DOWN);
	DBUSTRACE(("%s: unlink all URBs\n", __FUNCTION__));
	if (usbos_info->intr_urb)
		USB_UNLINK_URB(usbos_info->intr_urb);

	if (usbos_info->ctl_urb)
		USB_UNLINK_URB(usbos_info->ctl_urb);

	if (usbos_info->blk_urb)
		USB_UNLINK_URB(usbos_info->blk_urb);

	dbus_usbos_unlink(&usbos_info->req_txpostedq, &usbos_info->txposted_lock);
	if (usbos_info->txposted > 0) {
		DBUSERR(("%s ERROR: tx REQs posted=%d in stop!\n", __FUNCTION__,
			usbos_info->txposted));
	}
	dbus_usbos_unlink(&usbos_info->req_rxpostedq, &usbos_info->rxposted_lock);

#ifdef USBOS_THREAD
	/* yield the CPU to rx packet thread */
	while (1) {
		if (atomic_read(&usbos_info->usbos_list_cnt) <= 0)	break;
		wake_up_interruptible(&usbos_info->usbos_queue_head);
		msleep(3);
	}
#endif /* USBOS_THREAD */

	if (usbos_info->rxposted > 0) {
		DBUSERR(("%s ERROR: rx REQs posted=%d in stop!\n", __FUNCTION__,
			usbos_info->rxposted));
	}

	ASSERT(usbos_info->txposted == 0 && usbos_info->rxposted == 0);

#ifdef DBUS_LINUX_RXDPC
	/* Stop the dpc thread */
	if (usbos_info->dpc_pid >= 0) {
		KILL_PROC(usbos_info->dpc_pid, SIGTERM);
		wait_for_completion(&usbos_info->dpc_exited);
	}

	/* Move pending reqs to free queue so they can be freed */
	while ((req = dbus_usbos_qdeq(&usbos_info->req_rxpendingq,
		&usbos_info->rxpending_lock)) != NULL) {
		dbus_usbos_qenq(&usbos_info->req_freeq, req,
			&usbos_info->free_lock);
	}
#endif /* DBUS_LINUX_RXDPC */
}

#ifdef KERNEL26
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) \
	&& (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38))
/**
 * Linux kernel sports a 'USB auto suspend' feature. See: http://lwn.net/Articles/373550/
 * The suspend method is called by the Linux kernel to warn the driver that the device is going to
 * be suspended.  If the driver returns a negative error code, the suspend will be aborted. If the
 * driver returns 0, it must cancel all outstanding URBs (usb_kill_urb()) and not submit any more.
 */
static int
dbus_usbos_suspend(struct usb_interface *intf,
            pm_message_t message)
{
	DBUSERR(("%s suspend state: %d\n", __FUNCTION__, g_probe_info.suspend_state));
	if (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) {
		g_probe_info.suspend_state = USBOS_SUSPEND_STATE_SUSPENDED;
		return 0;
	}
	else {
		return -EBUSY;
	}
}

/**
 * The resume method is called to tell the driver that the device has been resumed and the driver
 * can return to normal operation.  URBs may once more be submitted.
 */
static int dbus_usbos_resume(struct usb_interface *intf)
{
	DBUSERR(("%s Device resumed\n", __FUNCTION__));
	g_probe_info.suspend_state = USBOS_SUSPEND_STATE_DEVICE_ACTIVE;
	return 0;
}
#endif /* ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) */

/**
 * Called by Linux kernel at initialization time, kernel wants to know if our driver will accept the
 * caller supplied USB interface. Note that USB drivers are bound to interfaces, and not to USB
 * devices.
 */
static int
dbus_usbos_probe(struct usb_interface *intf, const struct usb_device_id *id)
#else
static void *
dbus_usbos_probe(struct usb_device *usb, unsigned int ifnum, const struct usb_device_id *id)
#endif /* KERNEL26 */
{
	int ep;
	struct usb_endpoint_descriptor *endpoint;
	int ret = 0;
#ifdef KERNEL26
	struct usb_device *usb = interface_to_usbdev(intf);
#else
	int claimed = 0;
#endif
	int num_of_eps;
#ifdef BCMUSBDEV_COMPOSITE
	int wlan_if = -1;
	bool intr_ep = FALSE;
#endif /* BCMUSBDEV_COMPOSITE */

#ifdef BCMUSBDEV_COMPOSITE
	wlan_if = dbus_usbos_intf_wlan(usb);
#ifdef KERNEL26
	if ((wlan_if >= 0) && (IFPTR(usb, wlan_if) == intf)) {
#else
	if (wlan_if == ifnum) {
#endif /* KERNEL26 */
#endif /* BCMUSBDEV_COMPOSITE */
		g_probe_info.usb = usb;
		g_probe_info.dldone = TRUE;
#ifdef BCMUSBDEV_COMPOSITE
	} else {
		DBUSTRACE(("dbus_usbos_probe: skip probe for non WLAN interface\n"));
		ret = BCME_UNSUPPORTED;
		goto fail;
	}
#endif /* BCMUSBDEV_COMPOSITE */

#ifdef KERNEL26
	g_probe_info.intf = intf;
#endif /* KERNEL26 */

#ifdef BCMUSBDEV_COMPOSITE
	if (IFDESC(usb, wlan_if).bInterfaceNumber > USB_COMPIF_MAX) {
#else
	if (IFDESC(usb, CONTROL_IF).bInterfaceNumber) {
#endif /* BCMUSBDEV_COMPOSITE */
		ret = -1;
		goto fail;
	}
	if (id != NULL) {
		g_probe_info.vid = id->idVendor;
		g_probe_info.pid = id->idProduct;
	}

#ifdef KERNEL26
	usb_set_intfdata(intf, &g_probe_info);
#endif

	/* Check that the device supports only one configuration */
	if (usb->descriptor.bNumConfigurations != 1) {
		ret = -1;
		goto fail;
	}

	if (usb->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC) {
#ifdef BCMUSBDEV_COMPOSITE
		if ((usb->descriptor.bDeviceClass != USB_CLASS_MISC) &&
			(usb->descriptor.bDeviceClass != USB_CLASS_WIRELESS)) {
#endif /* BCMUSBDEV_COMPOSITE */
			ret = -1;
			goto fail;
#ifdef BCMUSBDEV_COMPOSITE
		}
#endif /* BCMUSBDEV_COMPOSITE */
	}

	/*
	 * Only the BDC interface configuration is supported:
	 *	Device class: USB_CLASS_VENDOR_SPEC
	 *	if0 class: USB_CLASS_VENDOR_SPEC
	 *	if0/ep0: control
	 *	if0/ep1: bulk in
	 *	if0/ep2: bulk out (ok if swapped with bulk in)
	 */
	if (CONFIGDESC(usb)->bNumInterfaces != 1) {
#ifdef BCMUSBDEV_COMPOSITE
		if (CONFIGDESC(usb)->bNumInterfaces > USB_COMPIF_MAX) {
#endif /* BCMUSBDEV_COMPOSITE */
			ret = -1;
			goto fail;
#ifdef BCMUSBDEV_COMPOSITE
		}
#endif /* BCMUSBDEV_COMPOSITE */
	}

	/* Check interface */
#ifndef KERNEL26
#ifdef BCMUSBDEV_COMPOSITE
	if (usb_interface_claimed(IFPTR(usb, wlan_if))) {
#else
	if (usb_interface_claimed(IFPTR(usb, CONTROL_IF))) {
#endif /* BCMUSBDEV_COMPOSITE */
		ret = -1;
		goto fail;
	}
#endif /* !KERNEL26 */

#ifdef BCMUSBDEV_COMPOSITE
	if ((IFDESC(usb, wlan_if).bInterfaceClass != USB_CLASS_VENDOR_SPEC ||
		IFDESC(usb, wlan_if).bInterfaceSubClass != 2 ||
		IFDESC(usb, wlan_if).bInterfaceProtocol != 0xff) &&
		(IFDESC(usb, wlan_if).bInterfaceClass != USB_CLASS_MISC ||
		IFDESC(usb, wlan_if).bInterfaceSubClass != USB_SUBCLASS_COMMON ||
		IFDESC(usb, wlan_if).bInterfaceProtocol != USB_PROTO_IAD)) {
			DBUSERR(("%s: invalid control interface: class %d, subclass %d, proto %d\n",
				__FUNCTION__,
				IFDESC(usb, wlan_if).bInterfaceClass,
				IFDESC(usb, wlan_if).bInterfaceSubClass,
				IFDESC(usb, wlan_if).bInterfaceProtocol));
#else
	if (IFDESC(usb, CONTROL_IF).bInterfaceClass != USB_CLASS_VENDOR_SPEC ||
		IFDESC(usb, CONTROL_IF).bInterfaceSubClass != 2 ||
		IFDESC(usb, CONTROL_IF).bInterfaceProtocol != 0xff) {
			DBUSERR(("%s: invalid control interface: class %d, subclass %d, proto %d\n",
				__FUNCTION__,
				IFDESC(usb, CONTROL_IF).bInterfaceClass,
				IFDESC(usb, CONTROL_IF).bInterfaceSubClass,
				IFDESC(usb, CONTROL_IF).bInterfaceProtocol));
#endif /* BCMUSBDEV_COMPOSITE */
			ret = -1;
			goto fail;
	}

	/* Check control endpoint */
#ifdef BCMUSBDEV_COMPOSITE
	endpoint = &IFEPDESC(usb, wlan_if, 0);
#else
	endpoint = &IFEPDESC(usb, CONTROL_IF, 0);
#endif /* BCMUSBDEV_COMPOSITE */
	if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT) {
#ifdef BCMUSBDEV_COMPOSITE
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) !=
			USB_ENDPOINT_XFER_BULK) {
#endif /* BCMUSBDEV_COMPOSITE */
			DBUSERR(("%s: invalid control endpoint %d\n",
				__FUNCTION__, endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK));
			ret = -1;
			goto fail;
#ifdef BCMUSBDEV_COMPOSITE
		}
#endif /* BCMUSBDEV_COMPOSITE */
	}

#ifdef BCMUSBDEV_COMPOSITE
	if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
#endif /* BCMUSBDEV_COMPOSITE */
		g_probe_info.intr_pipe =
			usb_rcvintpipe(usb, endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
#ifdef BCMUSBDEV_COMPOSITE
		intr_ep = TRUE;
	}
#endif /* BCMUSBDEV_COMPOSITE */

#ifndef KERNEL26
	/* Claim interface */
#ifdef BCMUSBDEV_COMPOSITE
	usb_driver_claim_interface(&dbus_usbdev, IFPTR(usb, wlan_if), &g_probe_info);
#else
	usb_driver_claim_interface(&dbus_usbdev, IFPTR(usb, CONTROL_IF), &g_probe_info);
#endif /* BCMUSBDEV_COMPOSITE */
	claimed = 1;
#endif /* !KERNEL26 */
	g_probe_info.rx_pipe = 0;
	g_probe_info.rx_pipe2 = 0;
	g_probe_info.tx_pipe = 0;
#ifdef BCMUSBDEV_COMPOSITE
	if (intr_ep)
		ep = 1;
	else
		ep = 0;
	num_of_eps = IFDESC(usb, wlan_if).bNumEndpoints - 1;
#else
	num_of_eps = IFDESC(usb, BULK_IF).bNumEndpoints - 1;
#endif /* BCMUSBDEV_COMPOSITE */

	if ((num_of_eps != 2) && (num_of_eps != 3)) {
#ifdef BCMUSBDEV_COMPOSITE
		if (num_of_eps > 7)
#endif /* BCMUSBDEV_COMPOSITE */
			ASSERT(0);
	}
	/* Check data endpoints and get pipes */
#ifdef BCMUSBDEV_COMPOSITE
	for (; ep <= num_of_eps; ep++) {
		endpoint = &IFEPDESC(usb, wlan_if, ep);
#else
	for (ep = 1; ep <= num_of_eps; ep++) {
		endpoint = &IFEPDESC(usb, BULK_IF, ep);
#endif /* BCMUSBDEV_COMPOSITE */
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) !=
		    USB_ENDPOINT_XFER_BULK) {
			DBUSERR(("%s: invalid data endpoint %d\n",
			           __FUNCTION__, ep));
			ret = -1;
			goto fail;
		}

		if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {
			if (!g_probe_info.rx_pipe) {
				g_probe_info.rx_pipe = usb_rcvbulkpipe(usb,
					(endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK));
			} else {
				g_probe_info.rx_pipe2 = usb_rcvbulkpipe(usb,
					(endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK));
			}

		} else
			g_probe_info.tx_pipe = usb_sndbulkpipe(usb, (endpoint->bEndpointAddress &
			     USB_ENDPOINT_NUMBER_MASK));
	}

	/* Allocate interrupt URB and data buffer */
	/* RNDIS says 8-byte intr, our old drivers used 4-byte */
#ifdef BCMUSBDEV_COMPOSITE
	g_probe_info.intr_size = (IFEPDESC(usb, wlan_if, 0).wMaxPacketSize == 16) ? 8 : 4;
	g_probe_info.interval = IFEPDESC(usb, wlan_if, 0).bInterval;
	usb->quirks |= USB_QUIRK_NO_SET_INTF;
#else
	g_probe_info.intr_size = (IFEPDESC(usb, CONTROL_IF, 0).wMaxPacketSize == 16) ? 8 : 4;
	g_probe_info.interval = IFEPDESC(usb, CONTROL_IF, 0).bInterval;
#endif /* BCMUSBDEV_COMPOSITE */

#ifndef KERNEL26
	/* usb_fill_int_urb does the interval decoding in 2.6 */
	if (usb->speed == USB_SPEED_HIGH)
		g_probe_info.interval = 1 << (g_probe_info.interval - 1);
#endif
	if (usb->speed == USB_SPEED_SUPER) {
		g_probe_info.device_speed = SUPER_SPEED;
		DBUSERR(("super speed device detected\n"));
	} else if (usb->speed == USB_SPEED_HIGH) {
		g_probe_info.device_speed = HIGH_SPEED;
		DBUSERR(("high speed device detected\n"));
	} else {
		g_probe_info.device_speed = FULL_SPEED;
		DBUSERR(("full speed device detected\n"));
	}
	if (g_probe_info.dereged == FALSE && probe_cb) {
		disc_arg = probe_cb(probe_arg, "", USB_BUS, 0);
	}

	g_probe_info.disc_cb_done = FALSE;

	/* Success */
#ifdef KERNEL26
	return DBUS_OK;
#else
	usb_inc_dev_use(usb);
	return &g_probe_info;
#endif

fail:
#ifdef BCMUSBDEV_COMPOSITE
	if (ret != BCME_UNSUPPORTED)
#endif /* BCMUSBDEV_COMPOSITE */
		DBUSERR(("%s: failed with errno %d\n", __FUNCTION__, ret));
#ifndef KERNEL26
	if (claimed)
#ifdef BCMUSBDEV_COMPOSITE
		usb_driver_release_interface(&dbus_usbdev, IFPTR(usb, wlan_if));
#else
		usb_driver_release_interface(&dbus_usbdev, IFPTR(usb, CONTROL_IF));
#endif /* BCMUSBDEV_COMPOSITE */
#endif /* !KERNEL26 */

#ifdef KERNEL26
	usb_set_intfdata(intf, NULL);
	return ret;
#else
	return NULL;
#endif
}

/** Called by Linux kernel, is the counter part of dbus_usbos_probe() */
#ifdef KERNEL26
static void
dbus_usbos_disconnect(struct usb_interface *intf)
#else
static void
dbus_usbos_disconnect(struct usb_device *usb, void *ptr)
#endif
{
#ifdef KERNEL26
	struct usb_device *usb = interface_to_usbdev(intf);
	probe_info_t *probe_usb_init_data = usb_get_intfdata(intf);
#else
	probe_info_t *probe_usb_init_data = (probe_info_t *) ptr;
#endif
	usbos_info_t *usbos_info;

	if ((probe_usb_init_data == NULL) || (usb == NULL)) {
		/* Should never happen */
		ASSERT(0);
		return;
	}

	usbos_info = (usbos_info_t *) probe_usb_init_data->usbos_info;
	if (usbos_info) {
		if ((probe_usb_init_data->dereged == FALSE) && disconnect_cb) {
			disconnect_cb(disc_arg);
			probe_usb_init_data->disc_cb_done = TRUE;
		}
	}

#ifndef KERNEL26
#ifdef BCMUSBDEV_COMPOSITE
	usb_driver_release_interface(&dbus_usbdev, IFPTR(usb, wlan_if));
#else
	usb_driver_release_interface(&dbus_usbdev, IFPTR(usb, CONTROL_IF));
#endif /* BCMUSBDEV_COMPOSITE */
	usb_dec_dev_use(usb);
#endif /* !KERNEL26 */
}

/** Higher layer (dbus_usb.c) wants to transmit an I/O Request Block */
static int
dbus_usbos_intf_send_irb(void *bus, dbus_irb_tx_t *txirb)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	urb_req_t *req;
	int ret = DBUS_OK;
	unsigned long flags;
	void *pkt;

	if (usbos_info == NULL)
		return DBUS_ERR;

	if (!(req = dbus_usbos_qdeq(&usbos_info->req_freeq, &usbos_info->free_lock))) {
		DBUSERR(("%s No free URB!\n", __FUNCTION__));
		return DBUS_ERR_TXDROP;
	}

	spin_lock_irqsave(&usbos_info->txlock, flags);
	req->arg = txirb;
	req->usbinfo = usbos_info;

	/* Prepare the URB */
	if (txirb->buf) {
		usb_fill_bulk_urb(req->urb, usbos_info->usb, usbos_info->tx_pipe, txirb->buf,
			txirb->len, (usb_complete_t)dbus_usbos_send_complete, req);
	} else if (txirb->pkt) {
		uint32 len = 0, pktlen = 0;
		void *transfer_buf;
#ifdef WL_USB_ZLP_PAD
		uint32	zlpflag = 0;
#endif
		len = pkttotlen(usbos_info->pub->osh, txirb->pkt);
		/* check the length and change if not 4 bytes aligned. */
		len	= ROUNDUP(len, sizeof(uint32));
#ifdef WL_USB_ZLP_PAD
		zlpflag = !(len % BCM_USB_HOST_TOTALLEN_ZLP);
		/* the single packets which need ZLP are also processed here */
		if (PKTNEXT(osbos_info->pub->osh, txirb->pkt) || zlpflag)
#else
		if (PKTNEXT(osbos_info->pub->osh, txirb->pkt))
#endif
		{

#ifdef WL_USB_ZLP_PAD
			if (zlpflag) {
				/* create space for pad bytes */
				len += BCM_USB_HOST_TOTALLEN_ZLP_PAD;
			}
#endif
			transfer_buf = kmalloc(len, GFP_ATOMIC);
			if (!transfer_buf) {
				ret = DBUS_ERR_TXDROP;
				DBUSERR(("fail to alloc to usb buffer\n"));
				goto fail;
			}

			pkt = txirb->pkt;
			txirb->send_buf = transfer_buf;
			while (pkt) {
				pktlen = PKTLEN(usbos_info->pub->osh, pkt);
				bcopy(PKTDATA(usbos_info->pub->osh, pkt), transfer_buf, pktlen);
				transfer_buf += pktlen;
				pkt = PKTNEXT(usbos_info->pub->osh, pkt);
			}

#ifdef WL_USB_ZLP_PAD
			if (zlpflag) {
				/* set the value of pad bytes to zero */
				bzero(txirb->send_buf +(len - BCM_USB_HOST_TOTALLEN_ZLP_PAD),
					BCM_USB_HOST_TOTALLEN_ZLP_PAD);
			}
#endif
			usb_fill_bulk_urb(req->urb, usbos_info->usb, usbos_info->tx_pipe,
				txirb->send_buf,
				len,
				(usb_complete_t)dbus_usbos_send_complete, req);

		} else {
			txirb->send_buf = NULL;
			len = PKTLEN(usbos_info->pub->osh, txirb->pkt);
			len = ROUNDUP(len, sizeof(uint32));
			usb_fill_bulk_urb(req->urb, usbos_info->usb, usbos_info->tx_pipe,
				PKTDATA(usbos_info->pub->osh, txirb->pkt),
				len,
				(usb_complete_t)dbus_usbos_send_complete, req);
		}
	} else {
		ASSERT(0);
	}

	req->urb->transfer_flags |= URB_QUEUE_BULK;

	if ((ret = USB_SUBMIT_URB(req->urb))) {
		dbus_usbos_qenq(&usbos_info->req_freeq, req, &usbos_info->free_lock);
		if (txirb->send_buf) {
			kfree(txirb->send_buf);
		}
		ret = DBUS_ERR_TXDROP;
		goto fail;
	}

	usbos_info->txposted++;

	dbus_usbos_qenq(&usbos_info->req_txpostedq, req, &usbos_info->txposted_lock);
fail:
	spin_unlock_irqrestore(&usbos_info->txlock, flags);
	return ret;
}

/** Higher layer (dbus_usb.c) recycles a received (and used) packet. */
static int
dbus_usbos_intf_recv_irb(void *bus, dbus_irb_rx_t *rxirb)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	int ret = DBUS_OK;

	if (usbos_info == NULL)
		return DBUS_ERR;

	ret = dbus_usbos_recv_urb_submit(usbos_info, rxirb, 0);
	return ret;
}

static int
dbus_usbos_intf_recv_irb_from_ep(void *bus, dbus_irb_rx_t *rxirb, uint32 ep_idx)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	int ret = DBUS_OK;

	if (usbos_info == NULL)
		return DBUS_ERR;

	ret = dbus_usbos_recv_urb_submit(usbos_info, rxirb, ep_idx);
	return ret;
}

/** Higher layer (dbus_usb.c) want to cancel an IRB */
static int
dbus_usbos_intf_cancel_irb(void *bus, dbus_irb_tx_t *txirb)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if (usbos_info == NULL)
		return DBUS_ERR;

	return DBUS_ERR;
}

static int
dbus_usbos_intf_send_ctl(void *bus, uint8 *buf, int len)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	int ret = DBUS_OK;
	uint16 size;

	if ((usbos_info == NULL) || (buf == NULL) || (len == 0))
		return DBUS_ERR;

	if (usbos_info->ctl_urb == NULL)
		return DBUS_ERR;

	size = len;
	usbos_info->ctl_write.wLength = cpu_to_le16p(&size);
	usbos_info->ctl_urb->transfer_buffer_length = size;

	usb_fill_control_urb(usbos_info->ctl_urb,
		usbos_info->usb,
		usbos_info->ctl_out_pipe,
		(unsigned char *) &usbos_info->ctl_write,
		buf, size, (usb_complete_t)dbus_usbos_ctlwrite_complete, usbos_info);

	ret = USB_SUBMIT_URB(usbos_info->ctl_urb);
	if (ret < 0) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		return DBUS_ERR_TXCTLFAIL;
	}

	return DBUS_OK;
}

/** This function does not seem to be called by anyone, including dbus_usb.c */
static int
dbus_usbos_intf_recv_ctl(void *bus, uint8 *buf, int len)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	int ret = DBUS_OK;
	uint16 size;
#ifdef USBOS_THREAD
	unsigned long flags;
#endif /* USBOS_THREAD */

	if ((usbos_info == NULL) || (buf == NULL) || (len == 0))
		return DBUS_ERR;

	if (usbos_info->ctl_urb == NULL)
		return DBUS_ERR;

#ifdef USBOS_THREAD
	spin_lock_irqsave(&usbos_info->ctrl_lock, flags);
#endif
	size = len;
	usbos_info->ctl_read.wLength = cpu_to_le16p(&size);
	usbos_info->ctl_urb->transfer_buffer_length = size;

	if (usbos_info->rxctl_deferrespok) {
		/* BMAC model */
		usbos_info->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR |
			USB_RECIP_INTERFACE;
		usbos_info->ctl_read.bRequest = DL_DEFER_RESP_OK;
	} else {
		/* full dongle model */
		usbos_info->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_CLASS |
			USB_RECIP_INTERFACE;
		usbos_info->ctl_read.bRequest = 1;
	}

	usb_fill_control_urb(usbos_info->ctl_urb,
		usbos_info->usb,
		usbos_info->ctl_in_pipe,
		(unsigned char *) &usbos_info->ctl_read,
		buf, size, (usb_complete_t)dbus_usbos_ctlread_complete, usbos_info);

	ret = USB_SUBMIT_URB(usbos_info->ctl_urb);
#ifdef USBOS_THREAD
	spin_unlock_irqrestore(&usbos_info->ctrl_lock, flags);
#endif
	if (ret < 0) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		return DBUS_ERR_RXCTLFAIL;
	}

	return ret;
}

static int
dbus_usbos_intf_get_attrib(void *bus, dbus_attrib_t *attrib)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if ((usbos_info == NULL) || (attrib == NULL))
		return DBUS_ERR;

	attrib->bustype = DBUS_USB;
	attrib->vid = g_probe_info.vid;
	attrib->pid = g_probe_info.pid;
	attrib->devid = 0x4322;

	attrib->nchan = 1;

	/* MaxPacketSize for USB hi-speed bulk out is 512 bytes
	 * and 64-bytes for full-speed.
	 * When sending pkt > MaxPacketSize, Host SW breaks it
	 * up into multiple packets.
	 */
	attrib->mtu = usbos_info->maxps;

	return DBUS_OK;
}

/** Called by higher layer (dbus_usb.c) when it wants to 'up' the USB interface to the dongle */
static int
dbus_usbos_intf_up(void *bus)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	uint16 ifnum;
#ifdef BCMUSBDEV_COMPOSITE
	int wlan_if = 0;
#endif
	if (usbos_info == NULL)
		return DBUS_ERR;

	if (usbos_info->usb == NULL)
		return DBUS_ERR;

#if (defined(WLC_HIGH) && defined(WLC_LOW))
	/* full dongle use intr EP, bmac doesn't use it */
	if (usbos_info->intr_urb) {
		int ret;

		usb_fill_int_urb(usbos_info->intr_urb, usbos_info->usb,
			usbos_info->intr_pipe, &usbos_info->intr,
			usbos_info->intr_size, (usb_complete_t)dbus_usbos_intr_complete,
			usbos_info, usbos_info->interval);

		if ((ret = USB_SUBMIT_URB(usbos_info->intr_urb))) {
			DBUSERR(("%s USB_SUBMIT_URB failed with status %d\n", __FUNCTION__, ret));
			return DBUS_ERR;
		}
	}
#endif	/* (defined(WLC_HIGH) && defined(WLC_LOW)) */

	if (usbos_info->ctl_urb) {
		usbos_info->ctl_in_pipe = usb_rcvctrlpipe(usbos_info->usb, 0);
		usbos_info->ctl_out_pipe = usb_sndctrlpipe(usbos_info->usb, 0);

#ifdef BCMUSBDEV_COMPOSITE
		wlan_if = dbus_usbos_intf_wlan(usbos_info->usb);
		ifnum = cpu_to_le16(IFDESC(usbos_info->usb, wlan_if).bInterfaceNumber);
#else
		ifnum = cpu_to_le16(IFDESC(usbos_info->usb, CONTROL_IF).bInterfaceNumber);
#endif /* BCMUSBDEV_COMPOSITE */
		/* CTL Write */
		usbos_info->ctl_write.bRequestType =
			USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		usbos_info->ctl_write.bRequest = 0;
		usbos_info->ctl_write.wValue = cpu_to_le16(0);
		usbos_info->ctl_write.wIndex = cpu_to_le16p(&ifnum);

		/* CTL Read */
		usbos_info->ctl_read.bRequestType =
			USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		usbos_info->ctl_read.bRequest = 1;
		usbos_info->ctl_read.wValue = cpu_to_le16(0);
		usbos_info->ctl_read.wIndex = cpu_to_le16p(&ifnum);
	}

	/* Success, indicate usbos_info is fully up */
	dbus_usbos_state_change(usbos_info, DBUS_STATE_UP);

	return DBUS_OK;
}

static int
dbus_usbos_intf_down(void *bus)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if (usbos_info == NULL)
		return DBUS_ERR;

	dbusos_stop(usbos_info);
	return DBUS_OK;
}

static int
dbus_usbos_intf_stop(void *bus)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if (usbos_info == NULL)
		return DBUS_ERR;

	dbusos_stop(usbos_info);
	return DBUS_OK;
}

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
static void
dbus_usbos_intf_dump(void *bus, struct bcmstrbuf *b)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	int i = 0, j = 0;

	if (b) {
		bcm_bprintf(b, "\ndbus linux dump\n");
		bcm_bprintf(b, "txposted %d rxposted %d\n",
			usbos_info->txposted, usbos_info->rxposted);

		bcm_bprintf(b, "RXDPC: dpc_cnt %d dpc_pktcnt %d dpc_maxpktcnt %d avg_dpc_pktcnt\n",
			usbos_info->dpc_cnt, usbos_info->dpc_pktcnt,
			usbos_info->dpc_maxpktcnt, usbos_info->dpc_cnt ?
			(usbos_info->dpc_pktcnt/usbos_info->dpc_cnt):1);

		/* Histogram */
		bcm_bprintf(b, "txposted\n");
	} else {
		printf("\ndbus linux dump\n");
		printf("txposted %d rxposted %d\n",
			usbos_info->txposted, usbos_info->rxposted);
		printf("RXDPC: dpc_cnt %d dpc_pktcnt %d dpc_maxpktcnt %d avg_dpc_pktcnt %d\n",
			usbos_info->dpc_cnt, usbos_info->dpc_pktcnt,
			usbos_info->dpc_maxpktcnt, usbos_info->dpc_cnt ?
			(usbos_info->dpc_pktcnt/usbos_info->dpc_cnt):1);

		/* Histogram */
		printf("txposted\n");
	}

	for (i = 0; i < usbos_info->pub->ntxq; i++) {
		if (usbos_info->txposted_hist == NULL) {
			break;
		}
		if (usbos_info->txposted_hist[i]) {
			if (b)
				bcm_bprintf(b, "%d: %d ", i, usbos_info->txposted_hist[i]);
			else
				printf("%d: %d ", i, usbos_info->txposted_hist[i]);
			j++;
			if (j % 10 == 0) {
				if (b)
					bcm_bprintf(b, "\n");
				else
					printf("\n");
			}
		}
	}

	j = 0;
	if (b)
		bcm_bprintf(b, "\nrxposted\n");
	else
		printf("\nrxposted\n");
	for (i = 0; i < usbos_info->pub->nrxq; i++) {
		if (usbos_info->rxposted_hist == NULL) {
			break;
		}
		if (usbos_info->rxposted_hist[i]) {
			if (b)
				bcm_bprintf(b, "%d: %d ", i, usbos_info->rxposted_hist[i]);
			else
				printf("%d: %d ", i, usbos_info->rxposted_hist[i]);
			j++;
			if (j % 10 == 0) {
				if (b)
					bcm_bprintf(b, "\n");
				else
					printf("\n");
			}
		}
	}
	if (b)
		bcm_bprintf(b, "\n");
	else
		printf("\n");

	return;
}
#endif /* BCMDBG || DBUS_LINUX_HIST */

/** Called by higher layer (dbus_usb.c) */
static int
dbus_usbos_intf_set_config(void *bus, dbus_config_t *config)
{
	usbos_info_t* usbos_info = bus;

	usbos_info->rxctl_deferrespok = config->rxctl_deferrespok;
	return DBUS_OK;
}

/**
 * In some cases, the code must submit an URB and wait for its completion.
 * Related: dbus_usbos_sync_complete()
 */
static int
dbus_usbos_sync_wait(usbos_info_t *usbinfo, uint16 time)
{
	int ret;
	int err = DBUS_OK;
	int ms = time;

	ret = wait_event_interruptible_timeout(usbinfo->wait,
		usbinfo->waitdone == TRUE, (ms * HZ / 1000));

	if ((usbinfo->waitdone == FALSE) || (usbinfo->sync_urb_status)) {
		DBUSERR(("%s: timeout(%d) or urb err=0x%x\n",
			__FUNCTION__, ret, usbinfo->sync_urb_status));
		err = DBUS_ERR;
		BCM_REFERENCE(ret);
	}
	usbinfo->waitdone = FALSE;
	return err;
}

/**
 * In some cases, the code must submit an URB and wait for its completion.
 * Related: dbus_usbos_sync_wait()
 */
static void
dbus_usbos_sync_complete(CALLBACK_ARGS)
{
	usbos_info_t *usbos_info = (usbos_info_t *)urb->context;

	usbos_info->waitdone = TRUE;
	wake_up_interruptible(&usbos_info->wait);

	usbos_info->sync_urb_status = urb->status;

	if (urb->status) {
		DBUSERR(("%s: sync urb error %d\n", __FUNCTION__, urb->status));
	}
}

/** Called by dbus_usb.c when it wants to download firmware into the dongle */
bool
dbus_usbos_dl_cmd(usbos_info_t *usbinfo, uint8 cmd, void *buffer, int buflen)
{
	int ret = DBUS_OK;
	char *tmpbuf;
	uint16 size;

	if ((usbinfo == NULL) || (buffer == NULL) || (buflen == 0))
		return FALSE;

	if (usbinfo->ctl_urb == NULL)
		return FALSE;

	tmpbuf = (char *) MALLOC(usbinfo->pub->osh, buflen);
	if (!tmpbuf) {
		DBUSERR(("%s: Unable to allocate memory \n", __FUNCTION__));
		return FALSE;
	}

	size = buflen;
	usbinfo->ctl_urb->transfer_buffer_length = size;

	usbinfo->ctl_read.wLength = cpu_to_le16p(&size);
	usbinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR |
		USB_RECIP_INTERFACE;
	usbinfo->ctl_read.bRequest = cmd;

	if (cmd == DL_GO) {
		usbinfo->ctl_read.wIndex = 1;
	}

	usb_fill_control_urb(usbinfo->ctl_urb,
		usbinfo->usb,
		usb_rcvctrlpipe(usbinfo->usb, 0),
		(unsigned char *) &usbinfo->ctl_read,
		(void *) tmpbuf, size, (usb_complete_t)dbus_usbos_sync_complete, usbinfo);

	ret = USB_SUBMIT_URB(usbinfo->ctl_urb);
	if (ret < 0) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		MFREE(usbinfo->pub->osh, tmpbuf, buflen);
		return FALSE;
	}

	ret = dbus_usbos_sync_wait(usbinfo, USB_SYNC_WAIT_TIMEOUT);
	memcpy(buffer, tmpbuf, buflen);
	MFREE(usbinfo->pub->osh, tmpbuf, buflen);

	return (ret == DBUS_OK);
}

/**
 * Called by dbus_usb.c when it wants to download a buffer into the dongle (e.g. as part of the
 * download process, when writing nvram variables).
 */
int
dbus_write_membytes(usbos_info_t* usbinfo, bool set, uint32 address, uint8 *data, uint size)
{
	hwacc_t hwacc;
	int write_bytes = 4;
	int status;
	int retval = 0;

	DBUSTRACE(("Enter:%s\n", __FUNCTION__));

	/* Read is not supported */
	if (set == 0) {
		DBUSERR(("Currently read is not supported!!\n"));
		return -1;
	}

	hwacc.cmd = DL_CMD_WRHW;
	hwacc.addr = address;

	DBUSTRACE(("Address:%x size:%d", hwacc.addr, size));
	do {
		if (size >= 4) {
			write_bytes = 4;
		} else if (size >= 2) {
			write_bytes = 2;
		} else {
			write_bytes = 1;
		}

		hwacc.len = write_bytes;

		while (size >= write_bytes) {
			hwacc.data = *((unsigned int*)data);

			status = USB_CONTROL_MSG(usbinfo->usb, usb_sndctrlpipe(usbinfo->usb, 0),
				DL_WRHW, (USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE),
				1, 0, (char *)&hwacc, sizeof(hwacc_t), USB_CTRL_EP_TIMEOUT);

			if (status < 0) {
				retval = -1;
				DBUSERR((" Ctrl write hwacc failed w/status %d @ address:%x \n",
					status, hwacc.addr));
				goto err;
			}

			hwacc.addr += write_bytes;
			data += write_bytes;
			size -= write_bytes;
		}
	} while (size > 0);

err:
	return retval;
}


static int
dbus_usbos_readreg(void *bus, uint32 regaddr, int datalen, uint32 *value)
{
	usbos_info_t *usbinfo = (usbos_info_t *) bus;
	int ret = DBUS_ERR;
	uint32 cmd;
	hwacc_t	hwacc;
	uint16 size = sizeof(hwacc_t);

	if (datalen == 1)
		cmd = DL_RDHW8;
	else if (datalen == 2)
		cmd = DL_RDHW16;
	else
		cmd = DL_RDHW32;

	if (usbinfo == NULL)
		return ret;

	if (usbinfo->ctl_urb == NULL)
		return ret;

	usbinfo->ctl_urb->transfer_buffer_length = size;

	usbinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR |
		USB_RECIP_INTERFACE;
	usbinfo->ctl_read.bRequest = cmd;
	usbinfo->ctl_read.wLength = htol16(size);
	usbinfo->ctl_read.wValue = htol16(regaddr & 0xFFFF);
	usbinfo->ctl_read.wIndex = htol16(regaddr >> 16);


	usb_fill_control_urb(usbinfo->ctl_urb,
		usbinfo->usb,
		usb_rcvctrlpipe(usbinfo->usb, 0),
		(unsigned char *) &usbinfo->ctl_read,
		(void *) &hwacc, size, (usb_complete_t)dbus_usbos_sync_complete, usbinfo);

	ret = USB_SUBMIT_URB(usbinfo->ctl_urb);
	if (ret < 0) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		return ret;
	}

	ret = dbus_usbos_sync_wait(usbinfo, USB_SYNC_WAIT_TIMEOUT);

	if (ret == DBUS_OK) {
		*value = hwacc.data;
		return ret;
	}
	return ret;
}

int
dbus_usbos_writereg(void *bus, uint32 regaddr, int datalen, uint32 data)
{
	usbos_info_t *usbinfo = (usbos_info_t *) bus;
	int ret = DBUS_ERR;
	uint32 cmd = DL_WRHW;
	hwacc_t	hwacc;
	uint16 size = sizeof(hwacc_t);

	if (usbinfo == NULL)
		return ret;

	if (usbinfo->ctl_urb == NULL)
		return ret;

	hwacc.cmd = DL_WRHW;
	hwacc.addr = regaddr;
	hwacc.data = data;
	hwacc.len = datalen;

	usbinfo->ctl_urb->transfer_buffer_length = size;

	usbinfo->ctl_write.bRequestType = USB_DIR_OUT| USB_TYPE_VENDOR | USB_RECIP_INTERFACE;
	usbinfo->ctl_write.bRequest = cmd;
	usbinfo->ctl_write.wLength = htol16(size);
	usbinfo->ctl_write.wValue = htol16(1);
	usbinfo->ctl_write.wIndex = htol16(0);

	usb_fill_control_urb(usbinfo->ctl_urb,
		usbinfo->usb,
		usb_sndctrlpipe(usbinfo->usb, 0),
		(unsigned char *) &usbinfo->ctl_write,
		(void *) &hwacc, size, (usb_complete_t)dbus_usbos_sync_complete, usbinfo);

	ret = USB_SUBMIT_URB(usbinfo->ctl_urb);
	if (ret < 0) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		return ret;
	}

	ret = dbus_usbos_sync_wait(usbinfo, USB_SYNC_WAIT_TIMEOUT);

	return ret;
}

int
dbus_usbos_wait(usbos_info_t *usbinfo, uint16 ms)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
	if (in_interrupt())
		mdelay(ms);
	else
		msleep_interruptible(ms);
#else
	wait_ms(ms);
#endif
	return DBUS_OK;
}

/** Called by dbus_usb.c as part of the firmware download process */
bool
dbus_usbos_dl_send_bulk(usbos_info_t *usbinfo, void *buffer, int len)
{
	int ret = DBUS_OK;

#ifdef EHCI_FASTPATH_TX
	struct ehci_qtd *qtd = optimize_ehci_qtd_alloc(GFP_KERNEL);
	int token = EHCI_QTD_SET_CERR(3);

	if (qtd == NULL)
		goto fail;

	optimize_qtd_fill_with_data(usbinfo->pub, 0, qtd, buffer, token, len);
	optimize_submit_async(qtd, 0);
#else
	if (usbinfo == NULL)
		goto fail;

	if (usbinfo->blk_urb == NULL)
		goto fail;

	/* Prepare the URB */
	usb_fill_bulk_urb(usbinfo->blk_urb, usbinfo->usb, usbinfo->tx_pipe, buffer,
		len, (usb_complete_t)dbus_usbos_sync_complete, usbinfo);

	usbinfo->blk_urb->transfer_flags |= URB_QUEUE_BULK;

	if ((ret = USB_SUBMIT_URB(usbinfo->blk_urb))) {
		DBUSERR(("%s: usb_submit_urb failed %d\n", __FUNCTION__, ret));
		goto fail;
	}
#endif /* EHCI_FASTPATH_TX */

	ret = dbus_usbos_sync_wait(usbinfo, USB_SYNC_WAIT_TIMEOUT);

	return (ret == DBUS_OK);
fail:
	return FALSE;
}

static bool
dbus_usbos_intf_recv_needed(void *bus)
{
	return FALSE;
}

/**
 * Higher layer (dbus_usb.c) wants to execute a function on the condition that the rx spin lock has
 * been acquired.
 */
static void*
dbus_usbos_intf_exec_rxlock(void *bus, exec_cb_t cb, struct exec_parms *args)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	void *ret;
	unsigned long flags;

	if (usbos_info == NULL)
		return NULL;

	spin_lock_irqsave(&usbos_info->rxlock, flags);
	ret = cb(args);
	spin_unlock_irqrestore(&usbos_info->rxlock, flags);

	return ret;
}

static void*
dbus_usbos_intf_exec_txlock(void *bus, exec_cb_t cb, struct exec_parms *args)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	void *ret;
	unsigned long flags;

	if (usbos_info == NULL)
		return NULL;

	spin_lock_irqsave(&usbos_info->txlock, flags);
	ret = cb(args);
	spin_unlock_irqrestore(&usbos_info->txlock, flags);

	return ret;
}

/**
 * if an error condition was detected in this module, the higher DBUS layer (dbus_usb.c) has to
 * be notified.
 */
static int
dbus_usbos_errhandler(void *bus, int err)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if (usbos_info == NULL)
		return DBUS_ERR;

	if (usbos_info->cbarg && usbos_info->cbs) {
		if (usbos_info->cbs->errhandler)
			usbos_info->cbs->errhandler(usbos_info->cbarg, err);
	}

	return DBUS_OK;
}

/**
 * if a change in bus state was detected in this module, the higher DBUS layer (dbus_usb.c) has to
 * be notified.
 */
static int
dbus_usbos_state_change(void *bus, int state)
{
	usbos_info_t *usbos_info = (usbos_info_t *) bus;

	if (usbos_info == NULL)
		return DBUS_ERR;

	if (usbos_info->cbarg && usbos_info->cbs) {
		if (usbos_info->cbs->state_change)
			usbos_info->cbs->state_change(usbos_info->cbarg, state);
	}

	usbos_info->pub->busstate = state;
	return DBUS_OK;
}

int
dbus_bus_osl_register(int vid, int pid, probe_cb_t prcb,
	disconnect_cb_t discb, void *prarg, dbus_intf_t **intf, void *param1, void *param2)
{
	bzero(&g_probe_info, sizeof(probe_info_t));

	probe_cb = prcb;
	disconnect_cb = discb;
	probe_arg = prarg;

	devid_table[0].idVendor = vid;
	devid_table[0].idProduct = pid;

	*intf = &dbus_usbos_intf;

	USB_REGISTER();

	return DBUS_ERR_NODEVICE;
}

int
dbus_bus_osl_deregister()
{
	g_probe_info.dereged = TRUE;

	if (disconnect_cb && (g_probe_info.disc_cb_done == FALSE))
		disconnect_cb(disc_arg);

	USB_DEREGISTER();

	return DBUS_OK;
}


void *
dbus_usbos_intf_attach(dbus_pub_t *pub, void *cbarg, dbus_intf_callbacks_t *cbs)
{
	usbos_info_t *usbos_info;

	if (g_probe_info.dldone == FALSE) {
		DBUSERR(("%s: err device not downloaded!\n", __FUNCTION__));
		return NULL;
	}

	/* Sanity check for BUS_INFO() */
	ASSERT(OFFSETOF(usbos_info_t, pub) == 0);

	usbos_info = MALLOC(pub->osh, sizeof(usbos_info_t));
	if (usbos_info == NULL)
		return NULL;

	bzero(usbos_info, sizeof(usbos_info_t));

	usbos_info->pub = pub;
	usbos_info->cbarg = cbarg;
	usbos_info->cbs = cbs;

	/* Needed for disconnect() */
	g_probe_info.usbos_info = usbos_info;

	/* Update USB Info */
	usbos_info->usb = g_probe_info.usb;
	usbos_info->rx_pipe = g_probe_info.rx_pipe;
	usbos_info->rx_pipe2 = g_probe_info.rx_pipe2;
	usbos_info->tx_pipe = g_probe_info.tx_pipe;
	usbos_info->intr_pipe = g_probe_info.intr_pipe;
	usbos_info->intr_size = g_probe_info.intr_size;
	usbos_info->interval = g_probe_info.interval;
	usbos_info->pub->device_speed = g_probe_info.device_speed;
	if (usbos_info->rx_pipe2) {
		usbos_info->pub->attrib.has_2nd_bulk_in_ep = 1;
	} else {
		usbos_info->pub->attrib.has_2nd_bulk_in_ep = 0;
	}

	if (usbos_info->tx_pipe)
		usbos_info->maxps = usb_maxpacket(usbos_info->usb,
			usbos_info->tx_pipe, usb_pipeout(usbos_info->tx_pipe));

	INIT_LIST_HEAD(&usbos_info->req_freeq);
	INIT_LIST_HEAD(&usbos_info->req_rxpostedq);
	INIT_LIST_HEAD(&usbos_info->req_txpostedq);
	spin_lock_init(&usbos_info->free_lock);
	spin_lock_init(&usbos_info->rxposted_lock);
	spin_lock_init(&usbos_info->txposted_lock);
	spin_lock_init(&usbos_info->rxlock);
	spin_lock_init(&usbos_info->txlock);

#ifdef DBUS_LINUX_RXDPC
	INIT_LIST_HEAD(&usbos_info->req_rxpendingq);
	spin_lock_init(&usbos_info->rxpending_lock);
#endif /* DBUS_LINUX_RXDPC */

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	usbos_info->txposted_hist = MALLOC(pub->osh, (usbos_info->pub->ntxq+1) * sizeof(int));
	if (usbos_info->txposted_hist) {
		bzero(usbos_info->txposted_hist, (usbos_info->pub->ntxq+1) * sizeof(int));
	}
	usbos_info->rxposted_hist = MALLOC(pub->osh, (usbos_info->pub->nrxq+1) * sizeof(int));
	if (usbos_info->rxposted_hist) {
		bzero(usbos_info->rxposted_hist, (usbos_info->pub->nrxq+1) * sizeof(int));
	}
#endif
#ifdef USB_DISABLE_INT_EP
	usbos_info->intr_urb = NULL;
#else
	if (!(usbos_info->intr_urb = USB_ALLOC_URB())) {
		DBUSERR(("%s: usb_alloc_urb (tx) failed\n", __FUNCTION__));
		goto fail;
	}
#endif

	if (!(usbos_info->ctl_urb = USB_ALLOC_URB())) {
		DBUSERR(("%s: usb_alloc_urb (tx) failed\n", __FUNCTION__));
		goto fail;
	}

	init_waitqueue_head(&usbos_info->wait);

	if (!(usbos_info->blk_urb = USB_ALLOC_URB())) {	/* for embedded image downloading */
		DBUSERR(("%s: usb_alloc_urb (tx) failed\n", __FUNCTION__));
		goto fail;
	}

	usbos_info->rxbuf_len = (uint)usbos_info->pub->rxsize;


#ifdef DBUS_LINUX_RXDPC		/* Initialize DPC thread */
	sema_init(&usbos_info->dpc_sem, 0);
	init_completion(&usbos_info->dpc_exited);
	usbos_info->dpc_pid = kernel_thread(dbus_usbos_dpc_thread, usbos_info, 0);
	if (usbos_info->dpc_pid < 0) {
		DBUSERR(("%s: failed to create dpc thread\n", __FUNCTION__));
		goto fail;
	}
#endif /* DBUS_LINUX_RXDPC */

	if (dbus_usbos_urbreqs_alloc(usbos_info) != DBUS_OK) {
		goto fail;
	}

#ifdef USBOS_THREAD
	if (dbus_usbos_thread_init(usbos_info) == NULL)
		goto fail;
#endif /* USBOS_THREAD */

	pub->dev_info = g_probe_info.usb;

#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
	spin_lock_init(&usbos_info->fastpath_lock);
	if (optimize_init(usbos_info, usbos_info->usb, usbos_info->tx_pipe,
		usbos_info->rx_pipe, usbos_info->rx_pipe2) != 0) {
		DBUSERR(("%s: optimize_init failed!\n", __FUNCTION__));
		goto fail;
	}

#endif /* EHCI_FASTPATH_TX || EHCI_FASTPATH_RX */

	return (void *) usbos_info;
fail:
#ifdef DBUS_LINUX_RXDPC
	if (usbos_info->dpc_pid >= 0) {
		KILL_PROC(usbos_info->dpc_pid, SIGTERM);
		wait_for_completion(&usbos_info->dpc_exited);
	}
#endif /* DBUS_LINUX_RXDPC */
	if (usbos_info->intr_urb) {
		USB_FREE_URB(usbos_info->intr_urb);
		usbos_info->intr_urb = NULL;
	}

	if (usbos_info->ctl_urb) {
		USB_FREE_URB(usbos_info->ctl_urb);
		usbos_info->ctl_urb = NULL;
	}

#if defined(BCM_DNGL_EMBEDIMAGE) || defined(BCM_REQUEST_FW)
	if (usbos_info->blk_urb) {
		USB_FREE_URB(usbos_info->blk_urb);
		usbos_info->blk_urb = NULL;
	}
#endif

	dbus_usbos_urbreqs_free(usbos_info);

	g_probe_info.usbos_info = NULL;

	MFREE(pub->osh, usbos_info, sizeof(usbos_info_t));
	return NULL;

}

void
dbus_usbos_intf_detach(dbus_pub_t *pub, void *info)
{
	usbos_info_t *usbos_info = (usbos_info_t *) info;
	osl_t *osh = pub->osh;

	if (usbos_info == NULL) {
		return;
	}

#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
	optimize_deinit(usbos_info, usbos_info->usb);
#endif
	/* Must unlink all URBs prior to driver unload;
	 * otherwise an URB callback can occur after driver
	 * has been de-allocated and rmmod'd
	 */
	dbusos_stop(usbos_info);

	if (usbos_info->intr_urb) {
		USB_FREE_URB(usbos_info->intr_urb);
		usbos_info->intr_urb = NULL;
	}

	if (usbos_info->ctl_urb) {
		USB_FREE_URB(usbos_info->ctl_urb);
		usbos_info->ctl_urb = NULL;
	}

	if (usbos_info->blk_urb) {
		USB_FREE_URB(usbos_info->blk_urb);
		usbos_info->blk_urb = NULL;
	}

	dbus_usbos_urbreqs_free(usbos_info);

#if defined(BCMDBG) || defined(DBUS_LINUX_HIST)
	if (usbos_info->txposted_hist) {
		MFREE(osh, usbos_info->txposted_hist, (usbos_info->pub->ntxq+1) * sizeof(int));
	}
	if (usbos_info->rxposted_hist) {
		MFREE(osh, usbos_info->rxposted_hist, (usbos_info->pub->nrxq+1) * sizeof(int));
	}
#endif /* BCMDBG || DBUS_LINUX_HIST */
#ifdef USBOS_THREAD
	dbus_usbos_thread_deinit(usbos_info);
#endif /* USBOS_THREAD */

	g_probe_info.usbos_info = NULL;
	MFREE(osh, usbos_info, sizeof(usbos_info_t));
}

/*
 *	Kernel need have CONFIG_PM and CONFIG_USB_SUSPEND enabled
 * 	autosuspend also has to be enabled. if not enable by default,
 *	set /sys/bus/usb/devices/.../power/level to auto, where ... is the device'S ID
 *
 *	wl_down ->
 *      bcm_rpc_sleep -> bcm_rpc_tp_sleep -> dbus_pnp_sleep -> dbus_usbos_intf_pnp ->
 *
 *      wl_up ->
 *      bcm_rpc_resume -> bcm_rpc_tp_resume -> dbus_pnp_resume -> dbus_usbos_intf_pnp ->
 */
static int
dbus_usbos_intf_pnp(void *bus, int evnt)
{
#if !((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND))
	DBUSERR(("PNP: %s not supported", __FUNCTION__));
	return DBUS_OK;
#else
	usbos_info_t *usbos_info = (usbos_info_t *) bus;
	DBUSERR(("PNP: %s evnt %d\n", __FUNCTION__, evnt));
	if (usbos_info == NULL)
		return DBUS_ERR;
	if (evnt == DBUS_PNP_RESUME) {
		DBUSTRACE(("intf pnp RESUME\n"));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
		atomic_set(&g_probe_info.intf->pm_usage_cnt, 1);
#else
		g_probe_info.intf->pm_usage_cnt = 1;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33))
		USB_AUTOPM_SET_INTERFACE(g_probe_info.intf);
#else
		USB_ENABLE_AUTOSUSPEND(g_probe_info.usb);
#endif

		if (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPEND_PENDING) {
			/* SUSPEND not happened yet! */
			g_probe_info.suspend_state = USBOS_SUSPEND_STATE_DEVICE_ACTIVE;
		}

		if (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_SUSPENDED) {
		SPINWAIT((g_probe_info.suspend_state ==
			USBOS_SUSPEND_STATE_DEVICE_ACTIVE), 3000);
		}

		if (g_probe_info.suspend_state == USBOS_SUSPEND_STATE_DEVICE_ACTIVE) {
			DBUSTRACE(("resume USB device OK\n"));
			if (!dbus_usbos_intf_up((void *)g_probe_info.usbos_info))
				return DBUS_OK;
		}
		DBUSERR(("resume failed\n"));
		return DBUS_ERR;
	} else if (evnt == DBUS_PNP_SLEEP) {
		DBUSTRACE(("PNP SLEEP\n"));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38))
		dbus_usbos_sleep(bus);
#endif
	}
	return DBUS_OK;
#endif /* ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) */
}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) \
	&& (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38))
/**
 * Before we are requesting the Linux kernel to auto suspend: Linux kernel requires a device to
 * cancel all its pending URBs when it is going to be suspended, and not generate new URBs.
 */
static void
dbus_usbos_sleep(usbos_info_t *usbos_info)
{
	ASSERT(usbos_info);

	g_probe_info.suspend_state = USBOS_SUSPEND_STATE_SUSPEND_PENDING;
	dbus_usbos_state_change(usbos_info, DBUS_STATE_SLEEP);
	DBUSTRACE(("%s: unlink all URBs\n", __FUNCTION__));
	if (usbos_info->intr_urb)
		USB_UNLINK_URB(usbos_info->intr_urb);
	if (usbos_info->ctl_urb)
		USB_UNLINK_URB(usbos_info->ctl_urb);
	if (usbos_info->blk_urb)
		USB_UNLINK_URB(usbos_info->blk_urb);

	dbus_usbos_unlink(&usbos_info->req_txpostedq, &usbos_info->txposted_lock);
	if (usbos_info->txposted > 0) {
		DBUSERR(("%s ERROR: tx REQs posted=%d in stop!\n", __FUNCTION__,
			usbos_info->txposted));
	}
	dbus_usbos_unlink(&usbos_info->req_rxpostedq, &usbos_info->rxposted_lock);
	if (usbos_info->rxposted > 0) {
		DBUSERR(("%s ERROR: rx REQs posted=%d in stop!\n", __FUNCTION__,
			usbos_info->rxposted));
	}

	/* Make sure all the urb are completed, usb_unlink_urb doesn't guarantee
	* that. Wait for 9000us since max irq interval for EHCI is 8ms.
	*/
	SPINWAIT(usbos_info->txposted != 0 || usbos_info->rxposted != 0, 9000);
	if (!(usbos_info->txposted == 0 && usbos_info->rxposted == 0)) {
		printf("fail to  cancel irbs in 9000us\n");
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	atomic_set(&g_probe_info.intf->pm_usage_cnt, 0);
#else
	g_probe_info.intf->pm_usage_cnt = 0;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33))
	USB_AUTOPM_SET_INTERFACE(g_probe_info.intf);
#else
	USB_ENABLE_AUTOSUSPEND(g_probe_info.usb);
#endif
}
#endif /* ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)) && defined(CONFIG_USB_SUSPEND)) */

#ifdef USBOS_THREAD
/**
 * Increase system performance by creating a USB thread that runs parallel to other system
 * activity.
 */
static void*
dbus_usbos_thread_init(usbos_info_t *usbos_info)
{
	usbos_list_entry_t  *entry;
	unsigned long       flags, ii;

	spin_lock_init(&usbos_info->usbos_list_lock);
	INIT_LIST_HEAD(&usbos_info->usbos_list);
	INIT_LIST_HEAD(&usbos_info->usbos_free_list);
	init_waitqueue_head(&usbos_info->usbos_queue_head);
	atomic_set(&usbos_info->usbos_list_cnt, 0);


	for (ii = 0; ii < (usbos_info->pub->nrxq + usbos_info->pub->ntxq); ii++) {
		entry = MALLOC(usbos_info->pub->osh, sizeof(usbos_list_entry_t));
		if (entry) {
			spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);
			list_add_tail((struct list_head*) entry, &usbos_info->usbos_free_list);
			spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);
		} else {
			DBUSERR(("Failed to create list\n"));
		}
	}

	usbos_info->usbos_kt = kthread_create(dbus_usbos_thread_func,
		usbos_info, "usb-thread");

	if (IS_ERR(usbos_info->usbos_kt)) {
		DBUSERR(("Thread Creation failed\n"));
		return (NULL);
	}

	wake_up_process(usbos_info->usbos_kt);

	return (usbos_info->usbos_kt);
}


static void
dbus_usbos_thread_deinit(usbos_info_t *usbos_info)
{
	struct list_head    *cur, *next;
	usbos_list_entry_t  *entry;
	unsigned long       flags;

	if (usbos_info->usbos_kt) {
		wake_up_interruptible(&usbos_info->usbos_queue_head);
		kthread_stop(usbos_info->usbos_kt);
	}

	list_for_each_safe(cur, next, &usbos_info->usbos_list)
	{
		entry = list_entry(cur, struct usbos_list_entry, list);
		/* detach this entry from the list and then free the entry */
		spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);
		list_del(cur);
		MFREE(usbos_info->pub->osh, entry, sizeof(usbos_list_entry_t));
		spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);
	}

	list_for_each_safe(cur, next, &usbos_info->usbos_free_list)
	{
		entry = list_entry(cur, struct usbos_list_entry, list);
		/* detach this entry from the list and then free the entry */
		spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);
		list_del(cur);
		MFREE(usbos_info->pub->osh, entry, sizeof(usbos_list_entry_t));
		spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);
	}
}

/** Process completed URBs in a worker thread */
static int
dbus_usbos_thread_func(void *data)
{
	usbos_info_t        *usbos_info = (usbos_info_t *)data;
	usbos_list_entry_t  *entry;
	struct list_head    *cur, *next;
	unsigned long       flags;

#ifdef WL_THREADNICE
	set_user_nice(current, WL_THREADNICE);
#endif

	while (1) {
		/* If the list is empty, then go to sleep */
		wait_event_interruptible_timeout
		(usbos_info->usbos_queue_head,
			atomic_read(&usbos_info->usbos_list_cnt) > 0,
			100);

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);

		/* For each entry on the list, process it.  Remove the entry from
		* the list when done.
		*/
		list_for_each_safe(cur, next, &usbos_info->usbos_list)
		{
			urb_req_t           *req;
			int                 len;
			int                 stat;
			usbos_info_t        *usbos_info;

			entry = list_entry(cur, struct usbos_list_entry, list);
			if (entry == NULL)
				break;

			req = entry->urb_context;
			len = entry->urb_length;
			stat = entry->urb_status;
			usbos_info = req->usbinfo;

			/* detach this entry from the list and attach it to the free list */
			list_del_init(cur);
			spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);

			dbus_usbos_recv_complete_handle(req, len, stat);

			spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);

			list_add_tail(cur, &usbos_info->usbos_free_list);

			atomic_dec(&usbos_info->usbos_list_cnt);
		}

		spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);

	}

	return 0;
}

/* Called on Linux calling URB callback, see dbus_usbos_recv_complete() */
static void
dbus_usbos_dispatch_schedule(CALLBACK_ARGS)
{
	urb_req_t           *req = urb->context;
	usbos_info_t        *usbos_info = req->usbinfo;
	usbos_list_entry_t  *entry;
	unsigned long       flags;
	struct list_head    *cur;

	spin_lock_irqsave(&usbos_info->usbos_list_lock, flags);

	cur   = usbos_info->usbos_free_list.next;
	entry = list_entry(cur, struct usbos_list_entry, list);

	/* detach this entry from the free list and prepare it insert it to use list */
	list_del_init(cur);

	if (entry) {
		entry->urb_context = urb->context;
		entry->urb_length  = urb->actual_length;
		entry->urb_status  = urb->status;

		atomic_inc(&usbos_info->usbos_list_cnt);
		list_add_tail(cur, &usbos_info->usbos_list);
	}
	else {
		DBUSERR(("!!!!!!OUT OF MEMORY!!!!!!!\n"));
	}

	spin_unlock_irqrestore(&usbos_info->usbos_list_lock, flags);

	/* thread */
	wake_up_interruptible(&usbos_info->usbos_queue_head);

}

#endif /* USBOS_THREAD */

#ifdef USB_TRIGGER_DEBUG
static bool
dbus_usbos_ctl_send_debugtrig(usbos_info_t* usbinfo)
{
	bootrom_id_t id;

	if (usbinfo == NULL)
		return FALSE;

	id.chip = 0xDEAD;

	dbus_usbos_dl_cmd(usbinfo, DL_DBGTRIG, &id, sizeof(bootrom_id_t));

	/* ignore the result for now */
	return TRUE;
}
#endif /* USB_TRIGGER_DEBUG */


#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
/** New optimized code for USB AP */
void inline optimize_ehci_qtd_init(struct ehci_qtd *qtd, dma_addr_t dma)
{
	memset(qtd, 0, sizeof(*qtd));
	wmb();
	qtd->qtd_self = dma;
	qtd->qtd_status = cpu_to_le32(EHCI_QTD_HALTED);
	qtd->qtd_next = EHCI_NULL;
	qtd->qtd_altnext = EHCI_NULL;
	qtd->obj_next = NULL;
	qtd->rpc = NULL;
	/* qtd->buff = NULL; */
	qtd->xacterrs = EHCI_QTD_XACTERR_MAX;
	wmb();
}

struct ehci_qtd *optimize_ehci_qtd_alloc(gfp_t flags)
{
	struct ehci_qtd		*qtd;
	dma_addr_t		dma;

	usbos_info_t *usbos_info = g_probe_info.usbos_info;

	struct dma_pool *pool = usbos_info->qtd_pool;

	qtd = dma_pool_alloc(pool, flags, &dma);
	if (qtd != NULL) {
		optimize_ehci_qtd_init(qtd, dma);
	}
	return qtd;
}

void optimize_ehci_qtd_free(struct ehci_qtd *qtd)
{
	usbos_info_t *usbos_info = g_probe_info.usbos_info;
	struct dma_pool *pool = usbos_info->qtd_pool;
	dma_pool_free(pool, qtd, qtd->qtd_self);
}

/*
 * Loosely follows qtd_copy_status
 * Greatly simplified as there are only three options: normal, short read, and disaster
 */
static int BCMFASTPATH get_qtd_status(struct ehci_qtd *qtd, int token, int *actual_length)
{
	int	status = -EINPROGRESS;

	*actual_length += qtd->length - EHCI_QTD_GET_BYTES(token);

	/* Short read is not an error */
	if (unlikely (SHORT_READ_Q (token)))
		status = -EREMOTEIO;

	/* Check for serious problems */
	if (token & EHCI_QTD_HALTED) {
		status = -EPROTO;
		if (token & (EHCI_QTD_BABBLE | EHCI_QTD_MISSEDMICRO | EHCI_QTD_BUFERR |
			EHCI_QTD_XACTERR))
			printk("EHCI Fastpath: Serious USB issue qtd %p token %08x --> status %d\n",
				qtd, token, status);
	}

	return status;

}

static void dump_qtd(struct ehci_qtd *qtd)
{
	printk("qtd_next %08x qtd_altnext %08x qtd_status %08x\n", qtd->qtd_next,
		qtd->qtd_altnext, qtd->qtd_status);
}

static void dump_qh(struct ehci_qh *qh)
{
	struct ehci_qtd *qtd = (struct ehci_qtd *)(qh->qh_curqtd | 0xa0000000);
	printk("EHCI Fastpath: QH %p Dump\n", qh);
	printk("qtd_next %08x info1 %08x info2 %08x current %08x\n", qh->qh_link, qh->qh_endp,
		qh->qh_endphub, qh->qh_curqtd);
	printk("overlay\n");
	dump_qtd((struct ehci_qtd *)&qh->ow_next);
	while ((((int)qtd)&EHCI_NULL) == 0)
	{
		printk("QTD %p\n", qtd);
		dump_qtd((struct ehci_qtd *)qtd);
		qtd = (struct ehci_qtd *)(qtd->qtd_next | 0xa0000000);
	}
}


/**
 * This code assumes the caller holding a lock
 * It is currently called from scan_async that should have the lock
 * Lock shall be dropped around the actual completion, then reacquired
 * This is a clean implementation of the qh_completions()
 */
static void BCMFASTPATH ehci_bypass_callback(int pipeindex, struct ehci_qh *qh, spinlock_t *lock)
{
							/* Loop variables */
	struct ehci_qtd		*qtd, 			/* current QTD */
				*end = qh->dummy, 	/* "afterend" */
				*next;
	int			stopped;

	usbos_info_t *usbos_info = g_probe_info.usbos_info;

	/* printk("EHCI Fastpath: callback pipe %d QH %p lock %p\n", pipeindex, qh, lock); */

	/*
	 * This code should not require any interlocking with QTD additions
	 * The additions never touch QH, we should never touch 'end'
	 * Note that QTD additions will keep 'end' in place
	 */
	for (qtd = qh->first_qtd; qtd != end; qtd = next)
	{
		u32		status;		/* Status bits from QTD */

		/* Get the status bits from the QTD */
		rmb();
		status = hc32_to_cpu(qtd->qtd_status);

		if ((status & EHCI_QTD_ACTIVE) == 0) {
			if (unlikely((status & EHCI_QTD_HALTED) != 0)) {
				/* Retry transaction errors until we
				 * reach the software xacterr limit
				 */
				if ((status & EHCI_QTD_XACTERR) &&
					EHCI_QTD_GET_CERR(status) == 0 &&
					--qtd->xacterrs > 0) {
					/* Reset the token in the qtd and the
					 * qh overlay (which still contains
					 * the qtd) so that we pick up from
					 * where we left off
					 */
					printk("EHCI Fastpath: detected XactErr "
						"qtd %p len %d/%d retry %d\n",
						qtd, qtd->length - EHCI_QTD_GET_BYTES(status),
						qtd->length,
						EHCI_QTD_XACTERR_MAX - qtd->xacterrs);

					status &= ~EHCI_QTD_HALTED;
					status |= EHCI_QTD_ACTIVE | EHCI_QTD_SET_CERR(3);
					qtd->qtd_status = cpu_to_le32(status);
					wmb();
					qh->ow_status = cpu_to_le32(status);

					break;
				}

				/* QTD processing was aborted - highly unlikely (never seen, so not
				 * tested). In very new 2.6, we can retry. In 2.4 and older 2.6,
				 * life sucks (the USB stack does the same)
				 */
				printk("EHCI Fastpath: QTD halted\n");
				dump_qh(qh);
				stopped = 1;
			}
		} else
			/* Inactive QTD is an afterend, finished the list */
			break;

		/* Remove the QTD from software QH. This should be done before dropping the lock
		 * in for upper layer
		 */
		next = qtd->obj_next;
		qh->first_qtd = next;

		/* Upper layer processing. */
		if (EHCI_QTD_GET_PID(status) == 0)  /* OUT pipe */
		{
			if (qtd->rpc == NULL)
			{
				usbos_info->waitdone = TRUE;
				wake_up_interruptible(&usbos_info->wait);
				usbos_info->sync_urb_status = 0;
			} else {



				/* usb_info_t *usb_info = (usb_info_t *) handle; */
				usb_info_t *usb_info = (usb_info_t *) usbos_info->cbarg;
				/* if(usb_info && usb_info->cbs && usb_info->cbs->send_irb_complete)
				 * usb_info->cbs->send_irb_complete(usb_info->cbarg, txirb, status);
				 */

				dbus_info_t *dbus_info = (dbus_info_t *)usb_info->cbarg;

				/* Free the coalesce buffer, if multi-buffer packet only. Do not
				 * rely on buff, as it might not even exist
				 */
				if (PKTNEXT(dbus_info->pub->osh, qtd->rpc)) {
					/* printk("k-Freeing %p\n", qtd->buff); */
					kfree(qtd->buff);
				}

				if (dbus_info->cbs && dbus_info->cbs->send_complete)
				{
					atomic_dec(&s_tx_pending);
					spin_unlock(lock);
					/* printk("Sending to RPC qtd %p\n", qtd); */
#if !(defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC))
	#error Configuration not supported; read dbus_if_send_irb_complete for guidelines
#endif
					dbus_info->cbs->send_complete(dbus_info->cbarg, qtd->rpc,
						0);
					if ((atomic_read(&s_tx_pending) < 16) &&
						dbus_info->txoff && !dbus_info->txoverride) {
						dbus_flowctrl_tx(dbus_info, OFF);
					}
					spin_lock(lock);

					/* Things could have happened while the lock was gone,
					 * resync to the hardware
					 */
					next = qh->first_qtd;
					end = qh->dummy;
				}
			}

			optimize_ehci_qtd_free(qtd);
		}
		else	   /* IN pipe */
		{
			/* Simulates the upstream travel */
			usb_info_t *usb_info = (usb_info_t *) usbos_info->cbarg;
			dbus_info_t *dbus_info = (dbus_info_t *)usb_info->cbarg;
			/* unsigned long       flags; */
			int actual_length = 0;

			/* All our reads must be short */
			if (!SHORT_READ_Q (status)) ASSERT(0);

			/* Done with hardware, convert status to error codes */
			status = get_qtd_status(qtd, status, &actual_length);

			switch (status) {
			/* success */
			case 0:
			case -EINPROGRESS:
			case -EREMOTEIO:
				status = 0;
				break;

			case -ECONNRESET:		/* canceled */
			case -ENOENT:
			case -EPROTO:
				DBUSERR(("%s: ehci unlink. status %x\n", __FUNCTION__, status));
				break;
			}

			if (g_probe_info.dereged) {
				printk("%s: DBUS deregistering, ignoring recv callback\n",
					__FUNCTION__);
				return;
			}

			dma_unmap_single(
				usbos_info->usb->bus->controller,
				(dma_addr_t)qtd->qtd_buffer_hi[0],
				actual_length,
				DMA_FROM_DEVICE);


			if (dbus_info->pub.busstate != DBUS_STATE_DOWN) {
				if (status == 0) {
					void *buf = qtd->rpc;

					ASSERT(buf != NULL);
					spin_unlock(lock);
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
					/* Note that these ifdefs are indirectly coming from
					 * dbus_usbos_recv_urb_submit The code itself is from
					 * dbus_if_recv_irb_complete that makes the decision
					 * at runtime, yet it is only pkt or buf depending on
					 * the NOCOPY setup, never both :-)
					 */
					if (dbus_info->cbs && dbus_info->cbs->recv_pkt)
						dbus_info->cbs->recv_pkt(dbus_info->cbarg, buf);
#else
					if (actual_length > 0) {
						if (dbus_info->cbs && dbus_info->cbs->recv_buf)
							dbus_info->cbs->recv_buf(dbus_info->cbarg,
							buf, actual_length);
					}
#endif
					spin_lock(lock);

					/* Things could have happened while the lock was gone,
					 * resync to the hardware
					 */
					next = qh->first_qtd;
					end = qh->dummy;

					/* Reinitialize this qtd since it will be reused. */
					optimize_ehci_qtd_init(qtd, qtd->qtd_self);

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
					/* Note that these ifdefs are coming from
					 * dbus_usbos_recv_urb_submit. In the NOCOPY configuration,
					 * force an allocation of a new packet
					 */
					optimize_submit_rx_request(&dbus_info->pub, 1, qtd, NULL);

#else
					/* In the copy mode, simply reuse the buffer; upper level
					 * had already consumed the data
					 */
					optimize_submit_rx_request(&dbus_info->pub, 1, qtd, buf);
#endif
					/* Not to free this qtd because it will be reused. */
					continue;
				}
			} else {
				printk("%s: DBUS down, ignoring recv callback\n", __FUNCTION__);
			}
			optimize_ehci_qtd_free(qtd);
		}
	}
}

static void optimize_urb_callback(struct urb *urb)
{
	struct usb_ctrlrequest *req = urb->context;

	kfree(req);
	USB_FREE_URB(urb);
}

/* Shall be called under an external lock (currently RPC_TP_LOCK) */
static int optimize_submit_urb(struct usb_device *usb, void *ptr, int request)
{
	struct usb_ctrlrequest *req;
	struct urb *urb;

	if ((urb = USB_ALLOC_URB()) == NULL) {
		printk("EHCI Fastpath: Error allocating URB in optimize_EP!");
		return -ENOMEM;
	}

	if ((req = kmalloc(sizeof(struct usb_ctrlrequest), GFP_ATOMIC)) == NULL) {
		printk("EHCI Fastpath: Failed to allocate memory for control request in"
			" optimize_EP!");
		USB_FREE_URB(urb);
		return -ENOMEM;
	}

	req->bRequestType = (USB_TYPE_VENDOR | USB_RECIP_OTHER);
	req->bRequest = request;

	/* Use this instead of a buffer */
	req->wValue = ((int)ptr & 0xffff);
	req->wIndex = ((((int)ptr)>>16) & 0xffff);
	req->wLength = 0;

	printk("EHCI Fastpath: usb_dev %p\n", usb);
	printk("EHCI Fastpath: bus %p\n", usb->bus);
	printk("EHCI Fastpath: Hub %p\n", usb->bus->root_hub);

	usb_fill_control_urb(
		urb,
		usb->bus->root_hub,
		usb_sndctrlpipe(usb->bus->root_hub, 0),
		(void *)req,
		NULL,
		0,
		optimize_urb_callback,
		req);

	USB_SUBMIT_URB(urb);

	if (urb->status != 0) {
		printk("EHCI Fastpath: Cannot submit URB in optimize_EP: %d\n", urb->status);
	}

	return urb->status;
}

static int epnum(int pipe)
{
	int epn = usb_pipeendpoint(pipe);
	if (usb_pipein (pipe))
		epn |= 0x10;
	return epn;
}


static int optimize_init(usbos_info_t *usbos_info, struct usb_device *usb, int out, int in, int in2)
{
	int retval = -EPIPE;

	atomic_set(&s_tx_pending, 0);
	/* atomic_set(&s_rx_pending, 0); */

	usbos_info->tx_ep = epnum(out);
	usbos_info->rx_ep = epnum(in);
	usbos_info->rx2_ep = epnum(in2);
	usbos_info->usb_device = usb;

	/* printk("EHCI Fastpath: Create pool %p %p %p\n", usb, usb->bus, usb->bus->controller); */

	/* QTDs for bulk transfers - separate pool */
	usbos_info->qtd_pool = dma_pool_create("usbnet_qtd",
		usb->bus->controller,
		sizeof(struct ehci_qtd),
		EHCI_QTD_ALIGN /* byte alignment (for hw parts) */,
		4096 /* can't cross 4K */);
	if (!usbos_info->qtd_pool) {
		printk("EHCI Fastpath: Cannot create the QTD pool\n");
		goto fail;
	}

	/* detaching the EP */
	if (optimize_submit_urb(usb, usb, EHCI_SET_BYPASS_DEV) != 0)
		goto fail;
	optimize_submit_urb(usb, ehci_bypass_callback, EHCI_SET_BYPASS_CB);
	optimize_submit_urb(usb, usbos_info->qtd_pool, EHCI_SET_BYPASS_POOL);
#ifdef EHCI_FASTPATH_TX
	optimize_submit_urb(usb, (void*)((0<<16)|usbos_info->tx_ep), EHCI_FASTPATH);
#endif
#ifdef EHCI_FASTPATH_RX
	optimize_submit_urb(usb, (void*)((1<<16)|usbos_info->rx_ep),   EHCI_FASTPATH);
#endif

	/* getting the QH */
	printk("EHCI Fastpath: EP in %d EP in2 %d EP out %d\n", usbos_info->rx_ep,
		usbos_info->rx2_ep, usbos_info->tx_ep);

	return 0;

fail:
	return retval;
}

static int optimize_deinit_qtds(struct ehci_qh *qh, int coalesce_buf)
{
	usbos_info_t *usbos_info = g_probe_info.usbos_info;
	struct ehci_qtd *qtd, *end, *next;
	unsigned long	flags;

	if (qh == NULL)
		return 0;

	end = qh->dummy;

	printk("%s %d. qh = %p\n", __func__, __LINE__, qh);

	spin_lock_irqsave(&usbos_info->fastpath_lock, flags);
	for (qtd = qh->first_qtd; qtd != end; qtd = next) {
		next = qtd->obj_next;
		qh->first_qtd = next;

		/* Free the coalesce buffer, if multi-buffer packet only. Do not
		 * rely on buff, as it might not even exist
		 */
		if (coalesce_buf && PKTNEXT(dbus_info->pub->osh, qtd->rpc)) {
			printk("k-Freeing %p, ", qtd->buff);
			kfree(qtd->buff);
		}
		printk("freeing qtd %p\n", qtd);

		optimize_ehci_qtd_free(qtd);
	}
	spin_unlock_irqrestore(&usbos_info->fastpath_lock, flags);

	return 0;
}


static BCMFASTPATH struct ehci_qh *get_ep(usbos_info_t *usbos_info, int ep)
{
#ifdef KERNEL26
	struct usb_host_endpoint *epp = NULL;
	switch (ep)
	{
	case 0: epp = usbos_info->usb_device->ep_out[usbos_info->tx_ep&0xf]; break;
	case 1: epp = usbos_info->usb_device->ep_in[usbos_info->rx_ep&0xf]; break;
	case 2: epp = usbos_info->usb_device->ep_in[usbos_info->rx2_ep&0xf]; break;
	default: ASSERT(0);
	}
	if (epp != NULL)
		return (struct ehci_qh *)epp->hcpriv;
	else return NULL;
#else
	switch (ep)
	{
	case 0: return (struct ehci_qh *)(((struct hcd_dev*)(usbos_info->
		usb_device->hcpriv))->ep[usbos_info->tx_ep]);
	case 1: return (struct ehci_qh *)(((struct hcd_dev*)(usbos_info->
		usb_device->hcpriv))->ep[usbos_info->rx_ep]);
	case 2: return (struct ehci_qh *)(((struct hcd_dev*)(usbos_info->
		usb_device->hcpriv))->ep[usbos_info->rx2_ep]);
	default: ASSERT(0);
	}
	return NULL;
#endif /* KERNEL26 */
}

static int optimize_deinit(usbos_info_t *usbos_info, struct usb_device *usb)
{
	optimize_deinit_qtds(get_ep(usbos_info, 0), 1);
	optimize_deinit_qtds(get_ep(usbos_info, 1), 0);
#if defined(EHCI_FASTPATH_TX) || defined(EHCI_FASTPATH_RX)
	optimize_submit_urb(usb, (void *)0, EHCI_CLR_EP_BYPASS);
#endif
	dma_pool_destroy(usbos_info->qtd_pool);
	return 0;
}

/** Reassemble the segmented packet */
static int BCMFASTPATH optimize_gather(const dbus_pub_t *pub, void *pkt, void **buf)
{
	int len = 0;

	void *transfer_buf = kmalloc(pkttotlen(pub->osh, pkt),
		GFP_ATOMIC);
	*buf = transfer_buf;

	if (!transfer_buf) {
		printk("fail to alloc to usb buffer\n");
		return 0;
	}

	while (pkt) {
		int pktlen = PKTLEN(pub->osh, pkt);
		bcopy(PKTDATA(pub->osh, pkt), transfer_buf, pktlen);
		transfer_buf += pktlen;
		len += pktlen;
		pkt = PKTNEXT(usbos_info->pub->osh, pkt);
	}

	/* printk("Coalesced a %d-byte buffer\n", len); */

	return len;
}

int BCMFASTPATH optimize_qtd_fill_with_rpc(const dbus_pub_t *pub, int epn,
	struct ehci_qtd *qtd, void *rpc, int token, int len)
{
	void *data = NULL;

	if (len == 0)
		return optimize_qtd_fill_with_data(pub, epn, qtd, data, token, len);

	ASSERT(rpc != NULL);
	data = PKTDATA(pub->osh, rpc);
	qtd->rpc = rpc;

	if (PKTNEXT(pub->osh, rpc)) {
		len = optimize_gather(pub, rpc, &data);
		qtd->buff = data;
	}

	return optimize_qtd_fill_with_data(pub, epn, qtd, data, token, len);
}

/** Fill the QTD from the data buffer */
int BCMFASTPATH optimize_qtd_fill_with_data(const dbus_pub_t *pub, int epn,
	struct ehci_qtd *qtd, void *data, int token, int len)
{
	int		i, bytes_fit, page_offset;
	dma_addr_t	addr = 0;

	/* struct usb_host_endpoint *ep = get_ep(epn); */
	usbos_info_t *usbos_info = g_probe_info.usbos_info;

	token |= (EHCI_QTD_ACTIVE | EHCI_QTD_IOC); /* Allow execution, force interrupt */

	if (len > 0) {
		addr = dma_map_single(
			usbos_info->usb->bus->controller,
			data,
			len,
			EHCI_QTD_GET_PID(token) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}

	qtd->qtd_buffer[0] = cpu_to_hc32((u32)addr);
	/* Here qtd->qtd_buffer_hi[0] is leveraged to store addr value, which
	 * is needed when invoking dma_unmap_single() in ehci_bypass_callback().
	 * This is valid for EHCI 32bit only.
	 */
	qtd->qtd_buffer_hi[0] = cpu_to_hc32((u32)addr);
	page_offset = (addr & (EHCI_PAGE_SIZE-1));
	bytes_fit = EHCI_PAGE_SIZE - page_offset;
	addr -= page_offset;
	if (len < bytes_fit)
		bytes_fit = len;
	else {
		addr +=  EHCI_PAGE_SIZE;

		for (i = 1; bytes_fit < len && i < EHCI_QTD_NBUFFERS; i++) {
			qtd->qtd_buffer[i] = cpu_to_hc32((u32)addr);
			qtd->qtd_buffer_hi[i] = 0;
			addr += EHCI_PAGE_SIZE;
			if ((bytes_fit + EHCI_PAGE_SIZE) < len)
				bytes_fit += EHCI_PAGE_SIZE;
			else
				bytes_fit = len;
		}

		if (bytes_fit != len)
		{
			ASSERT(0);
		}
	}
	qtd->qtd_status = cpu_to_hc32((bytes_fit << 16) | token);
	qtd->length = bytes_fit;

	return bytes_fit;
}


/** Reimplementation of qh_append_tds()
 * Returns nonzero if too many requests pending
 */
int BCMFASTPATH optimize_submit_async(struct ehci_qtd *qtd, int epn)
{
	/* Clean implementation along the lines of qh_append_tds() */

	struct ehci_qtd		*afterend; /* Element at the end of the QTD chain (after the
					    * last useful one, "after-end")
					    */
	dma_addr_t		hw_addr;
	__hc32			status;
	unsigned long	flags;

	usbos_info_t *usbos_info = g_probe_info.usbos_info;
	struct ehci_qh *qh = get_ep(usbos_info, epn);
	usb_info_t *usb_info = (usb_info_t *) usbos_info->cbarg;
	dbus_info_t *dbus_info = (dbus_info_t *)usb_info->cbarg;

	/* printk("Submit qtd %p to pipe %d (%p)\n", qtd, epn, qh); */
	if (qh == NULL)
	{
		printk("EHCI Fastpath: Attempt of optimized submit to a non-optimized pipe\n");
		return -1;
	}

	spin_lock_irqsave(&usbos_info->fastpath_lock, flags);

	/* Limit outstanding - for rpc behavior only */
	/* printk("QH qtd_status %08x\n", qh->hw->qtd_status); */

	if ((qtd->qtd_status & (1<<8)) == 0)
	{
		atomic_inc(&s_tx_pending);
		if (atomic_read(&s_tx_pending) > 16*2) /* (dbus_info->tx_low_watermark * 3)) */
			dbus_flowctrl_tx(dbus_info, TRUE);
	}

	ASSERT(qh != NULL);


	/*
	 * Standard list processing trick:
	 *   * old "afterend" is filled with the incoming data while still HALTed
	 *   * new element is appended and prepared to serve as new afterend
	 *   * now old afterend is activated
	 * This way, HW never races the SW - no semaphores are necessary, as long as this function
	 * is not reentered for the same QH
	 */

	/* Make new QTD to be HALTed, wait for it to actually happen */
	status = qtd->qtd_status;
	qtd->qtd_status = cpu_to_le32(EHCI_QTD_HALTED);
	wmb();

	/* Now copy all information from the new QTD to the old afterend,
	 * except the own HW address
	 */
	afterend = qh->dummy;
	hw_addr = afterend->qtd_self;
	*afterend = *qtd;
	afterend->qtd_self = hw_addr;

	/* The new QTD is ready to serve as a new afterend, append it */
	qh->dummy = qtd;
	afterend->qtd_next = qtd->qtd_self;
	afterend->qtd_altnext = qtd->qtd_self;  /* Always assume short read. Harmless in our case */
	afterend->obj_next = qtd;

	/* Wait for writes to happen and enable the old afterend (now containing the QTD data) */
	wmb();
	afterend->qtd_status = status;
	wmb();

	spin_unlock_irqrestore(&usbos_info->fastpath_lock, flags);

	return 0;
}

void BCMFASTPATH optimize_submit_rx_request(const dbus_pub_t *pub, int epn, struct ehci_qtd *qtd_in,
                                            void *buf)
{
	usbos_info_t *usbos_info = g_probe_info.usbos_info;
	int len = usbos_info->rxbuf_len;
	void *pkt;
	struct ehci_qtd *qtd;
	int token = EHCI_QTD_SET_CERR(3) | EHCI_QTD_SET_PID(1);

	if (qtd_in == NULL) {
		qtd = optimize_ehci_qtd_alloc(GFP_KERNEL);
		if (!qtd) {
			printk("EHCI Fastpath: Out of QTDs\n");
			return;
		}
	}
	else
		qtd = qtd_in;

	if (buf == NULL)
	{
		/* NOCOPY, allocate own packet */
		/* Follow dbus_usbos_recv_urb_submit */
		pkt = PKTGET(usbos_info->pub->osh, len, FALSE);
		if (pkt == NULL) {
			printk("%s: PKTGET failed\n", __FUNCTION__);
			optimize_ehci_qtd_free(qtd);
			return;
		}
		/* consider the packet "native" so we don't count it as MALLOCED in the osl */
		PKTTONATIVE(usbos_info->pub->osh, pkt);
		qtd->rpc = pkt;
		buf = PKTDATA(usbos_info->pub->osh, pkt);

	}
	else
		qtd->rpc = buf;

	optimize_qtd_fill_with_data(pub, epn, qtd, buf, token, len);
	optimize_submit_async(qtd, epn);
}
#endif /* EHCI_FASTPATH_TX || EHCI_FASTPATH_RX */

#ifdef BCM_REQUEST_FW
void *
dbus_get_fw_nvfile(int devid, uint8 **fw, int *fwlen, int type, uint16 boardtype,
	uint16 boardrev)
{
	s8 fw_name[64];
	s8 file_name[64];
	const struct firmware *firmware = NULL;
	int err;

	strcpy(fw_name, "brcm/bcm");
	if (type == DBUS_FIRMWARE) {
		switch (devid) {
		case BCM4350_CHIP_ID:
			strcat(fw_name, "4350");
			break;
		case BCM43143_CHIP_ID:
			strcat(fw_name, "43143");
			break;
		case BCM43234_CHIP_ID:
		case BCM43235_CHIP_ID:
		case BCM43236_CHIP_ID:
			strcat(fw_name, "43236");
			break;
		case BCM43242_CHIP_ID:
			strcat(fw_name, "43242");
			break;
		case BCM43526_CHIP_ID:
			strcat(fw_name, "43526");
			break;
		case BCM4360_CHIP_ID:
			strcat(fw_name, "4360");
			break;
		default:
			DBUSERR(("unsupported device %x\n", devid));
			return NULL;
		}
		/* load firmware */
		strcat(fw_name, "-firmware.bin");
		err = request_firmware(&firmware, fw_name, &g_probe_info.usb->dev);
		if (err) {
			DBUSERR(("fail to request firmware %s\n", fw_name));
			return NULL;
		}
	} else {
		switch (devid) {
		case BCM4350_CHIP_ID:
			strcat(fw_name, "4350");
			break;
		case BCM43143_CHIP_ID:
			strcat(fw_name, "43143");
			break;
		case BCM43234_CHIP_ID:
			strcat(fw_name, "43234");
			break;
		case BCM43235_CHIP_ID:
			strcat(fw_name, "43235");
			break;
		case BCM43236_CHIP_ID:
			strcat(fw_name, "43236");
			break;
		case BCM43242_CHIP_ID:
			strcat(fw_name, "43242");
			break;
		case BCM43526_CHIP_ID:
			strcat(fw_name, "43526");
			break;
		case BCM4360_CHIP_ID:
			strcat(fw_name, "4360");
			break;
		default:
			DBUSERR(("unsupported device %x\n", devid));
			return NULL;
		}
		/* load board specific nvram file */
		snprintf(file_name, sizeof(file_name), "%s-%2x-%2x.nvm", fw_name,
			boardtype, boardrev);
		err = request_firmware(&firmware, file_name, &g_probe_info.usb->dev);
		if (err) {
			/* board specific file missing - load the generic nvram file */
			snprintf(file_name, sizeof(file_name), "%s.nvm", fw_name);
			err = request_firmware(&firmware, file_name, &g_probe_info.usb->dev);
			if (err) {
				return NULL;
			}
		}
	}

	*fwlen = firmware->size;
	*fw = (uint8 *)firmware->data;
	return (void *)firmware;
}

void
dbus_release_fw_nvfile(void *firmware)
{
	release_firmware((struct firmware *)firmware);
}
#endif /* #ifdef BCM_REQUEST_FW */

/** For a composite device the interface order is not guaranteed,
 * scan the device struct for the WLAN interface
 */
#ifdef BCMUSBDEV_COMPOSITE
static int
dbus_usbos_intf_wlan(struct usb_device *usb)
{
	int i, num_of_eps, ep, intf_wlan = -1;
	int num_intf = CONFIGDESC(usb)->bNumInterfaces;
	struct usb_endpoint_descriptor *endpoint;

	for (i = 0; i < num_intf; i++) {
		if (IFDESC(usb, i).bInterfaceClass != USB_CLASS_VENDOR_SPEC)
			continue;
		num_of_eps = IFDESC(usb, i).bNumEndpoints;

		for (ep = 0; ep < num_of_eps; ep++) {
			endpoint = &IFEPDESC(usb, i, ep);
			if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
				USB_ENDPOINT_XFER_BULK) {
				intf_wlan = i;
				break;
			}
		}
		if (ep < num_of_eps)
			break;
	}

	return intf_wlan;
}
#endif /* BCMUSBDEV_COMPOSITE */
