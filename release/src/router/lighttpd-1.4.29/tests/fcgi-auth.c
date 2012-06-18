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
	char* p;

	while (FCGI_Accept() >= 0) {
		/* wait for fastcgi authorizer request */

		printf("Content-type: text/html\r\n");

		if (((p = getenv("QUERY_STRING")) == NULL) ||
		    strcmp(p, "ok") != 0) {
			printf("Status: 403 Forbidden\r\n\r\n");
		} else {
			printf("\r\n");
			/* default Status is 200 - allow access */
		}

		printf("foobar\r\n");
	}

	return 0;
}
