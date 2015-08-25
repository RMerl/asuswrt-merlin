/* $Id: wsola_test.c 3550 2011-05-05 05:33:27Z nanang $ */
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
#include <pjmedia/wsola.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <stdio.h>
#include <assert.h>

#define CLOCK_RATE	    16000
#define SAMPLES_PER_FRAME   (10 * CLOCK_RATE / 1000)

#define RESET()	    memset(buf1, 0, sizeof(buf1)), \
		    memset(buf2, 0, sizeof(buf2)), \
		    memset(frm1, 0, sizeof(frm1)), \
		    memset(frm2, 0, sizeof(frm2))

#if 0
void test_find_pitch(void)
{
    enum { ON = 111, FRM_PART_LEN=20 };
    short buf2[SAMPLES_PER_FRAME*2], buf1[SAMPLES_PER_FRAME*2],
	  frm2[SAMPLES_PER_FRAME], frm1[SAMPLES_PER_FRAME];
    short *ref, *pos;

    /* Case 1. all contiguous */
    RESET();
    ref = buf1 + 10;
    *ref = ON;
    frm1[0] = ON;

    pos = pjmedia_wsola_find_pitch(frm1, SAMPLES_PER_FRAME, NULL, 0,
				   buf1, SAMPLES_PER_FRAME*2, NULL, 0, PJ_TRUE);
    assert(pos == ref);

    /* Case 2: contiguous buffer, non-contiguous frame */
    RESET();
    ref = buf1 + 17;
    *ref = ON;
    *(ref+FRM_PART_LEN) = ON;
    frm1[0] = ON;
    frm2[0] = ON;

    /* Noise */
    buf1[0] = ON;

    pos = pjmedia_wsola_find_pitch(frm1, FRM_PART_LEN, frm2, SAMPLES_PER_FRAME - FRM_PART_LEN,
				   buf1, SAMPLES_PER_FRAME*2, NULL, 0, PJ_TRUE);
    assert(pos == ref);

    /* Case 3: non-contiguous buffer, contiguous frame, found in buf1 */
    RESET();
    ref = buf1 + 17;
    *ref = ON;
    buf2[17] = ON;
    frm1[0] = ON;
    frm1[FRM_PART_LEN] = ON;

    /* Noise */
    buf1[0] = ON;

    pos = pjmedia_wsola_find_pitch(frm1, SAMPLES_PER_FRAME, NULL, 0,
				   buf1, FRM_PART_LEN,
				   buf2, SAMPLES_PER_FRAME,
				   PJ_TRUE);

    assert(pos == ref);
}
#endif

