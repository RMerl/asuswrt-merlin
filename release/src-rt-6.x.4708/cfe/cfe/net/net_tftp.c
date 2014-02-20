/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TFTP Client 				File: net_tftp.c
    *  
    *  This module contains a TFTP client.
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
#include "cfe_console.h"
#include "cfe_timer.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "cfe.h"

#include "cfe_loader.h"

#include "net_api.h"

#include "bsp_config.h"


/*  *********************************************************************
    *  TFTP protocol
    ********************************************************************* */

#define UDP_PROTO_TFTP 		69

#define TFTP_BLOCKSIZE 		512

#define TFTP_OP_RRQ		1
#define TFTP_OP_WRQ		2
#define TFTP_OP_DATA		3
#define TFTP_OP_ACK		4
#define TFTP_OP_ERROR		5

#define TFTP_ERR_DISKFULL	3

#ifndef TFTP_MAX_RETRIES
#define TFTP_MAX_RETRIES	8
#endif

#define TFTP_RRQ_TIMEOUT	CFE_HZ*5	/* ticks */
//#define TFTP_RECV_TIMEOUT	CFE_HZ*5	/* ticks */
#define TFTP_RECV_TIMEOUT	CFE_HZ*1	/* ticks */

#ifdef RESCUE_MODE
unsigned char tftpipfrom[4] = { 0xc0, 0xa8, 0x01, 0x01 };
unsigned char tftpipto[4] = { 0xc0, 0xa8, 0x01, 0x01 };
uint16_t ackport = 7777;
#endif

/*  *********************************************************************
    *  TFTP context
    ********************************************************************* */

typedef struct tftp_fsctx_s {
    int dummy;
} tftp_fsctx_t;

typedef struct tftp_info_s {
    int tftp_socket;
    uint8_t tftp_data[TFTP_BLOCKSIZE];
    int tftp_blklen;
    int tftp_blkoffset;
    int tftp_fileoffset;
    uint16_t tftp_blknum;
    uint8_t tftp_ipaddr[IP_ADDR_LEN];
    int tftp_lastblock;
    int tftp_error;
    int tftp_filemode;
    char *tftp_filename;
} tftp_info_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int tftp_fileop_init(void **fsctx,void *devicename);
static int tftp_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int tftp_fileop_read(void *ref,uint8_t *buf,int len);
static int tftp_fileop_write(void *ref,uint8_t *buf,int len);
static int tftp_fileop_seek(void *ref,int offset,int how);
static void tftp_fileop_close(void *ref);
static void tftp_fileop_uninit(void *fsctx);
#ifdef RESCUE_MODE
extern int send_rescueack(unsigned short no, unsigned short lo);
#endif

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

int tftp_max_retries = TFTP_MAX_RETRIES;
int tftp_rrq_timeout = TFTP_RRQ_TIMEOUT;
int tftp_recv_timeout = TFTP_RECV_TIMEOUT;

/*  *********************************************************************
    *  TFTP fileio dispatch table
    ********************************************************************* */

const fileio_dispatch_t tftp_fileops = {
    "tftp",
    LOADFLG_NOBB | FSYS_TYPE_NETWORK,
    tftp_fileop_init,
    tftp_fileop_open,
    tftp_fileop_read,
    tftp_fileop_write,
    tftp_fileop_seek,
    tftp_fileop_close,
    tftp_fileop_uninit
};

