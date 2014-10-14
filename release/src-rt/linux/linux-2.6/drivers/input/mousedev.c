/*
 * Input driver to ExplorerPS/2 device driver module.
 *
 * Copyright (c) 1999-2002 Vojtech Pavlik
 * Copyright (c) 2004      Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MOUSEDEV_MINOR_BASE	32
#define MOUSEDEV_MINORS		32
#define MOUSEDEV_MIX		31

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/random.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/kernel.h>
#ifdef CONFIG_INPUT_MOUSEDEV_PSAUX
#include <linux/miscdevice.h>
#endif

MODULE_AUTHOR("Vojtech Pavlik <vojtech@ucw.cz>");
MODULE_DESCRIPTION("Mouse (ExplorerPS/2) device interfaces");
MODULE_LICENSE("GPL");

#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_X
#define CONFIG_INPUT_MOUSEDEV_SCREEN_X	1024
#endif
#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_Y
#define CONFIG_INPUT_MOUSEDEV_SCREEN_Y	768
#endif

static int xres = CONFIG_INPUT_MOUSEDEV_SCREEN_X;
module_param(xres, uint, 0644);
MODULE_PARM_DESC(xres, "Horizontal screen resolution");

static int yres = CONFIG_INPUT_MOUSEDEV_SCREEN_Y;
module_param(yres, uint, 0644);
MODULE_PARM_DESC(yres, "Vertical screen resolution");

static unsigned tap_time = 200;
module_param(tap_time, uint, 0644);
MODULE_PARM_DESC(tap_time, "Tap time for touchpads in absolute mode (msecs)");

struct mousedev_hw_data {
	int dx, dy, dz;
	int x, y;
	int abs_event;
	unsigned long buttons;
};

struct mousedev {
	int open;
	int minor;
	struct input_handle handle;
	wait_queue_head_t wait;
	struct list_head client_list;
	spinlock_t client_lock; /* protects client_list */
	struct mutex mutex;
	struct device dev;
	bool exist;

	struct list_head mixdev_node;
	int mixdev_open;

	struct mousedev_hw_data packet;
	unsigned int pkt_count;
	int old_x[4], old_y[4];
	int frac_dx, frac_dy;
	unsigned long touch;
};

enum mousedev_emul {
	MOUSEDEV_EMUL_PS2,
	MOUSEDEV_EMUL_IMPS,
	MOUSEDEV_EMUL_EXPS
};

struct mousedev_motion {
	int dx, dy, dz;
	unsigned long buttons;
};

#define PACKET_QUEUE_LEN	16
struct mousedev_client {
	struct fasync_struct *fasync;
	struct mousedev *mousedev;
	struct list_head node;

	struct mousedev_motion packets[PACKET_QUEUE_LEN];
	unsigned int head, tail;
	spinlock_t packet_lock;
	int pos_x, pos_y;

	signed char ps2[6];
	unsigned char ready, buffer, bufsiz;
	unsigned char imexseq, impsseq;
	enum mousedev_emul mode;
	unsigned long last_buttons;
};

#define MOUSEDEV_SEQ_LEN	6

static unsigned char mousedev_imps_seq[] = { 0xf3, 200, 0xf3, 100, 0xf3, 80 };
static unsigned char mousedev_imex_seq[] = { 0xf3, 200, 0xf3, 200, 0xf3, 80 };

static struct input_handler mousedev_handler;

static struct mousedev *mousedev_table[MOUSEDEV_MINORS];
static DEFINE_MUTEX(mousedev_table_mutex);
static struct mousedev *mousedev_mix;
static LIST_HEAD(mousedev_mix_list);

static void mixdev_open_devices(void);
static void mixdev_close_devices(void);

#define fx(i)  (mousedev->old_x[(mousedev->pkt_count - (i)) & 03])
#define fy(i)  (mousedev->old_y[(mousedev->pkt_count - (i)) & 03])