int expand(pj_pool_t *pool, const char *filein, const char *fileout,
	   int expansion_rate100, int lost_rate10, int lost_burst)
{
    enum { LOST_RATE = 10 };
    FILE *in, *out;
    short frame[SAMPLES_PER_FRAME];
    pjmedia_wsola *wsola;
    pj_timestamp elapsed, zero;
    unsigned samples;
    int last_lost = 0;

    /* Lost burst must be > 0 */
    assert(lost_rate10==0 || lost_burst > 0);

    in = fopen(filein, "rb");
    if (!in) return 1;
    out = fopen(fileout, "wb");
    if (!out) return 1;

    pjmedia_wsola_create(pool, CLOCK_RATE, SAMPLES_PER_FRAME, 1, 0, &wsola);

    samples = 0;
    elapsed.u64 = 0;

    while (fread(frame, SAMPLES_PER_FRAME*2, 1, in) == 1) {
	pj_timestamp t1, t2;

	if (lost_rate10 == 0) {

	    /* Expansion */
	    pj_get_timestamp(&t1);
	    pjmedia_wsola_save(wsola, frame, 0);
	    pj_get_timestamp(&t2);

	    pj_sub_timestamp(&t2, &t1);
	    pj_add_timestamp(&elapsed, &t2);

	    fwrite(frame, SAMPLES_PER_FRAME*2, 1, out);

	    samples += SAMPLES_PER_FRAME;

	    if ((rand() % 100) < expansion_rate100) {

		pj_get_timestamp(&t1);
		pjmedia_wsola_generate(wsola, frame);
		pj_get_timestamp(&t2);

		pj_sub_timestamp(&t2, &t1);
		pj_add_timestamp(&elapsed, &t2);
    
		samples += SAMPLES_PER_FRAME;

		fwrite(frame, SAMPLES_PER_FRAME*2, 1, out);
	    } 

	} else {
	    /* Lost */

	    if ((rand() % 10) < lost_rate10) {
		int burst;

		for (burst=0; burst<lost_burst; ++burst) {
		    pj_get_timestamp(&t1);
		    pjmedia_wsola_generate(wsola, frame);
		    pj_get_timestamp(&t2);

		    pj_sub_timestamp(&t2, &t1);
		    pj_add_timestamp(&elapsed, &t2);

		    samples += SAMPLES_PER_FRAME;

		    fwrite(frame, SAMPLES_PER_FRAME*2, 1, out);
		}
		last_lost = 1;
	    } else {
		pj_get_timestamp(&t1);
		pjmedia_wsola_save(wsola, frame, last_lost);
		pj_get_timestamp(&t2);

		pj_sub_timestamp(&t2, &t1);
		pj_add_timestamp(&elapsed, &t2);

		samples += SAMPLES_PER_FRAME;

		fwrite(frame, SAMPLES_PER_FRAME*2, 1, out);
		last_lost = 0;
	    }

	}

    }

    zero.u64 = 0;
    zero.u64 = pj_elapsed_usec(&zero, &elapsed);
    
    zero.u64 = samples * PJ_INT64(1000000) / zero.u64;
    assert(zero.u32.hi == 0);

    PJ_LOG(3,("test.c", "Processing: %f Msamples per second", 
	      zero.u32.lo/1000000.0));
    PJ_LOG(3,("test.c", "CPU load for current settings: %f%%",
	      CLOCK_RATE * 100.0 / zero.u32.lo));

    pjmedia_wsola_destroy(wsola);
    fclose(in);
    fclose(out);

    return 0;
}

static void save_file(const char *file, 
		      short frame[], unsigned count)
{
    FILE *f = fopen(file, "wb");
    fwrite(frame, count, 2, f);
    fclose(f);
}

