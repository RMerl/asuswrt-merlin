/* 
   Unix SMB/CIFS implementation.

   libreplace tests

   Copyright (C) Jelmer Vernooij 2006

     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "replace.h"

/*
  we include all the system/ include files here so that libreplace tests
  them in the build farm
*/
#include "system/capability.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "system/glob.h"
#include "system/iconv.h"
#include "system/locale.h"
#include "system/network.h"
#include "system/passwd.h"
#include "system/printing.h"
#include "system/readline.h"
#include "system/select.h"
#include "system/shmem.h"
#include "system/syslog.h"
#include "system/terminal.h"
#include "system/time.h"
#include "system/wait.h"
#include "system/aio.h"

#define TESTFILE "testfile.dat"

/*
  test ftruncate() function
 */
static int test_ftruncate(void)
{
	struct stat st;
	int fd;
	const int size = 1234;
	printf("test: ftruncate\n");
	unlink(TESTFILE);
	fd = open(TESTFILE, O_RDWR|O_CREAT, 0600);
	if (fd == -1) {
		printf("failure: ftruncate [\n"
			   "creating '%s' failed - %s\n]\n", TESTFILE, strerror(errno));
		return false;
	}
	if (ftruncate(fd, size) != 0) {
		printf("failure: ftruncate [\n%s\n]\n", strerror(errno));
		return false;
	}
	if (fstat(fd, &st) != 0) {
		printf("failure: ftruncate [\nfstat failed - %s\n]\n", strerror(errno));
		return false;
	}
	if (st.st_size != size) {
		printf("failure: ftruncate [\ngave wrong size %d - expected %d\n]\n",
		       (int)st.st_size, size);
		return false;
	}
	unlink(TESTFILE);
	printf("success: ftruncate\n");
	return true;
}

/*
  test strlcpy() function.
  see http://www.gratisoft.us/todd/papers/strlcpy.html
 */
static int test_strlcpy(void)
{
	char buf[4];
	const struct {
		const char *src;
		size_t result;
	} tests[] = {
		{ "abc", 3 },
		{ "abcdef", 6 },
		{ "abcd", 4 },
		{ "", 0 },
		{ NULL, 0 }
	};
	int i;
	printf("test: strlcpy\n");
	for (i=0;tests[i].src;i++) {
		if (strlcpy(buf, tests[i].src, sizeof(buf)) != tests[i].result) {
			printf("failure: strlcpy [\ntest %d failed\n]\n", i);
			return false;
		}
	}
	printf("success: strlcpy\n");
	return true;
}

static int test_strlcat(void)
{
	char tmp[10];
	printf("test: strlcat\n");
	strlcpy(tmp, "", sizeof(tmp));
	if (strlcat(tmp, "bla", 3) != 3) {
		printf("failure: strlcat [\ninvalid return code\n]\n");
		return false;
	}
	if (strcmp(tmp, "bl") != 0) {
		printf("failure: strlcat [\nexpected \"bl\", got \"%s\"\n]\n", 
			   tmp);
		return false;
	}

	strlcpy(tmp, "da", sizeof(tmp));
	if (strlcat(tmp, "me", 4) != 4) {
		printf("failure: strlcat [\nexpected \"dam\", got \"%s\"\n]\n",
			   tmp);
		return false;
	}

	printf("success: strlcat\n");
	return true;
}

static int test_mktime(void)
{
	/* FIXME */
	return true;
}

static int test_initgroups(void)
{
	/* FIXME */
	return true;
}

static int test_memmove(void)
{
	/* FIXME */
	return true;
}

static int test_strdup(void)
{
	char *x;
	printf("test: strdup\n");
	x = strdup("bla");
	if (strcmp("bla", x) != 0) {
		printf("failure: strdup [\nfailed: expected \"bla\", got \"%s\"\n]\n",
			   x);
		return false;
	}
	free(x);
	printf("success: strdup\n");
	return true;
}	

static int test_setlinebuf(void)
{
	printf("test: setlinebuf\n");
	setlinebuf(stdout);
	printf("success: setlinebuf\n");
	return true;
}

static int test_vsyslog(void)
{
	/* FIXME */
	return true;
}

static int test_timegm(void)
{
	/* FIXME */
	return true;
}

