/* pppoe.c - pppd plugin to implement PPPoE protocol.
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 * Borrows heavily from the PPPoATM plugin by Mitchell Blank Jr.,
 * which is based in part on work from Jens Axboe and Paul Mackerras.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "pppoe.h"
#if _linux_
extern int new_style_driver;    /* From sys-linux.c */
#include <linux/ppp_defs.h>
#include <linux/if_pppox.h>
#include <linux/if_ppp.h>
#else
#error this module meant for use with linux only at this time
#endif


#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "ccp.h"
#include "pathnames.h"

const char pppd_version[] = VERSION;

#define _PATH_ETHOPT         _ROOT_PATH "/etc/ppp/options."

#define PPPOE_MTU	1492
extern int kill_link;
static char *bad_options[] = {
    "noaccomp",
    "-ac",
    "default-asyncmap",
    "-am",
    "asyncmap",
    "-as",
    "escape",
    "multilink",
    "receive-all",
    "crtscts",
    "-crtscts",
    "nocrtscts",
    "cdtrcts",
    "nocdtrcts",
    "xonxoff",
    "modem",
    "local",
    "sync",
    "deflate",
    "nodeflate",
    "vj",
    "novj",
    "nobsdcomp",
    "bsdcomp",
    "-bsdcomp",
    NULL
};

bool	pppoe_server=0;
char	*pppoe_srv_name=NULL;
char	*pppoe_ac_name=NULL;
char    *hostuniq = NULL;
int     retries = 0;

int setdevname_pppoe(const char *cp);

static option_t pppoe_options[] = {
	{ "device name", o_wild, (void *) &setdevname_pppoe,
	  "Serial port device name",
	  OPT_DEVNAM | OPT_PRIVFIX | OPT_NOARG  | OPT_A2STRVAL | OPT_STATIC,
	  devnam},
	{ "pppoe_srv_name", o_string, &pppoe_srv_name,
	  "PPPoE service name"},
	{ "pppoe_ac_name", o_string, &pppoe_ac_name,
	  "PPPoE access concentrator name"},
	{ "pppoe_hostuniq", o_string, &hostuniq,
	  "PPPoE client uniq hostid "},
	{ "pppoe_retransmit", o_int, &retries,
	  "PPPoE client number of retransmit tries"},
	{ "pppoe_server", o_bool, &pppoe_server,
	  "PPPoE listen for incoming requests",1},
	{ NULL }
};



struct session *ses = NULL;
static int connect_pppoe_ses(void)
{
    int i,err=-1;
    if( pppoe_server == 1 ){
	srv_init_ses(ses,devnam);
    }else{
	client_init_ses(ses,devnam);
    }
    strlcpy(ppp_devnam, devnam, sizeof(ppp_devnam));

    err= session_connect ( ses );

    if(err < 0){
	poe_fatal(ses,"Failed to negotiate PPPoE connection: %d %m",errno,errno);
    }


    poe_info(ses,"Connecting PPPoE socket: %E %04x %s %p",
	     ses->sp.sa_addr.pppoe.remote,
	     ses->sp.sa_addr.pppoe.sid,
	     ses->sp.sa_addr.pppoe.dev,ses);

    err = connect(ses->fd, (struct sockaddr*)&ses->sp,
		  sizeof(struct sockaddr_pppox));


    if( err < 0 ){
	poe_fatal(ses,"Failed to connect PPPoE socket: %d %m",errno,errno);
	return err;
    }
    /* Once the logging is fixed, print a message here indicating
       connection parameters */

    return ses->fd;
}

static void disconnect_pppoe_ses(void)
{
    int ret;
    warn("Doing disconnect");
    session_disconnect(ses);
    ses->sp.sa_addr.pppoe.sid = 0;
    ret = connect(ses->fd, (struct sockaddr*)&ses->sp,
	    sizeof(struct sockaddr_pppox));

}


static int setspeed_pppoe(const char *cp)
{
    return 0;
}

