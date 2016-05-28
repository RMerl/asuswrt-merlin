
#ifdef LIBREPLACE_CONFIGURE_TEST_STRPTIME

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define true 1
#define false 0

#ifndef __STRING
#define __STRING(x)    #x
#endif

/* make printf a no-op */
#define printf if(0) printf

#else /* LIBREPLACE_CONFIGURE_TEST_STRPTIME */

#include "replace.h"
#include "system/time.h"

#endif /* LIBREPLACE_CONFIGURE_TEST_STRPTIME */

int libreplace_test_strptime(void)
{
	const char *s = "20070414101546Z";
	char *ret;
	struct tm t, t2;

	memset(&t, 0, sizeof(t));
	memset(&t2, 0, sizeof(t2));

	printf("test: strptime\n");

	ret = strptime(s, "%Y%m%d%H%M%S", &t);
	if ( ret == NULL ) {
		printf("failure: strptime [\n"
		       "returned NULL\n"
		       "]\n");
		return false;
	}

	if ( *ret != 'Z' ) {
		printf("failure: strptime [\n"
		       "ret doesn't point to 'Z'\n"
		       "]\n");
		return false;
	}

	ret = strptime(s, "%Y%m%d%H%M%SZ", &t2);
	if ( ret == NULL ) {
		printf("failure: strptime [\n"
		       "returned NULL with Z\n"
		       "]\n");
		return false;
	}

	if ( *ret != '\0' ) {
		printf("failure: strptime [\n"
		       "ret doesn't point to '\\0'\n"
		       "]\n");
		return false;
	}

#define CMP_TM_ELEMENT(t1,t2,elem) \
	if (t1.elem != t2.elem) { \
		printf("failure: strptime [\n" \
		       "result differs if the format string has a 'Z' at the end\n" \
		       "element: %s %d != %d\n" \
		       "]\n", \
		       __STRING(elen), t1.elem, t2.elem); \
		return false; \
	}

	CMP_TM_ELEMENT(t,t2,tm_sec);
	CMP_TM_ELEMENT(t,t2,tm_min);
	CMP_TM_ELEMENT(t,t2,tm_hour);
	CMP_TM_ELEMENT(t,t2,tm_mday);
	CMP_TM_ELEMENT(t,t2,tm_mon);
	CMP_TM_ELEMENT(t,t2,tm_year);
	CMP_TM_ELEMENT(t,t2,tm_wday);
	CMP_TM_ELEMENT(t,t2,tm_yday);
	CMP_TM_ELEMENT(t,t2,tm_isdst);

	if (t.tm_sec != 46) {
		printf("failure: strptime [\n"
		       "tm_sec: expected: 46, got: %d\n"
		       "]\n",
		       t.tm_sec);
		return false;
	}

	if (t.tm_min != 15) {
		printf("failure: strptime [\n"
		       "tm_min: expected: 15, got: %d\n"
		       "]\n",
		       t.tm_min);
		return false;
	}

	if (t.tm_hour != 10) {
		printf("failure: strptime [\n"
		       "tm_hour: expected: 10, got: %d\n"
		       "]\n",
		       t.tm_hour);
		return false;
	}

	if (t.tm_mday != 14) {
		printf("failure: strptime [\n"
		       "tm_mday: expected: 14, got: %d\n"
		       "]\n",
		       t.tm_mday);
		return false;
	}

	if (t.tm_mon != 3) {
		printf("failure: strptime [\n"
		       "tm_mon: expected: 3, got: %d\n"
		       "]\n",
		       t.tm_mon);
		return false;
	}

	if (t.tm_year != 107) {
		printf("failure: strptime [\n"
		       "tm_year: expected: 107, got: %d\n"
		       "]\n",
		       t.tm_year);
		return false;
	}

	if (t.tm_wday != 6) { /* saturday */
		printf("failure: strptime [\n"
		       "tm_wday: expected: 6, got: %d\n"
		       "]\n",
		       t.tm_wday);
		return false;
	}

	if (t.tm_yday != 103) {
		printf("failure: strptime [\n"
		       "tm_yday: expected: 103, got: %d\n"
		       "]\n",
		       t.tm_yday);
		return false;
	}

	/* we don't test this as it depends on the host configuration
	if (t.tm_isdst != 0) {
		printf("failure: strptime [\n"
		       "tm_isdst: expected: 0, got: %d\n"
		       "]\n",
		       t.tm_isdst);
		return false;
	}*/

	printf("success: strptime\n");

	return true;
}

#ifdef LIBREPLACE_CONFIGURE_TEST_STRPTIME
int main (void)
{
	int ret;
	ret = libreplace_test_strptime();
	if (ret == false) return 1;
	return 0;
}
#endif
