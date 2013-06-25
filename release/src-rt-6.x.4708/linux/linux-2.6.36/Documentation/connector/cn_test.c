/*
 * 	cn_test.c
 * 
 * 2004+ Copyright (c) Evgeniy Polyakov <zbr@ioremap.net>
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define pr_fmt(fmt) "cn_test: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/timer.h>

#include <linux/connector.h>

static struct cb_id cn_test_id = { CN_NETLINK_USERS + 3, 0x456 };
static char cn_test_name[] = "cn_test";
static struct sock *nls;
static struct timer_list cn_test_timer;

static void cn_test_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp)
{
	pr_info("%s: %lu: idx=%x, val=%x, seq=%u, ack=%u, len=%d: %s.\n",
	        __func__, jiffies, msg->id.idx, msg->id.val,
	        msg->seq, msg->ack, msg->len,
	        msg->len ? (char *)msg->data : "");
}

/*
 * Do not remove this function even if no one is using it as
 * this is an example of how to get notifications about new
 * connector user registration
 */

static u32 cn_test_timer_counter;
static void cn_test_timer_func(unsigned long __data)
{
	struct cn_msg *m;
	char data[32];

	pr_debug("%s: timer fired with data %lu\n", __func__, __data);

	m = kzalloc(sizeof(*m) + sizeof(data), GFP_ATOMIC);
	if (m) {

		memcpy(&m->id, &cn_test_id, sizeof(m->id));
		m->seq = cn_test_timer_counter;
		m->len = sizeof(data);

		m->len =
		    scnprintf(data, sizeof(data), "counter = %u",
			      cn_test_timer_counter) + 1;

		memcpy(m + 1, data, m->len);

		cn_netlink_send(m, 0, GFP_ATOMIC);
		kfree(m);
	}

	cn_test_timer_counter++;

	mod_timer(&cn_test_timer, jiffies + msecs_to_jiffies(1000));
}

static int cn_test_init(void)
{
	int err;

	err = cn_add_callback(&cn_test_id, cn_test_name, cn_test_callback);
	if (err)
		goto err_out;
	cn_test_id.val++;
	err = cn_add_callback(&cn_test_id, cn_test_name, cn_test_callback);
	if (err) {
		cn_del_callback(&cn_test_id);
		goto err_out;
	}

	setup_timer(&cn_test_timer, cn_test_timer_func, 0);
	mod_timer(&cn_test_timer, jiffies + msecs_to_jiffies(1000));

	pr_info("initialized with id={%u.%u}\n",
		cn_test_id.idx, cn_test_id.val);

	return 0;

      err_out:
	if (nls && nls->sk_socket)
		sock_release(nls->sk_socket);

	return err;
}

static void cn_test_fini(void)
{
	del_timer_sync(&cn_test_timer);
	cn_del_callback(&cn_test_id);
	cn_test_id.val--;
	cn_del_callback(&cn_test_id);
	if (nls && nls->sk_socket)
		sock_release(nls->sk_socket);
}

module_init(cn_test_init);
module_exit(cn_test_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evgeniy Polyakov <zbr@ioremap.net>");
MODULE_DESCRIPTION("Connector's test module");