static void mousedev_touchpad_event(struct input_dev *dev,
				    struct mousedev *mousedev,
				    unsigned int code, int value)
{
	int size, tmp;
	enum { FRACTION_DENOM = 128 };

	switch (code) {

	case ABS_X:

		fx(0) = value;
		if (mousedev->touch && mousedev->pkt_count >= 2) {
			size = input_abs_get_max(dev, ABS_X) -
					input_abs_get_min(dev, ABS_X);
			if (size == 0)
				size = 256 * 2;

			tmp = ((value - fx(2)) * 256 * FRACTION_DENOM) / size;
			tmp += mousedev->frac_dx;
			mousedev->packet.dx = tmp / FRACTION_DENOM;
			mousedev->frac_dx =
				tmp - mousedev->packet.dx * FRACTION_DENOM;
		}
		break;

	case ABS_Y:
		fy(0) = value;
		if (mousedev->touch && mousedev->pkt_count >= 2) {
			/* use X size for ABS_Y to keep the same scale */
			size = input_abs_get_max(dev, ABS_X) -
					input_abs_get_min(dev, ABS_X);
			if (size == 0)
				size = 256 * 2;

			tmp = -((value - fy(2)) * 256 * FRACTION_DENOM) / size;
			tmp += mousedev->frac_dy;
			mousedev->packet.dy = tmp / FRACTION_DENOM;
			mousedev->frac_dy = tmp -
				mousedev->packet.dy * FRACTION_DENOM;
		}
		break;
	}
}

static void mousedev_abs_event(struct input_dev *dev, struct mousedev *mousedev,
				unsigned int code, int value)
{
	int min, max, size;

	switch (code) {

	case ABS_X:
		min = input_abs_get_min(dev, ABS_X);
		max = input_abs_get_max(dev, ABS_X);

		size = max - min;
		if (size == 0)
			size = xres ? : 1;

		value = clamp(value, min, max);

		mousedev->packet.x = ((value - min) * xres) / size;
		mousedev->packet.abs_event = 1;
		break;

	case ABS_Y:
		min = input_abs_get_min(dev, ABS_Y);
		max = input_abs_get_max(dev, ABS_Y);

		size = max - min;
		if (size == 0)
			size = yres ? : 1;

		value = clamp(value, min, max);

		mousedev->packet.y = yres - ((value - min) * yres) / size;
		mousedev->packet.abs_event = 1;
		break;
	}
}

static void mousedev_rel_event(struct mousedev *mousedev,
				unsigned int code, int value)
{
	switch (code) {
	case REL_X:
		mousedev->packet.dx += value;
		break;

	case REL_Y:
		mousedev->packet.dy -= value;
		break;

	case REL_WHEEL:
		mousedev->packet.dz -= value;
		break;
	}
}

static void mousedev_key_event(struct mousedev *mousedev,
				unsigned int code, int value)
{
	int index;

	switch (code) {

	case BTN_TOUCH:
	case BTN_0:
	case BTN_LEFT:		index = 0; break;

	case BTN_STYLUS:
	case BTN_1:
	case BTN_RIGHT:		index = 1; break;

	case BTN_2:
	case BTN_FORWARD:
	case BTN_STYLUS2:
	case BTN_MIDDLE:	index = 2; break;

	case BTN_3:
	case BTN_BACK:
	case BTN_SIDE:		index = 3; break;

	case BTN_4:
	case BTN_EXTRA:		index = 4; break;

	default:		return;
	}

	if (value) {
		set_bit(index, &mousedev->packet.buttons);
		set_bit(index, &mousedev_mix->packet.buttons);
	} else {
		clear_bit(index, &mousedev->packet.buttons);
		clear_bit(index, &mousedev_mix->packet.buttons);
	}
}

