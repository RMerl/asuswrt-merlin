/* $Id: tonegen.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/tonegen.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/pool.h>

/* amplitude */
#define AMP	PJMEDIA_TONEGEN_VOLUME

#ifndef M_PI
#   define M_PI  ((DATA)3.141592653589793238462643383279)
#endif

#if PJMEDIA_TONEGEN_ALG==PJMEDIA_TONEGEN_SINE
    #include <math.h>
    #define DATA	double

    /*
     * This is the good old tone generator using sin().
     * Speed = 1347 usec to generate 1 second, 8KHz dual-tones (2.66GHz P4).
     *         approx. 10.91 MIPS
     *
     *         506,535 usec/100.29 MIPS on ARM926EJ-S.
     */
    struct gen
    {
	DATA add;
	DATA c;
	DATA vol;
    };

    #define GEN_INIT(var,R,F,A) var.add = ((DATA)F)/R, var.c=0, var.vol=A
    #define GEN_SAMP(val,var)   val = (short)(sin(var.c * 2 * M_PI) * \
					      var.vol); \
			        var.c += var.add

#elif PJMEDIA_TONEGEN_ALG==PJMEDIA_TONEGEN_FLOATING_POINT
    #include <math.h>
    #define DATA	float

    /*
     * Default floating-point based tone generation using sine wave 
     * generation from:
     *   http://www.musicdsp.org/showone.php?id=10.
     * This produces good quality tone in relatively faster time than
     * the normal sin() generator.
     * Speed = 350 usec to generate 1 second, 8KHz dual-tones (2.66GHz P4).
     *         approx. 2.84 MIPS
     *
     *         18,037 usec/3.57 MIPS on ARM926EJ-S.
     */
    struct gen
    {
	DATA a, s0, s1;
    };

    #define GEN_INIT(var,R,F,A) var.a = (DATA) (2.0 * sin(M_PI * F / R)); \
			        var.s0 = 0; \
			        var.s1 = (DATA)(0 - (int)A)
    #define GEN_SAMP(val,var)   var.s0 = var.s0 - var.a * var.s1; \
			        var.s1 = var.s1 + var.a * var.s0; \
			        val = (short) var.s0

