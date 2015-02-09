/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fir.h - General telephony FIR routines
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2002 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(_FIR_H_)
#define _FIR_H_

/*
   Blackfin NOTES & IDEAS:

   A simple dot product function is used to implement the filter.  This performs
   just one MAC/cycle which is inefficient but was easy to implement as a first
   pass.  The current Blackfin code also uses an unrolled form of the filter
   history to avoid 0 length hardware loop issues.  This is wasteful of
   memory.

   Ideas for improvement:

   1/ Rewrite filter for dual MAC inner loop.  The issue here is handling
   history sample offsets that are 16 bit aligned - the dual MAC needs
   32 bit aligmnent.  There are some good examples in libbfdsp.

   2/ Use the hardware circular buffer facility tohalve memory usage.

   3/ Consider using internal memory.

   Using less memory might also improve speed as cache misses will be
   reduced. A drop in MIPs and memory approaching 50% should be
   possible.

   The foreground and background filters currenlty use a total of
   about 10 MIPs/ch as measured with speedtest.c on a 256 TAP echo
   can.
*/

/*
 * 16 bit integer FIR descriptor. This defines the working state for a single
 * instance of an FIR filter using 16 bit integer coefficients.
 */
struct fir16_state_t {
	int taps;
	int curr_pos;
	const int16_t *coeffs;
	int16_t *history;
};

/*
 * 32 bit integer FIR descriptor. This defines the working state for a single
 * instance of an FIR filter using 32 bit integer coefficients, and filtering
 * 16 bit integer data.
 */
struct fir32_state_t {
	int taps;
	int curr_pos;
	const int32_t *coeffs;
	int16_t *history;
};

/*
 * Floating point FIR descriptor. This defines the working state for a single
 * instance of an FIR filter using floating point coefficients and data.
 */
struct fir_float_state_t {
	int taps;
	int curr_pos;
	const float *coeffs;
	float *history;
};

static inline const int16_t *fir16_create(struct fir16_state_t *fir,
					      const int16_t *coeffs, int taps)
{
	fir->taps = taps;
	fir->curr_pos = taps - 1;
	fir->coeffs = coeffs;
#if defined(__bfin__)
	fir->history = kcalloc(2 * taps, sizeof(int16_t), GFP_KERNEL);
#else
	fir->history = kcalloc(taps, sizeof(int16_t), GFP_KERNEL);
#endif
	return fir->history;
}

static inline void fir16_flush(struct fir16_state_t *fir)
{
#if defined(__bfin__)
	memset(fir->history, 0, 2 * fir->taps * sizeof(int16_t));
#else
	memset(fir->history, 0, fir->taps * sizeof(int16_t));
#endif
}

static inline void fir16_free(struct fir16_state_t *fir)
{
	kfree(fir->history);
}

#ifdef __bfin__
static inline int32_t dot_asm(short *x, short *y, int len)
{
	int dot;

	len--;

	__asm__("I0 = %1;\n\t"
		"I1 = %2;\n\t"
		"A0 = 0;\n\t"
		"R0.L = W[I0++] || R1.L = W[I1++];\n\t"
		"LOOP dot%= LC0 = %3;\n\t"
		"LOOP_BEGIN dot%=;\n\t"
		"A0 += R0.L * R1.L (IS) || R0.L = W[I0++] || R1.L = W[I1++];\n\t"
		"LOOP_END dot%=;\n\t"
		"A0 += R0.L*R1.L (IS);\n\t"
		"R0 = A0;\n\t"
		"%0 = R0;\n\t"
		: "=&d"(dot)
		: "a"(x), "a"(y), "a"(len)
		: "I0", "I1", "A1", "A0", "R0", "R1"
	);

	return dot;
}
#endif

static inline int16_t fir16(struct fir16_state_t *fir, int16_t sample)
{
	int32_t y;
#if defined(__bfin__)
	fir->history[fir->curr_pos] = sample;
	fir->history[fir->curr_pos + fir->taps] = sample;
	y = dot_asm((int16_t *) fir->coeffs, &fir->history[fir->curr_pos],
		    fir->taps);
#else
	int i;
	int offset1;
	int offset2;

	fir->history[fir->curr_pos] = sample;

	offset2 = fir->curr_pos;
	offset1 = fir->taps - offset2;
	y = 0;
	for (i = fir->taps - 1; i >= offset1; i--)
		y += fir->coeffs[i] * fir->history[i - offset1];
	for (; i >= 0; i--)
		y += fir->coeffs[i] * fir->history[i + offset2];
#endif
	if (fir->curr_pos <= 0)
		fir->curr_pos = fir->taps;
	fir->curr_pos--;
	return (int16_t) (y >> 15);
}

static inline const int16_t *fir32_create(struct fir32_state_t *fir,
					      const int32_t *coeffs, int taps)
{
	fir->taps = taps;
	fir->curr_pos = taps - 1;
	fir->coeffs = coeffs;
	fir->history = kcalloc(taps, sizeof(int16_t), GFP_KERNEL);
	return fir->history;
}

static inline void fir32_flush(struct fir32_state_t *fir)
{
	memset(fir->history, 0, fir->taps * sizeof(int16_t));
}

static inline void fir32_free(struct fir32_state_t *fir)
{
	kfree(fir->history);
}

static inline int16_t fir32(struct fir32_state_t *fir, int16_t sample)
{
	int i;
	int32_t y;
	int offset1;
	int offset2;

	fir->history[fir->curr_pos] = sample;
	offset2 = fir->curr_pos;
	offset1 = fir->taps - offset2;
	y = 0;
	for (i = fir->taps - 1; i >= offset1; i--)
		y += fir->coeffs[i] * fir->history[i - offset1];
	for (; i >= 0; i--)
		y += fir->coeffs[i] * fir->history[i + offset2];
	if (fir->curr_pos <= 0)
		fir->curr_pos = fir->taps;
	fir->curr_pos--;
	return (int16_t) (y >> 15);
}

#endif