int compress(pj_pool_t *pool, 
	     const char *filein, const char *fileout, 
	     int rate10)
{
    enum { BUF_CNT = SAMPLES_PER_FRAME * 10 };
    FILE *in, *out;
    pjmedia_wsola *wsola;
    short buf[BUF_CNT];
    pj_timestamp elapsed, zero;
    unsigned samples = 0;
    
    in = fopen(filein, "rb");
    if (!in) return 1;
    out = fopen(fileout, "wb");
    if (!out) return 1;

    pjmedia_wsola_create(pool, CLOCK_RATE, SAMPLES_PER_FRAME, 1, 0, &wsola);

    elapsed.u64 = 0;

    for (;;) {
	unsigned size_del, count;
	pj_timestamp t1, t2;
	int i;

	if (fread(buf, sizeof(buf), 1, in) != 1)
	    break;

	count = BUF_CNT;
	size_del = 0;
	pj_get_timestamp(&t1);

	for (i=0; i<rate10; ++i) {
	    unsigned to_del = SAMPLES_PER_FRAME;
#if 0
	    /* Method 1: buf1 contiguous */
	    pjmedia_wsola_discard(wsola, buf, count, NULL, 0, &to_del);
#elif 0
	    /* Method 2: split, majority in buf1 */
	    assert(count > SAMPLES_PER_FRAME);
	    pjmedia_wsola_discard(wsola, buf, count-SAMPLES_PER_FRAME, 
				  buf+count-SAMPLES_PER_FRAME, SAMPLES_PER_FRAME, 
				  &to_del);
#elif 0
	    /* Method 3: split, majority in buf2 */
	    assert(count > SAMPLES_PER_FRAME);
	    pjmedia_wsola_discard(wsola, buf, SAMPLES_PER_FRAME, 
				  buf+SAMPLES_PER_FRAME, count-SAMPLES_PER_FRAME, 
				  &to_del);
#elif 1
	    /* Method 4: split, each with small length */
	    enum { TOT_LEN = 3 * SAMPLES_PER_FRAME };
	    unsigned buf1_len = (rand() % TOT_LEN);
	    short *ptr = buf + count - TOT_LEN;
	    assert(count > TOT_LEN);
	    if (buf1_len==0) buf1_len=SAMPLES_PER_FRAME*2;
	    pjmedia_wsola_discard(wsola, ptr, buf1_len, 
				  ptr+buf1_len, TOT_LEN-buf1_len, 
				  &to_del);
#endif
	    count -= to_del;
	    size_del += to_del;
	}
	pj_get_timestamp(&t2);
	
	samples += BUF_CNT;

	pj_sub_timestamp(&t2, &t1);
	pj_add_timestamp(&elapsed, &t2);

	assert(size_del >= SAMPLES_PER_FRAME);

	fwrite(buf, count, 2, out);
    }

    pjmedia_wsola_destroy(wsola);
    fclose(in);
    fclose(out);

    zero.u64 = 0;
    zero.u64 = pj_elapsed_usec(&zero, &elapsed);
    
    zero.u64 = samples * PJ_INT64(1000000) / zero.u64;
    assert(zero.u32.hi == 0);

    PJ_LOG(3,("test.c", "Processing: %f Msamples per second", 
	      zero.u32.lo/1000000.0));
    PJ_LOG(3,("test.c", "CPU load for current settings: %f%%",
	      CLOCK_RATE * 100.0 / zero.u32.lo));

    return 0;
}


static void mem_test(pj_pool_t *pool)
{
    char unused[1024];
    short   *frame = pj_pool_alloc(pool, 240+4*160);
    pj_timestamp elapsed, zero, t1, t2;
    unsigned samples = 0;

    elapsed.u64 = 0;
    while (samples < 50000000) {

	pj_get_timestamp(&t1);
	pjmedia_move_samples(frame, frame+160, 240+2*160);
	pj_get_timestamp(&t2);
	pj_sub_timestamp(&t2, &t1);

	elapsed.u64 += t2.u64;

	memset(unused, 0, sizeof(unused));
	samples += 160;
    }
    

    

    zero.u64 = 0;
    zero.u64 = pj_elapsed_usec(&zero, &elapsed);
    
    zero.u64 = samples * PJ_INT64(1000000) / zero.u64;
    assert(zero.u32.hi == 0);

    PJ_LOG(3,("test.c", "Processing: %f Msamples per second", 
	      zero.u32.lo/1000000.0));
    PJ_LOG(3,("test.c", "CPU load for current settings: %f%%",
	      CLOCK_RATE * 100.0 / zero.u32.lo));

}

int main()
{
    pj_caching_pool cp;
    pj_pool_t *pool;
    int i, rc;

    //test_find_pitch();

    pj_init();
    pj_caching_pool_init(&cp, NULL, 0);
    pool = pj_pool_create(&cp.factory, "", 1000, 1000, NULL);

    srand(2);

    rc = expand(pool, "galileo16.pcm", "temp1.pcm", 20, 0, 0);
    rc = compress(pool, "temp1.pcm", "output.pcm", 1);

    for (i=0; i<2; ++i) {
	rc = expand(pool, "output.pcm", "temp1.pcm", 20, 0, 0);
	rc = compress(pool, "temp1.pcm", "output.pcm", 1);
    }

    if (rc != 0) {
	puts("Error");
	return 1;
    }

#if 0
    {
	char s[10];
	puts("Press ENTER to quit");
	fgets(s, sizeof(s), stdin);
    }
#endif

    return 0;
}