#elif PJMEDIA_TONEGEN_ALG==PJMEDIA_TONEGEN_FIXED_POINT_CORDIC
    /* Cordic algorithm with 28 bit size, from:
     * http://www.dcs.gla.ac.uk/~jhw/cordic/
     * Speed = 742 usec to generate 1 second, 8KHz dual-tones (2.66GHz P4).
     *         (PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP=7)
     *         approx. 6.01 MIPS
     *
     *         ARM926EJ-S results:
     *	        loop=7:   8,943 usec/1.77 MIPS
     *		loop=8:   9,872 usec/1.95 MIPS
     *          loop=10: 11,662 usec/2.31 MIPS
     *          loop=12: 13,561 usec/2.69 MIPS
     */
    #define CORDIC_1K		0x026DD3B6
    #define CORDIC_HALF_PI	0x06487ED5
    #define CORDIC_PI		(CORDIC_HALF_PI * 2)
    #define CORDIC_MUL_BITS	26
    #define CORDIC_MUL		(1 << CORDIC_MUL_BITS)
    #define CORDIC_NTAB		28
    #define CORDIC_LOOP		PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP

    static int cordic_ctab [] = 
    {
	0x03243F6A, 0x01DAC670, 0x00FADBAF, 0x007F56EA, 0x003FEAB7, 
	0x001FFD55, 0x000FFFAA, 0x0007FFF5, 0x0003FFFE, 0x0001FFFF, 
	0x0000FFFF, 0x00007FFF, 0x00003FFF, 0x00001FFF, 0x00000FFF, 
	0x000007FF, 0x000003FF, 0x000001FF, 0x000000FF, 0x0000007F, 
	0x0000003F, 0x0000001F, 0x0000000F, 0x00000007, 0x00000003, 
	0x00000001, 0x00000000, 0x00000000 
    };

    static pj_int32_t cordic(pj_int32_t theta, unsigned n)
    {
	unsigned k;
	int d;
	pj_int32_t tx;
	pj_int32_t x = CORDIC_1K, y = 0, z = theta;

	for (k=0; k<n; ++k) {
	    #if 0
	    d = (z>=0) ? 0 : -1;
	    #else
	    /* Only slightly (~2.5%) faster, but not portable? */
	     d = z>>27;
	    #endif
	    tx = x - (((y>>k) ^ d) - d);
	    y = y + (((x>>k) ^ d) - d);
	    z = z - ((cordic_ctab[k] ^ d) - d);
	    x = tx;
	}  
	return y;
    }

    /* Note: theta must be uint32 here */
    static pj_int32_t cordic_sin(pj_uint32_t theta, unsigned n)
    {
	if (theta < CORDIC_HALF_PI)
	    return cordic(theta, n);
	else if (theta < CORDIC_PI)
	    return cordic(CORDIC_HALF_PI-(theta-CORDIC_HALF_PI), n);
	else if (theta < CORDIC_PI + CORDIC_HALF_PI)
	    return -cordic(theta - CORDIC_PI, n);
	else if (theta < 2 * CORDIC_PI)
	    return -cordic(CORDIC_HALF_PI-(theta-3*CORDIC_HALF_PI), n);
	else {
	    pj_assert(!"Invalid cordic_sin() value");
	    return 0;
	}
    }

    struct gen
    {
	unsigned    add;
	pj_uint32_t c;
	unsigned    vol;
    };

    #define VOL(var,v)		(((v) * var.vol) >> 15)
    #define GEN_INIT(var,R,F,A)	gen_init(&var, R, F, A)
    #define GEN_SAMP(val,var)	val = gen_samp(&var)

    static void gen_init(struct gen *var, unsigned R, unsigned F, unsigned A)
    {
	var->add = 2*CORDIC_PI/R * F;
	var->c = 0;
	var->vol = A;
    }

    PJ_INLINE(short) gen_samp(struct gen *var)
    {
	pj_int32_t val;
	val = cordic_sin(var->c, CORDIC_LOOP);
	/*val = (val * 32767) / CORDIC_MUL;
	 *val = VOL((*var), val);
	 */
	val = ((val >> 10) * var->vol) >> 16;
	var->c += var->add;
	if (var->c > 2*CORDIC_PI)
	    var->c -= (2 * CORDIC_PI);
	return (short) val;
    }

#elif PJMEDIA_TONEGEN_ALG==PJMEDIA_TONEGEN_FAST_FIXED_POINT

    /* 
     * Fallback algorithm when floating point is disabled.
     * This is a very fast fixed point tone generation using sine wave
     * approximation from
     *    http://www.audiomulch.com/~rossb/code/sinusoids/ 
     * Quality wise not so good, but it's blazing fast!
     * Speed = 117 usec to generate 1 second, 8KHz dual-tones (2.66GHz P4).
     *         approx. 0.95 MIPS
     *
     *         1,449 usec/0.29 MIPS on ARM926EJ-S.
     */
    PJ_INLINE(int) approximate_sin3(unsigned x)
    {	
	    unsigned s=-(int)(x>>31);
	    x+=x;
	    x=x>>16;
	    x*=x^0xffff;            // x=x*(2-x)
	    x+=x;                   // optional
	    return x^s;
    }
    struct gen
    {
	unsigned add;
	unsigned c;
	unsigned vol;
    };

    #define MAXI		((unsigned)0xFFFFFFFF)
    #define SIN			approximate_sin3
    #define VOL(var,v)		(((v) * var.vol) >> 15)
    #define GEN_INIT(var,R,F,A)	var.add = MAXI/R * F, var.c=0, var.vol=A
    #define GEN_SAMP(val,var)	val = (short) VOL(var,SIN(var.c)>>16); \
				var.c += var.add

#else
    #error "PJMEDIA_TONEGEN_ALG is not set correctly"
#endif

struct gen_state
{
    struct gen tone1;
    struct gen tone2;
    pj_bool_t  has_tone2;
};


static void init_generate_single_tone(struct gen_state *state,
				      unsigned clock_rate, 
				      unsigned freq,
				      unsigned vol)
{
    GEN_INIT(state->tone1,clock_rate,freq,vol);
    state->has_tone2 = PJ_FALSE;
}

static void generate_single_tone(struct gen_state *state,
				 unsigned channel_count,
				 unsigned samples,
				 short buf[]) 
{
    short *end = buf + samples;

    if (channel_count==1) {

	while (buf < end) {
	    GEN_SAMP(*buf++, state->tone1);
	}

    } else if (channel_count == 2) {

	while (buf < end) {
	    GEN_SAMP(*buf, state->tone1);
	    *(buf+1) = *buf;
	    buf += 2;
	}
    }
}


