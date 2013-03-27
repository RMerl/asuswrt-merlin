/* 
   Framework for testing with a server process
   Copyright (C) 2001-2008, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#include <sys/types.h>

#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <fcntl.h>
#include <netdb.h>
#include <signal.h>

#include "ne_socket.h"
#include "ne_utils.h"
#include "ne_string.h"

#include "tests.h"
#include "child.h"

static pid_t child = 0;

int clength;

static struct in_addr lh_addr = {0}, hn_addr = {0};

const char *want_header = NULL;
got_header_fn got_header = NULL;
char *local_hostname = NULL;

/* If we have pipe(), then use a pipe between the parent and child to
 * know when the child is ready to accept incoming connections.
 * Otherwise use boring sleep()s trying to avoid the race condition
 * between listen() and connect() in the two processes. */
#ifdef HAVE_PIPE
#define USE_PIPE 1
#endif

int lookup_localhost(void)
{
    /* this will break if a system is set up so that `localhost' does
     * not resolve to 127.0.0.1, but... */
    lh_addr.s_addr = inet_addr("127.0.0.1");
    return OK;
}

int lookup_hostname(void)
{
    char buf[BUFSIZ];
    struct hostent *ent;

    local_hostname = NULL;
    ONV(gethostname(buf, BUFSIZ) < 0,
	("gethostname failed: %s", strerror(errno)));

    ent = gethostbyname(buf);
#ifdef HAVE_HSTRERROR
    ONV(ent == NULL,
	("could not resolve `%s': %s", buf, hstrerror(h_errno)));
#else
    ONV(ent == NULL, ("could not resolve `%s'", buf));
#endif

    local_hostname = ne_strdup(ent->h_name);

    return OK;
}

static int do_listen(struct in_addr addr, int port)
{
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr = {0};
    int val = 1;

    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
    
    saddr.sin_addr = addr;
    saddr.sin_port = htons(port);
    saddr.sin_family = AF_INET;

    if (bind(ls, (struct sockaddr *)&saddr, sizeof(saddr))) {
	printf("bind failed: %s\n", strerror(errno));
	return -1;
    }
    if (listen(ls, 5)) {
	printf("listen failed: %s\n", strerror(errno));
	return -1;
    }

    return ls;
}

void minisleep(void)
{
#ifdef HAVE_USLEEP
    usleep(500);
#else
    sleep(1);
#endif
}

int reset_socket(ne_socket *sock)
{
#ifdef SO_LINGER
    /* Stevens' magic trick to send an RST on close(). */
    struct linger l = {1, 0};
    return setsockopt(ne_sock_fd(sock), SOL_SOCKET, SO_LINGER, &l, sizeof l);
#else
    return 1;
#endif
}

/* close 'sock', performing lingering close to avoid premature RST. */
static int close_socket(ne_socket *sock)
{
#ifdef HAVE_SHUTDOWN
    char buf[20];
    int fd = ne_sock_fd(sock);
    
    shutdown(fd, 0);
    while (ne_sock_read(sock, buf, sizeof buf) > 0);
#endif
    return ne_sock_close(sock);
}

/* This runs as the child process. */
static int server_child(int readyfd, struct in_addr addr, int port,
			server_fn callback, void *userdata)
{
    ne_socket *s = ne_sock_create();
    int ret, listener;

    in_child();

    listener = do_listen(addr, port);
    if (listener < 0)
	return FAIL;

#ifdef USE_PIPE
    /* Tell the parent we're ready for the request. */
    if (write(readyfd, "a", 1) != 1) abort();
#endif

    ONN("accept failed", ne_sock_accept(s, listener));

    ret = callback(s, userdata);

    close_socket(s);

    return ret;
}

int spawn_server(int port, server_fn fn, void *ud)
{
    return spawn_server_addr(1, port, fn, ud);
}

