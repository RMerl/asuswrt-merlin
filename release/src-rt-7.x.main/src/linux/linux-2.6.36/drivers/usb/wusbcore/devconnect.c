

#include <linux/jiffies.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "wusbhc.h"

static void wusbhc_devconnect_acked_work(struct work_struct *work);

static void wusb_dev_free(struct wusb_dev *wusb_dev)
{
	if (wusb_dev) {
		kfree(wusb_dev->set_gtk_req);
		usb_free_urb(wusb_dev->set_gtk_urb);
		kfree(wusb_dev);
	}
}

static struct wusb_dev *wusb_dev_alloc(struct wusbhc *wusbhc)
{
	struct wusb_dev *wusb_dev;
	struct urb *urb;
	struct usb_ctrlrequest *req;

	wusb_dev = kzalloc(sizeof(*wusb_dev), GFP_KERNEL);
	if (wusb_dev == NULL)
		goto err;

	wusb_dev->wusbhc = wusbhc;

	INIT_WORK(&wusb_dev->devconnect_acked_work, wusbhc_devconnect_acked_work);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (urb == NULL)
		goto err;
	wusb_dev->set_gtk_urb = urb;

	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (req == NULL)
		goto err;
	wusb_dev->set_gtk_req = req;

	req->bRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
	req->bRequest = USB_REQ_SET_DESCRIPTOR;
	req->wValue = cpu_to_le16(USB_DT_KEY << 8 | wusbhc->gtk_index);
	req->wIndex = 0;
	req->wLength = cpu_to_le16(wusbhc->gtk.descr.bLength);

	return wusb_dev;
err:
	wusb_dev_free(wusb_dev);
	return NULL;
}


/*
 * Using the Connect-Ack list, fill out the @wusbhc Connect-Ack WUSB IE
 * properly so that it can be added to the MMC.
 *
 * We just get the @wusbhc->ca_list and fill out the first four ones or
 * less (per-spec WUSB1.0[7.5, before T7-38). If the ConnectAck WUSB
 * IE is not allocated, we alloc it.
 *
 * @wusbhc->mutex must be taken
 */
static void wusbhc_fill_cack_ie(struct wusbhc *wusbhc)
{
	unsigned cnt;
	struct wusb_dev *dev_itr;
	struct wuie_connect_ack *cack_ie;

	cack_ie = &wusbhc->cack_ie;
	cnt = 0;
	list_for_each_entry(dev_itr, &wusbhc->cack_list, cack_node) {
		cack_ie->blk[cnt].CDID = dev_itr->cdid;
		cack_ie->blk[cnt].bDeviceAddress = dev_itr->addr;
		if (++cnt >= WUIE_ELT_MAX)
			break;
	}
	cack_ie->hdr.bLength = sizeof(cack_ie->hdr)
		+ cnt * sizeof(cack_ie->blk[0]);
}

/*
 * Register a new device that wants to connect
 *
 * A new device wants to connect, so we add it to the Connect-Ack
 * list. We give it an address in the unauthorized range (bit 8 set);
 * user space will have to drive authorization further on.
 *
 * @dev_addr: address to use for the device (which is also the port
 *            number).
 *
 * @wusbhc->mutex must be taken
 */
static struct wusb_dev *wusbhc_cack_add(struct wusbhc *wusbhc,
					struct wusb_dn_connect *dnc,
					const char *pr_cdid, u8 port_idx)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	int new_connection = wusb_dn_connect_new_connection(dnc);
	u8 dev_addr;
	int result;

	/* Is it registered already? */
	list_for_each_entry(wusb_dev, &wusbhc->cack_list, cack_node)
		if (!memcmp(&wusb_dev->cdid, &dnc->CDID,
			    sizeof(wusb_dev->cdid)))
			return wusb_dev;
	/* We don't have it, create an entry, register it */
	wusb_dev = wusb_dev_alloc(wusbhc);
	if (wusb_dev == NULL)
		return NULL;
	wusb_dev_init(wusb_dev);
	wusb_dev->cdid = dnc->CDID;
	wusb_dev->port_idx = port_idx;

	/*
	 * Devices are always available within the cluster reservation
	 * and since the hardware will take the intersection of the
	 * per-device availability and the cluster reservation, the
	 * per-device availability can simply be set to always
	 * available.
	 */
	bitmap_fill(wusb_dev->availability.bm, UWB_NUM_MAS);

	if (1 && new_connection == 0)
		new_connection = 1;
	if (new_connection) {
		dev_addr = (port_idx + 2) | WUSB_DEV_ADDR_UNAUTH;

		dev_info(dev, "Connecting new WUSB device to address %u, "
			"port %u\n", dev_addr, port_idx);

		result = wusb_set_dev_addr(wusbhc, wusb_dev, dev_addr);
		if (result < 0)
			return NULL;
	}
	wusb_dev->entry_ts = jiffies;
	list_add_tail(&wusb_dev->cack_node, &wusbhc->cack_list);
	wusbhc->cack_count++;
	wusbhc_fill_cack_ie(wusbhc);

	return wusb_dev;
}

