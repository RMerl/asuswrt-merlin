/*
 *  Parallel port to Walkera WK-0701 TX joystick
 *
 *  Copyright (c) 2008 Peter Popovec
 *
 *  More about driver:  <file:Documentation/input/walkera0701.txt>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
*/

/* #define WK0701_DEBUG */

#define RESERVE 20000
#define SYNC_PULSE 1306000
#define BIN0_PULSE 288000
#define BIN1_PULSE 438000

#define ANALOG_MIN_PULSE 318000
#define ANALOG_MAX_PULSE 878000
#define ANALOG_DELTA 80000

#define BIN_SAMPLE ((BIN0_PULSE + BIN1_PULSE) / 2)

#define NO_SYNC 25

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/parport.h>
#include <linux/input.h>
#include <linux/hrtimer.h>

MODULE_AUTHOR("Peter Popovec <popovec@fei.tuke.sk>");
MODULE_DESCRIPTION("Walkera WK-0701 TX as joystick");
MODULE_LICENSE("GPL");

static unsigned int walkera0701_pp_no;
module_param_named(port, walkera0701_pp_no, int, 0);
MODULE_PARM_DESC(port,
		 "Parallel port adapter for Walkera WK-0701 TX (default is 0)");

/*
 * For now, only one device is supported, if somebody need more devices, code
 * can be expanded, one struct walkera_dev per device must be allocated and
 * set up by walkera0701_connect (release of device by walkera0701_disconnect)
 */

struct walkera_dev {
	unsigned char buf[25];
	u64 irq_time, irq_lasttime;
	int counter;
	int ack;

	struct input_dev *input_dev;
	struct hrtimer timer;

	struct parport *parport;
	struct pardevice *pardevice;
};

static struct walkera_dev w_dev;

static inline void walkera0701_parse_frame(struct walkera_dev *w)
{
	int i;
	int val1, val2, val3, val4, val5, val6, val7, val8;
	int crc1, crc2;

	for (crc1 = crc2 = i = 0; i < 10; i++) {
		crc1 += w->buf[i] & 7;
		crc2 += (w->buf[i] & 8) >> 3;
	}
	if ((w->buf[10] & 7) != (crc1 & 7))
		return;
	if (((w->buf[10] & 8) >> 3) != (((crc1 >> 3) + crc2) & 1))
		return;
	for (crc1 = crc2 = 0, i = 11; i < 23; i++) {
		crc1 += w->buf[i] & 7;
		crc2 += (w->buf[i] & 8) >> 3;
	}
	if ((w->buf[23] & 7) != (crc1 & 7))
		return;
	if (((w->buf[23] & 8) >> 3) != (((crc1 >> 3) + crc2) & 1))
		return;
	val1 = ((w->buf[0] & 7) * 256 + w->buf[1] * 16 + w->buf[2]) >> 2;
	val1 *= ((w->buf[0] >> 2) & 2) - 1;	/* sign */
	val2 = (w->buf[2] & 1) << 8 | (w->buf[3] << 4) | w->buf[4];
	val2 *= (w->buf[2] & 2) - 1;	/* sign */
	val3 = ((w->buf[5] & 7) * 256 + w->buf[6] * 16 + w->buf[7]) >> 2;
	val3 *= ((w->buf[5] >> 2) & 2) - 1;	/* sign */
	val4 = (w->buf[7] & 1) << 8 | (w->buf[8] << 4) | w->buf[9];
	val4 *= (w->buf[7] & 2) - 1;	/* sign */
	val5 = ((w->buf[11] & 7) * 256 + w->buf[12] * 16 + w->buf[13]) >> 2;
	val5 *= ((w->buf[11] >> 2) & 2) - 1;	/* sign */
	val6 = (w->buf[13] & 1) << 8 | (w->buf[14] << 4) | w->buf[15];
	val6 *= (w->buf[13] & 2) - 1;	/* sign */
	val7 = ((w->buf[16] & 7) * 256 + w->buf[17] * 16 + w->buf[18]) >> 2;
	val7 *= ((w->buf[16] >> 2) & 2) - 1;	/*sign */
	val8 = (w->buf[18] & 1) << 8 | (w->buf[19] << 4) | w->buf[20];
	val8 *= (w->buf[18] & 2) - 1;	/*sign */

#ifdef WK0701_DEBUG
	{
		int magic, magic_bit;
		magic = (w->buf[21] << 4) | w->buf[22];
		magic_bit = (w->buf[24] & 8) >> 3;
		printk(KERN_DEBUG
		       "walkera0701: %4d %4d %4d %4d  %4d %4d %4d %4d (magic %2x %d)\n",
		       val1, val2, val3, val4, val5, val6, val7, val8, magic,
		       magic_bit);
	}
#endif
	input_report_abs(w->input_dev, ABS_X, val2);
	input_report_abs(w->input_dev, ABS_Y, val1);
	input_report_abs(w->input_dev, ABS_Z, val6);
	input_report_abs(w->input_dev, ABS_THROTTLE, val3);
	input_report_abs(w->input_dev, ABS_RUDDER, val4);
	input_report_abs(w->input_dev, ABS_MISC, val7);
	input_report_key(w->input_dev, BTN_GEAR_DOWN, val5 > 0);
}

static inline int read_ack(struct pardevice *p)
{
	return parport_read_status(p->port) & 0x40;
}

