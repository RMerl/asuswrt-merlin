/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the Stanford.txt file.
**
*/
/**
*   igmpproxy.c - The main file for the IGMP proxy application.
*
*   February 2005 - Johnny Egeland
*/

#include "igmpproxy.h"

static const char Usage[] = 
"Usage: igmpproxy [-h] [-d] [-v [-v]] <configfile>\n"
"\n" 
"   -h   Display this help screen\n"
"   -d   Run in debug mode. Output all messages on stderr\n"
"   -v   Be verbose. Give twice to see even debug messages.\n"
"\n"
PACKAGE_STRING "\n"
;

// Local function Prototypes
static void signalHandler(int);
int     igmpProxyInit();
void    igmpProxyCleanUp();
void    igmpProxyRun();

// Global vars...
static int sighandled = 0;
#define	GOT_SIGINT	0x01
#define	GOT_SIGHUP	0x02
#define	GOT_SIGUSR1	0x04
#define	GOT_SIGUSR2	0x08

// The upstream VIF index
int         upStreamVif;   

/**
*   Program main method. Is invoked when the program is started
*   on commandline. The number of commandline arguments, and a
*   pointer to the arguments are recieved on the line...
*/    
int main( int ArgCn, char *ArgVc[] ) {

    int c;

    // Parse the commandline options and setup basic settings..
    while ((c = getopt(ArgCn, ArgVc, "vdh")) != -1) {
        switch (c) {
        case 'd':
            Log2Stderr = true;
            break;
        case 'v':
            LogLevel++;
            break;
        case 'h':
            fputs(Usage, stderr);
            exit(0);
            break;
        default:
            exit(1);
            break;
        }
    }

    if (optind != ArgCn - 1) {
	fputs("You must specify the configuration file.\n", stderr);
	exit(1);
    }
    char *configFilePath = ArgVc[optind];

    // Chech that we are root
    if (geteuid() != 0) {
       fprintf(stderr, "igmpproxy: must be root\n");
       exit(1);
    }

    openlog("igmpproxy", LOG_PID, LOG_USER);

    // Write debug notice with file path...
    my_log(LOG_DEBUG, 0, "Searching for config file at '%s'" , configFilePath);

    do {

        // Loads the config file...
        if( ! loadConfig( configFilePath ) ) {
            my_log(LOG_ERR, 0, "Unable to load config file...");
            break;
        }
    
        // Initializes the deamon.
        if ( !igmpProxyInit() ) {
            my_log(LOG_ERR, 0, "Unable to initialize IGMPproxy.");
            break;
        }

	if ( !Log2Stderr ) {

	    // Only daemon goes past this line...
	    if (fork()) exit(0);

	    // Detach daemon from terminal
	    if ( close( 0 ) < 0 || close( 1 ) < 0 || close( 2 ) < 0
		 || open( "/dev/null", 0 ) != 0 || dup2( 0, 1 ) < 0 || dup2( 0, 2 ) < 0
		 || setpgrp() < 0
	       ) {
		my_log( LOG_ERR, errno, "failed to detach daemon" );
	    }
	}

        // Go to the main loop.
        igmpProxyRun();
    
        // Clean up
        igmpProxyCleanUp();

    } while ( false );

    // Inform that we are exiting.
    my_log(LOG_INFO, 0, "Shutdown complete....");

    exit(0);
}



/**
*   Handles the initial startup of the daemon.
*/
int igmpProxyInit() {
    struct sigaction sa;
    int Err;


    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;    /* Interrupt system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Loads configuration for Physical interfaces...
    buildIfVc();    
    
    // Configures IF states and settings
    configureVifs();

    switch ( Err = enableMRouter() ) {
    case 0: break;
    case EADDRINUSE: my_log( LOG_ERR, EADDRINUSE, "MC-Router API already in use" ); break;
    default: my_log( LOG_ERR, Err, "MRT_INIT failed" );
    }

    /* create VIFs for all IP, non-loop interfaces
     */
    {
        unsigned Ix;
        struct IfDesc *Dp;
        int     vifcount = 0;
        upStreamVif = -1;

        for ( Ix = 0; (Dp = getIfByIx(Ix)); Ix++ ) {

            if ( Dp->InAdr.s_addr && ! (Dp->Flags & IFF_LOOPBACK) && Dp->state != IF_STATE_DISABLED ) {
                if(Dp->state == IF_STATE_UPSTREAM) {
                    if(upStreamVif == -1) {
                        upStreamVif = Ix;
                    } else {
                        my_log(LOG_ERR, 0, "Vif #%d was already upstream. Cannot set VIF #%d as upstream as well.",
                            upStreamVif, Ix);
                    }
                }

                addVIF( Dp );
                vifcount++;
            }
        }

        // If there is only one VIF, or no defined upstream VIF, we send an error.
        if(vifcount < 2 || upStreamVif < 0) {
            my_log(LOG_ERR, 0, "There must be at least 2 Vif's where one is upstream.");
        }
    }  
    
    // Initialize IGMP
    initIgmp();
    // Initialize Routing table
    initRouteTable();
    // Initialize timer
    callout_init();


    return 1;
}