static int test_setenv(void)
{
#define TEST_SETENV(key, value, overwrite, result) do { \
	int _ret; \
	char *_v; \
	_ret = setenv(key, value, overwrite); \
	if (_ret != 0) { \
		printf("failure: setenv [\n" \
			"setenv(%s, %s, %d) failed\n" \
			"]\n", \
			key, value, overwrite); \
		return false; \
	} \
	_v=getenv(key); \
	if (!_v) { \
		printf("failure: setenv [\n" \
			"getenv(%s) returned NULL\n" \
			"]\n", \
			key); \
		return false; \
	} \
	if (strcmp(result, _v) != 0) { \
		printf("failure: setenv [\n" \
			"getenv(%s): '%s' != '%s'\n" \
			"]\n", \
			key, result, _v); \
		return false; \
	} \
} while(0)

#define TEST_UNSETENV(key) do { \
	char *_v; \
	unsetenv(key); \
	_v=getenv(key); \
	if (_v) { \
		printf("failure: setenv [\n" \
			"getenv(%s): NULL != '%s'\n" \
			"]\n", \
			SETENVTEST_KEY, _v); \
		return false; \
	} \
} while (0)

#define SETENVTEST_KEY "SETENVTESTKEY"
#define SETENVTEST_VAL "SETENVTESTVAL"

	printf("test: setenv\n");
	TEST_SETENV(SETENVTEST_KEY, SETENVTEST_VAL"1", 0, SETENVTEST_VAL"1");
	TEST_SETENV(SETENVTEST_KEY, SETENVTEST_VAL"2", 0, SETENVTEST_VAL"1");
	TEST_SETENV(SETENVTEST_KEY, SETENVTEST_VAL"3", 1, SETENVTEST_VAL"3");
	TEST_SETENV(SETENVTEST_KEY, SETENVTEST_VAL"4", 1, SETENVTEST_VAL"4");
	TEST_UNSETENV(SETENVTEST_KEY);
	TEST_UNSETENV(SETENVTEST_KEY);
	TEST_SETENV(SETENVTEST_KEY, SETENVTEST_VAL"5", 0, SETENVTEST_VAL"5");
	TEST_UNSETENV(SETENVTEST_KEY);
	TEST_UNSETENV(SETENVTEST_KEY);
	printf("success: setenv\n");
	return true;
}

static int test_strndup(void)
{
	char *x;
	printf("test: strndup\n");
	x = strndup("bla", 0);
	if (strcmp(x, "") != 0) {
		printf("failure: strndup [\ninvalid\n]\n");
		return false;
	}
	free(x);
	x = strndup("bla", 2);
	if (strcmp(x, "bl") != 0) {
		printf("failure: strndup [\ninvalid\n]\n");
		return false;
	}
	free(x);
	x = strndup("bla", 10);
	if (strcmp(x, "bla") != 0) {
		printf("failure: strndup [\ninvalid\n]\n");
		return false;
	}
	free(x);
	printf("success: strndup\n");
	return true;
}

static int test_strnlen(void)
{
	printf("test: strnlen\n");
	if (strnlen("bla", 2) != 2) {
		printf("failure: strnlen [\nunexpected length\n]\n");
		return false;
	}

	if (strnlen("some text\n", 0) != 0) {
		printf("failure: strnlen [\nunexpected length\n]\n");
		return false;
	}

	if (strnlen("some text", 20) != 9) {
		printf("failure: strnlen [\nunexpected length\n]\n");
		return false;
	}

	printf("success: strnlen\n");
	return true;
}

static int test_waitpid(void)
{
	/* FIXME */
	return true;
}

static int test_seteuid(void)
{
	/* FIXME */
	return true;
}

static int test_setegid(void)
{
	/* FIXME */
	return true;
}

static int test_asprintf(void)
{
	char *x;
	printf("test: asprintf\n");
	if (asprintf(&x, "%d", 9) != 1) {
		printf("failure: asprintf [\ngenerate asprintf\n]\n");
		return false;
	}
	if (strcmp(x, "9") != 0) {
		printf("failure: asprintf [\ngenerate asprintf\n]\n");
		return false;
	}
	if (asprintf(&x, "dat%s", "a") != 4) {
		printf("failure: asprintf [\ngenerate asprintf\n]\n");
		return false;
	}
	if (strcmp(x, "data") != 0) {
		printf("failure: asprintf [\ngenerate asprintf\n]\n");
		return false;
	}
	printf("success: asprintf\n");
	return true;
}

static int test_snprintf(void)
{
	char tmp[10];
	printf("test: snprintf\n");
	if (snprintf(tmp, 3, "foo%d", 9) != 4) {
		printf("failure: snprintf [\nsnprintf return code failed\n]\n");
		return false;
	}

	if (strcmp(tmp, "fo") != 0) {
		printf("failure: snprintf [\nsnprintf failed\n]\n");
		return false;
	}

	printf("success: snprintf\n");
	return true;
}

