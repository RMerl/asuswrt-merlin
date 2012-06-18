/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Generic HTTP routines
 *
 * Copyright 2001, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <shutils.h>

static char *
base64enc(const char *p, char *buf, int len)
{
	char al[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	char *s = buf;

	while (*p) {
		if (s >= buf+len-4)
			break;
		*(s++) = al[(*p >> 2) & 0x3F];
		*(s++) = al[((*p << 4) & 0x30) | ((*(p+1) >> 4) & 0x0F)];
		*s = *(s+1) = '=';
		*(s+2) = 0;
		if (! *(++p)) break;
		*(s++) = al[((*p << 2) & 0x3C) | ((*(p+1) >> 6) & 0x03)];
		if (! *(++p)) break;
		*(s++) = al[*(p++) & 0x3F];
	}

	return buf;
}

enum {
	METHOD_GET,
	METHOD_POST
};

static int
wget(int method, const char *server, char *buf, size_t count, off_t offset)
{
	char url[PATH_MAX] = { 0 }, *s;
	char *host = url, *path = "", auth[128] = { 0 }, line[512];
	unsigned short port = 80;
	int fd;
	FILE *fp;
	struct sockaddr_in sin;
	int chunked = 0, len = 0;

	strncpy(url, server, sizeof(url));

	/* Parse URL */
	if (!strncmp(url, "http://", 7)) {
		port = 80;
		host = url + 7;
	}
	if ((s = strchr(host, '/'))) {
		*s++ = '\0';
		path = s;
	}
	if ((s = strchr(host, '@'))) {
		*s++ = '\0';
		base64enc(host, auth, sizeof(auth));
		host = s;
	}
	if ((s = strchr(host, ':'))) {
		*s++ = '\0';
		port = atoi(s);
	}

	/* Open socket */
	if (!inet_aton(host, &sin.sin_addr))
		return (-(errno = EINVAL));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	dprintf("Connecting to %s:%u...\n", host, port);
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
	    connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0 ||
	    !(fp = fdopen(fd, "r+"))) {
		perror(host);
		if (fd >= 0)
			close(fd);
		return -errno;
	}
	dprintf("connected!\n");

	/* Send HTTP request */
	fprintf(fp, "%s /%s HTTP/1.1\r\n", method == METHOD_POST ? "POST" : "GET", path);
	fprintf(fp, "Host: %s\r\n", host);
	fprintf(fp, "User-Agent: wget\r\n");
	if (strlen(auth))
		fprintf(fp, "Authorization: Basic %s\r\n", auth);
	if (offset)
		fprintf(fp, "Range: bytes=%ld-\r\n", offset);
	if (method == METHOD_POST) {
		fprintf(fp, "Content-Type: application/x-www-form-urlencoded\r\n");
		fprintf(fp, "Content-Length: %d\r\n\r\n", strlen(buf));
		fputs(buf, fp);
	} else
		fprintf(fp,"Connection: close\r\n\r\n");

	/* Check HTTP response */
	dprintf("HTTP request sent, awaiting response...\n");
	if (fgets(line, sizeof(line), fp)) {
		dprintf("%s", line);
		for (s = line; *s && !isspace(*s); s++);
		for (; isspace(*s); s++);
		switch (atoi(s)) {
		case 200: if (offset) goto done; else break;
		case 206: if (offset) break; else goto done;
		default: goto done;
		}
	}

	/* Parse headers */
	while (fgets(line, sizeof(line), fp)) {
		dprintf("%s", line);
		for (s = line; *s == '\r'; s++);
		if (*s == '\n')
			break;
		if (!strncasecmp(s, "Content-Length:", 15)) {
			for (s += 15; isblank(*s); s++);
			chomp(s);
			len = atoi(s);
		}
		else if (!strncasecmp(s, "Transfer-Encoding:", 18)) {
			for (s += 18; isblank(*s); s++);
			chomp(s);
			if (!strncasecmp(s, "chunked", 7))
				chunked = 1;
		}
	}

	if (chunked && fgets(line, sizeof(line), fp))
		len = strtol(line, NULL, 16);
	
	len = (len > count) ? count : len;
	len = fread(buf, 1, len, fp);

 done:
	/* Close socket */
	fflush(fp);
	fclose(fp);
	return len;
}

int
http_get(const char *server, char *buf, size_t count, off_t offset)
{
	return wget(METHOD_GET, server, buf, count, offset);
}

int
http_post(const char *server, char *buf, size_t count)
{
	/* No continuation generally possible with POST */
	return wget(METHOD_POST, server, buf, count, 0);
}