static void mousedev_notify_readers(struct mousedev *mousedev,
				    struct mousedev_hw_data *packet)
{
	struct mousedev_client *client;
	struct mousedev_motion *p;
	unsigned int new_head;
	int wake_readers = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(client, &mousedev->client_list, node) {

		/* Just acquire the lock, interrupts already disabled */
		spin_lock(&client->packet_lock);

		p = &client->packets[client->head];
		if (client->ready && p->buttons != mousedev->packet.buttons) {
			new_head = (client->head + 1) % PACKET_QUEUE_LEN;
			if (new_head != client->tail) {
				p = &client->packets[client->head = new_head];
				memset(p, 0, sizeof(struct mousedev_motion));
			}
		}

		if (packet->abs_event) {
			p->dx += packet->x - client->pos_x;
			p->dy += packet->y - client->pos_y;
			client->pos_x = packet->x;
			client->pos_y = packet->y;
		}

		client->pos_x += packet->dx;
		client->pos_x = client->pos_x < 0 ?
			0 : (client->pos_x >= xres ? xres : client->pos_x);
		client->pos_y += packet->dy;
		client->pos_y = client->pos_y < 0 ?
			0 : (client->pos_y >= yres ? yres : client->pos_y);

		p->dx += packet->dx;
		p->dy += packet->dy;
		p->dz += packet->dz;
		p->buttons = mousedev->packet.buttons;

		if (p->dx || p->dy || p->dz ||
		    p->buttons != client->last_buttons)
			client->ready = 1;

		spin_unlock(&client->packet_lock);

		if (client->ready) {
			kill_fasync(&client->fasync, SIGIO, POLL_IN);
			wake_readers = 1;
		}
	}
	rcu_read_unlock();

	if (wake_readers)
		wake_up_interruptible(&mousedev->wait);
}

static void mousedev_touchpad_touch(struct mousedev *mousedev, int value)
{
	if (!value) {
		if (mousedev->touch &&
		    time_before(jiffies,
				mousedev->touch + msecs_to_jiffies(tap_time))) {
			/*
			 * Toggle left button to emulate tap.
			 * We rely on the fact that mousedev_mix always has 0
			 * motion packet so we won't mess current position.
			 */
			set_bit(0, &mousedev->packet.buttons);
			set_bit(0, &mousedev_mix->packet.buttons);
			mousedev_notify_readers(mousedev, &mousedev_mix->packet);
			mousedev_notify_readers(mousedev_mix,
						&mousedev_mix->packet);
			clear_bit(0, &mousedev->packet.buttons);
			clear_bit(0, &mousedev_mix->packet.buttons);
		}
		mousedev->touch = mousedev->pkt_count = 0;
		mousedev->frac_dx = 0;
		mousedev->frac_dy = 0;

	} else if (!mousedev->touch)
		mousedev->touch = jiffies;
}

static void mousedev_event(struct input_handle *handle,
			   unsigned int type, unsigned int code, int value)
{
	struct mousedev *mousedev = handle->private;

	switch (type) {

	case EV_ABS:
		/* Ignore joysticks */
		if (test_bit(BTN_TRIGGER, handle->dev->keybit))
			return;

		if (test_bit(BTN_TOOL_FINGER, handle->dev->keybit))
			mousedev_touchpad_event(handle->dev,
						mousedev, code, value);
		else
			mousedev_abs_event(handle->dev, mousedev, code, value);

		break;

	case EV_REL:
		mousedev_rel_event(mousedev, code, value);
		break;

	case EV_KEY:
		if (value != 2) {
			if (code == BTN_TOUCH &&
			    test_bit(BTN_TOOL_FINGER, handle->dev->keybit))
				mousedev_touchpad_touch(mousedev, value);
			else
				mousedev_key_event(mousedev, code, value);
		}
		break;

	case EV_SYN:
		if (code == SYN_REPORT) {
			if (mousedev->touch) {
				mousedev->pkt_count++;
				/*
				 * Input system eats duplicate events,
				 * but we need all of them to do correct
				 * averaging so apply present one forward
				 */
				fx(0) = fx(1);
				fy(0) = fy(1);
			}

			mousedev_notify_readers(mousedev, &mousedev->packet);
			mousedev_notify_readers(mousedev_mix, &mousedev->packet);

			mousedev->packet.dx = mousedev->packet.dy =
				mousedev->packet.dz = 0;
			mousedev->packet.abs_event = 0;
		}
		break;
	}
}

