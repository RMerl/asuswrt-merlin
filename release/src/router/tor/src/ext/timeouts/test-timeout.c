#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "timeout.h"

#define THE_END_OF_TIME ((timeout_t)-1)

static int check_misc(void) {
	if (TIMEOUT_VERSION != timeout_version())
		return 1;
	if (TIMEOUT_V_REL != timeout_v_rel())
		return 1;
	if (TIMEOUT_V_API != timeout_v_api())
		return 1;
	if (TIMEOUT_V_ABI != timeout_v_abi())
		return 1;
	if (strcmp(timeout_vendor(), TIMEOUT_VENDOR))
		return 1;
	return 0;
}

static int check_open_close(timeout_t hz_set, timeout_t hz_expect) {
	int err=0;
	struct timeouts *tos = timeouts_open(hz_set, &err);
	if (!tos)
		return 1;
	if (err)
		return 1;
	if (hz_expect != timeouts_hz(tos))
		return 1;
	timeouts_close(tos);
	return 0;
}

/* Not very random */
static timeout_t random_to(timeout_t min, timeout_t max)
{
	if (max <= min)
		return min;
	/* Not actually all that random, but should exercise the code. */
	timeout_t rand64 = random() * (timeout_t)INT_MAX + random();
	return min + (rand64 % (max-min));
}

/* configuration for check_randomized */
struct rand_cfg {
	/* When creating timeouts, smallest possible delay */
	timeout_t min_timeout;
	/* When creating timeouts, largest possible delay */
	timeout_t max_timeout;
	/* First time to start the clock at. */
	timeout_t start_at;
	/* Do not advance the clock past this time. */
	timeout_t end_at;
	/* Number of timeouts to create and monitor. */
	int n_timeouts;
	/* Advance the clock by no more than this each step. */
	timeout_t max_step;
	/* Use relative timers and stepping */
	int relative;
	/* Every time the clock ticks, try removing this many timeouts at
	 * random. */
	int try_removing;
	/* When we're done, advance the clock to the end of time. */
	int finalize;
};

