/* $Id: http_client.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "test.h"

#if INCLUDE_HTTP_CLIENT_TEST

#define THIS_FILE	    "test_http"
//#define VERBOSE
#define STR_PREC(s) (int)s.slen, s.ptr
#define USE_LOCAL_SERVER

#include <pjlib.h>
#include <pjlib-util.h>

#define ACTION_REPLY	0
#define ACTION_IGNORE	-1

static struct server_t
{
    pj_sock_t	     sock;
    pj_uint16_t	     port;
    pj_thread_t	    *thread;

    /* Action:
     *	0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int		    action;
    pj_bool_t       send_content_length;
    unsigned	    data_size;
    unsigned        buf_size;
} g_server;

static pj_bool_t thread_quit;
static pj_timer_heap_t *timer_heap;
static pj_ioqueue_t *ioqueue;
static pj_pool_t *pool;
static pj_http_req *http_req;
static pj_bool_t test_cancel = PJ_FALSE;
static pj_size_t total_size;
static pj_size_t send_size = 0;
static pj_status_t sstatus;
static pj_sockaddr_in addr;
static int counter = 0;

static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;
    char *pkt = (char*)pj_pool_alloc(pool, srv->buf_size);
    pj_sock_t newsock = PJ_INVALID_SOCKET;

    while (!thread_quit) {
	pj_ssize_t pkt_len;
	int rc;
        pj_fd_set_t rset;
	pj_time_val timeout = {0, 500};

	while (!thread_quit) {
	    PJ_FD_ZERO(&rset);
	    PJ_FD_SET(srv->sock, &rset);
	    rc = pj_sock_select(srv->sock+1, &rset, NULL, NULL, &timeout);
	    if (rc != 1) {
		continue;
	    }

	    rc = pj_sock_accept(srv->sock, &newsock, NULL, NULL);
	    if (rc == PJ_SUCCESS) {
		break;
	    }
	}

	if (thread_quit)
	    break;

	while (!thread_quit) {
            PJ_FD_ZERO(&rset);
            PJ_FD_SET(newsock, &rset);
            rc = pj_sock_select(newsock+1, &rset, NULL, NULL, &timeout);
            if (rc != 1) {
        	PJ_LOG(3,("http test", "client timeout"));
                continue;
            }

            pkt_len = srv->buf_size;
            rc = pj_sock_recv(newsock, pkt, &pkt_len, 0);
            if (rc == PJ_SUCCESS) {
                break;
            }
        }

	if (thread_quit)
	    break;

	/* Simulate network RTT */
	pj_thread_sleep(50);

	if (srv->action == ACTION_IGNORE) {
	    continue;
	} else if (srv->action == ACTION_REPLY) {
            unsigned send_size = 0, ctr = 0;
            pj_ansi_sprintf(pkt, "HTTP/1.0 200 OK\r\n");
            if (srv->send_content_length) {
                pj_ansi_sprintf(pkt + pj_ansi_strlen(pkt), 
                                "Content-Length: %d\r\n",
                                srv->data_size);
            }
            pj_ansi_sprintf(pkt + pj_ansi_strlen(pkt), "\r\n");
            pkt_len = pj_ansi_strlen(pkt);
            rc = pj_sock_send(newsock, pkt, &pkt_len, 0);
            if (rc != PJ_SUCCESS) {
        	pj_sock_close(newsock);
        	continue;
            }
            while (send_size < srv->data_size) {
                pkt_len = srv->data_size - send_size;
                if (pkt_len > (signed)srv->buf_size)
                    pkt_len = srv->buf_size;
                send_size += pkt_len;
                pj_create_random_string(pkt, pkt_len);
                pj_ansi_sprintf(pkt, "\nPacket: %d", ++ctr);
                pkt[pj_ansi_strlen(pkt)] = '\n';
		rc = pj_sock_send(newsock, pkt, &pkt_len, 0);
		if (rc != PJ_SUCCESS)
		    break;
            }
            pj_sock_close(newsock);
	}
    }

    return 0;
}

static void on_data_read(pj_http_req *hreq, void *data, pj_size_t size)
{
    PJ_UNUSED_ARG(hreq);
    PJ_UNUSED_ARG(data);

    PJ_LOG(5, (THIS_FILE, "\nData received: %d bytes", size));
    if (size > 0) {
#ifdef VERBOSE
        printf("%.*s\n", (int)size, (char *)data);
#endif
    }
}

