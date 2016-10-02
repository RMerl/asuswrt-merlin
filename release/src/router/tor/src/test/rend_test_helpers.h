/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"

#ifndef TOR_REND_TEST_HELPERS_H
#define TOR_REND_TEST_HELPERS_H

void generate_desc(int time_diff, rend_encoded_v2_service_descriptor_t **desc,
                   char **service_id, int intro_points);
void create_descriptor(rend_service_descriptor_t **generated,
                       char **service_id, int intro_points);

#endif