static void init_generate_dual_tone(struct gen_state *state,
				    unsigned clock_rate, 
				    unsigned freq1,
				    unsigned freq2,
				    unsigned vol)
{
    GEN_INIT(state->tone1,clock_rate,freq1,vol);
    GEN_INIT(state->tone2,clock_rate,freq2,vol);
    state->has_tone2 = PJ_TRUE;
}


static void generate_dual_tone(struct gen_state *state,
			       unsigned channel_count,
			       unsigned samples,
			       short buf[]) 
{
    short *end = buf + samples;

    if (channel_count==1) {
	int val, val2;
	while (buf < end) {
	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    *buf++ = (short)((val+val2) >> 1);
	}
    } else if (channel_count == 2) {
	int val, val2;
	while (buf < end) {

	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    val = (val + val2) >> 1;

	    *buf++ = (short)val;
	    *buf++ = (short)val;
	}
    }
}


static void init_generate_tone(struct gen_state *state,
			       unsigned clock_rate, 
			       unsigned freq1,
			       unsigned freq2,
			       unsigned vol)
{
    if (freq2)
	init_generate_dual_tone(state, clock_rate, freq1, freq2 ,vol);
    else
	init_generate_single_tone(state, clock_rate, freq1,vol);
}


static void generate_tone(struct gen_state *state,
			  unsigned channel_count,
			  unsigned samples,
			  short buf[])
{
    if (!state->has_tone2)
	generate_single_tone(state, channel_count, samples, buf);
    else
	generate_dual_tone(state, channel_count, samples, buf);
}


/****************************************************************************/

#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('t', 'n', 'g', 'n')
#define THIS_FILE   "tonegen.c"

#if 0
#   define TRACE_(expr)	PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif

enum flags
{
    PJMEDIA_TONE_INITIALIZED	= 1,
    PJMEDIA_TONE_ENABLE_FADE	= 2
};

struct tonegen
{
    pjmedia_port	base;

    /* options */
    unsigned		options;
    unsigned		playback_options;
    unsigned		fade_in_len;	/* fade in for this # of samples */
    unsigned		fade_out_len;	/* fade out for this # of samples*/

    /* lock */
    pj_lock_t	       *lock;

    /* Digit map */
    pjmedia_tone_digit_map  *digit_map;

    /* Tone generation state */
    struct gen_state	state;

    /* Currently played digits: */
    unsigned		count;		    /* # of digits		*/
    unsigned		cur_digit;	    /* currently played		*/
    unsigned		dig_samples;	    /* sample pos in cur digit	*/
    pjmedia_tone_desc	digits[PJMEDIA_TONEGEN_MAX_DIGITS];/* array of digits*/
};


/* Default digit map is DTMF */
static pjmedia_tone_digit_map digit_map = 
{
    16,
    {
	{ '0', 941,  1336 },
	{ '1', 697,  1209 },
	{ '2', 697,  1336 },
	{ '3', 697,  1477 },
	{ '4', 770,  1209 },
	{ '5', 770,  1336 },
	{ '6', 770,  1477 },
	{ '7', 852,  1209 },
	{ '8', 852,  1336 },
	{ '9', 852,  1477 },
	{ 'a', 697,  1633 },
	{ 'b', 770,  1633 },
	{ 'c', 852,  1633 },
	{ 'd', 941,  1633 },
	{ '*', 941,  1209 },
	{ '#', 941,  1477 },
    }
};


static pj_status_t tonegen_get_frame(pjmedia_port *this_port, 
				     pjmedia_frame *frame);
static pj_status_t tonegen_destroy(pjmedia_port *this_port);