static void on_send_data(pj_http_req *hreq,
                         void **data, pj_size_t *size)
{
    char *sdata;
    pj_size_t sendsz = 8397;

    PJ_UNUSED_ARG(hreq);

    if (send_size + sendsz > total_size) {
        sendsz = total_size - send_size;
    }
    send_size += sendsz;

    sdata = (char*)pj_pool_alloc(pool, sendsz);
    pj_create_random_string(sdata, sendsz);
    pj_ansi_sprintf(sdata, "\nSegment #%d\n", ++counter);
    *data = sdata;
    *size = sendsz;

    PJ_LOG(5, (THIS_FILE, "\nSending data progress: %d out of %d bytes", 
           send_size, total_size));
}


static void on_complete(pj_http_req *hreq, pj_status_t status,
                        const pj_http_resp *resp)
{
    PJ_UNUSED_ARG(hreq);

    if (status == PJ_ECANCELLED) {
        PJ_LOG(5, (THIS_FILE, "Request cancelled"));
        return;
    } else if (status == PJ_ETIMEDOUT) {
        PJ_LOG(5, (THIS_FILE, "Request timed out!"));
        return;
    } else if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Error %d", status));
        return;
    }
    PJ_LOG(5, (THIS_FILE, "\nData completed: %d bytes", resp->size));
    if (resp->size > 0 && resp->data) {
#ifdef VERBOSE
        printf("%.*s\n", (int)resp->size, (char *)resp->data);
#endif
    }
}

static void on_response(pj_http_req *hreq, const pj_http_resp *resp)
{
    pj_size_t i;

    PJ_UNUSED_ARG(hreq);
    PJ_UNUSED_ARG(resp);
    PJ_UNUSED_ARG(i);

#ifdef VERBOSE
    printf("%.*s, %d, %.*s\n", STR_PREC(resp->version),
           resp->status_code, STR_PREC(resp->reason));
    for (i = 0; i < resp->headers.count; i++) {
        printf("%.*s : %.*s\n", 
               STR_PREC(resp->headers.header[i].name),
               STR_PREC(resp->headers.header[i].value));
    }
#endif

    if (test_cancel) {
	/* Need to delay closing the client socket here, otherwise the
	 * server will get SIGPIPE when sending response.
	 */
	pj_thread_sleep(100);
        pj_http_req_cancel(hreq, PJ_TRUE);
        test_cancel = PJ_FALSE;
    }
}


pj_status_t parse_url(int inst_id, const char *url, pj_http_url *hurl)
{
    pj_str_t surl;
    pj_status_t status;

    pj_cstr(&surl, url);
    status = pj_http_req_parse_url(inst_id, &surl, hurl);
#ifdef VERBOSE
    if (!status) {
        printf("URL: %s\nProtocol: %.*s\nHost: %.*s\nPort: %d\nPath: %.*s\n\n",
               url, STR_PREC(hurl->protocol), STR_PREC(hurl->host),
               hurl->port, STR_PREC(hurl->path));
    } else {
    }
#endif
    return status;
}

