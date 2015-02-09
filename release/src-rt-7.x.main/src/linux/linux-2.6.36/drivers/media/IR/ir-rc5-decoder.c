/* ir-rc5-decoder.c - handle RC5(x) IR Pulse/Space protocol
 *
 * Copyright (C) 2010 by Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

/*
 * This code handles 14 bits RC5 protocols and 20 bits RC5x protocols.
 * There are other variants that use a different number of bits.
 * This is currently unsupported.
 * It considers a carrier of 36 kHz, with a total of 14/20 bits, where
 * the first two bits are start bits, and a third one is a filing bit
 */

#include "ir-core-priv.h"

#define RC5_NBITS		14
#define RC5X_NBITS		20
#define CHECK_RC5X_NBITS	8
#define RC5_UNIT		888888 /* ns */
#define RC5_BIT_START		(1 * RC5_UNIT)
#define RC5_BIT_END		(1 * RC5_UNIT)
#define RC5X_SPACE		(4 * RC5_UNIT)

enum rc5_state {
	STATE_INACTIVE,
	STATE_BIT_START,
	STATE_BIT_END,
	STATE_CHECK_RC5X,
	STATE_FINISHED,
};

/**
 * ir_rc5_decode() - Decode one RC-5 pulse or space
 * @input_dev:	the struct input_dev descriptor of the device
 * @ev:		the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int ir_rc5_decode(struct input_dev *input_dev, struct ir_raw_event ev)
{
	struct ir_input_dev *ir_dev = input_get_drvdata(input_dev);
	struct rc5_dec *data = &ir_dev->raw->rc5;
	u8 toggle;
	u32 scancode;

        if (!(ir_dev->raw->enabled_protocols & IR_TYPE_RC5))
                return 0;

	if (IS_RESET(ev)) {
		data->state = STATE_INACTIVE;
		return 0;
	}

	if (!geq_margin(ev.duration, RC5_UNIT, RC5_UNIT / 2))
		goto out;

again:
	IR_dprintk(2, "RC5(x) decode started at state %i (%uus %s)\n",
		   data->state, TO_US(ev.duration), TO_STR(ev.pulse));

	if (!geq_margin(ev.duration, RC5_UNIT, RC5_UNIT / 2))
		return 0;

	switch (data->state) {

	case STATE_INACTIVE:
		if (!ev.pulse)
			break;

		data->state = STATE_BIT_START;
		data->count = 1;
		/* We just need enough bits to get to STATE_CHECK_RC5X */
		data->wanted_bits = RC5X_NBITS;
		decrease_duration(&ev, RC5_BIT_START);
		goto again;

	case STATE_BIT_START:
		if (!eq_margin(ev.duration, RC5_BIT_START, RC5_UNIT / 2))
			break;

		data->bits <<= 1;
		if (!ev.pulse)
			data->bits |= 1;
		data->count++;
		data->state = STATE_BIT_END;
		return 0;

	case STATE_BIT_END:
		if (!is_transition(&ev, &ir_dev->raw->prev_ev))
			break;

		if (data->count == data->wanted_bits)
			data->state = STATE_FINISHED;
		else if (data->count == CHECK_RC5X_NBITS)
			data->state = STATE_CHECK_RC5X;
		else
			data->state = STATE_BIT_START;

		decrease_duration(&ev, RC5_BIT_END);
		goto again;

	case STATE_CHECK_RC5X:
		if (!ev.pulse && geq_margin(ev.duration, RC5X_SPACE, RC5_UNIT / 2)) {
			/* RC5X */
			data->wanted_bits = RC5X_NBITS;
			decrease_duration(&ev, RC5X_SPACE);
		} else {
			/* RC5 */
			data->wanted_bits = RC5_NBITS;
		}
		data->state = STATE_BIT_START;
		goto again;

	case STATE_FINISHED:
		if (ev.pulse)
			break;

		if (data->wanted_bits == RC5X_NBITS) {
			/* RC5X */
			u8 xdata, command, system;
			xdata    = (data->bits & 0x0003F) >> 0;
			command  = (data->bits & 0x00FC0) >> 6;
			system   = (data->bits & 0x1F000) >> 12;
			toggle   = (data->bits & 0x20000) ? 1 : 0;
			command += (data->bits & 0x01000) ? 0 : 0x40;
			scancode = system << 16 | command << 8 | xdata;

			IR_dprintk(1, "RC5X scancode 0x%06x (toggle: %u)\n",
				   scancode, toggle);

		} else {
			/* RC5 */
			u8 command, system;
			command  = (data->bits & 0x0003F) >> 0;
			system   = (data->bits & 0x007C0) >> 6;
			toggle   = (data->bits & 0x00800) ? 1 : 0;
			command += (data->bits & 0x01000) ? 0 : 0x40;
			scancode = system << 8 | command;

			IR_dprintk(1, "RC5 scancode 0x%04x (toggle: %u)\n",
				   scancode, toggle);
		}

		ir_keydown(input_dev, scancode, toggle);
		data->state = STATE_INACTIVE;
		return 0;
	}

out:
	IR_dprintk(1, "RC5(x) decode failed at state %i (%uus %s)\n",
		   data->state, TO_US(ev.duration), TO_STR(ev.pulse));
	data->state = STATE_INACTIVE;
	return -EINVAL;
}

static struct ir_raw_handler rc5_handler = {
	.protocols	= IR_TYPE_RC5,
	.decode		= ir_rc5_decode,
};

static int __init ir_rc5_decode_init(void)
{
	ir_raw_handler_register(&rc5_handler);

	printk(KERN_INFO "IR RC5(x) protocol handler initialized\n");
	return 0;
}

static void __exit ir_rc5_decode_exit(void)
{
	ir_raw_handler_unregister(&rc5_handler);
}

module_init(ir_rc5_decode_init);
module_exit(ir_rc5_decode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
MODULE_AUTHOR("Red Hat Inc. (http://www.redhat.com)");
MODULE_DESCRIPTION("RC5(x) IR protocol decoder");