/*
 * Create an instance of tone generator with the specified parameters.
 * When the tone generator is first created, it will be loaded with the
 * default digit map.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_create2(pj_pool_t *pool,
					    const pj_str_t *name,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    unsigned options,
					    pjmedia_port **p_port)
{
    const pj_str_t STR_TONE_GEN = pj_str("tonegen");
    struct tonegen  *tonegen;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && clock_rate && channel_count && 
		     samples_per_frame && bits_per_sample == 16 && 
		     p_port != NULL, PJ_EINVAL);

    /* Only support mono and stereo */
    PJ_ASSERT_RETURN(channel_count==1 || channel_count==2, PJ_EINVAL);

    /* Create and initialize port */
    tonegen = PJ_POOL_ZALLOC_T(pool, struct tonegen);
    if (name == NULL || name->slen == 0) name = &STR_TONE_GEN;
    status = pjmedia_port_info_init(&tonegen->base.info, name, 
				    SIGNATURE, clock_rate, channel_count, 
				    bits_per_sample, samples_per_frame);
    if (status != PJ_SUCCESS)
	return status;

    tonegen->options = options;
    tonegen->base.get_frame = &tonegen_get_frame;
    tonegen->base.on_destroy = &tonegen_destroy;
    tonegen->digit_map = &digit_map;

    tonegen->fade_in_len = PJMEDIA_TONEGEN_FADE_IN_TIME * clock_rate / 1000;
    tonegen->fade_out_len = PJMEDIA_TONEGEN_FADE_OUT_TIME * clock_rate / 1000;

    /* Lock */
    if (options & PJMEDIA_TONEGEN_NO_LOCK) {
	status = pj_lock_create_null_mutex(pool, "tonegen", &tonegen->lock);
    } else {
	status = pj_lock_create_simple_mutex(pool, "tonegen", &tonegen->lock);
    }

    if (status != PJ_SUCCESS) {
	return status;
    }

    TRACE_((THIS_FILE, "Tonegen created: %u/%u/%u/%u", clock_rate, 
	    channel_count, samples_per_frame, bits_per_sample));

    /* Done */
    *p_port = &tonegen->base;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_tonegen_create( pj_pool_t *pool,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    unsigned options,
					    pjmedia_port **p_port)
{
    return pjmedia_tonegen_create2(pool, NULL, clock_rate, channel_count,
				   samples_per_frame, bits_per_sample, 
				   options, p_port);
}


/*
 * Check if the tone generator is still busy producing some tones.
 */
PJ_DEF(pj_bool_t) pjmedia_tonegen_is_busy(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_TRUE);
    return tonegen->count != 0;
}


/*
 * Instruct the tone generator to stop current processing.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_stop(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);

    TRACE_((THIS_FILE, "tonegen_stop()"));

    pj_lock_acquire(tonegen->lock);
    tonegen->count = 0;
    tonegen->cur_digit = 0;
    tonegen->dig_samples = 0;
    pj_lock_release(tonegen->lock);

    return PJ_SUCCESS;
}


/*
 * Instruct the tone generator to stop current processing.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_rewind(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);

    TRACE_((THIS_FILE, "tonegen_rewind()"));

    /* Reset back to the first tone */
    pj_lock_acquire(tonegen->lock);
    tonegen->cur_digit = 0;
    tonegen->dig_samples = 0;
    pj_lock_release(tonegen->lock);

    return PJ_SUCCESS;
}


/*
 * Callback to destroy tonegen
 */
static pj_status_t tonegen_destroy(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);

    TRACE_((THIS_FILE, "tonegen_destroy()"));

    pj_lock_acquire(tonegen->lock);
    pj_lock_release(tonegen->lock);

    pj_lock_destroy(tonegen->lock);

    return PJ_SUCCESS;
}

/*
 * Fill a frame with tones.
 */