static int parse_url_test()
{
    struct test_data
    {
	char *url;
	pj_status_t result;
	const char *username;
	const char *passwd;
	const char *host;
	int port;
	const char *path;
    } test_data[] =
    {
	/* Simple URL without '/' in the end */
        {"http://www.pjsip.org", PJ_SUCCESS, "", "", "www.pjsip.org", 80, "/"},

        /* Simple URL with port number but without '/' in the end */
        {"http://pjsip.org:8080", PJ_SUCCESS, "", "", "pjsip.org", 8080, "/"},

        /* URL with path */
        {"http://127.0.0.1:280/Joomla/index.php?option=com_content&task=view&id=5&Itemid=6",
        	PJ_SUCCESS, "", "", "127.0.0.1", 280,
        	"/Joomla/index.php?option=com_content&task=view&id=5&Itemid=6"},

	/* URL with port and path */
	{"http://pjsip.org:81/about-us/", PJ_SUCCESS, "", "", "pjsip.org", 81, "/about-us/"},

	/* unsupported protocol */
	{"ftp://www.pjsip.org", PJ_ENOTSUP, "", "", "", 80, ""},

	/* invalid format */
	{"http:/pjsip.org/about-us/", PJLIB_UTIL_EHTTPINURL, "", "", "", 80, ""},

	/* invalid port number */
	{"http://pjsip.org:xyz/", PJLIB_UTIL_EHTTPINPORT, "", "", "", 80, ""},

	/* with username and password */
	{"http://user:pass@pjsip.org", PJ_SUCCESS, "user", "pass", "pjsip.org", 80, "/"},

	/* password only*/
	{"http://:pass@pjsip.org", PJ_SUCCESS, "", "pass", "pjsip.org", 80, "/"},

	/* user only*/
	{"http://user:@pjsip.org", PJ_SUCCESS, "user", "", "pjsip.org", 80, "/"},

	/* empty username and passwd*/
	{"http://:@pjsip.org", PJ_SUCCESS, "", "", "pjsip.org", 80, "/"},

	/* '@' character in username and path */
	{"http://user@pjsip.org/@", PJ_SUCCESS, "user", "", "pjsip.org", 80, "/@"},

	/* '@' character in path */
	{"http://pjsip.org/@", PJ_SUCCESS, "", "", "pjsip.org", 80, "/@"},

	/* '@' character in path */
	{"http://pjsip.org/one@", PJ_SUCCESS, "", "", "pjsip.org", 80, "/one@"},

	/* Invalid URL */
	{"http://:", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://@", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:/", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:///", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://@/", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:///@", PJ_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://:::", PJ_EINVAL, "", "", "", 0, ""},
    };
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(test_data); ++i) {
	struct test_data *ptd;
	pj_http_url hurl;
	pj_status_t status;

	ptd = &test_data[i];

	PJ_LOG(3, (THIS_FILE, ".. %s", ptd->url));
	status = parse_url(0, ptd->url, &hurl);

	if (status != ptd->result) {
	    PJ_LOG(3,(THIS_FILE, "%d", status));
	    return -11;
	}
	if (status != PJ_SUCCESS)
	    continue;
	if (pj_strcmp2(&hurl.username, ptd->username))
	    return -12;
	if (pj_strcmp2(&hurl.passwd, ptd->passwd))
	    return -13;
	if (pj_strcmp2(&hurl.host, ptd->host))
	    return -14;
	if (hurl.port != ptd->port)
	    return -15;
	if (pj_strcmp2(&hurl.path, ptd->path))
	    return -16;
    }

    return 0;
}

/* 
 * GET request scenario 1: using on_response() and on_data_read()
 * Server replies with content-length. Application cancels the 
 * request upon receiving the response, then start it again.
 */
int http_client_test1()
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_http_req_param param;
    char urlbuf[80];

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;
    pj_http_req_param_default(&param);

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -31;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -32;

#ifdef USE_LOCAL_SERVER

    thread_quit = PJ_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = PJ_TRUE;
    g_server.data_size = 2970;
    g_server.buf_size = 1024;

    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != PJ_SUCCESS)
        return -41;

    pj_sockaddr_in_init(&addr, NULL, 0);

    sstatus = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != PJ_SUCCESS)
        return -43;

    {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);
	sstatus = pj_sock_getsockname(g_server.sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    return -44;
	g_server.port = pj_sockaddr_in_get_port(&addr);
	pj_ansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/about-us/",
			 g_server.port);
	url = pj_str(urlbuf);
    }

    sstatus = pj_sock_listen(g_server.sock, 8);
    if (sstatus != PJ_SUCCESS)
        return -45;

    sstatus = pj_thread_create(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != PJ_SUCCESS)
        return -47;

#else
    pj_cstr(&url, "http://www.teluu.com/about-us/");
