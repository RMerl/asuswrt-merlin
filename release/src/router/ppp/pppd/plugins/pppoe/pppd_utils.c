/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include "pppoe.h"

/*
 *
 */
int build_ppp_opts(char *args[],struct session *ses)
{
    char buf[256];
    int retval=0,i=0;
    
    memset(buf,0,256);
    
/* pppds path */
    if ( NULL != ses->filt->pppd){
	args[0]=(char *)malloc(strlen(ses->filt->pppd));
        strcpy (args[0],ses->filt->pppd);
    } else {
	args[0]=(char *)malloc(strlen(_PATH_PPPD));
        strcpy (args[0],_PATH_PPPD);
    }
    
/*  long device name */
    snprintf(buf, 256,"%02x:%02x:%02x:%02x:%02x:%02x/%04x/%s",
	     ses->remote.sll_addr[0],
	     ses->remote.sll_addr[1],
	     ses->remote.sll_addr[2],
	     ses->remote.sll_addr[3],
	     ses->remote.sll_addr[4],
	     ses->remote.sll_addr[5],
	     ses->sp.sa_addr.pppoe.sid,
	     ses->name);
    args[1]=(char *)malloc(strlen(buf));
    strcpy(args[1],buf);
    
    i=2;
    
/* override options file */
    if (NULL != ses->filt->fname ) {
	
	if (!ses->filt->peermode) {
	    args[i]=(char *)malloc(strlen("file"));
	    strcpy (args[i],"file");
	    i++;
	    args[i]=(char *)malloc(strlen(ses->filt->fname)+1);
	    strcpy (args[i],ses->filt->fname);
	    i++;
	} else{ /* peermode */
	    args[i]=(char *)malloc(strlen("call"));
	    strcpy (args[i],"call");
	    i++;
	    args[i]=(char *)malloc(strlen(ses->filt->fname)+1);
	    strcpy (args[i],ses->filt->fname);
	    i++;
	}
    }
    
/* user requested for a specific name */
    if (NULL != ses->filt->ntag) {
	if ( NULL != ses->filt->ntag->tag_data) {
	    args[i]=(char *)malloc(strlen("pppoe_ac_name"));
	    strcpy(args[i],"pppoe_ac_name");
	    i++;
	    args[i]=(char *)malloc(ntohs(ses->filt->ntag->tag_len));
	    strcpy(args[i],ses->filt->ntag->tag_data);
	    i++;
	}
    }
/* user requested for a specific service name */
    if (NULL != ses->filt->stag) {
	if ( NULL != ses->filt->stag->tag_data) {
	    args[i]=(char *)malloc(strlen("pppoe_srv_name"));
	    strcpy(args[i],"pppoe_srv_name");
	    i++;
	    args[i]=(char *)malloc(ntohs(ses->filt->stag->tag_len));
	    strcpy(args[i],ses->filt->stag->tag_data);
	    i++;
	}
    }
    
/*
 */
    if (ses->opt_daemonize) {
	args[i]=(char *)malloc(strlen("nodetach"));
	strcpy(args[i],"nodetach");
	i++;
    }
    
    args[i]=NULL;
    {
	int j;
	poe_info(ses,"calling pppd with %d args\n",i);
	j=i;
	for (i=0; i<j,NULL !=args[i]; i++) {
	    poe_info(ses," <%d: %s > \n",i,args[i]);
	}
    }
    return retval;
}


/*
 *
 */
int ppp_connect (struct session *ses)
{
    int ret,pid;
    char *args[32];
    
    
    poe_info(ses,"calling ses_connect\n");
    do{
	ret = session_connect(ses);
    }while(ret == 0);

    if (ret > 0 )
	if (ses->np == 1 && ret == 1)
	    return ses->np; /* -G */
    if (ses->np == 2)
	return ses->np; /* -H */

    if( ret <= 0){
	return ret;
    }
    
    poe_info(ses,"DONE calling ses_connect np is %d \n",ses->np);
    
    
    pid = fork ();
    if (pid < 0) {
	poe_error (ses,"unable to fork() for pppd: %m");
	poe_die (-1);
    }
    
    
    if(!pid) {
	poe_info(ses,"calling build_ppp_opts\n");
	if (0> build_ppp_opts(args,ses)) {
	    poe_error(ses,"ppp_connect: failed to build ppp_opts\n");
	    return -1;
	}
	execvp(args[0],args);
	poe_info (ses," child got killed");
    } else if( ses->type == SESSION_CLIENT) {
	if (!ses->opt_daemonize)
	    return 1;
	pause();
	poe_info (ses," OK we got killed");
	return -1;
    }
    return 1;
}

