/*
 * Printing backend for the build farm
 *
 * Copyright (C) Volker Lendecke 2006
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "printing.h"

#if defined(DEVELOPER) || defined(ENABLE_BUILD_FARM_HACKS)

static int test_queue_get(const char *printer_name,
			  enum printing_types printing_type,
			  char *lpq_command,
			  print_queue_struct **q,
			  print_status_struct *status)
{
	return -1;
}

static int test_queue_pause(int snum)
{
	return -1;
}

static int test_queue_resume(int snum)
{
	return -1;
}

static int test_job_delete(const char *sharename, const char *lprm_command,
			   struct printjob *pjob)
{
	return -1;
}

static int test_job_pause(int snum, struct printjob *pjob)
{
	return -1;
}

static int test_job_resume(int snum, struct printjob *pjob)
{
	return -1;
}

static int test_job_submit(int snum, struct printjob *pjob)
{
	return -1;
};

struct printif test_printif =
{
	PRINT_TEST,
	test_queue_get,
	test_queue_pause,
	test_queue_resume,
	test_job_delete,
	test_job_pause,
	test_job_resume,
	test_job_submit,
};

#else
 /* this keeps fussy compilers happy */
 void print_test_dummy(void);
 void print_test_dummy(void) {}
#endif /* DEVELOPER||ENABLE_BUILD_FARM_HACKS */