/*
 * Remove a Connect-Ack context entry from the HCs view
 *
 * @wusbhc->mutex must be taken
 */
static void wusbhc_cack_rm(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	list_del_init(&wusb_dev->cack_node);
	wusbhc->cack_count--;
	wusbhc_fill_cack_ie(wusbhc);
}

/*
 * @wusbhc->mutex must be taken */
static
void wusbhc_devconnect_acked(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	wusbhc_cack_rm(wusbhc, wusb_dev);
	if (wusbhc->cack_count)
		wusbhc_mmcie_set(wusbhc, 0, 0, &wusbhc->cack_ie.hdr);
	else
		wusbhc_mmcie_rm(wusbhc, &wusbhc->cack_ie.hdr);
}

static void wusbhc_devconnect_acked_work(struct work_struct *work)
{
	struct wusb_dev *wusb_dev = container_of(work, struct wusb_dev,
						 devconnect_acked_work);
	struct wusbhc *wusbhc = wusb_dev->wusbhc;

	mutex_lock(&wusbhc->mutex);
	wusbhc_devconnect_acked(wusbhc, wusb_dev);
	mutex_unlock(&wusbhc->mutex);

	wusb_dev_put(wusb_dev);
}

static
void wusbhc_devconnect_ack(struct wusbhc *wusbhc, struct wusb_dn_connect *dnc,
			   const char *pr_cdid)
{
	int result;
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	struct wusb_port *port;
	unsigned idx, devnum;

	mutex_lock(&wusbhc->mutex);

	/* Check we are not handling it already */
	for (idx = 0; idx < wusbhc->ports_max; idx++) {
		port = wusb_port_by_idx(wusbhc, idx);
		if (port->wusb_dev
		    && memcmp(&dnc->CDID, &port->wusb_dev->cdid, sizeof(dnc->CDID)) == 0)
			goto error_unlock;
	}
	/* Look up those fake ports we have for a free one */
	for (idx = 0; idx < wusbhc->ports_max; idx++) {
		port = wusb_port_by_idx(wusbhc, idx);
		if ((port->status & USB_PORT_STAT_POWER)
		    && !(port->status & USB_PORT_STAT_CONNECTION))
			break;
	}
	if (idx >= wusbhc->ports_max) {
		dev_err(dev, "Host controller can't connect more devices "
			"(%u already connected); device %s rejected\n",
			wusbhc->ports_max, pr_cdid);
		/* NOTE: we could send a WUIE_Disconnect here, but we haven't
		 *       event acked, so the device will eventually timeout the
		 *       connection, right? */
		goto error_unlock;
	}

	devnum = idx + 2;

	/* Make sure we are using no crypto on that "virtual port" */
	wusbhc->set_ptk(wusbhc, idx, 0, NULL, 0);

	/* Grab a filled in Connect-Ack context, fill out the
	 * Connect-Ack Wireless USB IE, set the MMC */
	wusb_dev = wusbhc_cack_add(wusbhc, dnc, pr_cdid, idx);
	if (wusb_dev == NULL)
		goto error_unlock;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &wusbhc->cack_ie.hdr);
	if (result < 0)
		goto error_unlock;
	/* Give the device at least 2ms (WUSB1.0[7.5.1p3]), let's do
	 * three for a good measure */
	msleep(3);
	port->wusb_dev = wusb_dev;
	port->status |= USB_PORT_STAT_CONNECTION;
	port->change |= USB_PORT_STAT_C_CONNECTION;
	/* Now the port status changed to connected; khubd will
	 * pick the change up and try to reset the port to bring it to
	 * the enabled state--so this process returns up to the stack
	 * and it calls back into wusbhc_rh_port_reset().
	 */
error_unlock:
	mutex_unlock(&wusbhc->mutex);
	return;

}

