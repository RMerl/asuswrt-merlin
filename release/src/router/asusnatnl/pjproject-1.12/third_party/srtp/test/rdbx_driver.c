/*
 * rdbx_driver.c
 *
 * driver for the rdbx implementation (replay database with extended range)
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>    /* for printf()          */
#include "getopt_s.h" /* for local getopt()    */

#include "rdbx.h"

#ifdef ROC_TEST
#error "rdbx_t won't work with ROC_TEST - bitmask same size as seq_median"
#endif

#include "ut_sim.h"

err_status_t 
test_replay_dbx(int num_trials);

double
rdbx_check_adds_per_second(int num_trials);

void
usage(char *prog_name) {
  printf("usage: %s [ -t | -v ]\n", prog_name);
  exit(255);
}

int
main (int argc, char *argv[]) {
  double rate;
  err_status_t status;
  char q;
  unsigned do_timing_test = 0;
  unsigned do_validation = 0;

  /* process input arguments */
  while (1) {
    q = getopt_s(argc, argv, "tv");
    if (q == -1) 
      break;
    switch (q) {
    case 't':
      do_timing_test = 1;
      break;
    case 'v':
      do_validation = 1;
      break;
    default:
      usage(argv[0]);
    }    
  }

  printf("rdbx (replay database w/ extended range) test driver\n"
	 "David A. McGrew\n"
	 "Cisco Systems, Inc.\n");

  if (!do_validation && !do_timing_test)
    usage(argv[0]);

  if (do_validation) {
  printf("testing rdbx_t...\n");

    status = test_replay_dbx(1 << 12);
    if (status) {
      printf("failed\n");
      exit(1);
    }
    printf("passed\n");
  }

  if (do_timing_test) {
    rate = rdbx_check_adds_per_second(1 << 18);
    printf("rdbx_check/replay_adds per second: %e\n", rate);
  }
  
  return 0;
}

void
print_rdbx(rdbx_t *rdbx) {
  printf("rdbx: {%llu, %s}\n",
	 (unsigned long long)(rdbx->index), v128_bit_string(&rdbx->bitmask));
}


/*
 * rdbx_check_add(rdbx, idx) checks a known-to-be-good idx against
 * rdbx, then adds it.  if a failure is detected (i.e., the check
 * indicates that the value is already in rdbx) then
 * err_status_algo_fail is returned.
 *
 */

err_status_t
rdbx_check_add(rdbx_t *rdbx, uint32_t idx) {
  int delta;
  xtd_seq_num_t est;
  
  delta = index_guess(&rdbx->index, &est, idx);
  
  if (rdbx_check(rdbx, delta) != err_status_ok) {
    printf("replay_check failed at index %u\n", idx);
    return err_status_algo_fail;
  }

  /*
   * in practice, we'd authenticate the packet containing idx, using
   * the estimated value est, at this point
   */
  
  if (rdbx_add_index(rdbx, delta) != err_status_ok) {
    printf("rdbx_add_index failed at index %u\n", idx);
    return err_status_algo_fail;
  }  

  return err_status_ok;
}

/*
 * rdbx_check_expect_failure(rdbx_t *rdbx, uint32_t idx)
 * 
 * checks that a sequence number idx is in the replay database
 * and thus will be rejected
 */

err_status_t
rdbx_check_expect_failure(rdbx_t *rdbx, uint32_t idx) {
  int delta;
  xtd_seq_num_t est;
  err_status_t status;

  delta = index_guess(&rdbx->index, &est, idx);

  status = rdbx_check(rdbx, delta);
  if (status == err_status_ok) {
    printf("delta: %d ", delta);
    printf("replay_check failed at index %u (false positive)\n", idx);
    return err_status_algo_fail; 
  }

  return err_status_ok;
}

err_status_t
rdbx_check_unordered(rdbx_t *rdbx, uint32_t idx) {
  err_status_t rstat;

  rstat = rdbx_check(rdbx, idx);
  if ((rstat != err_status_ok) && (rstat != err_status_replay_old)) {
    printf("replay_check_unordered failed at index %u\n", idx);
    return err_status_algo_fail;
  }
  return err_status_ok;
}

#define MAX_IDX 160

err_status_t
test_replay_dbx(int num_trials) {
  rdbx_t rdbx;
  uint32_t idx, ircvd;
  ut_connection utc;
  err_status_t status;
  int num_fp_trials;

  status = rdbx_init(&rdbx);
  if (status) {
    printf("replay_init failed with error code %d\n", status);
    exit(1);
  }

  /*
   *  test sequential insertion 
   */
  printf("\ttesting sequential insertion...");
  for (idx=0; idx < num_trials; idx++) {
    status = rdbx_check_add(&rdbx, idx);
    if (status)
      return status;
  }
  printf("passed\n");

  /*
   *  test for false positives by checking all of the index
   *  values which we've just added
   *
   * note that we limit the number of trials here, since allowing the
   * rollover counter to roll over would defeat this test
   */
  num_fp_trials = num_trials % 0x10000;
  if (num_fp_trials == 0) {
    printf("warning: no false positive tests performed\n");
  }
  printf("\ttesting for false positives...");
  for (idx=0; idx < num_fp_trials; idx++) {
    status = rdbx_check_expect_failure(&rdbx, idx);
    if (status)
      return status;
  }
  printf("passed\n");

  /* re-initialize */
  if (rdbx_init(&rdbx) != err_status_ok) {
    printf("replay_init failed\n");
    return err_status_init_fail;
  }

  /*
   * test non-sequential insertion 
   *
   * this test covers only fase negatives, since the values returned
   * by ut_next_index(...) are distinct
   */
  ut_init(&utc);

  printf("\ttesting non-sequential insertion...");  
  for (idx=0; idx < num_trials; idx++) {
    ircvd = ut_next_index(&utc);
    status = rdbx_check_unordered(&rdbx, ircvd);
    if (status)
      return status;
  }
  printf("passed\n");

  return err_status_ok;
}



#include <time.h>       /* for clock()  */
#include <stdlib.h>     /* for random() */

double
rdbx_check_adds_per_second(int num_trials) {
  uint32_t i;
  int delta;
  rdbx_t rdbx;
  xtd_seq_num_t est;
  clock_t timer;
  int failures;                    /* count number of failures        */
  
  if (rdbx_init(&rdbx) != err_status_ok) {
    printf("replay_init failed\n");
    exit(1);
  }  

  failures = 0;
  timer = clock();
  for(i=0; i < num_trials; i++) {
    
    delta = index_guess(&rdbx.index, &est, i);
    
    if (rdbx_check(&rdbx, delta) != err_status_ok) 
      ++failures;
    else
      if (rdbx_add_index(&rdbx, delta) != err_status_ok)
	++failures;
  }
  timer = clock() - timer;

  printf("number of failures: %d \n", failures);

  return (double) CLOCKS_PER_SEC * num_trials / timer;
}