#ifdef RESCUE_MODE
#define ip_addriszero(a) (((a)[0]|(a)[1]|(a)[2]|(a)[3]) == 0)
static void ui_myshowifconfig(void)
{
        char *devname;
        uint8_t *addr;

        devname = (char *) net_getparam(NET_DEVNAME);
        if (devname == NULL) {
                xprintf("Network interface has not been configured\n");
                return;
        }
        xprintf("Device %s: ",devname);
        addr = net_getparam(NET_HWADDR);
        if (addr)
                xprintf(" hwaddr %a",addr);
        addr = net_getparam(NET_IPADDR);
        if (addr) {
                if (ip_addriszero(addr))
                        xprintf(", ipaddr not set");
                else
                        xprintf(", ipaddr %I",addr);
        }
        addr = net_getparam(NET_NETMASK);
        if (addr) {
                if (ip_addriszero(addr))
                        xprintf(", mask not set");
                else
                        xprintf(", mask %I",addr);
        }
        xprintf("\n");
        xprintf("        ");
        addr = net_getparam(NET_GATEWAY);
        if (addr) {
                if (ip_addriszero(addr))
                        xprintf("gateway not set");
                else
                        xprintf("gateway %I",addr);
        }
        addr = net_getparam(NET_NAMESERVER);
        if (addr) {
                if (ip_addriszero(addr))
                        xprintf(", nameserver not set");
        else
                xprintf(", nameserver %I",addr);
        }
        addr = net_getparam(NET_DOMAIN);
        if (addr) {
                xprintf(", domain %s",addr);
        }
        xprintf("\n");
}
#endif

/*  *********************************************************************
    *  _tftp_open(info,hostname,filename,mode)
    *  
    *  Open a file on a remote host, using the TFTP protocol.
    *  
    *  Input parameters: 
    *  	   info - TFTP information
    *  	   hostname - host name or IP address of remote host
    *  	   filename - name of file on remote system
    *      mode - file open mode, read or write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int _tftp_open(tftp_info_t *info,char *hostname,char *filename,int mode)
{
    ebuf_t *buf = NULL;
    const char *datamode = "octet";
    uint16_t type,error,block;
    int res;
    int retries;

    /*
     * Look up the remote host's IP address
     */

    res = dns_lookup(hostname,info->tftp_ipaddr);
    if (res < 0) return res;

    /*
     * Open a UDP socket to the TFTP server
     */    

    info->tftp_socket = udp_socket(UDP_PROTO_TFTP);
    info->tftp_lastblock = 0;
    info->tftp_error = 0;
    info->tftp_filemode = mode;

    /*
     * Try to send the RRQ packet to open the file
     */

    for (retries = 0; retries < tftp_max_retries; retries++) {

	buf = udp_alloc();
	if (!buf) break;

	if (info->tftp_filemode == FILE_MODE_READ) {
	    ebuf_append_u16_be(buf,TFTP_OP_RRQ);	/* read file */
	    }
	else {
	    ebuf_append_u16_be(buf,TFTP_OP_WRQ);	/* write file */
	    }
	ebuf_append_bytes(buf,filename,strlen(filename)+1);
	ebuf_append_bytes(buf,datamode,strlen(datamode)+1);

	udp_send(info->tftp_socket,buf,info->tftp_ipaddr);

	buf = udp_recv_with_timeout(info->tftp_socket,tftp_rrq_timeout);
	if (buf) break;
	}

    /*
     * If we got no response, bail now.
     */

    if (!buf) {
	udp_close(info->tftp_socket);
	info->tftp_socket = -1;
	return CFE_ERR_TIMEOUT;
	}

    /*
     * Otherwise, process the response.
     */

    ebuf_get_u16_be(buf,type);

    switch (type) {
	case TFTP_OP_ACK:
	    /*
	     * Acks are what we get back on a WRQ command,
	     * but are otherwise unexpected.
	     */
	    if (info->tftp_filemode == FILE_MODE_WRITE) {
		udp_connect(info->tftp_socket,(uint16_t) buf->eb_usrdata);
		info->tftp_blknum = 1;
		info->tftp_blklen = 0;
		udp_free(buf);
		return 0;
		break;
		}
	    /* fall through */
	case TFTP_OP_RRQ:
	case TFTP_OP_WRQ:
	default:
	    /* 
	     * we aren't expecting any of these messages
	     */
	    udp_free(buf);
	    udp_close(info->tftp_socket);
	    info->tftp_socket = -1;
	    return CFE_ERR_PROTOCOLERR;

	case TFTP_OP_ERROR:
	    ebuf_get_u16_be(buf,error);
	    xprintf("TFTP error %d: %s\n",error,ebuf_ptr(buf));
	    udp_free(buf);
	    udp_close(info->tftp_socket);
	    info->tftp_socket = -1;
	    return CFE_ERR_PROTOCOLERR;

	case TFTP_OP_DATA:
	    /*
	     * Yay, we've got data!  Store the first block.
	     */
	    ebuf_get_u16_be(buf,block);
	    udp_connect(info->tftp_socket,(uint16_t) buf->eb_usrdata);
	    info->tftp_blknum = block;
	    info->tftp_blklen = ebuf_length(buf);
	    ebuf_get_bytes(buf,info->tftp_data,ebuf_length(buf));
	    udp_free(buf);
	    if (info->tftp_blklen < TFTP_BLOCKSIZE) {
		info->tftp_lastblock = 1;		/* EOF */
		}
	    return 0;
	    break;
	    
	}
}