/*
 * Disconnect a Wireless USB device from its fake port
 *
 * Marks the port as disconnected so that khubd can pick up the change
 * and drops our knowledge about the device.
 *
 * Assumes there is a device connected
 *
 * @port_index: zero based port number
 *
 * NOTE: @wusbhc->mutex is locked
 *
 * WARNING: From here it is not very safe to access anything hanging off
 *	    wusb_dev
 */
static void __wusbhc_dev_disconnect(struct wusbhc *wusbhc,
				    struct wusb_port *port)
{
	struct wusb_dev *wusb_dev = port->wusb_dev;

	port->status &= ~(USB_PORT_STAT_CONNECTION | USB_PORT_STAT_ENABLE
			  | USB_PORT_STAT_SUSPEND | USB_PORT_STAT_RESET
			  | USB_PORT_STAT_LOW_SPEED | USB_PORT_STAT_HIGH_SPEED);
	port->change |= USB_PORT_STAT_C_CONNECTION | USB_PORT_STAT_C_ENABLE;
	if (wusb_dev) {
		dev_dbg(wusbhc->dev, "disconnecting device from port %d\n", wusb_dev->port_idx);
		if (!list_empty(&wusb_dev->cack_node))
			list_del_init(&wusb_dev->cack_node);
		/* For the one in cack_add() */
		wusb_dev_put(wusb_dev);
	}
	port->wusb_dev = NULL;

	/* After a device disconnects, change the GTK (see [WUSB]
	 * section 6.2.11.2). */
	if (wusbhc->active)
		wusbhc_gtk_rekey(wusbhc);

	/* The Wireless USB part has forgotten about the device already; now
	 * khubd's timer will pick up the disconnection and remove the USB
	 * device from the system
	 */
}

static void __wusbhc_keep_alive(struct wusbhc *wusbhc)
{
	struct device *dev = wusbhc->dev;
	unsigned cnt;
	struct wusb_dev *wusb_dev;
	struct wusb_port *wusb_port;
	struct wuie_keep_alive *ie = &wusbhc->keep_alive_ie;
	unsigned keep_alives, old_keep_alives;

	old_keep_alives = ie->hdr.bLength - sizeof(ie->hdr);
	keep_alives = 0;
	for (cnt = 0;
	     keep_alives < WUIE_ELT_MAX && cnt < wusbhc->ports_max;
	     cnt++) {
		unsigned tt = msecs_to_jiffies(wusbhc->trust_timeout);

		wusb_port = wusb_port_by_idx(wusbhc, cnt);
		wusb_dev = wusb_port->wusb_dev;

		if (wusb_dev == NULL)
			continue;
		if (wusb_dev->usb_dev == NULL || !wusb_dev->usb_dev->authenticated)
			continue;

		if (time_after(jiffies, wusb_dev->entry_ts + tt)) {
			dev_err(dev, "KEEPALIVE: device %u timed out\n",
				wusb_dev->addr);
			__wusbhc_dev_disconnect(wusbhc, wusb_port);
		} else if (time_after(jiffies, wusb_dev->entry_ts + tt/2)) {
			/* Approaching timeout cut out, need to refresh */
			ie->bDeviceAddress[keep_alives++] = wusb_dev->addr;
		}
	}
	if (keep_alives & 0x1)	/* pad to even number ([WUSB] section 7.5.9) */
		ie->bDeviceAddress[keep_alives++] = 0x7f;
	ie->hdr.bLength = sizeof(ie->hdr) +
		keep_alives*sizeof(ie->bDeviceAddress[0]);
	if (keep_alives > 0)
		wusbhc_mmcie_set(wusbhc, 10, 5, &ie->hdr);
	else if (old_keep_alives != 0)
		wusbhc_mmcie_rm(wusbhc, &ie->hdr);
}

/*
 * Do a run through all devices checking for timeouts
 */
static void wusbhc_keep_alive_run(struct work_struct *ws)
{
	struct delayed_work *dw = to_delayed_work(ws);
	struct wusbhc *wusbhc =	container_of(dw, struct wusbhc, keep_alive_timer);

	mutex_lock(&wusbhc->mutex);
	__wusbhc_keep_alive(wusbhc);
	mutex_unlock(&wusbhc->mutex);

	queue_delayed_work(wusbd, &wusbhc->keep_alive_timer,
			   msecs_to_jiffies(wusbhc->trust_timeout / 2));
}

/*
 * Find the wusb_dev from its device address.
 *
 * The device can be found directly from the address (see
 * wusb_cack_add() for where the device address is set to port_idx
 * +2), except when the address is zero.
 */
static struct wusb_dev *wusbhc_find_dev_by_addr(struct wusbhc *wusbhc, u8 addr)
{
	int p;