static pj_status_t tonegen_get_frame(pjmedia_port *port, 
				     pjmedia_frame *frame)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    short *dst, *end;
    unsigned clock_rate = tonegen->base.info.clock_rate;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);

    pj_lock_acquire(tonegen->lock);

    if (tonegen->count == 0) {
	/* We don't have digits to play */
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	goto on_return;
    }

    if (tonegen->cur_digit > tonegen->count) {
	/* We have played all the digits */
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	    TRACE_((THIS_FILE, "tonegen_get_frame(): rewind"));

	} else {
	    tonegen->count = 0;
	    tonegen->cur_digit = 0;
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    TRACE_((THIS_FILE, "tonegen_get_frame(): no more digit"));
	    goto on_return;
	}
    }

    if (tonegen->dig_samples>=(tonegen->digits[tonegen->cur_digit].on_msec+
			       tonegen->digits[tonegen->cur_digit].off_msec)*
			       clock_rate / 1000)
    {
	/* We have finished with current digit */
	tonegen->cur_digit++;
	tonegen->dig_samples = 0;

	TRACE_((THIS_FILE, "tonegen_get_frame(): next digit"));
    }

    if (tonegen->cur_digit >= tonegen->count) {
	/* After we're finished with the last digit, we have played all 
	 * the digits 
	 */
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	    TRACE_((THIS_FILE, "tonegen_get_frame(): rewind"));

	} else {
	    tonegen->count = 0;
	    tonegen->cur_digit = 0;
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    TRACE_((THIS_FILE, "tonegen_get_frame(): no more digit"));
	    goto on_return;
	}
    }
    
    dst = (short*) frame->buf;
    end = dst + port->info.samples_per_frame;

    while (dst < end) {
	pjmedia_tone_desc *dig = &tonegen->digits[tonegen->cur_digit];
	unsigned required, cnt, on_samp, off_samp;

	required = end - dst;
	on_samp = dig->on_msec * clock_rate / 1000;
	off_samp = dig->off_msec * clock_rate / 1000;

	/* Init tonegen */
	if (tonegen->dig_samples == 0 && 
	    (tonegen->count!=1 || !(dig->flags & PJMEDIA_TONE_INITIALIZED)))
	{
	    init_generate_tone(&tonegen->state, port->info.clock_rate,
			       dig->freq1, dig->freq2, dig->volume);
	    dig->flags |= PJMEDIA_TONE_INITIALIZED;
	    if (tonegen->cur_digit > 0) {
		/* Clear initialized flag of previous digit */
		tonegen->digits[tonegen->cur_digit-1].flags &= 
						(~PJMEDIA_TONE_INITIALIZED);
	    }
	}

	/* Add tone signal */
	if (tonegen->dig_samples < on_samp) {
	    cnt = on_samp - tonegen->dig_samples;
	    if (cnt > required)
		cnt = required;
	    generate_tone(&tonegen->state, port->info.channel_count,
			  cnt, dst);

	    dst += cnt;
	    tonegen->dig_samples += cnt;
	    required -= cnt;

	    if ((dig->flags & PJMEDIA_TONE_ENABLE_FADE) && 
		tonegen->dig_samples == cnt) 
	    {
		/* Fade in */
		short *samp = (dst - cnt);
		short *end;

		if (cnt > tonegen->fade_in_len)
		    cnt = tonegen->fade_in_len;
		end = samp + cnt;
		if (cnt) {
		    const unsigned step = 0xFFFF / cnt;
		    unsigned scale = 0;

		    for (; samp < end; ++samp) {
			(*samp) = (short)(((*samp) * scale) >> 16);
			scale += step;
		    }
		}
	    } else if ((dig->flags & PJMEDIA_TONE_ENABLE_FADE) &&
			tonegen->dig_samples==on_samp) 
	    {
		/* Fade out */
		if (cnt > tonegen->fade_out_len)
		    cnt = tonegen->fade_out_len;
		if (cnt) {
		    short *samp = (dst - cnt);
		    const unsigned step = 0xFFFF / cnt;
		    unsigned scale = 0xFFFF - step;

		    for (; samp < dst; ++samp) {
			(*samp) = (short)(((*samp) * scale) >> 16);
			scale -= step;
		    }
		}
	    }

	    if (dst == end)
		break;
	}

	/* Add silence signal */
	cnt = off_samp + on_samp - tonegen->dig_samples;
	if (cnt > required)
	    cnt = required;
	pjmedia_zero_samples(dst, cnt);
	dst += cnt;
	tonegen->dig_samples += cnt;

	/* Move to next digit if we're finished with this tone */
	if (tonegen->dig_samples >= on_samp + off_samp) {
	    tonegen->cur_digit++;
	    tonegen->dig_samples = 0;

	    if (tonegen->cur_digit >= tonegen->count) {
		/* All digits have been played */
		if ((tonegen->options & PJMEDIA_TONEGEN_LOOP) ||
		    (tonegen->playback_options & PJMEDIA_TONEGEN_LOOP))
		{
		    tonegen->cur_digit = 0;
		} else {
		    break;
		}
	    }
	}
    }

    if (dst < end)
	pjmedia_zero_samples(dst, end-dst);

    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = port->info.bytes_per_frame;

    TRACE_((THIS_FILE, "tonegen_get_frame(): frame created, level=%u",
	    pjmedia_calc_avg_signal((pj_int16_t*)frame->buf, frame->size/2)));

    if (tonegen->cur_digit >= tonegen->count) {
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	    TRACE_((THIS_FILE, "tonegen_get_frame(): rewind"));

	} else {
	    tonegen->count = 0;
	    tonegen->cur_digit = 0;

	    TRACE_((THIS_FILE, "tonegen_get_frame(): no more digit"));
	}
    }

