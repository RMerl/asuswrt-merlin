/*
 *
 *  Generic Bluetooth SDIO driver
 *
 *  Copyright (C) 2007  Cambridge Silicon Radio Ltd.
 *  Copyright (C) 2007  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>

#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#define VERSION "0.1"

static const struct sdio_device_id btsdio_table[] = {
	/* Generic Bluetooth Type-A SDIO device */
	{ SDIO_DEVICE_CLASS(SDIO_CLASS_BT_A) },

	/* Generic Bluetooth Type-B SDIO device */
	{ SDIO_DEVICE_CLASS(SDIO_CLASS_BT_B) },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(sdio, btsdio_table);

struct btsdio_data {
	struct hci_dev   *hdev;
	struct sdio_func *func;

	struct work_struct work;

	struct sk_buff_head txq;
};

#define REG_RDAT     0x00	/* Receiver Data */
#define REG_TDAT     0x00	/* Transmitter Data */
#define REG_PC_RRT   0x10	/* Read Packet Control */
#define REG_PC_WRT   0x11	/* Write Packet Control */
#define REG_RTC_STAT 0x12	/* Retry Control Status */
#define REG_RTC_SET  0x12	/* Retry Control Set */
#define REG_INTRD    0x13	/* Interrupt Indication */
#define REG_CL_INTRD 0x13	/* Interrupt Clear */
#define REG_EN_INTRD 0x14	/* Interrupt Enable */
#define REG_MD_STAT  0x20	/* Bluetooth Mode Status */

static int btsdio_tx_packet(struct btsdio_data *data, struct sk_buff *skb)
{
	int err;

	BT_DBG("%s", data->hdev->name);

	/* Prepend Type-A header */
	skb_push(skb, 4);
	skb->data[0] = (skb->len & 0x0000ff);
	skb->data[1] = (skb->len & 0x00ff00) >> 8;
	skb->data[2] = (skb->len & 0xff0000) >> 16;
	skb->data[3] = bt_cb(skb)->pkt_type;

	err = sdio_writesb(data->func, REG_TDAT, skb->data, skb->len);
	if (err < 0) {
		skb_pull(skb, 4);
		sdio_writeb(data->func, 0x01, REG_PC_WRT, NULL);
		return err;
	}

	data->hdev->stat.byte_tx += skb->len;

	kfree_skb(skb);

	return 0;
}

static void btsdio_work(struct work_struct *work)
{
	struct btsdio_data *data = container_of(work, struct btsdio_data, work);
	struct sk_buff *skb;
	int err;

	BT_DBG("%s", data->hdev->name);

	sdio_claim_host(data->func);

	while ((skb = skb_dequeue(&data->txq))) {
		err = btsdio_tx_packet(data, skb);
		if (err < 0) {
			data->hdev->stat.err_tx++;
			skb_queue_head(&data->txq, skb);
			break;
		}
	}

	sdio_release_host(data->func);
}

static int btsdio_rx_packet(struct btsdio_data *data)
{
	u8 hdr[4] __attribute__ ((aligned(4)));
	struct sk_buff *skb;
	int err, len;

	BT_DBG("%s", data->hdev->name);

	err = sdio_readsb(data->func, hdr, REG_RDAT, 4);
	if (err < 0)
		return err;

	len = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16);
	if (len < 4 || len > 65543)
		return -EILSEQ;

	skb = bt_skb_alloc(len - 4, GFP_KERNEL);
	if (!skb) {
		/* Out of memory. Prepare a read retry and just
		 * return with the expectation that the next time
		 * we're called we'll have more memory. */
		return -ENOMEM;
	}

	skb_put(skb, len - 4);

	err = sdio_readsb(data->func, skb->data, REG_RDAT, len - 4);
	if (err < 0) {
		kfree_skb(skb);
		return err;
	}

	data->hdev->stat.byte_rx += len;

	skb->dev = (void *) data->hdev;
	bt_cb(skb)->pkt_type = hdr[3];

	err = hci_recv_frame(skb);
	if (err < 0)
		return err;

	sdio_writeb(data->func, 0x00, REG_PC_RRT, NULL);

	return 0;
}

static void btsdio_interrupt(struct sdio_func *func)
{
	struct btsdio_data *data = sdio_get_drvdata(func);
	int intrd;

	BT_DBG("%s", data->hdev->name);

	intrd = sdio_readb(func, REG_INTRD, NULL);
	if (intrd & 0x01) {
		sdio_writeb(func, 0x01, REG_CL_INTRD, NULL);

		if (btsdio_rx_packet(data) < 0) {
			data->hdev->stat.err_rx++;
			sdio_writeb(data->func, 0x01, REG_PC_RRT, NULL);
		}
	}
}