int spawn_server_addr(int bind_local, int port, server_fn fn, void *ud)
{
    int fds[2];
    struct in_addr addr;

    addr = bind_local?lh_addr:hn_addr;

#ifdef USE_PIPE
    if (pipe(fds)) {
	perror("spawn_server: pipe");
	return FAIL;
    }
#else
    /* avoid using uninitialized variable. */
    fds[0] = fds[1] = 0;
#endif
    
    child = fork();

    ONN("fork server", child == -1);

    if (child == 0) {
	/* this is the child. */
	int ret;

	ret = server_child(fds[1], addr, port, fn, ud);

#ifdef USE_PIPE
	close(fds[0]);
	close(fds[1]);
#endif

	/* print the error out otherwise it gets lost. */
	if (ret) {
	    printf("server child failed: %s\n", test_context);
	}
	
	/* and quit the child. */
        NE_DEBUG(NE_DBG_HTTP, "child exiting with %d\n", ret);
	exit(ret);
    } else {
	char ch;
	
#ifdef USE_PIPE
	if (read(fds[0], &ch, 1) < 0)
	    perror("parent read");

	close(fds[0]);
	close(fds[1]);
#else
	minisleep();
#endif

	return OK;
    }
}

int spawn_server_repeat(int port, server_fn fn, void *userdata, int n)
{
    int fds[2];

#ifdef USE_PIPE
    if (pipe(fds)) {
	perror("spawn_server: pipe");
	return FAIL;
    }
#else
    /* avoid using uninitialized variable. */
    fds[0] = fds[1] = 0;
#endif

    child = fork();
    
    if (child == 0) {
	/* this is the child. */
	int listener, count = 0;
	
	in_child();
	
	listener = do_listen(lh_addr, port);

#ifdef USE_PIPE
	if (write(fds[1], "Z", 1) != 1) abort();
#endif

	close(fds[1]);
	close(fds[0]);

	/* Loop serving requests. */
	while (++count < n) {
	    ne_socket *sock = ne_sock_create();
	    int ret;

	    NE_DEBUG(NE_DBG_HTTP, "child awaiting connection #%d.\n", count);
	    ONN("accept failed", ne_sock_accept(sock, listener));
	    ret = fn(sock, userdata);
	    close_socket(sock);
	    NE_DEBUG(NE_DBG_HTTP, "child served request, %d.\n", ret);
	    if (ret) {
		printf("server child failed: %s\n", test_context);
		exit(-1);
	    }
	    /* don't send back notification to parent more than
	     * once. */
	}
	
	NE_DEBUG(NE_DBG_HTTP, "child aborted.\n");
	close(listener);

	exit(-1);
    
    } else {
	char ch;
	/* this is the parent. wait for the child to get ready */
#ifdef USE_PIPE
	if (read(fds[0], &ch, 1) < 0)
	    perror("parent read");

	close(fds[0]);
	close(fds[1]);
#else
	minisleep();
#endif
    }

    return OK;
}

int dead_server(void)
{
    int status;

    if (waitpid(child, &status, WNOHANG)) {
	/* child quit already! */
	return FAIL;
    }

    NE_DEBUG(NE_DBG_HTTP, "child has not quit.\n");

    return OK;
}


int await_server(void)
{
    int status, code;

    (void) wait(&status);
    
    /* so that we aren't reaped by mistake. */
    child = 0;

    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
        
        ONV(code,
            ("server process terminated abnormally: %s (%d)",
             code == FAIL ? "FAIL" : "error", code));
    }
    else {
        ONV(WIFSIGNALED(status),
            ("server process terminated by signal %d", WTERMSIG(status)));
    }

    return OK;
}

int reap_server(void)
{
    int status;
    
    if (child != 0) {
	(void) kill(child, SIGTERM);
	minisleep();
	(void) wait(&status);
	child = 0;
    }

    return OK;
}

ssize_t server_send(ne_socket *sock, const char *str, size_t len)
{
    NE_DEBUG(NE_DBG_HTTP, "Sending: %.*s\n", (int)len, str);
    return ne_sock_fullwrite(sock, str, len);
}

