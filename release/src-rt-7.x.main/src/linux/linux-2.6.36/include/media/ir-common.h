/*
 *
 * some common structs and functions to handle infrared remotes via
 * input layer ...
 *
 * (c) 2003 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
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
 */

#ifndef _IR_COMMON
#define _IR_COMMON

#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <media/ir-core.h>

#define RC5_START(x)	(((x)>>12)&3)
#define RC5_TOGGLE(x)	(((x)>>11)&1)
#define RC5_ADDR(x)	(((x)>>6)&31)
#define RC5_INSTR(x)	((x)&63)

struct ir_input_state {
	/* configuration */
	u64      ir_type;

	/* key info */
	u32                ir_key;      /* ir scancode */
	u32                keycode;     /* linux key code */
	int                keypressed;  /* current state */
};

/* this was saa7134_ir and bttv_ir, moved here for
 * rc5 decoding. */
struct card_ir {
	struct input_dev        *dev;
	struct ir_input_state   ir;
	char                    name[32];
	char                    phys[32];
	int			users;

	u32			running:1;
	struct ir_dev_props	props;

	/* Usual gpio signalling */

	u32                     mask_keycode;
	u32                     mask_keydown;
	u32                     mask_keyup;
	u32                     polling;
	u32                     last_gpio;
	int			shift_by;
	int			start; // What should RC5_START() be
	int			addr; // What RC5_ADDR() should be.
	int			rc5_key_timeout;
	int			rc5_remote_gap;
	struct work_struct      work;
	struct timer_list       timer;

	/* RC5 gpio */
	u32 rc5_gpio;
	struct timer_list timer_end;	/* timer_end for code completion */
	struct timer_list timer_keyup;	/* timer_end for key release */
	u32 last_rc5;			/* last good rc5 code */
	u32 last_bit;			/* last raw bit seen */
	u32 code;			/* raw code under construction */
	struct timeval base_time;	/* time of last seen code */
	int active;			/* building raw code */

	/* NEC decoding */
	u32			nec_gpio;
	struct tasklet_struct   tlet;

	/* IR core raw decoding */
	u32			raw_decode;
};

/* Routines from ir-functions.c */

int ir_input_init(struct input_dev *dev, struct ir_input_state *ir,
		   const u64 ir_type);
void ir_input_nokey(struct input_dev *dev, struct ir_input_state *ir);
void ir_input_keydown(struct input_dev *dev, struct ir_input_state *ir,
		      u32 ir_key);
u32  ir_extract_bits(u32 data, u32 mask);
int  ir_dump_samples(u32 *samples, int count);
int  ir_decode_biphase(u32 *samples, int count, int low, int high);
int  ir_decode_pulsedistance(u32 *samples, int count, int low, int high);
u32  ir_rc5_decode(unsigned int code);

void ir_rc5_timer_end(unsigned long data);
void ir_rc5_timer_keyup(unsigned long data);

#endif