/*  *********************************************************************
    *  _tftp_readmore(info)
    *  
    *  Read the next block of the file.  We do this by acking the
    *  previous block.  Once that block is acked, the TFTP server
    *  should send the next block to us.
    *  
    *  Input parameters: 
    *  	   info - TFTP information
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int _tftp_readmore(tftp_info_t *info)
{
    ebuf_t *buf;
    uint16_t cmd,block;
    int retries;

    /*
     * If we've already read the last block, there is no more
     */

#ifdef RESCUE_MODE
    if (info->tftp_lastblock) {
        xprintf("- Last block -\n");
        return 1;
    }
    if (info->tftp_error) {
        xprintf("break !! tftp_error !!\n");
        return CFE_ERR_TIMEOUT;
    }
#else
    if (info->tftp_lastblock) return 1;
    if (info->tftp_error) return CFE_ERR_TIMEOUT;
#endif

    /*
     * Otherwise, ack the current block so another one will come
     */

    for (retries = 0; retries < tftp_max_retries; retries++) {

	buf = udp_alloc();
	if (!buf) return -1;

	/*
	 * Send the ack
	 */

	ebuf_append_u16_be(buf,TFTP_OP_ACK);
	ebuf_append_u16_be(buf,info->tftp_blknum);
	udp_send(info->tftp_socket,buf,info->tftp_ipaddr);

	/*
	 * Wait for some response, retransmitting as necessary
	 */

	buf = udp_recv_with_timeout(info->tftp_socket,tftp_max_retries>1 ? 50: tftp_recv_timeout);
#ifdef RESCUE_MODE
        if (!buf)
        {
                xprintf("break! no netctx or timer expired ! \n");
                continue;
        }
#else
        if (buf == NULL) continue;
#endif

	/*
	 * Got a response, make sure it's right
	 */

	ebuf_get_u16_be(buf,cmd);
	if (cmd != TFTP_OP_DATA) {
	    udp_free(buf);
	    continue;
	    }

	ebuf_get_u16_be(buf,block);
	if (block != ((info->tftp_blknum+1) % 0x10000)) {
	    udp_free(buf);
	    continue;
	    }

	/*
	 * It's the correct response.  Copy the user data
	 */

	info->tftp_blknum = block;
	info->tftp_blklen = ebuf_length(buf);
	ebuf_get_bytes(buf,info->tftp_data,ebuf_length(buf));
	udp_free(buf);
	break;
	}

    /*
     * If the block is less than 512 bytes long, it's the EOF block.
     */

    if (retries == tftp_max_retries) {
#ifdef RESCUE_MODE
        xprintf("break!! reach max retry!!\n");
#endif
	info->tftp_error = 1;
	return CFE_ERR_TIMEOUT;
	}

    if (info->tftp_blklen < TFTP_BLOCKSIZE) {
#ifdef RESCUE_MODE
        xprintf("- last blk -\n");
#endif
	info->tftp_lastblock = 1;		/* EOF */
	}

    return 0;
}