static int check_randomized(const struct rand_cfg *cfg)
{
#define FAIL() do {					\
		printf("Failure on line %d\n", __LINE__);	\
		goto done;					\
	} while (0)

	int i, err;
	int rv = 1;
	struct timeout *t = calloc(cfg->n_timeouts, sizeof(struct timeout));
	timeout_t *timeouts = calloc(cfg->n_timeouts, sizeof(timeout_t));
	uint8_t *fired = calloc(cfg->n_timeouts, sizeof(uint8_t));
        uint8_t *found = calloc(cfg->n_timeouts, sizeof(uint8_t));
	uint8_t *deleted = calloc(cfg->n_timeouts, sizeof(uint8_t));
	struct timeouts *tos = timeouts_open(0, &err);
	timeout_t now = cfg->start_at;
	int n_added_pending = 0, cnt_added_pending = 0;
	int n_added_expired = 0, cnt_added_expired = 0;
        struct timeouts_it it_p, it_e, it_all;
        int p_done = 0, e_done = 0, all_done = 0;
	struct timeout *to = NULL;
	const int rel = cfg->relative;

	if (!t || !timeouts || !tos || !fired || !found || !deleted)
		FAIL();
	timeouts_update(tos, cfg->start_at);

	for (i = 0; i < cfg->n_timeouts; ++i) {
		if (&t[i] != timeout_init(&t[i], rel ? 0 : TIMEOUT_ABS))
			FAIL();
		if (timeout_pending(&t[i]))
			FAIL();
		if (timeout_expired(&t[i]))
			FAIL();

		timeouts[i] = random_to(cfg->min_timeout, cfg->max_timeout);

		timeouts_add(tos, &t[i], timeouts[i] - (rel ? now : 0));
		if (timeouts[i] <= cfg->start_at) {
			if (timeout_pending(&t[i]))
				FAIL();
			if (! timeout_expired(&t[i]))
				FAIL();
			++n_added_expired;
		} else {
			if (! timeout_pending(&t[i]))
				FAIL();
			if (timeout_expired(&t[i]))
				FAIL();
			++n_added_pending;
		}
	}

	if (!!n_added_pending != timeouts_pending(tos))
		FAIL();
	if (!!n_added_expired != timeouts_expired(tos))
		FAIL();

        /* Test foreach, interleaving a few iterators. */
        TIMEOUTS_IT_INIT(&it_p, TIMEOUTS_PENDING);
        TIMEOUTS_IT_INIT(&it_e, TIMEOUTS_EXPIRED);
        TIMEOUTS_IT_INIT(&it_all, TIMEOUTS_ALL);
        while (! (p_done && e_done && all_done)) {
		if (!p_done) {
			to = timeouts_next(tos, &it_p);
			if (to) {
				i = to - &t[0];
				++found[i];
				++cnt_added_pending;
			} else {
				p_done = 1;
			}
		}
		if (!e_done) {
			to = timeouts_next(tos, &it_e);
			if (to) {
				i = to - &t[0];
				++found[i];
				++cnt_added_expired;
			} else {
				e_done = 1;
			}
		}
		if (!all_done) {
			to = timeouts_next(tos, &it_all);
			if (to) {
				i = to - &t[0];
				++found[i];
			} else {
				all_done = 1;
			}
		}
        }

	for (i = 0; i < cfg->n_timeouts; ++i) {
		if (found[i] != 2)
			FAIL();
	}
	if (cnt_added_expired != n_added_expired)
		FAIL();
	if (cnt_added_pending != n_added_pending)
		FAIL();

	while (NULL != (to = timeouts_get(tos))) {
		i = to - &t[0];
		assert(&t[i] == to);
		if (timeouts[i] > cfg->start_at)
			FAIL(); /* shouldn't have happened yet */

		--n_added_expired; /* drop expired timeouts. */
		++fired[i];
	}

	if (n_added_expired != 0)
		FAIL();

	while (now < cfg->end_at) {
		int n_fired_this_time = 0;
		timeout_t first_at = timeouts_timeout(tos) + now;

		timeout_t oldtime = now;
		timeout_t step = random_to(1, cfg->max_step);
		int another;
		now += step;
		if (rel)
			timeouts_step(tos, step);
		else
			timeouts_update(tos, now);

		for (i = 0; i < cfg->try_removing; ++i) {
			int idx = random() % cfg->n_timeouts;
			if (! fired[idx]) {
				timeout_del(&t[idx]);
				++deleted[idx];
			}
		}

		another = (timeouts_timeout(tos) == 0);

		while (NULL != (to = timeouts_get(tos))) {
			if (! another)
				FAIL(); /* Thought we saw the last one! */
			i = to - &t[0];
			assert(&t[i] == to);
			if (timeouts[i] > now)
				FAIL(); /* shouldn't have happened yet */
			if (timeouts[i] <= oldtime)
				FAIL(); /* should have happened already */
			if (timeouts[i] < first_at)
				FAIL(); /* first_at should've been earlier */
			fired[i]++;
			n_fired_this_time++;
			another = (timeouts_timeout(tos) == 0);
		}
		if (n_fired_this_time && first_at > now)
			FAIL(); /* first_at should've been earlier */
		if (another)
			FAIL(); /* Huh? We think there are more? */
		if (!timeouts_check(tos, stderr))
			FAIL();
	}

	for (i = 0; i < cfg->n_timeouts; ++i) {
		if (fired[i] > 1)
			FAIL(); /* Nothing fired twice. */
		if (timeouts[i] <= now) {
			if (!(fired[i] || deleted[i]))
				FAIL();
		} else {
			if (fired[i])
				FAIL();
		}
		if (fired[i] && deleted[i])
			FAIL();
		if (cfg->finalize > 1) {
			if (!fired[i])
				timeout_del(&t[i]);
		}
	}

	/* Now nothing more should fire between now and the end of time. */
	if (cfg->finalize) {
		timeouts_update(tos, THE_END_OF_TIME);
		if (cfg->finalize > 1) {
			if (timeouts_get(tos))
				FAIL();
			TIMEOUTS_FOREACH(to, tos, TIMEOUTS_ALL)
				FAIL();
		}
	}
	rv = 0;

 done:
	if (tos) timeouts_close(tos);
	if (t) free(t);
	if (timeouts) free(timeouts);
	if (fired) free(fired);
	if (found) free(found);
	if (deleted) free(deleted);
	return rv;
}

