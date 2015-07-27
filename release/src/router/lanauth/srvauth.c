/*
 * lanauth server
 * (c) 2009 Vladislav Grishenko <themiron@mail.ru>
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <wait.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LTC_NO_CIPHERS
#define LTC_NO_MODES
#define LTC_NO_HASHES
#define LTC_NO_MACS
#define LTC_NO_PRNGS
#define LTC_NO_PK
#define LTC_NO_PKCS
#define LTC_NO_TEST
#define MD5
#define RIPEMD160
#include <tomcrypt.h>

#define V1_PORT 8899
#define V2_PORT 8314

struct challenge {
	unsigned char len;
	unsigned char value[255];
};

struct digest {
	short int len;
	unsigned char value[256];
};

static char *config;

static int open_socket(int port)
{
	struct sockaddr_in sin;
	int fd, one = 1;

	fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
	if (fd < 0)
		goto error;

#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0 &&
	    errno != ENOPROTOOPT)
		goto error;
#endif
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
		goto error;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	if (bind(fd, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		goto error;

	if (listen(fd, 5) < 0)
		goto error;

	return fd;

error:
	fprintf(stderr, "error: %s\n", strerror(errno));
	close(fd);
	return -1;
}

static int auth(int ver, char *client, char *pass, unsigned char *level, struct challenge *challenge, struct digest *digest)
{
	hash_state ctx;
	struct digest check;
	char strlevel[sizeof("255")];
	unsigned char reqlevel;

	switch (ver) {
	case 1:
		for (reqlevel = 0; reqlevel <= 2; reqlevel++) {
			snprintf(strlevel, sizeof(strlevel), "%hhu", reqlevel);

			md5_init(&ctx);
			md5_process(&ctx, challenge->value, challenge->len);
			md5_process(&ctx, (unsigned char *) client, strlen(client));
			md5_process(&ctx, (unsigned char *) strlevel, strlen(strlevel));
			md5_process(&ctx, (unsigned char *) pass, strlen(pass));
			md5_done(&ctx, check.value);

			if (memcmp(digest->value, check.value, 16) == 0) {
				*level = reqlevel;
				return 0;
			}
		}
		break;
	case 2:
		rmd160_init(&ctx);
		rmd160_process(&ctx, challenge->value, challenge->len);
		rmd160_process(&ctx, (unsigned char *) pass, strlen(pass));
		rmd160_done(&ctx, check.value);

		if (memcmp(digest->value + digest->value[1], check.value, 20) == 0) {
			*level = digest->value[0] + 1;
			return 0;
		}
		break;
	}

	return -1;
}

static int read_config(char *client, char *pass, int size)
{
	FILE *fp;
	char line[256], *value;
	int ret;

	fp = fopen(config, "r");
	if (fp == NULL)
		return -1;

	ret = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *ptr = line;

		value = strsep(&ptr, " \t\r\n");
		if (strcmp(value, client) != 0)
			continue;

		value = strsep(&ptr, " \t\r\n");
		ret = snprintf(pass, size, "%s", value);
	}
	fclose(fp);

	return ret;
}

static int process(int fd, int ver, struct in_addr src)
{
	unsigned char op;
	struct challenge challenge;
	struct digest digest;
	char client[sizeof("255.255.255.255")];
	char pass[32];
	unsigned char level;
	int ret, i;

	snprintf(client, sizeof(client), "%s", inet_ntoa(src));
	op = (read_config(client, pass, sizeof(pass)) > 0) ? 1 : 0;

	ret = write(fd, &op, sizeof(op));
	if (ret < sizeof(op))
		goto error;
	if (op == 0)
		goto error;

	ret = fork();
	if (ret < 0)
		goto error;
	if (ret > 0) {
		ret = 0;
		goto error;
	}

	while (1) {
		challenge.len = sizeof(challenge.value);
		for (i = 0; i < challenge.len; i++)
			challenge.value[i] = rand() % 256;
		ret = write(fd, &challenge, sizeof(challenge));
		if (ret < sizeof(challenge))
			break;

		digest.len = (ver == 1) ? 16 : 256;
		ret = read(fd, &digest.value, digest.len);
		if (ret < digest.len)
			break;

		ret = auth(ver, client, pass, &level, &challenge, &digest);
		if (ret < 0)
			break;
		
		ret = write(fd, &level, sizeof(level));
		if (ret < sizeof(level))
			break;

		sleep(60);
		if (read_config(client, pass, sizeof(pass)) <= 0)
			break;
	}

	close(fd);
	_exit(EXIT_SUCCESS);

error:
	close(fd);
	return ret;
}

static void sighandler(int sig)
{
	int ret;

	do {
		ret = waitpid(-1, NULL, WNOHANG);
	} while (ret > 0);
}

int main(int argc, char *argv[])
{
	int fd, fd1, fd2;
	int ret;
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	config = argv[1];
	if (config == NULL)
		return EXIT_FAILURE;

	fd1 = open_socket(V1_PORT);
	fd2 = open_socket(V2_PORT);

	if (fd1 < 0 || fd2 < 0)
		return EXIT_FAILURE;

	if (daemon(1, 1) < 0) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	//signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, sighandler);

	ret = 0;
	while (ret >= 0) {
		fd_set rfds;
//		struct timeval tv = { 60, 0 };

		FD_ZERO(&rfds);
		FD_SET(fd1, &rfds);
		FD_SET(fd2, &rfds);

		do {
			ret = select(MAX(fd1, fd2) + 1, &rfds, NULL, NULL, NULL);
		} while (ret < 0 && errno == EINTR);

		if (ret < 0)
			break;
		if (ret == 0)
			continue;

		if (FD_ISSET(fd1, &rfds)) {
			fd = accept(fd1, (struct sockaddr *) &sin, &len);
			if (fd < 0)
				break;
			ret = process(fd, 1, sin.sin_addr);
		}
		if (FD_ISSET(fd2, &rfds)) {
			fd = accept(fd2, (struct sockaddr *) &sin, &len);
			if (fd < 0)
				break;
			ret = process(fd, 2, sin.sin_addr);
		}
	}

	close(fd1);
	close(fd2);

	return EXIT_SUCCESS;
}