static int test_vasprintf(void)
{
	/* FIXME */
	return true;
}

static int test_vsnprintf(void)
{
	/* FIXME */
	return true;
}

static int test_opendir(void)
{
	/* FIXME */
	return true;
}

extern int test_readdir_os2_delete(void);

static int test_readdir(void)
{
	printf("test: readdir\n");
	if (test_readdir_os2_delete() != 0) {
		return false;
	}
	printf("success: readdir\n");
	return true;
}

static int test_telldir(void)
{
	/* FIXME */
	return true;
}

static int test_seekdir(void)
{
	/* FIXME */
	return true;
}

static int test_dlopen(void)
{
	/* FIXME: test dlopen, dlsym, dlclose, dlerror */
	return true;
}


static int test_chroot(void)
{
	/* FIXME: chroot() */
	return true;
}

static int test_bzero(void)
{
	/* FIXME: bzero */
	return true;
}

static int test_strerror(void)
{
	/* FIXME */
	return true;
}

static int test_errno(void)
{
	printf("test: errno\n");
	errno = 3;
	if (errno != 3) {
		printf("failure: errno [\nerrno failed\n]\n");
		return false;
	}

	printf("success: errno\n");
	return true;
}

static int test_mkdtemp(void)
{
	/* FIXME */
	return true;
}

static int test_mkstemp(void)
{
	/* FIXME */
	return true;
}

static int test_pread(void)
{
	/* FIXME */
	return true;
}

static int test_pwrite(void)
{
	/* FIXME */
	return true;
}

static int test_getpass(void)
{
	/* FIXME */
	return true;
}

static int test_inet_ntoa(void)
{
	/* FIXME */
	return true;
}

#define TEST_STRTO_X(type,fmt,func,str,base,res,diff,rrnoo) do {\
	type _v; \
	char _s[64]; \
	char *_p = NULL;\
	char *_ep = NULL; \
	strlcpy(_s, str, sizeof(_s));\
	if (diff >= 0) { \
		_ep = &_s[diff]; \
	} \
	errno = 0; \
	_v = func(_s, &_p, base); \
	if (errno != rrnoo) { \
		printf("failure: %s [\n" \
		       "\t%s\n" \
		       "\t%s(\"%s\",%d,%d): " fmt " (=/!)= " fmt "\n" \
		       "\terrno: %d != %d\n" \
		       "]\n", \
		        __STRING(func), __location__, __STRING(func), \
		       str, diff, base, res, _v, rrnoo, errno); \
		return false; \
	} else if (_v != res) { \
		printf("failure: %s [\n" \
		       "\t%s\n" \
		       "\t%s(\"%s\",%d,%d): " fmt " != " fmt "\n" \
		       "]\n", \
		       __STRING(func), __location__, __STRING(func), \
		       str, diff, base, res, _v); \
		return false; \
	} else if (_p != _ep) { \
		printf("failure: %s [\n" \
		       "\t%s\n" \
		       "\t%s(\"%s\",%d,%d): " fmt " (=/!)= " fmt "\n" \
		       "\tptr: %p - %p = %d != %d\n" \
		       "]\n", \
		       __STRING(func), __location__, __STRING(func), \
		       str, diff, base, res, _v, _ep, _p, (int)(diff - (_ep - _p)), diff); \
		return false; \
	} \
} while (0)