/*  *********************************************************************
    *  _tftp_writemore(info)
    *  
    *  Write the next block of the file, sending the data from our
    *  holding buffer.  Note that the holding buffer must be full
    *  or else we consider this to be an EOF.
    *  
    *  Input parameters: 
    *  	   info - TFTP information
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int _tftp_writemore(tftp_info_t *info)
{
    ebuf_t *buf;
    uint16_t cmd,block,error;
    int retries;

    /*
     * If we've already written the last block, there is no more
     */

    if (info->tftp_lastblock) return 1;
    if (info->tftp_error) return CFE_ERR_TIMEOUT;

    /*
     * Otherwise, send a block
     */
    for (retries = 0; retries < tftp_max_retries; retries++) {

	buf = udp_alloc();
	if (!buf) return -1;

	/*
	 * Send the data
	 */

	ebuf_append_u16_be(buf,TFTP_OP_DATA);
	ebuf_append_u16_be(buf,info->tftp_blknum);
	ebuf_append_bytes(buf,info->tftp_data,info->tftp_blklen);
	udp_send(info->tftp_socket,buf,info->tftp_ipaddr);

	/*
	 * Wait for some response, retransmitting as necessary
	 */

	buf = udp_recv_with_timeout(info->tftp_socket,tftp_max_retries>1 ? 50: tftp_recv_timeout);
	if (buf == NULL) continue;

	/*
	 * Got a response, make sure it's right
	 */

	ebuf_get_u16_be(buf,cmd);

	if (cmd == TFTP_OP_ERROR) {
	    ebuf_get_u16_be(buf,error);
	    xprintf("TFTP write error %d: %s\n",error,ebuf_ptr(buf));
	    info->tftp_error = 1;
	    info->tftp_lastblock = 1;
	    udp_free(buf);
	    return CFE_ERR_IOERR;
	    }

	if (cmd != TFTP_OP_ACK) {
	    udp_free(buf);
	    continue;
	    }

	ebuf_get_u16_be(buf,block);
	if (block != ((info->tftp_blknum+1) % 0x10000)) {
	    udp_free(buf);
	    continue;
	    }

	/*
	 * It's the correct response.  Update the block #
	 */

	info->tftp_blknum++;
	if (info->tftp_blklen != TFTP_BLOCKSIZE) info->tftp_lastblock = 1;
	udp_free(buf);
	break;
	}

    /*
     * If we had some failure, mark the stream with an error
     */

    if (retries == tftp_max_retries) {
	info->tftp_error = 1;
	return CFE_ERR_TIMEOUT;
	}

    return 0;
}


