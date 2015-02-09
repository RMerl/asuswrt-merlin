
#include <linux/usb/wusb.h>
#include <linux/slab.h>
#include "wusbhc.h"

/* Initialize the MMCIEs handling mechanism */
int wusbhc_mmcie_create(struct wusbhc *wusbhc)
{
	u8 mmcies = wusbhc->mmcies_max;
	wusbhc->mmcie = kcalloc(mmcies, sizeof(wusbhc->mmcie[0]), GFP_KERNEL);
	if (wusbhc->mmcie == NULL)
		return -ENOMEM;
	mutex_init(&wusbhc->mmcie_mutex);
	return 0;
}

/* Release resources used by the MMCIEs handling mechanism */
void wusbhc_mmcie_destroy(struct wusbhc *wusbhc)
{
	kfree(wusbhc->mmcie);
}

/*
 * Add or replace an MMC Wireless USB IE.
 *
 * @interval:    See WUSB1.0[8.5.3.1]
 * @repeat_cnt:  See WUSB1.0[8.5.3.1]
 * @handle:      See WUSB1.0[8.5.3.1]
 * @wuie:        Pointer to the header of the WUSB IE data to add.
 *               MUST BE allocated in a kmalloc buffer (no stack or
 *               vmalloc).
 *               THE CALLER ALWAYS OWNS THE POINTER (we don't free it
 *               on remove, we just forget about it).
 * @returns:     0 if ok, < 0 errno code on error.
 *
 * Goes over the *whole* @wusbhc->mmcie array looking for (a) the
 * first free spot and (b) if @wuie is already in the array (aka:
 * transmitted in the MMCs) the spot were it is.
 *
 * If present, we "overwrite it" (update).
 *
 *
 * NOTE: Need special ordering rules -- see below WUSB1.0 Table 7-38.
 *       The host uses the handle as the 'sort' index. We
 *       allocate the last one always for the WUIE_ID_HOST_INFO, and
 *       the rest, first come first serve in inverse order.
 *
 *       Host software must make sure that it adds the other IEs in
 *       the right order... the host hardware is responsible for
 *       placing the WCTA IEs in the right place with the other IEs
 *       set by host software.
 *
 * NOTE: we can access wusbhc->wa_descr without locking because it is
 *       read only.
 */
int wusbhc_mmcie_set(struct wusbhc *wusbhc, u8 interval, u8 repeat_cnt,
		     struct wuie_hdr *wuie)
{
	int result = -ENOBUFS;
	unsigned handle, itr;

	/* Search a handle, taking into account the ordering */
	mutex_lock(&wusbhc->mmcie_mutex);
	switch (wuie->bIEIdentifier) {
	case WUIE_ID_HOST_INFO:
		/* Always last */
		handle = wusbhc->mmcies_max - 1;
		break;
	case WUIE_ID_ISOCH_DISCARD:
		dev_err(wusbhc->dev, "Special ordering case for WUIE ID 0x%x "
			"unimplemented\n", wuie->bIEIdentifier);
		result = -ENOSYS;
		goto error_unlock;
	default:
		/* search for it or find the last empty slot */
		handle = ~0;
		for (itr = 0; itr < wusbhc->mmcies_max - 1; itr++) {
			if (wusbhc->mmcie[itr] == wuie) {
				handle = itr;
				break;
			}
			if (wusbhc->mmcie[itr] == NULL)
				handle = itr;
		}
		if (handle == ~0)
			goto error_unlock;
	}
	result = (wusbhc->mmcie_add)(wusbhc, interval, repeat_cnt, handle,
				     wuie);
	if (result >= 0)
		wusbhc->mmcie[handle] = wuie;
error_unlock:
	mutex_unlock(&wusbhc->mmcie_mutex);
	return result;
}
EXPORT_SYMBOL_GPL(wusbhc_mmcie_set);

/*
 * Remove an MMC IE previously added with wusbhc_mmcie_set()
 *
 * @wuie	Pointer used to add the WUIE
 */
