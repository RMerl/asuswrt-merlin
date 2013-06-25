/*
 *	IrNET protocol module : Synchronous PPP over an IrDA socket.
 *
 *		Jean II - HPL `00 - <jt@hpl.hp.com>
 *
 * This file contains definitions and declarations global to the IrNET module,
 * all grouped in one place...
 * This file is a *private* header, so other modules don't want to know
 * what's in there...
 *
 * Note : as most part of the Linux kernel, this module is available
 * under the GNU General Public License (GPL).
 */

#ifndef IRNET_H
#define IRNET_H

/************************** DOCUMENTATION ***************************/

/***************************** INCLUDES *****************************/

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/tty.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/capability.h>
#include <linux/ctype.h>	/* isspace() */
#include <linux/string.h>	/* skip_spaces() */
#include <asm/uaccess.h>
#include <linux/init.h>

#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/ppp_channel.h>

#include <net/irda/irda.h>
#include <net/irda/iriap.h>
#include <net/irda/irias_object.h>
#include <net/irda/irlmp.h>
#include <net/irda/irttp.h>
#include <net/irda/discovery.h>

/***************************** OPTIONS *****************************/
/*
 * Define or undefine to compile or not some optional part of the
 * IrNET driver...
 * Note : the present defaults make sense, play with that at your
 * own risk...
 */
/* IrDA side of the business... */
#define DISCOVERY_NOMASK	/* To enable W2k compatibility... */
#define ADVERTISE_HINT		/* Advertise IrLAN hint bit */
#define ALLOW_SIMULT_CONNECT	/* This seem to work, cross fingers... */
#define DISCOVERY_EVENTS	/* Query the discovery log to post events */
#define INITIAL_DISCOVERY	/* Dump current discovery log as events */
#undef STREAM_COMPAT		/* Not needed - potentially messy */
#undef CONNECT_INDIC_KICK	/* Might mess IrDA, not needed */
#undef FAIL_SEND_DISCONNECT	/* Might mess IrDA, not needed */
#undef PASS_CONNECT_PACKETS	/* Not needed ? Safe */
#undef MISSING_PPP_API		/* Stuff I wish I could do */

/* PPP side of the business */
#define BLOCK_WHEN_CONNECT	/* Block packets when connecting */
#define CONNECT_IN_SEND		/* Retry IrDA connection procedure */
#undef FLUSH_TO_PPP		/* Not sure about this one, let's play safe */
#undef SECURE_DEVIRNET		/* Bah... */

/****************************** DEBUG ******************************/

/*
 * This set of flags enable and disable all the various warning,
 * error and debug message of this driver.
 * Each section can be enabled and disabled independently
 */
/* In the PPP part */
#define DEBUG_CTRL_TRACE	0	/* Control channel */
#define DEBUG_CTRL_INFO		0	/* various info */
#define DEBUG_CTRL_ERROR	1	/* problems */
#define DEBUG_FS_TRACE		0	/* filesystem callbacks */
#define DEBUG_FS_INFO		0	/* various info */
#define DEBUG_FS_ERROR		1	/* problems */
#define DEBUG_PPP_TRACE		0	/* PPP related functions */
#define DEBUG_PPP_INFO		0	/* various info */
#define DEBUG_PPP_ERROR		1	/* problems */
#define DEBUG_MODULE_TRACE	0	/* module insertion/removal */
#define DEBUG_MODULE_ERROR	1	/* problems */

/* In the IrDA part */
#define DEBUG_IRDA_SR_TRACE	0	/* IRDA subroutines */
#define DEBUG_IRDA_SR_INFO	0	/* various info */
#define DEBUG_IRDA_SR_ERROR	1	/* problems */
#define DEBUG_IRDA_SOCK_TRACE	0	/* IRDA main socket functions */
#define DEBUG_IRDA_SOCK_INFO	0	/* various info */
#define DEBUG_IRDA_SOCK_ERROR	1	/* problems */
#define DEBUG_IRDA_SERV_TRACE	0	/* The IrNET server */
#define DEBUG_IRDA_SERV_INFO	0	/* various info */
#define DEBUG_IRDA_SERV_ERROR	1	/* problems */
#define DEBUG_IRDA_TCB_TRACE	0	/* IRDA IrTTP callbacks */
#define DEBUG_IRDA_CB_INFO	0	/* various info */
#define DEBUG_IRDA_CB_ERROR	1	/* problems */
#define DEBUG_IRDA_OCB_TRACE	0	/* IRDA other callbacks */
#define DEBUG_IRDA_OCB_INFO	0	/* various info */
#define DEBUG_IRDA_OCB_ERROR	1	/* problems */