	if (addr == 0xff) /* unconnected */
		return NULL;

	if (addr > 0) {
		int port = (addr & ~0x80) - 2;
		if (port < 0 || port >= wusbhc->ports_max)
			return NULL;
		return wusb_port_by_idx(wusbhc, port)->wusb_dev;
	}

	/* Look for the device with address 0. */
	for (p = 0; p < wusbhc->ports_max; p++) {
		struct wusb_dev *wusb_dev = wusb_port_by_idx(wusbhc, p)->wusb_dev;
		if (wusb_dev && wusb_dev->addr == addr)
			return wusb_dev;
	}
	return NULL;
}

/*
 * Handle a DN_Alive notification (WUSB1.0[7.6.1])
 *
 * This just updates the device activity timestamp and then refreshes
 * the keep alive IE.
 *
 * @wusbhc shall be referenced and unlocked
 */
static void wusbhc_handle_dn_alive(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	mutex_lock(&wusbhc->mutex);
	wusb_dev->entry_ts = jiffies;
	__wusbhc_keep_alive(wusbhc);
	mutex_unlock(&wusbhc->mutex);
}

/*
 * Handle a DN_Connect notification (WUSB1.0[7.6.1])
 *
 * @wusbhc
 * @pkt_hdr
 * @size:    Size of the buffer where the notification resides; if the
 *           notification data suggests there should be more data than
 *           available, an error will be signaled and the whole buffer
 *           consumed.
 *
 * @wusbhc->mutex shall be held
 */
static void wusbhc_handle_dn_connect(struct wusbhc *wusbhc,
				     struct wusb_dn_hdr *dn_hdr,
				     size_t size)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dn_connect *dnc;
	char pr_cdid[WUSB_CKHDID_STRSIZE];
	static const char *beacon_behaviour[] = {
		"reserved",
		"self-beacon",
		"directed-beacon",
		"no-beacon"
	};

	if (size < sizeof(*dnc)) {
		dev_err(dev, "DN CONNECT: short notification (%zu < %zu)\n",
			size, sizeof(*dnc));
		return;
	}

	dnc = container_of(dn_hdr, struct wusb_dn_connect, hdr);
	ckhdid_printf(pr_cdid, sizeof(pr_cdid), &dnc->CDID);
	dev_info(dev, "DN CONNECT: device %s @ %x (%s) wants to %s\n",
		 pr_cdid,
		 wusb_dn_connect_prev_dev_addr(dnc),
		 beacon_behaviour[wusb_dn_connect_beacon_behavior(dnc)],
		 wusb_dn_connect_new_connection(dnc) ? "connect" : "reconnect");
	/* ACK the connect */
	wusbhc_devconnect_ack(wusbhc, dnc, pr_cdid);
}

/*
 * Handle a DN_Disconnect notification (WUSB1.0[7.6.1])
 *
 * Device is going down -- do the disconnect.
 *
 * @wusbhc shall be referenced and unlocked
 */
static void wusbhc_handle_dn_disconnect(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev)
{
	struct device *dev = wusbhc->dev;

	dev_info(dev, "DN DISCONNECT: device 0x%02x going down\n", wusb_dev->addr);

	mutex_lock(&wusbhc->mutex);
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, wusb_dev->port_idx));
	mutex_unlock(&wusbhc->mutex);
}

/*
 * Handle a Device Notification coming a host
 *
 * The Device Notification comes from a host (HWA, DWA or WHCI)
 * wrapped in a set of headers. Somebody else has peeled off those
 * headers for us and we just get one Device Notifications.
 *
 * Invalid DNs (e.g., too short) are discarded.
 *
 * @wusbhc shall be referenced
 *
 * FIXMES:
 *  - implement priorities as in WUSB1.0[Table 7-55]?
 */
void wusbhc_handle_dn(struct wusbhc *wusbhc, u8 srcaddr,
		      struct wusb_dn_hdr *dn_hdr, size_t size)
{
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;

	if (size < sizeof(struct wusb_dn_hdr)) {
		dev_err(dev, "DN data shorter than DN header (%d < %d)\n",
			(int)size, (int)sizeof(struct wusb_dn_hdr));
		return;
	}

	wusb_dev = wusbhc_find_dev_by_addr(wusbhc, srcaddr);
	if (wusb_dev == NULL && dn_hdr->bType != WUSB_DN_CONNECT) {
		dev_dbg(dev, "ignoring DN %d from unconnected device %02x\n",
			dn_hdr->bType, srcaddr);
		return;
	}

