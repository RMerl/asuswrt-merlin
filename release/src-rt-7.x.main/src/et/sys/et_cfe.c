/*
 * CFE polled-mode device driver for
 * Broadcom BCM47XX 10/100 Mbps Ethernet Controller
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: et_cfe.c 380255 2013-01-22 03:15:28Z $
 */

#include <et_cfg.h>
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_console.h"
#include "cfe_timer.h"
#include "cfe_error.h"
#include "ui_command.h"

#include <typedefs.h>
#include <osl.h>
#include <epivers.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <bcmdevs.h>
#include <bcmenetmib.h>
#include <bcmenetrxh.h>
#include <bcmutils.h>
#include <et_dbg.h>
#include <hndsoc.h>
#include <bcmgmacrxh.h>
#include <etc.h>

typedef struct et_info {
	etc_info_t *etc;		/* pointer to common os-independent data */
	cfe_devctx_t *ctx;		/* backpoint to device */
	int64_t timer;			/* one second watchdog timer */
	osl_t *osh;
	struct et_info *next;		/* pointer to next et_info_t in chain */
} et_info_t;

static et_info_t *et_list = NULL;

void et_init(et_info_t *et, uint options);
void et_reset(et_info_t *et);
void et_link_up(et_info_t *et);
void et_link_down(et_info_t *et);
int et_up(et_info_t *et);
int et_down(et_info_t *et, int reset);
void et_dump(et_info_t *et, struct bcmstrbuf *b);
void et_addcmd(void);
void et_intrson(et_info_t *et);

void
et_init(et_info_t *et, uint options)
{
	ET_TRACE(("et%d: et_init\n", et->etc->unit));

	etc_reset(et->etc);
	etc_init(et->etc, options);
}

void
et_reset(et_info_t *et)
{
	ET_TRACE(("et%d: et_reset\n", et->etc->unit));

	etc_reset(et->etc);
}

void
et_link_up(et_info_t *et)
{
	ET_ERROR(("et%d: link up (%d%s)\n",
		et->etc->unit, et->etc->speed, (et->etc->duplex? "FD" : "HD")));
}

void
et_link_down(et_info_t *et)
{
	ET_ERROR(("et%d: link down\n", et->etc->unit));
}

int
et_up(et_info_t *et)
{
	if (et->etc->up)
		return 0;

	ET_TRACE(("et%d: et_up\n", et->etc->unit));

	etc_up(et->etc);

	/* schedule one second watchdog timer */
	TIMER_SET(et->timer, CFE_HZ / 2);

	return 0;
}

int
et_down(et_info_t *et, int reset)
{
	ET_TRACE(("et%d: et_down\n", et->etc->unit));

	/* stop watchdog timer */
	TIMER_CLEAR(et->timer);

	etc_down(et->etc, reset);

	return 0;
}

void
et_dump(et_info_t *et, struct bcmstrbuf *b)
{
#ifdef BCMDBG
	etc_dump(et->etc, b);
#endif
}

et_info_t *et_phyfind(et_info_t *et, uint coreunit);
uint16 et_phyrd(et_info_t *et, uint phyaddr, uint reg);
void et_phywr(et_info_t *et, uint reg, uint phyaddr, uint16 val);

/*
 * 47XX-specific shared mdc/mdio contortion:
 * Find the et associated with the same chip as <et>
 * and coreunit matching <coreunit>.
 */
et_info_t *
et_phyfind(et_info_t *et, uint coreunit)
{
	et_info_t *tmp;

	/* walk the list et's */
	for (tmp = et_list; tmp; tmp = tmp->next) {
		if (et->etc == NULL)
			continue;
		if (tmp->etc->coreunit != coreunit)
			continue;
		break;
	}
	return (tmp);
}

/* shared phy read entry point */
uint16
et_phyrd(et_info_t *et, uint phyaddr, uint reg)
{
	return et->etc->chops->phyrd(et->etc->ch, phyaddr, reg);
}

/* shared phy write entry point */
void
et_phywr(et_info_t *et, uint phyaddr, uint reg, uint16 val)
{
	et->etc->chops->phywr(et->etc->ch, phyaddr, reg, val);
}