static int mousedev_fasync(int fd, struct file *file, int on)
{
	struct mousedev_client *client = file->private_data;

	return fasync_helper(fd, file, on, &client->fasync);
}

static void mousedev_free(struct device *dev)
{
	struct mousedev *mousedev = container_of(dev, struct mousedev, dev);

	input_put_device(mousedev->handle.dev);
	kfree(mousedev);
}

static int mousedev_open_device(struct mousedev *mousedev)
{
	int retval;

	retval = mutex_lock_interruptible(&mousedev->mutex);
	if (retval)
		return retval;

	if (mousedev->minor == MOUSEDEV_MIX)
		mixdev_open_devices();
	else if (!mousedev->exist)
		retval = -ENODEV;
	else if (!mousedev->open++) {
		retval = input_open_device(&mousedev->handle);
		if (retval)
			mousedev->open--;
	}

	mutex_unlock(&mousedev->mutex);
	return retval;
}

static void mousedev_close_device(struct mousedev *mousedev)
{
	mutex_lock(&mousedev->mutex);

	if (mousedev->minor == MOUSEDEV_MIX)
		mixdev_close_devices();
	else if (mousedev->exist && !--mousedev->open)
		input_close_device(&mousedev->handle);

	mutex_unlock(&mousedev->mutex);
}

/*
 * Open all available devices so they can all be multiplexed in one.
 * stream. Note that this function is called with mousedev_mix->mutex
 * held.
 */
static void mixdev_open_devices(void)
{
	struct mousedev *mousedev;

	if (mousedev_mix->open++)
		return;

	list_for_each_entry(mousedev, &mousedev_mix_list, mixdev_node) {
		if (!mousedev->mixdev_open) {
			if (mousedev_open_device(mousedev))
				continue;

			mousedev->mixdev_open = 1;
		}
	}
}

/*
 * Close all devices that were opened as part of multiplexed
 * device. Note that this function is called with mousedev_mix->mutex
 * held.
 */
static void mixdev_close_devices(void)
{
	struct mousedev *mousedev;

	if (--mousedev_mix->open)
		return;

	list_for_each_entry(mousedev, &mousedev_mix_list, mixdev_node) {
		if (mousedev->mixdev_open) {
			mousedev->mixdev_open = 0;
			mousedev_close_device(mousedev);
		}
	}
}


static void mousedev_attach_client(struct mousedev *mousedev,
				   struct mousedev_client *client)
{
	spin_lock(&mousedev->client_lock);
	list_add_tail_rcu(&client->node, &mousedev->client_list);
	spin_unlock(&mousedev->client_lock);
	synchronize_rcu();
}

static void mousedev_detach_client(struct mousedev *mousedev,
				   struct mousedev_client *client)
{
	spin_lock(&mousedev->client_lock);
	list_del_rcu(&client->node);
	spin_unlock(&mousedev->client_lock);
	synchronize_rcu();
}

static int mousedev_release(struct inode *inode, struct file *file)
{
	struct mousedev_client *client = file->private_data;
	struct mousedev *mousedev = client->mousedev;

	mousedev_detach_client(mousedev, client);
	kfree(client);

	mousedev_close_device(mousedev);
	put_device(&mousedev->dev);

	return 0;
}

