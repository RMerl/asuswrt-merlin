/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  TCP protocol commands			File: ui_tcpcmds.c
    *  
    *  This file contains commands that make use of the TCP protocol
    *  in CFE, assuming it's configured.
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

#include "cfe_timer.h"

#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"

#include "bsp_config.h"

#if CFG_TCP
#include "net_ebuf.h"
#include "net_api.h"
#endif


/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

#if CFG_TCP

int ui_init_tcpcmds(void);

static int ui_cmd_rlogin(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_connect(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_listen(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_tcpconstest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_ttcp(ui_cmdline_t *cmd,int argc,char *argv[]);
#define isdigit(d) (((d) >= '0') && ((d) <= '9'))


/*  *********************************************************************
    *  ui_init_tcpcmds()
    *  
    *  Add TCP-specific commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_tcpcmds(void)
{

    cmd_addcmd("rlogin",
	       ui_cmd_rlogin,
	       NULL,
	       "mini rlogin client.",
	       "rlogin hostname [username]\n\n"
	       "Connects to a remote system using the RLOGIN protocol.  The remote"
	       "system must have appropriate permissions in place (usually via the"
	       "file '.rhosts') for CFE to connect.  To terminate the session, type"
	       "a tilde (~) character followed by a period (.)",
	       "");

    cmd_addcmd("tcp connect",
	       ui_cmd_connect,
	       NULL,
	       "TCP connection test.",
	       "tcp connect hostname [portnum]",
	       "-q;sink output, don't display on terminal|"
	       "-d;Send junk data to discard|"
	       "-nodelay;set nodelay option on socket|"
	       "-srcport=*;Specify the source port");

    cmd_addcmd("tcp listen",
	       ui_cmd_listen,
	       NULL,
	       "port listener.",
	       "tcp listen portnum",
	       "-q;sink output, don't display on terminal|"
	       "-d;Send junk data to discard|"
	       "-nodelay;set nodelay option on socket");


    cmd_addcmd("tcp constest",
	       ui_cmd_tcpconstest,
	       NULL,
	       "tcp console test.",
	       "tcp constest device",
	       "");

    cmd_addcmd("ttcp",
	       ui_cmd_ttcp,
	       NULL,
	       "TCP test command.",
	       "ttcp -t [-options] host\n"
	       "ttcp -r [-options]\n\n",
	       "-t;Source a pattern to the network|"
	       "-r;Sink (discard) data from the network|"
	       "-D;Don't buffer TCP writes (TCP_NODELAY)|"
	       "-n=*;Number of buffers to send (-t only) (default 2048)|"
	       "-l=*;Size of buffer to send/receive (default 2048)|"
	       "-p=*;Port number to use (default 5001)");

    return 0;
}





static unsigned long rand(void)
{
    static unsigned long seed = 1;
    long x, hi, lo, t;

    x = seed;
    hi = x / 127773;
    lo = x % 127773;
    t = 16807 * lo - 2836 * hi;
    if (t <= 0) t += 0x7fffffff;
    seed = t;
    return t;
}


static int ui_cmd_rlogin(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int s;
    uint8_t hostaddr[IP_ADDR_LEN];
    char *host;
    int res;
    int connflag;
    int rxdata;
    int sport;
    uint8_t data[100];
    int tilde;
    char *username;
    uint8_t *p;

    /*
     * Process args
     */

    host = cmd_getarg(cmd,0);
    if (!host) return ui_showusage(cmd);

    username = cmd_getarg(cmd,1);
    if (!username) username = "";

    /*
     * Look up remote host
     */

    if (isdigit(*host)) {
	if (parseipaddr(host,hostaddr) < 0) {
	    xprintf("Invalid IP address: %s\n",host);
	    return -1;
	    }
	}
    else {
	res = dns_lookup(host,hostaddr);
	if (res < 0) {
	    return ui_showerror(res,"Could not resolve IP address of host %s",host);
	    }
	}

    /*
     * Create TCP socket and bind to a port number less than 1023
     * See RFC1282 for more info about this
     */

    s = tcp_socket();

    if (s < 0) {
	return ui_showerror(s,"Could not create TCP socket");
	}

    res = 0;
    tilde = 0;
    for (sport = 1023; sport > 513; sport--) {
	res = tcp_bind(s,sport);
	if (res == 0) break;
	}

    if (sport == 513) {
	ui_showerror(res,"No ports available for RLOGIN");
	return res;
	}

    /*
     * Establish a connection.  Our sockets default to nonblocking
     * so we want to switch to blocking temporarily to
     * let the tcp_connect routine do this by itself.
     */

    tcp_setflags(s,0);
    res = tcp_connect(s,hostaddr,513);
    if (res < 0) {
	ui_showerror(res,"Could not connect to host %I",hostaddr);
	tcp_close(s);
	return res;
	}


    /*
     * Construct the initial RLOGIN sequence to include
     * our user name and terminal type
     */

    p = data;
    *p++ = '\0';
    p += sprintf(p,"%s",username) + 1;
    p += sprintf(p,"%s",username) + 1;
    p += sprintf(p,"vt100/38400") + 1;
    
    tcp_send(s,data,p-&data[0]);

    res = tcp_recv(s,data,1);			/* receive result code */
    if (res <= 0) {
	goto remdisc;
	}

    /*
     * Switch back to nonblocking I/O for the loop
     */

    tcp_setflags(s,TCPFLG_NBIO);

    /*
     * Begin processing loop
     */

    connflag = TRUE;
    for (;;) {

	/*
	 * Test connection status
	 */

	tcp_status(s,&connflag,&rxdata,NULL);
	if (connflag != TCPSTATUS_CONNECTED) {
	    goto remdisc;
	    }

	/*
	 * Process received data
	 */

	if (rxdata != 0) {
	    res = tcp_recv(s,data,sizeof(data));
	    if (res > 0) {
		console_write(data,res);
		}
	    if (res < 0) {
		ui_showerror(res,"TCP read error");
		break;
		}
	    }

	/*
	 * Process transmitted data
	 */

	if (console_status()) {
	    console_read(data,1);
	    if (tilde == 1) {
		if (data[0] == '.') break;
		tcp_send(s,data,1);
		}
	    else {
		if (data[0] == '~') tilde = 1;
		else tcp_send(s,data,1);
		}
	    }

	/*
	 * Give the background a chance
	 */

	POLL();
	}

    printf("Disconnecting...");
    tcp_close(s);
    printf("done.\n");
    return 0;

remdisc:    
    printf("Remote host is no longer connected.\n");
    tcp_close(s);
    return 0;
}



static int ui_cmd_connect(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int s;
    uint8_t hostaddr[IP_ADDR_LEN];
    char *host;
    char *port;
    int res;
    int connflag;
    int rxdata;
    uint8_t data[100];
    int quiet;
    int discard;
    int total = 0;
    int total2 = 0;
    char b = 0;
    cfe_timer_t t;
    char *bigbuf;
    int nodelay;
    char *sport = NULL;

    bigbuf = KMALLOC(4096,0);
    for (res = 0; res < 4096; res++) bigbuf[res] = 'A'+(res%26);

    quiet = cmd_sw_isset(cmd,"-q");
    discard = cmd_sw_isset(cmd,"-d");
    nodelay = cmd_sw_isset(cmd,"-nodelay");
    
    host = cmd_getarg(cmd,0);
    if (!host) return -1;

    port = cmd_getarg(cmd,1);
    if (!port) port = "23";

    if (strcmp(port,"discard") == 0) port = "9";
    else if (strcmp(port,"chargen") == 0) port = "19";
    else if (strcmp(port,"echo") == 0) port = "7";

    if (isdigit(*host)) {
	if (parseipaddr(host,hostaddr) < 0) {
	    xprintf("Invalid IP address: %s\n",host);
	    return -1;
	    }
	}
    else {
	res = dns_lookup(host,hostaddr);
	if (res < 0) {
	    return ui_showerror(res,"Could not resolve IP address of host %s",host);
	    }
	}


    s = tcp_socket();

    if (s < 0) {
	return ui_showerror(s,"Could not create TCP socket");
	}

    if (cmd_sw_value(cmd,"-srcport",&sport)) {
	res = tcp_bind(s,atoi(sport));
	if (res < 0) {
	    ui_showerror(res,"Could not bind to port %s",sport);
	    tcp_close(s);
	    return res;
	    }
	}

    res = tcp_connect(s,hostaddr,atoi(port));
    if (res < 0) {
	ui_showerror(res,"Could not connect to host %I",hostaddr);
	tcp_close(s);
	return res;
	}

    TIMER_SET(t,CFE_HZ*30);
    connflag = 0;
    while (!TIMER_EXPIRED(t)) {
	POLL();
	tcp_status(s,&connflag,NULL,NULL);
	if (connflag == TCPSTATUS_CONNECTING) continue;
	break;
	}

    if (connflag != TCPSTATUS_CONNECTED) {
	printf("Could not connect to remote host\n");
	tcp_close(s);
	return -1;
	}
    else {
	printf("Connected to remote host.\n");
	}
    

    if (nodelay) tcp_setflags(s,TCPFLG_NODELAY);

    connflag = TRUE;
    for (;;) {
	tcp_status(s,&connflag,&rxdata,NULL);
	if (connflag != TCPSTATUS_CONNECTED) {
	    printf("Remote host is no longer connected.\n");
	    break;
	    }
	if (rxdata != 0) {
	    res = tcp_recv(s,data,sizeof(data));
	    if (res > 0) {
		if (quiet) {
		    total += res;
		    if (total > 1000000) {
			total -= 1000000;
			printf(".");
			}
		    }
		else {
		    console_write(data,res);
		    }
		}
	    if (res < 0) {
		ui_showerror(res,"TCP read error");
		}
	    }
	if (console_status()) {
	    console_read(data,1);
	    if (data[0] == 1) break;
	    else if (data[0] == 3) break;
	    else if (data[0] == 4) {
		for (res = 0; res < 100; res++) data[res] = 'A'+(res%26);
		tcp_send(s,data,100);
		}
	    else if (data[0] == 5) tcp_send(s,bigbuf,2048);
	    else if (data[0] == 2) tcp_debug(s,0);
	    else tcp_send(s,data,1);
	    }
	if (discard) {
	    res = rand() % sizeof(data);
	    memset(data,b,res);
	    b++;
	    res = tcp_send(s,data,res);
	    if (res > 0) {
		total2 += res;
		if (total2 > 1000000) {
		    total2 -= 1000000;
		    printf("+");
		    }
		}

	    }

	POLL();
	}

    printf("Disconnecting...");
    tcp_close(s);
    printf("done.\n");
    
    KFREE(bigbuf);

    return 0;
}


static int ui_cmd_listen(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int s;
    char *port;
    int res;
    int connflag;
    int rxdata;
    uint8_t data[100];
    int quiet;
    int discard;
    int total = 0;
    int total2 = 0;
    char b = 0;
    char *bigbuf;
    uint16_t p;
    uint16_t remport;
    uint8_t remaddr[IP_ADDR_LEN];
    int nodelay;

    bigbuf = KMALLOC(4096,0);
    for (res = 0; res < 4096; res++) bigbuf[res] = 'A'+(res%26);

    quiet = cmd_sw_isset(cmd,"-q");
    discard = cmd_sw_isset(cmd,"-d");
    nodelay = cmd_sw_isset(cmd,"-nodelay");
    
    port = cmd_getarg(cmd,0);
    if (!port) port = "1234";
    p = atoi(port);

    s = tcp_socket();

    if (s < 0) {
	return ui_showerror(s,"Could not create TCP socket");
	}

    res = tcp_listen(s,p);
    if (res < 0) {
	ui_showerror(res,"Could not set socket to listen");
	tcp_close(s);
	return res;
	}

    printf("Listening...");
    connflag = FALSE;
    for (;;) {
	if (console_status()) break;
	tcp_status(s,&connflag,NULL,NULL);
	if (connflag == TCPSTATUS_CONNECTED) break;
	POLL();
	}

    if (connflag != TCPSTATUS_CONNECTED) {
	printf("No connection received from remote host\n");
	tcp_close(s);
	return -1;
	}

    tcp_peeraddr(s,remaddr,&remport);
    printf("Connection from port %u on %I\n",remport,remaddr);

    if (nodelay) tcp_setflags(s,TCPFLG_NODELAY);

    connflag = TRUE;
    for (;;) {
	tcp_status(s,&connflag,&rxdata,NULL);
	if (connflag != TCPSTATUS_CONNECTED) {
	    printf("Remote host is no longer connected.\n");
	    break;
	    }
	if (rxdata != 0) {
	    res = tcp_recv(s,data,sizeof(data));
	    if (res > 0) {
		if (quiet) {
		    total += res;
		    if (total > 1000000) {
			total -= 1000000;
			printf(".");
			}
		    }
		else {
		    console_write(data,res);
		    }
		}
	    if (res < 0) {
		ui_showerror(res,"TCP read error");
		}
	    }
	if (console_status()) {
	    console_read(data,1);
	    if (data[0] == 1) break;
	    if (data[0] == 3) break;
	    if (data[0] == 4) {
		for (res = 0; res < 100; res++) data[res] = 'A'+(res%26);
		tcp_send(s,data,100);
		}
	    if (data[0] == 5) tcp_send(s,bigbuf,2048);
	    if (data[0] == 2) tcp_debug(s,0);
	    else tcp_send(s,data,1);
	    }
	if (discard) {
	    res = rand() % sizeof(data);
	    memset(data,b,res);
	    b++;
	    res = tcp_send(s,data,res);
	    if (res > 0) {
		total2 += res;
		if (total2 > 1000000) {
		    total2 -= 1000000;
		    printf("+");
		    }
		}

	    }

	POLL();
	}


    printf("Disconnecting...");
    tcp_close(s);
    printf("done.\n");
    
    KFREE(bigbuf);

    return 0;
}


static int ui_cmd_tcpconstest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;
    int fh;
    int res;
    uint8_t data[100];

    x = cmd_getarg(cmd,0);
    if (!x) return ui_showusage(cmd);

    fh = cfe_open(x);
    if (fh < 0) return ui_showerror(fh,"Could not open device %s",x);
    
    for (;;) {
	if (console_status()) break;
	res = cfe_read(fh,data,sizeof(data));
	if (res < 0) {
	    ui_showerror(res,"could not read data"); 
	    break;
	    }
	console_write(data,res);
	}

    cfe_close(fh);

    return 0;
}

static int ui_cmd_ttcp(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int s;
    uint8_t hostaddr[IP_ADDR_LEN];
    char *host;
    int res;
    int totalbytes = 0;
    int totalbufs = 0;
    cfe_timer_t start_time;
    cfe_timer_t stop_time;
    cfe_timer_t t;
    int connflag;
    char *bigbuf;
    int nodelay;
    int numbuf;
    int buflen;
    int txmode,rxmode;
    uint16_t port;

    char *x;

    if (cmd_sw_value(cmd,"-n",&x)) numbuf = atoi(x);
    else numbuf = 2048;

    if (cmd_sw_value(cmd,"-l",&x)) buflen = atoi(x);
    else buflen = 2048;

    if (cmd_sw_value(cmd,"-p",&x)) port = atoi(x);
    else port = 5001;

    if ((numbuf == 0) || (buflen == 0)) return ui_showusage(cmd);


    bigbuf = KMALLOC(buflen,0);
    for (res = 0; res < buflen; res++) bigbuf[res] = 'A'+(res%26);

    txmode = cmd_sw_isset(cmd,"-t");
    rxmode = cmd_sw_isset(cmd,"-r");

    if (!(txmode ^ rxmode)) {
	return ui_showerror(-1,"You must specify one of -t or -r");
	}

    nodelay = cmd_sw_isset(cmd,"-D");

    if (txmode) {
	host = cmd_getarg(cmd,0);
	if (!host) return ui_showusage(cmd);

	if (isdigit(*host)) {
	    if (parseipaddr(host,hostaddr) < 0) {
		return ui_showerror(-1,"Invalid IP address: %s\n",host);
		}
	    }
	else {
	    res = dns_lookup(host,hostaddr);
	    if (res < 0) {
		return ui_showerror(res,"Could not resolve IP address of host %s",host);
		}
	    }
	}


    s = tcp_socket();

    if (s < 0) {
	return ui_showerror(s,"Could not create TCP socket");
	}


    if (txmode) {
	res = tcp_connect(s,hostaddr,port);
	if (res < 0) {
	    ui_showerror(res,"Could not connect to host %I",hostaddr);
	    tcp_close(s);
	    return res;
	    }

	TIMER_SET(t,CFE_HZ*30);
	connflag = 0;
	while (!TIMER_EXPIRED(t)) {
	    POLL();
	    tcp_status(s,&connflag,NULL,NULL);
	    if (connflag == TCPSTATUS_CONNECTING) continue;
	    break;
	    }

	if (connflag != TCPSTATUS_CONNECTED) {
	    printf("Could not connect to remote host\n");
	    tcp_close(s);
	    return -1;
	    }
	else {
	    printf("Connected to remote host.\n");
	    }
	}

    if (rxmode) {
	printf("Waiting for connection on port %d: ",port);
	tcp_listen(s,port);
	for (;;) {
	    if (console_status()) break;
	    tcp_status(s,&connflag,NULL,NULL);
	    if (connflag == TCPSTATUS_CONNECTED) break;
	    POLL();
	    }
	if (connflag != TCPSTATUS_CONNECTED) {
	    printf("No connection received from remote host\n");
	    tcp_close(s);
	    return -1;
	    }
	printf("done.\n");
	}


    if (nodelay) tcp_setflags(s,TCPFLG_NODELAY);	/* also sets blocking */
    else tcp_setflags(s,0);

    start_time = cfe_ticks;

    if (rxmode) {
	while (1) {
	    POLL();
	    res = tcp_recv(s,bigbuf,buflen);
	    if (res != buflen) break;
	    totalbytes += res;
	    totalbufs++;
	    }
	}
    else {
	while (numbuf > 0) {
	    POLL();
	    res = tcp_send(s,bigbuf,buflen);
	    if (res != buflen) break;
	    numbuf--;
	    totalbytes += res;
	    totalbufs++;
	    }
	}

    stop_time = cfe_ticks;

    tcp_close(s);

    if ((res < 0) && !rxmode) {
	ui_showerror(res,"Network I/O error");
	}
    else {
	printf("%d bytes transferred via %d calls in %lld ticks\n",
	       totalbytes,totalbufs,stop_time-start_time);
	}
    

    KFREE(bigbuf);

    return 0;

}

#endif	 /* CFG_TCP */
