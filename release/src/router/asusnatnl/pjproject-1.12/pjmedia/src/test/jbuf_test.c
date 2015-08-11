/* $Id: jbuf_test.c 3814 2011-10-13 09:02:41Z nanang $ */
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
#include <stdio.h>
#include <ctype.h>
#include <pj/pool.h>
#include "test.h"

#define JB_INIT_PREFETCH    0
#define JB_MIN_PREFETCH	    0
#define JB_MAX_PREFETCH	    10
#define JB_PTIME	    20
#define JB_BUF_SIZE	    50

//#define REPORT
//#define PRINT_COMMENT

typedef struct test_param_t {
    pj_bool_t adaptive;
    unsigned init_prefetch;
    unsigned min_prefetch;
    unsigned max_prefetch;
} test_param_t;

typedef struct test_cond_t {
    int burst;
    int discard;
    int lost;
    int empty;
    int delay;	    /**< Average delay, in frames.	    */
    int delay_min;  /**< Minimum delay, in frames.	    */
} test_cond_t;

static pj_bool_t parse_test_headers(char *line, test_param_t *param, 
			       test_cond_t *cond)
{
    char *p = line;

    if (*p == '%') {
	/* Test params. */
	char mode_st[16];

	sscanf(p+1, "%s %u %u %u", mode_st, &param->init_prefetch, 
	       &param->min_prefetch, &param->max_prefetch);
	param->adaptive = (pj_ansi_stricmp(mode_st, "adaptive") == 0);

    } else if (*p == '!') {
	/* Success condition. */
	char cond_st[16];
	unsigned cond_val;

	sscanf(p+1, "%s %u", cond_st, &cond_val);
	if (pj_ansi_stricmp(cond_st, "burst") == 0)
	    cond->burst = cond_val;
	else if (pj_ansi_stricmp(cond_st, "delay") == 0)
	    cond->delay = cond_val;
	else if (pj_ansi_stricmp(cond_st, "delay_min") == 0)
	    cond->delay_min = cond_val;
	else if (pj_ansi_stricmp(cond_st, "discard") == 0)
	    cond->discard = cond_val;
	else if (pj_ansi_stricmp(cond_st, "empty") == 0)
	    cond->empty = cond_val;
	else if (pj_ansi_stricmp(cond_st, "lost") == 0)
	    cond->lost = cond_val;

    } else if (*p == '=') {
	/* Test title. */
	++p;
	while (*p && isspace(*p)) ++p;
	printf("%s", p);

    } else if (*p == '#') {
	/* Ignore comment line. */

    } else {
	/* Unknown header, perhaps this is the test data */

	/* Skip spaces */
	while (*p && isspace(*p)) ++p;

	/* Test data started.*/
	if (*p != 0)
	    return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t process_test_data(char data, pjmedia_jbuf *jb, 
				   pj_uint16_t *seq, pj_uint16_t *last_seq)
{
    char frame[1];
    char f_type;
    pj_bool_t print_state = PJ_TRUE;
    pj_bool_t data_eos = PJ_FALSE;

    switch (toupper(data)) {
    case 'G': /* Get */
	pjmedia_jbuf_get_frame(jb, frame, &f_type);
	break;
    case 'P': /* Put */
	pjmedia_jbuf_put_frame(jb, (void*)frame, 1, *seq);
	*last_seq = *seq;
	++*seq;
	break;
    case 'L': /* Lost */
	*last_seq = *seq;
	++*seq;
	printf("Lost\n");
	break;
    case 'R': /* Sequence restarts */
	*seq = 1;
	printf("Sequence restarting, from %u to %u\n", *last_seq, *seq);
	break;
    case 'J': /* Sequence jumps */
	(*seq) += 20;
	printf("Sequence jumping, from %u to %u\n", *last_seq, *seq);
	break;
    case 'D': /* Frame duplicated */
	pjmedia_jbuf_put_frame(jb, (void*)frame, 1, *seq - 1);
	break;
    case 'O': /* Old/late frame */
	pjmedia_jbuf_put_frame(jb, (void*)frame, 1, *seq - 10 - pj_rand()%40);
	break;
    case '.': /* End of test session. */
	data_eos = PJ_TRUE;
	break;
    default:
	print_state = PJ_FALSE;
	printf("Unknown test data '%c'\n", data);
	break;
    }

    if (data_eos)
	return PJ_FALSE;

#ifdef REPORT
    if (print_state) {
	pjmedia_jb_state state;

	pjmedia_jbuf_get_state(jb, &state);
	printf("seq=%d\t%c\tsize=%d\tprefetch=%d\n", 
	       *last_seq, toupper(data), state.size, state.prefetch);
    }
#endif

    return PJ_TRUE;
}

int jbuf_main(void)
{
    FILE *input;
    pj_bool_t data_eof = PJ_FALSE;
    int old_log_level;
    int rc = 0;
    const char* input_filename = "Jbtest.dat";
    const char* input_search_path[] = { 
	"../build",
	"pjmedia/build",
	"build"
    };

    /* Try to open test data file in the working directory */
    input = fopen(input_filename, "rt");

    /* If that fails, try to open test data file in specified search paths */
    if (input == NULL) {
	char input_path[PJ_MAXPATH];
	int i;

	for (i = 0; !input && i < PJ_ARRAY_SIZE(input_search_path); ++i) {
	    pj_ansi_snprintf(input_path, PJ_MAXPATH, "%s/%s",
			     input_search_path[i],
			     input_filename);
	    input = fopen(input_path, "rt");
	}
    }
    
    /* Failed to open test data file. */
    if (input == NULL) {
	printf("Failed to open test data file, Jbtest.dat\n");
	return -1;
    }

    old_log_level = pj_log_get_level(0);
    pj_log_set_level(0, 5);

    while (rc == 0 && !data_eof) {
	pj_str_t jb_name = {"JBTEST", 6};
	pjmedia_jbuf *jb;
	pj_pool_t *pool;
	pjmedia_jb_state state;
	pj_uint16_t last_seq = 0;
	pj_uint16_t seq = 1;
	char line[1024], *p = NULL;

	test_param_t param;
	test_cond_t cond;

	param.adaptive = PJ_TRUE;
	param.init_prefetch = JB_INIT_PREFETCH;
	param.min_prefetch = JB_MIN_PREFETCH;
	param.max_prefetch = JB_MAX_PREFETCH;

	cond.burst = -1;
	cond.delay = -1;
	cond.delay_min = -1;
	cond.discard = -1;
	cond.empty = -1;
	cond.lost = -1;

	printf("\n\n");

	/* Parse test session title, param, and conditions */
	do {
	    p = fgets(line, sizeof(line), input);
	} while (p && parse_test_headers(line, &param, &cond));

	/* EOF test data */
	if (p == NULL)
	    break;

	//printf("======================================================\n");

	/* Initialize test session */
	pool = pj_pool_create(mem, "JBPOOL", 256*16, 256*16, NULL);
	pjmedia_jbuf_create(pool, &jb_name, 1, JB_PTIME, JB_BUF_SIZE, &jb);
	pjmedia_jbuf_reset(jb);

	if (param.adaptive) {
	    pjmedia_jbuf_set_adaptive(jb, 
				      param.init_prefetch, 
				      param.min_prefetch,
				      param.max_prefetch);
	} else {
	    pjmedia_jbuf_set_fixed(jb, param.init_prefetch);
	}

#ifdef REPORT
	pjmedia_jbuf_get_state(jb, &state);
	printf("Initial\tsize=%d\tprefetch=%d\tmin.pftch=%d\tmax.pftch=%d\n", 
	       state.size, state.prefetch, state.min_prefetch, 
	       state.max_prefetch);
#endif


	/* Test session start */
	while (1) {
	    char c;
	    
	    /* Get next line of test data */
	    if (!p || *p == 0) {
		p = fgets(line, sizeof(line), input);
		if (p == NULL) {
		    data_eof = PJ_TRUE;
		    break;
		}
	    }

	    /* Get next char of test data */
	    c = *p++;

	    /* Skip spaces */
	    if (isspace(c))
		continue;

	    /* Print comment line */
	    if (c == '#') {
#ifdef PRINT_COMMENT
		while (*p && isspace(*p)) ++p;
		if (*p) printf("..%s", p);
#endif
		*p = 0;
		continue;
	    }

	    /* Process test data */
	    if (!process_test_data(c, jb, &seq, &last_seq))
		break;
	}

	/* Print JB states */
	pjmedia_jbuf_get_state(jb, &state);
	printf("------------------------------------------------------\n");
	printf("Summary:\n");
	printf("  size=%d prefetch=%d\n", state.size, state.prefetch);
	printf("  delay (min/max/avg/dev)=%d/%d/%d/%d ms\n",
	       state.min_delay, state.max_delay, state.avg_delay, 
	       state.dev_delay);
	printf("  lost=%d discard=%d empty=%d burst(avg)=%d\n", 
	       state.lost, state.discard, state.empty, state.avg_burst);

	/* Evaluate test session */
	if (cond.burst >= 0 && (int)state.avg_burst > cond.burst) {
	    printf("! 'Burst' should be %d, it is %d\n", 
		   cond.burst, state.avg_burst);
	    rc |= 1;
	}
	if (cond.delay >= 0 && (int)state.avg_delay/JB_PTIME > cond.delay) {
	    printf("! 'Delay' should be %d, it is %d\n", 
		   cond.delay, state.avg_delay/JB_PTIME);
	    rc |= 2;
	}
	if (cond.delay_min >= 0 && (int)state.min_delay/JB_PTIME > cond.delay_min) {
	    printf("! 'Minimum delay' should be %d, it is %d\n", 
		   cond.delay_min, state.min_delay/JB_PTIME);
	    rc |= 32;
	}
	if (cond.discard >= 0 && (int)state.discard > cond.discard) {
	    printf("! 'Discard' should be %d, it is %d\n",
		   cond.discard, state.discard);
	    rc |= 4;
	}
	if (cond.empty >= 0 && (int)state.empty > cond.empty) {
	    printf("! 'Empty' should be %d, it is %d\n", 
		   cond.empty, state.empty);
	    rc |= 8;
	}
	if (cond.lost >= 0 && (int)state.lost > cond.lost) {
	    printf("! 'Lost' should be %d, it is %d\n", 
		   cond.lost, state.lost);
	    rc |= 16;
	}

	pjmedia_jbuf_destroy(jb);
	pj_pool_release(pool);
    }

    fclose(input);
    pj_log_set_level(0, old_log_level);

    return rc;
}
