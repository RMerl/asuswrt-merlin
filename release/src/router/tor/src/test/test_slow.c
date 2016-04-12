/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_slow.c
 * \brief Slower unit tests for many pieces of the lower level Tor modules.
 **/

#include "orconfig.h"

#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "or.h"
#include "test.h"

extern struct testcase_t slow_crypto_tests[];
extern struct testcase_t slow_util_tests[];

struct testgroup_t testgroups[] = {
  { "slow/crypto/", slow_crypto_tests },
  { "slow/util/", slow_util_tests },
  END_OF_GROUPS
};