static int _tftpd_open(tftp_info_t *info,char *hostname,char *filename,int mode)
{
    ebuf_t *buf = NULL;
    uint16_t type;
    int res;
    int retries;
    char ch = 0;
#ifdef RESCUE_MODE
        uint8_t asuslink[13] = "ASUSSPACELINK";
        uint8_t maclink[13]="snxxxxxxxxxxx";
        unsigned char tftpmask[4] = { 0xff, 0xff, 0xff, 0x00 };
        int i;
        char tftpnull;
        uint8_t ipaddr[4] = { 0xc0, 0xa8, 0x01, 0x0c };
        uint8_t hwaddr[6] = { 0x00, 0xe0, 0x18, 0x00, 0x3e, 0xc4 };
#endif

    /*
     * Open a UDP socket
     */    

    info->tftp_socket = udp_socket(UDP_PROTO_TFTP);
    info->tftp_lastblock = 0;
    info->tftp_error = 0;
    info->tftp_filemode = mode;

    /*
     * Listen for requests
     */

    res = udp_bind(info->tftp_socket,UDP_PROTO_TFTP);
    if (res < 0) return res;

    res = CFE_ERR_TIMEOUT;

    for (retries = 0; retries < tftp_max_retries; retries++) {	// go load wait
	printf("..tftp retry wait %d\n", retries);
	while (console_status()) {
	    console_read(&ch,1);
	    if (ch == 3) break;
	}
	if (ch == 3) {
	    res = CFE_ERR_INTR;
	    break;
	}

	buf = udp_recv_with_timeout(info->tftp_socket,tftp_max_retries>1 ? 50: tftp_recv_timeout);
	if (buf == NULL) continue;

	/*
	 * Process the request
	 */

	ebuf_get_u16_be(buf,type);

	switch (type) {
	    case TFTP_OP_RRQ:
#ifdef RESCUE_MODE
                         udp_connect(info->tftp_socket,(uint16_t) buf->eb_usrdata);
                         ackport = buf->eb_usrdata;
                         memcpy(info->tftp_ipaddr,buf->eb_usrptr,IP_ADDR_LEN);
                         info->tftp_blknum = 1;
                         info->tftp_blklen = 0;
                         for (i=0; i<13; i++) {
                                if (buf->eb_ptr[i] != asuslink[i])
                                                break;
                         }
                         if (i==13) {
                                tftpipfrom[0]=buf->eb_ptr[16];
                                tftpipfrom[1]=buf->eb_ptr[15];
                                tftpipfrom[2]=buf->eb_ptr[14];
                                tftpipfrom[3]=buf->eb_ptr[13];
                                tftpipto[0]=buf->eb_usrptr[0];
                                tftpipto[1]=buf->eb_usrptr[1];
                                tftpipto[2]=buf->eb_usrptr[2];
                                tftpipto[3]=buf->eb_usrptr[3];
                                        net_setparam(NET_IPADDR,tftpipfrom);
                                        net_setparam(NET_NETMASK,tftpmask);
                                        net_setparam(NET_GATEWAY,tftpipfrom);
                                        ui_myshowifconfig();
                                        net_setnetvars();
                                        for (i=0; i<4; i++)
                                                ipaddr[i]=tftpipto[i];
                                        for (i=0; i<6; i++)
                                                hwaddr[i]=buf->eb_data[6+i];
                                        buf = udp_alloc();
                                        if (!buf) {
                                                res = CFE_ERR_TIMEOUT;
                                                break;
                                        }
                                        ebuf_append_u16_be(buf, 3);
                                        ebuf_append_u16_be(buf, 1);
                                        ebuf_append_bytes(buf,&tftpnull, 0);
                                        arp_delete(ipaddr);
                                        arp_add(ipaddr,hwaddr);
                                        udp_send(info->tftp_socket, buf, tftpipto);
                                }
                                else {
                                        for (i=0; i<13; i++) {
                                                if (buf->eb_ptr[i] != maclink[i])
                                                        break;
                                        }
                                        if (i==13) {
                                                tftpipfrom[0]=buf->eb_ptr[16];
                                                tftpipfrom[1]=buf->eb_ptr[15];
                                                tftpipfrom[2]=buf->eb_ptr[14];
                                                tftpipfrom[3]=buf->eb_ptr[13];
                                                tftpipto[0]=buf->eb_usrptr[0];
                                                tftpipto[1]=buf->eb_usrptr[1];
                                                tftpipto[2]=buf->eb_usrptr[2];
                                                tftpipto[3]=buf->eb_usrptr[3];
                                                net_setparam(NET_IPADDR,tftpipfrom);
                                                net_setparam(NET_NETMASK,tftpmask);
                                                net_setparam(NET_GATEWAY,tftpipfrom);
                                                ui_myshowifconfig();
                                                net_setnetvars();
                                                for (i=0; i<4; i++)
                                                        ipaddr[i]=tftpipto[i];
                                                for (i=0; i<6; i++)
                                                        hwaddr[i]=buf->eb_data[6+i];
                                                buf = udp_alloc();
                                                if (!buf) {
                                                        res = CFE_ERR_TIMEOUT;
                                                break;
                                        }
                                        ebuf_append_u16_be(buf, 3);
                                        ebuf_append_u16_be(buf, 1);
                                        ebuf_append_bytes(buf,&tftpnull, 0);
                                        arp_delete(ipaddr);
                                        arp_add(ipaddr,hwaddr);
                                        udp_send(info->tftp_socket, buf, tftpipto);
                                }
                        }
                        res = CFE_ERR_TIMEOUT;
#else
		udp_connect(info->tftp_socket,(uint16_t) buf->eb_usrdata);
		memcpy(info->tftp_ipaddr,buf->eb_usrptr,IP_ADDR_LEN);
		info->tftp_blknum = 1;
		info->tftp_blklen = 0;
		res = 0;
#endif
		break;

	    case TFTP_OP_WRQ:
		udp_connect(info->tftp_socket,(uint16_t) buf->eb_usrdata);
		memcpy(info->tftp_ipaddr,buf->eb_usrptr,IP_ADDR_LEN);
		info->tftp_blknum = 0;
		res = _tftp_readmore(info);
		break;

	    default:
		/* 
		 * we aren't expecting any of these messages
		 */
		res = CFE_ERR_PROTOCOLERR;
		break;
	}

	udp_free(buf);
	break;
    }

    if (res) {
	udp_close(info->tftp_socket);
	info->tftp_socket = -1;
    }

    return res;
}