/*  *********************************************************************
    *  ETHER_PROBE(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Probe and install driver.
    *  Create a context structure and attach to the
    *  specified network device.
    *  
    *  Input parameters: 
    *  	   drv - driver descriptor
    *  	   probe_a - device ID
    *  	   probe_b - unit number
    *  	   probe_ptr - mapped registers
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void
et_probe(cfe_driver_t *drv,
	 unsigned long probe_a, unsigned long probe_b, 
	 void *probe_ptr)
{
	et_info_t *et;
	uint16 device;
	uint unit;
	char name[128];

	device = (uint16) probe_a;
	unit = (uint) probe_b;

	if (!(et = (et_info_t *) KMALLOC(sizeof(et_info_t), 0))) {
		ET_ERROR(("et%d: KMALLOC failed\n", unit));
		return;
	}
	bzero(et, sizeof(et_info_t));

	et->osh = osl_attach(et);
	ASSERT(et->osh);

#ifdef	CFG_SIM
	/* Make it chatty in simulation */
	et_msg_level = 0xf;
#endif

	/* common load-time initialization */
	if ((et->etc = etc_attach(et, VENDOR_BROADCOM, device, unit, et->osh, probe_ptr)) == NULL) {
		ET_ERROR(("et%d: etc_attach failed\n", unit));
		KFREE(et);
		return;
	}

	/* this is a polling driver - the chip intmask stays zero */
	et->etc->chops->intrsoff(et->etc->ch);

	/* add us to the global linked list */
	et->next = et_list;
	et_list = et;

	/* print hello string */
	et->etc->chops->longname(et->etc->ch, name, sizeof (name));
	printf("et%d: %s %s\n", unit, name, EPI_VERSION_STR);

	cfe_attach(drv, et, NULL, name);
}

/*  *********************************************************************
    *  ETHER_OPEN(ctx)
    *  
    *  Open the Ethernet device.  The MAC is reset, initialized, and
    *  prepared to receive and send packets.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int
et_open(cfe_devctx_t *ctx)
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;

	ET_TRACE(("et%d: et_open\n", et->etc->unit));

	return et_up(et);
}

/*  *********************************************************************
    *  ETHER_READ(ctx,buffer)
    *  
    *  Read a packet from the Ethernet device.  If no packets are
    *  available, the read will succeed but return 0 bytes.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int
et_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;
	int events;
	void *p;
	bcmenetrxh_t *rxh;
	uint16 flags;
	char eabuf[32];

	ET_TRACE(("et%d: et_read\n", et->etc->unit));

	/* assume no packets */
	buffer->buf_retlen = 0;

	/* poll for packet */
	events = et->etc->chops->getintrevents(et->etc->ch, FALSE);
	if (events & INTR_ERROR)
		return CFE_ERR_IOERR;
	if ((events & INTR_RX) == 0)
		return 0;

	/* get packet */
	if (!(p = et->etc->chops->rx(et->etc->ch)))
		goto done;

	/* packet buffer starts with rxhdr */
	rxh = (bcmenetrxh_t *) PKTDATA(NULL, p);

	/* strip off rxhdr */
	PKTPULL(NULL, p, HWRXOFF);

	/* check for reported frame errors */
	flags = RXH_FLAGS(et->etc, rxh);
	if (flags) {
		bcm_ether_ntoa((struct ether_addr *)
	                       (((struct ether_header *) PKTDATA(NULL, p))->ether_shost), eabuf);
		if (RXH_OVERSIZE(et->etc, rxh)) {
			ET_ERROR(("et%d: rx: over size packet from %s\n", et->etc->unit, eabuf));
		}
		if (RXH_CRC(et->etc, rxh)) {
			ET_ERROR(("et%d: rx: crc error from %s\n", et->etc->unit, eabuf));
		}
		if (RXH_OVF(et->etc, rxh)) {
			ET_ERROR(("et%d: rx: fifo overflow\n", et->etc->unit));
		}
		if (RXH_NO(et->etc, rxh)) {
			ET_ERROR(("et%d: rx: crc error (odd nibbles) from %s\n",
			          et->etc->unit, eabuf));
		}
		if (RXH_RXER(et->etc, rxh)) {
			ET_ERROR(("et%d: rx: symbol error from %s\n", et->etc->unit, eabuf));
		}
	} else {
		bcopy(PKTDATA(NULL, p), buffer->buf_ptr, PKTLEN(NULL, p));
		buffer->buf_retlen = PKTLEN(NULL, p);
		ET_PRHDR("rx", (struct ether_header *) buffer->buf_ptr,
		         buffer->buf_retlen, et->etc->unit);
		ET_PRPKT("rxpkt", buffer->buf_ptr, buffer->buf_retlen, et->etc->unit);
	}

	/* free packet */
	PKTFREE(et->osh, p, FALSE);

done:
	/* post more rx bufs */
	et->etc->chops->rxfill(et->etc->ch);

	return 0;
}