static int test_strtoll(void)
{
	printf("test: strtoll\n");

#define TEST_STRTOLL(str,base,res,diff,errnoo) TEST_STRTO_X(int64_t, "%lld", strtoll,str,base,res,diff,errnoo)

	TEST_STRTOLL("15",	10,	15LL,	2, 0);
	TEST_STRTOLL("  15",	10,	15LL,	4, 0);
	TEST_STRTOLL("15",	0,	15LL,	2, 0);
	TEST_STRTOLL(" 15 ",	0,	15LL,	3, 0);
	TEST_STRTOLL("+15",	10,	15LL,	3, 0);
	TEST_STRTOLL("  +15",	10,	15LL,	5, 0);
	TEST_STRTOLL("+15",	0,	15LL,	3, 0);
	TEST_STRTOLL(" +15 ",	0,	15LL,	4, 0);
	TEST_STRTOLL("-15",	10,	-15LL,	3, 0);
	TEST_STRTOLL("  -15",	10,	-15LL,	5, 0);
	TEST_STRTOLL("-15",	0,	-15LL,	3, 0);
	TEST_STRTOLL(" -15 ",	0,	-15LL,	4, 0);
	TEST_STRTOLL("015",	10,	15LL,	3, 0);
	TEST_STRTOLL("  015",	10,	15LL,	5, 0);
	TEST_STRTOLL("015",	0,	13LL,	3, 0);
	TEST_STRTOLL("  015",	0,	13LL,	5, 0);
	TEST_STRTOLL("0x15",	10,	0LL,	1, 0);
	TEST_STRTOLL("  0x15",	10,	0LL,	3, 0);
	TEST_STRTOLL("0x15",	0,	21LL,	4, 0);
	TEST_STRTOLL("  0x15",	0,	21LL,	6, 0);

	TEST_STRTOLL("10",	16,	16LL,	2, 0);
	TEST_STRTOLL("  10 ",	16,	16LL,	4, 0);
	TEST_STRTOLL("0x10",	16,	16LL,	4, 0);
	TEST_STRTOLL("0x10",	0,	16LL,	4, 0);
	TEST_STRTOLL(" 0x10 ",	0,	16LL,	5, 0);
	TEST_STRTOLL("+10",	16,	16LL,	3, 0);
	TEST_STRTOLL("  +10 ",	16,	16LL,	5, 0);
	TEST_STRTOLL("+0x10",	16,	16LL,	5, 0);
	TEST_STRTOLL("+0x10",	0,	16LL,	5, 0);
	TEST_STRTOLL(" +0x10 ",	0,	16LL,	6, 0);
	TEST_STRTOLL("-10",	16,	-16LL,	3, 0);
	TEST_STRTOLL("  -10 ",	16,	-16LL,	5, 0);
	TEST_STRTOLL("-0x10",	16,	-16LL,	5, 0);
	TEST_STRTOLL("-0x10",	0,	-16LL,	5, 0);
	TEST_STRTOLL(" -0x10 ",	0,	-16LL,	6, 0);
	TEST_STRTOLL("010",	16,	16LL,	3, 0);
	TEST_STRTOLL("  010 ",	16,	16LL,	5, 0);
	TEST_STRTOLL("-010",	16,	-16LL,	4, 0);

	TEST_STRTOLL("11",	8,	9LL,	2, 0);
	TEST_STRTOLL("011",	8,	9LL,	3, 0);
	TEST_STRTOLL("011",	0,	9LL,	3, 0);
	TEST_STRTOLL("-11",	8,	-9LL,	3, 0);
	TEST_STRTOLL("-011",	8,	-9LL,	4, 0);
	TEST_STRTOLL("-011",	0,	-9LL,	4, 0);

	TEST_STRTOLL("011",	8,	9LL,	3, 0);
	TEST_STRTOLL("011",	0,	9LL,	3, 0);
	TEST_STRTOLL("-11",	8,	-9LL,	3, 0);
	TEST_STRTOLL("-011",	8,	-9LL,	4, 0);
	TEST_STRTOLL("-011",	0,	-9LL,	4, 0);

	TEST_STRTOLL("Text",	0,	0LL,	0, 0);

	TEST_STRTOLL("9223372036854775807",	10,	9223372036854775807LL,	19, 0);
	TEST_STRTOLL("9223372036854775807",	0,	9223372036854775807LL,	19, 0);
	TEST_STRTOLL("9223372036854775808",	0,	9223372036854775807LL,	19, ERANGE);
	TEST_STRTOLL("9223372036854775808",	10,	9223372036854775807LL,	19, ERANGE);
	TEST_STRTOLL("0x7FFFFFFFFFFFFFFF",	0,	9223372036854775807LL,	18, 0);
	TEST_STRTOLL("0x7FFFFFFFFFFFFFFF",	16,	9223372036854775807LL,	18, 0);
	TEST_STRTOLL("7FFFFFFFFFFFFFFF",	16,	9223372036854775807LL,	16, 0);
	TEST_STRTOLL("0x8000000000000000",	0,	9223372036854775807LL,	18, ERANGE);
	TEST_STRTOLL("0x8000000000000000",	16,	9223372036854775807LL,	18, ERANGE);
	TEST_STRTOLL("80000000000000000",	16,	9223372036854775807LL,	17, ERANGE);
	TEST_STRTOLL("0777777777777777777777",	0,	9223372036854775807LL,	22, 0);
	TEST_STRTOLL("0777777777777777777777",	8,	9223372036854775807LL,	22, 0);
	TEST_STRTOLL("777777777777777777777",	8,	9223372036854775807LL,	21, 0);
	TEST_STRTOLL("01000000000000000000000",	0,	9223372036854775807LL,	23, ERANGE);
	TEST_STRTOLL("01000000000000000000000",	8,	9223372036854775807LL,	23, ERANGE);
	TEST_STRTOLL("1000000000000000000000",	8,	9223372036854775807LL,	22, ERANGE);

	TEST_STRTOLL("-9223372036854775808",	10,	-9223372036854775807LL -1,	20, 0);
	TEST_STRTOLL("-9223372036854775808",	0,	-9223372036854775807LL -1,	20, 0);
	TEST_STRTOLL("-9223372036854775809",	0,	-9223372036854775807LL -1,	20, ERANGE);
	TEST_STRTOLL("-9223372036854775809",	10,	-9223372036854775807LL -1,	20, ERANGE);
	TEST_STRTOLL("-0x8000000000000000",	0,	-9223372036854775807LL -1,	19, 0);
	TEST_STRTOLL("-0x8000000000000000",	16,	-9223372036854775807LL -1,	19, 0);
	TEST_STRTOLL("-8000000000000000",	16,	-9223372036854775807LL -1,	17, 0);
	TEST_STRTOLL("-0x8000000000000001",	0,	-9223372036854775807LL -1,	19, ERANGE);
	TEST_STRTOLL("-0x8000000000000001",	16,	-9223372036854775807LL -1,	19, ERANGE);
	TEST_STRTOLL("-80000000000000001",	16,	-9223372036854775807LL -1,	18, ERANGE);
	TEST_STRTOLL("-01000000000000000000000",0,	-9223372036854775807LL -1,	24, 0);
	TEST_STRTOLL("-01000000000000000000000",8,	-9223372036854775807LL -1,	24, 0);
	TEST_STRTOLL("-1000000000000000000000",	8,	-9223372036854775807LL -1,	23, 0);
	TEST_STRTOLL("-01000000000000000000001",0,	-9223372036854775807LL -1,	24, ERANGE);
	TEST_STRTOLL("-01000000000000000000001",8,	-9223372036854775807LL -1,	24, ERANGE);
	TEST_STRTOLL("-1000000000000000000001",	8,	-9223372036854775807LL -1,	23, ERANGE);

	printf("success: strtoll\n");
	return true;
}