on_return:
    pj_lock_release(tonegen->lock);
    return PJ_SUCCESS;
}


/*
 * Play tones.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_play( pjmedia_port *port,
					  unsigned count,
					  const pjmedia_tone_desc tones[],
					  unsigned options)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    unsigned i;

    PJ_ASSERT_RETURN(port && port->info.signature == SIGNATURE &&
		     count && tones, PJ_EINVAL);

    /* Don't put more than available buffer */
    PJ_ASSERT_RETURN(count+tonegen->count <= PJMEDIA_TONEGEN_MAX_DIGITS,
		     PJ_ETOOMANY);

    pj_lock_acquire(tonegen->lock);

    /* Set playback options */
    tonegen->playback_options = options;
    
    /* Copy digits */
    pj_memcpy(tonegen->digits + tonegen->count,
	      tones, count * sizeof(pjmedia_tone_desc));
    
    /* Normalize volume, and check if we need to disable fading.
     * Disable fading if tone off time is zero. Application probably
     * wants to play this tone continuously (e.g. dial tone).
     */
    for (i=0; i<count; ++i) {
	pjmedia_tone_desc *t = &tonegen->digits[i+tonegen->count];
	if (t->volume == 0)
	    t->volume = AMP;
	else if (t->volume < 0)
	    t->volume = (short) -t->volume;
	/* Reset flags */
	t->flags = 0;
	if (t->off_msec != 0)
	    t->flags |= PJMEDIA_TONE_ENABLE_FADE;
    }

    tonegen->count += count;

    pj_lock_release(tonegen->lock);

    return PJ_SUCCESS;
}


/*
 * Play digits.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_play_digits( pjmedia_port *port,
						 unsigned count,
						 const pjmedia_tone_digit digits[],
						 unsigned options)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    pjmedia_tone_desc tones[PJMEDIA_TONEGEN_MAX_DIGITS];
    const pjmedia_tone_digit_map *map;
    unsigned i;

    PJ_ASSERT_RETURN(port && port->info.signature == SIGNATURE &&
		     count && digits, PJ_EINVAL);
    PJ_ASSERT_RETURN(count < PJMEDIA_TONEGEN_MAX_DIGITS, PJ_ETOOMANY);

    pj_lock_acquire(tonegen->lock);

    map = tonegen->digit_map;

    for (i=0; i<count; ++i) {
	int d = pj_tolower(digits[i].digit);
	unsigned j;

	/* Translate ASCII digits with digitmap */
	for (j=0; j<map->count; ++j) {
	    if (d == map->digits[j].digit)
		break;
	}
	if (j == map->count) {
	    pj_lock_release(tonegen->lock);
	    return PJMEDIA_RTP_EINDTMF;
	}

	tones[i].freq1 = map->digits[j].freq1;
	tones[i].freq2 = map->digits[j].freq2;
	tones[i].on_msec = digits[i].on_msec;
	tones[i].off_msec = digits[i].off_msec;
	tones[i].volume = digits[i].volume;
    }

    pj_lock_release(tonegen->lock);

    return pjmedia_tonegen_play(port, count, tones, options);
}


/*
 * Get the digit-map currently used by this tone generator.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_get_digit_map(pjmedia_port *port,
						  const pjmedia_tone_digit_map **m)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);
    PJ_ASSERT_RETURN(m != NULL, PJ_EINVAL);

    *m = tonegen->digit_map;

    return PJ_SUCCESS;
}


/*
 * Set digit map to be used by the tone generator.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_set_digit_map(pjmedia_port *port,
						  pjmedia_tone_digit_map *m)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);
    PJ_ASSERT_RETURN(m != NULL, PJ_EINVAL);

    pj_lock_acquire(tonegen->lock);

    tonegen->digit_map = m;

    pj_lock_release(tonegen->lock);

    return PJ_SUCCESS;
}