static void init_device_pppoe(void)
{
    struct filter *filt;
    unsigned int size=0;
    ses=(void *)malloc(sizeof(struct session));
    if(!ses){
	fatal("No memory for new PPPoE session");
    }
    memset(ses,0,sizeof(struct session));

    if ((ses->filt=malloc(sizeof(struct filter))) == NULL) {
	poe_error (ses,"failed to malloc for Filter ");
	poe_die (-1);
    }

    filt=ses->filt;  /* makes the code more readable */
    memset(filt,0,sizeof(struct filter));

    if (pppoe_ac_name !=NULL) {
	if (strlen (pppoe_ac_name) > 255) {
	    poe_error (ses," AC name too long (maximum allowed 256 chars)");
	    poe_die(-1);
	}
	ses->filt->ntag = make_filter_tag(PTT_AC_NAME,
					  strlen(pppoe_ac_name),
					  pppoe_ac_name);

	if ( ses->filt->ntag== NULL) {
	    poe_error (ses,"failed to malloc for AC name");
	    poe_die(-1);
	}

    }


    if (pppoe_srv_name !=NULL) {
	if (strlen (pppoe_srv_name) > 255) {
	    poe_error (ses," Service name too long
	                (maximum allowed 256 chars)");
	    poe_die(-1);
	}
	ses->filt->stag = make_filter_tag(PTT_SRV_NAME,
					  strlen(pppoe_srv_name),
					  pppoe_srv_name);
	if ( ses->filt->stag == NULL) {
	    poe_error (ses,"failed to malloc for service name");
	    poe_die(-1);
	}
    }

    if (hostuniq) {
	ses->filt->htag = make_filter_tag(PTT_HOST_UNIQ,
					  strlen(hostuniq),
					  hostuniq);
	if ( ses->filt->htag == NULL) {
	    poe_error (ses,"failed to malloc for Uniq Host Id ");
	    poe_die(-1);
	}
    }

    if (retries) {
	ses->retries=retries;
    }

    memcpy( ses->name, devnam, IFNAMSIZ);
    ses->opt_debug=1;
}

static void pppoe_extra_options()
{
    int ret;
    char buf[256];
    snprintf(buf, 256, _PATH_ETHOPT "%s",devnam);
    if(!options_from_file(buf, 0, 0, 1))
	exit(EXIT_OPTION_ERROR);

}



static void send_config_pppoe(int mtu,
			      u_int32_t asyncmap,
			      int pcomp,
			      int accomp)
{
    int sock;
    struct ifreq ifr;

    if (mtu > PPPOE_MTU)
	warn("Couldn't increase MTU to %d", mtu);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
	fatal("Couldn't create IP socket: %m");
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(sock, SIOCSIFMTU, (caddr_t) &ifr) < 0)
	fatal("ioctl(SIOCSIFMTU): %m");
    (void) close (sock);
}


static void recv_config_pppoe(int mru,
			      u_int32_t asyncmap,
			      int pcomp,
			      int accomp)
{
    if (mru > PPPOE_MTU)
	error("Couldn't increase MRU to %d", mru);
}

static void set_xaccm_pppoe(int unit, ext_accm accm)
{
    /* NOTHING */
}



struct channel pppoe_channel;
/* Check is cp is a valid ethernet device
 * return either 1 if "cp" is a reasonable thing to name a device
 * or die.
 * Note that we don't actually open the device at this point
 * We do need to fill in:
 *   devnam: a string representation of the device
 */

int (*old_setdevname_hook)(const char* cp) = NULL;
int setdevname_pppoe(const char *cp)
{
    int ret;
    char dev[IFNAMSIZ+1];
    int addr[ETH_ALEN];
    int sid;

    char **a;
    ret =sscanf(cp, FMTSTRING(IFNAMSIZ),addr, addr+1, addr+2,
		addr+3, addr+4, addr+5,&sid,dev);
    if( ret != 8 ){

	ret = get_sockaddr_ll(cp,NULL);
        if (ret < 0)
	    fatal("PPPoE: Cannot create PF_PACKET socket for PPPoE discovery\n");
	if (ret == 1)
	    strncpy(devnam, cp, sizeof(devnam));
    }else{
	/* long form parsed */
	ret = get_sockaddr_ll(dev,NULL);
        if (ret < 0)
	    fatal("PPPoE: Cannot create PF_PACKET socket for PPPoE discovery\n");

	strncpy(devnam, cp, sizeof(devnam));
	ret = 1;
    }


    if( ret == 1 && the_channel != &pppoe_channel ){

	the_channel = &pppoe_channel;

	{
	    char **a;
	    for (a = bad_options; *a != NULL; a++)
		remove_option(*a);
	}
	modem = 0;

	lcp_allowoptions[0].neg_accompression = 0;
	lcp_wantoptions[0].neg_accompression = 0;

	lcp_allowoptions[0].neg_asyncmap = 0;
	lcp_wantoptions[0].neg_asyncmap = 0;

	lcp_allowoptions[0].neg_pcompression = 0;
	lcp_wantoptions[0].neg_pcompression = 0;

	ccp_allowoptions[0].deflate = 0 ;
	ccp_wantoptions[0].deflate = 0 ;

	ipcp_allowoptions[0].neg_vj=0;
	ipcp_wantoptions[0].neg_vj=0;

	ccp_allowoptions[0].bsd_compress = 0;
	ccp_wantoptions[0].bsd_compress = 0;

	init_device_pppoe();
    }
    return ret;
}



void plugin_init(void)
{
/*
  fatal("PPPoE plugin loading...");
*/

#if _linux_
    if (!ppp_available() && !new_style_driver)
	fatal("Kernel doesn't support ppp_generic needed for PPPoE");
#else
    fatal("No PPPoE support on this OS");
#endif
    add_options(pppoe_options);


    info("PPPoE Plugin Initialized");
}

struct channel pppoe_channel = {
    options: pppoe_options,
    process_extra_options: &pppoe_extra_options,
    check_options: NULL,
    connect: &connect_pppoe_ses,
    disconnect: &disconnect_pppoe_ses,
    establish_ppp: &generic_establish_ppp,
    disestablish_ppp: &generic_disestablish_ppp,
    send_config: &send_config_pppoe,
    recv_config: &recv_config_pppoe,
    close: NULL,
    cleanup: NULL
};

