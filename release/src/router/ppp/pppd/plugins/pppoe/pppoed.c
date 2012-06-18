/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include "pppoe.h"

int detached=1;
void
sigproc (int src)
{
    int i;
    fprintf (stderr,"Received signal %d", src);
}

void
sigchild (int src)
{
    pid_t pid;
    int status;
    int i;
    pid = waitpid (-1, &status, WNOHANG);
    
    if (!detached)
	fprintf (stderr,"Child received signal %d PID %d, status %d", src, pid, status);
    if (pid < 1) {
	return;
    }
}

void
print_help ()
{
    
    fprintf (stdout,"\npppoe version %d.%d build %d", VERSION_MAJOR, VERSION_MINOR,
	     VERSION_DATE);
    fprintf (stdout,"\nrecognized options are:");
    fprintf (stdout,"\n -I <interface> : overrides the default interface of eth0");
    fprintf (stdout,"\n -S : starts pppoed in server mode");
    fprintf (stdout,"\n -R <num_retries>: forces pppoed to be restarted num_retries");
    fprintf (stdout,"\n                   should the other end be detected to be dead.");
    fprintf (stdout,"\n                   Needs lcp_echo. Read the INSTALL file instructions");
    fprintf (stdout,"\n -F <filename> : specifies additional ppp options file");
    fprintf (stdout,"\n -C <filename> : ppp options file in /etc/ppp/peers/");
    fprintf (stdout,"\n -d <level> : sets debug level");
    fprintf (stdout,"\n -D : prevents pppoed from detaching itself and running in the background");
    fprintf (stdout,"\n -P <path to pppd> : selects a different pppd. Defaults to " _PATH_PPPD);
    fprintf (stdout,"\n -A <AC name> to select a specific AC by name");
    fprintf (stdout,"\n -E <AC service name> to select a specific AC service by name");
    fprintf (stdout,"\n -G Do service discovery only");
    fprintf (stdout,"\n -H Do service discovery and connection (no pppd)\n");
}