#endif

    if (pj_http_req_create(0, pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -33;

    test_cancel = PJ_TRUE;
    if (pj_http_req_start(http_req))
        return -35;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

    if (pj_http_req_start(http_req))
        return -37;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_TRUE;
    pj_thread_join(g_server.thread);
    pj_sock_close(g_server.sock);
#endif

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* 
 * GET request scenario 2: using on_complete() to get the 
 * complete data. Server does not reply with content-length.
 * Request timed out, application sets a longer timeout, then
 * then restart the request.
 */
int http_client_test2()
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_http_req_param param;
    pj_time_val timeout;
    char urlbuf[80];

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_response = &on_response;
    pj_http_req_param_default(&param);

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -41;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -42;

#ifdef USE_LOCAL_SERVER

    pj_cstr(&url, "http://127.0.0.1:380");
    param.timeout.sec = 0;
    param.timeout.msec = 2000;

    thread_quit = PJ_FALSE;
    g_server.action = ACTION_IGNORE;
    g_server.send_content_length = PJ_FALSE;
    g_server.data_size = 4173;
    g_server.buf_size = 1024;

    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != PJ_SUCCESS)
        return -41;

    pj_sockaddr_in_init(&addr, NULL, 0);

    sstatus = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != PJ_SUCCESS)
        return -43;

    {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);
	sstatus = pj_sock_getsockname(g_server.sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    return -44;
	g_server.port = pj_sockaddr_in_get_port(&addr);
	pj_ansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d",
			 g_server.port);
	url = pj_str(urlbuf);
    }

    sstatus = pj_sock_listen(g_server.sock, 8);
    if (sstatus != PJ_SUCCESS)
        return -45;

    sstatus = pj_thread_create(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != PJ_SUCCESS)
        return -47;

#else
    pj_cstr(&url, "http://www.google.com.sg");
    param.timeout.sec = 0;
    param.timeout.msec = 50;
#endif

    pj_http_headers_add_elmt2(&param.headers, (char*)"Accept",
			     (char*)"image/gif, image/x-xbitmap, image/jpeg, "
				    "image/pjpeg, application/x-ms-application,"
				    " application/vnd.ms-xpsdocument, "
			            "application/xaml+xml, "
			            "application/x-ms-xbap, "
			            "application/x-shockwave-flash, "
			            "application/vnd.ms-excel, "
			            "application/vnd.ms-powerpoint, "
			            "application/msword, */*");
    pj_http_headers_add_elmt2(&param.headers, (char*)"Accept-Language",
	                      (char*)"en-sg");
    pj_http_headers_add_elmt2(&param.headers, (char*)"User-Agent",
                              (char*)"Mozilla/4.0 (compatible; MSIE 7.0; "
                                     "Windows NT 6.0; SLCC1; "
                                     ".NET CLR 2.0.50727; "
                                     ".NET CLR 3.0.04506)");
    if (pj_http_req_create(0, pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -43;

    if (pj_http_req_start(http_req))
        return -45;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    g_server.action = ACTION_REPLY;
#endif

    timeout.sec = 0; timeout.msec = 10000;
    pj_http_req_set_timeout(http_req, &timeout);
    if (pj_http_req_start(http_req))
        return -47;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_TRUE;
    pj_thread_join(g_server.thread);
    pj_sock_close(g_server.sock);
#endif

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/*
 * PUT request scenario 1: sending the whole data at once
 */
int http_client_test_put1()
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_http_req_param param;
    char *data;
    int length = 3875;
    char urlbuf[80];

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -51;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -52;

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = PJ_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 4096;

    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != PJ_SUCCESS)
        return -41;

    pj_sockaddr_in_init(&addr, NULL, 0);

    sstatus = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != PJ_SUCCESS)
        return -43;

    {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);
	sstatus = pj_sock_getsockname(g_server.sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    return -44;
	g_server.port = pj_sockaddr_in_get_port(&addr);
	pj_ansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test.txt",
			 g_server.port);
	url = pj_str(urlbuf);
    }

    sstatus = pj_sock_listen(g_server.sock, 8);
    if (sstatus != PJ_SUCCESS)
        return -45;

    sstatus = pj_thread_create(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != PJ_SUCCESS)
        return -47;

#else
    pj_cstr(&url, "http://127.0.0.1:280/test/test.txt");

