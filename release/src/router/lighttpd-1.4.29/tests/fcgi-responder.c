#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_FASTCGI_FASTCGI_H
#include <fastcgi/fcgi_stdio.h>
#else
#include <fcgi_stdio.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main () {
	int num_requests = 2;

	while (num_requests > 0 &&
	       FCGI_Accept() >= 0) {
		char* p;

		if (NULL != (p = getenv("QUERY_STRING"))) {
			if (0 == strcmp(p, "lf")) {
				printf("Status: 200 OK\n\n");
			} else if (0 == strcmp(p, "crlf")) {
				printf("Status: 200 OK\r\n\r\n");
			} else if (0 == strcmp(p, "slow-lf")) {
				printf("Status: 200 OK\n");
				fflush(stdout);
				printf("\n");
			} else if (0 == strcmp(p,"slow-crlf")) {
				printf("Status: 200 OK\r\n");
				fflush(stdout);
				printf("\r\n");
			} else if (0 == strcmp(p, "die-at-end")) {
				printf("Status: 200 OK\r\n\r\n");
				num_requests--;
			} else {
				printf("Status: 200 OK\r\n\r\n");
			}
		} else {
			printf("Status: 500 Internal Foo\r\n\r\n");
		}

		if (0 == strcmp(p, "path_info")) {
			printf("%s", getenv("PATH_INFO"));
		} else if (0 == strcmp(p, "script_name")) {
			printf("%s", getenv("SCRIPT_NAME"));
		} else {
			printf("test123");
		}
	}

	return 0;
}