int 
get_args (int argc, char **argv,struct session *sess)
{
    struct filter *filt;
    struct host_tag *tg;
    int opt;
    

    sess->opt_debug = 0;
    DEB_DISC=0;
    DEB_DISC2=0;
    sess->log_to_fd = 1;
    sess->np = 0;
    sess->opt_daemonize = 0;
    
    sess->log_to_fd = fileno (stdout);
    
/* defaults to eth0 */
    strcpy (sess->name, "eth0");
    
    
    if ((sess->filt=malloc(sizeof(struct filter))) == NULL) {
        poe_error (sess,"failed to malloc for Filter ");
        poe_die (-1);
    }
    
    filt=sess->filt;  /* makes the code more readable */
    memset(filt,0,sizeof(struct filter));
    
    filt->num_restart=1;
    
/* set default filters; move this to routine */
    /* parse options */
    
    while ((opt = getopt (argc, argv, "A:C:E:d:DR:I:F:L:V:P:SN:GH")) != -1)
	
	switch (opt) {
	case 'R':			/* sets number of retries */
	    filt->num_restart = strtol (optarg, (char **) NULL, 10);
	    filt->num_restart += 1;
	    break;
	case 'I':			/* sets interface */
	    if (strlen (optarg) >= IFNAMSIZ) {
		poe_error (sess,"interface name cannot exceed %d characters", IFNAMSIZ - 1);
		return (-1);
	    }
	    strncpy (sess->name, optarg, strlen(optarg)+1);
	    break;
	case 'C':			/* name of the file in /etc/ppp/peers */
	    if (NULL != filt->fname) {
		poe_error (sess,"-F can not be used with -C");
		return (-1);
	    }
	    if (strlen(optarg) > MAX_FNAME) {
		poe_error (sess,"file name cannot exceed %d characters", MAX_FNAME - 1);
		return (-1);
	    }
	    filt->fname=malloc(strlen(optarg));
	    strncpy (filt->fname, optarg, strlen(optarg));
	    filt->peermode=1;
	    break;
	case 'F':			/* sets the options file */
	    if (NULL != filt->fname) {
		poe_error (sess,"-F can not be used with -C");
		return (-1);
	    }
	    
	    if (strlen(optarg) > MAX_FNAME) {
		poe_error (sess,"file name cannot exceed %d characters", MAX_FNAME - 1);
		return (-1);
	    }
	    filt->fname=malloc(strlen(optarg)+1);
	    strncpy (filt->fname, optarg, strlen(optarg)+1);
	    
	    poe_info (sess,"selected %s as filename\n",filt->fname);
	    break;
	case 'D':			/* don't daemonize */
	    sess->opt_daemonize = 1;
	    detached=0;
	    break;
	case 'd':			/* debug level */
	    sess->opt_debug = strtol (optarg, (char **) NULL, 10);
	    if (sess->opt_debug & 0x0002)
		DEB_DISC=1;
	    if (sess->opt_debug & 0x0004)
		DEB_DISC2=1;
	    break;
	case 'P':			/* sets the pppd binary */
	    if (strlen(optarg) > MAX_FNAME) {
		poe_error (sess,"pppd binary cant exceed %d characters", MAX_FNAME - 1);
		return (-1);
	    }
	    filt->pppd=malloc(strlen(optarg));
	    strncpy (filt->pppd, optarg, strlen(optarg));
	    break;
	case 'H':			
	    sess->np = 2;
	    break;
	case 'G':			
	    sess->np = 1;
	    break;
	case 'V':			/* version */
	    fprintf (stdout,"pppoe version %d.%d build %d", VERSION_MAJOR,
		     VERSION_MINOR, VERSION_DATE);
	    return (0);
	case 'S':			/* server mode */
	    sess->type = SESSION_SERVER;
	    break;
	case 'A':			/* AC override */
	    poe_info (sess,"AC name override to %s", optarg);
	    if (strlen (optarg) > 255) {
		poe_error (sess," AC name too long 
			  (maximum allowed 256 chars)");
		poe_die(-1);
	    }
	    if ((sess->filt->ntag= malloc (sizeof (struct pppoe_tag) + 
					   strlen (optarg)))== NULL) {
		poe_error (sess,"failed to malloc for AC name");
		poe_die(-1);
	    }
	    sess->filt->ntag->tag_len=htons(strlen(optarg));
	    sess->filt->ntag->tag_type=PTT_AC_NAME;
	    strcpy(sess->filt->ntag->tag_data,optarg);
	    break;
	case 'E':			/* AC service name override */
	    poe_info (sess,"AC service name override to %s", optarg);
	    if (strlen (optarg) > 255) {
		poe_error (sess," Service name too long 
	          (maximum allowed 256 chars)");
		poe_die(-1);
	    }
	    
	    if ((filt->stag = malloc (strlen (optarg) + sizeof (struct pppoe_tag))) == NULL) {
		poe_error (sess,"failed to malloc for service name: %m");
		return (-1);
	    }
	    
	    filt->stag->tag_len = htons (strlen (optarg));
	    filt->stag->tag_type = PTT_SRV_NAME;
	    strcpy ((char *) (filt->stag->tag_data), optarg);
	    break;
	default:
	    poe_error (sess,"Unknown option '%c'", optopt);
	    print_help ();
	    return (-1);
	}
    
    
    return (1);
    
}


int main(int argc, char** argv){
    int ret;
    struct filter *filt;
    struct session *ses = (struct session *)malloc(sizeof(struct session));
    char buf[256];
    ses=(void *)malloc(sizeof(struct session));
    
    if(!ses){
	return -1;
    }
    memset(ses,0,sizeof(struct session));
    
    
    
    openlog ("pppoed", LOG_PID | LOG_NDELAY, LOG_PPPOE);
    setlogmask (LOG_UPTO (ses->opt_debug ? LOG_DEBUG : LOG_INFO));
    
    
    if ((get_args (argc,(char **) argv,ses)) <1)
        poe_die(-1);
    
    filt=ses->filt;  /* makes the code more readable */
    
    if (!ses->np) {
	poe_create_pidfile (ses);
//	signal (SIGINT, &sigproc);
//	signal (SIGTERM, &sigproc);
	signal (SIGCHLD, &sigchild);
    }
    
    if(ses->type == SESSION_CLIENT){

	poe_info(ses,"calling client_init_ses\n");
	ret = client_init_ses(ses,ses->name);
    
	if( ret < 0 ){
	    return -1;
	}

	while (ses->filt->num_restart > 0)
	{
	    poe_info(ses,"Restart number %d ",ses->filt->num_restart);
	    ppp_connect (ses);
	    ses->filt->num_restart--;
	}

    }else if( ses->type == SESSION_SERVER ){

	poe_info(ses,"calling srv_init_ses\n");
	ret = srv_init_ses(ses,ses->name);

	if( ret < 0 ){
	    return -1;
	}

	ret = 1;
	while(ret>=0)
	    ret = ppp_connect(ses);
    
    }

    
    
    
    poe_info(ses,"ppp_connect came back! %d",ret);
    
    exit(0);
    
}