	switch (dn_hdr->bType) {
	case WUSB_DN_CONNECT:
		wusbhc_handle_dn_connect(wusbhc, dn_hdr, size);
		break;
	case WUSB_DN_ALIVE:
		wusbhc_handle_dn_alive(wusbhc, wusb_dev);
		break;
	case WUSB_DN_DISCONNECT:
		wusbhc_handle_dn_disconnect(wusbhc, wusb_dev);
		break;
	case WUSB_DN_MASAVAILCHANGED:
	case WUSB_DN_RWAKE:
	case WUSB_DN_SLEEP:
		break;
	case WUSB_DN_EPRDY:
		/* The hardware handles these. */
		break;
	default:
		dev_warn(dev, "unknown DN %u (%d octets) from %u\n",
			 dn_hdr->bType, (int)size, srcaddr);
	}
}
EXPORT_SYMBOL_GPL(wusbhc_handle_dn);

/*
 * Disconnect a WUSB device from a the cluster
 *
 * @wusbhc
 * @port     Fake port where the device is (wusbhc index, not USB port number).
 *
 * In Wireless USB, a disconnect is basically telling the device he is
 * being disconnected and forgetting about him.
 *
 * We send the device a Device Disconnect IE (WUSB1.0[7.5.11]) for 100
 * ms and then keep going.
 *
 * We don't do much in case of error; we always pretend we disabled
 * the port and disconnected the device. If physically the request
 * didn't get there (many things can fail in the way there), the stack
 * will reject the device's communication attempts.
 *
 * @wusbhc should be refcounted and locked
 */
void __wusbhc_dev_disable(struct wusbhc *wusbhc, u8 port_idx)
{
	int result;
	struct device *dev = wusbhc->dev;
	struct wusb_dev *wusb_dev;
	struct wuie_disconnect *ie;

	wusb_dev = wusb_port_by_idx(wusbhc, port_idx)->wusb_dev;
	if (wusb_dev == NULL) {
		/* reset no device? ignore */
		dev_dbg(dev, "DISCONNECT: no device at port %u, ignoring\n",
			port_idx);
		return;
	}
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, port_idx));

	ie = kzalloc(sizeof(*ie), GFP_KERNEL);
	if (ie == NULL)
		return;
	ie->hdr.bLength = sizeof(*ie);
	ie->hdr.bIEIdentifier = WUIE_ID_DEVICE_DISCONNECT;
	ie->bDeviceAddress = wusb_dev->addr;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &ie->hdr);
	if (result < 0)
		dev_err(dev, "DISCONNECT: can't set MMC: %d\n", result);
	else {
		/* At least 6 MMCs, assuming at least 1 MMC per zone. */
		msleep(7*4);
		wusbhc_mmcie_rm(wusbhc, &ie->hdr);
	}
	kfree(ie);
}

/*
 * Walk over the BOS descriptor, verify and grok it
 *
 * @usb_dev: referenced
 * @wusb_dev: referenced and unlocked
 *
 * The BOS descriptor is defined at WUSB1.0[7.4.1], and it defines a
 * "flexible" way to wrap all kinds of descriptors inside an standard
 * descriptor (wonder why they didn't use normal descriptors,
 * btw). Not like they lack code.
 *
 * At the end we go to look for the WUSB Device Capabilities
 * (WUSB1.0[7.4.1.1]) that is wrapped in a device capability descriptor
 * that is part of the BOS descriptor set. That tells us what does the
 * device support (dual role, beacon type, UWB PHY rates).
 */
static int wusb_dev_bos_grok(struct usb_device *usb_dev,
			     struct wusb_dev *wusb_dev,
			     struct usb_bos_descriptor *bos, size_t desc_size)
{
	ssize_t result;
	struct device *dev = &usb_dev->dev;
	void *itr, *top;