static int test_strtoull(void)
{
	printf("test: strtoull\n");

#define TEST_STRTOULL(str,base,res,diff,errnoo) TEST_STRTO_X(uint64_t,"%llu",strtoull,str,base,res,diff,errnoo)

	TEST_STRTOULL("15",	10,	15LLU,	2, 0);
	TEST_STRTOULL("  15",	10,	15LLU,	4, 0);
	TEST_STRTOULL("15",	0,	15LLU,	2, 0);
	TEST_STRTOULL(" 15 ",	0,	15LLU,	3, 0);
	TEST_STRTOULL("+15",	10,	15LLU,	3, 0);
	TEST_STRTOULL("  +15",	10,	15LLU,	5, 0);
	TEST_STRTOULL("+15",	0,	15LLU,	3, 0);
	TEST_STRTOULL(" +15 ",	0,	15LLU,	4, 0);
	TEST_STRTOULL("-15",	10,	18446744073709551601LLU,	3, 0);
	TEST_STRTOULL("  -15",	10,	18446744073709551601LLU,	5, 0);
	TEST_STRTOULL("-15",	0,	18446744073709551601LLU,	3, 0);
	TEST_STRTOULL(" -15 ",	0,	18446744073709551601LLU,	4, 0);
	TEST_STRTOULL("015",	10,	15LLU,	3, 0);
	TEST_STRTOULL("  015",	10,	15LLU,	5, 0);
	TEST_STRTOULL("015",	0,	13LLU,	3, 0);
	TEST_STRTOULL("  015",	0,	13LLU,	5, 0);
	TEST_STRTOULL("0x15",	10,	0LLU,	1, 0);
	TEST_STRTOULL("  0x15",	10,	0LLU,	3, 0);
	TEST_STRTOULL("0x15",	0,	21LLU,	4, 0);
	TEST_STRTOULL("  0x15",	0,	21LLU,	6, 0);

	TEST_STRTOULL("10",	16,	16LLU,	2, 0);
	TEST_STRTOULL("  10 ",	16,	16LLU,	4, 0);
	TEST_STRTOULL("0x10",	16,	16LLU,	4, 0);
	TEST_STRTOULL("0x10",	0,	16LLU,	4, 0);
	TEST_STRTOULL(" 0x10 ",	0,	16LLU,	5, 0);
	TEST_STRTOULL("+10",	16,	16LLU,	3, 0);
	TEST_STRTOULL("  +10 ",	16,	16LLU,	5, 0);
	TEST_STRTOULL("+0x10",	16,	16LLU,	5, 0);
	TEST_STRTOULL("+0x10",	0,	16LLU,	5, 0);
	TEST_STRTOULL(" +0x10 ",	0,	16LLU,	6, 0);
	TEST_STRTOULL("-10",	16,	-16LLU,	3, 0);
	TEST_STRTOULL("  -10 ",	16,	-16LLU,	5, 0);
	TEST_STRTOULL("-0x10",	16,	-16LLU,	5, 0);
	TEST_STRTOULL("-0x10",	0,	-16LLU,	5, 0);
	TEST_STRTOULL(" -0x10 ",	0,	-16LLU,	6, 0);
	TEST_STRTOULL("010",	16,	16LLU,	3, 0);
	TEST_STRTOULL("  010 ",	16,	16LLU,	5, 0);
	TEST_STRTOULL("-010",	16,	-16LLU,	4, 0);

	TEST_STRTOULL("11",	8,	9LLU,	2, 0);
	TEST_STRTOULL("011",	8,	9LLU,	3, 0);
	TEST_STRTOULL("011",	0,	9LLU,	3, 0);
	TEST_STRTOULL("-11",	8,	-9LLU,	3, 0);
	TEST_STRTOULL("-011",	8,	-9LLU,	4, 0);
	TEST_STRTOULL("-011",	0,	-9LLU,	4, 0);

	TEST_STRTOULL("011",	8,	9LLU,	3, 0);
	TEST_STRTOULL("011",	0,	9LLU,	3, 0);
	TEST_STRTOULL("-11",	8,	-9LLU,	3, 0);
	TEST_STRTOULL("-011",	8,	-9LLU,	4, 0);
	TEST_STRTOULL("-011",	0,	-9LLU,	4, 0);

	TEST_STRTOULL("Text",	0,	0LLU,	0, 0);

	TEST_STRTOULL("9223372036854775807",	10,	9223372036854775807LLU,	19, 0);
	TEST_STRTOULL("9223372036854775807",	0,	9223372036854775807LLU,	19, 0);
	TEST_STRTOULL("9223372036854775808",	0,	9223372036854775808LLU,	19, 0);
	TEST_STRTOULL("9223372036854775808",	10,	9223372036854775808LLU,	19, 0);
	TEST_STRTOULL("0x7FFFFFFFFFFFFFFF",	0,	9223372036854775807LLU,	18, 0);
	TEST_STRTOULL("0x7FFFFFFFFFFFFFFF",	16,	9223372036854775807LLU,	18, 0);
	TEST_STRTOULL("7FFFFFFFFFFFFFFF",	16,	9223372036854775807LLU,	16, 0);
	TEST_STRTOULL("0x8000000000000000",	0,	9223372036854775808LLU,	18, 0);
	TEST_STRTOULL("0x8000000000000000",	16,	9223372036854775808LLU,	18, 0);
	TEST_STRTOULL("8000000000000000",	16,	9223372036854775808LLU,	16, 0);
	TEST_STRTOULL("0777777777777777777777",	0,	9223372036854775807LLU,	22, 0);
	TEST_STRTOULL("0777777777777777777777",	8,	9223372036854775807LLU,	22, 0);
	TEST_STRTOULL("777777777777777777777",	8,	9223372036854775807LLU,	21, 0);
	TEST_STRTOULL("01000000000000000000000",0,	9223372036854775808LLU,	23, 0);
	TEST_STRTOULL("01000000000000000000000",8,	9223372036854775808LLU,	23, 0);
	TEST_STRTOULL("1000000000000000000000",	8,	9223372036854775808LLU,	22, 0);

	TEST_STRTOULL("-9223372036854775808",	10,	9223372036854775808LLU,	20, 0);
	TEST_STRTOULL("-9223372036854775808",	0,	9223372036854775808LLU,	20, 0);
	TEST_STRTOULL("-9223372036854775809",	0,	9223372036854775807LLU,	20, 0);
	TEST_STRTOULL("-9223372036854775809",	10,	9223372036854775807LLU,	20, 0);
	TEST_STRTOULL("-0x8000000000000000",	0,	9223372036854775808LLU,	19, 0);
	TEST_STRTOULL("-0x8000000000000000",	16,	9223372036854775808LLU,	19, 0);
	TEST_STRTOULL("-8000000000000000",	16,	9223372036854775808LLU,	17, 0);
	TEST_STRTOULL("-0x8000000000000001",	0,	9223372036854775807LLU,	19, 0);
	TEST_STRTOULL("-0x8000000000000001",	16,	9223372036854775807LLU,	19, 0);
	TEST_STRTOULL("-8000000000000001",	16,	9223372036854775807LLU,	17, 0);
	TEST_STRTOULL("-01000000000000000000000",0,	9223372036854775808LLU,	24, 0);
	TEST_STRTOULL("-01000000000000000000000",8,	9223372036854775808LLU,	24, 0);
	TEST_STRTOULL("-1000000000000000000000",8,	9223372036854775808LLU,	23, 0);
	TEST_STRTOULL("-01000000000000000000001",0,	9223372036854775807LLU,	24, 0);
	TEST_STRTOULL("-01000000000000000000001",8,	9223372036854775807LLU,	24, 0);
	TEST_STRTOULL("-1000000000000000000001",8,	9223372036854775807LLU,	23, 0);

	TEST_STRTOULL("18446744073709551615",	0,	18446744073709551615LLU,	20, 0);
	TEST_STRTOULL("18446744073709551615",	10,	18446744073709551615LLU,	20, 0);
	TEST_STRTOULL("18446744073709551616",	0,	18446744073709551615LLU,	20, ERANGE);
	TEST_STRTOULL("18446744073709551616",	10,	18446744073709551615LLU,	20, ERANGE);
	TEST_STRTOULL("0xFFFFFFFFFFFFFFFF",	0,	18446744073709551615LLU,	18, 0);
	TEST_STRTOULL("0xFFFFFFFFFFFFFFFF",	16,	18446744073709551615LLU,	18, 0);
	TEST_STRTOULL("FFFFFFFFFFFFFFFF",	16,	18446744073709551615LLU,	16, 0);
	TEST_STRTOULL("0x10000000000000000",	0,	18446744073709551615LLU,	19, ERANGE);
	TEST_STRTOULL("0x10000000000000000",	16,	18446744073709551615LLU,	19, ERANGE);
	TEST_STRTOULL("10000000000000000",	16,	18446744073709551615LLU,	17, ERANGE);
	TEST_STRTOULL("01777777777777777777777",0,	18446744073709551615LLU,	23, 0);
	TEST_STRTOULL("01777777777777777777777",8,	18446744073709551615LLU,	23, 0);
	TEST_STRTOULL("1777777777777777777777",	8,	18446744073709551615LLU,	22, 0);
	TEST_STRTOULL("02000000000000000000000",0,	18446744073709551615LLU,	23, ERANGE);
	TEST_STRTOULL("02000000000000000000000",8,	18446744073709551615LLU,	23, ERANGE);
	TEST_STRTOULL("2000000000000000000000",	8,	18446744073709551615LLU,	22, ERANGE);

	TEST_STRTOULL("-18446744073709551615",	0,	1LLU,				21, 0);
	TEST_STRTOULL("-18446744073709551615",	10,	1LLU,				21, 0);
	TEST_STRTOULL("-18446744073709551616",	0,	18446744073709551615LLU,	21, ERANGE);
	TEST_STRTOULL("-18446744073709551616",	10,	18446744073709551615LLU,	21, ERANGE);
	TEST_STRTOULL("-0xFFFFFFFFFFFFFFFF",	0,	1LLU,				19, 0);
	TEST_STRTOULL("-0xFFFFFFFFFFFFFFFF",	16,	1LLU,				19, 0);
	TEST_STRTOULL("-FFFFFFFFFFFFFFFF",	16,	1LLU,				17, 0);
	TEST_STRTOULL("-0x10000000000000000",	0,	18446744073709551615LLU,	20, ERANGE);
	TEST_STRTOULL("-0x10000000000000000",	16,	18446744073709551615LLU,	20, ERANGE);
	TEST_STRTOULL("-10000000000000000",	16,	18446744073709551615LLU,	18, ERANGE);
	TEST_STRTOULL("-01777777777777777777777",0,	1LLU,				24, 0);
	TEST_STRTOULL("-01777777777777777777777",8,	1LLU,				24, 0);
	TEST_STRTOULL("-1777777777777777777777",8,	1LLU,				23, 0);
	TEST_STRTOULL("-02000000000000000000000",0,	18446744073709551615LLU,	24, ERANGE);
	TEST_STRTOULL("-02000000000000000000000",8,	18446744073709551615LLU,	24, ERANGE);
	TEST_STRTOULL("-2000000000000000000000",8,	18446744073709551615LLU,	23, ERANGE);

	printf("success: strtuoll\n");
	return true;
}