#endif

    pj_http_req_param_default(&param);
    pj_strset2(&param.method, (char*)"PUT");
    data = (char*)pj_pool_alloc(pool, length);
    pj_create_random_string(data, length);
    pj_ansi_sprintf(data, "PUT test\n");
    param.reqdata.data = data;
    param.reqdata.size = length;
    if (pj_http_req_create(0, pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -53;

    if (pj_http_req_start(http_req))
        return -55;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_TRUE;
    pj_thread_join(g_server.thread);
    pj_sock_close(g_server.sock);
#endif

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/*
 * PUT request scenario 2: using on_send_data() callback to
 * sending the data in chunks
 */
int http_client_test_put2()
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_http_req_param param;
    char urlbuf[80];

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_send_data = &on_send_data;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -51;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -52;

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = PJ_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 16384;

    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != PJ_SUCCESS)
        return -41;

    pj_sockaddr_in_init(&addr, NULL, 0);

    sstatus = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != PJ_SUCCESS)
        return -43;

    {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);
	sstatus = pj_sock_getsockname(g_server.sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    return -44;
	g_server.port = pj_sockaddr_in_get_port(&addr);
	pj_ansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test2.txt",
			 g_server.port);
	url = pj_str(urlbuf);
    }

    sstatus = pj_sock_listen(g_server.sock, 8);
    if (sstatus != PJ_SUCCESS)
        return -45;

    sstatus = pj_thread_create(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != PJ_SUCCESS)
        return -47;

#else
    pj_cstr(&url, "http://127.0.0.1:280/test/test2.txt");

#endif

    pj_http_req_param_default(&param);
    pj_strset2(&param.method, (char*)"PUT");
    total_size = 15383;
    send_size = 0;
    param.reqdata.total_size = total_size;
    if (pj_http_req_create(0, pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -53;

    if (pj_http_req_start(http_req))
        return -55;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_TRUE;
    pj_thread_join(g_server.thread);
    pj_sock_close(g_server.sock);
#endif

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

int http_client_test_delete()
{
    pj_str_t url;
    pj_http_req_callback hcb;
    pj_http_req_param param;
    char urlbuf[80];

    pj_bzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = pj_pool_create(mem, NULL, 8192, 4096, NULL);
    if (pj_timer_heap_create(pool, 16, &timer_heap))
        return -61;
    if (pj_ioqueue_create(pool, 16, &ioqueue))
        return -62;

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = PJ_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 1024;

    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != PJ_SUCCESS)
        return -41;

    pj_sockaddr_in_init(&addr, NULL, 0);

    sstatus = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != PJ_SUCCESS)
        return -43;

    {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);
	sstatus = pj_sock_getsockname(g_server.sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    return -44;
	g_server.port = pj_sockaddr_in_get_port(&addr);
	pj_ansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test2.txt",
			 g_server.port);
	url = pj_str(urlbuf);
    }

    sstatus = pj_sock_listen(g_server.sock, 8);
    if (sstatus != PJ_SUCCESS)
        return -45;

    sstatus = pj_thread_create(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != PJ_SUCCESS)
        return -47;

#else
    pj_cstr(&url, "http://127.0.0.1:280/test/test2.txt");
#endif

    pj_http_req_param_default(&param);
    pj_strset2(&param.method, (char*)"DELETE");
    if (pj_http_req_create(0, pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -63;

    if (pj_http_req_start(http_req))
        return -65;

    while (pj_http_req_is_running(http_req)) {
        pj_time_val delay = {0, 50};
	pj_ioqueue_poll(ioqueue, &delay);
	pj_timer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = PJ_TRUE;
    pj_thread_join(g_server.thread);
    pj_sock_close(g_server.sock);
#endif

    pj_http_req_destroy(http_req);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

int http_client_test()
{
    int rc;
    
    PJ_LOG(3, (THIS_FILE, "..Testing URL parsing"));
    rc = parse_url_test();
    if (rc)
        return rc;

    PJ_LOG(3, (THIS_FILE, "..Testing GET request scenario 1"));
    rc = http_client_test1();
    if (rc)
        return rc;

    PJ_LOG(3, (THIS_FILE, "..Testing GET request scenario 2"));
    rc = http_client_test2();
    if (rc)
        return rc;

    PJ_LOG(3, (THIS_FILE, "..Testing PUT request scenario 1"));
    rc = http_client_test_put1();
    if (rc)
        return rc;

    PJ_LOG(3, (THIS_FILE, "..Testing PUT request scenario 2"));
    rc = http_client_test_put2();
    if (rc)
        return rc;

    PJ_LOG(3, (THIS_FILE, "..Testing DELETE request"));
    rc = http_client_test_delete();
    if (rc)
        return rc;

    return PJ_SUCCESS;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_http_client_test;
#endif	/* INCLUDE_HTTP_CLIENT_TEST */
