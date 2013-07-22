/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  "HTTP" file system			File: cfe_httpfs.c
    *  
    *  This filesystem driver lets you read files from an HTTP
    *  server.  
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_console.h"

#include "cfe.h"

#include "bsp_config.h"

#if CFG_TCP && CFG_HTTPFS

#include "net_ebuf.h"
#include "net_api.h"


/*  *********************************************************************
    *  HTTP context
    ********************************************************************* */

/*
 * File system context - describes overall file system info,
 * such as the handle to the underlying device.
 */

typedef struct http_fsctx_s {
    int http_tmp;			/* context not really needed */
} http_fsctx_t;

/*
 * File context - describes an open file on the file system.
 */

#define HTTP_BUFSIZE	1024
typedef struct http_file_s {
    http_fsctx_t *http_fsctx;
    int http_socket;
    uint8_t http_buffer[HTTP_BUFSIZE];
    uint8_t *http_bptr;
    int http_blen;
    int http_offset;
    char *http_filename;
} http_file_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int http_fileop_init(void **fsctx,void *devicename);
static int http_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int http_fileop_read(void *ref,uint8_t *buf,int len);
static int http_fileop_write(void *ref,uint8_t *buf,int len);
static int http_fileop_seek(void *ref,int offset,int how);
static void http_fileop_close(void *ref);
static void http_fileop_uninit(void *fsctx);

/*  *********************************************************************
    *  RAW fileio dispatch table
    ********************************************************************* */

const fileio_dispatch_t http_fileops = {
    "http",
    FSYS_TYPE_NETWORK,
    http_fileop_init,
    http_fileop_open,
    http_fileop_read,
    http_fileop_write,
    http_fileop_seek,
    http_fileop_close,
    http_fileop_uninit
};

static int http_fileop_init(void **newfsctx,void *dev)
{
    http_fsctx_t *fsctx;

    *newfsctx = NULL;

    fsctx = KMALLOC(sizeof(http_fsctx_t),0);
    if (!fsctx) {
	return CFE_ERR_NOMEM;
	}

    fsctx->http_tmp = 0;
    *newfsctx = fsctx;

    return 0;
}