	/* Walk over BOS capabilities, verify them */
	itr = (void *)bos + sizeof(*bos);
	top = itr + desc_size - sizeof(*bos);
	while (itr < top) {
		struct usb_dev_cap_header *cap_hdr = itr;
		size_t cap_size;
		u8 cap_type;
		if (top - itr < sizeof(*cap_hdr)) {
			dev_err(dev, "Device BUG? premature end of BOS header "
				"data [offset 0x%02x]: only %zu bytes left\n",
				(int)(itr - (void *)bos), top - itr);
			result = -ENOSPC;
			goto error_bad_cap;
		}
		cap_size = cap_hdr->bLength;
		cap_type = cap_hdr->bDevCapabilityType;
		if (cap_size == 0)
			break;
		if (cap_size > top - itr) {
			dev_err(dev, "Device BUG? premature end of BOS data "
				"[offset 0x%02x cap %02x %zu bytes]: "
				"only %zu bytes left\n",
				(int)(itr - (void *)bos),
				cap_type, cap_size, top - itr);
			result = -EBADF;
			goto error_bad_cap;
		}
		switch (cap_type) {
		case USB_CAP_TYPE_WIRELESS_USB:
			if (cap_size != sizeof(*wusb_dev->wusb_cap_descr))
				dev_err(dev, "Device BUG? WUSB Capability "
					"descriptor is %zu bytes vs %zu "
					"needed\n", cap_size,
					sizeof(*wusb_dev->wusb_cap_descr));
			else
				wusb_dev->wusb_cap_descr = itr;
			break;
		default:
			dev_err(dev, "BUG? Unknown BOS capability 0x%02x "
				"(%zu bytes) at offset 0x%02x\n", cap_type,
				cap_size, (int)(itr - (void *)bos));
		}
		itr += cap_size;
	}
	result = 0;
error_bad_cap:
	return result;
}

/*
 * Add information from the BOS descriptors to the device
 *
 * @usb_dev: referenced
 * @wusb_dev: referenced and unlocked
 *
 * So what we do is we alloc a space for the BOS descriptor of 64
 * bytes; read the first four bytes which include the wTotalLength
 * field (WUSB1.0[T7-26]) and if it fits in those 64 bytes, read the
 * whole thing. If not we realloc to that size.
 *
 * Then we call the groking function, that will fill up
 * wusb_dev->wusb_cap_descr, which is what we'll need later on.
 */
static int wusb_dev_bos_add(struct usb_device *usb_dev,
			    struct wusb_dev *wusb_dev)
{
	ssize_t result;
	struct device *dev = &usb_dev->dev;
	struct usb_bos_descriptor *bos;
	size_t alloc_size = 32, desc_size = 4;

	bos = kmalloc(alloc_size, GFP_KERNEL);
	if (bos == NULL)
		return -ENOMEM;
	result = usb_get_descriptor(usb_dev, USB_DT_BOS, 0, bos, desc_size);
	if (result < 4) {
		dev_err(dev, "Can't get BOS descriptor or too short: %zd\n",
			result);
		goto error_get_descriptor;
	}
	desc_size = le16_to_cpu(bos->wTotalLength);
	if (desc_size >= alloc_size) {
		kfree(bos);
		alloc_size = desc_size;
		bos = kmalloc(alloc_size, GFP_KERNEL);
		if (bos == NULL)
			return -ENOMEM;
	}
	result = usb_get_descriptor(usb_dev, USB_DT_BOS, 0, bos, desc_size);
	if (result < 0 || result != desc_size) {
		dev_err(dev, "Can't get  BOS descriptor or too short (need "
			"%zu bytes): %zd\n", desc_size, result);
		goto error_get_descriptor;
	}
	if (result < sizeof(*bos)
	    || le16_to_cpu(bos->wTotalLength) != desc_size) {
		dev_err(dev, "Can't get  BOS descriptor or too short (need "
			"%zu bytes): %zd\n", desc_size, result);
		goto error_get_descriptor;
	}

	result = wusb_dev_bos_grok(usb_dev, wusb_dev, bos, result);
	if (result < 0)
		goto error_bad_bos;
	wusb_dev->bos = bos;
	return 0;

error_bad_bos:
error_get_descriptor:
	kfree(bos);
	wusb_dev->wusb_cap_descr = NULL;
	return result;
}

static void wusb_dev_bos_rm(struct wusb_dev *wusb_dev)
{
	kfree(wusb_dev->bos);
	wusb_dev->wusb_cap_descr = NULL;
};

static struct usb_wireless_cap_descriptor wusb_cap_descr_default = {
	.bLength = sizeof(wusb_cap_descr_default),
	.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_CAP_TYPE_WIRELESS_USB,

	.bmAttributes = USB_WIRELESS_BEACON_NONE,
	.wPHYRates = cpu_to_le16(USB_WIRELESS_PHY_53),
	.bmTFITXPowerInfo = 0,
	.bmFFITXPowerInfo = 0,
	.bmBandGroup = cpu_to_le16(0x0001),	/* WUSB1.0[7.4.1] bottom */
	.bReserved = 0
};