static int btsdio_open(struct hci_dev *hdev)
{
	struct btsdio_data *data = hdev->driver_data;
	int err;

	BT_DBG("%s", hdev->name);

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	sdio_claim_host(data->func);

	err = sdio_enable_func(data->func);
	if (err < 0) {
		clear_bit(HCI_RUNNING, &hdev->flags);
		goto release;
	}

	err = sdio_claim_irq(data->func, btsdio_interrupt);
	if (err < 0) {
		sdio_disable_func(data->func);
		clear_bit(HCI_RUNNING, &hdev->flags);
		goto release;
	}

	if (data->func->class == SDIO_CLASS_BT_B)
		sdio_writeb(data->func, 0x00, REG_MD_STAT, NULL);

	sdio_writeb(data->func, 0x01, REG_EN_INTRD, NULL);

release:
	sdio_release_host(data->func);

	return err;
}

static int btsdio_close(struct hci_dev *hdev)
{
	struct btsdio_data *data = hdev->driver_data;

	BT_DBG("%s", hdev->name);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	sdio_claim_host(data->func);

	sdio_writeb(data->func, 0x00, REG_EN_INTRD, NULL);

	sdio_release_irq(data->func);
	sdio_disable_func(data->func);

	sdio_release_host(data->func);

	return 0;
}

static int btsdio_flush(struct hci_dev *hdev)
{
	struct btsdio_data *data = hdev->driver_data;

	BT_DBG("%s", hdev->name);

	skb_queue_purge(&data->txq);

	return 0;
}

static int btsdio_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;
	struct btsdio_data *data = hdev->driver_data;

	BT_DBG("%s", hdev->name);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:
		hdev->stat.sco_tx++;
		break;

	default:
		return -EILSEQ;
	}

	skb_queue_tail(&data->txq, skb);

	schedule_work(&data->work);

	return 0;
}

static void btsdio_destruct(struct hci_dev *hdev)
{
	struct btsdio_data *data = hdev->driver_data;

	BT_DBG("%s", hdev->name);

	kfree(data);
}

static int btsdio_probe(struct sdio_func *func,
				const struct sdio_device_id *id)
{
	struct btsdio_data *data;
	struct hci_dev *hdev;
	struct sdio_func_tuple *tuple = func->tuples;
	int err;

	BT_DBG("func %p id %p class 0x%04x", func, id, func->class);

	while (tuple) {
		BT_DBG("code 0x%x size %d", tuple->code, tuple->size);
		tuple = tuple->next;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->func = func;

	INIT_WORK(&data->work, btsdio_work);

	skb_queue_head_init(&data->txq);

	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(data);
		return -ENOMEM;
	}

	hdev->bus = HCI_SDIO;
	hdev->driver_data = data;

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &func->dev);

	hdev->open     = btsdio_open;
	hdev->close    = btsdio_close;
	hdev->flush    = btsdio_flush;
	hdev->send     = btsdio_send_frame;
	hdev->destruct = btsdio_destruct;

	hdev->owner = THIS_MODULE;

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		kfree(data);
		return err;
	}

	sdio_set_drvdata(func, data);

	return 0;
}

static void btsdio_remove(struct sdio_func *func)
{
	struct btsdio_data *data = sdio_get_drvdata(func);
	struct hci_dev *hdev;

	BT_DBG("func %p", func);

	if (!data)
		return;

	hdev = data->hdev;

	sdio_set_drvdata(func, NULL);

	hci_unregister_dev(hdev);

	hci_free_dev(hdev);
}

static struct sdio_driver btsdio_driver = {
	.name		= "btsdio",
	.probe		= btsdio_probe,
	.remove		= btsdio_remove,
	.id_table	= btsdio_table,
};

static int __init btsdio_init(void)
{
	BT_INFO("Generic Bluetooth SDIO driver ver %s", VERSION);

	return sdio_register_driver(&btsdio_driver);
}

static void __exit btsdio_exit(void)
{
	sdio_unregister_driver(&btsdio_driver);
}

module_init(btsdio_init);
module_exit(btsdio_exit);

MODULE_AUTHOR("Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("Generic Bluetooth SDIO driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