/**
*   Clean up all on exit...
*/
void igmpProxyCleanUp() {

    my_log( LOG_DEBUG, 0, "clean handler called" );
    
    free_all_callouts();    // No more timeouts.
    clearAllRoutes();       // Remove all routes.
    disableMRouter();       // Disable the multirout API

}

/**
*   Get current time.
*/
int igmpProxyTime(struct timeval *tv)
{
#ifdef HAVE_CLOCK_MONOTONIC
	struct timespec tp;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0)
		return -1;

	tv->tv_sec = tp.tv_sec;
	tv->tv_usec = tp.tv_nsec / 1000;

	return 0;
#else
	return gettimeofday(tv, NULL);
#endif
}

/**
*   Main daemon loop.
*/
void igmpProxyRun() {
    // Get the config.
    //struct Config *config = getCommonConfig();
    // Set some needed values.
    register int recvlen;
    int     MaxFD, Rt, secs;
    fd_set  ReadFDS;
    socklen_t dummy = 0;
    struct  timeval  curtime, lasttime, difftime, tv; 
    // The timeout is a pointer in order to set it to NULL if nessecary.
    struct  timeval  *timeout = &tv;

    // Initialize timer vars
    difftime.tv_usec = 0;
    igmpProxyTime(&curtime);
    lasttime = curtime;

    // First thing we send a membership query in downstream VIF's...
    sendGeneralMembershipQuery();

    // Loop until the end...
    for (;;) {

        // Process signaling...
        if (sighandled) {
            if (sighandled & GOT_SIGINT) {
                sighandled &= ~GOT_SIGINT;
                my_log(LOG_NOTICE, 0, "Got a interupt signal. Exiting.");
                break;
            }
        }

        // Prepare timeout...
        secs = timer_nextTimer();
        if(secs == -1) {
            timeout = NULL;
        } else {
            timeout->tv_usec = 0;
            timeout->tv_sec = secs;
        }

        // Prepare for select.
        MaxFD = MRouterFD;

        FD_ZERO( &ReadFDS );
        FD_SET( MRouterFD, &ReadFDS );

        // wait for input
        Rt = select( MaxFD +1, &ReadFDS, NULL, NULL, timeout );

        // log and ignore failures
        if( Rt < 0 ) {
            my_log( LOG_WARNING, errno, "select() failure" );
            continue;
        }
        else if( Rt > 0 ) {

            // Read IGMP request, and handle it...
            if( FD_ISSET( MRouterFD, &ReadFDS ) ) {
    
                recvlen = recvfrom(MRouterFD, recv_buf, RECV_BUF_SIZE,
                                   0, NULL, &dummy);
                if (recvlen < 0) {
                    if (errno != EINTR) my_log(LOG_ERR, errno, "recvfrom");
                    continue;
                }
                

                acceptIgmp(recvlen);
            }
        }

        // At this point, we can handle timeouts...
        do {
            /*
             * If the select timed out, then there's no other
             * activity to account for and we don't need to
             * call igmpProxyTime.
             */
            if (Rt == 0) {
                curtime.tv_sec = lasttime.tv_sec + secs;
                curtime.tv_usec = lasttime.tv_usec;
                Rt = -1; /* don't do this next time through the loop */
            } else {
                igmpProxyTime(&curtime);
            }
            difftime.tv_sec = curtime.tv_sec - lasttime.tv_sec;
            difftime.tv_usec += curtime.tv_usec - lasttime.tv_usec;
            while (difftime.tv_usec > 1000000) {
                difftime.tv_sec++;
                difftime.tv_usec -= 1000000;
            }
            if (difftime.tv_usec < 0) {
                difftime.tv_sec--;
                difftime.tv_usec += 1000000;
            }
            lasttime = curtime;
            if (secs == 0 || difftime.tv_sec > 0)
                age_callout_queue(difftime.tv_sec);
            secs = -1;
        } while (difftime.tv_sec > 0);

    }

}

/*
 * Signal handler.  Take note of the fact that the signal arrived
 * so that the main loop can take care of it.
 */
static void signalHandler(int sig) {
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        sighandled |= GOT_SIGINT;
        break;
        /* XXX: Not in use.
        case SIGHUP:
            sighandled |= GOT_SIGHUP;
            break;
    
        case SIGUSR1:
            sighandled |= GOT_SIGUSR1;
            break;
    
        case SIGUSR2:
            sighandled |= GOT_SIGUSR2;
            break;
        */
    }
}