/* 
FIXME:
Types:
bool
socklen_t
uint_t
uint{8,16,32,64}_t
int{8,16,32,64}_t
intptr_t

Constants:
PATH_NAME_MAX
UINT{16,32,64}_MAX
INT32_MAX
*/

static int test_va_copy(void)
{
	/* FIXME */
	return true;
}

static int test_FUNCTION(void)
{
	printf("test: FUNCTION\n");
	if (strcmp(__FUNCTION__, "test_FUNCTION") != 0) {
		printf("failure: FAILURE [\nFAILURE invalid\n]\n");
		return false;
	}
	printf("success: FUNCTION\n");
	return true;
}

static int test_MIN(void)
{
	printf("test: MIN\n");
	if (MIN(20, 1) != 1) {
		printf("failure: MIN [\nMIN invalid\n]\n");
		return false;
	}
	if (MIN(1, 20) != 1) {
		printf("failure: MIN [\nMIN invalid\n]\n");
		return false;
	}
	printf("success: MIN\n");
	return true;
}

static int test_MAX(void)
{
	printf("test: MAX\n");
	if (MAX(20, 1) != 20) {
		printf("failure: MAX [\nMAX invalid\n]\n");
		return false;
	}
	if (MAX(1, 20) != 20) {
		printf("failure: MAX [\nMAX invalid\n]\n");
		return false;
	}
	printf("success: MAX\n");
	return true;
}

