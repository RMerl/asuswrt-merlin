/* Copyright (c) 2014-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TEST_HELPERS_H
#define TOR_TEST_HELPERS_H

const char *get_yesterday_date_str(void);

/* Number of descriptors contained in test_descriptors.txt. */
#define HELPER_NUMBER_OF_DESCRIPTORS 8

void helper_setup_fake_routerlist(void);

extern const char TEST_DESCRIPTORS[];

#endif