static int mousedev_open(struct inode *inode, struct file *file)
{
	struct mousedev_client *client;
	struct mousedev *mousedev;
	int error;
	int i;

#ifdef CONFIG_INPUT_MOUSEDEV_PSAUX
	if (imajor(inode) == MISC_MAJOR)
		i = MOUSEDEV_MIX;
	else
#endif
		i = iminor(inode) - MOUSEDEV_MINOR_BASE;

	if (i >= MOUSEDEV_MINORS)
		return -ENODEV;

	error = mutex_lock_interruptible(&mousedev_table_mutex);
	if (error) {
		return error;
	}
	mousedev = mousedev_table[i];
	if (mousedev)
		get_device(&mousedev->dev);
	mutex_unlock(&mousedev_table_mutex);

	if (!mousedev) {
		return -ENODEV;
	}

	client = kzalloc(sizeof(struct mousedev_client), GFP_KERNEL);
	if (!client) {
		error = -ENOMEM;
		goto err_put_mousedev;
	}

	spin_lock_init(&client->packet_lock);
	client->pos_x = xres / 2;
	client->pos_y = yres / 2;
	client->mousedev = mousedev;
	mousedev_attach_client(mousedev, client);

	error = mousedev_open_device(mousedev);
	if (error)
		goto err_free_client;

	file->private_data = client;
	return 0;

 err_free_client:
	mousedev_detach_client(mousedev, client);
	kfree(client);
 err_put_mousedev:
	put_device(&mousedev->dev);
	return error;
}

static inline int mousedev_limit_delta(int delta, int limit)
{
	return delta > limit ? limit : (delta < -limit ? -limit : delta);
}

static void mousedev_packet(struct mousedev_client *client,
			    signed char *ps2_data)
{
	struct mousedev_motion *p = &client->packets[client->tail];

	ps2_data[0] = 0x08 |
		((p->dx < 0) << 4) | ((p->dy < 0) << 5) | (p->buttons & 0x07);
	ps2_data[1] = mousedev_limit_delta(p->dx, 127);
	ps2_data[2] = mousedev_limit_delta(p->dy, 127);
	p->dx -= ps2_data[1];
	p->dy -= ps2_data[2];

	switch (client->mode) {
	case MOUSEDEV_EMUL_EXPS:
		ps2_data[3] = mousedev_limit_delta(p->dz, 7);
		p->dz -= ps2_data[3];
		ps2_data[3] = (ps2_data[3] & 0x0f) | ((p->buttons & 0x18) << 1);
		client->bufsiz = 4;
		break;

	case MOUSEDEV_EMUL_IMPS:
		ps2_data[0] |=
			((p->buttons & 0x10) >> 3) | ((p->buttons & 0x08) >> 1);
		ps2_data[3] = mousedev_limit_delta(p->dz, 127);
		p->dz -= ps2_data[3];
		client->bufsiz = 4;
		break;

	case MOUSEDEV_EMUL_PS2:
	default:
		ps2_data[0] |=
			((p->buttons & 0x10) >> 3) | ((p->buttons & 0x08) >> 1);
		p->dz = 0;
		client->bufsiz = 3;
		break;
	}

	if (!p->dx && !p->dy && !p->dz) {
		if (client->tail == client->head) {
			client->ready = 0;
			client->last_buttons = p->buttons;
		} else
			client->tail = (client->tail + 1) % PACKET_QUEUE_LEN;
	}
}

static void mousedev_generate_response(struct mousedev_client *client,
					int command)
{
	client->ps2[0] = 0xfa; /* ACK */

	switch (command) {

	case 0xeb: /* Poll */
		mousedev_packet(client, &client->ps2[1]);
		client->bufsiz++; /* account for leading ACK */
		break;

	case 0xf2: /* Get ID */
		switch (client->mode) {
		case MOUSEDEV_EMUL_PS2:
			client->ps2[1] = 0;
			break;
		case MOUSEDEV_EMUL_IMPS:
			client->ps2[1] = 3;
			break;
		case MOUSEDEV_EMUL_EXPS:
			client->ps2[1] = 4;
			break;
		}
		client->bufsiz = 2;
		break;

	case 0xe9: /* Get info */
		client->ps2[1] = 0x60; client->ps2[2] = 3; client->ps2[3] = 200;
		client->bufsiz = 4;
		break;

	case 0xff: /* Reset */
		client->impsseq = client->imexseq = 0;
		client->mode = MOUSEDEV_EMUL_PS2;
		client->ps2[1] = 0xaa; client->ps2[2] = 0x00;
		client->bufsiz = 3;
		break;

	default:
		client->bufsiz = 1;
		break;
	}
	client->buffer = client->bufsiz;
}