static int test_socketpair(void)
{
	int sock[2];
	char buf[20];

	printf("test: socketpair\n");

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock) == -1) {
		printf("failure: socketpair [\n"
			   "socketpair() failed\n"
			   "]\n");
		return false;
	}

	if (write(sock[1], "automatisch", 12) == -1) {
		printf("failure: socketpair [\n"
			   "write() failed: %s\n"
			   "]\n", strerror(errno));
		return false;
	}

	if (read(sock[0], buf, 12) == -1) {
		printf("failure: socketpair [\n"
			   "read() failed: %s\n"
			   "]\n", strerror(errno));
		return false;
	}

	if (strcmp(buf, "automatisch") != 0) {
		printf("failure: socketpair [\n"
			   "expected: automatisch, got: %s\n"
			   "]\n", buf);
		return false;
	}

	printf("success: socketpair\n");

	return true;
}

extern int libreplace_test_strptime(void);

static int test_strptime(void)
{
	return libreplace_test_strptime();
}

struct torture_context;
bool torture_local_replace(struct torture_context *ctx)
{
	bool ret = true;
	ret &= test_ftruncate();
	ret &= test_strlcpy();
	ret &= test_strlcat();
	ret &= test_mktime();
	ret &= test_initgroups();
	ret &= test_memmove();
	ret &= test_strdup();
	ret &= test_setlinebuf();
	ret &= test_vsyslog();
	ret &= test_timegm();
	ret &= test_setenv();
	ret &= test_strndup();
	ret &= test_strnlen();
	ret &= test_waitpid();
	ret &= test_seteuid();
	ret &= test_setegid();
	ret &= test_asprintf();
	ret &= test_snprintf();
	ret &= test_vasprintf();
	ret &= test_vsnprintf();
	ret &= test_opendir();
	ret &= test_readdir();
	ret &= test_telldir();
	ret &= test_seekdir();
	ret &= test_dlopen();
	ret &= test_chroot();
	ret &= test_bzero();
	ret &= test_strerror();
	ret &= test_errno();
	ret &= test_mkdtemp();
	ret &= test_mkstemp();
	ret &= test_pread();
	ret &= test_pwrite();
	ret &= test_getpass();
	ret &= test_inet_ntoa();
	ret &= test_strtoll();
	ret &= test_strtoull();
	ret &= test_va_copy();
	ret &= test_FUNCTION();
	ret &= test_MIN();
	ret &= test_MAX();
	ret &= test_socketpair();
	ret &= test_strptime();

	return ret;
}

#if _SAMBA_BUILD_<4
int main(void)
{
	bool ret = torture_local_replace(NULL);
	if (ret) 
		return 0;
	return -1;
}
#endif