void wusbhc_mmcie_rm(struct wusbhc *wusbhc, struct wuie_hdr *wuie)
{
	int result;
	unsigned handle, itr;

	mutex_lock(&wusbhc->mmcie_mutex);
	for (itr = 0; itr < wusbhc->mmcies_max; itr++) {
		if (wusbhc->mmcie[itr] == wuie) {
			handle = itr;
			goto found;
		}
	}
	mutex_unlock(&wusbhc->mmcie_mutex);
	return;

found:
	result = (wusbhc->mmcie_rm)(wusbhc, handle);
	if (result == 0)
		wusbhc->mmcie[itr] = NULL;
	mutex_unlock(&wusbhc->mmcie_mutex);
}
EXPORT_SYMBOL_GPL(wusbhc_mmcie_rm);

static int wusbhc_mmc_start(struct wusbhc *wusbhc)
{
	int ret;

	mutex_lock(&wusbhc->mutex);
	ret = wusbhc->start(wusbhc);
	if (ret >= 0)
		wusbhc->active = 1;
	mutex_unlock(&wusbhc->mutex);

	return ret;
}

static void wusbhc_mmc_stop(struct wusbhc *wusbhc)
{
	mutex_lock(&wusbhc->mutex);
	wusbhc->active = 0;
	wusbhc->stop(wusbhc, WUSB_CHANNEL_STOP_DELAY_MS);
	mutex_unlock(&wusbhc->mutex);
}

/*
 * wusbhc_start - start transmitting MMCs and accepting connections
 * @wusbhc: the HC to start
 *
 * Establishes a cluster reservation, enables device connections, and
 * starts MMCs with appropriate DNTS parameters.
 */
int wusbhc_start(struct wusbhc *wusbhc)
{
	int result;
	struct device *dev = wusbhc->dev;

	WARN_ON(wusbhc->wuie_host_info != NULL);

	result = wusbhc_rsv_establish(wusbhc);
	if (result < 0) {
		dev_err(dev, "cannot establish cluster reservation: %d\n",
			result);
		goto error_rsv_establish;
	}

	result = wusbhc_devconnect_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error enabling device connections: %d\n", result);
		goto error_devconnect_start;
	}

	result = wusbhc_sec_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error starting security in the HC: %d\n", result);
		goto error_sec_start;
	}
	result = wusbhc->set_num_dnts(wusbhc, 0, 15);
	if (result < 0) {
		dev_err(dev, "Cannot set DNTS parameters: %d\n", result);
		goto error_set_num_dnts;
	}
	result = wusbhc_mmc_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error starting wusbch: %d\n", result);
		goto error_wusbhc_start;
	}

	return 0;

error_wusbhc_start:
	wusbhc_sec_stop(wusbhc);
error_set_num_dnts:
error_sec_start:
	wusbhc_devconnect_stop(wusbhc);
error_devconnect_start:
	wusbhc_rsv_terminate(wusbhc);
error_rsv_establish:
	return result;
}

/*
 * wusbhc_stop - stop transmitting MMCs
 * @wusbhc: the HC to stop
 *
 * Stops the WUSB channel and removes the cluster reservation.
 */
void wusbhc_stop(struct wusbhc *wusbhc)
{
	wusbhc_mmc_stop(wusbhc);
	wusbhc_sec_stop(wusbhc);
	wusbhc_devconnect_stop(wusbhc);
	wusbhc_rsv_terminate(wusbhc);
}

/*
 * Set/reset/update a new CHID
 *
 * Depending on the previous state of the MMCs, start, stop or change
 * the sent MMC. This effectively switches the host controller on and
 * off (radio wise).
 */
int wusbhc_chid_set(struct wusbhc *wusbhc, const struct wusb_ckhdid *chid)
{
	int result = 0;

	if (memcmp(chid, &wusb_ckhdid_zero, sizeof(*chid)) == 0)
		chid = NULL;

	mutex_lock(&wusbhc->mutex);
	if (chid) {
		if (wusbhc->active) {
			mutex_unlock(&wusbhc->mutex);
			return -EBUSY;
		}
		wusbhc->chid = *chid;
	}
	mutex_unlock(&wusbhc->mutex);

	if (chid)
		result = uwb_radio_start(&wusbhc->pal);
	else
		uwb_radio_stop(&wusbhc->pal);
	return result;
}
EXPORT_SYMBOL_GPL(wusbhc_chid_set);
