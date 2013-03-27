/* 
   Tests for LFS support in neon
   Copyright (C) 2004-2006, Joe Orton <joe@manyfish.co.uk>

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

#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "ne_request.h"

#include "child.h"
#include "utils.h"
#include "tests.h"

#ifndef INT64_C
#define INT64_C(x) x ## LL
#endif

static const char data[] = "Hello, world.\n";
static off64_t point = INT64_C(2) << 32;
#define SPARSE "sparse.bin"

/* make a sparse large file */
static int make_sparse_file(void)
{
    int fd = open64(SPARSE, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    ONN("could not create large file " SPARSE, fd < 0);
    ONN("seek to point", lseek64(fd, point, SEEK_SET) != point);
    ONN("could not write to file", 
        write(fd, data, strlen(data)) != (ssize_t)strlen(data));
    ONN("close failed", close(fd));
    
    return OK;
}

/* server function which checks that the request body sent was the
 * same as the 'data' array. */
static int serve_check_body(ne_socket *sock, void *userdata)
{
    CALL(discard_request(sock));

    if (clength != (ssize_t)strlen(data)) {
        CALL(discard_body(sock));
        SEND_STRING(sock, "HTTP/1.0 400 Bad Request Body Length\r\n"
                    "\r\n");
    } else {
        char buf[20];
        
        if (ne_sock_fullread(sock, buf, clength) == 0) {
            SEND_STRING(sock, "HTTP/1.0 200 OK Then!\r\n\r\n");
        }
    }
    return 0;
}

/* sends a small segment of the file from a high offset. */
static int send_high_offset(void)
{
    int ret, fd = open64(SPARSE, O_RDONLY);
    ne_session *sess;
    ne_request *req;

    ONN("could not open sparse file", fd < 0);

    CALL(make_session(&sess, serve_check_body, NULL));
    
    req = ne_request_create(sess, "PUT", "/sparse");
    ne_set_request_body_fd(req, fd, point, strlen(data));
    ret = ne_request_dispatch(req);
    CALL(await_server());
    ONV(ret != NE_OK || ne_get_status(req)->klass != 2,
        ("request failed: %s", ne_get_error(sess)));
    ne_request_destroy(req);
    ne_session_destroy(sess);
    close(fd);
    return OK;
}

#if 1
#define RESPSIZE INT64_C(4295008256)
#define RESPSTR "4295008256"
#else
#define RESPSIZE INT64_C(2147491840) /* 2^31+8192 */
#define RESPSTR "2147491840" 
#endif

/* Reads a request, sends a large response, reads a request, then
 * sends a little response. */
static int serve_large_response(ne_socket *sock, void *ud)
{
    int n = 0;
    char empty[8192];

    CALL(discard_request(sock));

    SEND_STRING(sock, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: " RESPSTR "\r\n"
                "Server: BigFileServerTM\r\n" "\r\n");
    
    memset(empty, 0, sizeof empty);

    for (n = 0; n < RESPSIZE/sizeof(empty); n++) {
        if (ne_sock_fullwrite(sock, empty, sizeof empty)) {
            NE_DEBUG(NE_DBG_SOCKET, "fullwrite failed\n");
            return 1;
        }
    }

    NE_DEBUG(NE_DBG_SOCKET, "Wrote %d lots of %d\n", n, (int)sizeof empty);
    
    CALL(discard_request(sock));

    SEND_STRING(sock, "HTTP/1.1 200 OK\r\n"
                "Connection: close\r\n\r\n");
    
    return 0;
}

static int read_large_response(void)
{
    ne_session *sess;
    ne_request *req;
    off64_t count = 0;
    int ret;
    char buf[8192];
#ifdef NE_DEBUGGING
    int old_mask = ne_debug_mask;
#endif

    CALL(make_session(&sess, serve_large_response, NULL));

    req = ne_request_create(sess, "GET", "/foo");

    ret = ne_begin_request(req);

#ifdef NE_DEBUGGING
    ne_debug_init(ne_debug_stream, ne_debug_mask & ~(NE_DBG_HTTPBODY|NE_DBG_HTTP));
#endif

    if (ret == NE_OK) {
        while ((ret = ne_read_response_block(req, buf, sizeof buf)) > 0)
            count += ret;
        if (ret == NE_OK)
            ret = ne_end_request(req);
    }

#ifdef NE_DEBUGGING
    ne_debug_init(ne_debug_stream, old_mask);
#endif
        
    ONV(ret, ("request failed: %s", ne_get_error(sess)));
    ONV(count != RESPSIZE, 
        ("response body was %" NE_FMT_OFF64_T " not %" NE_FMT_OFF64_T,
         count, RESPSIZE));

    ne_request_destroy(req);

    CALL(any_2xx_request(sess, "/bar"));
    CALL(await_server());
    ne_session_destroy(sess);
    return OK;
}

ne_test tests[] = {
    T(make_sparse_file),
    T(send_high_offset),
    T(read_large_response),
    T(NULL),
};
