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
#ifndef __UTILS_H
#define __UTILS_H   1

#include <sys/types.h>
#include "dstrbuf.h"

typedef enum {
	IS_ASCII,
	IS_UTF8,
	IS_PARTIAL_UTF8,
	IS_OTHER
} CharSetType;

dstrbuf *expandPath(const char *path);
int copyfile(const char *from, const char *to);
dstrbuf *randomString(size_t size);
dstrbuf *getFirstEmail(void);
void properExit(int sig);
void chomp(char *str);
int copyUpTo(dstrbuf *buf, int stop, FILE *in);
CharSetType getCharSet(const u_char *str);
dstrbuf *encodeUtf8String(const u_char *str, bool use_qp);

#endif /* __UTILS_H */