static void wusb_dev_add_ncb(struct usb_device *usb_dev)
{
	int result = 0;
	struct wusb_dev *wusb_dev;
	struct wusbhc *wusbhc;
	struct device *dev = &usb_dev->dev;
	u8 port_idx;

	if (usb_dev->wusb == 0 || usb_dev->devnum == 1)
		return;		/* skip non wusb and wusb RHs */

	usb_set_device_state(usb_dev, USB_STATE_UNAUTHENTICATED);

	wusbhc = wusbhc_get_by_usb_dev(usb_dev);
	if (wusbhc == NULL)
		goto error_nodev;
	mutex_lock(&wusbhc->mutex);
	wusb_dev = __wusb_dev_get_by_usb_dev(wusbhc, usb_dev);
	port_idx = wusb_port_no_to_idx(usb_dev->portnum);
	mutex_unlock(&wusbhc->mutex);
	if (wusb_dev == NULL)
		goto error_nodev;
	wusb_dev->usb_dev = usb_get_dev(usb_dev);
	usb_dev->wusb_dev = wusb_dev_get(wusb_dev);
	result = wusb_dev_sec_add(wusbhc, usb_dev, wusb_dev);
	if (result < 0) {
		dev_err(dev, "Cannot enable security: %d\n", result);
		goto error_sec_add;
	}
	/* Now query the device for it's BOS and attach it to wusb_dev */
	result = wusb_dev_bos_add(usb_dev, wusb_dev);
	if (result < 0) {
		dev_err(dev, "Cannot get BOS descriptors: %d\n", result);
		goto error_bos_add;
	}
	result = wusb_dev_sysfs_add(wusbhc, usb_dev, wusb_dev);
	if (result < 0)
		goto error_add_sysfs;
out:
	wusb_dev_put(wusb_dev);
	wusbhc_put(wusbhc);
error_nodev:
	return;

	wusb_dev_sysfs_rm(wusb_dev);
error_add_sysfs:
	wusb_dev_bos_rm(wusb_dev);
error_bos_add:
	wusb_dev_sec_rm(wusb_dev);
error_sec_add:
	mutex_lock(&wusbhc->mutex);
	__wusbhc_dev_disconnect(wusbhc, wusb_port_by_idx(wusbhc, port_idx));
	mutex_unlock(&wusbhc->mutex);
	goto out;
}

/*
 * Undo all the steps done at connection by the notifier callback
 *
 * NOTE: @usb_dev locked
 */
static void wusb_dev_rm_ncb(struct usb_device *usb_dev)
{
	struct wusb_dev *wusb_dev = usb_dev->wusb_dev;

	if (usb_dev->wusb == 0 || usb_dev->devnum == 1)
		return;		/* skip non wusb and wusb RHs */

	wusb_dev_sysfs_rm(wusb_dev);
	wusb_dev_bos_rm(wusb_dev);
	wusb_dev_sec_rm(wusb_dev);
	wusb_dev->usb_dev = NULL;
	usb_dev->wusb_dev = NULL;
	wusb_dev_put(wusb_dev);
	usb_put_dev(usb_dev);
}

/*
 * Handle notifications from the USB stack (notifier call back)
 *
 * This is called when the USB stack does a
 * usb_{bus,device}_{add,remove}() so we can do WUSB specific
 * handling. It is called with [for the case of
 * USB_DEVICE_{ADD,REMOVE} with the usb_dev locked.
 */
int wusb_usb_ncb(struct notifier_block *nb, unsigned long val,
		 void *priv)
{
	int result = NOTIFY_OK;

	switch (val) {
	case USB_DEVICE_ADD:
		wusb_dev_add_ncb(priv);
		break;
	case USB_DEVICE_REMOVE:
		wusb_dev_rm_ncb(priv);
		break;
	case USB_BUS_ADD:
		/* ignore (for now) */
	case USB_BUS_REMOVE:
		break;
	default:
		WARN_ON(1);
		result = NOTIFY_BAD;
	};
	return result;
}

/*
 * Return a referenced wusb_dev given a @wusbhc and @usb_dev
 */
struct wusb_dev *__wusb_dev_get_by_usb_dev(struct wusbhc *wusbhc,
					   struct usb_device *usb_dev)
{
	struct wusb_dev *wusb_dev;
	u8 port_idx;

	port_idx = wusb_port_no_to_idx(usb_dev->portnum);
	BUG_ON(port_idx > wusbhc->ports_max);
	wusb_dev = wusb_port_by_idx(wusbhc, port_idx)->wusb_dev;
	if (wusb_dev != NULL)		/* ops, device is gone */
		wusb_dev_get(wusb_dev);
	return wusb_dev;
}
EXPORT_SYMBOL_GPL(__wusb_dev_get_by_usb_dev);