/*  *********************************************************************
    *  _tftp_close(info)
    *  
    *  Close a TFTP file.  There are two cases for what we do
    *  here.  If we're closing the file mid-stream, send an error
    *  packet to tell the host we're getting out early.  Otherwise,
    *  just ack the final packet and the connection will close
    *  gracefully.
    *  
    *  Input parameters: 
    *  	   info - TFTP info
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int _tftp_close(tftp_info_t *info)
{
    ebuf_t *buf;
    const char *emsg = "transfer cancelled";	/* some error message */

    if (info->tftp_socket == -1) return 0;

    if (info->tftp_filemode == FILE_MODE_READ) {
	buf = udp_alloc();
	if (buf) {
	    /* If we're on the EOF packet, just send an ack */
	    if (info->tftp_lastblock) {
		ebuf_append_u16_be(buf,TFTP_OP_ACK);
		ebuf_append_u16_be(buf,info->tftp_blknum);
		}
	    else {
		ebuf_append_u16_be(buf,TFTP_OP_ERROR);
		ebuf_append_u16_be(buf,TFTP_ERR_DISKFULL);
		ebuf_append_bytes(buf,emsg,strlen(emsg)+1);
		}
	    udp_send(info->tftp_socket,buf,info->tftp_ipaddr);
	    }
	}
    else {
	/* Just flush out the remaining write data, if any */
	_tftp_writemore(info);
	}

    udp_close(info->tftp_socket);
    info->tftp_socket = -1;
    return 0;
}



/*  *********************************************************************
    *  tftp_fileop_init(fsctx,device)
    *  
    *  Create a file system device context for the TFTP service
    *  
    *  Input parameters: 
    *  	   fsctx - location to place context information
    *  	   device - underlying device (unused)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int tftp_fileop_init(void **fsctx,void *dev)
{
    void *ref;

    ref = KMALLOC(sizeof(tftp_fsctx_t),0);

    if (!ref) return CFE_ERR_NOMEM;
    *fsctx = ref;
    return 0;
}

/*  *********************************************************************
    *  tftp_fileop_open(ref,fsctx,filename,mode)
    *  
    *  This is the filesystem entry point for opening a TFTP file.
    *  Allocate a tftp_info structure, open the TFTP file, and
    *  return a handle.
    *  
    *  Input parameters: 
    *  	   ref - location to place reference data (the TFTP structure)
    *      fsctx - filesystem context
    *  	   filename - name of remote file to open
    *	   mode - FILE_MODE_READ or FILE_MODE_WRITE
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int tftp_fileop_open(void **ref,void *fsctx,char *filename,int mode)
{
    tftp_info_t *info;
    char *host;
    char *file;
    int res;

    if ((mode != FILE_MODE_READ) && (mode != FILE_MODE_WRITE)) {
	/* must be either read or write, not both */
	return CFE_ERR_UNSUPPORTED;
	}

    /* Allocate the tftp info structure */

    info = KMALLOC(sizeof(tftp_info_t),0);
    if (!info) {
	return CFE_ERR_NOMEM;
	}


    info->tftp_filename = lib_strdup(filename);
    if (!info->tftp_filename) {
	KFREE(info);
	return CFE_ERR_NOMEM;
	}

    lib_chop_filename(info->tftp_filename,&host,&file);

    /* Open the file */

    if (!*host && !*file) {
	/* TFTP server if hostname and filename are not specified */
#ifdef RESCUE_MODE
        xprintf("TFTP Server.\n");
#endif
	res = _tftpd_open(info,host,file,mode);
    } else {
	/* TFTP client otherwise */
#ifdef RESCUE_MODE
        xprintf("TFTP Client.\n");
#endif
	res = _tftp_open(info,host,file,mode);
    }

    if (res == 0) {
	info->tftp_blkoffset = 0;
	info->tftp_fileoffset = 0;
	*ref = info;
	}
    else {
	KFREE(info->tftp_filename);
	KFREE(info);
	*ref = NULL;
	}
    return res;
}