/*  *********************************************************************
    *  ETHER_INPSTAT(ctx,inpstat)
    *  
    *  Check for received packets on the Ethernet device
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      inpstat - pointer to input status structure
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int
et_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;
	int events;

	events = et->etc->chops->getintrevents(et->etc->ch, FALSE);
	inpstat->inp_status = ((events & INTR_RX) ? 1 : 0);

	return 0;
}

/*  *********************************************************************
    *  ETHER_WRITE(ctx,buffer)
    *  
    *  Write a packet to the Ethernet device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int
et_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
	et_info_t *et = ctx->dev_softc;
	int events;
	void *p;

	if (!(p = PKTGET(NULL, buffer->buf_length, TRUE))) {
		ET_ERROR(("et%d: PKTGET failed\n", et->etc->unit));
		return CFE_ERR_NOMEM;
	}

	bcopy(buffer->buf_ptr, PKTDATA(NULL, p), buffer->buf_length);

	ET_PRHDR("tx", (struct ether_header *)PKTDATA(NULL, p), PKTLEN(NULL, p), et->etc->unit);
	ET_PRPKT("txpkt", PKTDATA(NULL, p), PKTLEN(NULL, p), et->etc->unit);

	ASSERT(*et->etc->txavail[TX_Q0] > 0);

	/* transmit the frame */
	et->etc->chops->tx(et->etc->ch, p);

	while (((events = et->etc->chops->getintrevents(et->etc->ch, FALSE)) & (INTR_ERROR | INTR_TX)) == 0);

	/* reclaim any completed tx frames */
	et->etc->chops->txreclaim(et->etc->ch, FALSE);

	return (events & INTR_ERROR) ? CFE_ERR_IOERR : CFE_OK;
}

/*  *********************************************************************
    *  ETHER_IOCTL(ctx,buffer)
    *  
    *  Do device-specific I/O control operations for the device
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      buffer - pointer to buffer descriptor.  
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */
static int
et_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;
	int val;

	ET_TRACE(("et%d: et_ioctl: cmd 0x%x\n", et->etc->unit, buffer->buf_ioctlcmd));

	switch (buffer->buf_ioctlcmd) {

	case IOCTL_ETHER_GETHWADDR:
		bcopy(&et->etc->cur_etheraddr, buffer->buf_ptr, ETHER_ADDR_LEN);
		break;

	case IOCTL_ETHER_SETHWADDR:
		bcopy(buffer->buf_ptr, &et->etc->cur_etheraddr, ETHER_ADDR_LEN);
		et_init(et, ET_INIT_DEF_OPTIONS);
		break;

	case IOCTL_ETHER_GETSPEED:
		val = ETHER_SPEED_UNKNOWN;
		if (et->etc->linkstate) {
			if (et->etc->speed == 10)
				val = et->etc->duplex ? ETHER_SPEED_10FDX : ETHER_SPEED_10HDX;
			else if (et->etc->speed == 100)
				val = et->etc->duplex ? ETHER_SPEED_100FDX : ETHER_SPEED_100HDX;
			else if (et->etc->speed == 1000)
				val = ETHER_SPEED_1000FDX;
		}
		*((int *) buffer->buf_ptr) = val;
		break;

	case IOCTL_ETHER_SETSPEED:
		val = *((int *) buffer->buf_ptr);
		if (val == ETHER_SPEED_AUTO)
			val = ET_AUTO;
		else if (val == ETHER_SPEED_10HDX)
			val = ET_10HALF;
		else if (val == ETHER_SPEED_10FDX)
			val = ET_10FULL;
		else if (val == ETHER_SPEED_100HDX)
			val = ET_100HALF;
		else if (val == ETHER_SPEED_100FDX)
			val = ET_100FULL;
		else if (val == ETHER_SPEED_1000FDX)
			val = ET_1000FULL;
		else
			return CFE_ERR_UNSUPPORTED;
		return etc_ioctl(et->etc, ETCSPEED, &val);

	case IOCTL_ETHER_GETLINK:
		*((int *) buffer->buf_ptr) = (int) et->etc->linkstate;
		break;

	case IOCTL_ETHER_GETLOOPBACK:
		*((int *) buffer->buf_ptr) = et->etc->loopbk;
		break;

	case IOCTL_ETHER_SETLOOPBACK:
		val = *((int *) buffer->buf_ptr);
		if (val == ETHER_LOOPBACK_OFF)
			val = (int) FALSE;
		else
			val = (int) TRUE;
		return etc_ioctl(et->etc, ETCLOOP, &val);

	default:
		return CFE_ERR_UNSUPPORTED;

	}

	return 0;
}