struct intervals_cfg {
	const timeout_t *timeouts;
	int n_timeouts;
	timeout_t start_at;
	timeout_t end_at;
	timeout_t skip;
};

int
check_intervals(struct intervals_cfg *cfg)
{
	int i, err;
	int rv = 1;
	struct timeout *to;
	struct timeout *t = calloc(cfg->n_timeouts, sizeof(struct timeout));
	unsigned *fired = calloc(cfg->n_timeouts, sizeof(unsigned));
	struct timeouts *tos = timeouts_open(0, &err);

	timeout_t now = cfg->start_at;
	if (!t || !tos || !fired)
		FAIL();

	timeouts_update(tos, now);

	for (i = 0; i < cfg->n_timeouts; ++i) {
		if (&t[i] != timeout_init(&t[i], TIMEOUT_INT))
			FAIL();
		if (timeout_pending(&t[i]))
			FAIL();
		if (timeout_expired(&t[i]))
			FAIL();

		timeouts_add(tos, &t[i], cfg->timeouts[i]);
		if (! timeout_pending(&t[i]))
			FAIL();
		if (timeout_expired(&t[i]))
			FAIL();
	}

	while (now < cfg->end_at) {
		timeout_t delay = timeouts_timeout(tos);
		if (cfg->skip && delay < cfg->skip)
			delay = cfg->skip;
		timeouts_step(tos, delay);
		now += delay;

		while (NULL != (to = timeouts_get(tos))) {
			i = to - &t[0];
			assert(&t[i] == to);
			fired[i]++;
			if (0 != (to->expires - cfg->start_at) % cfg->timeouts[i])
				FAIL();
			if (to->expires <= now)
				FAIL();
			if (to->expires > now + cfg->timeouts[i])
				FAIL();
		}
		if (!timeouts_check(tos, stderr))
			FAIL();
	}

	timeout_t duration = now - cfg->start_at;
	for (i = 0; i < cfg->n_timeouts; ++i) {
		if (cfg->skip) {
			if (fired[i] > duration / cfg->timeouts[i])
				FAIL();
		} else {
			if (fired[i] != duration / cfg->timeouts[i])
				FAIL();
		}
		if (!timeout_pending(&t[i]))
			FAIL();
	}

	rv = 0;
 done:
	if (t) free(t);
	if (fired) free(fired);
	if (tos) free(tos);
	return rv;
}