/* falling edge, prepare to BIN value calculation */
static void walkera0701_irq_handler(void *handler_data)
{
	u64 pulse_time;
	struct walkera_dev *w = handler_data;

	w->irq_time = ktime_to_ns(ktime_get());
	pulse_time = w->irq_time - w->irq_lasttime;
	w->irq_lasttime = w->irq_time;

	/* cancel timer, if in handler or active do resync */
	if (unlikely(0 != hrtimer_try_to_cancel(&w->timer))) {
		w->counter = NO_SYNC;
		return;
	}

	if (w->counter < NO_SYNC) {
		if (w->ack) {
			pulse_time -= BIN1_PULSE;
			w->buf[w->counter] = 8;
		} else {
			pulse_time -= BIN0_PULSE;
			w->buf[w->counter] = 0;
		}
		if (w->counter == 24) {	/* full frame */
			walkera0701_parse_frame(w);
			w->counter = NO_SYNC;
			if (abs(pulse_time - SYNC_PULSE) < RESERVE)	/* new frame sync */
				w->counter = 0;
		} else {
			if ((pulse_time > (ANALOG_MIN_PULSE - RESERVE)
			     && (pulse_time < (ANALOG_MAX_PULSE + RESERVE)))) {
				pulse_time -= (ANALOG_MIN_PULSE - RESERVE);
				pulse_time = (u32) pulse_time / ANALOG_DELTA;	/* overtiping is safe, pulsetime < s32.. */
				w->buf[w->counter++] |= (pulse_time & 7);
			} else
				w->counter = NO_SYNC;
		}
	} else if (abs(pulse_time - SYNC_PULSE - BIN0_PULSE) <
				RESERVE + BIN1_PULSE - BIN0_PULSE)	/* frame sync .. */
		w->counter = 0;

	hrtimer_start(&w->timer, ktime_set(0, BIN_SAMPLE), HRTIMER_MODE_REL);
}

static enum hrtimer_restart timer_handler(struct hrtimer
					  *handle)
{
	struct walkera_dev *w;

	w = container_of(handle, struct walkera_dev, timer);
	w->ack = read_ack(w->pardevice);

	return HRTIMER_NORESTART;
}

static int walkera0701_open(struct input_dev *dev)
{
	struct walkera_dev *w = input_get_drvdata(dev);

	parport_enable_irq(w->parport);
	return 0;
}

static void walkera0701_close(struct input_dev *dev)
{
	struct walkera_dev *w = input_get_drvdata(dev);

	parport_disable_irq(w->parport);
}

static int walkera0701_connect(struct walkera_dev *w, int parport)
{
	int err = -ENODEV;

	w->parport = parport_find_number(parport);
	if (w->parport == NULL)
		return -ENODEV;

	if (w->parport->irq == -1) {
		printk(KERN_ERR "walkera0701: parport without interrupt\n");
		goto init_err;
	}

	err = -EBUSY;
	w->pardevice = parport_register_device(w->parport, "walkera0701",
				    NULL, NULL, walkera0701_irq_handler,
				    PARPORT_DEV_EXCL, w);
	if (!w->pardevice)
		goto init_err;

	if (parport_negotiate(w->pardevice->port, IEEE1284_MODE_COMPAT))
		goto init_err1;

	if (parport_claim(w->pardevice))
		goto init_err1;

	w->input_dev = input_allocate_device();
	if (!w->input_dev)
		goto init_err2;

	input_set_drvdata(w->input_dev, w);
	w->input_dev->name = "Walkera WK-0701 TX";
	w->input_dev->phys = w->parport->name;
	w->input_dev->id.bustype = BUS_PARPORT;

	/* TODO what id vendor/product/version ? */
	w->input_dev->id.vendor = 0x0001;
	w->input_dev->id.product = 0x0001;
	w->input_dev->id.version = 0x0100;
	w->input_dev->open = walkera0701_open;
	w->input_dev->close = walkera0701_close;

	w->input_dev->evbit[0] = BIT(EV_ABS) | BIT_MASK(EV_KEY);
	w->input_dev->keybit[BIT_WORD(BTN_GEAR_DOWN)] = BIT_MASK(BTN_GEAR_DOWN);

	input_set_abs_params(w->input_dev, ABS_X, -512, 512, 0, 0);
	input_set_abs_params(w->input_dev, ABS_Y, -512, 512, 0, 0);
	input_set_abs_params(w->input_dev, ABS_Z, -512, 512, 0, 0);
	input_set_abs_params(w->input_dev, ABS_THROTTLE, -512, 512, 0, 0);
	input_set_abs_params(w->input_dev, ABS_RUDDER, -512, 512, 0, 0);
	input_set_abs_params(w->input_dev, ABS_MISC, -512, 512, 0, 0);

	err = input_register_device(w->input_dev);
	if (err)
		goto init_err3;

	hrtimer_init(&w->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	w->timer.function = timer_handler;
	return 0;

 init_err3:
	input_free_device(w->input_dev);
 init_err2:
	parport_release(w->pardevice);
 init_err1:
	parport_unregister_device(w->pardevice);
 init_err:
	parport_put_port(w->parport);
	return err;
}

static void walkera0701_disconnect(struct walkera_dev *w)
{
	hrtimer_cancel(&w->timer);
	input_unregister_device(w->input_dev);
	parport_release(w->pardevice);
	parport_unregister_device(w->pardevice);
	parport_put_port(w->parport);
}

static int __init walkera0701_init(void)
{
	return walkera0701_connect(&w_dev, walkera0701_pp_no);
}

static void __exit walkera0701_exit(void)
{
	walkera0701_disconnect(&w_dev);
}

module_init(walkera0701_init);
module_exit(walkera0701_exit);