static ssize_t mousedev_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct mousedev_client *client = file->private_data;
	unsigned char c;
	unsigned int i;

	for (i = 0; i < count; i++) {

		if (get_user(c, buffer + i))
			return -EFAULT;

		spin_lock_irq(&client->packet_lock);

		if (c == mousedev_imex_seq[client->imexseq]) {
			if (++client->imexseq == MOUSEDEV_SEQ_LEN) {
				client->imexseq = 0;
				client->mode = MOUSEDEV_EMUL_EXPS;
			}
		} else
			client->imexseq = 0;

		if (c == mousedev_imps_seq[client->impsseq]) {
			if (++client->impsseq == MOUSEDEV_SEQ_LEN) {
				client->impsseq = 0;
				client->mode = MOUSEDEV_EMUL_IMPS;
			}
		} else
			client->impsseq = 0;

		mousedev_generate_response(client, c);

		spin_unlock_irq(&client->packet_lock);
	}

	kill_fasync(&client->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&client->mousedev->wait);

	return count;
}

static ssize_t mousedev_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	struct mousedev_client *client = file->private_data;
	struct mousedev *mousedev = client->mousedev;
	signed char data[sizeof(client->ps2)];
	int retval = 0;

	if (!client->ready && !client->buffer && mousedev->exist &&
	    (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	retval = wait_event_interruptible(mousedev->wait,
			!mousedev->exist || client->ready || client->buffer);
	if (retval)
		return retval;

	if (!mousedev->exist)
		return -ENODEV;

	spin_lock_irq(&client->packet_lock);

	if (!client->buffer && client->ready) {
		mousedev_packet(client, client->ps2);
		client->buffer = client->bufsiz;
	}

	if (count > client->buffer)
		count = client->buffer;

	memcpy(data, client->ps2 + client->bufsiz - client->buffer, count);
	client->buffer -= count;

	spin_unlock_irq(&client->packet_lock);

	if (copy_to_user(buffer, data, count))
		return -EFAULT;

	return count;
}

/* No kernel lock - fine */
static unsigned int mousedev_poll(struct file *file, poll_table *wait)
{
	struct mousedev_client *client = file->private_data;
	struct mousedev *mousedev = client->mousedev;
	unsigned int mask;

	poll_wait(file, &mousedev->wait, wait);

	mask = mousedev->exist ? POLLOUT | POLLWRNORM : POLLHUP | POLLERR;
	if (client->ready || client->buffer)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations mousedev_fops = {
	.owner =	THIS_MODULE,
	.read =		mousedev_read,
	.write =	mousedev_write,
	.poll =		mousedev_poll,
	.open =		mousedev_open,
	.release =	mousedev_release,
	.fasync =	mousedev_fasync,
	.llseek = noop_llseek,
};

static int mousedev_install_chrdev(struct mousedev *mousedev)
{
	mousedev_table[mousedev->minor] = mousedev;
	return 0;
}

static void mousedev_remove_chrdev(struct mousedev *mousedev)
{
	mutex_lock(&mousedev_table_mutex);
	mousedev_table[mousedev->minor] = NULL;
	mutex_unlock(&mousedev_table_mutex);
}

/*
 * Mark device non-existent. This disables writes, ioctls and
 * prevents new users from opening the device. Already posted
 * blocking reads will stay, however new ones will fail.
 */
static void mousedev_mark_dead(struct mousedev *mousedev)
{
	mutex_lock(&mousedev->mutex);
	mousedev->exist = false;
	mutex_unlock(&mousedev->mutex);
}

/*
 * Wake up users waiting for IO so they can disconnect from
 * dead device.
 */
static void mousedev_hangup(struct mousedev *mousedev)
{
	struct mousedev_client *client;

	spin_lock(&mousedev->client_lock);
	list_for_each_entry(client, &mousedev->client_list, node)
		kill_fasync(&client->fasync, SIGIO, POLL_HUP);
	spin_unlock(&mousedev->client_lock);

	wake_up_interruptible(&mousedev->wait);
}

static void mousedev_cleanup(struct mousedev *mousedev)
{
	struct input_handle *handle = &mousedev->handle;

	mousedev_mark_dead(mousedev);
	mousedev_hangup(mousedev);
	mousedev_remove_chrdev(mousedev);

	/* mousedev is marked dead so no one else accesses mousedev->open */
	if (mousedev->open)
		input_close_device(handle);
}

static struct mousedev *mousedev_create(struct input_dev *dev,
					struct input_handler *handler,
					int minor)
{
	struct mousedev *mousedev;
	int error;

	mousedev = kzalloc(sizeof(struct mousedev), GFP_KERNEL);
	if (!mousedev) {
		error = -ENOMEM;
		goto err_out;
	}

	INIT_LIST_HEAD(&mousedev->client_list);
	INIT_LIST_HEAD(&mousedev->mixdev_node);
	spin_lock_init(&mousedev->client_lock);
	mutex_init(&mousedev->mutex);
	lockdep_set_subclass(&mousedev->mutex,
			     minor == MOUSEDEV_MIX ? SINGLE_DEPTH_NESTING : 0);
	init_waitqueue_head(&mousedev->wait);

	if (minor == MOUSEDEV_MIX)
		dev_set_name(&mousedev->dev, "mice");
	else
		dev_set_name(&mousedev->dev, "mouse%d", minor);

	mousedev->minor = minor;
	mousedev->exist = true;
	mousedev->handle.dev = input_get_device(dev);
	mousedev->handle.name = dev_name(&mousedev->dev);
	mousedev->handle.handler = handler;
	mousedev->handle.private = mousedev;

	mousedev->dev.class = &input_class;
	if (dev)
		mousedev->dev.parent = &dev->dev;
	mousedev->dev.devt = MKDEV(INPUT_MAJOR, MOUSEDEV_MINOR_BASE + minor);
	mousedev->dev.release = mousedev_free;
	device_initialize(&mousedev->dev);

	if (minor != MOUSEDEV_MIX) {
		error = input_register_handle(&mousedev->handle);
		if (error)
			goto err_free_mousedev;
	}

	error = mousedev_install_chrdev(mousedev);
	if (error)
		goto err_unregister_handle;

	error = device_add(&mousedev->dev);
	if (error)
		goto err_cleanup_mousedev;

	return mousedev;

 err_cleanup_mousedev:
	mousedev_cleanup(mousedev);
 err_unregister_handle:
	if (minor != MOUSEDEV_MIX)
		input_unregister_handle(&mousedev->handle);
 err_free_mousedev:
	put_device(&mousedev->dev);
 err_out:
	return ERR_PTR(error);
}

static void mousedev_destroy(struct mousedev *mousedev)
{
	device_del(&mousedev->dev);
	mousedev_cleanup(mousedev);
	if (mousedev->minor != MOUSEDEV_MIX)
		input_unregister_handle(&mousedev->handle);
	put_device(&mousedev->dev);
}

static int mixdev_add_device(struct mousedev *mousedev)
{
	int retval;

	retval = mutex_lock_interruptible(&mousedev_mix->mutex);
	if (retval)
		return retval;

	if (mousedev_mix->open) {
		retval = mousedev_open_device(mousedev);
		if (retval)
			goto out;

		mousedev->mixdev_open = 1;
	}

	get_device(&mousedev->dev);
	list_add_tail(&mousedev->mixdev_node, &mousedev_mix_list);

 out:
	mutex_unlock(&mousedev_mix->mutex);
	return retval;
}

static void mixdev_remove_device(struct mousedev *mousedev)
{
	mutex_lock(&mousedev_mix->mutex);

	if (mousedev->mixdev_open) {
		mousedev->mixdev_open = 0;
		mousedev_close_device(mousedev);
	}

	list_del_init(&mousedev->mixdev_node);
	mutex_unlock(&mousedev_mix->mutex);

	put_device(&mousedev->dev);
}

static int mousedev_connect(struct input_handler *handler,
			    struct input_dev *dev,
			    const struct input_device_id *id)
{
	struct mousedev *mousedev;
	int minor;
	int error;

	for (minor = 0; minor < MOUSEDEV_MINORS; minor++)
		if (!mousedev_table[minor])
			break;

	if (minor == MOUSEDEV_MINORS) {
		pr_err("no more free mousedev devices\n");
		return -ENFILE;
	}

	mousedev = mousedev_create(dev, handler, minor);
	if (IS_ERR(mousedev))
		return PTR_ERR(mousedev);

	error = mixdev_add_device(mousedev);
	if (error) {
		mousedev_destroy(mousedev);
		return error;
	}

	return 0;
}

static void mousedev_disconnect(struct input_handle *handle)
{
	struct mousedev *mousedev = handle->private;

	mixdev_remove_device(mousedev);
	mousedev_destroy(mousedev);
}

static const struct input_device_id mousedev_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT |
				INPUT_DEVICE_ID_MATCH_RELBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_REL) },
		.keybit = { [BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) },
		.relbit = { BIT_MASK(REL_X) | BIT_MASK(REL_Y) },
	},	/* A mouse like device, at least one button,
		   two relative axes */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_RELBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_REL) },
		.relbit = { BIT_MASK(REL_WHEEL) },
	},	/* A separate scrollwheel */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT |
				INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},	/* A tablet like device, at least touch detection,
		   two absolute axes */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT |
				INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOOL_FINGER)] =
				BIT_MASK(BTN_TOOL_FINGER) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
				BIT_MASK(ABS_PRESSURE) |
				BIT_MASK(ABS_TOOL_WIDTH) },
	},	/* A touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},	/* Mouse-like device with absolute X and Y but ordinary
		   clicks, like hp ILO2 High Performance mouse */

	{ },	/* Terminating entry */
};