#define DEBUG_ASSERT		0	/* Verify all assertions */

/*
 * These are the macros we are using to actually print the debug
 * statements. Don't look at it, it's ugly...
 *
 * One of the trick is that, as the DEBUG_XXX are constant, the
 * compiler will optimise away the if() in all cases.
 */
/* All error messages (will show up in the normal logs) */
#define DERROR(dbg, format, args...) \
	{if(DEBUG_##dbg) \
		printk(KERN_INFO "irnet: %s(): " format, __func__ , ##args);}

/* Normal debug message (will show up in /var/log/debug) */
#define DEBUG(dbg, format, args...) \
	{if(DEBUG_##dbg) \
		printk(KERN_DEBUG "irnet: %s(): " format, __func__ , ##args);}

/* Entering a function (trace) */
#define DENTER(dbg, format, args...) \
	{if(DEBUG_##dbg) \
		printk(KERN_DEBUG "irnet: -> %s" format, __func__ , ##args);}

/* Entering and exiting a function in one go (trace) */
#define DPASS(dbg, format, args...) \
	{if(DEBUG_##dbg) \
		printk(KERN_DEBUG "irnet: <>%s" format, __func__ , ##args);}

/* Exiting a function (trace) */
#define DEXIT(dbg, format, args...) \
	{if(DEBUG_##dbg) \
		printk(KERN_DEBUG "irnet: <-%s()" format, __func__ , ##args);}

/* Exit a function with debug */
#define DRETURN(ret, dbg, args...) \
	{DEXIT(dbg, ": " args);\
	return ret; }

/* Exit a function on failed condition */
#define DABORT(cond, ret, dbg, args...) \
	{if(cond) {\
		DERROR(dbg, args);\
		return ret; }}

/* Invalid assertion, print out an error and exit... */
#define DASSERT(cond, ret, dbg, args...) \
	{if((DEBUG_ASSERT) && !(cond)) {\
		DERROR(dbg, "Invalid assertion: " args);\
		return ret; }}

/************************ CONSTANTS & MACROS ************************/

/* Paranoia */
#define IRNET_MAGIC	0xB00754

/* Number of control events in the control channel buffer... */
#define IRNET_MAX_EVENTS	8	/* Should be more than enough... */

/****************************** TYPES ******************************/

/*
 * This is the main structure where we store all the data pertaining to
 * one instance of irnet.
 * Note : in irnet functions, a pointer this structure is usually called
 * "ap" or "self". If the code is borrowed from the IrDA stack, it tend
 * to be called "self", and if it is borrowed from the PPP driver it is
 * "ap". Apart from that, it's exactly the same structure ;-)
 */
typedef struct irnet_socket
{
  /* ------------------- Instance management ------------------- */
  /* We manage a linked list of IrNET socket instances */
  irda_queue_t		q;		/* Must be first - for hasbin */
  int			magic;		/* Paranoia */

  /* --------------------- FileSystem part --------------------- */
  /* "pppd" interact directly with us on a /dev/ file */
  struct file *		file;		/* File descriptor of this instance */
  /* TTY stuff - to keep "pppd" happy */
  struct ktermios	termios;	/* Various tty flags */
  /* Stuff for the control channel */
  int			event_index;	/* Last read in the event log */

  /* ------------------------- PPP part ------------------------- */
  /* We interface directly to the ppp_generic driver in the kernel */
  int			ppp_open;	/* registered with ppp_generic */
  struct ppp_channel	chan;		/* Interface to generic ppp layer */

  int			mru;		/* Max size of PPP payload */
  u32			xaccm[8];	/* Asynchronous character map (just */
  u32			raccm;		/* to please pppd - dummy) */
  unsigned int		flags;		/* PPP flags (compression, ...) */
  unsigned int		rbits;		/* Unused receive flags ??? */
  struct work_struct disconnect_work;   /* Process context disconnection */
  /* ------------------------ IrTTP part ------------------------ */
  /* We create a pseudo "socket" over the IrDA tranport */
  unsigned long		ttp_open;	/* Set when IrTTP is ready */
  unsigned long		ttp_connect;	/* Set when IrTTP is connecting */
  struct tsap_cb *	tsap;		/* IrTTP instance (the connection) */

  char			rname[NICKNAME_MAX_LEN + 1];
					/* IrDA nickname of destination */
  __u32			rdaddr;		/* Requested peer IrDA address */
  __u32			rsaddr;		/* Requested local IrDA address */
  __u32			daddr;		/* actual peer IrDA address */
  __u32			saddr;		/* my local IrDA address */
  __u8			dtsap_sel;	/* Remote TSAP selector */
  __u8			stsap_sel;	/* Local TSAP selector */

  __u32			max_sdu_size_rx;/* Socket parameters used for IrTTP */
  __u32			max_sdu_size_tx;
  __u32			max_data_size;
  __u8			max_header_size;
  LOCAL_FLOW		tx_flow;	/* State of the Tx path in IrTTP */

  /* ------------------- IrLMP and IrIAS part ------------------- */
  /* Used for IrDA Discovery and socket name resolution */
  void *		ckey;		/* IrLMP client handle */
  __u16			mask;		/* Hint bits mask (filter discov.)*/
  int			nslots;		/* Number of slots for discovery */

  struct iriap_cb *	iriap;		/* Used to query remote IAS */
  int			errno;		/* status of the IAS query */

  /* -------------------- Discovery log part -------------------- */
  /* Used by initial discovery on the control channel
   * and by irnet_discover_daddr_and_lsap_sel() */
  struct irda_device_info *discoveries;	/* Copy of the discovery log */
  int			disco_index;	/* Last read in the discovery log */
  int			disco_number;	/* Size of the discovery log */

} irnet_socket;

/*
 * This is the various event that we will generate on the control channel
 */
typedef enum irnet_event
{
  IRNET_DISCOVER,		/* New IrNET node discovered */
  IRNET_EXPIRE,			/* IrNET node expired */
  IRNET_CONNECT_TO,		/* IrNET socket has connected to other node */
  IRNET_CONNECT_FROM,		/* Other node has connected to IrNET socket */
  IRNET_REQUEST_FROM,		/* Non satisfied connection request */
  IRNET_NOANSWER_FROM,		/* Failed connection request */
  IRNET_BLOCKED_LINK,		/* Link (IrLAP) is blocked for > 3s */
  IRNET_DISCONNECT_FROM,	/* IrNET socket has disconnected */
  IRNET_DISCONNECT_TO		/* Closing IrNET socket */
} irnet_event;

/*
 * This is the storage for an event and its arguments
 */
typedef struct irnet_log
{
  irnet_event	event;
  int		unit;
  __u32		saddr;
  __u32		daddr;
  char		name[NICKNAME_MAX_LEN + 1];	/* 21 + 1 */
  __u16_host_order hints;			/* Discovery hint bits */
} irnet_log;

/*
 * This is the storage for all events and related stuff...
 */
typedef struct irnet_ctrl_channel
{
  irnet_log	log[IRNET_MAX_EVENTS];	/* Event log */
  int		index;		/* Current index in log */
  spinlock_t	spinlock;	/* Serialize access to the event log */
  wait_queue_head_t	rwait;	/* processes blocked on read (or poll) */
} irnet_ctrl_channel;

/**************************** PROTOTYPES ****************************/
/*
 * Global functions of the IrNET module
 * Note : we list here also functions called from one file to the other.
 */

/* -------------------------- IRDA PART -------------------------- */
extern int
	irda_irnet_create(irnet_socket *);	/* Initialise a IrNET socket */
extern int
	irda_irnet_connect(irnet_socket *);	/* Try to connect over IrDA */
extern void
	irda_irnet_destroy(irnet_socket *);	/* Teardown  a IrNET socket */
extern int
	irda_irnet_init(void);		/* Initialise IrDA part of IrNET */
extern void
	irda_irnet_cleanup(void);	/* Teardown IrDA part of IrNET */

/**************************** VARIABLES ****************************/

/* Control channel stuff - allocated in irnet_irda.h */
extern struct irnet_ctrl_channel	irnet_events;

#endif /* IRNET_H */