/*  *********************************************************************
    *  ETHER_CLOSE(ctx)
    *  
    *  Close the Ethernet device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *  	   
    *  Return value:
    *  	   status, 0 = ok
    ********************************************************************* */

static int
et_close(cfe_devctx_t *ctx)
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;

	ET_TRACE(("et%d: et_close\n", et->etc->unit));

	return et_down(et, 1);
}

void
et_intrson(et_info_t *et)
{
	/* this is a polling driver - the chip intmask stays zero */
	ET_TRACE(("et%d: et_intrson\n", et->etc->unit));
}

/*  *********************************************************************
    *  ETHER_POLL(ctx,ticks)
    *  
    *  Check for changes in the PHY, so we can track speed changes.
    *  
    *  Input parameters: 
    *  	   ctx - device context (includes ptr to our softc)
    *      ticks- current time in ticks
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void
et_poll(cfe_devctx_t *ctx, int64_t ticks)
{
	et_info_t *et = (et_info_t *) ctx->dev_softc;

	if (TIMER_RUNNING(et->timer) &&
	    TIMER_EXPIRED(et->timer)) {
		etc_watchdog(et->etc);
		TIMER_SET(et->timer, CFE_HZ / 2);
	}
}

const static cfe_devdisp_t et_dispatch = {
	et_open,
	et_read,
	et_inpstat,
	et_write,
	et_ioctl,
	et_close,
	et_poll,
	NULL
};

const cfe_driver_t bcmet = {
	"Broadcom Ethernet",
	"eth",
	CFE_DEV_NETWORK,
	&et_dispatch,
	et_probe
};

static int
ui_cmd_et(ui_cmdline_t *cmdline, int argc, char *argv[])
{
	char *command, *name;
	et_info_t *et;
	cfe_device_t *dev;
	int cmd, val, ret;
	char *arg = (char *) &val;

	if (!(command = cmd_getarg(cmdline, 0)))
		return CFE_ERR_INV_PARAM;

	if (!cmd_sw_value(cmdline, "-i", &name) || !name)
		name = "eth0";
	if (!(dev = cfe_finddev(name)) ||
	    !dev->dev_softc)
		return CFE_ERR_DEVNOTFOUND;
	for (et = et_list; et; et = et->next)
		if (et == dev->dev_softc)
			break;
	if (!et && !(et = et_list))
		return CFE_ERR_DEVNOTFOUND;

	if (!strcmp(command, "up"))
		cmd = ETCUP;
	else if (!strcmp(command, "down"))
		cmd = ETCDOWN;
	else if (!strcmp(command, "loop")) {
		cmd = ETCLOOP;
		arg = cmd_getarg(cmdline, 1);
	}
	else if (!strcmp(command, "dump")) {
		char *p;
		if (!(arg = KMALLOC(IOCBUFSZ, 0)))
			return CFE_ERR_NOMEM;
		bzero(arg, IOCBUFSZ);
		if ((ret = etc_ioctl(et->etc, ETCDUMP, arg))) {
			KFREE(arg);
			return ret;
		}
		/* No puts in cfe, and printf only has a 512 byte buffer */
		p = arg;
		while (*p)
			printf("%c", *p++);
		KFREE(arg);
		return 0;
	}
	else if (!strcmp(command, "msglevel")) {
		cmd = ETCSETMSGLEVEL;
		arg = cmd_getarg(cmdline, 1);
	}
	else if (!strcmp(command, "promisc")) {
		cmd = ETCPROMISC;
		arg = cmd_getarg(cmdline, 1);
	}
	else
		return CFE_ERR_INV_PARAM;

	if (!arg)
		return CFE_ERR_INV_PARAM;
	else if (arg != (char *) &val) {
		val = bcm_strtoul(arg, NULL, 0);
		arg = (char *) &val;
	}

	return etc_ioctl(et->etc, cmd, arg);
}

void
et_addcmd(void)
{
	cmd_addcmd("et",
		   ui_cmd_et,
		   NULL,
		   "Broadcom Ethernet utility.",
		   "et command [args..]\n\n"
		   "Configures the specified Broadcom Ethernet interface.",
		   "-i=*;Specifies the interface|"
		   "up;Activate the specified interface|"
		   "down;Deactivate the specified interface|"
		   "loop;Sets the loopback mode (0,1)|"
		   "dump;Dump driver information|"
		   "msglevel;Sets the driver message level|"
		   "promisc;Sets promiscuous mode|");
}