MODULE_DEVICE_TABLE(input, mousedev_ids);

static struct input_handler mousedev_handler = {
	.event =	mousedev_event,
	.connect =	mousedev_connect,
	.disconnect =	mousedev_disconnect,
	.fops =		&mousedev_fops,
	.minor =	MOUSEDEV_MINOR_BASE,
	.name =		"mousedev",
	.id_table =	mousedev_ids,
};

#ifdef CONFIG_INPUT_MOUSEDEV_PSAUX
static struct miscdevice psaux_mouse = {
	PSMOUSE_MINOR, "psaux", &mousedev_fops
};
static int psaux_registered;
#endif

static int __init mousedev_init(void)
{
	int error;

	mousedev_mix = mousedev_create(NULL, &mousedev_handler, MOUSEDEV_MIX);
	if (IS_ERR(mousedev_mix))
		return PTR_ERR(mousedev_mix);

	error = input_register_handler(&mousedev_handler);
	if (error) {
		mousedev_destroy(mousedev_mix);
		return error;
	}

#ifdef CONFIG_INPUT_MOUSEDEV_PSAUX
	error = misc_register(&psaux_mouse);
	if (error)
		pr_warning("could not register psaux device, error: %d\n",
			   error);
	else
		psaux_registered = 1;
#endif

	pr_info("PS/2 mouse device common for all mice\n");

	return 0;
}

static void __exit mousedev_exit(void)
{
#ifdef CONFIG_INPUT_MOUSEDEV_PSAUX
	if (psaux_registered)
		misc_deregister(&psaux_mouse);
#endif
	input_unregister_handler(&mousedev_handler);
	mousedev_destroy(mousedev_mix);
}

module_init(mousedev_init);
module_exit(mousedev_exit);