static int http_fileop_open(void **ref,void *fsctx_arg,char *filename,int mode)
{
    http_fsctx_t *fsctx;
    http_file_t *file;
    char temp[200];
    char *hostname, *filen;
    int hlen;
    int termidx;
    int res;
    int err = 0;
    char *hptr;
    char *tok;
    uint8_t hostaddr[IP_ADDR_LEN];
    uint8_t termstr[4];
    uint8_t b;

    if (mode != FILE_MODE_READ) return CFE_ERR_UNSUPPORTED;

    fsctx = (http_fsctx_t *) fsctx_arg;

    file = KMALLOC(sizeof(http_file_t),0);
    if (!file) {
	return CFE_ERR_NOMEM;
	}

    file->http_filename = lib_strdup(filename);
    if (!file->http_filename) {
	KFREE(file);
	return CFE_ERR_NOMEM;
	}

    lib_chop_filename(file->http_filename,&hostname,&filen);

    /*
     * Look up remote host
     */

    res = dns_lookup(hostname,hostaddr);
    if (res < 0) {
	KFREE(file);
	return res;
	}

    file->http_socket = tcp_socket();
    if (file->http_socket < 0) {
	KFREE(file->http_filename);
	KFREE(file);
	return -1;
	}

    /*
     * Connect to remote host.
     */

    tcp_setflags(file->http_socket,0);	/* set socket to blocking */
    res = tcp_connect(file->http_socket,hostaddr,80);

    if (res < 0) {
	tcp_close(file->http_socket);
	KFREE(file->http_filename);
	KFREE(file);
	return res;
	}

    /*
     * Send GET command.  Supply the hostname (for HTTP 1.1 requirements)
     * and set the connection to close as soon as all the data is received.
     */

    hlen = sprintf(temp,"GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",filen,hostname);

    res = tcp_send(file->http_socket,temp,hlen);
    if (res < 0) {
	tcp_close(file->http_socket);
	KFREE(file->http_filename);
	KFREE(file);
	return res;
	}

    /*
     * Read bytes until we either reach EOF or we see "\r\n\r\n"
     * This is the server's status string.
     */

    termstr[0] = '\r'; termstr[1] = '\n';
    termstr[2] = '\r'; termstr[3] = '\n';
    termidx = 0;

    file->http_offset = 0;
    file->http_blen = 0;
    file->http_bptr = file->http_buffer;

    res = 0;
    for (;;) {
	res = tcp_recv(file->http_socket,&b,1);
	if (res < 0) break;
	if (b == termstr[termidx]) {
	    termidx++;
	    if (termidx == 4) break;
	    }
	else {
	    termidx = 0;
	    }

	/*
	 * Save the bytes from the header up to our buffer
	 * size.  It's okay if we don't save it all,
	 * since all we want is the result code which comes
	 * first.
	 */

	if (file->http_blen < (HTTP_BUFSIZE-1)) {
	    *(file->http_bptr) = b;
	    file->http_bptr++;
	    file->http_blen++;
	    }
	}

    /*
     * Premature EOFs are not good, bail now.
     */

    if (res < 0) {
	err = CFE_ERR_EOF;
	goto protocolerr;
	}

    /*
     * Skip past the HTTP response header and grab the result code.
     * Sanity check it a little.
     */

    *(file->http_bptr) = 0;

    hptr = file->http_buffer;
    tok = lib_gettoken(&hptr);
    if (!tok || (memcmp(tok,"HTTP",4) != 0)) {
	err = CFE_ERR_PROTOCOLERR;
	goto protocolerr;
	}

    tok = lib_gettoken(&hptr);
    if (!tok) {
	err = CFE_ERR_PROTOCOLERR;
	goto protocolerr;
	}

    switch (lib_atoi(tok)) {
	case 200:
	    err = 0;
	    break;
	case 404:
	    err = CFE_ERR_FILENOTFOUND;
	    break;

	}

    /*
     * If we get to here, the file is okay and we're about to receive data.
     */

    if (err == 0) {
	*ref = file;
	return 0;
	}

protocolerr:
    tcp_close(file->http_socket);
    KFREE(file->http_filename);
    KFREE(file);
    *ref = NULL;
    return err;
}

static int http_fileop_read(void *ref,uint8_t *buf,int len)
{
    http_file_t *file = (http_file_t *) ref;
    int res;

    res = tcp_recv(file->http_socket,buf,len);

    if (res > 0) {
	file->http_offset += res;
	return res;
	}

    return 0;		/* Any error becomes "EOF" */
}

static int http_fileop_write(void *ref,uint8_t *buf,int len)
{
    return CFE_ERR_UNSUPPORTED;
}

static int http_fileop_seek(void *ref,int offset,int how)
{
    http_file_t *file = (http_file_t *) ref;
    int newoffset;
    int res;
    int buflen;

    switch (how) {
	case FILE_SEEK_BEGINNING:
	    newoffset = offset;
	    break;
	case FILE_SEEK_CURRENT:
	    newoffset = file->http_offset + offset;
	    break;
	default:
	    newoffset = offset;
	    break;
	}

    /*
     * Can't seek backwards.
     */
    if (newoffset < file->http_offset) {
	return CFE_ERR_UNSUPPORTED;
	}

    /*
     * Eat data till offset reaches where we want.
     */

    while (file->http_offset != newoffset) {
	buflen = HTTP_BUFSIZE;
	if (buflen > (newoffset - file->http_offset)) buflen = (newoffset-file->http_offset);
	res = tcp_recv(file->http_socket,file->http_buffer,buflen);
	if (res < 0) break;
	file->http_offset += res;
	}

    return file->http_offset;
}


static void http_fileop_close(void *ref)
{
    http_file_t *file = (http_file_t *) ref;

    tcp_close(file->http_socket);
    KFREE(file->http_filename);
    KFREE(file);
}

static void http_fileop_uninit(void *fsctx_arg)
{
    http_fsctx_t *fsctx = (http_fsctx_t *) fsctx_arg;

    KFREE(fsctx);
}

#endif
