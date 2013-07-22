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
 * The stuff which is common for many tests.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UBI_VOLUME_PATTERN "/dev/ubi%d_%d"
#define MIN_AVAIL_EBS 5
#define PAGE_SIZE 4096

#define min(a, b) ((a) < (b) ? (a) : (b))

/* Normal messages */
#define normsg(fmt, ...) do {                                                  \
        printf(TESTNAME ": " fmt "\n", ##__VA_ARGS__);                         \
} while(0)

#define errmsg(fmt, ...) ({                                                    \
	__errmsg(TESTNAME, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__);        \
	-1;                                                                    \
})

#define failed(name) ({                                                        \
	__failed(TESTNAME, __FUNCTION__, __LINE__, name);                      \
	-1;                                                                    \
})

#define initial_check(argc, argv)                                              \
	__initial_check(TESTNAME, argc, argv)

#define check_volume(vol_id, req)                                              \
	__check_volume(libubi, &dev_info, TESTNAME, __FUNCTION__,              \
		       __LINE__, vol_id, req)

#define check_vol_patt(node, byte)                                             \
	__check_vol_patt(libubi, TESTNAME, __FUNCTION__, __LINE__, node, byte)

#define update_vol_patt(node, bytes, byte)                                     \
	__update_vol_patt(libubi, TESTNAME, __FUNCTION__, __LINE__,            \
			  node, bytes, byte)

#define check_failed(ret, error, func, fmt, ...) ({                            \
	int __ret;                                                             \
		                                                               \
	if (!ret) {                                                            \
		errmsg("%s() returned success but should have failed", func);  \
		errmsg(fmt, ##__VA_ARGS__);                                    \
		__ret = -1;                                                    \
	}                                                                      \
	if (errno != (error)) {                                                \
		errmsg("%s failed with error %d (%s), expected %d (%s)",       \
		       func, errno, strerror(errno), error, strerror(error));  \
		errmsg(fmt, ##__VA_ARGS__);                                    \
		__ret = -1;                                                    \
	}                                                                      \
	__ret = 0;                                                             \
})

/* Alignments to test, @s is eraseblock size */
#define ALIGNMENTS(s)                                                          \
	{3, 5, 27, 666, 512, 1024, 2048, (s)/2-3, (s)/2-2, (s)/2-1, (s)/2+1,   \
	 (s)/2+2, (s)/2+3, (s)/3-3, (s)/3-2, (s)/3-1, (s)/3+1, (s)/3+2,        \
	 (s)/3+3, (s)/4-3, (s)/4-2, (s)/4-1, (s)/4+1, (s)/4+2, (s)/4+3,        \
	 (s)/5-3, (s)/5-2, (s)/5-1, (s)/5+1, (s)/5+2, (s)/5+3, (s)-17, (s)-9,  \
	 (s)-8, (s)-6, (s)-4, (s)-1, (s)};

extern int seed_random_generator(void);

extern void __errmsg(const char *test, const char *func, int line,
		     const char *fmt, ...);
extern void __failed(const char *test, const char *func, int line,
		     const char *failed);
extern int __initial_check(const char *test, int argc, char * const argv[]);
extern int __check_volume(libubi_t libubi, struct ubi_dev_info *dev_info,
			  const char *test, const char *func, int line,
			  int vol_id, const struct ubi_mkvol_request *req);
extern int __check_vol_patt(libubi_t libubi, const char *test, const char *func,
			    int line, const char *node, uint8_t byte);
extern int __update_vol_patt(libubi_t libubi, const char *test, const char *func,
			     int line, const char *node, long long bytes,
			     uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* !__COMMON_H__ */