int
main(int argc, char **argv)
{
	int j;
	int n_failed = 0;
#define DO(fn) do {                             \
		printf("."); fflush(stdout);	\
		if (fn) {			\
			++n_failed;		\
			printf("%s failed\n", #fn);	\
		}					\
        } while (0)

#define DO_N(n, fn) do {			\
		for (j = 0; j < (n); ++j) {	\
			DO(fn);			\
		}				\
	} while (0)

        DO(check_misc());
	DO(check_open_close(1000, 1000));
	DO(check_open_close(0, TIMEOUT_mHZ));

	struct rand_cfg cfg1 = {
		.min_timeout = 1,
		.max_timeout = 100,
		.start_at = 5,
		.end_at = 1000,
		.n_timeouts = 1000,
		.max_step = 10,
		.relative = 0,
		.try_removing = 0,
		.finalize = 2,
		};
	DO_N(300,check_randomized(&cfg1));

	struct rand_cfg cfg2 = {
		.min_timeout = 20,
		.max_timeout = 1000,
		.start_at = 10,
		.end_at = 100,
		.n_timeouts = 1000,
		.max_step = 5,
		.relative = 1,
		.try_removing = 0,
		.finalize = 2,
		};
	DO_N(300,check_randomized(&cfg2));

	struct rand_cfg cfg2b = {
		.min_timeout = 20,
		.max_timeout = 1000,
		.start_at = 10,
		.end_at = 100,
		.n_timeouts = 1000,
		.max_step = 5,
		.relative = 1,
		.try_removing = 0,
		.finalize = 1,
		};
	DO_N(300,check_randomized(&cfg2b));

	struct rand_cfg cfg2c = {
		.min_timeout = 20,
		.max_timeout = 1000,
		.start_at = 10,
		.end_at = 100,
		.n_timeouts = 1000,
		.max_step = 5,
		.relative = 1,
		.try_removing = 0,
		.finalize = 0,
		};
	DO_N(300,check_randomized(&cfg2c));

	struct rand_cfg cfg3 = {
		.min_timeout = 2000,
		.max_timeout = ((uint64_t)1) << 50,
		.start_at = 100,
		.end_at = ((uint64_t)1) << 49,
		.n_timeouts = 1000,
		.max_step = 1<<31,
		.relative = 0,
		.try_removing = 0,
		.finalize = 2,
		};
	DO_N(10,check_randomized(&cfg3));

	struct rand_cfg cfg3b = {
		.min_timeout = ((uint64_t)1) << 50,
		.max_timeout = ((uint64_t)1) << 52,
		.start_at = 100,
		.end_at = ((uint64_t)1) << 53,
		.n_timeouts = 1000,
		.max_step = ((uint64_t)1)<<48,
		.relative = 0,
		.try_removing = 0,
		.finalize = 2,
		};
	DO_N(10,check_randomized(&cfg3b));

	struct rand_cfg cfg4 = {
		.min_timeout = 2000,
		.max_timeout = ((uint64_t)1) << 30,
		.start_at = 100,
		.end_at = ((uint64_t)1) << 26,
		.n_timeouts = 10000,
		.max_step = 1<<16,
		.relative = 0,
		.try_removing = 3,
		.finalize = 2,
		};
	DO_N(10,check_randomized(&cfg4));

	const timeout_t primes[] = {
		2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,
		59,61,67,71,73,79,83,89,97
	};
	const timeout_t factors_of_1337[] = {
		1, 7, 191, 1337
	};
	const timeout_t multiples_of_five[] = {
		5, 10, 15, 20, 25, 30, 35, 40, 45, 50
	};

	struct intervals_cfg icfg1 = {
		.timeouts = primes,
		.n_timeouts = sizeof(primes)/sizeof(timeout_t),
		.start_at = 50,
		.end_at = 5322,
		.skip = 0,
	};
	DO(check_intervals(&icfg1));

	struct intervals_cfg icfg2 = {
		.timeouts = factors_of_1337,
		.n_timeouts = sizeof(factors_of_1337)/sizeof(timeout_t),
		.start_at = 50,
		.end_at = 50000,
		.skip = 0,
	};
	DO(check_intervals(&icfg2));

	struct intervals_cfg icfg3 = {
		.timeouts = multiples_of_five,
		.n_timeouts = sizeof(multiples_of_five)/sizeof(timeout_t),
		.start_at = 49,
		.end_at = 5333,
		.skip = 0,
	};
	DO(check_intervals(&icfg3));

	struct intervals_cfg icfg4 = {
		.timeouts = primes,
		.n_timeouts = sizeof(primes)/sizeof(timeout_t),
		.start_at = 50,
		.end_at = 5322,
		.skip = 16,
	};
	DO(check_intervals(&icfg4));

        if (n_failed) {
          puts("\nFAIL");
        } else {
          puts("\nOK");
        }
	return !!n_failed;
}

/* TODO:

 * Solve PR#3.

 * Investigate whether any untaken branches are possible.

 */