/*  *********************************************************************
    *  tftp_fileop_read(ref,buf,len)
    *  
    *  Read some bytes from the remote TFTP file.  Do this by copying
    *  data from the block buffer, and reading more data from the 
    *  remote file as necessary. 
    *  
    *  Input parameters: 
    *  	   ref - tftp_info structure
    *  	   buf - destination buffer address (NULL to read data and
    *  	         not copy it anywhere)
    *  	   len - number of bytes to read
    *  	   
    *  Return value:
    *  	   number of bytes read, 0 for EOF, <0 if an error occured.
    ********************************************************************* */

static int tftp_fileop_read(void *ref,uint8_t *buf,int len)
{
    tftp_info_t *info = (tftp_info_t *) ref;
    int copied = 0;
    int amtcopy;
    int res;
    
    if (info->tftp_error) return CFE_ERR_IOERR;
    if (info->tftp_filemode == FILE_MODE_WRITE) return CFE_ERR_UNSUPPORTED;

    while (len) {

	if (info->tftp_blkoffset >= info->tftp_blklen) break;
	amtcopy = len;

	if (amtcopy > (info->tftp_blklen-info->tftp_blkoffset)) {
	    amtcopy = (info->tftp_blklen-info->tftp_blkoffset);
	    }

	if (buf) {
	    memcpy(buf,&(info->tftp_data[info->tftp_blkoffset]),amtcopy);
	    buf += amtcopy;
	    }

	info->tftp_blkoffset += amtcopy;
	len -= amtcopy;
	info->tftp_fileoffset += amtcopy;
	copied += amtcopy;

	if (info->tftp_blkoffset >= info->tftp_blklen) {
	    res = _tftp_readmore(info);
	    if (res != 0) break;
	    info->tftp_blkoffset = 0;
	    }
	}

    return copied;
}

/*  *********************************************************************
    *  tftp_fileop_write(ref,buf,len)
    *  
    *  Write some bytes to the remote TFTP file.  Do this by copying
    *  data to the block buffer, and writing data to the  
    *  remote file as necessary. 
    *  
    *  Input parameters: 
    *  	   ref - tftp_info structure
    *  	   buf - source buffer address
    *  	   len - number of bytes to write
    *  	   
    *  Return value:
    *  	   number of bytes written, 0 for EOF, <0 if an error occured.
    ********************************************************************* */