void wusb_dev_destroy(struct kref *_wusb_dev)
{
	struct wusb_dev *wusb_dev = container_of(_wusb_dev, struct wusb_dev, refcnt);

	list_del_init(&wusb_dev->cack_node);
	wusb_dev_free(wusb_dev);
}
EXPORT_SYMBOL_GPL(wusb_dev_destroy);

/*
 * Create all the device connect handling infrastructure
 *
 * This is basically the device info array, Connect Acknowledgement
 * (cack) lists, keep-alive timers (and delayed work thread).
 */
int wusbhc_devconnect_create(struct wusbhc *wusbhc)
{
	wusbhc->keep_alive_ie.hdr.bIEIdentifier = WUIE_ID_KEEP_ALIVE;
	wusbhc->keep_alive_ie.hdr.bLength = sizeof(wusbhc->keep_alive_ie.hdr);
	INIT_DELAYED_WORK(&wusbhc->keep_alive_timer, wusbhc_keep_alive_run);

	wusbhc->cack_ie.hdr.bIEIdentifier = WUIE_ID_CONNECTACK;
	wusbhc->cack_ie.hdr.bLength = sizeof(wusbhc->cack_ie.hdr);
	INIT_LIST_HEAD(&wusbhc->cack_list);

	return 0;
}

/*
 * Release all resources taken by the devconnect stuff
 */
void wusbhc_devconnect_destroy(struct wusbhc *wusbhc)
{
	/* no op */
}

int wusbhc_devconnect_start(struct wusbhc *wusbhc)
{
	struct device *dev = wusbhc->dev;
	struct wuie_host_info *hi;
	int result;

	hi = kzalloc(sizeof(*hi), GFP_KERNEL);
	if (hi == NULL)
		return -ENOMEM;

	hi->hdr.bLength       = sizeof(*hi);
	hi->hdr.bIEIdentifier = WUIE_ID_HOST_INFO;
	hi->attributes        = cpu_to_le16((wusbhc->rsv->stream << 3) | WUIE_HI_CAP_ALL);
	hi->CHID              = wusbhc->chid;
	result = wusbhc_mmcie_set(wusbhc, 0, 0, &hi->hdr);
	if (result < 0) {
		dev_err(dev, "Cannot add Host Info MMCIE: %d\n", result);
		goto error_mmcie_set;
	}
	wusbhc->wuie_host_info = hi;

	queue_delayed_work(wusbd, &wusbhc->keep_alive_timer,
			   (wusbhc->trust_timeout*CONFIG_HZ)/1000/2);

	return 0;

error_mmcie_set:
	kfree(hi);
	return result;
}

/*
 * wusbhc_devconnect_stop - stop managing connected devices
 * @wusbhc: the WUSB HC
 *
 * Disconnects any devices still connected, stops the keep alives and
 * removes the Host Info IE.
 */
void wusbhc_devconnect_stop(struct wusbhc *wusbhc)
{
	int i;

	mutex_lock(&wusbhc->mutex);
	for (i = 0; i < wusbhc->ports_max; i++) {
		if (wusbhc->port[i].wusb_dev)
			__wusbhc_dev_disconnect(wusbhc, &wusbhc->port[i]);
	}
	mutex_unlock(&wusbhc->mutex);

	cancel_delayed_work_sync(&wusbhc->keep_alive_timer);
	wusbhc_mmcie_rm(wusbhc, &wusbhc->wuie_host_info->hdr);
	kfree(wusbhc->wuie_host_info);
	wusbhc->wuie_host_info = NULL;
}

/*
 * wusb_set_dev_addr - set the WUSB device address used by the host
 * @wusbhc: the WUSB HC the device is connect to
 * @wusb_dev: the WUSB device
 * @addr: new device address
 */
int wusb_set_dev_addr(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev, u8 addr)
{
	int result;

	wusb_dev->addr = addr;
	result = wusbhc->dev_info_set(wusbhc, wusb_dev);
	if (result < 0)
		dev_err(wusbhc->dev, "device %d: failed to set device "
			"address\n", wusb_dev->port_idx);
	else
		dev_info(wusbhc->dev, "device %d: %s addr %u\n",
			 wusb_dev->port_idx,
			 (addr & WUSB_DEV_ADDR_UNAUTH) ? "unauth" : "auth",
			 wusb_dev->addr);

	return result;
}
