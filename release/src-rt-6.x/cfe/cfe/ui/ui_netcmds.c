/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Network commands				File: ui_netcmds.c
    *  
    *  Network user interface
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

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"
#include "cfe_ioctl.h"

#include "cfe_error.h"

#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"
#include "bsp_config.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_api.h"

#include "cfe_fileops.h"

#define ip_addriszero(a) (((a)[0]|(a)[1]|(a)[2]|(a)[3]) == 0)
#define isdigit(d) (((d) >= '0') && ((d) <= '9'))

int ui_init_netcmds(void);

#if CFG_NETWORK
static int ui_cmd_ifconfig(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_arp(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_ping(ui_cmdline_t *cmd,int argc,char *argv[]);
#if CFG_TCP
extern int ui_init_tcpcmds(void);
#endif
#endif

typedef struct netparam_s {
    const char *str;
    int num;
} netparam_t;

const static netparam_t loopbacktypes[] = {
    {"off",ETHER_LOOPBACK_OFF},
    {"internal",ETHER_LOOPBACK_INT},
    {"external",ETHER_LOOPBACK_EXT},
    {0,NULL}};

const static netparam_t speedtypes[] = {
    {"auto",ETHER_SPEED_AUTO},
    {"10hdx",ETHER_SPEED_10HDX},
    {"10fdx",ETHER_SPEED_10FDX},
    {"100hdx",ETHER_SPEED_100HDX},
    {"100fdx",ETHER_SPEED_100FDX},
    {"1000hdx",ETHER_SPEED_1000HDX},
    {"1000fdx",ETHER_SPEED_1000FDX},
    {0,NULL}};


int ui_init_netcmds(void)
{
#if CFG_NETWORK
    cmd_addcmd("ifconfig",
	       ui_cmd_ifconfig,
	       NULL,
	       "Configure the Ethernet interface",
	       "ifconfig device [options..]\n\n"
	       "Activates and configures the specified Ethernet interface and sets its\n"
	       "IP address, netmask, and other parameters.\n"
#if CFG_DHCP
               "The -auto switch can be used to set this information via DHCP.\n"
               ,
               "-auto;Configure interface automatically via DHCP|"
#else
               ,
#endif	/* CFG_DHCP */
               "-off;Deactivate the specified interface|"
	       "-addr=*;Specifies the IP address of the interface|"
	       "-mask=*;Specifies the subnet mask for the interface|"
               "-gw=*;Specifies the gateway address for the interface|"
               "-dns=*;Specifies the name server address for the interface|"
               "-domain=*;Specifies the default domain for name service queries|"
	       "-speed=*;Sets the interface speed (auto,10fdx,10hdx,\n100fdx,\n"
	       "100hdx,1000fdx,1000hdx)|"
	       "-loopback=*;Sets the loopback mode (off,internal,external)  "
	       "External\nloopback causes the phy to be placed in loopback mode|"
               "-hwaddr=*;Sets the hardware address (overrides environment)");

    cmd_addcmd("arp",
	       ui_cmd_arp,
	       NULL,
	       "Display or modify the ARP Table",
	       "arp [-d] [ip-address] [dest-address]\n\n"
	       "Without any parameters, the arp command will display the contents of the\n"
               "arp table.  With two parameters, arp can be used to add permanent arp\n"
               "entries to the table (permanent arp entries do not time out)",
	       "-d;Delete the specified ARP entry.  If specified, ip-address\n"
               "may be * to delete all entries.");

    cmd_addcmd("ping",
	       ui_cmd_ping,
	       NULL,
	       "Ping a remote IP host.",
	       "ping [-t] remote-host\n\n"
	       "This command sends an ICMP ECHO message to a remote host and waits for \n"
	       "a reply.  The network interface must be configured and operational for\n"
	       "this command to work.  If the interface is configured for loopback mode\n"
	       "the packet will be sent through the network interface, so this command\n"
	       "can be used for a simple network test.",
	       "-t;Ping forever, or until the ENTER key is struck|"
	       "-x;Exit immediately on first error (use with -f or -t)|"
	       "-f;Flood ping (use carefully!) - ping as fast as possible|"
	       "-s=*;Specify the number of ICMP data bytes|"
	       "-c=*;Specify number of packets to echo|"
	       "-A;don't abort even if key is pressed|"
	       "-E;Require all packets sent to be returned, for successful return status");

#if CFG_TCP
    ui_init_tcpcmds();
#endif
	       

#endif

    return 0;
}


#if CFG_NETWORK
static int parsexdigit(char str)
{
    int digit;

    if ((str >= '0') && (str <= '9')) digit = str - '0';
    else if ((str >= 'a') && (str <= 'f')) digit = str - 'a' + 10;
    else if ((str >= 'A') && (str <= 'F')) digit = str - 'A' + 10;
    else return -1;

    return digit;
}


static int parsehwaddr(char *str,uint8_t *hwaddr)
{
    int digit1,digit2;
    int idx = 6;

    while (*str && (idx > 0)) {
	digit1 = parsexdigit(*str);
	if (digit1 < 0) return -1;
	str++;
	if (!*str) return -1;

	if ((*str == ':') || (*str == '-')) {
	    digit2 = digit1;
	    digit1 = 0;
	    }
	else {
	    digit2 = parsexdigit(*str);
	    if (digit2 < 0) return -1;
	    str++;
	    }

	*hwaddr++ = (digit1 << 4) | digit2;
	idx--;

	if (*str == '-') str++;
	if (*str == ':') str++;
	}
    return 0;
}



static int ui_ifdown(void)
{
    char *devname;

    devname = (char *) net_getparam(NET_DEVNAME);
    if (devname) {
	xprintf("Device %s has been deactivated.\n",devname);
	net_uninit();
	net_setnetvars();
	}

    return 0;
}

static void ui_showifconfig(void)
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
    if (addr) xprintf(" hwaddr %a",addr);

    addr = net_getparam(NET_IPADDR);
    if (addr) {
	if (ip_addriszero(addr)) xprintf(", ipaddr not set");
	else xprintf(", ipaddr %I",addr);
	}

    addr = net_getparam(NET_NETMASK);
    if (addr) {
	if (ip_addriszero(addr)) xprintf(", mask not set");
	else xprintf(", mask %I",addr);
	}

    xprintf("\n");
    xprintf("        ");

    addr = net_getparam(NET_GATEWAY);
    if (addr) {
	if (ip_addriszero(addr)) xprintf("gateway not set");
	else xprintf("gateway %I",addr);
	}

    addr = net_getparam(NET_NAMESERVER);
    if (addr) {
	if (ip_addriszero(addr)) xprintf(", nameserver not set");
	else xprintf(", nameserver %I",addr);
	}

    addr = net_getparam(NET_DOMAIN);
    if (addr) {
	xprintf(", domain %s",addr);	
	}

    xprintf("\n");
}

#if CFG_DHCP
static int ui_ifconfig_auto(ui_cmdline_t *cmd,char *devname)
{
    int err;	
    dhcpreply_t *reply = NULL;
    char *x;
    uint8_t hwaddr[6];

    net_uninit();

    err = net_init(devname);
    if (err < 0) {
	xprintf("Could not activate device %s: %s\n",
		devname,cfe_errortext(err));
	return err;
	}

    if (cmd_sw_value(cmd,"-hwaddr",&x)) {
	if (parsehwaddr(x,hwaddr) != 0) {
	    xprintf("Invalid hardware address: %s\n",x);
	    net_uninit();
	    return CFE_ERR_INV_PARAM;
	    }
	else {
	    net_setparam(NET_HWADDR,hwaddr);
	    }
	}

    err = dhcp_bootrequest(&reply);

    if (err < 0) {
	xprintf("DHCP registration failed on device %s\n",devname);
	net_uninit();
	return CFE_ERR_NETDOWN;
	}

    net_setparam(NET_IPADDR,reply->dr_ipaddr);
    net_setparam(NET_NETMASK,reply->dr_netmask);
    net_setparam(NET_GATEWAY,reply->dr_gateway);
    net_setparam(NET_NAMESERVER,reply->dr_nameserver);
    net_setparam(NET_DOMAIN,(uint8_t *)reply->dr_domainname);

    dhcp_set_envvars(reply);

    if (reply) dhcp_free_reply(reply);

    ui_showifconfig();
    net_setnetvars();
    return 0;
}
#endif	/* CFG_DHCP */

static int ui_ifconfig_getsw(ui_cmdline_t *cmd,char *swname,char *descr,uint8_t *addr)
{
    char *x;

    x = NULL;

    if (cmd_sw_value(cmd,swname,&x) == 0) return 0;

    if ((x == NULL) || (parseipaddr(x,addr) < 0)) {
	xprintf("Invalid %s: %s\n",descr,x ? x : "(none)");
	return -1;
	}

    return 1;
}

static int ui_ifconfig_lookup(char *name,char *val,const netparam_t *list) 
{
    const netparam_t *p = list;
    
    while (p->str) {
	if (strcmp(p->str,val) == 0) return p->num;
	p++;
	}

    xprintf("Invalid parameter for %s: Valid options are: ");
    
    p = list;
    while (p->str) {
	xprintf("%s ",p->str);
	p++;
	}

    xprintf("\n");
    return -1;
}


#define FLG_IPADDR 1
#define FLG_NETMASK 2
#define FLG_GATEWAY 4
#define FLG_NAMESERVER 8
#define FLG_DOMAIN 16
#define FLG_LOOPBACK 32
#define FLG_SPEED 64
#define FLG_HWADDR 128

static int ui_cmd_ifconfig(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *devname;
    int flags = 0;
    uint8_t ipaddr[IP_ADDR_LEN];
    uint8_t netmask[IP_ADDR_LEN];
    uint8_t gateway[IP_ADDR_LEN];
    uint8_t nameserver[IP_ADDR_LEN];
    uint8_t hwaddr[6];
    int speed = ETHER_SPEED_AUTO;
    int loopback = ETHER_LOOPBACK_OFF;
    char *domain = NULL;
    int res;
    char *x;

    if (argc < 1) {
	ui_showifconfig();
	return 0;
	}

    devname = cmd_getarg(cmd,0);

    if (cmd_sw_isset(cmd,"-off")) {
	return ui_ifdown();
	}
	
#if CFG_DHCP
    if (cmd_sw_isset(cmd,"-auto")) {
	return ui_ifconfig_auto(cmd,devname);
	}
#endif	/* CFG_DHCP */

    res = ui_ifconfig_getsw(cmd,"-addr","interface IP address",ipaddr);
    if (res < 0) return CFE_ERR_INV_PARAM;
    if (res > 0) {
	flags |= FLG_IPADDR;
	}

    res = ui_ifconfig_getsw(cmd,"-mask","netmask",netmask);
    if (res < 0) return CFE_ERR_INV_PARAM;
    if (res > 0) {
	flags |= FLG_NETMASK;
	}

    res = ui_ifconfig_getsw(cmd,"-gw","gateway IP address",gateway);
    if (res < 0) return CFE_ERR_INV_PARAM;
    if (res > 0) {
	flags |= FLG_GATEWAY;
	}

    res = ui_ifconfig_getsw(cmd,"-dns","name server IP address",nameserver);
    if (res < 0) return CFE_ERR_INV_PARAM;
    if (res > 0) {
	flags |= FLG_NAMESERVER;
	}

    if (cmd_sw_value(cmd,"-domain",&domain)) {
	if (domain) flags |= FLG_DOMAIN;
	}

    if (cmd_sw_value(cmd,"-speed",&x)) {
	speed = ui_ifconfig_lookup("-speed",x,speedtypes);
	if (speed >= 0) flags |= FLG_SPEED;
	else return CFE_ERR_INV_PARAM;
	}

    if (cmd_sw_value(cmd,"-loopback",&x)) {
	loopback = ui_ifconfig_lookup("-loopback",x,loopbacktypes);
	if (loopback >= 0) flags |= FLG_LOOPBACK;
	else return CFE_ERR_INV_PARAM;
	}

    if (cmd_sw_value(cmd,"-hwaddr",&x)) {
	if (parsehwaddr(x,hwaddr) != 0) {
	    xprintf("Invalid hardware address: %s\n",x);
	    return CFE_ERR_INV_PARAM;
	    }
	else {
	    flags |= FLG_HWADDR;
	    }
	}

    /*
     * If the network is running and the device name is
     * different, uninit the net first.
     */

    x = (char *) net_getparam(NET_DEVNAME);

    if ((x != NULL) && (strcmp(x,devname) != 0)) {
	net_uninit();
	}

    /*
     * Okay, initialize the network if it is not already on.  If it
     * is OFF, the "net_devname" parameter will be NULL.
     */

    if (x == NULL) {
	res = net_init(devname);		/* turn interface on */
	if (res < 0) {
	    ui_showerror(res,"Could not activate network interface '%s'",devname);
	    return res;
	    }
	}

    /*
     * Set the parameters
     */

    if (flags & FLG_HWADDR)     net_setparam(NET_HWADDR,hwaddr);
    if (flags & FLG_IPADDR)     net_setparam(NET_IPADDR,ipaddr);
    if (flags & FLG_NETMASK)    net_setparam(NET_NETMASK,netmask);
    if (flags & FLG_GATEWAY)    net_setparam(NET_GATEWAY,gateway);
    if (flags & FLG_NAMESERVER) net_setparam(NET_NAMESERVER,nameserver);
    if (flags & FLG_DOMAIN)     net_setparam(NET_DOMAIN,(uint8_t *)domain);
    if (flags & FLG_SPEED)      net_setparam(NET_SPEED,(uint8_t *) &speed);
    if (flags & FLG_LOOPBACK)   net_setparam(NET_LOOPBACK,(uint8_t *) &loopback);

    ui_showifconfig();
    net_setnetvars();

    return 0;
}


static int ui_cmd_arp(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int idx;
    uint8_t ipaddr[IP_ADDR_LEN];
    uint8_t hwaddr[ENET_ADDR_LEN];
    char *x;
    int once = 0;

    if (cmd_sw_isset(cmd,"-d")) {
	if ((x = cmd_getarg(cmd,0)) == NULL) {
	    return ui_showusage(cmd);
	    }

	if (strcmp(x,"*") == 0) {
	    while (arp_enumerate(0,ipaddr,hwaddr) >= 0) {
		arp_delete(ipaddr);
		}
	    }
	else {
	    if (parseipaddr(x,ipaddr) < 0) {
		xprintf("Invalid IP address: %s\n",x);
		return CFE_ERR_INV_PARAM;
		}
	    arp_delete(ipaddr);
	    }
	return 0;
	}

    /*
     * Get the IP address.  If NULL, display the table.
     */

    x = cmd_getarg(cmd,0);
    if (x == NULL) {
	idx = 0;
	while (arp_enumerate(idx,ipaddr,hwaddr) >= 0) {
	    if (once == 0) {
		xprintf("Hardware Address   IP Address\n");
		xprintf("-----------------  ---------------\n");
		once = 1;
		}
	    xprintf("%a  %I\n",hwaddr,ipaddr);
	    idx++;
	    }
	if (idx == 0) xprintf("No ARP entries.\n");
	return 0;
	}

    if (parseipaddr(x,ipaddr) < 0) {
	xprintf("Invalid IP address: %s\n",x);
	return CFE_ERR_INV_PARAM;
	}

    /* 
     * Get the hardware address.
     */

    x = cmd_getarg(cmd,1);
    if (x == NULL) {
	return ui_showusage(cmd);
	}

    if (parsehwaddr(x,hwaddr) < 0) {
	xprintf("Invalid hardware address: %s\n",x);
	return CFE_ERR_INV_PARAM;
	}

    arp_add(ipaddr,hwaddr);

    return 0;
}

#define IP_HDR_LENGTH    20
#define ICMP_HDR_LENGTH  8
#define PING_HDR_LENGTH  (IP_HDR_LENGTH+ICMP_HDR_LENGTH)
#define MAX_PKT_LENGTH   1500

static int ui_cmd_ping(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *host;
    uint8_t hostaddr[IP_ADDR_LEN];
    int res;
    int seq = 0;
    int forever = 0;
    int count = 1;
    int ttlcount = 1;
    int countreturned = 0;
    int size = 56;
    int flood = 0;
    int retval = 0;
    int exitonerror = 0;
    int needexact = 0;
    int noabort = 0;
    char  *x;

    host = cmd_getarg(cmd,0);
    if (!host) return -1;
	
    if (cmd_sw_isset(cmd,"-t")) {
	forever = 1;
	}

    /* Per traditional Unix usage, the size argument to ping is
       the number of ICMP data bytes.  The frame on the wire will also
       include the ethernet, IP and ICMP headers (14, 20, and
       8 bytes respectively) and ethernet trailer (CRC, 4 bytes). */
    if (cmd_sw_value(cmd,"-s",&x)) {
	size = atoi(x);
	if (size < 0)
	    size = 0;
	if (size > MAX_PKT_LENGTH - PING_HDR_LENGTH)
	    size = MAX_PKT_LENGTH - PING_HDR_LENGTH;
	}

    if (cmd_sw_isset(cmd,"-f")) {
	flood = 1;
	forever = 1;
	}

    if (cmd_sw_isset(cmd,"-x")) {
	exitonerror = 1;
	}

    if (cmd_sw_value(cmd,"-c",&x)) {
	count = atoi(x);
	ttlcount = count;
	forever = 0;
	}

    if (cmd_sw_isset(cmd,"-A")) {
	noabort = 1;
	}

    if (cmd_sw_isset(cmd,"-E")) {
	needexact = 1;
	}

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

    if (forever) xprintf("Press ENTER to stop pinging\n");

    do {
	res = icmp_ping(hostaddr,seq,size);

	if (res < 0) {
	    xprintf("Could not transmit echo request\n");
	    retval = CFE_ERR_IOERR;
	    break;
	    }
	else if (res == 0) {
	    xprintf("%s (%I) is not responding (seq=%d)\n",host,hostaddr,seq);
	    retval = CFE_ERR_TIMEOUT;
	    if (exitonerror) break;
	    }
	else {
	    countreturned++;
	    if (!flood || ((seq % 10000) == 0)) {
		if (forever || (ttlcount > 1)) {
		    xprintf("%s (%I) is alive (seq=%d)\n",host,hostaddr,seq);
		    }
		else xprintf("%s (%I) is alive\n",host,hostaddr);
		}
	    }

	if ((forever || (count > 1))  && !flood) {
	    if (res > 0) cfe_sleep(CFE_HZ);
	    }

	seq++;
	count--;

	} while ((forever || (count > 0)) && (noabort || !console_status()));

    xprintf("%s (%I): %d packets sent, %d received\n",host,hostaddr,
	    ttlcount-count,countreturned);
    return (needexact ? (countreturned != ttlcount) : (countreturned == 0));
}

#endif