static int tftp_fileop_write(void *ref,uint8_t *buf,int len)
{
    tftp_info_t *info = (tftp_info_t *) ref;
    int copied = 0;
    int amtcopy;
    int res;
    
    if (info->tftp_error) return CFE_ERR_IOERR;
    if (info->tftp_filemode == FILE_MODE_READ) return CFE_ERR_UNSUPPORTED;

    while (len) {

	amtcopy = len;
	if (amtcopy > (TFTP_BLOCKSIZE - info->tftp_blklen)) {
	    amtcopy = (TFTP_BLOCKSIZE - info->tftp_blklen);
	    }

	memcpy(&(info->tftp_data[info->tftp_blklen]),buf,amtcopy);
	buf += amtcopy;

	info->tftp_blklen += amtcopy;
	len -= amtcopy;
	info->tftp_fileoffset += amtcopy;
	copied += amtcopy;

	if (info->tftp_blklen == TFTP_BLOCKSIZE) {
	    res = _tftp_writemore(info);
	    if (res != 0) {
		break;
		}
	    info->tftp_blklen = 0;
	    }
	}

    return copied;
}

/*  *********************************************************************
    *  tftp_fileop_seek(ref,offset,how)
    *  
    *  Seek within a TFTP file.  Note that you can only seek *forward*,
    *  as TFTP doesn't really let you go backwards.  (I suppose you
    *  could reopen the file, but thus far nobody needs to go
    *  backwards).  You can only seek in a file in read mode.
    *  
    *  Input parameters: 
    *  	   ref - our tftp information
    *  	   offset - distance to move
    *  	   how - how to move, (FILE_SEEK_*)
    *  	   
    *  Return value:
    *  	   new offset, or <0 if an error occured.
    ********************************************************************* */

static int tftp_fileop_seek(void *ref,int offset,int how)
{
    tftp_info_t *info = (tftp_info_t *) ref;
    int delta;
    int startloc;
    int res;

    if (info->tftp_filemode == FILE_MODE_WRITE) return CFE_ERR_UNSUPPORTED;

    switch (how) {
	case FILE_SEEK_BEGINNING:
	    startloc = info->tftp_fileoffset;
	    break;
	case FILE_SEEK_CURRENT:
	    startloc = 0;
	    break;
	default:
	    startloc = 0;
	    break;
	}

    delta = offset - startloc;
    if (delta < 0) {
	xprintf("Warning: negative seek on tftp file attempted\n");
	return CFE_ERR_UNSUPPORTED;
	}
    res = tftp_fileop_read(ref,NULL,delta);
    if (res < 0) return res;

    return info->tftp_fileoffset;
}


/*  *********************************************************************
    *  tftp_fileop_close(ref)
    *  
    *  Close the TFTP file.
    *  
    *  Input parameters: 
    *  	   ref - our TFTP info
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void tftp_fileop_close(void *ref)
{
    tftp_info_t *info = (tftp_info_t *) ref;

    _tftp_close(info);

    KFREE(info->tftp_filename);
    KFREE(info);
}


/*  *********************************************************************
    *  tftp_fileop_uninit(fsctx)
    *  
    *  Uninitialize the filesystem context, freeing allocated
    *  resources.
    *  
    *  Input parameters: 
    *  	   fsctx - our context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void tftp_fileop_uninit(void *fsctx)
{
    KFREE(fsctx);
}

#ifdef RESCUE_MODE
extern int send_rescueack(unsigned short no, unsigned short lo)
{
        ebuf_t *buf = NULL;
        int acksocket;
        char tftpnull;
        int res, i;

        /*
         * Open a UDP socket to the TFTP server
         */
        acksocket = udp_socket(UDP_PROTO_TFTP);
        res = udp_bind(acksocket, 69);
        if (res < 0) {
                return res;
        }
        udp_connect(acksocket, ackport);
        for (i = 0; i < 1; i++) {
                buf = udp_alloc();
                if (!buf)
                        return -1;
                /*
                 * Send the data
                 */
                ebuf_append_u16_be(buf, no);
                ebuf_append_u16_be(buf, lo);
                ebuf_append_bytes(buf,&tftpnull, 0);
                udp_send(acksocket ,buf, tftpipto);
        }
        if (buf)
                udp_free(buf);
        udp_close(acksocket);
        return 0;
}
#endif

