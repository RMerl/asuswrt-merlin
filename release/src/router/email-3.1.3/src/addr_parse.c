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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "email.h"
#include "utils.h"


/**
 * strip any quotes from the begining and end of the
 * email name passed.  
**/
char *
stripEmailName(char *str)
{
	char *end_quote;

	if (!str) {
		return NULL;
	}

	/* first remove end quote */
	if ((end_quote = strrchr(str, '"'))) {
		*end_quote = '\0';
	}
	/* now remove begining quote */
	if (*str == '"') {
		str++;
	}
	return str;
}

/**
 * strip any quotes and brackets from the begining of the 
 * email address that is passed.
**/
char *
stripEmailAddr(char *str)
{
	char *end_quote, *end_bracket;

	if (!str) {
		return NULL;
	}

	/* first check if we have an end quote */
	if ((end_quote = strrchr(str, '"'))) {
		*end_quote = '\0';
	}
	/* now check for end brace */
	if ((end_bracket = strrchr(str, '>'))) {
		*end_bracket = '\0';
	}

	/* now check for begining quote and bracket */
	if ((*str == '"') || (*str == '<')) {
		str++;
	}
	if ((*str == '<') || (*str == '"')) {
		str++;
	}
	return str;
}

/**
 * Parses a given address to separate the name from the address.
 */
int
parseAddr(const char *addr, dstrbuf *ret_name, dstrbuf *ret_addr)
{
	int retval = 0;
	dstrbuf *copy = DSB_NEW;
	dsbCopy(copy, addr);

	/* If there is a formatted email address, break it up */
	if (strchr(copy->str, '<') && copy->str[0] != '<') {
		char *tok = strtok(copy->str, "<");
		dsbCopy(ret_name, tok);
		tok = strtok(NULL, "<");
		tok = strtok(tok, ">");
		if (tok == NULL) {
			retval = ERROR;
			goto end;
		} else {
			dsbCopy(ret_addr, tok);
		}
	} else {
		/* it's just a regular addr (no name) */
		char *stripped = stripEmailAddr(copy->str);
		dsbCopy(ret_addr, stripped);
	}
end:
	dsbDestroy(copy);
	return retval;
}

/** 
 * just a wrapper for snprintf() to make code
 * more readable.  A reader will know that the email address is being
 * formated by reading this name.
**/
dstrbuf *
formatEmailAddr(char *name, char *address)
{
	char *proper_name, *proper_addr;
	dstrbuf *ret = DSB_NEW, *dsb=NULL;
	CharSetType charset;

	assert(address != NULL);

	proper_addr = stripEmailAddr(address);
	if (name) {
		proper_name = stripEmailName(name);
		if (Mopts.encoding) {
			charset = getCharSet((u_char *)proper_name);
			if (charset == IS_UTF8) {
				dsb = encodeUtf8String((u_char *)proper_name, false);
				proper_name = dsb->str;
			} else if (charset == IS_PARTIAL_UTF8) {
				dsb = encodeUtf8String((u_char *)proper_name, true);
				proper_name = dsb->str;
			}
		}
		dsbPrintf(ret, "\"%s\" <%s>", proper_name, proper_addr);
		if (dsb) {
			dsbDestroy(dsb);
		}
	} else {
		dsbPrintf(ret, "<%s>", proper_addr);
	}
	return ret;
}

/**
 * will take one email address as an argument 
 * and make sure it has the appropriate syntax of an email address. 
 * if so, return TRUE, else return FALSE 
**/
int
validateEmail(const char *email_addy)
{
	char *copy;
	char *name, *domain;
	int retval = SUCCESS;

	/* make a copy of email_addy so we don't ruin it with strtok */
	copy = xstrdup(email_addy);

	/* Get the name before the @ character. */
	name = strtok(copy, "@");
	if (name != NULL) {
		/* Get the domain after the @ character. */
		domain = strtok(NULL, "@");
		if (domain == NULL) {
			retval = ERROR;
		}
	} else {
		retval = ERROR;
	}
	free(copy);
	return retval;
}

