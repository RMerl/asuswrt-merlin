/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "email.h"
#include "error.h"

/**
 * Simply provide an fatal error message to stderr
 * We don't exit inside of this function because cleanup may 
 * need to happen so we allow the caller to handle the exit.
**/
void
fatal(const char *message, ...)
{
	int tmp_error = errno;
	va_list vp;
	FILE *fp;

	fp = fopen(EMAIL_LOG_FILE, "a+b");
	if( fp == NULL ){
	    printf("open file error!!\n");
	    return;
	}

	va_start(vp, message);
	fprintf(stderr, "email: FATAL: ");
	fputs("email: FATAL: ", fp);

	vfprintf(stderr, message, vp);
	vfprintf(fp, message, vp);

	/* if message has a \n mark, don't call perror */
	if (strchr(message, '\n') == NULL) {
		fprintf(stderr, ": %s\n", strerror(tmp_error));

		fputs(": ",fp);
		fputs(strerror(tmp_error), fp);
	}
	va_end(vp);

	fputs("\n",fp);
	fclose(fp);

	system("nvram set fb_state=2");
}

/**
 * Warning will just warn with the specified warning message 
 * and then return from the function not exiting the system.
**/
void
warning(const char *message, ...)
{
	int tmp_error = errno;
	va_list vp;

	va_start(vp, message);
	fprintf(stderr, "email: WARNING: ");
	vfprintf(stderr, message, vp);

	/* if message has a \n mark, don't call perror */
	if (strchr(message, '\n') == NULL) {
		fprintf(stderr, ": %s\n", strerror(tmp_error));
	}
	va_end(vp);
}

