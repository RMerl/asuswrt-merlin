/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Artem B. Bityutskiy
 *
 * This test creates and deletes volumes in parallel.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "libubi.h"
#define TESTNAME "mkvol_paral"
#include "common.h"

#define THREADS_NUM 4
#define ITERATIONS  500

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;
static int iterations = ITERATIONS;

/**
 * the_thread - the testing thread.
 *
 * @ptr  thread number
 */
static void * the_thread(void *ptr)
{
	int n = (long)ptr, iter = iterations;
	struct ubi_mkvol_request req;
	const char *name =  TESTNAME ":the_thread()";
	char nm[strlen(name) + 50];

	req.alignment = 1;
	req.bytes = dev_info.avail_bytes/ITERATIONS;
	req.vol_type = UBI_DYNAMIC_VOLUME;
	sprintf(nm, "%s:%d", name, n);
	req.name = nm;

	while (iter--) {
		req.vol_id = UBI_VOL_NUM_AUTO;
		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			return NULL;
		}
		if (ubi_rmvol(libubi, node, req.vol_id)) {
			failed("ubi_rmvol");
			return NULL;
		}
	}

	return NULL;
}

int main(int argc, char * const argv[])
{
	int i, ret;
	pthread_t threads[THREADS_NUM];

	if (initial_check(argc, argv))
		return 1;

	node = argv[1];

	libubi = libubi_open();
	if (libubi == NULL) {
		failed("libubi_open");
		return 1;
	}

	if (ubi_get_dev_info(libubi, node, &dev_info)) {
		failed("ubi_get_dev_info");
		goto close;
	}

	for (i = 0; i < THREADS_NUM; i++) {
		ret = pthread_create(&threads[i], NULL, &the_thread, (void*)(long)i);
		if (ret) {
			failed("pthread_create");
			goto close;
		}
	}

	for (i = 0; i < THREADS_NUM; i++)
		pthread_join(threads[i], NULL);

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}