int discard_request(ne_socket *sock)
{
    char buffer[1024];
    size_t offset = want_header?strlen(want_header):0;

    clength = 0;

    NE_DEBUG(NE_DBG_HTTP, "Discarding request...\n");
    do {
	ONV(ne_sock_readline(sock, buffer, 1024) < 0,
	    ("error reading line: %s", ne_sock_error(sock)));
	NE_DEBUG(NE_DBG_HTTP, "[req] %s", buffer);
	if (strncasecmp(buffer, "content-length:", 15) == 0) {
	    clength = atoi(buffer + 16);
	}
	if (got_header != NULL && want_header != NULL && 
	    strncasecmp(buffer, want_header, offset) == 0 &&
	    buffer[offset] == ':') {
	    char *value = buffer + offset + 1;
	    if (*value == ' ') value++;
	    got_header(ne_shave(value, "\r\n"));
	}
    } while (strcmp(buffer, "\r\n") != 0);
    
    return OK;
}

int discard_body(ne_socket *sock)
{
    while (clength > 0) {
	char buf[BUFSIZ];
	size_t bytes = clength;
	ssize_t ret;
	if (bytes > sizeof(buf)) bytes = sizeof(buf);
	NE_DEBUG(NE_DBG_HTTP, "Discarding %" NE_FMT_SIZE_T " bytes.\n",
		 bytes);
	ret = ne_sock_read(sock, buf, bytes);
	ONV(ret < 0, ("socket read failed (%" NE_FMT_SSIZE_T "): %s", 
		      ret, ne_sock_error(sock)));
	clength -= ret;
	NE_DEBUG(NE_DBG_HTTP, "Got %" NE_FMT_SSIZE_T " bytes.\n", ret);
    }
    NE_DEBUG(NE_DBG_HTTP, "Discard successful.\n");
    return OK;
}

int serve_file(ne_socket *sock, void *ud)
{
    char buffer[BUFSIZ];
    struct stat st;
    struct serve_file_args *args = ud;
    ssize_t ret;
    int fd;

    CALL(discard_request(sock));

    ne_sock_fullread(sock, buffer, clength);

    fd = open(args->fname, O_RDONLY);
    if (fd < 0) {
	SEND_STRING(sock, 
		    "HTTP/1.0 404 File Not Found\r\n"
		    "Content-Length: 0\r\n\r\n");
	return 0;
    }

    ONN("fstat fd", fstat(fd, &st));

    SEND_STRING(sock, "HTTP/1.0 200 OK\r\n");
    if (args->chunks) {
	sprintf(buffer, "Transfer-Encoding: chunked\r\n");
    } else {
	sprintf(buffer, "Content-Length: %" NE_FMT_OFF_T "\r\n", 
		st.st_size);
    } 
    
    if (args->headers) {
	strcat(buffer, args->headers);
    }

    strcat(buffer, "\r\n");

    SEND_STRING(sock, buffer);

    NE_DEBUG(NE_DBG_HTTP, "Serving %s (%" NE_FMT_OFF_T " bytes).\n",
	     args->fname, st.st_size);

    if (args->chunks) {
	char buf[1024];
	
	while ((ret = read(fd, &buf, args->chunks)) > 0) {
	    /* this is a small integer, cast it explicitly to avoid
	     * warnings with printing an ssize_t. */
	    sprintf(buffer, "%x\r\n", (unsigned int)ret);
	    SEND_STRING(sock, buffer);
	    ONN("writing body", ne_sock_fullwrite(sock, buf, ret));
	    SEND_STRING(sock, "\r\n");
	}
	
	SEND_STRING(sock, "0\r\n\r\n");
	
    } else {
	while ((ret = read(fd, buffer, BUFSIZ)) > 0) {
	    ONN("writing body", ne_sock_fullwrite(sock, buffer, ret));
	}
    }

    ONN("error reading from file", ret < 0);

    (void) close(fd);

    return OK;
}
